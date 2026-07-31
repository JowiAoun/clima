// SPDX-FileCopyrightText: 2026 Jowi Aoun
// SPDX-License-Identifier: MPL-2.0
//
// The two halves of geocoding, and why they are two interfaces rather than one.
//
// docs/04-architecture.md §4.3 lists `igeocode.h` beside `iforecastprovider.h`
// as one more provider interface. It turned out to be two, and the split is not
// tidiness — it is the shape of the problem:
//
//   FORWARD, a name in and places out, is a network provider. It is
//   asynchronous, it can fail with every ErrorKind in the enum, it is cached
//   for seven days (§4.5), and it is answered by Open-Meteo's hosted GeoNames
//   index, which can afford to search a hundred alternate spellings of Toronto
//   in forty languages. Nothing local could.
//
//   REVERSE, a coordinate in and one place out, is a lookup in a table. It is
//   synchronous, it cannot time out, it cannot be rate-limited, and it needs no
//   User-Agent — because it is a bundled dataset and not a request. See
//   libclima/providers/geocoding/geonamesindex.h for why it had to be, which
//   begins with Nominatim answering 403 to the first request ever sent to it.
//
// Giving both of those the same QFuture-returning signature would mean the
// reverse path pretends it might fail slowly, and every caller writing a
// continuation for an answer that was available before the call returned.
//
// ---- both sides speak GeoNames ----------------------------------------------
//
// The thing that makes the pair coherent is that Open-Meteo's geocoder and the
// bundled index are the *same upstream dataset*. Type "Toronto" and get
// geonameid 6167865; stand in Toronto and get geonameid 6167865. One entity,
// one `Place::geonamesId`, one row in the places table. A reverse geocoder
// built on Nominatim would have returned an OSM relation id, which has no
// correspondence to a GeoNames id at all — so the app could never have known
// that the place the user searched for and the place they were standing in
// were the same place.

#pragma once

#include "libclima/core/result.h"
#include "libclima/domain/coordinate.h"
#include "libclima/domain/place.h"

#include <QFuture>
#include <QList>
#include <QString>

namespace clima {

struct GeocodeQuery {
    // What the user typed. Not trimmed or case-folded here — the provider
    // does that, because what counts as whitespace is a per-provider question
    // and the cache key has to be built from whatever it decided.
    QString name;

    // How many results to ask for. Open-Meteo caps at 100; ten is a search
    // popover's worth and more than a person reads.
    int count = 10;

    // The language the *names* come back in — "Berlin" against "Berlín" — as
    // an ISO 639-1 code. Part of the cache key: §4.5 says geocoding results
    // are cached "keyed by query+lang", and two languages are two answers.
    QString language = QStringLiteral("en");
};

// A forward geocoder. Asynchronous, network-backed, and the only part of
// geocoding that can fail in the ways docs/04-architecture.md §4.4 enumerates.
class IForwardGeocoder
{
public:
    virtual ~IForwardGeocoder();

    [[nodiscard]] virtual QString id() const = 0;

    // Shown in About → Data sources. A licence obligation, not a nicety:
    // docs/02-data-sources.md §2.9 requires the GeoNames credit under CC-BY
    // 4.0 and the Open-Meteo credit alongside it.
    [[nodiscard]] virtual QStringList attribution() const = 0;

    // Empty query, or one shorter than the provider's minimum, resolves to an
    // empty list rather than an error: a person who has typed one character
    // has not made a mistake.
    virtual QFuture<Result<QList<Place>>> search(const GeocodeQuery &query) = 0;

    // Re-reads one place by its GeoNames id. This is how a saved place is
    // refreshed after an upstream rename, and it is the reason
    // `Place::geonamesId` is stored at all.
    virtual QFuture<Result<Place>> resolve(qint64 geonamesId) = 0;
};

// What a reverse lookup found.
struct ReverseMatch {
    Place  place;
    double distanceKm = 0.0;

    // True when the point is inside the settlement's modelled footprint —
    // "you are in Toronto" rather than "the nearest town is Toronto". The UI
    // may want to phrase those differently; nothing in the engine does.
    bool insideFootprint = false;
};

// A reverse geocoder. Synchronous, because the only implementation is a table
// lookup and pretending otherwise would cost every caller a continuation.
class IReverseGeocoder
{
public:
    virtual ~IReverseGeocoder();

    [[nodiscard]] virtual QString      id() const = 0;
    [[nodiscard]] virtual QStringList  attribution() const = 0;

    // ErrorKind::Unsupported when there is no populated place within the
    // cutoff — a point in the ocean, or deep in a desert. That is not a
    // failure and the UI must not report it as one: §4.4 says a provider that
    // returns nothing makes the UI *hide* the feature rather than show a
    // broken one, and here that means showing the coordinate instead of
    // inventing a city four hundred kilometres away.
    [[nodiscard]] virtual Result<ReverseMatch> reverse(const Coordinate &at) const = 0;
};

} // namespace clima
