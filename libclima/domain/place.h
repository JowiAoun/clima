// SPDX-FileCopyrightText: 2026 Jowi Aoun
// SPDX-License-Identifier: MPL-2.0
//
// A place the user cares about: what it is called, where it is, and the one
// number that says it is the same place somebody else means.
//
// ---- geonamesId is the identity, and the rest is description ----------------
//
// Every place in Clima comes from GeoNames. The forward geocoder is
// Open-Meteo's search API, which is GeoNames; the offline reverse geocoder is a
// packed cities15000, which is GeoNames. So "Toronto" found by typing and
// "Toronto" found by standing in it are the same row upstream, 6167865, and
// storing that number is what lets the app know it.
//
// Everything else about a place is a *description* and descriptions drift.
// Reykjavík is spelled "Reykjavik" in Open-Meteo's snapshot and "Reykjavík" in
// today's dump; a city is renamed; a country changes how its first-level
// divisions are named. A saved place keyed on its name would silently become a
// second place the day any of that happened, and the user's home would move.
// Keyed on 6167865 it does not, and `GET /v1/get?id=6167865` re-reads the
// current description whenever it is worth refreshing.
//
// A place with `geonamesId == 0` is one the user pinned by coordinate — a
// dropped map pin, a manually typed latitude. It is legal and it simply has no
// upstream identity to reconcile against.
//
// ---- why this is in domain/ and not next to the database --------------------
//
// It was in libclima/cache/cachestore.h, which meant that anything wanting to
// name a place — the geocoder, the reverse index, the location model — had to
// include a header that drags in QSqlDatabase. A value type is not owned by
// whichever layer happened to persist it first; it sits beside Coordinate,
// which is the other thing everything needs and nothing owns.

#pragma once

#include "libclima/domain/coordinate.h"

#include <QDateTime>
#include <QString>

#include <optional>

namespace clima {

struct Place {
    // The row id in the local places table. Zero until saved, and *not* an
    // identity anybody outside this machine shares.
    qint64 id = 0;

    // The GeoNames id. Zero for a place that did not come from GeoNames.
    qint64 geonamesId = 0;

    QString name;          // "Toronto"
    QString admin1;        // "Ontario" — state, province, région, prefecture
    QString country;       // "Canada"
    QString countryCode;   // "CA", ISO 3166-1 alpha-2, and what routes alerts
    QString timezone;      // "America/Toronto", IANA

    // Full precision, on purpose. Requests round to Coordinate::keyDecimals;
    // what the user chose is a different fact from what we ask for, and only
    // one of them can be recovered from the other.
    Coordinate coordinate;

    std::optional<double> elevationMetres;

    // Exactly one saved place is home, and the places table enforces that with
    // a partial unique index rather than trusting the code above it. Home is
    // what the app opens on, from cache, before any network call completes.
    bool isHome = false;

    int       sortOrder = 0;
    QDateTime addedAt;

    // "Ontario, Canada"; just the country where there is no first-level
    // division, and empty where there is neither.
    [[nodiscard]] QString region() const;

    // "Toronto, Ontario" — the string the location bar shows. Falls back to
    // the bare name rather than leaving a trailing comma.
    [[nodiscard]] QString label() const;

    // Whether two Places are the same place upstream. Compares geonamesId when
    // both have one, and the rounded coordinate otherwise — rounded, because
    // two pins a metre apart are the same pin as far as every request this
    // engine makes is concerned.
    [[nodiscard]] bool isSameEntity(const Place &other) const;
};

} // namespace clima
