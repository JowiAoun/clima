// SPDX-FileCopyrightText: 2026 Jowi Aoun
// SPDX-License-Identifier: MPL-2.0

#include "openmeteogeocoder.h"

#include "libclima/cache/cachestore.h"
#include "libclima/core/clock.h"
#include "libclima/net/httpclient.h"
#include "libclima/net/requestkey.h"
#include "libclima/providers/geocoding/geocodingparser.h"

#include <QPromise>

#include <algorithm>

namespace clima {

namespace {

// A future that is already finished when it is returned. Callers cannot tell
// the difference from one that took a round trip, which is the point: a cache
// hit and a miss are the same call site.
template <typename T>
QFuture<T> readyFuture(T value)
{
    QPromise<T> promise;
    promise.start();
    promise.addResult(std::move(value));
    promise.finish();
    return promise.future();
}

constexpr const char *providerId = "open-meteo-geocoding";

} // namespace

OpenMeteoGeocoder::OpenMeteoGeocoder(HttpClient *http, CacheStore *cache, Clock *clock,
                                     QObject *parent)
    : QObject(parent)
    , m_http(http)
    , m_cache(cache)
    , m_clock(clock)
    , m_baseUrl(QStringLiteral("https://geocoding-api.open-meteo.com"))
{
}

OpenMeteoGeocoder::~OpenMeteoGeocoder() = default;

QString OpenMeteoGeocoder::id() const
{
    return QString::fromLatin1(providerId);
}

QStringList OpenMeteoGeocoder::attribution() const
{
    // Two credits and not one. Open-Meteo hosts and serves the index;
    // GeoNames is whose data it is. docs/02-data-sources.md §2.9 requires
    // both, and §2.7 records that the geocoding API is GeoNames-backed under
    // CC-BY 4.0.
    return {
        QStringLiteral("Geocoding by Open-Meteo.com (https://open-meteo.com), "
                       "CC BY 4.0 (https://creativecommons.org/licenses/by/4.0/)"),
        QStringLiteral("Place names from GeoNames (https://www.geonames.org), "
                       "CC BY 4.0 (https://creativecommons.org/licenses/by/4.0/)"),
    };
}

void OpenMeteoGeocoder::setBaseUrl(const QUrl &url)
{
    m_baseUrl = url;
}

OpenMeteoGeocoder::Cached OpenMeteoGeocoder::lookUp(const QString &key) const
{
    Cached cached;
    if (m_cache == nullptr)
        return cached;

    const Result<CacheEntry> entry = m_cache->get(key);
    if (!entry)
        return cached;

    cached.payload = entry.value().payload;
    cached.fresh = m_cache->isFresh(entry.value());
    return cached;
}

void OpenMeteoGeocoder::store(const QString &key, const QString &endpoint,
                              const HttpResponse &response)
{
    if (m_cache == nullptr || response.body.isEmpty())
        return;

    CacheEntry entry;
    entry.key = key;
    entry.providerId = QString::fromLatin1(providerId);
    entry.endpoint = endpoint;
    entry.kind = DataKind::Geocoding;
    entry.payload = response.body;
    entry.contentType = response.contentType;
    entry.fetchedAt = response.fetchedAt;
    entry.expiresAt = response.expiresAt;
    entry.validators = response.validators;

    // A failed cache write is not a failed search. The answer is already in
    // hand; losing the chance to reuse it in a week is not worth turning into
    // an error the user sees.
    m_cache->put(entry);
}

QFuture<Result<QList<Place>>> OpenMeteoGeocoder::search(const GeocodeQuery &query)
{
    // simplified() rather than trimmed(): a user who typed "new  york" with two
    // spaces and a user who typed one have asked the same question, and two
    // cache keys for one question is a request that never hits.
    const QString name = query.name.simplified();
    if (name.size() < minimumQueryLength)
        return readyFuture(Result<QList<Place>>(QList<Place>{}));

    HttpRequest request;
    request.providerId = QString::fromLatin1(providerId);
    request.endpoint = QStringLiteral("geocoding/search");
    request.url = m_baseUrl.resolved(QUrl(QStringLiteral("/v1/search")));
    request.kind = DataKind::Geocoding;

    // No validator is offered by this endpoint (§4.5's geocoding row says so),
    // and a conditional request against a server that never sends an ETag is a
    // header for nothing.
    request.conditional = false;

    request.parameters = {
        { QStringLiteral("name"), name },
        { QStringLiteral("count"), QString::number(std::max(1, query.count)) },
        { QStringLiteral("language"), query.language },
        { QStringLiteral("format"), QStringLiteral("json") },
    };

    const QString key = RequestKey::forRequest(request).toString();
    const Cached cached = lookUp(key);
    if (cached.fresh)
        return readyFuture(parseGeocodingSearch(cached.payload));

    const QString endpoint = request.endpoint;
    return m_http->send(request).then(
        this,
        [this, key, endpoint, cached](const Result<HttpResponse> &result) -> Result<QList<Place>> {
            if (!result) {
                // Stale beats nothing. §4.5 marks geocoding
                // stale-while-revalidate, and a week-old answer to "Toronto"
                // is the same answer.
                if (!cached.payload.isEmpty())
                    return parseGeocodingSearch(cached.payload);
                return result.error();
            }

            store(key, endpoint, result.value());
            return parseGeocodingSearch(result.value().body);
        });
}

QFuture<Result<Place>> OpenMeteoGeocoder::resolve(qint64 geonamesId)
{
    if (geonamesId <= 0) {
        return readyFuture(Result<Place>(
            Error(ErrorKind::NotFound, QStringLiteral("%1 is not a GeoNames id").arg(geonamesId))));
    }

    HttpRequest request;
    request.providerId = QString::fromLatin1(providerId);
    request.endpoint = QStringLiteral("geocoding/get");
    request.url = m_baseUrl.resolved(QUrl(QStringLiteral("/v1/get")));
    request.kind = DataKind::Geocoding;
    request.conditional = false;
    request.parameters = { { QStringLiteral("id"), QString::number(geonamesId) } };

    const QString key = RequestKey::forRequest(request).toString();
    const Cached cached = lookUp(key);
    if (cached.fresh)
        return readyFuture(parseGeocodingPlace(cached.payload));

    const QString endpoint = request.endpoint;
    return m_http->send(request).then(
        this,
        [this, key, endpoint, cached](const Result<HttpResponse> &result) -> Result<Place> {
            if (!result) {
                if (!cached.payload.isEmpty())
                    return parseGeocodingPlace(cached.payload);
                return result.error();
            }

            store(key, endpoint, result.value());
            return parseGeocodingPlace(result.value().body);
        });
}

} // namespace clima
