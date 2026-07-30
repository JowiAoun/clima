// SPDX-License-Identifier: GPL-3.0-or-later
// Stand-in for the Open-Meteo provider.
//
// 48 hourly samples starting at 21:00. The values at the labelled indices are
// hand-tuned to match the MSN screenshot this prototype is modelled on, so the
// two can be compared side by side. Shape of the API deliberately mirrors what
// libclima's ForecastProvider will return: parallel per-hour arrays plus derived
// helpers, no formatting decisions baked in.
.pragma library

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

var precipProb = [
    24, 22, 18, 18, 16, 18, 14, 20, 12, 10,  8,  6,
     4,  3,  4,  9,  7, 22, 18, 30, 24, 13, 10,  8,
     6,  5,  4,  3,  5,  8, 12, 10, 14, 20, 26, 22,
    18, 12,  9,  6,  4,  3,  2,  4,  7, 11, 16, 14
];

var cloud = [
    88, 90, 84, 78, 68, 46, 42, 38, 32, 30, 26, 34,
    32, 36, 40, 38, 44, 46, 52, 56, 50, 44, 26, 20,
    30, 36, 40, 44, 48, 52, 46, 40, 34, 28, 44, 58,
    52, 40, 32, 26, 22, 28, 36, 44, 52, 60, 66, 70
];

var count = temperature.length;

// Observed rain in the first few hours — this is why the past region is hatched
// and the leading icons are showers rather than cloud.
var precipMm = _buildPrecipMm();

function _buildPrecipMm() {
    var out = [];
    for (var i = 0; i < count; ++i)
        out.push(0);
    out[0] = 0.4; out[1] = 0.6; out[2] = 0.3; out[3] = 0.2; out[4] = 0.1;
    out[34] = 0.2; out[35] = 0.3;
    return out;
}

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
