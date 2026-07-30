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

var startHour       = 21;   // index 0 is 21:00 on day 0
var nowIndex        = 3;    // 00:00
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
// than to exercise a switch statement: last night's rain still drizzling out
// as the page opens, a thunder-free but genuinely heavy band through the
// afternoon that climbs light → moderate → heavy → moderate and back, its own
// tail, and a light band after tomorrow's sunrise. Sleet, snow and hail cannot
// occur at these temperatures and so are not here; the gallery is where those
// live, which is what the gallery is for.
var precipMm = _buildPrecipMm();

function _buildPrecipMm() {
    var out = [];
    for (var i = 0; i < count; ++i)
        out.push(0);

    // 21:00 – 01:00, easing off. Straddles `nowIndex`, so the effect has to be
    // right on both sides of the now line on first paint.
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
var days = [
    { date: 29, label: "Yesterday", high: 21, low: 18, icon: "cloudy",     nightIcon: "cloudy" },
    { date: 30, label: "Today",     high: 26, low: 16, icon: "partly-day", nightIcon: "partly-night" },
    { date: 31, label: "Fri",       high: 28, low: 19, icon: "clear-day",  nightIcon: "partly-night" },
    { date:  1, label: "Sat",       high: 26, low: 19, icon: "rain",       nightIcon: "rain-night" },
    { date:  2, label: "Sun",       high: 21, low: 15, icon: "rain",       nightIcon: "rain-night" },
    { date:  3, label: "Mon",       high: 25, low: 14, icon: "clear-day",  nightIcon: "clear-night" },
    { date:  4, label: "Tue",       high: 28, low: 16, icon: "clear-day",  nightIcon: "clear-night" },
    { date:  5, label: "Wed",       high: 27, low: 17, icon: "partly-day", nightIcon: "partly-night" },
    { date:  6, label: "Thu",       high: 24, low: 16, icon: "rain",       nightIcon: "rain-night" }
];

var todayIndex = 1;

// Fractional indices, so a marker can sit between two samples.
var sunEvents = [
    { index:  8.73, kind: "sunrise", text: "5:44 AM" },
    { index: 23.55, kind: "sunset",  text: "8:33 PM" },
    { index: 32.75, kind: "sunrise", text: "5:45 AM" },
    { index: 47.53, kind: "sunset",  text: "8:32 PM" }
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
