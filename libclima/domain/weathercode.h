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
//      a sun at noon and a moon at midnight. Hence the `isDay` argument. Only
//      the four kinds you can see sky through take it — see
//      `dayAndNightDifferOnlyWhereTheSkyIsVisible` in the tests.
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

#include <QList>
#include <QString>

#include <optional>

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
// switches on the string, and every one of the thirteen below has a picture
// there:
//
//     clear-day  clear-night  partly-day  partly-night  cloudy
//     fog  drizzle  rain  rain-night  sleet  snow  thunder  hail
//
// Six of them did not, for as long as the glyph set was the seven the prototype
// had drawn against mock data. A `drawableToday()` used to stand here and fold
// the other six into those seven — fog and snow to cloudy, drizzle, sleet,
// thunder and hail to rain — so that nothing rendered as an empty item. It was
// always meant to be deleted the day the glyphs existed, and they do now.
//
// What it cost while it stood is worth recording, because it is the failure
// mode of every "degrade for now" helper: the app drew an ordinary shower over
// a WMO 95, and a shower is a *plausible* picture of a thunderstorm, so nothing
// looked broken. The default Toronto fixture has carried a WMO 95 in its ten-day
// strip the whole time, and fifty recorded golden images went past it without
// one of them being the thing that noticed.
QString precipitationTypeName(PrecipitationType type);
QString conditionKindName(ConditionKind kind);

// ---- the tables --------------------------------------------------------------
//
// A code outside the tables is not an error and not a guess: `None`,
// `Cloudy` and an empty string respectively. WMO defines codes we will never
// see from a forecast model (duststorms, tornado-adjacent 19), and inventing a
// picture for one is how a chart ends up claiming something nobody forecast.

PrecipitationType precipitationTypeFor(int wmoCode);
ConditionKind     conditionFor(int wmoCode, bool isDay);

// ---- folding several hours into one picture ---------------------------------
//
// One glyph often has to stand for more than one hour. The ten-day strip's day
// card is a whole day in one icon; the chart's header band draws a glyph every
// second column, because two dozen 27 px icons will not fit across a plot. Both
// are the same question — given several codes, which one is the picture? — and
// they take different answers, for a reason worth writing down.
//
// The table is two scales end to end. 0-3 is a cloud-cover ramp: clear, mainly
// clear, partly cloudy, overcast. From 45 up it stops describing how much sky
// is showing and starts naming a thing that is happening — fog, drizzle, rain,
// snow, showers, lightning. Only the second half is ordered by how much a
// reader needs to know.

// A whole day, or any span where no single hour has a claim on the label.
// The numerically largest, which is exactly what Open-Meteo's own daily
// `weather_code` is: measured against its own hourly series over 8 places and
// 16 days on 2026-08-22, the daily value was the maximum of that day's 24
// hourly codes on 125 of 125 complete days.
//
// "Ordered by severity" is doing real work and it is only roughly true: 82
// (violent rain showers) outranks 75 (heavy snow), so a snowy day with one
// shower in it is drawn as a shower. The alternative is a hand-ranked table of
// a hundred codes that nobody can check against anything, and over a whole day
// the approximation is the same one the provider already made.
//
// Absent in, absent out: an empty list has no picture, which is different from
// a clear sky.
[[nodiscard]] std::optional<int> mostSignificantCode(const QList<int> &codes);

// A short span that one label names, where `codes` is in time order and
// `codes.first()` is the labelled hour.
//
// The label says "2 PM" and the reader reads the glyph beside it as two o'clock,
// so two o'clock's sky is what it shows — unless something is happening
// somewhere in the span, in which case that is shown instead, because a column
// that quietly drops the only stormy hour it covers is the failure this exists
// to prevent.
//
// Folding the whole span with `mostSignificantCode` instead was tried and is
// wrong in the common case: it takes the cloudier of two adjacent hours every
// time, so an afternoon that is clear until four reports overcast from two, and
// the reference capture of the app's own front page changed from a sunny
// afternoon to a cloudy one. Cloud cover is a ramp and reading its maximum is
// not a severity judgement; rain is an event and missing one is.
[[nodiscard]] std::optional<int> codeForLabelledSpan(const QList<int> &codes);

// One short phrase, localised. Empty for a code we have no wording for, which
// the UI already renders as "—".
QString conditionText(int wmoCode, bool isDay);

} // namespace clima
