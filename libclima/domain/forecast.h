// SPDX-FileCopyrightText: 2026 Jowi Aoun
// SPDX-License-Identifier: MPL-2.0
//
// The forecast, in the shape the rest of Clima reads it — whoever produced it.
//
// Every provider adapts into this and nothing downstream knows which one
// answered except the one field that records it. That is design principle 2 in
// docs/04-architecture.md §4.1, "no provider name appears in UI code", made a
// type: a view model that switched on `providerId` would compile, and it would
// be the first crack in the fallback story, because the day MET Norway serves
// instead of Open-Meteo every such switch takes its default branch.
//
// ============================================================================
// EVERY VALUE IS OPTIONAL, AND THAT IS THE WHOLE POINT
//
// Every measurement below is a `Reading`, which is std::optional<double>. Not
// "every measurement a provider might lack" — every measurement, including the
// ones all of them have. libclima/domain/reading.h argues the case; the short
// version is that a plain double for a field MET Norway does not carry means
// the gust row reads "0 km/h" during a gale and nothing anywhere goes red.
//
// ============================================================================
// THE ONE CONVENTION THAT IS EASY TO GET WRONG: WHEN A NUMBER APPLIES
//
// Two kinds of quantity live side by side in an hourly series and they are not
// timestamped the same way:
//
//   instantaneous   temperature, humidity, pressure, wind, cloud cover.
//                   The value AT `time`.
//
//   accumulated     precipitation, rain, showers, snowfall — and, because it
//                   describes a stretch of weather rather than a moment, the
//                   weather code.
//                   The value over the hour ENDING AT `time`, i.e. the
//                   half-open interval [time - 1h, time).
//
// "Ending at" and not "beginning at", because that is Open-Meteo's convention —
// its documentation for `precipitation` reads "sum of the preceding hour" — and
// Open-Meteo is the primary. A domain model whose convention disagreed with its
// primary provider would put the shifting work in the common path instead of in
// the fallback, which is exactly backwards.
//
// MET Norway's convention is the other one: a `next_1_hours` block hanging off
// the entry at T describes [T, T+1h). Its adapter therefore shifts, and
// libclima/providers/metno/metnoforecastprovider.cpp explains the shift where
// it happens. This paragraph exists so that the next adapter's author asks the
// question at all.
//
// ============================================================================
// TIME IS UTC, EVERYWHERE IN HERE
//
// Same rule as libclima/core/clock.h: a stored or compared instant is UTC, and
// a local zone belongs at the moment a string is formatted for a human.
// `timeZone` on the Forecast is what that formatting uses; it is a property of
// the *place*, not of the machine, so that a saved location in Tokyo still
// renders its own evening when the app is running in Toronto.

#pragma once

#include "libclima/domain/coordinate.h"
#include "libclima/domain/reading.h"

#include <QDate>
#include <QDateTime>
#include <QList>
#include <QString>
#include <QTimeZone>

#include <optional>

namespace clima {

// WMO code 0-99 (docs/02-data-sources.md §2.2). Open-Meteo emits these
// directly; MET Norway's symbol vocabulary is mapped onto them in
// libclima/providers/metno/symbolcode.h, which documents the exact subset that
// mapping can produce — including four codes Open-Meteo never emits.
using WeatherCode = std::optional<int>;

// ---- a moment ---------------------------------------------------------------

struct CurrentConditions {
    QDateTime   time;
    Reading     temperature;          // °C
    Reading     apparentTemperature;  // °C
    Reading     relativeHumidity;     // %
    Reading     dewPoint;             // °C
    Reading     precipitation;        // mm, over the preceding hour
    Reading     windSpeed;            // km/h
    Reading     windGust;             // km/h
    Reading     windDirection;        // degrees, the direction it blows FROM
    Reading     pressureMsl;          // hPa, reduced to mean sea level
    Reading     cloudCover;           // %

    // Kilometres, and the unit is the whole reason this comment exists.
    // Open-Meteo reports visibility in metres; the axis in
    // app/qml/Clima/metrics.js runs 0 to 25 and detaildata.js labels it "km",
    // so storing metres would mean every consumer divided by a thousand and
    // the first one that forgot would draw a flat line pinned to the top of a
    // chart that looks like it is working. The division happens once, in the
    // adapter that knows the provider's unit.
    Reading     visibility;           // km
    Reading     uvIndex;
    WeatherCode weatherCode;

    // Daylight at `time` at this place. Optional rather than computed from the
    // sun's position, because a provider that says so is authoritative and a
    // provider that does not say so is not something to guess about.
    std::optional<bool> isDay;

    [[nodiscard]] bool isEmpty() const;
};

// ---- an hour ----------------------------------------------------------------
//
// Read the "when a number applies" section above before adding a field: which
// half of that table a new quantity belongs in is not obvious from its name,
// and getting it wrong shifts a rain bar by one column on one provider only.

struct HourlyPoint {
    QDateTime time;

    Reading temperature;
    Reading apparentTemperature;
    Reading relativeHumidity;
    Reading dewPoint;

    Reading precipitation;             // mm, the hour ending at `time`
    Reading rain;                      // mm, of which liquid
    Reading showers;                   // mm, of which convective
    Reading snowfall;                  // cm, of which snow
    Reading precipitationProbability;  // %

    Reading windSpeed;
    Reading windGust;
    Reading windDirection;

    Reading pressureMsl;               // hPa
    Reading cloudCover;                // %
    Reading visibility;                // km — see CurrentConditions above
    Reading uvIndex;

    WeatherCode         weatherCode;
    std::optional<bool> isDay;
};

// ---- a day ------------------------------------------------------------------
//
// `date` is a calendar date in the forecast's own time zone, which is why the
// zone is on the Forecast rather than left to the caller: two providers
// grouping the same hours by different midnights produce two different
// ten-day views of the same weather.

struct DailyPoint {
    QDate date;

    Reading temperatureMax;
    Reading temperatureMin;
    Reading apparentTemperatureMax;
    Reading apparentTemperatureMin;

    Reading precipitationSum;          // mm
    Reading rainSum;
    Reading showersSum;
    Reading snowfallSum;               // cm
    Reading precipitationProbabilityMax;
    Reading precipitationHours;

    Reading windSpeedMax;
    Reading windGustMax;
    Reading windDirectionDominant;

    Reading uvIndexMax;

    WeatherCode weatherCode;

    // Absent for a provider with no sun product — MET Norway's Locationforecast
    // is one, and its Sunrise 3.0 endpoint is a separate request we do not make.
    // A UI reading these must hide the sun arc rather than draw an arc from
    // midnight to midnight.
    QDateTime sunrise;
    QDateTime sunset;

    // Seconds between sunrise and sunset, and of that, seconds of direct sun.
    //
    // `daylight` is not derivable from the pair above and this is the field
    // that says so. Above the Arctic circle in summer Open-Meteo answers with
    // sunrise at that day's midnight and sunset at the *next* day's midnight —
    // two perfectly valid timestamps whose difference a Sun card will read as
    // zero unless it measures both from one reference (see
    // libclima/domain/timeaxis.h). 86400 here is the unambiguous statement that
    // the sun does not set today, and 0 that it does not rise.
    Reading daylightSeconds;
    Reading sunshineSeconds;

    // ---- the moon ------------------------------------------------------------
    //
    // Invalid where the event does not happen, which is ordinary rather than
    // exceptional: the moon fails to rise on about one calendar day a month,
    // because its rising drifts roughly fifty minutes later each day and
    // eventually skips a midnight. tests/fixtures/openmeteo/toronto-summer.json
    // has a null `moonrise` at index 7 for exactly that reason. A card that
    // treats an absent moonrise as an error will show one twelve times a year.
    QDateTime moonrise;
    QDateTime moonset;

    // 0 and 1 are new, 0.5 is full, and the number in between is a position in
    // the cycle rather than a brightness. Use `moonIllumination()` — the lit
    // fraction is not linear in the phase, and reading it as one reports a
    // waxing crescent as a quarter lit.
    Reading moonPhase;
};

// The lit fraction of the disc, 0 at new and 1 at full:
//
//     (1 - cos(2 * pi * phase)) / 2
//
// which is the projected width of the terminator and not an approximation of
// it. Absent in, absent out.
Reading moonIllumination(Reading moonPhase);

// The traditional name for a phase, untranslated — an identifier the UI looks
// up a localised string with, in the same spirit as the condition kinds in
// libclima/domain/weathercode.h: "new", "waxing-crescent", "first-quarter",
// "waxing-gibbous", "full", "waning-gibbous", "last-quarter",
// "waning-crescent".
//
// The four exact phases get a narrow window rather than an eighth of the cycle
// each. A moon called "full" for three and a half days is a label that has
// stopped meaning anything, and the night it is actually full is the night
// somebody looks up.
QString moonPhaseName(Reading moonPhase);

// ---- the whole answer -------------------------------------------------------

struct Forecast {
    // Which provider produced this. Recorded for the About screen, for the
    // "source: MET Norway" line the UI shows when the fallback served, and for
    // diagnostics — NOT for behaviour. See the header comment.
    QString providerId;

    // The coordinate the provider answered for, which is not always the one we
    // asked for: both Open-Meteo and MET Norway snap to a model grid cell and
    // say so in the response. Showing the requested point while displaying a
    // forecast for a cell 6 km away is a small lie that a Home screen with an
    // elevation label makes visible.
    Coordinate coordinate;

    // Metres. The grid cell's elevation, again the provider's own, which is
    // what makes its temperature make sense in a valley.
    Reading elevation;

    // The zone `DailyPoint::date` is grouped by and the zone a UI formats in.
    // May be invalid when a provider does not report one — MET Norway does not
    // — in which case the caller supplied it or it is UTC. See
    // ForecastRequest::timeZone.
    QTimeZone timeZone;

    // When the provider's model run was issued, if it says. Distinct from
    // `fetchedAt`: a forecast fetched thirty seconds ago can still be six hours
    // old, and "updated 25 minutes ago" in §4.5's stale-while-revalidate row is
    // about the second number while a user asking how fresh the *forecast* is
    // means the first.
    QDateTime issuedAt;

    // When we received it, from the injected Clock. Never the wall clock.
    QDateTime fetchedAt;

    CurrentConditions  current;
    QList<HourlyPoint> hourly;
    QList<DailyPoint>  daily;

    [[nodiscard]] bool isEmpty() const;

    // The hourly point covering `at`, or nullptr. Linear scan: the series is
    // 24 to 384 entries and a binary search would need the sort order to be a
    // documented invariant rather than a thing that happens to be true.
    [[nodiscard]] const HourlyPoint *hourAt(const QDateTime &at) const;
};

} // namespace clima
