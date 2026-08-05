// SPDX-FileCopyrightText: 2026 Jowi Aoun
// SPDX-License-Identifier: MPL-2.0

#include "httpclient.h"

#include "climaidentity.h"
#include "libclima/core/clock.h"
#include "libclima/net/validatorstore.h"

#include <QLocale>
#include <QLoggingCategory>
#include <QNetworkAccessManager>
#include <QNetworkCookie>
#include <QNetworkCookieJar>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QPromise>
#include <QRandomGenerator>
#include <QTimer>

namespace clima {

Q_LOGGING_CATEGORY(lcHttp, "clima.net.http")

namespace {

// Parses an HTTP-date. Three formats are legal in RFC 7231 and servers in the
// wild use all three, so all three are tried rather than assuming the modern
// one — an Expires we fail to parse is a request we make again for nothing,
// which is precisely the waste this class exists to avoid.
QDateTime parseHttpDate(const QByteArray &raw)
{
    if (raw.isEmpty())
        return {};

    const QString text = QString::fromLatin1(raw).trimmed();

    // IMF-fixdate, the one every modern server sends:
    //   Sun, 06 Nov 1994 08:49:37 GMT
    // Qt's RFC2822 parser handles this, including the obsolete zone names.
    QDateTime parsed = QDateTime::fromString(text, Qt::RFC2822Date);
    if (parsed.isValid())
        return parsed.toUTC();

    // The two obsolete formats, in the C locale because the month and day names
    // in an HTTP-date are English by specification and a French machine's
    // QLocale would refuse "Nov".
    const QLocale c = QLocale::c();
    for (const char *format : { "ddd, dd MMM yyyy HH:mm:ss 'GMT'",   // IMF-fixdate, zone literal
                                "dddd, dd-MMM-yy HH:mm:ss 'GMT'",    // RFC 850
                                "ddd MMM d HH:mm:ss yyyy" }) {       // asctime
        parsed = c.toDateTime(text, QLatin1String(format));
        if (parsed.isValid()) {
            parsed.setTimeZone(QTimeZone::UTC);
            return parsed;
        }
    }

    return {};
}

// A cookie jar that keeps nothing.
//
// Nothing in Clima has a login, and §4.1's "zero telemetry, zero accounts" is a
// claim we make on the download page. A Set-Cookie we accept is a stable
// identifier we then carry across every subsequent request to that host without
// meaning to — which is precisely the cross-request correlation the claim says
// we do not do.
//
// A subclass rather than QNetworkAccessManager::setCookieJar(nullptr), because
// that overload dereferences its argument to compare threads and passing null
// crashes. An empty jar is also the more honest spelling: the jar exists, it is
// offered every cookie, and it declines them.
class NoCookieJar final : public QNetworkCookieJar
{
public:
    using QNetworkCookieJar::QNetworkCookieJar;

    bool setCookiesFromUrl(const QList<QNetworkCookie> &, const QUrl &) override { return false; }
    QList<QNetworkCookie> cookiesForUrl(const QUrl &) const override { return {}; }
};

// Retry-After is either delta-seconds or an HTTP-date. Returns a negative
// duration when there was no usable header, so that "absent" and "zero
// seconds" stay distinguishable — a server saying "retry immediately" is a
// different instruction from a server saying nothing.
std::chrono::milliseconds parseRetryAfter(const QByteArray &raw, const QDateTime &now)
{
    if (raw.isEmpty())
        return std::chrono::milliseconds(-1);

    bool         isNumber = false;
    const qint64 seconds = raw.trimmed().toLongLong(&isNumber);
    if (isNumber)
        return std::chrono::milliseconds(qMax<qint64>(0, seconds * 1000));

    const QDateTime when = parseHttpDate(raw);
    if (when.isValid())
        return std::chrono::milliseconds(qMax<qint64>(0, now.msecsTo(when)));

    return std::chrono::milliseconds(-1);
}

} // namespace

// ---- the in-flight record ---------------------------------------------------
//
// One per key, shared by every caller waiting on it. Held by shared_ptr because
// it outlives the map entry: a retry timer and a QNetworkReply both reference
// it, and the map entry is erased the moment the promise is fulfilled.
struct HttpClient::InFlight {
    HttpRequest request;
    RequestKey  key;

    // shared_ptr rather than a member, because QPromise is move-only and the
    // lambdas that fulfil it are copied into connections.
    std::shared_ptr<QPromise<Result<HttpResponse>>> promise;
    QFuture<Result<HttpResponse>>                   future;

    Validators sent;      // what we put on the wire, so a 304 can restate it
    int        retries = 0;
    bool       finished = false;
};

HttpClient::HttpClient(Clock *clock, QObject *parent)
    : QObject(parent)
    , m_clock(clock)
    , m_network(new QNetworkAccessManager(this))
    // Seeded once from the system generator. Jitter is the one place in this
    // codebase where a nondeterministic source is the point rather than a bug
    // — see backoff.h — and seeding once rather than per-draw keeps the whole
    // process's schedule reproducible from a single value a test can set.
    , m_backoff(BackoffPolicy{}, QRandomGenerator::global()->generate())
{
    // Qt's default in Qt 6 already, stated anyway: a provider that redirects to
    // http:// from https:// is a downgrade we do not follow, and a policy that
    // is inherited rather than declared is a policy that changes under us in a
    // Qt update.
    m_network->setRedirectPolicy(QNetworkRequest::NoLessSafeRedirectPolicy);

    // See NoCookieJar above: not paranoia, the privacy claim being true rather
    // than merely intended.
    m_network->setCookieJar(new NoCookieJar);

    // Replies are deleted by the finished handler, after it has read them.
    // Auto-delete would free the reply before a queued connection ran.
    m_network->setAutoDeleteReplies(false);
}

HttpClient::~HttpClient()
{
    // Every waiting caller gets an answer. A QFuture that is destroyed without
    // being finished leaves its watchers waiting for a signal that will never
    // come, which in a UI is a spinner that never stops.
    const auto flights = m_inFlight.values();
    m_inFlight.clear();
    for (const auto &flight : flights) {
        if (flight->finished)
            continue;
        flight->finished = true;
        flight->promise->addResult(
            Result<HttpResponse>(Error(ErrorKind::Cancelled,
                                       QStringLiteral("the http client was destroyed"))));
        flight->promise->finish();
    }
}

QByteArray HttpClient::userAgent()
{
    // Built once, from CMake's variables. See libclima/climaidentity.h.in for
    // why every piece of this is generated rather than written down.
    static const QByteArray agent = QByteArrayLiteral("Clima/") + CLIMA_ENGINE_VERSION
        + QByteArrayLiteral(" (+") + CLIMA_PROJECT_URL
        + QByteArrayLiteral("; ") + CLIMA_CONTACT + QByteArrayLiteral(")");
    return agent;
}

void HttpClient::setValidatorStore(ValidatorStore *store)
{
    m_validators = store;
}

void HttpClient::setBackoffPolicy(const BackoffPolicy &policy, quint32 seed)
{
    m_backoff = Backoff(policy, seed);
}

void HttpClient::setTransferTimeout(std::chrono::milliseconds timeout)
{
    m_transferTimeout = timeout;
}

bool HttpClient::isProviderDisabled(const QString &providerId) const
{
    return m_disabled.contains(providerId);
}

QStringList HttpClient::disabledProviders() const
{
    QStringList ids(m_disabled.cbegin(), m_disabled.cend());
    ids.sort();
    return ids;
}

int HttpClient::inFlightCount() const
{
    return int(m_inFlight.size());
}

int HttpClient::requestedCount() const
{
    return m_requested;
}

int HttpClient::dispatchedCount() const
{
    return m_dispatched;
}

int HttpClient::coalescedCount() const
{
    return m_coalesced;
}

QFuture<Result<HttpResponse>> HttpClient::send(const HttpRequest &request)
{
    ++m_requested;

    // ---- the hard stop, checked before anything else ------------------------
    //
    // Before the key is computed, before a QNetworkRequest is built, before the
    // event loop is involved. A disabled provider must not produce so much as a
    // DNS lookup, and the cheapest way to guarantee that is for this to be the
    // first branch in the function.
    if (isProviderDisabled(request.providerId)) {
        Error error(ErrorKind::ProviderDisabled,
                    QStringLiteral("provider is disabled for the life of this process after a "
                                   "403; see the earlier providerDisabled diagnostic"));
        error.setProviderId(request.providerId);

        QPromise<Result<HttpResponse>> promise;
        promise.start();
        promise.addResult(Result<HttpResponse>(error));
        promise.finish();
        return promise.future();
    }

    const RequestKey key = RequestKey::forRequest(request);

    // ---- coalescing ---------------------------------------------------------
    //
    // Same key, same answer. Every caller gets a copy of the one future, and
    // the one QPromise fulfils all of them at once.
    if (const auto existing = m_inFlight.value(key)) {
        ++m_coalesced;
        qCDebug(lcHttp) << "coalesced onto in-flight request" << key.toString();
        return existing->future;
    }

    auto flight = std::make_shared<InFlight>();
    flight->request = request;
    flight->key = key;
    flight->promise = std::make_shared<QPromise<Result<HttpResponse>>>();
    flight->promise->start();
    flight->future = flight->promise->future();

    m_inFlight.insert(key, flight);
    dispatch(flight);
    return flight->future;
}

void HttpClient::dispatch(const std::shared_ptr<InFlight> &flight)
{
    ++m_dispatched;

    QNetworkRequest request(composeUrl(flight->request));

    // ---- the User-Agent -----------------------------------------------------
    //
    // Set here and only here. HttpRequest::headers is applied *after* this and
    // is filtered so that it cannot overwrite it — see below. This is the
    // header a 403 hangs off, and the whole class is built around it being
    // exactly one string.
    request.setRawHeader(QByteArrayLiteral("User-Agent"), userAgent());

    for (auto it = flight->request.headers.cbegin(); it != flight->request.headers.cend(); ++it) {
        // A provider that tries to set its own User-Agent is refused rather
        // than obeyed. Silently, because there is nothing a caller could do
        // about it and a warning per request would be noise — but the filter
        // is here so that the compliance string cannot be routed around by
        // adding a map entry.
        if (it.key().compare(QByteArrayLiteral("User-Agent"), Qt::CaseInsensitive) == 0)
            continue;
        request.setRawHeader(it.key(), it.value());
    }

    // ---- conditional GET ----------------------------------------------------
    flight->sent = Validators{};
    if (flight->request.conditional && m_validators != nullptr) {
        if (const auto stored = m_validators->validatorsFor(flight->key.toString())) {
            flight->sent = *stored;
            if (!stored->entityTag.isEmpty())
                request.setRawHeader(QByteArrayLiteral("If-None-Match"), stored->entityTag);
            if (!stored->lastModified.isEmpty())
                request.setRawHeader(QByteArrayLiteral("If-Modified-Since"), stored->lastModified);
        }
    }

    request.setTransferTimeout(m_transferTimeout);

    // Qt's own HTTP cache is off — there isn't one attached — but saying so
    // makes the intent unambiguous: the cache in this app is CacheStore, and
    // two caching layers with two TTL tables is a bug waiting for a support
    // thread nobody can reproduce.
    request.setAttribute(QNetworkRequest::CacheLoadControlAttribute,
                         QNetworkRequest::AlwaysNetwork);

    QNetworkReply *reply = m_network->get(request);
    connect(reply, &QNetworkReply::finished, this, [this, flight, reply]() {
        onReplyFinished(flight, reply);
        reply->deleteLater();
    });
}

void HttpClient::onReplyFinished(const std::shared_ptr<InFlight> &flight, QNetworkReply *reply)
{
    if (flight->finished)
        return;

    const QDateTime now = m_clock->now();
    const int status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
    const QString providerId = flight->request.providerId;

    const auto fail = [&](ErrorKind kind, const QString &message) {
        Error error(kind, message);
        error.setHttpStatus(status);
        error.setProviderId(providerId);
        return error;
    };

    // ---- 403: the hard stop --------------------------------------------------
    //
    // First, and before the transport-error branch, because a 403 arrives with
    // QNetworkReply::ContentAccessDenied set and would otherwise be classified
    // as an ordinary access failure and retried. Retrying a User-Agent-policy
    // 403 is the specific mistake that turns "refused" into "banned".
    if (status == 403) {
        const QString reason =
            QStringLiteral("403 from %1 — the server refused our User-Agent (%2). "
                           "This is a policy refusal, not an outage: the provider is now "
                           "disabled for the life of this process and will not be retried.")
                .arg(reply->url().host(), QString::fromLatin1(userAgent()));
        disableProvider(providerId, reason);
        finish(flight, Result<HttpResponse>(fail(ErrorKind::UserAgentRejected, reason)));
        return;
    }

    // ---- transport failures --------------------------------------------------
    if (status == 0) {
        const QNetworkReply::NetworkError code = reply->error();
        if (code == QNetworkReply::NoError) {
            finish(flight,
                   Result<HttpResponse>(fail(ErrorKind::Network,
                                             QStringLiteral("no status and no error"))));
            return;
        }

        const bool timedOut = code == QNetworkReply::TimeoutError
            || code == QNetworkReply::OperationCanceledError;
        const Error error = fail(timedOut ? ErrorKind::Timeout : ErrorKind::Network,
                                 reply->errorString());

        if (m_backoff.shouldRetry(flight->retries)) {
            scheduleRetry(flight, m_backoff.delayForRetry(flight->retries));
            return;
        }
        finish(flight, Result<HttpResponse>(error));
        return;
    }

    // ---- 429 and 5xx: backoff ------------------------------------------------
    if (status == 429 || status >= 500) {
        const ErrorKind kind = status == 429 ? ErrorKind::RateLimited : ErrorKind::ServerError;
        Error error = fail(kind, reply->errorString().isEmpty()
                                   ? QStringLiteral("HTTP %1").arg(status)
                                   : reply->errorString());

        // The server's instruction beats our guess. Clamped to the same cap, so
        // a broken Retry-After of a fortnight cannot park a request forever.
        const auto advised =
            parseRetryAfter(reply->rawHeader(QByteArrayLiteral("Retry-After")), now);
        const auto delay = advised.count() >= 0
            ? m_backoff.clampToCap(advised)
            : m_backoff.delayForRetry(flight->retries);

        error.setRetryAfter(now.addMSecs(delay.count()));

        if (m_backoff.shouldRetry(flight->retries)) {
            scheduleRetry(flight, delay);
            return;
        }
        finish(flight, Result<HttpResponse>(error));
        return;
    }

    // ---- 304: a success, and a cheap one ------------------------------------
    if (status == 304) {
        HttpResponse response;
        response.status = status;
        response.notModified = true;
        response.fetchedAt = now;
        response.retries = flight->retries;
        response.url = reply->url();

        // A 304 may restate the validators or may not. Keeping the ones we sent
        // when the server says nothing is what makes a second conditional
        // request possible; dropping them would mean the next fetch is
        // unconditional and the agreement in §2.9 lasts exactly one round trip.
        response.validators = flight->sent;
        const QByteArray etag = reply->rawHeader(QByteArrayLiteral("ETag"));
        if (!etag.isEmpty())
            response.validators.entityTag = etag;

        const QDateTime serverExpires =
            parseHttpDate(reply->rawHeader(QByteArrayLiteral("Expires")));
        if (serverExpires.isValid())
            response.validators.expires = serverExpires;

        const QDateTime ours = expiryFor(flight->request.kind, now);
        response.expiresAt = (serverExpires.isValid() && (!ours.isValid() || serverExpires > ours))
            ? serverExpires
            : ours;

        if (m_validators != nullptr)
            m_validators->storeValidators(flight->key.toString(), response.validators);

        finish(flight, Result<HttpResponse>(response));
        return;
    }

    // ---- 404 and the rest of 4xx --------------------------------------------
    //
    // The body is read and appended, truncated. QNetworkReply::errorString() for
    // a 4xx is "…server replied: Bad Request" and nothing else, while the answer
    // to *why* is in the payload every one of our providers sends one in:
    // Open-Meteo says `{"error":true,"reason":"Cannot initialize WeatherVariable
    // from invalid String value …"}`, api.weather.gov says `"Parameter \"point\"
    // is invalid: out of bounds"`. Dropping that turned a sentence naming the
    // mistake into "Bad Request", which is the difference between a five-minute
    // fix and an afternoon with a packet capture.
    //
    // Truncated because this reaches a log: an error message is a sentence, and
    // a provider that answers 4xx with a page of HTML must not paste it into
    // one.
    if (status >= 400) {
        const ErrorKind kind = status == 404 || status == 410 ? ErrorKind::NotFound
                                                              : ErrorKind::HttpStatus;

        QString message = reply->errorString();
        const QByteArray body = reply->readAll().trimmed();
        if (!body.isEmpty()) {
            constexpr int limit = 400;
            QString detail = QString::fromUtf8(body.left(limit));
            if (body.size() > limit)
                detail += QStringLiteral("…");
            message += QStringLiteral(" — ") + detail.simplified();
        }

        finish(flight, Result<HttpResponse>(fail(kind, message)));
        return;
    }

    // ---- 2xx -----------------------------------------------------------------
    HttpResponse response;
    response.status = status;
    response.body = reply->readAll();
    response.contentType = reply->rawHeader(QByteArrayLiteral("Content-Type"));
    response.fetchedAt = now;
    response.retries = flight->retries;
    response.url = reply->url();
    response.validators.entityTag = reply->rawHeader(QByteArrayLiteral("ETag"));
    response.validators.lastModified = reply->rawHeader(QByteArrayLiteral("Last-Modified"));

    // ---- Expires beats our TTL when it is longer ----------------------------
    //
    // §4.5's table is our refresh policy, and a provider is entitled to a
    // longer opinion about the shelf life of its own bytes. Honouring the
    // longer of the two costs nothing and saves a request that would have been
    // answered with the same payload. The shorter of the two is *not* honoured:
    // a provider that sets Expires to five seconds is describing its CDN, not
    // telling us to poll twelve times a minute.
    const QDateTime serverExpires = parseHttpDate(reply->rawHeader(QByteArrayLiteral("Expires")));
    if (serverExpires.isValid())
        response.validators.expires = serverExpires;

    const QDateTime ours = expiryFor(flight->request.kind, now);
    response.expiresAt = (serverExpires.isValid() && (!ours.isValid() || serverExpires > ours))
        ? serverExpires
        : ours;

    if (m_validators != nullptr && !response.validators.isEmpty())
        m_validators->storeValidators(flight->key.toString(), response.validators);

    finish(flight, Result<HttpResponse>(response));
}

void HttpClient::scheduleRetry(const std::shared_ptr<InFlight> &flight,
                               std::chrono::milliseconds delay)
{
    const int retry = flight->retries;
    ++flight->retries;

    Q_EMIT retryScheduled(flight->request.providerId, flight->key.toString(), retry,
                          int(delay.count()));
    qCDebug(lcHttp) << "retry" << retry << "for" << flight->key.toString() << "in"
                    << delay.count() << "ms";

    // Receiver is `this`, so destroying the client cancels the timer rather
    // than firing a lambda into a dangling object. The flight is captured by
    // shared_ptr and re-checked, because it can be finished from the
    // destructor while the timer is pending.
    QTimer::singleShot(delay, this, [this, flight]() {
        if (flight->finished)
            return;
        dispatch(flight);
    });
}

void HttpClient::finish(const std::shared_ptr<InFlight> &flight, Result<HttpResponse> result)
{
    if (flight->finished)
        return;
    flight->finished = true;

    // Out of the map before the promise is fulfilled. A continuation attached
    // to the future can call send() again for the same key — a fallback chain
    // retrying against a second provider is exactly that shape — and it must
    // see an empty slot rather than coalesce onto a request that has already
    // answered.
    m_inFlight.remove(flight->key);

    flight->promise->addResult(std::move(result));
    flight->promise->finish();
}

void HttpClient::disableProvider(const QString &providerId, const QString &reason)
{
    if (m_disabled.contains(providerId))
        return;
    m_disabled.insert(providerId);
    qCWarning(lcHttp).noquote() << reason;
    Q_EMIT providerDisabled(providerId, reason);
}

} // namespace clima
