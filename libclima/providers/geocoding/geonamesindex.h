// SPDX-FileCopyrightText: 2026 Jowi Aoun
// SPDX-License-Identifier: MPL-2.0
//
// Thirty-one thousand cities in four hundred kilobytes, and the nearest one to
// a point, without a network.
//
// ============================================================================
// WHY THIS IS OFFLINE, WHICH IS THE INTERESTING PART
//
// Reverse geocoding — a coordinate in, "Toronto, Ontario" out — has one obvious
// implementation and it does not work. Nominatim is the OpenStreetMap
// geocoder, it is free, and asked once from a developer machine with a
// properly identifying User-Agent naming the project and a contact address, it
// answered HTTP 403 "Access denied" to the FIRST request. Not the eleventh, not
// after a burst: the first. The OSM Foundation's usage policy is enforced
// rather than aspirational, and a desktop app that ships a Nominatim call for
// every user's location ships a feature that is already broken.
//
// So the lookup happens here, against a dataset compiled into the binary. Four
// things follow from that, and each of them would be worth the four hundred
// kilobytes on its own:
//
//   1. It dodges the 403. There is no request to refuse.
//
//   2. It is DETERMINISTIC. The same coordinate produces the same city on
//      every machine, forever, with no clock and no network in the answer.
//      docs/04-architecture.md §4.11 wants golden-image chart tests, and a
//      golden image with a place name in the corner is only golden if that
//      name cannot change under it.
//
//   3. It works offline, which is the app's headline property. "Renders from
//      cache, then reconciles" (§4.1, design principle 1) is a promise that a
//      network call in the *labelling* path would quietly break: the forecast
//      would come up from cache and the place name would not.
//
//   4. It is the SAME DATASET the forward geocoder searches. Open-Meteo's
//      geocoding API is GeoNames; this file is GeoNames. Type "Toronto" and
//      the API returns geonameid 6167865. Stand at 43.65 N, 79.38 W and this
//      index returns geonameid 6167865. One entity, one id, one saved place —
//      so the app cannot end up with a searched Toronto and a detected Toronto
//      as two rows in the places table. Nominatim would have returned an OSM
//      relation id, which has no correspondence to a GeoNames id at all, and
//      the two paths could never have been reconciled.
//
// ============================================================================
//
// ---- nearest is the wrong answer, and the fix is one byte a row -------------
//
// Stand at Yonge and Queen in downtown Toronto. The nearest row in this
// dataset is Moss Park, 880 m away, population 20 506 — a neighbourhood.
// Then Etobicoke, then Thornhill. Toronto itself is 6.4 km away, because a
// city's row sits at its centroid and a city is bigger than a point. The same
// thing happens in Singapore, in Tokyo and in Paris: plain nearest-neighbour
// over GeoNames answers with a subdivision nobody outside the city has heard
// of.
//
// So every row carries a modelled radius — its *reach* — derived at pack time
// from population by treating the settlement as a disc at a typical urban
// density: r = sqrt(P / (pi * rho)). Toronto gets 21 km, Reykjavík 4.4 km, a
// 16 000-person town 1.6 km. tools/geonames/pack.mjs derives it and explains
// the constant.
//
// The rule then has two steps, and the second one is not decoration:
//
//   * Among candidates within the cutoff, take the smallest d / reach — the
//     settlement whose footprint you are furthest *inside*. If that ratio is
//     at most 1, you are standing in it, and that is the answer.
//
//   * If every ratio exceeds 1 you are standing in open country, and the
//     ranking flips to plain distance. Without this step, a point 30 km from a
//     village and 60 km from a city would be labelled with the city, because a
//     big reach forgives a big distance — which is right when you are inside
//     the city and wrong when you are not.
//
// ---- the grid ---------------------------------------------------------------
//
// A flat five-degree grid, 36 by 72, with the rows sorted by cell so that a
// cell is a contiguous range and the index is one array of start offsets. A
// query builds a latitude/longitude bounding box for the cutoff radius, walks
// the cells it touches and computes an exact great-circle distance for each
// row inside them. At the default 250 km that is a few hundred rows, which is
// microseconds; a brute-force scan of all 31 673 would also be fast enough,
// and the grid is here because the cutoff makes most of the file irrelevant
// and there is no reason to touch it.
//
// The longitude half-width of the box is taken at the latitude *furthest from
// the equator* that the box reaches, because a degree of longitude shrinks
// with cos(latitude) and a box computed at the query's own latitude would be
// too narrow at its poleward edge. Near the poles the window opens to the
// whole circle rather than dividing by a cosine approaching zero.
//
// ---- the file format --------------------------------------------------------
//
// Written by tools/geonames/pack.mjs, which documents the encoder's half. All
// integers little-endian.
//
//   HEADER, uncompressed, 100 bytes
//     0   4   magic "CLGX"
//     4   4   format version, 1
//     8   4   row count
//    12   4   grid cell size, whole degrees
//    16   4   latitude cell count
//    20   4   longitude cell count
//    24   4   country count
//    28   4   admin1 name count
//    32   4   timezone name count
//    36   4   coordinate scale, 10000
//    40   4   reach step, metres
//    44   4   section count, 12
//    48   4   payload length before compression
//    52   48  twelve section lengths, uncompressed
//
//   PAYLOAD, one qCompress-format block: four bytes of big-endian length then
//   a zlib stream. Twelve sections back to back, in this order:
//
//     0  cellStarts      zigzag varint deltas, cellCount + 1 values
//     1  latitudes       zigzag varint deltas, in units of 1e-4 degrees
//     2  longitudes      zigzag varint deltas
//     3  geonamesIds     zigzag varint deltas
//     4  reach           one byte a row, in units of the header's reach step
//     5  countryIndex    one byte a row
//     6  admin1Index     plain varints
//     7  timezoneIndex   plain varints
//     8  countryCodes    two ASCII bytes each
//     9  admin1Names     newline-separated UTF-8, entry 0 is empty
//    10  timezoneNames   newline-separated UTF-8
//    11  cityNames       newline-separated UTF-8, one a row
//
// Columnar rather than row-major, and that is worth a third of the file: a
// row-major layout interleaves four unrelated kinds of number and deflate
// finds no runs in it. Measured on this data, 664 KB row-major against 413 KB
// columnar.
//
// ---- coordinates are stored to four decimals, and that is not a compromise --
//
// `Coordinate::keyDecimals` is 4. Every outbound request in this engine is
// rounded to four decimals before it is hashed or sent — libclima/domain/
// coordinate.h explains why, and MET Norway's terms ask for it by name. A
// fifth decimal stored here would be a digit no cache key, no URL and no
// comparison in the product could ever see.
//
// The packer rounds half away from zero, exactly as `Coordinate::rounded`
// does, so a place found by reverse geocoding and the same place found by
// searching Open-Meteo produce bit-identical doubles once both are rounded.
// tests/tst_reversegeocode.cpp asserts that against a recorded response.
//
// ---- memory ------------------------------------------------------------------
//
// About a megabyte once decoded, and none of it until something calls load().
// The name blob is kept as bytes with an offset table rather than as 31 673
// QStrings, which would be closer to two megabytes for a table from which
// exactly one row is ever read.

#pragma once

#include "libclima/core/result.h"
#include "libclima/domain/coordinate.h"

#include <QByteArray>
#include <QString>
#include <QStringList>
#include <QVector>

#include <optional>

namespace clima {

// One row, decoded.
struct GeonamesCity {
    quint32    geonamesId = 0;
    Coordinate coordinate;         // already at Coordinate::keyDecimals
    QString    name;               // "Reykjavík", in the dataset's own spelling
    QString    countryCode;        // ISO 3166-1 alpha-2
    QString    admin1;             // resolved to its name; empty where a country has none
    QString    timezone;           // IANA
    double     reachKm = 0.0;      // the modelled radius, see the header comment
};

class GeonamesIndex
{
public:
    GeonamesIndex();
    ~GeonamesIndex();

    GeonamesIndex(const GeonamesIndex &) = delete;
    GeonamesIndex &operator=(const GeonamesIndex &) = delete;

    // Where the packed index lives once CMake has compiled it in. Public so a
    // diagnostic can print it and a test can prove the resource is present
    // rather than inferring it from a successful load.
    [[nodiscard]] static QString bundledResourcePath();

    // Reads the bundled resource. Idempotent: a second call is a no-op, so
    // callers may treat it as "make sure this is ready" rather than having to
    // track whether it has happened.
    Status loadBundled();

    // For tests, and for a future user-supplied index. Takes the packed bytes,
    // not a path.
    Status load(const QByteArray &packed);

    [[nodiscard]] bool isLoaded() const { return m_loaded; }
    [[nodiscard]] int  cityCount() const { return m_rowCount; }

    // Undefined for a row outside [0, cityCount). Callers get rows from
    // nearest(), which does not invent them.
    [[nodiscard]] GeonamesCity cityAt(int row) const;

    struct Match {
        int    row = -1;
        double distanceKm = 0.0;
        double reachKm = 0.0;

        // True when the point is inside the settlement's modelled footprint —
        // "you are in Toronto" rather than "the nearest place is Toronto".
        // The caller may want to say it differently.
        bool insideFootprint = false;
    };

    // The best row within `maxDistanceKm`, by the two-step rule in the header
    // comment. Empty when nothing is that close, which is the honest answer
    // for a point in the ocean.
    [[nodiscard]] std::optional<Match> nearest(const Coordinate &at, double maxDistanceKm) const;

    // Great-circle distance on a sphere of the earth's mean radius. Exposed
    // because the test that checks the ranking rule needs the same number the
    // ranking used, and computing it twice from two formulas is how a test
    // ends up asserting its own arithmetic.
    [[nodiscard]] static double distanceKm(const Coordinate &a, const Coordinate &b);

    // Kilometres per degree of latitude on that sphere. The bounding box needs
    // it and so does anything converting a cutoff into a window.
    static constexpr double kilometresPerDegree = 111.19492664455873;

private:
    [[nodiscard]] int cellIndex(int latitudeCell, int longitudeCell) const;

    bool m_loaded = false;

    int m_rowCount = 0;
    int m_cellDegrees = 0;
    int m_latitudeCells = 0;
    int m_longitudeCells = 0;
    int m_coordinateScale = 1;
    int m_reachStepMetres = 0;

    QVector<qint32>  m_latitudes;      // in units of 1 / m_coordinateScale degrees
    QVector<qint32>  m_longitudes;
    QVector<quint32> m_geonamesIds;
    QVector<quint32> m_cellStarts;     // cellCount + 1 entries

    QByteArray m_reach;                // one byte a row
    QByteArray m_countryIndex;         // one byte a row
    QVector<quint16> m_admin1Index;
    QVector<quint16> m_timezoneIndex;

    QStringList m_countryCodes;
    QStringList m_admin1Names;
    QStringList m_timezoneNames;

    // The city names stay as bytes. m_nameOffsets has rowCount + 1 entries so
    // that a row's length is the difference between two of them.
    QByteArray       m_nameBlob;
    QVector<quint32> m_nameOffsets;
};

} // namespace clima
