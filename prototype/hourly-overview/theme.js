// SPDX-License-Identifier: GPL-3.0-or-later
// Design tokens for the hourly overview prototype.
// These become the real Clima design system later; keep them in one place so the
// whole chart can be re-skinned without touching layout code.
.pragma library

var color = {
    pageBg:        "#111a2b",

    cardBg:        "#1a2440",
    cardBorder:    "#2a3557",
    panelBg:       "#1e2a48",
    dayCardBg:     "#151d33",   // unselected day cards recede; the selected one
                                // takes cardBg so it merges with the chart below

    textPrimary:   "#ffffff",
    textMuted:     "#98a4be",
    textDim:       "#7a86a2",

    gridLine:      "#1cffffff",
    gridLineWeak:  "#10ffffff",

    pastVeil:      "#14ffffff",
    pastHatch:     "#1effffff",
    nowLine:       "#59ffffff",

    listRowAlt:    "#0affffff",
    nowRowBg:      "#1fffd24a",

    stripBg:       "#2d3d5e",
    stripPast:     "#26324e",
    stripDivider:  "#1affffff",
    droplet:       "#93c6f2",

    accent:        "#ffd24a",
    toggleTrack:   "#3b4767",
    toggleKnob:    "#c6cede",

    pillHover:          "#26ffffff",
    switchActive:       "#2c3a59",
    switchBorder:       "#445273",
    daySelectedBorder:  "#39466b",

    pagerBg:       "#f0323f5e",
    pagerBgHover:  "#f0455677",
    pagerGlyph:    "#dde4f2",

    sunGlyphWarm:  "#ffd97a",
    sunGlyphCool:  "#f2952f",
    moonGlyph:     "#f2e3b8",
    cloudTop:      "#ffffff",
    cloudBottom:   "#c1cddf",
    rainDrop:      "#7fb6e8",

    // Clouds are drawn white, which vanishes on the pale day badge — this variant
    // keeps them readable there without changing them everywhere else.
    cloudTopOnLight:    "#fbfdff",
    cloudBottomOnLight: "#9db0cc",

    badgeDayTop:     "#fdfefe",
    badgeDayBottom:  "#dde5f0",
    badgeNightTop:   "#6d9ae8",
    badgeNightBottom:"#3f63bd"
};

var metric = {
    hourWidth:        48,
    plotHeight:       252,
    axisTopPad:       12,
    headerBandHeight: 78,
    gutterWidth:      40,
    stripHeight:      26,
    stripGap:         10,
    panelPadding:     12,
    cardPadding:      20,
    cardRadius:       10,
    panelRadius:      8
};

var scale = {
    tempMin:  0,
    tempMax:  40,
    tickStep: 10
};

// Colour ramps, keyed by *normalised axis position*: p = 0 at the top of the value
// axis, p = 1 at the bottom. Keying on position rather than on a unit is what lets
// one gradient implementation serve °C, %, km/h and hPa alike.
//
// The idea itself is the good part of MSN's design: colour encodes the absolute
// value, so a 19° hour reads green and a 27° hour reads tan at the same place on
// the same chart. Alpha falls off downward so the gridlines stay legible through
// the fill.
//
// Every ramp has exactly eight stops, because QML cannot generate GradientStop
// elements from a Repeater and the gradient is therefore declared statically.
var ramp = {
    temp: {
        fill: [{ p: 0.00, c: "#cce0764f" }, { p: 0.20, c: "#ccd59a5e" },
               { p: 0.35, c: "#ccbba083" }, { p: 0.45, c: "#c2a2ab82" },
               { p: 0.55, c: "#b886ab80" }, { p: 0.70, c: "#a05f9670" },
               { p: 0.85, c: "#88508a66" }, { p: 1.00, c: "#6e497f5f" }],
        line: [{ p: 0.00, c: "#f0f5a283" }, { p: 0.20, c: "#f0efc08f" },
               { p: 0.35, c: "#f0decbae" }, { p: 0.45, c: "#f0c8d0a9" },
               { p: 0.55, c: "#f0b1d3a9" }, { p: 0.70, c: "#f094cb9d" },
               { p: 0.85, c: "#f082c391" }, { p: 1.00, c: "#f076b988" }]
    },
    wind: {
        fill: [{ p: 0.00, c: "#cce2684f" }, { p: 0.18, c: "#ccd98a55" },
               { p: 0.34, c: "#ccc9a664" }, { p: 0.48, c: "#c2a8ae76" },
               { p: 0.62, c: "#b88aa48e" }, { p: 0.74, c: "#a06098a0" },
               { p: 0.88, c: "#88528ea6" }, { p: 1.00, c: "#6e4a83a4" }],
        line: [{ p: 0.00, c: "#f0f59a84" }, { p: 0.18, c: "#f0efb78e" },
               { p: 0.34, c: "#f0e2cf9d" }, { p: 0.48, c: "#f0c8d3b2" },
               { p: 0.62, c: "#f0aad0c6" }, { p: 0.74, c: "#f092c8d4" },
               { p: 0.88, c: "#f083c0da" }, { p: 1.00, c: "#f078b6d8" }]
    },
    humidity: {
        fill: [{ p: 0.00, c: "#cc4f8ed6" }, { p: 0.18, c: "#cc5893cf" },
               { p: 0.34, c: "#cc6699c6" }, { p: 0.48, c: "#c2789fb9" },
               { p: 0.62, c: "#b88fa5aa" }, { p: 0.74, c: "#a0a1a79c" },
               { p: 0.88, c: "#88b0a58c" }, { p: 1.00, c: "#6ebc9f7c" }],
        line: [{ p: 0.00, c: "#f08fbdee" }, { p: 0.18, c: "#f097c0e9" },
               { p: 0.34, c: "#f0a2c4e2" }, { p: 0.48, c: "#f0b0c8d9" },
               { p: 0.62, c: "#f0c0cbce" }, { p: 0.74, c: "#f0cdccc3" },
               { p: 0.88, c: "#f0d8cbb6" }, { p: 1.00, c: "#f0e0c8aa" }]
    },
    cloud: {
        fill: [{ p: 0.00, c: "#ccb9c4d6" }, { p: 0.18, c: "#ccaabbd0" },
               { p: 0.34, c: "#cc99b1c9" }, { p: 0.48, c: "#c286a6c2" },
               { p: 0.62, c: "#b8749bbc" }, { p: 0.74, c: "#a06390b6" },
               { p: 0.88, c: "#885585b0" }, { p: 1.00, c: "#6e4a7bab" }],
        line: [{ p: 0.00, c: "#f0e2e9f4" }, { p: 0.18, c: "#f0d6e0f0" },
               { p: 0.34, c: "#f0c8d7ec" }, { p: 0.48, c: "#f0b9cee8" },
               { p: 0.62, c: "#f0a9c5e4" }, { p: 0.74, c: "#f09abce0" },
               { p: 0.88, c: "#f08cb3dc" }, { p: 1.00, c: "#f080abd8" }]
    },
    pressure: {
        fill: [{ p: 0.00, c: "#cc9b86d8" }, { p: 0.18, c: "#cc9089d4" },
               { p: 0.34, c: "#cc848ccf" }, { p: 0.48, c: "#c2788fc9" },
               { p: 0.62, c: "#b86d92c4" }, { p: 0.74, c: "#a06395bf" },
               { p: 0.88, c: "#885a97ba" }, { p: 1.00, c: "#6e5299b6" }],
        line: [{ p: 0.00, c: "#f0cbbdef" }, { p: 0.18, c: "#f0c3c0ec" },
               { p: 0.34, c: "#f0bbc3e9" }, { p: 0.48, c: "#f0b3c6e6" },
               { p: 0.62, c: "#f0abc9e3" }, { p: 0.74, c: "#f0a4cce0" },
               { p: 0.88, c: "#f09dcedd" }, { p: 1.00, c: "#f096d0da" }]
    },
    visibility: {
        fill: [{ p: 0.00, c: "#cc74b8d8" }, { p: 0.18, c: "#cc78b2ce" },
               { p: 0.34, c: "#cc7fabc2" }, { p: 0.48, c: "#c288a4b5" },
               { p: 0.62, c: "#b8939ca8" }, { p: 0.74, c: "#a09e949a" },
               { p: 0.88, c: "#88a88c8d" }, { p: 1.00, c: "#6eb08581" }],
        line: [{ p: 0.00, c: "#f0aadcf0" }, { p: 0.18, c: "#f0aed7e9" },
               { p: 0.34, c: "#f0b4d1e0" }, { p: 0.48, c: "#f0bacbd7" },
               { p: 0.62, c: "#f0c1c5ce" }, { p: 0.74, c: "#f0c8bfc4" },
               { p: 0.88, c: "#f0ceb9bb" }, { p: 1.00, c: "#f0d4b4b2" }]
    },
    // Bar ramps. Sampled per value rather than used as a gradient, so the stops
    // are the category boundaries of the published scale where one exists.
    precip: {
        fill: [{ p: 0.00, c: "#ff3f6fd8" }, { p: 0.18, c: "#ff4879d9" },
               { p: 0.34, c: "#ff5285db" }, { p: 0.48, c: "#ff5d91dd" },
               { p: 0.62, c: "#ff699edf" }, { p: 0.74, c: "#ff77abe2" },
               { p: 0.88, c: "#ff86b8e5" }, { p: 1.00, c: "#ff96c5e8" }],
        line: [{ p: 0.00, c: "#ff6f9be8" }, { p: 1.00, c: "#ffb4d8f2" }]
    },
    // European AQI bands: good, fair, moderate, poor, very poor.
    aqi: {
        fill: [{ p: 0.00, c: "#ff8c1e4b" }, { p: 0.20, c: "#ffc03050" },
               { p: 0.40, c: "#ffe85a50" }, { p: 0.55, c: "#fff08a45" },
               { p: 0.70, c: "#ffe8c93f" }, { p: 0.82, c: "#ffa8d15a" },
               { p: 0.92, c: "#ff5cc79a" }, { p: 1.00, c: "#ff4ec3c8" }],
        line: [{ p: 0.00, c: "#ffb04a70" }, { p: 1.00, c: "#ff8fd8dc" }]
    },
    // WHO UV bands: low, moderate, high, very high, extreme.
    uv: {
        fill: [{ p: 0.00, c: "#ff8b5fc4" }, { p: 0.17, c: "#ffa155bd" },
               { p: 0.33, c: "#ffd6484e" }, { p: 0.50, c: "#fff07038" },
               { p: 0.63, c: "#fff5a02f" }, { p: 0.75, c: "#ffe8c73c" },
               { p: 0.88, c: "#ffa9cf55" }, { p: 1.00, c: "#ff5ec18a" }],
        line: [{ p: 0.00, c: "#ffb98ede" }, { p: 1.00, c: "#ff8fd4ac" }]
    }
};
