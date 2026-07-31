// SPDX-FileCopyrightText: 2026 Jowi Aoun
// SPDX-License-Identifier: MPL-2.0
//
// Open-Meteo's geocoding JSON, turned into Places.
//
// A free function over bytes, with no network, no client and no state, because
// that is what makes it a golden-file test — docs/04-architecture.md §4.11:
// "Golden-file tests against recorded API responses committed to
// tests/fixtures/ — no network in CI". A parser that could only be reached
// through a provider that could only be reached through an HTTP client is a
// parser tested through two layers that are not the subject.
//
// ---- the shape of the response, and the trap in it --------------------------
//
// A search returns:
//
//     {"results":[{"id":6167865,"name":"Toronto","latitude":43.70643, …}], …}
//
// and a search that matched nothing returns:
//
//     {"generationtime_ms":0.0337}
//
// with no `results` key at all — verified against the live service. An empty
// array is not what comes back, so a parser that reads `results` and trusts it
// to be an array gets a null QJsonValue, calls toArray() on it, and reports
// zero results. Which is right, by accident, and would stay right until
// somebody added an error branch keyed on "the key is missing". It is handled
// deliberately here instead: no `results` is an empty list and not an error,
// because a person who has typed three letters that match nothing has not
// caused a failure.
//
// `GET /v1/get?id=…` returns one object at the top level, with no envelope.
// Same fields, so both go through the same per-object reader.
//
// ---- which fields are taken, and which are ignored --------------------------
//
// Taken: id, name, latitude, longitude, country_code, country, admin1,
// timezone, elevation. Those are exactly the fields a Place has, and
// `admin1` — the first-level division, "Ontario" — is what turns twelve
// Torontos into twelve distinguishable rows in a search popover.
//
// Ignored: admin2/3/4 and their ids, feature_code, population, postcodes,
// country_id, admin1_id. They are real and they are useful and none of them
// is used yet, and a parser that fills fields nothing reads is a parser whose
// tests assert things nothing depends on.
//
// ---- the country name when the response omits it ----------------------------
//
// Some rows come back with `country_code` and no `country`. Rather than leave
// the field empty, it is filled from QLocale — the same mapping the offline
// reverse geocoder uses, so that the two paths cannot produce "Czechia" and
// "" for the same place and store them as two rows.

#pragma once

#include "libclima/core/result.h"
#include "libclima/domain/place.h"

#include <QByteArray>
#include <QList>

namespace clima {

// Parses a `/v1/search` response. An absent or empty `results` is an empty
// list, not an error; malformed JSON is ErrorKind::Parse.
Result<QList<Place>> parseGeocodingSearch(const QByteArray &json);

// Parses a `/v1/get?id=…` response, which is one bare object.
// ErrorKind::NotFound when the object carries no usable id.
Result<Place> parseGeocodingPlace(const QByteArray &json);

} // namespace clima
