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

    textPrimary:   "#ffffff",
    textMuted:     "#98a4be",
    textDim:       "#7a86a2",

    gridLine:      "#1cffffff",
    gridLineWeak:  "#10ffffff",

    pastVeil:      "#14ffffff",
    pastHatch:     "#1effffff",
    nowLine:       "#59ffffff",

    stripBg:       "#2d3d5e",
    stripPast:     "#26324e",
    stripDivider:  "#1affffff",
    droplet:       "#93c6f2",

    accent:        "#ffd24a",
    toggleTrack:   "#3b4767",
    toggleKnob:    "#c6cede",

    pagerBg:       "#f0323f5e",
    pagerBgHover:  "#f0455677",
    pagerGlyph:    "#dde4f2",

    sunGlyphWarm:  "#ffd97a",
    sunGlyphCool:  "#f2952f",
    moonGlyph:     "#f2e3b8",
    cloudTop:      "#ffffff",
    cloudBottom:   "#c1cddf",
    rainDrop:      "#7fb6e8"
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

// Temperature colour ramp. Colour encodes absolute temperature, so the fill is a
// single vertical gradient over the value axis, clipped to the area under the
// curve — which is why a cool morning reads green at the same height where a warm
// afternoon reads tan.
// Alpha falls off toward the bottom of the scale so the value gridlines stay
// legible through the fill, the way they do in the reference.
var fillRamp = [
    { t: 40, c: "#cce0764f" },
    { t: 32, c: "#ccd59a5e" },
    { t: 26, c: "#ccbba083" },
    { t: 22, c: "#c2a2ab82" },
    { t: 18, c: "#b886ab80" },
    { t: 12, c: "#a05f9670" },
    { t:  6, c: "#88508a66" },
    { t:  0, c: "#6e497f5f" }
];

var lineRamp = [
    { t: 40, c: "#f0f5a283" },
    { t: 32, c: "#f0efc08f" },
    { t: 26, c: "#f0decbae" },
    { t: 22, c: "#f0c8d0a9" },
    { t: 18, c: "#f0b1d3a9" },
    { t: 12, c: "#f094cb9d" },
    { t:  6, c: "#f082c391" },
    { t:  0, c: "#f076b988" }
];
