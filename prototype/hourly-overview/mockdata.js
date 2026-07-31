// SPDX-FileCopyrightText: 2026 Jowi Aoun
// SPDX-License-Identifier: GPL-3.0-or-later
// Stand-in for the Open-Meteo provider.
//
// 48 hourly samples starting at 21:00. The values at the labelled indices are
// hand-tuned to match the MSN screenshot this prototype is modelled on, so the
// two can be compared side by side. Shape of the API deliberately mirrors what
// libclima's ForecastProvider will return: parallel per-hour arrays plus derived
// helpers, no formatting decisions baked in.
.pragma library

.import "precip.js" as Precip

// `nowIndex` is the one number in this file that has to agree with another
// file. detaildata.js observes at 12:28 PM, and two stand-ins describing the
// same forecast for the same place have to describe the same instant or the
// page contradicts itself: for a while this said index 3, which is midnight,
// and the hourly strip drew moon glyphs under a hero showing a sun and 27°.
//
// Index 15 is 12:00 — the hour that observation falls in — and it is the index
// detaildata.js was already written against. Its twelve-hour context window is
// `precipProb[9..20]` verbatim, and its own `nowIndex` of 6 lands exactly here.
// So this is the marker being moved to where the rest of the data already
// thought it was, not a new position chosen for it.
//
// Moving the marker rather than the series is what keeps the arrays below
// worth comparing to the reference: every value stays at the clock hour it was
// authored for, and the labelled hours are still MSN's, column for column.
// Fifteen observed hours is a lot of past to carry, and it is the honest
// amount — the series starts at 21:00 the evening before.
var startHour       = 21;   // index 0 is 21:00 on day 0
var nowIndex        = 15;   // 12:00, the hour detaildata.js observes
var firstLabelIndex = 1;    // label every 2 h from 22:00, so the curve starts
var labelStep       = 2;    // before the first label — as it does in MSN

var temperature = [
    19.4, 19.0, 19.0, 19.0, 18.6, 18.3, 18.0, 18.0, 18.2, 18.0, 18.4, 19.0,
    20.5, 22.0, 23.2, 24.0, 24.6, 25.0, 25.5, 26.0, 26.0, 25.4, 24.6, 23.0,
    22.2, 21.5, 21.0, 20.5, 20.0, 19.6, 19.2, 19.0, 19.6, 20.8, 22.6, 24.2,
    25.6, 26.6, 27.4, 28.0, 28.2, 27.6, 26.6, 25.4, 24.2, 23.0, 22.2, 21.4
];

// Chance of precipitation. This was hand-tuned to the MSN screenshot like the
// two series around it, and it no longer is at the wet hours — deliberately,
// and the divergence is the point. The reference forecast is dry, so a mock
// copied from it can only ever demonstrate a precipitation effect by not
// having any. The wet hours below carry the probability their amounts imply;
// everywhere else these are still the reference's numbers.
//
// A 4 % chance of 8.6 mm would have been the more embarrassing thing to ship.
var precipProb = [
    62, 55, 46, 40, 34, 20, 14, 12, 10,  8,  6,  5,
     6,  9, 14, 22, 34, 55, 72, 88, 80, 62, 45, 30,
    18, 12,  8,  6,  8, 12, 16, 20, 26, 34, 48, 58,
    50, 32, 20, 12,  8,  6,  5,  7, 10, 14, 18, 16
];

// Overcast where it rains, because it cannot rain out of a half-clear sky and
// the Cloud cover tab is one tab away from the wash that says it is raining.
// Elsewhere these are still the reference's numbers.
var cloud = [
    88, 90, 84, 78, 74, 46, 42, 38, 32, 30, 26, 34,
    32, 36, 40, 38, 52, 84, 92, 96, 94, 88, 76, 66,
    40, 36, 40, 44, 48, 52, 46, 40, 34, 46, 78, 88,
    80, 46, 32, 26, 22, 28, 36, 44, 52, 60, 66, 70
];

var count = temperature.length;

// Millimetres in the hour *starting* at each index — the convention every
// provider uses, and the one the wash under the chart is drawn on.
//
// Four spells, chosen to be a day someone would actually plan around rather
// than to exercise a switch statement: last night's rain, over by 01:00 and
// behind the now line by the time the page opens, a thunder-free but genuinely
// heavy band through the afternoon that climbs light → moderate → heavy →
// moderate and back, its own tail, and a light band after tomorrow's sunrise.
// The afternoon band begins two hours after "now", which is what lets
// detaildata.js say "dry now, rain from 2 p.m." and be right. Sleet, snow and
// hail cannot occur at these temperatures and so are not here; the gallery is
// where those live, which is what the gallery is for.
var precipMm = _buildPrecipMm();

function _buildPrecipMm() {
    var out = [];
    for (var i = 0; i < count; ++i)
        out.push(0);

    // 21:00 – 01:00, easing off. Entirely behind `nowIndex`, so the wash and
    // the past veil are composited over each other on first paint — which is
    // the one pair of layers here that can be got wrong and stay unnoticed,
    // since neither is ever seen over the other anywhere else.
    out[0]  = 0.35; out[1]  = 0.30; out[2]  = 0.22; out[3] = 0.15; out[4] = 0.11;

    // 14:00 – 18:00, the event of the day.
    out[17] = 0.6;  out[18] = 2.9;  out[19] = 8.6;  out[20] = 5.1; out[21] = 1.4;
    out[22] = 0.3;  out[23] = 0.15;

    // 07:00 – 09:00 the next morning.
    out[34] = 0.6;  out[35] = 1.1;  out[36] = 0.7;
    return out;
}

// The same hours, classified: type and intensity per hour, null where dry.
// Derived rather than typed, so the amounts above stay the single source of
// truth and no hour can be drizzling in the chart and pouring in the strip.
//
// Type falls out of temperature here because the mock has no weather codes.
// Open-Meteo sends a WMO code per hour and it is strictly better — it is the
// only way to know thunder or hail is involved — so `Precip.cells` takes one
// as its third argument, ready for the provider that has it.
var precipCells = Precip.cells(precipMm, temperature);

// Apparent temperature: humidity pushes the warm hours up, night wind pulls the
// cool hours down. Real values come from Open-Meteo's apparent_temperature.
var apparent = _buildApparent();

function _buildApparent() {
    var out = [];
    for (var i = 0; i < count; ++i) {
        var t = temperature[i];
        var delta = t >= 24 ? 1.2 + (t - 24) * 0.4
                            : (t <= 19 ? -1.2 : -0.4);
        out.push(Math.round((t + delta) * 10) / 10);
    }
    return out;
}

// ---------------------------------------------------------------------------
// Derived series for the other metric tabs.
//
// Generated from temperature/cloud/hour rather than hand-typed, so they stay
// internally coherent (humidity tracks temperature inversely, visibility drops in
// rain, air quality peaks at rush hour and clears in wind). Deterministic on
// purpose — no Math.random — so golden-image tests stay stable.
// ---------------------------------------------------------------------------

function _clamp(v, lo, hi) { return v < lo ? lo : (v > hi ? hi : v); }
function _round(v, d) { var m = Math.pow(10, d || 0); return Math.round(v * m) / m; }
function _hourOfDay(i) { return (startHour + i) % 24; }

function _build(fn) {
    var out = [];
    for (var i = 0; i < count; ++i)
        out.push(fn(i));
    return out;
}

var humidity = _build(function (i) {
    return _round(_clamp(95 - (temperature[i] - 17) * 3.8 + Math.sin(i * 0.7) * 2.5, 32, 99), 0);
});

var windSpeed = _build(function (i) {
    var h = _hourOfDay(i);
    var diurnal = 10 + 7 * Math.sin((h - 9) / 24 * 2 * Math.PI);
    return _round(_clamp(diurnal + 3 * Math.sin(i * 0.55) + cloud[i] * 0.04, 1, 39), 1);
});

var windGust = _build(function (i) {
    return _round(windSpeed[i] * (1.5 + 0.22 * Math.sin(i * 0.9)), 1);
});

var windDirection = _build(function (i) {
    return _round((215 + 45 * Math.sin(i * 0.28) + 360) % 360, 0);
});

var pressure = _build(function (i) {
    return _round(1013 + 5.5 * Math.sin((i + 8) / 48 * 2 * Math.PI) - cloud[i] * 0.025, 1);
});

var uvIndex = _build(function (i) {
    var h = _hourOfDay(i) + 0.5;
    if (h < 6 || h > 20)
        return 0;
    var arc = Math.sin((h - 6) / 14 * Math.PI);
    return _round(_clamp(arc * 9.2 * (1 - cloud[i] / 210), 0, 11), 1);
});

var visibility = _build(function (i) {
    var v = 24 - cloud[i] * 0.11
              - (precipMm[i] > 0 ? 9 : 0)
              - (humidity[i] > 90 ? 6 : 0);
    return _round(_clamp(v, 0.5, 25), 1);
});

// European AQI (0 good … 100+ extremely poor).
var airQuality = _build(function (i) {
    var h = _hourOfDay(i);
    var rush = Math.exp(-Math.pow((h - 8) / 2.2, 2)) + Math.exp(-Math.pow((h - 18) / 2.6, 2));
    var v = 17 + rush * 25 + (1 - windSpeed[i] / 32) * 13 - (precipMm[i] > 0 ? 8 : 0);
    return _round(_clamp(v, 4, 100), 0);
});

// ---------------------------------------------------------------------------
// Daily summaries for the day strip. Values match the reference screenshot.
// ---------------------------------------------------------------------------
// Every day carries both a daytime and a night-time condition. Unselected cards
// show only the daytime one; selecting a card reveals the pair.
//
// `month` is carried on every entry because the list crosses one: the desktop
// day strip never needed to know, and the moment a calendar screen existed
// "the 1st" stopped being unambiguous. `precip` is the daily probability the
// mobile ten-day strip shows under each column.
//
// Ten days forward of today, plus yesterday. Ten because that is what the
// screen it feeds is called, and the desktop strip — which only ever showed
// what fitted and paged the rest — gets the extra columns for free.
// `weekday` is carried rather than derived because `label` is not one: the
// first two entries are "Yesterday" and "Today", and the mobile week strip
// needs the actual day of the week under both of them.
var days = [
    { date: 29, month: 7, weekday: "Wed", label: "Yesterday", high: 21, low: 18, precip: 55, icon: "cloudy",     nightIcon: "cloudy" },
    { date: 30, month: 7, weekday: "Thu", label: "Today",     high: 26, low: 16, precip: 30, icon: "partly-day", nightIcon: "partly-night" },
    { date: 31, month: 7, weekday: "Fri", label: "Fri",       high: 28, low: 19, precip:  9, icon: "clear-day",  nightIcon: "partly-night" },
    { date:  1, month: 8, weekday: "Sat", label: "Sat",       high: 26, low: 19, precip: 62, icon: "rain",       nightIcon: "rain-night" },
    { date:  2, month: 8, weekday: "Sun", label: "Sun",       high: 21, low: 15, precip: 71, icon: "rain",       nightIcon: "rain-night" },
    { date:  3, month: 8, weekday: "Mon", label: "Mon",       high: 25, low: 14, precip:  6, icon: "clear-day",  nightIcon: "clear-night" },
    { date:  4, month: 8, weekday: "Tue", label: "Tue",       high: 28, low: 16, precip:  4, icon: "clear-day",  nightIcon: "clear-night" },
    { date:  5, month: 8, weekday: "Wed", label: "Wed",       high: 27, low: 17, precip: 18, icon: "partly-day", nightIcon: "partly-night" },
    { date:  6, month: 8, weekday: "Thu", label: "Thu",       high: 24, low: 16, precip: 48, icon: "rain",       nightIcon: "rain-night" },
    { date:  7, month: 8, weekday: "Fri", label: "Fri",       high: 23, low: 15, precip: 35, icon: "partly-day", nightIcon: "partly-night" },
    { date:  8, month: 8, weekday: "Sat", label: "Sat",       high: 26, low: 16, precip: 12, icon: "clear-day",  nightIcon: "clear-night" }
];

var todayIndex = 1;

// ---------------------------------------------------------------------------
// The month, for the calendar screen.
// ---------------------------------------------------------------------------
// July 2026: 31 days, the 1st a Wednesday, today the 30th. The weekday of the
// 1st is the only calendar fact here — everything else follows from it, so
// moving the month is one number rather than thirty-one.
//
// Days that also appear in `days` take their values from there rather than
// generating their own. A ten-day strip and a calendar that disagree about
// Friday's high is the same defect as a hero that disagrees with the card
// three rows down, and it is much harder to see: the two are never on screen
// together.
var month = {
    name: "July",
    year: 2026,
    number: 7,
    length: 31,
    firstWeekday: 3,        // 0 = Sunday, so the 1st is a Wednesday
    today: 30
};

var weekdayNames = ["Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat"];

function weekdayOf(date) {
    return (month.firstWeekday + date - 1) % 7;
}

// The entry in `days` for a date in this month, or null.
function dayFor(date, monthNumber) {
    for (var i = 0; i < days.length; ++i)
        if (days[i].date === date && days[i].month === monthNumber)
            return days[i];
    return null;
}

// One cell per day of the month.
//
// Deterministic and not random: the same seasonal shape every run, so a golden
// image of this screen means something. `Math.sin` of the date is a cheap
// spread that no reader will read as a pattern at four columns wide.
var monthDays = _buildMonth();

function _buildMonth() {
    var out = [];
    for (var d = 1; d <= month.length; ++d) {
        var known = dayFor(d, month.number);
        if (known !== null) {
            out.push({ date: d, weekday: weekdayNames[weekdayOf(d)],
                       high: known.high, low: known.low, icon: known.icon,
                       isToday: d === month.today });
            continue;
        }

        // A month that warms into its middle and cools out of it, with a
        // two-day wobble on top so consecutive days are not a ramp.
        var seasonal = 25 + 4 * Math.sin((d - 8) / month.length * Math.PI);
        var wobble = 2.6 * Math.sin(d * 1.7) + 1.4 * Math.sin(d * 0.6);
        var high = Math.round(seasonal + wobble);
        var spread = 7 + Math.round(2 * Math.sin(d * 1.1));

        // Wet days are the cool ones, which is the relation a reader will
        // check against the numbers beside it.
        var icon = wobble < -1.6 ? "rain"
                 : wobble < 0.2 ? "cloudy"
                 : wobble < 2.0 ? "partly-day"
                                : "clear-day";

        out.push({ date: d, weekday: weekdayNames[weekdayOf(d)],
                   high: high, low: high - spread, icon: icon,
                   isToday: d === month.today });
    }
    return out;
}

// Fractional indices, so a marker can sit between two samples.
//
// The times are detaildata.js's — 6:04 and 8:43, its `sun.riseMin` and
// `sun.setMin` to the minute — because the same sun cannot rise at two
// different times on one page. These had been 5:44 and 8:33, which is some
// other date's sun and put `isNight()` twenty minutes out from the arc on the
// Sun card at one end and ten at the other. Tomorrow's pair moves the way a
// real one does in late July: a minute later up, a minute earlier down.
//
// Index = the clock hour minus `startHour`, so 6:04 AM on day 1 is
// 6 + 4/60 − 21 + 24 = 9.07.
var sunEvents = [
    { index:  9.07, kind: "sunrise", text: "6:04 AM" },
    { index: 23.72, kind: "sunset",  text: "8:43 PM" },
    { index: 33.08, kind: "sunrise", text: "6:05 AM" },
    { index: 47.70, kind: "sunset",  text: "8:42 PM" }
];

var moonPhase = { name: "Waning Gibbous", illuminated: 0.74 };

function isNight(i) {
    return i < sunEvents[0].index
        || (i >= sunEvents[1].index && i < sunEvents[2].index)
        || i >= sunEvents[3].index;
}

function conditionFor(i) {
    var night = isNight(i);
    if (precipMm[i] >= 0.1 || precipProb[i] >= 45)
        return night ? "rain-night" : "rain";
    if (cloud[i] > 72)
        return "cloudy";
    if (cloud[i] > 25)
        return night ? "partly-night" : "partly-day";
    return night ? "clear-night" : "clear-day";
}

function conditionText(i) {
    switch (conditionFor(i)) {
    case "rain":         return "Rain showers";
    case "rain-night":   return "Rain showers";
    case "cloudy":       return "Cloudy";
    case "partly-day":   return "Partly sunny";
    case "partly-night": return "Partly cloudy";
    case "clear-day":    return "Sunny";
    case "clear-night":  return "Clear";
    }
    return "—";
}

function hourLabel(i) {
    if (i === nowIndex)
        return "Now";
    var h = (startHour + i) % 24;
    var suffix = h < 12 ? "AM" : "PM";
    var h12 = h % 12;
    if (h12 === 0)
        h12 = 12;
    return h12 + " " + suffix;
}

function clockLabel(i) {
    var h = (startHour + i) % 24;
    var suffix = h < 12 ? "AM" : "PM";
    var h12 = h % 12;
    if (h12 === 0)
        h12 = 12;
    return h12 + " " + suffix;
}

function labelIndices() {
    var out = [];
    for (var i = firstLabelIndex; i < count; i += labelStep)
        out.push(i);
    return out;
}

function valueTicks(min, max, step) {
    var out = [];
    for (var t = min; t <= max + 0.001; t += step)
        out.push(t);
    return out;
}

// Precipitation-probability buckets, one per label interval, value = bucket max.
function precipBuckets() {
    var out = [];
    for (var i = firstLabelIndex; i < count - 1; i += labelStep) {
        var p = precipProb[i];
        for (var k = 1; k < labelStep && i + k < count; ++k)
            p = Math.max(p, precipProb[i + k]);
        out.push({ index: i, span: labelStep, prob: p });
    }
    return out;
}
