// SPDX-FileCopyrightText: 2026 Jowi Aoun
// SPDX-License-Identifier: MPL-2.0
//
// WMO code 4677/4501, and the three questions the UI asks it.
//
// Every provider Clima will ever use reports the state of the sky as a small
// integer from the World Meteorological Organization's present-weather tables
// — Open-Meteo says so outright (docs/02-data-sources.md §2.2, "WMO weather
// codes 0–99"), MET Norway's symbol codes map onto them, and ECCC's do too.
// So the code is the one piece of a forecast that is genuinely portable
// between providers, and it belongs in domain/ rather than in any one adapter.
//
// The UI asks a code exactly three things, and they are three different
// questions with three different answers:
//
//   1. WHAT KIND of precipitation is this?  ->  PrecipitationType
//      app/qml/Clima/precip.js draws a different particle field for drizzle,
//      rain, sleet, snow, hail and thunder, and its `cellFor(mm, tempC, code)`
//      already takes a code as its third argument for exactly this. It wants
//      one of *its* six names, not a number — passing 95 straight through
//      would put "95" in STYLE[c.type], miss, and silently fall back to rain,
//      so a thunderstorm would draw as ordinary rain and nothing would say so.
//
//   2. WHICH GLYPH goes above the hour?  ->  ConditionKind
//      A different question, because the answer depends on the sun: code 0 is
//      a sun at noon and a moon at midnight. Hence the `isDay` argument.
//
//   3. WHAT DO I CALL IT?  ->  conditionText()
//      One short localised phrase. Translated here rather than in QML because
//      the code-to-phrase table is data and would otherwise be duplicated in
//      every front end that reuses the engine — the applet, the extension and
//      the CLI in D6 all need the same sentence.
//
// ---- what a code cannot tell you --------------------------------------------
//
// How hard it is raining, in millimetres. WMO distinguishes slight/moderate/
// heavy in the code itself (61/63/65), but only in bands, and a provider's
// choice of band is not the same decision as the NWS thresholds precip.js
// classifies intensity with. So intensity stays where it is — derived from the
// amount — and the code only ever decides *type*. That split is why
// `precipitationTypeFor` returns a type and not a whole cell.

#pragma once

#include <QString>

namespace clima {

// The six shapes app/qml/Clima/precip.js knows how to draw, plus None.
//
// Not a meteorological taxonomy: precip.js says so in its own header — "two
// kinds of falling water that look different get two entries, and two that
// look the same share one". Freezing drizzle is drizzle here because it falls
// like drizzle; that it freezes on contact is a hazard the alert layer
// reports, not a difference in the particle field.
enum class PrecipitationType {
    None,
    Drizzle,
    Rain,
    Sleet,
    Snow,
    Hail,
    Thunder,
};

// What the hour looks like, which is a larger vocabulary than the six above
// because most hours have no precipitation at all and still need a picture.
enum class ConditionKind {
    ClearDay,
    ClearNight,
    PartlyDay,
    PartlyNight,
    Cloudy,
    Fog,
    Drizzle,
    Rain,
    RainNight,
    Sleet,
    Snow,
    Thunder,
    Hail,
};

// ---- the strings QML matches on ---------------------------------------------
//
// These are contracts with app/qml/Clima/, not debug output. `WeatherGlyph.qml`
// switches on the string, and `mockdata.js` already uses the seven spellings it
// understands, so the names below are chosen to be those seven verbatim:
//
//     clear-day  clear-night  partly-day  partly-night  cloudy  rain  rain-night
//
// The six that are new — fog, drizzle, sleet, snow, thunder, hail — are the
// ones real data has and a hand-written mock never did. WeatherGlyph.qml draws
// *nothing* for a kind it does not recognise (every one of its `hasSun`,
// `hasCloud`, `hasRain` booleans is false and the item renders empty), so until
// its vocabulary grows, a caller should pass anything it gets through
// `drawableToday()` below. That function is here rather than in QML so the day
// the glyph set is completed, deleting it is one commit in one place.
QString precipitationTypeName(PrecipitationType type);
QString conditionKindName(ConditionKind kind);

// The nearest kind WeatherGlyph.qml can currently draw. Degrades by one step
// and never by more: snow and fog become cloudy (both are a sky you cannot see
// through), sleet, thunder, hail and drizzle become rain (all of them are
// falling water). An empty glyph would be a worse lie than a slightly wrong
// one, because an empty glyph reads as "no data".
ConditionKind drawableToday(ConditionKind kind);

// ---- the tables --------------------------------------------------------------
//
// A code outside the tables is not an error and not a guess: `None`,
// `Cloudy` and an empty string respectively. WMO defines codes we will never
// see from a forecast model (duststorms, tornado-adjacent 19), and inventing a
// picture for one is how a chart ends up claiming something nobody forecast.

PrecipitationType precipitationTypeFor(int wmoCode);
ConditionKind     conditionFor(int wmoCode, bool isDay);

// One short phrase, localised. Empty for a code we have no wording for, which
// the UI already renders as "—".
QString conditionText(int wmoCode, bool isDay);

} // namespace clima
