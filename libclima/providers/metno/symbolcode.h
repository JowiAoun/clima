// SPDX-FileCopyrightText: 2026 Jowi Aoun
// SPDX-License-Identifier: MPL-2.0
//
// MET Norway's `symbol_code` vocabulary, translated into WMO weather codes.
//
// Open-Meteo speaks WMO 4677 codes 0-99 (docs/02-data-sources.md §2.2). MET
// Norway speaks strings: "lightrainshowers_day", "heavysleetandthunder",
// "partlycloudy_night". The domain model carries a WMO code because the primary
// provider does, so the fallback has to translate — and translation is where a
// fallback quietly becomes wrong, because a symbol nobody mapped becomes an
// icon nobody drew and there is no error anywhere.
//
// The vocabulary is 44 base symbols, from MET's own legend
// (metno/weathericons, weather/legend.csv). Every one of them is in the table
// in symbolcode.cpp, and tst_metno.cpp asserts that — the list is short enough
// to be exhaustive and long enough that "I think I got them all" is not a
// claim worth making.
//
// ============================================================================
// TWO THINGS THAT WILL SURPRISE THE NEXT READER
//
// ---- 1. Two of the official symbol names have a typo in them ----------------
//
//     lightssleetshowersandthunder
//     lightssnowshowersandthunder
//          ^
//
// That stray `s` is upstream's, in the published legend, in the live API. It is
// not a transcription error here and it must not be "fixed": the string in the
// payload is the one with the typo, and a table that spells it correctly
// matches nothing. Both spellings are accepted, so this keeps working on the
// day MET correct it — which they cannot really do without breaking every
// client that got it right.
//
// (Note that the correctly-spelled `lightrainshowersandthunder` exists in the
// same family. The typo is only in the sleet and snow variants.)
//
// ---- 2. Four of the codes produced here are codes Open-Meteo never emits ----
//
// MET's "sludd" — sleet, rain and snow falling together — has no equivalent in
// the subset of WMO codes Open-Meteo publishes. The full WMO 4677 table does
// have them:
//
//     68, 69   rain or drizzle and snow, slight / moderate or heavy
//     83, 84   showers of rain and snow mixed, slight / moderate or heavy
//
// and those are what this table produces, because they are what the weather
// *is*. The alternative — folding sleet onto 66/67, freezing rain, which is in
// Open-Meteo's subset — would put a value in the field that the UI already
// knows how to draw and that describes different weather. Freezing rain is
// liquid that freezes on contact and closes roads; sleet is wet snow.
//
// The consequence is real and belongs here rather than in a commit message: a
// weather-code-to-icon table written by reading Open-Meteo's documentation will
// have holes at 68, 69, 83 and 84, and those holes only appear when the
// fallback is serving, which is the least-tested path in the app. That is
// precisely the class of bug docs/06-roadmap.md is warning about when it says
// an untested fallback is not a fallback. metNoWeatherCodes() below returns the
// exact set this file can produce so that a test on the UI side can assert its
// icon table covers them.
//
// ---- and one that will not surprise anybody, but is a choice ---------------
//
// Every "...andthunder" symbol maps to 95, thunderstorm, and never to 96 or 99.
// Those two mean thunderstorm *with hail*, slight and heavy. MET's vocabulary
// does not distinguish hail at all, so emitting 96 would be inventing a
// meteorological claim out of the word "heavy". Intensity is lost; a fact we do
// not have is not gained.

#pragma once

#include "libclima/domain/forecast.h"

#include <QList>
#include <QString>

#include <optional>

namespace clima {

// What a `symbol_code` says, once the `_day` / `_night` / `_polartwilight`
// suffix has been taken off it.
struct SymbolCode {
    WeatherCode code;

    // Absent for a symbol with no variants — "cloudy", "rain", "fog" — which is
    // MET's way of saying the sky looks the same either way, not a gap. A
    // caller must not read "no day/night suffix" as "night".
    std::optional<bool> isDay;

    [[nodiscard]] bool isValid() const { return code.has_value(); }
};

// Parses one `symbol_code`. An unknown string returns an invalid SymbolCode
// rather than a guess — a weather code the UI cannot draw is better than a
// wrong one it can.
SymbolCode parseSymbolCode(const QString &symbol);

// Every WMO code this translation can produce, sorted and deduplicated. Exists
// so that a test — here or in the UI — can assert an icon table covers the
// fallback provider's output, including the four codes Open-Meteo never emits.
QList<int> metNoWeatherCodes();

// Every base symbol MET publishes, for the exhaustiveness test. Sorted.
QStringList metNoSymbolNames();

} // namespace clima
