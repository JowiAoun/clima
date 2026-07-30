// SPDX-License-Identifier: GPL-3.0-or-later
// Current-conditions data for the weather-detail cards.
//
// Stand-in for the provider, shaped the way `libclima` will return it: plain
// values plus the short series a card needs to draw a trend. No formatting
// decisions — units, rounding and wording belong to the card.
//
// The numbers deliberately match the captured reference for the same location
// and hour, so a card can be held against `reference/msn/…` and compared
// directly rather than approximately.
//
// Deterministic: no Math.random anywhere, so golden-image tests stay stable.
.pragma library

var observedAt = "12:28 PM";

// Where the forecast is for. Here rather than in mockdata.js because it
// describes the observation rather than the hourly series.
var location = {
    name: "Toronto",
    region: "Ontario",
    label: "Toronto, Ontario",
    isHome: true
};

// The page headline.
//
// Every number it shows already lives in one of the per-measurable blocks below
// and is read from there — the temperature from `temperature`, the condition
// from `cloudCover`, the apparent temperature from `feelsLike`. Copying 27° into
// a second place is how a page ends up disagreeing with itself, and a hero that
// contradicts the card three rows down is worse than no hero.
//
// What is genuinely only the headline's: which glyph to draw, the unit it is
// labelled with, and the one-sentence outlook.
var current = {
    conditionKind: "clear-day",
    unitLabel: "°C",
    summary: "Expect sunny skies. The high will be 29°."
};

// Twelve hours of context, oldest first, for the cards that draw a sparkline.
// Index 6 is "now".
var nowIndex = 6;

var temperature = {
    value: 27, unit: "°",
    series: [21, 22, 23, 24, 25, 26, 27, 28, 29, 28, 26, 24],
    high: 29, low: 17, peakAt: "3:00 p.m.", lowAt: "5:00 a.m.",
    trend: "up", status: "Rising",
    body: "Rising with a peak of 29° at 3:00 p.m. Overnight low of 17° at 5:00 a.m."
};

var feelsLike = {
    value: 30, actual: 27, unit: "°",
    series: [23, 24, 25, 26, 28, 29, 30, 31, 32, 31, 28, 26],
    dominantFactor: "humidity",
    trend: "up", status: "Slightly warm",
    body: "Feels warmer than the actual temperature due to the humidity."
};

var cloudCover = {
    value: 8, unit: "%",
    condition: "Sunny",
    trend: "steady", status: "Sunny",
    body: "Steady with clear sky at 12:28 p.m. Clear sky expected in the evening."
};

var precipitation = {
    value: 0, unit: "mm", window: "In next 24h",
    // 10 mm in twenty-four hours is a thoroughly wet day: the ceiling a card
    // draws the amount against. Here rather than in the card because it decides
    // what the reader sees, which makes it data and not styling.
    scaleMax: 10,
    // Probability per hour, for cards that want a small distribution.
    series: [0, 0, 0, 0, 0, 0, 0, 0, 5, 10, 20, 35],
    trend: "steady", status: "No precipitation",
    body: "Similar to yesterday so far. Rain expected Saturday night."
};

var wind = {
    speed: 13, gust: 24, unit: "km/h",
    // Beaufort 5 — a fresh breeze, when loose paper starts blowing about. Above
    // this a card can clamp; below it the scale would compress every ordinary
    // day into the first third.
    scaleMax: 30,
    directionDeg: 294, directionLabel: "WNW",
    beaufort: 3, beaufortName: "Gentle breeze",
    trend: "steady", status: "Gentle breeze",
    // The reference's sentence is about the *evening*, not now — it only looked
    // like it contradicted the 13 km/h reading because it elided mid-qualifier.
    body: "Evening averages near 8 km/h, gusting to 12, from the NNW."
};

var humidity = {
    value: 45, unit: "%", dewPoint: 14, dewUnit: "°",
    // Eight columns, matching the reference's bar array.
    series: [38, 41, 44, 46, 45, 43, 42, 40],
    trend: "down", status: "Normal",
    body: "Decreasing with a low of 42% at 1:00 p.m."
};

var uv = {
    value: 7, max: 11,
    // WHO bands: low 0-2, moderate 3-5, high 6-7, very high 8-10, extreme 11+
    band: "High", peakAt: "2:00 p.m.",
    trend: "up", status: "High",
    body: "Maximum UV exposure for today will be high, expected at 2:00 p.m."
};

var airQuality = {
    value: 25, max: 100,
    band: "Good", pollutant: "PM2.5", pollutantValue: 4.4, pollutantUnit: "µg/m³",
    // Up, because the *index* is rising — which for air quality is the bad
    // direction. The trend tracks the number; the body says whether that is
    // good news. See docs/10-design-system.md §10.5.
    trend: "up", status: "Good",
    body: "Deteriorating, with PM2.5 the primary pollutant."
};

var visibility = {
    value: 16, unit: "km",
    // As far as a public forecast bothers to distinguish: past 20 km the answer
    // is just "you can see". `peak` below is today's best, which is a reading,
    // not a ceiling — scaling 16 km against 45 made an "Excellent" card draw a
    // third of a bar.
    scaleMax: 20,
    band: "Excellent", peak: 45, peakAt: "1:00 p.m.",
    trend: "up", status: "Excellent",
    body: "Improving through the afternoon; clearest around 1:00 p.m."
};

var pressure = {
    value: 1014, unit: "mb", at: "12:28 PM (Now)",
    series: [1009, 1010, 1010, 1011, 1012, 1013, 1014, 1014, 1013, 1012, 1012, 1011],
    min: 1005, max: 1020,
    trend: "up", status: "Rising slowly",
    body: "Rising slowly in the last 3 hours. Expected to fall slowly in the next 3 hours."
};

// Times as minutes past midnight, so a card can place them on an arc without
// parsing anything.
var sun = {
    riseMin: 6 * 60 + 4, setMin: 20 * 60 + 43, nowMin: 12 * 60 + 28,
    riseLabel: "6:04", riseSuffix: "AM", setLabel: "8:43", setSuffix: "PM",
    dayLength: "14 hrs 39 mins",
    trend: "none", status: "Daylight",
    body: "The sun is up for 14 hours and 39 minutes today, setting at 8:43 p.m."
};

var moon = {
    riseMin: 21 * 60 + 25, setMin: 8 * 60 + 3, nowMin: 12 * 60 + 28,
    riseLabel: "9:25", riseSuffix: "PM", setLabel: "8:03", setSuffix: "AM",
    upLength: "10 hrs 38 mins",
    phase: "Waning Gibbous", illumination: 0.72,
    trend: "none", status: "Waning Gibbous",
    body: "The moon is 72% illuminated and rises at 9:25 p.m. tonight."
};

// Fixed order for the grid, so the layout does not depend on object key order.
var order = ["temperature", "feelsLike", "cloudCover", "precipitation",
             "wind", "humidity", "uv", "airQuality", "visibility",
             "pressure", "sun", "moon"];

// Colour bands published by the relevant authority, for the cards that show a
// scale rather than a single value. Positions are normalised over the card's
// own range.
var bands = {
    uv:  [{ p: 0.00, c: "#5ec18a" }, { p: 0.27, c: "#e8c73c" },
          { p: 0.50, c: "#f5a02f" }, { p: 0.72, c: "#d6484e" },
          { p: 1.00, c: "#8b5fc4" }],
    aqi: [{ p: 0.00, c: "#4ec3c8" }, { p: 0.25, c: "#5cc79a" },
          { p: 0.50, c: "#e8c93f" }, { p: 0.75, c: "#f08a45" },
          { p: 1.00, c: "#c03050" }],
    visibility: [{ p: 0.00, c: "#5f8f78" }, { p: 0.50, c: "#6fbf95" },
                 { p: 1.00, c: "#8fe0b4" }]
};
