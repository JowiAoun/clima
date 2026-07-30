// SPDX-License-Identifier: GPL-3.0-or-later
// Design tokens for the hourly overview prototype.
// These become the real Clima design system later; keep them in one place so the
// whole chart can be re-skinned without touching layout code.
.pragma library

// Surfaces are translucent, not painted.
//
// The reference has no opaque cards at all: the page is one vertical gradient
// and every surface is a thin white wash over it, so a card's actual colour is
// whatever the gradient is doing behind it at that scroll depth. Flat fills
// cannot reproduce that — they are one colour everywhere, and the page stops
// reading as a single lit surface with things resting on it.
//
// Alpha ladder, matching the reference's three levels: 0.05 recedes, 0.07 is
// the default surface, 0.10 is raised or hovered. In #AARRGGBB those are 0d,
// 12 and 1a.
//
// The reference also puts backdrop-blur(60px) under each card. That is
// deliberately not reproduced: blurring a smooth vertical gradient returns
// almost exactly the same gradient, so it would cost an offscreen pass and a
// blur per card to change nothing. It earns its keep on their radar map, and
// that is where to revisit it.
var surfaceAlpha = {
    recede: "#0d",   // 0.05
    base:   "#12",   // 0.07
    raised: "#1a"    // 0.10
};

var color = {
    // Page gradient stops. Declared here, applied in Main.qml — QML cannot
    // generate GradientStop elements from a Repeater, so they are written out.
    pageStop0:     "#203580",   // 0.00
    pageStop1:     "#443e73",   // 0.06
    pageStop2:     "#443a66",   // 0.30
    pageStop3:     "#27284f",   // 0.60
    pageStop4:     "#171e44",   // 1.00
    pageBg:        "#27284f",   // flat fallback, ~the gradient's midpoint

    surfaceRecede: "#0dffffff",
    surfaceBase:   "#12ffffff",
    surfaceRaised: "#1affffff",

    cardBg:        "#12ffffff",  // the card surface
    cardBorder:    "#1affffff",
    panelBg:       "transparent", // the chart sits directly on the card: a
                                  // second wash inside the first would read as
                                  // 0.135, a panel darker than anything in the
                                  // reference
    dayCardBg:     "#0dffffff",   // unselected day cards recede; the selected
                                  // one takes cardBg and merges with the panel

    // Ink for text sitting *on* the accent, which is a light yellow. This was
    // previously cardBg — fine while cardBg was an opaque dark navy, invisible
    // the moment it became a white wash.
    onAccent:      "#141d33",

    textPrimary:   "#ffffff",
    textMuted:     "#98a4be",
    textDim:       "#7a86a2",

    gridLine:      "#1cffffff",
    gridLineWeak:  "#10ffffff",

    pastVeil:      "#14ffffff",
    pastHatch:     "#1effffff",
    nowLine:       "#59ffffff",

    // A forecast stretch is the same line as the observed one, drawn with less
    // certainty. Same value as nowLine and deliberately a separate name: they
    // mean different things and will not always want the same alpha.
    forecastDim:   "#59ffffff",

    // The unfilled part of a gauge — a dial track, a bar's empty remainder. It
    // has to be present enough that the filled part reads as a fraction of
    // something, which gridLine at 0.11 is not.
    trackLine:     "#2effffff",

    listRowAlt:    "#0affffff",
    nowRowBg:      "#1fffd24a",

    stripBg:       "#1affffff",
    stripPast:     "#0dffffff",
    stripDivider:  "#1affffff",
    droplet:       "#93c6f2",

    accent:        "#ffd02c",   // measured off the reference's selected pill
    toggleTrack:   "#26ffffff",
    toggleKnob:    "#c6cede",

    pillHover:          "#1affffff",
    switchActive:       "#1affffff",
    switchBorder:       "#33ffffff",
    daySelectedBorder:  "#33ffffff",

    // Pager buttons float over the chart, so they stay more opaque than a
    // surface — but still tinted rather than painted, or they punch a flat
    // hole in the gradient.
    pagerBg:       "#99141d33",
    pagerBgHover:  "#b3141d33",
    pagerGlyph:    "#e8edf7",

    trendUp:       "#ff9d5c",
    trendDown:     "#7fb6e8",
    trendSteady:   "#c6cede",

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

// Radii are deliberately generous. The reference reads "soft" because almost
// nothing in it meets at a hard edge, and the tab/panel junction is filleted
// rather than squared — see TabFillet.qml.
var metric = {
    hourWidth:        48,
    plotHeight:       252,
    axisTopPad:       12,
    headerBandHeight: 78,
    gutterWidth:      40,
    stripHeight:      28,
    stripGap:         12,
    panelPadding:     14,
    cardPadding:      22,
    cardRadius:       14,
    panelRadius:      12,
    controlRadius:    8,
    filletRadius:     18,

    // Weather-detail grid, measured off the reference: a 300x250 card with a
    // 12px radius and 16/20 padding, giving every visualisation the same
    // 260-wide box to work in.
    detailCardWidth:  300,
    detailCardHeight: 250,
    detailRadius:     12,
    detailPadH:       20,
    detailPadV:       16,
    detailGap:        16
};

// Type sizes, as tokens rather than as a table in a document, because twelve
// independently-written cards produced seven different sizes for the same role
// when the only thing binding them was prose.
//
// `font.pixelSize` is an int in Qt. A fractional value fails object creation and
// Qt reports it only as `Type X unavailable` from the *parent* file.
var type = {
    cardTitle:   15,
    detailTitle: 14,

    // The reading: the one number a card exists to show. A card carrying two
    // co-equal readings — sunrise and sunset, speed and gust — uses the pair
    // size for both. There is no third option: a card wanting one is really
    // asking for a different layout.
    reading:     34,
    readingPair: 26,

    status:      14,
    body:        12,
    label:       12,
    axis:        11
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
