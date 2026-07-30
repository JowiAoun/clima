// SPDX-License-Identifier: GPL-3.0-or-later
// Metric registry: the tab bar and the chart are both driven from this list.
//
// Adding a metric is a data change, not a code change — which is the point. In
// libclima this becomes a C++ registry populated from provider capabilities, so a
// tab only appears when the active provider actually has that variable for that
// location (Open-Meteo has no 15-minute data outside Central Europe and North
// America, for instance).
.pragma library

// kind: "area"  → filled curve, gradient down the value axis
//       "bars"  → one bar per hour, coloured by value
var list = [
    { id: "overview",      label: "Overview",      kind: "area", series: "temperature", unit: "°",
      min: 0,   max: 40,   step: 10, ramp: "temp",       decimals: 0, legend: "Temperature" },

    // autoScale: a fixed 0–4 mm axis renders a drizzle as a flat line, which reads
    // as "no data" rather than "a little rain". Rain is the one variable whose
    // range genuinely spans orders of magnitude, so its axis follows the data.
    { id: "precipitation", label: "Precipitation", kind: "bars", series: "precipMm",    unit: " mm",
      min: 0,   max: 4,    step: 1,  ramp: "precip",     decimals: 1, legend: "Precipitation amount",
      autoScale: true },

    { id: "wind",          label: "Wind",          kind: "area", series: "windSpeed",   unit: " km/h",
      min: 0,   max: 40,   step: 10, ramp: "wind",       decimals: 0, legend: "Wind speed",
      overlay: "windGust", overlayLegend: "Gusts" },

    { id: "airquality",    label: "Air Quality",   kind: "bars", series: "airQuality",  unit: "",
      min: 0,   max: 100,  step: 25, ramp: "aqi",        decimals: 0, legend: "European AQI" },

    { id: "humidity",      label: "Humidity",      kind: "area", series: "humidity",    unit: "%",
      min: 0,   max: 100,  step: 25, ramp: "humidity",   decimals: 0, legend: "Relative humidity" },

    { id: "cloud",         label: "Cloud cover",   kind: "area", series: "cloud",       unit: "%",
      min: 0,   max: 100,  step: 25, ramp: "cloud",      decimals: 0, legend: "Total cloud cover" },

    { id: "pressure",      label: "Pressure",      kind: "area", series: "pressure",    unit: " hPa",
      min: 995, max: 1030, step: 10, ramp: "pressure",   decimals: 0, legend: "Pressure (MSL)" },

    { id: "uv",            label: "UV",            kind: "bars", series: "uvIndex",     unit: "",
      min: 0,   max: 12,   step: 3,  ramp: "uv",         decimals: 0, legend: "UV index" },

    { id: "visibility",    label: "Visibility",    kind: "area", series: "visibility",  unit: " km",
      min: 0,   max: 25,   step: 5,  ramp: "visibility", decimals: 0, legend: "Visibility" },

    { id: "feels",         label: "Feels like",    kind: "area", series: "apparent",    unit: "°",
      min: 0,   max: 40,   step: 10, ramp: "temp",       decimals: 0, legend: "Feels like" }
];

function byId(id) {
    for (var i = 0; i < list.length; ++i)
        if (list[i].id === id)
            return list[i];
    return list[0];
}

function ticks(metric) {
    var out = [];
    for (var v = metric.min; v <= metric.max + 0.001; v += metric.step)
        out.push(Math.round(v * 100) / 100);
    return out;
}

// Smallest "nice" upper bound that contains the data, so an auto-scaled axis still
// lands on round numbers a human would have chosen.
function niceMax(values, floor) {
    var m = 0;
    for (var i = 0; i < values.length; ++i)
        if (values[i] > m)
            m = values[i];
    if (m <= 0)
        return floor > 0 ? floor : 1;

    var steps = [0.5, 1, 2, 2.5, 5, 10, 20, 25, 50, 100, 200, 250, 500, 1000];
    for (var k = 0; k < steps.length; ++k)
        if (m <= steps[k])
            return steps[k];
    return Math.ceil(m);
}

function ticksFor(min, max, divisions) {
    var n = divisions || 4;
    var out = [];
    for (var i = 0; i <= n; ++i)
        out.push(Math.round((min + (max - min) * i / n) * 100) / 100);
    return out;
}

// Axis bounds for a metric, honouring autoScale.
function axisMax(metric, values) {
    return metric.autoScale ? niceMax(values, metric.step) : metric.max;
}

function axisTicks(metric, values) {
    return metric.autoScale ? ticksFor(metric.min, axisMax(metric, values)) : ticks(metric);
}

// Header/readout formatting. Kept here so the chart never decides units.
function format(metric, value) {
    if (value === undefined || value === null || isNaN(value))
        return "–";
    return value.toFixed(metric.decimals) + metric.unit;
}
