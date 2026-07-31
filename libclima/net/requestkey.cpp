// SPDX-FileCopyrightText: 2026 Jowi Aoun
// SPDX-License-Identifier: MPL-2.0

#include "requestkey.h"

#include <QCryptographicHash>
#include <QHash>
#include <QUrlQuery>

#include <algorithm>

namespace clima {

namespace {

// Number of hex characters kept from the SHA-256. See the header: this is a
// legibility choice inside a single user's cache, not a security parameter.
constexpr int digestChars = 16;

QString parameterDigest(const HttpRequest &request)
{
    // Sorted, so that `?a=1&b=2` and `?b=2&a=1` are one request. A provider
    // that builds its query in a loop over a QMap and one that writes the
    // parameters out by hand must not produce two cache entries for the same
    // forecast.
    QStringList canonical;
    canonical.reserve(request.parameters.size() + 1);
    for (const auto &parameter : request.parameters)
        canonical.append(parameter.first + QLatin1Char('=') + parameter.second);
    std::sort(canonical.begin(), canonical.end());

    // The URL goes into the digest too. Two Open-Meteo endpoints can share an
    // endpoint label and differ only in host — /v1/forecast against a
    // self-hosted instance is not the same answer as against the public one —
    // and a key that ignored the host would serve one for the other.
    canonical.prepend(request.url.toString(QUrl::FullyEncoded));

    // Unit separator as the join, because it cannot appear in a URL or in a
    // percent-encoded parameter, so no combination of values can forge a
    // different split. `a=1&b=2` joined with `&` collides with a single
    // parameter literally named `a` with value `1&b=2`.
    const QByteArray material = canonical.join(QLatin1Char('\x1f')).toUtf8();

    return QString::fromLatin1(
        QCryptographicHash::hash(material, QCryptographicHash::Sha256)
            .toHex()
            .left(digestChars));
}

} // namespace

RequestKey RequestKey::forRequest(const HttpRequest &request)
{
    // THE line. Everything downstream — the coalescer, the cache, the stored
    // ETag — inherits its notion of "the same place" from here.
    const QString place = request.coordinate.has_value()
        ? request.coordinate->rounded().toKeyString()
        : QStringLiteral("-");

    RequestKey key;
    key.m_key = request.providerId
        + QLatin1Char('/') + request.endpoint
        + QLatin1Char('@') + place
        + QLatin1Char('#') + parameterDigest(request);
    return key;
}

size_t qHash(const RequestKey &key, size_t seed) noexcept
{
    return qHash(key.toString(), seed);
}

// ---- composeUrl -------------------------------------------------------------
//
// Lives here rather than in a httprequest.cpp of its own because it and
// RequestKey::forRequest are the same decision written twice: what the server
// is asked for, and what we call the thing we asked for. Splitting them across
// two files is how they drift.

QUrl composeUrl(const HttpRequest &request)
{
    QUrlQuery query;

    if (request.coordinate.has_value()) {
        // Rounded, with the same helper the key uses. The URL and the key
        // therefore agree by construction rather than by review.
        const Coordinate coordinate = request.coordinate->rounded();
        query.addQueryItem(request.latitudeParameter, coordinate.latitudeString());
        query.addQueryItem(request.longitudeParameter, coordinate.longitudeString());
    }

    for (const auto &parameter : request.parameters)
        query.addQueryItem(parameter.first, parameter.second);

    QUrl url = request.url;
    if (!query.isEmpty())
        url.setQuery(query);
    return url;
}

} // namespace clima
