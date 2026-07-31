// SPDX-FileCopyrightText: 2026 Jowi Aoun
// SPDX-License-Identifier: MPL-2.0
//
// A point on the earth, and the quantisation that keeps a map drag from
// hammering an API.
//
// ---- four decimal places, and why the number is not arbitrary ---------------
//
// One degree of latitude is about 111 km, so:
//
//     decimals   worst-case spacing     what it distinguishes
//     ---------  --------------------   ------------------------------------
//     2          ~1.1 km                neighbourhoods
//     3          ~110 m                 a city block
//     4          ~11 m                  a building
//     5          ~1.1 m                 a doorway
//
// Open-Meteo picks a *grid cell* from the coordinate — 1 to 11 km depending on
// the model, with 90 m DEM downscaling on top. MET Norway's terms ask outright
// for coordinates to be truncated to four decimals before they are sent. Four
// is therefore both the point where extra precision stops changing the answer
// and the number one of our providers asks for by name.
//
// The consequence that matters is the map. Dragging a map emits a stream of
// centre coordinates at full double precision, and every one of them is a
// distinct cache key, a distinct in-flight request and a distinct row in
// somebody's rate-limit ledger — for a forecast that is identical across all of
// them. Rounding first collapses the whole drag into one request.
//
// So: HttpClient rounds before it hashes *and* before it builds the URL, in one
// place, and nothing downstream has to remember. See libclima/net/httpclient.h.

#pragma once

#include <QString>

namespace clima {

struct Coordinate {
    double latitude  = 0.0;
    double longitude = 0.0;

    // The precision every cache key and every outbound request is quantised
    // to. Named rather than spelled `4` at each site, because the day it
    // changes it has to change everywhere at once or two callers disagree
    // about what the same place is called.
    static constexpr int keyDecimals = 4;

    [[nodiscard]] bool isValid() const;

    // Half-away-from-zero at `decimals` places. Not std::round on a scaled
    // double alone — that is what this does, but the scaling is written out so
    // the rounding mode is visible: banker's rounding would send two adjacent
    // coordinates to different cells depending on parity, which is a cache
    // miss nobody could reproduce.
    [[nodiscard]] Coordinate rounded(int decimals = keyDecimals) const;

    // The canonical spelling used in cache keys and in query strings:
    // "52.5200,13.4050" — fixed decimals, C locale, always a leading digit.
    //
    // C locale is load-bearing. QString::number honours QLocale in some
    // overloads and not others, and a comma decimal separator in a URL query
    // is a different request on a French machine than on an English one — the
    // kind of bug that only ever reproduces on somebody else's laptop.
    [[nodiscard]] QString toKeyString(int decimals = keyDecimals) const;

    // The two halves separately, for building a query string. Same formatting
    // rules as toKeyString().
    [[nodiscard]] QString latitudeString(int decimals = keyDecimals) const;
    [[nodiscard]] QString longitudeString(int decimals = keyDecimals) const;

    bool operator==(const Coordinate &other) const;
    bool operator!=(const Coordinate &other) const { return !(*this == other); }
};

} // namespace clima
