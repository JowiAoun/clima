// SPDX-FileCopyrightText: 2026 Jowi Aoun
// SPDX-License-Identifier: GPL-3.0-or-later
//
// HttpClient, against a QTcpServer on loopback. No network, ever — see
// tests/support/networkguard.h, which makes that a property of the process
// rather than a habit of the author.
//
// The four things asserted here are the four promises the class exists to keep:
// the User-Agent goes out on every request, a 403 stops everything for that
// provider and is never retried, duplicate requests become one request, and a
// stored ETag comes back out as If-None-Match.

#include "climaidentity.h"
#include "libclima/core/clock.h"
#include "libclima/net/httpclient.h"
#include "support/httpstub.h"
#include "support/networkguard.h"

#include <QHash>
#include <QSignalSpy>
#include <QTest>

using namespace clima;
using namespace std::chrono_literals;

namespace {

// Twenty lines, and the reason ValidatorStore is an interface rather than a
// CacheStore pointer. The network layer's tests need no database, no
// QStandardPaths and no migration runner.
class MemoryValidatorStore final : public ValidatorStore
{
public:
    std::optional<Validators> validatorsFor(const QString &key) const override
    {
        const auto it = m_entries.constFind(key);
        if (it == m_entries.cend())
            return std::nullopt;
        return *it;
    }

    void storeValidators(const QString &key, const Validators &validators) override
    {
        m_entries.insert(key, validators);
        ++m_writes;
    }

    [[nodiscard]] int writes() const { return m_writes; }

private:
    QHash<QString, Validators> m_entries;
    int                        m_writes = 0;
};

QByteArray httpDate(const QDateTime &when)
{
    return QLocale::c()
        .toString(when.toUTC(), QStringLiteral("ddd, dd MMM yyyy HH:mm:ss 'GMT'"))
        .toLatin1();
}

} // namespace

class TestHttpClient : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void initTestCase();
    void init();
    void cleanup();

    void userAgentIsWellFormedAndCarriesTheBuildVersion();
    void everyRequestCarriesTheUserAgent();
    void aRequestHeaderCannotOverrideTheUserAgent();

    void forbiddenDisablesTheProviderAndIsNeverRetried();
    void aDisabledProviderIsRefusedWithoutASingleRequest();
    void forbiddenDoesNotDisableTheOtherProviders();

    void serverErrorBacksOffAndRetries();
    void serverErrorGivesUpAfterTheLastRetry();
    void rateLimitReportsTheServersRetryAfter();
    void anAbsurdRetryAfterIsClampedToTheCap();

    void duplicateRequestsCoalesceIntoOne();
    void aMapDragCoalescesBecauseTheCoordinateIsRoundedFirst();
    void parameterOrderDoesNotChangeTheRequest();

    void conditionalGetSendsTheStoredEntityTag();
    void notModifiedIsASuccessAndNotAnError();
    void aLongerServerExpiresWinsOverOurTtl();
    void aShorterServerExpiresDoesNotShortenOurTtl();

private:
    HttpRequest forecastRequest() const;
    static bool settle(QFuture<Result<HttpResponse>> &future);

    HttpStub    m_stub;
    FrozenClock m_clock{ QDateTime(QDate(2026, 3, 14), QTime(9, 26, 53), QTimeZone::UTC) };
};

void TestHttpClient::initTestCase()
{
    NetworkGuard::install();
}

void TestHttpClient::init()
{
    m_stub.reset();
    QVERIFY2(m_stub.listen(), "could not listen on loopback");
    NetworkGuard::clearAttempts();
}

void TestHttpClient::cleanup()
{
    // Every test in this class ends by proving it stayed on the machine.
    QCOMPARE(NetworkGuard::externalAttempts(), QStringList());
}

HttpRequest TestHttpClient::forecastRequest() const
{
    HttpRequest request;
    request.providerId = QStringLiteral("open-meteo");
    request.endpoint = QStringLiteral("forecast");
    request.url = QUrl(m_stub.baseUrl() + QStringLiteral("/v1/forecast"));
    request.kind = DataKind::Forecast;
    request.coordinate = Coordinate{ 52.520008, 13.404954 };
    request.parameters = { { QStringLiteral("hourly"), QStringLiteral("temperature_2m") } };
    return request;
}

bool TestHttpClient::settle(QFuture<Result<HttpResponse>> &future)
{
    return QTest::qWaitFor([&future]() { return future.isFinished(); }, 5000);
}

// ---- the User-Agent ---------------------------------------------------------

void TestHttpClient::userAgentIsWellFormedAndCarriesTheBuildVersion()
{
    const QByteArray agent = HttpClient::userAgent();

    // The shape docs/02-data-sources.md §2.9 records:
    //     Clima/<version> (+<project url>; <contact>)
    QVERIFY2(agent.startsWith(QByteArrayLiteral("Clima/")), agent.constData());
    QVERIFY2(agent.contains(QByteArrayLiteral(" (+")), agent.constData());
    QVERIFY2(agent.endsWith(QByteArrayLiteral(")")), agent.constData());

    // The version is the build's, not a literal somebody typed. If this ever
    // fails it is because the identity header stopped coming from CMake, and
    // the symptom in the wild would be a client misidentifying itself to MET
    // Norway for a whole release.
    QVERIFY2(agent.contains(QByteArrayLiteral(CLIMA_ENGINE_VERSION)), agent.constData());
    QVERIFY2(agent.contains(QByteArrayLiteral(CLIMA_PROJECT_URL)), agent.constData());
    QVERIFY2(agent.contains(QByteArrayLiteral(CLIMA_CONTACT)), agent.constData());

    // Not the empty User-Agent that api.weather.gov answers with 403 today, and
    // not Qt's default either.
    QVERIFY(!agent.trimmed().isEmpty());
    QVERIFY(!agent.contains(QByteArrayLiteral("Mozilla")));
}

void TestHttpClient::everyRequestCarriesTheUserAgent()
{
    m_stub.enqueue(StubResponse::ok(QByteArrayLiteral("{}")));

    HttpClient client(&m_clock);
    auto       future = client.send(forecastRequest());
    QVERIFY(settle(future));
    QVERIFY(future.result().hasValue());

    QCOMPARE(m_stub.requestCount(), 1);
    QCOMPARE(m_stub.requests().at(0).header(QByteArrayLiteral("user-agent")),
             HttpClient::userAgent());
}

void TestHttpClient::aRequestHeaderCannotOverrideTheUserAgent()
{
    // A provider that sets its own User-Agent is refused rather than obeyed.
    // The compliance string is exactly one string and there is no route around
    // it — including a header map entry with different casing.
    m_stub.enqueue(StubResponse::ok(QByteArrayLiteral("{}")));

    HttpRequest request = forecastRequest();
    request.headers.insert(QByteArrayLiteral("user-agent"), QByteArrayLiteral("curl/8.0"));
    request.headers.insert(QByteArrayLiteral("Accept"), QByteArrayLiteral("application/json"));

    HttpClient client(&m_clock);
    auto       future = client.send(request);
    QVERIFY(settle(future));

    QCOMPARE(m_stub.requests().at(0).header(QByteArrayLiteral("user-agent")),
             HttpClient::userAgent());
    // The headers that are not the User-Agent still go through.
    QCOMPARE(m_stub.requests().at(0).header(QByteArrayLiteral("accept")),
             QByteArrayLiteral("application/json"));
}

// ---- 403: the hard stop -----------------------------------------------------

void TestHttpClient::forbiddenDisablesTheProviderAndIsNeverRetried()
{
    m_stub.enqueue(StubResponse::withStatus(403));

    HttpClient client(&m_clock);
    // A retry schedule that would fire almost immediately, so that "it was not
    // retried" is a statement about the code and not about the test being too
    // impatient to notice.
    client.setBackoffPolicy(BackoffPolicy{ 1ms, 2.0, 10ms, 5 }, 1);

    QSignalSpy disabled(&client, &HttpClient::providerDisabled);
    QSignalSpy retries(&client, &HttpClient::retryScheduled);

    auto future = client.send(forecastRequest());
    QVERIFY(settle(future));

    const Result<HttpResponse> result = future.result();
    QVERIFY(!result.hasValue());
    QCOMPARE(result.errorKind(), ErrorKind::UserAgentRejected);
    QCOMPARE(result.error().httpStatus(), 403);
    QCOMPARE(result.error().providerId(), QStringLiteral("open-meteo"));
    QVERIFY(!result.error().isRetryable());

    // Exactly one request reached the server. This is the assertion that
    // matters: retrying a User-Agent-policy 403 is how a project's address
    // goes from refused to banned.
    QCOMPARE(m_stub.requestCount(), 1);
    QCOMPARE(client.dispatchedCount(), 1);
    QCOMPARE(retries.count(), 0);

    QCOMPARE(disabled.count(), 1);
    QCOMPARE(disabled.at(0).at(0).toString(), QStringLiteral("open-meteo"));
    QVERIFY(client.isProviderDisabled(QStringLiteral("open-meteo")));
    QCOMPARE(client.disabledProviders(), QStringList{ QStringLiteral("open-meteo") });

    // And the diagnostic names what a human needs to act on: the host, and the
    // User-Agent that was refused.
    const QString reason = disabled.at(0).at(1).toString();
    QVERIFY2(reason.contains(QString::fromLatin1(HttpClient::userAgent())),
             qPrintable(reason));

    // Waiting past every retry the schedule could have produced changes
    // nothing. A "was not retried" assertion taken immediately after the
    // failure proves only that the retry had not happened yet.
    QTest::qWait(120);
    QCOMPARE(m_stub.requestCount(), 1);
}

void TestHttpClient::aDisabledProviderIsRefusedWithoutASingleRequest()
{
    m_stub.enqueue(StubResponse::withStatus(403));

    HttpClient client(&m_clock);
    auto       first = client.send(forecastRequest());
    QVERIFY(settle(first));
    QCOMPARE(m_stub.requestCount(), 1);

    // Ten more, for the rest of the process's life. None of them may produce
    // so much as a connection.
    for (int i = 0; i < 10; ++i) {
        auto later = client.send(forecastRequest());

        // Already finished, with no round trip and without the event loop
        // having turned.
        QVERIFY(later.isFinished());
        QVERIFY(!later.result().hasValue());
        QCOMPARE(later.result().errorKind(), ErrorKind::ProviderDisabled);
    }

    QCOMPARE(m_stub.requestCount(), 1);
    QCOMPARE(client.dispatchedCount(), 1);
    QCOMPARE(client.requestedCount(), 11);
}

void TestHttpClient::forbiddenDoesNotDisableTheOtherProviders()
{
    // The blast radius is one provider, because the routing layer above is
    // meant to fall through to the next one in the chain (§4.4). A 403 that
    // took down every provider would turn one broken User-Agent into an app
    // with no data at all.
    m_stub.enqueue(StubResponse::withStatus(403));
    m_stub.enqueue(StubResponse::ok(QByteArrayLiteral("{\"ok\":true}")));

    HttpClient client(&m_clock);

    auto first = client.send(forecastRequest());
    QVERIFY(settle(first));
    QVERIFY(client.isProviderDisabled(QStringLiteral("open-meteo")));

    HttpRequest fallback = forecastRequest();
    fallback.providerId = QStringLiteral("met-no");
    auto second = client.send(fallback);
    QVERIFY(settle(second));

    QVERIFY(second.result().hasValue());
    QCOMPARE(second.result().value().body, QByteArrayLiteral("{\"ok\":true}"));
    QVERIFY(!client.isProviderDisabled(QStringLiteral("met-no")));
}

// ---- 429 and 5xx: backoff ---------------------------------------------------

void TestHttpClient::serverErrorBacksOffAndRetries()
{
    m_stub.enqueue(StubResponse::withStatus(500));
    m_stub.enqueue(StubResponse::withStatus(503));
    m_stub.enqueue(StubResponse::ok(QByteArrayLiteral("{\"third\":\"time\"}")));

    HttpClient client(&m_clock);
    client.setBackoffPolicy(BackoffPolicy{ 2ms, 2.0, 50ms, 5 }, 99);

    QSignalSpy retries(&client, &HttpClient::retryScheduled);

    auto future = client.send(forecastRequest());
    QVERIFY(settle(future));

    const Result<HttpResponse> result = future.result();
    QVERIFY2(result.hasValue(), qPrintable(result.error().toString()));
    QCOMPARE(result.value().body, QByteArrayLiteral("{\"third\":\"time\"}"));
    QCOMPARE(result.value().retries, 2);

    QCOMPARE(m_stub.requestCount(), 3);
    QCOMPARE(retries.count(), 2);

    // The retry index climbs, which is what makes the delay grow.
    QCOMPARE(retries.at(0).at(2).toInt(), 0);
    QCOMPARE(retries.at(1).at(2).toInt(), 1);

    // Every scheduled delay landed inside the window its ceiling allowed.
    QVERIFY(retries.at(0).at(3).toInt() >= 0);
    QVERIFY(retries.at(0).at(3).toInt() <= 2);
    QVERIFY(retries.at(1).at(3).toInt() <= 4);
}

void TestHttpClient::serverErrorGivesUpAfterTheLastRetry()
{
    m_stub.enqueue(StubResponse::withStatus(503));   // the last entry repeats

    HttpClient client(&m_clock);
    client.setBackoffPolicy(BackoffPolicy{ 1ms, 2.0, 10ms, 3 }, 5);

    auto future = client.send(forecastRequest());
    QVERIFY(settle(future));

    const Result<HttpResponse> result = future.result();
    QVERIFY(!result.hasValue());
    QCOMPARE(result.errorKind(), ErrorKind::ServerError);
    QCOMPARE(result.error().httpStatus(), 503);
    QVERIFY(result.error().isRetryable());

    // One attempt plus three retries, and then it stops. A client that never
    // gives up is a client that hammers a dead endpoint until the process
    // ends.
    QCOMPARE(m_stub.requestCount(), 4);
    QCOMPARE(client.dispatchedCount(), 4);

    QTest::qWait(60);
    QCOMPARE(m_stub.requestCount(), 4);
}

void TestHttpClient::rateLimitReportsTheServersRetryAfter()
{
    m_stub.enqueue(StubResponse::withStatus(429).with(QByteArrayLiteral("Retry-After"),
                                                  QByteArrayLiteral("120")));

    HttpClient client(&m_clock);
    // No retries, so the error reaches the caller and its advice can be read.
    client.setBackoffPolicy(BackoffPolicy{ 1ms, 2.0, 30min, 0 }, 1);

    auto future = client.send(forecastRequest());
    QVERIFY(settle(future));

    const Result<HttpResponse> result = future.result();
    QVERIFY(!result.hasValue());
    QCOMPARE(result.errorKind(), ErrorKind::RateLimited);

    // The server said two minutes and the server wins. Frozen clock, so this
    // is an equality rather than a range.
    QCOMPARE(result.error().retryAfter(), m_clock.now().addSecs(120));
    QCOMPARE(m_stub.requestCount(), 1);
}

void TestHttpClient::anAbsurdRetryAfterIsClampedToTheCap()
{
    m_stub.enqueue(StubResponse::withStatus(503).with(QByteArrayLiteral("Retry-After"),
                                                  QByteArrayLiteral("604800")));   // a week

    HttpClient client(&m_clock);
    client.setBackoffPolicy(BackoffPolicy{ 1ms, 2.0, 30min, 0 }, 1);

    auto future = client.send(forecastRequest());
    QVERIFY(settle(future));

    // Honouring a week literally would park a location until the user
    // restarted. Thirty minutes is the cap and it applies to the server's
    // advice as well as to our own schedule.
    QCOMPARE(future.result().error().retryAfter(), m_clock.now().addSecs(30 * 60));
}

// ---- coalescing -------------------------------------------------------------

void TestHttpClient::duplicateRequestsCoalesceIntoOne()
{
    m_stub.enqueue(StubResponse::ok(QByteArrayLiteral("{\"once\":true}")));

    HttpClient client(&m_clock);

    // Three callers, the shape of a warm start: the home view, the hourly view
    // and the details sheet all want the same forecast in the same frame.
    auto first = client.send(forecastRequest());
    auto second = client.send(forecastRequest());
    auto third = client.send(forecastRequest());

    QCOMPARE(client.inFlightCount(), 1);
    QCOMPARE(client.requestedCount(), 3);
    QCOMPARE(client.dispatchedCount(), 1);
    QCOMPARE(client.coalescedCount(), 2);

    QVERIFY(settle(first));
    QVERIFY(settle(second));
    QVERIFY(settle(third));

    QCOMPARE(m_stub.requestCount(), 1);

    // And all three got the answer, not just the one that asked first.
    for (auto *future : { &first, &second, &third }) {
        QVERIFY(future->result().hasValue());
        QCOMPARE(future->result().value().body, QByteArrayLiteral("{\"once\":true}"));
    }

    // The slot is free again afterwards, so a later fetch is a fetch and not a
    // coalesce onto a request that already answered.
    QCOMPARE(client.inFlightCount(), 0);
}

void TestHttpClient::aMapDragCoalescesBecauseTheCoordinateIsRoundedFirst()
{
    m_stub.enqueue(StubResponse::ok(QByteArrayLiteral("{}")));

    HttpClient client(&m_clock);

    // Six centre coordinates from one drag. They differ in the fifth decimal
    // and beyond — under two metres, well inside a grid cell 11 km across — so
    // they are one request.
    //
    // Note what this does *not* claim. Rounding creates cell boundaries, and a
    // drag that crosses one legitimately issues a second request: 13.4049567
    // and 13.4049480 are nine millimetres apart and land either side of
    // 13.40495. That is correct — they are different grid cells, and the
    // forecast is allowed to differ — and it is asserted from the other side
    // by tst_requestkey.cpp's adjacentCellsAreDifferentKeys(). Every
    // coordinate below is deliberately inside one cell, because this test is
    // about the hundred requests that collapse and not about the one that
    // does not.
    const QList<Coordinate> drag = {
        { 52.520008, 13.404954 },   { 52.520011, 13.404961 },  { 52.5200149, 13.4049502 },
        { 52.5199962, 13.4049518 }, { 52.520003, 13.4049567 }, { 52.5200441, 13.4049612 },
    };

    QList<QFuture<Result<HttpResponse>>> futures;
    for (const Coordinate &coordinate : drag) {
        HttpRequest request = forecastRequest();
        request.coordinate = coordinate;
        futures.append(client.send(request));
    }

    QCOMPARE(client.dispatchedCount(), 1);
    for (auto &future : futures)
        QVERIFY(settle(future));
    QCOMPARE(m_stub.requestCount(), 1);

    // And the coordinate that went out is the rounded one, not the one the map
    // happened to be showing. Four decimals is what MET Norway's terms ask for
    // by name.
    const QByteArray target = m_stub.requests().at(0).target;
    QVERIFY2(target.contains(QByteArrayLiteral("latitude=52.5200")), target.constData());
    QVERIFY2(target.contains(QByteArrayLiteral("longitude=13.4050")), target.constData());
    QVERIFY2(!target.contains(QByteArrayLiteral("52.520008")), target.constData());
}

void TestHttpClient::parameterOrderDoesNotChangeTheRequest()
{
    m_stub.enqueue(StubResponse::ok(QByteArrayLiteral("{}")));

    HttpClient client(&m_clock);

    HttpRequest a = forecastRequest();
    a.parameters = { { QStringLiteral("hourly"), QStringLiteral("temperature_2m") },
                     { QStringLiteral("timezone"), QStringLiteral("auto") } };

    HttpRequest b = forecastRequest();
    b.parameters = { { QStringLiteral("timezone"), QStringLiteral("auto") },
                     { QStringLiteral("hourly"), QStringLiteral("temperature_2m") } };

    auto first = client.send(a);
    auto second = client.send(b);

    QCOMPARE(client.dispatchedCount(), 1);
    QVERIFY(settle(first));
    QVERIFY(settle(second));
    QCOMPARE(m_stub.requestCount(), 1);
}

// ---- conditional GET --------------------------------------------------------

void TestHttpClient::conditionalGetSendsTheStoredEntityTag()
{
    MemoryValidatorStore validators;

    // First round trip: the server hands us an ETag and a Last-Modified.
    m_stub.enqueue(StubResponse::ok(QByteArrayLiteral("{\"v\":1}"))
                       .with(QByteArrayLiteral("ETag"), QByteArrayLiteral("\"abc-123\""))
                       .with(QByteArrayLiteral("Last-Modified"),
                             QByteArrayLiteral("Sat, 14 Mar 2026 09:00:00 GMT")));

    HttpClient client(&m_clock);
    client.setValidatorStore(&validators);

    auto first = client.send(forecastRequest());
    QVERIFY(settle(first));
    QVERIFY(first.result().hasValue());
    QCOMPARE(first.result().value().validators.entityTag, QByteArrayLiteral("\"abc-123\""));

    // Nothing was sent the first time, because there was nothing on file.
    QVERIFY(m_stub.requests().at(0).header(QByteArrayLiteral("if-none-match")).isEmpty());
    QCOMPARE(validators.writes(), 1);

    // Second round trip: the validators go back out.
    auto second = client.send(forecastRequest());
    QVERIFY(settle(second));

    QCOMPARE(m_stub.requestCount(), 2);
    QCOMPARE(m_stub.requests().at(1).header(QByteArrayLiteral("if-none-match")),
             QByteArrayLiteral("\"abc-123\""));
    QCOMPARE(m_stub.requests().at(1).header(QByteArrayLiteral("if-modified-since")),
             QByteArrayLiteral("Sat, 14 Mar 2026 09:00:00 GMT"));
}

void TestHttpClient::notModifiedIsASuccessAndNotAnError()
{
    MemoryValidatorStore validators;
    validators.storeValidators(
        RequestKey::forRequest(forecastRequest()).toString(),
        Validators{ QByteArrayLiteral("\"unchanged\""), QByteArray(), QDateTime() });

    m_stub.enqueue(StubResponse::withStatus(304));

    HttpClient client(&m_clock);
    client.setValidatorStore(&validators);

    auto future = client.send(forecastRequest());
    QVERIFY(settle(future));

    const Result<HttpResponse> result = future.result();

    // A 304 means the data we already hold is confirmed current. Reporting
    // that as a failure would make every caller special-case a success.
    QVERIFY2(result.hasValue(), qPrintable(result.error().toString()));
    QCOMPARE(result.value().status, 304);
    QVERIFY(result.value().notModified);
    QVERIFY(result.value().body.isEmpty());

    // The validators survive the 304, so the *next* request is conditional
    // too. Dropping them would make the agreement in §2.9 last exactly one
    // round trip.
    QCOMPARE(result.value().validators.entityTag, QByteArrayLiteral("\"unchanged\""));

    // And the expiry moved forward: the entry is fresh again for a full TTL.
    QCOMPARE(result.value().expiresAt, m_clock.now().addSecs(30 * 60));
}

void TestHttpClient::aLongerServerExpiresWinsOverOurTtl()
{
    // Our forecast TTL is 30 minutes (§4.5). The server says two hours, and it
    // knows more about the shelf life of its own model run than our table does.
    const QDateTime serverExpiry = m_clock.now().addSecs(2 * 60 * 60);
    m_stub.enqueue(StubResponse::ok(QByteArrayLiteral("{}"))
                       .with(QByteArrayLiteral("Expires"), httpDate(serverExpiry)));

    HttpClient client(&m_clock);
    auto       future = client.send(forecastRequest());
    QVERIFY(settle(future));

    QVERIFY(future.result().hasValue());
    QCOMPARE(future.result().value().expiresAt, serverExpiry);
}

void TestHttpClient::aShorterServerExpiresDoesNotShortenOurTtl()
{
    // Five seconds is a CDN describing itself, not an instruction to poll
    // twelve times a minute. Our 30 minutes stands.
    m_stub.enqueue(StubResponse::ok(QByteArrayLiteral("{}"))
                       .with(QByteArrayLiteral("Expires"), httpDate(m_clock.now().addSecs(5))));

    HttpClient client(&m_clock);
    auto       future = client.send(forecastRequest());
    QVERIFY(settle(future));

    QVERIFY(future.result().hasValue());
    QCOMPARE(future.result().value().expiresAt, m_clock.now().addSecs(30 * 60));
}

QTEST_MAIN(TestHttpClient)
#include "tst_httpclient.moc"
