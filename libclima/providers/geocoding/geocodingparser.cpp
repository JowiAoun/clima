// SPDX-FileCopyrightText: 2026 Jowi Aoun
// SPDX-License-Identifier: MPL-2.0

#include "geocodingparser.h"

#include "libclima/providers/geocoding/offlinereversegeocoder.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>

namespace clima {

namespace {

std::optional<Place> placeFromObject(const QJsonObject &object)
{
    // A row with no id is a row we cannot reconcile with anything later, and
    // the whole point of storing geonamesId is reconciliation. Open-Meteo has
    // never sent one; refusing it is cheaper than discovering what a Place
    // with identity zero does to the places table's unique index.
    const qint64 id = qint64(object.value(QStringLiteral("id")).toDouble(0.0));
    if (id == 0)
        return std::nullopt;

    const QJsonValue latitude = object.value(QStringLiteral("latitude"));
    const QJsonValue longitude = object.value(QStringLiteral("longitude"));
    if (!latitude.isDouble() || !longitude.isDouble())
        return std::nullopt;

    Place place;
    place.geonamesId = id;
    place.name = object.value(QStringLiteral("name")).toString();
    place.admin1 = object.value(QStringLiteral("admin1")).toString();
    place.countryCode = object.value(QStringLiteral("country_code")).toString();
    place.country = object.value(QStringLiteral("country")).toString();
    place.timezone = object.value(QStringLiteral("timezone")).toString();
    place.coordinate = Coordinate{ latitude.toDouble(), longitude.toDouble() };

    if (place.country.isEmpty())
        place.country = OfflineReverseGeocoder::countryName(place.countryCode);

    const QJsonValue elevation = object.value(QStringLiteral("elevation"));
    if (elevation.isDouble())
        place.elevationMetres = elevation.toDouble();

    if (place.name.isEmpty() || !place.coordinate.isValid())
        return std::nullopt;

    return place;
}

Result<QJsonDocument> parseDocument(const QByteArray &json)
{
    QJsonParseError    error{};
    const QJsonDocument document = QJsonDocument::fromJson(json, &error);
    if (error.error != QJsonParseError::NoError) {
        return Error(ErrorKind::Parse,
                     QStringLiteral("the geocoder's response is not JSON: %1 at offset %2")
                         .arg(error.errorString())
                         .arg(error.offset));
    }
    if (!document.isObject()) {
        return Error(ErrorKind::Parse,
                     QStringLiteral("the geocoder's response is not a JSON object"));
    }
    return document;
}

} // namespace

Result<QList<Place>> parseGeocodingSearch(const QByteArray &json)
{
    const Result<QJsonDocument> document = parseDocument(json);
    if (!document)
        return document.error();

    const QJsonValue results = document.value().object().value(QStringLiteral("results"));

    // No `results` key is what a search with no matches actually returns. It
    // is a successful empty answer and not a parse failure — see the header.
    if (results.isUndefined() || results.isNull())
        return QList<Place>{};

    if (!results.isArray()) {
        return Error(ErrorKind::Parse,
                     QStringLiteral("the geocoder's `results` is present and is not an array"));
    }

    QList<Place> places;
    for (const QJsonValue &value : results.toArray()) {
        if (!value.isObject())
            continue;
        // One unusable row does not fail the search. A popover with nine of
        // the ten places the user could have meant is better than an error,
        // and the alternative — refusing the whole response because Open-Meteo
        // added a row shaped differently — is a client that breaks on a
        // provider's ordinary change.
        if (const std::optional<Place> place = placeFromObject(value.toObject()))
            places.append(*place);
    }

    return places;
}

Result<Place> parseGeocodingPlace(const QByteArray &json)
{
    const Result<QJsonDocument> document = parseDocument(json);
    if (!document)
        return document.error();

    const std::optional<Place> place = placeFromObject(document.value().object());
    if (!place) {
        return Error(ErrorKind::NotFound,
                     QStringLiteral("the geocoder returned no usable place for that id"));
    }

    return *place;
}

} // namespace clima
