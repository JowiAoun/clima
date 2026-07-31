// SPDX-FileCopyrightText: 2026 Jowi Aoun
// SPDX-License-Identifier: GPL-3.0-or-later
//
// The light theme, as overrides on theme.js rather than a second copy of it.
//
// ---- why this is a separate file --------------------------------------------
//
// theme.js stays the dark table, untouched. Every value here is the light
// answer to a value there, under the same group and the same key, and Theme.qml
// picks between the two on one property. Two consequences, both of them the
// reason it is arranged this way:
//
//   1. Dark cannot regress. Adding a theme does not edit the theme that exists,
//      so every capture taken before this file existed is still byte-identical
//      afterwards. That is the check that proves a theme was *added*.
//   2. gallery.js keeps working. It reaches the ramps with
//      `.import "../theme.js"` because a `.pragma library` cannot import a QML
//      singleton, and a restructure of theme.js would have broken that for no
//      gain.
//
// The cost is that the two files have to agree on their key sets. Theme.qml
// reads both through the same typed properties, so a key missing here is a
// binding that silently falls back to undefined — and Theme.qml asserts the
// shape at startup rather than letting that happen quietly.
//
// ---- the constraint this palette is answering -------------------------------
//
// docs/10-design-system.md §10.1: a card is a wash over the page, and §10.12
// says every sky phase is dark because "a wash is only a surface if something
// darker is behind it". That is not a preference for dark, it is a statement
// about the operand. Light mode inverts it: the ladder is the same three steps,
// but the wash follows the ink, so black over light instead of white over dark.
//
// One rule genuinely changes, and it is written up in §10.1: light mode gets a
// hairline on the outer card edge. On a dark page the range above the
// background does the work. On a light page there is almost none — 10% black
// over #eef1f7 is about a 1.13:1 step, which is enough to see that a card is
// there and not enough to see where two abutting cards meet.

.pragma library

// ---- the page ---------------------------------------------------------------
//
// Slightly blue rather than neutral. A pure grey page under cards that are grey
// washes is a screen with no temperature in it at all, and the weather palette
// — every ramp, every glyph — is built around blues.
var page = {
    bg: "#eef1f7"
};

// Deeper than dark's 0.05 / 0.07 / 0.10. Black over a light ground reads weaker
// than white over a dark one at the same alpha, because the eye is comparing
// against a much brighter surround; matching the numbers would have produced a
// ladder with no rungs.
var surfaceAlpha = {
    recede: "#0a",
    base:   "#0f",
    raised: "#17"
};

var surface = {
    recede: "#0a000000",
    base:   "#0f000000",
    raised: "#17000000",

    // Invariant, and the one surface token that is not a colour: it is the
    // assertion that there is no second wash inside the first.
    panel:  "transparent",

    rowAlt: "#08000000",
    rowNow: "#26e8a900",

    // The two opaque ones. Content scrolls under them, so they cannot be a
    // wash — §10.1 grants the nav and the menu that exception and nothing else.
    nav:    "#f2f7f9fc",
    menu:   "#f7ffffff"
};

// ---- ink --------------------------------------------------------------------
//
// `dim` is the one that wanted care. The obvious mirror of dark's #7a86a2 is a
// mid grey around #78849c, which lands at about 3.8:1 on this page — under the
// 4.5:1 the gallery's contrast column flags, and it is used for axis labels,
// which are read rather than glanced at.
var ink = {
    primary: "#141d2e",
    muted:   "#5a6580",
    dim:     "#6b7794"
};

var line = {
    grid:     "#1a000000",
    gridWeak: "#0f000000",
    track:    "#26000000",
    now:      "#59000000",
    forecast: "#4d000000",
    series:   "#8c000000",

    // Load-bearing in this theme and decorative in the other. See the header.
    card:     "#14000000",

    divider:  "#16000000",
    nav:      "#14000000",
    menu:     "#1f000000",
    control:  "#29000000"
};

// ---- accent -----------------------------------------------------------------
//
// Deepened from #ffd02c, which is a colour designed to sit on navy and which on
// a near-white page reads as a highlighter rather than a selection. `ink` does
// not move: it is the text drawn *on* the accent, and dark ink on yellow is the
// right answer under both themes.
//
// Note this is deliberately not the desktop's accent colour. Accent here is
// structure — the selected metric pill, the nav pill, the wash behind the now
// row — and ink.onAccent is tuned to it. Repainting it purple would break three
// legibility relationships at once.
var accent = {
    fill: "#e8a900",
    ink:  "#141d33"
};

var control = {
    toggleTrack:    "#1f000000",
    toggleKnob:     "#ffffff",
    pagerFill:      "#b3f4f6fa",
    pagerFillHover: "#ccf4f6fa",
    pagerGlyph:     "#1b2436",
    navGlyph:       "#5a6580"
};

// The plates invert with the page: in dark these are dark scrims carrying light
// text, and here they are light scrims carrying dark text. The text colour is
// ink.primary in both, so it follows on its own.
var overlay = {
    past:      "#12000000",
    pastHatch: "#1a000000",
    hatch:     "#14000000",
    caption:   "#ccf7f9fc",
    readout:   "#e6f7f9fc",
    scrim:     "#99f0f3f8"
};

// ---- state ------------------------------------------------------------------
//
// Hue-invariant, lightness-varying: warm for rising, cool for falling, and the
// three verdict levels keep the green/amber/red that every other weather
// product uses. `caution` is the one that could not simply be nudged — #e8c93f
// is a mid yellow that lands near 1.9:1 on this page, so it goes to a dark
// amber rather than a slightly deeper yellow. §4.10 forbids colour-only
// encoding anyway, so each of these is always paired with a glyph or a word.
var state = {
    trendUp:    "#c25a1a",
    trendDown:  "#2d6fa8",
    trendSteady:"#5a6580",
    good:       "#1f8a52",
    caution:    "#9a7a00",
    poor:       "#c8341c"
};

// ---- glyphs -----------------------------------------------------------------
//
// The cloud is the interesting one, and it was half-solved already. In dark the
// cloud is white on a night sky. On a near-white page a white cloud is not a
// cloud, it is a hole. theme.js already carries cloudTopOnLight/OnLight for the
// day badge — a pale plate inside a dark theme, which is the same problem in
// miniature — so the light theme's ordinary cloud is that pair, and the pale
// plate inside the light theme needs one darker still.
//
// moonShade inverts outright: in dark it is the navy of the unlit limb against
// a lit face, and here it is the pale grey of an unlit limb against a gold one.
var glyph = {
    sunWarm:            "#f5b942",
    sunCool:            "#e07d1a",
    moon:               "#c9a961",
    moonShade:          "#cfd6e6",

    // Darker than the OnLight pair below, and the first draft got this wrong by
    // reusing it. That pair was tuned for the day badge — a 40 px plate at
    // #fdfefe→#dde5f0 with a cloud drawn on it — where near-white works because
    // the plate is smaller and brighter than anything around it. A card in this
    // theme is not: it is a wash a few percent off the page, so a near-white
    // cloud on it has almost nothing to be seen against. Rendered, the ten-day
    // strip's cloudy days read as blank cards next to the sunny ones.
    cloudTop:           "#d8e2f0",
    cloudBottom:        "#7d93b5",

    // And so the badge variant has to go darker again to keep the same
    // relationship it has in dark: one step of contrast beyond the ordinary
    // cloud, for the one place that is paler than a card.
    cloudTopOnLight:    "#c3d2e6",
    cloudBottomOnLight: "#6b83a8",

    rain:               "#3d7fb8",
    droplet:            "#3f86c4"
};

// The day plate warms rather than pales. Dark's #fdfefe→#dde5f0 is a white disc
// that reads as "day" because the sky behind it is navy; on a light card it is
// a white disc on white. Sunlight is the obvious thing it can be instead.
var badge = {
    dayTop:      "#ffeeb8",
    dayBottom:   "#f6d98a",
    nightTop:    "#6d9ae8",
    nightBottom: "#3f63bd"
};

var scaffold = {
    ink:    "#6b7794",
    stroke: "#8fa6c0d8"
};

// ---- precipitation ----------------------------------------------------------
//
// The wash hues do not move: hue *is* the precipitation type, and rain does not
// become a different kind of rain because the reader turned a light on. What
// moves is everything that was drawn bright to read against a dark sky.
//
// The alphas rise, because a translucent blue over white is fainter than the
// same blue over navy.
//
// The drops invert. In dark they run from pale blue to white as intensity
// rises, which is the direction "more" goes on a dark ground; here they run
// from a soft blue-grey to a deep one, because on a light ground "more" is
// darker. Snow needed a judgement: white flakes are invisible on this page, so
// it is the one type whose drop is a tint rather than a shade.
//
// `flash` is the case with no light-mode equivalent at all. Lightning in dark
// is a frame of near-white over the storm band. Nothing can be brighter than a
// white page, so in light mode the band *deepens* for that frame instead — the
// sign of the effect changes, which PrecipField.qml handles, and §10.11 records
// it.
var precip = {
    wash: {
        drizzle: "#4f9ad4",
        rain:    "#4f9ad4",
        sleet:   "#7ba6cf",
        snow:    "#a9c8e8",
        hail:    "#b8d2ec",
        thunder: "#5d7ed0"
    },
    washAlpha: { light: 0.16, moderate: 0.24, heavy: 0.32 },
    edge: "#0f6d8ef2",
    drop: {
        drizzle: "#7fa0c0",
        rain:    "#6d93b8",
        sleet:   "#5d86ad",
        snow:    "#8fadc9",
        hail:    "#3f6b9c",
        thunder: "#4a7099"
    },
    splash: "#4a7ba8",
    flash:  "#2a3f66"
};

// ---- the sky ----------------------------------------------------------------
//
// All four phases survive and all four are bright. Sky.phaseAt() is untouched:
// it is a pure function of the clock and the sun times, and which palette that
// answer indexes into is not its business.
//
// The four differ in hue at roughly constant lightness, which is the rule dark
// already follows — §10.12 says the difference between phases is "hue and
// clarity, not lightness". Applying an existing rule at a new lightness is not a
// new rule. Two of them will read as nearly identical, and that is honest: a
// bright night and a bright day are the same brightness by construction and
// only the temperature separates them.
//
// A dark night in light mode was the alternative and it is wrong. Someone who
// picks Light at eleven at night has just said what reading conditions they
// want; dimming the page because the sun is down is the app overruling a
// preference it was handed a moment ago.
//
// stars is 0 in all four, and that is a rule rather than a value — see
// star.dark in Theme.qml and §10.12. The field exists because at night the sky
// is the darkest surface on the screen and darkness is the one place on this
// page that can afford texture without competing. On a near-white page there is
// nothing that is simultaneously visible and quieter than a card. A background
// is allowed to have nothing in it; the desktop has been permanently dusk since
// the beginning for the same reason.
//
// It is also cheaper: PageBackdrop already guards on starOpacity > 0, so the
// 130 field stars, 9 beacon Shapes and 3 constellations are never instantiated
// under this theme.
var sky = {
    night: { stops: ["#e8ecf6", "#eff2f9", "#f2f4fa", "#ecf0f7", "#e4e8f2"], stars: 0.00 },
    dawn:  { stops: ["#fbeee6", "#fdf3ea", "#fbf0ef", "#f4ecf2", "#ecebf3"], stars: 0.00 },
    day:   { stops: ["#e4eefb", "#edf4fd", "#f2f7fe", "#eaf1fa", "#e0e9f6"], stars: 0.00 },
    dusk:  { stops: ["#f4ecf6", "#f7effa", "#f6eef4", "#efeaf2", "#e7e6f0"], stars: 0.00 }
};

// ---- ramps ------------------------------------------------------------------
//
// Generated once by tools/theme/ramp-light.mjs and pasted here as literals
// rather than computed at runtime, for the reason theme.js gives about its own
// stops: QML cannot build a GradientStop from a Repeater, the values have to be
// there to be read, and a ramp that is computed is a ramp that cannot be
// reviewed in the gallery next to the one it is derived from.
//
// The three categorical ramps — precip, aqi, uv — are NOT transformed. Their
// hues are published authority bands (the WHO UV scale, the European AQI
// bands), a reader cross-checks them against the source, and §10.5 is explicit
// that where an authority publishes bands, the bands are the palette. Only
// their alpha envelope rises, the same way the precipitation washes do.
//
// The six continuous ramps are remapped: hue and chroma preserved, lightness
// inverted into the band a dark mark on a light ground needs. Inverted rather
// than shifted, because in a dark theme a high value is drawn bright and here
// it has to be drawn dark, and a shift would have kept the brightest stop the
// most prominent one on a page where that now means the least visible.
var ramp = {
    temp: {
        fill: [
                { p: 0, c: "#ccc6491b" },
                { p: 0.2, c: "#ccb77129" },
                { p: 0.35, c: "#cc91704c" },
                { p: 0.45, c: "#c27e8959" },
                { p: 0.55, c: "#b85f8a58" },
                { p: 0.7, c: "#a05c976e" },
                { p: 0.85, c: "#8859a175" },
                { p: 1, c: "#6e5ba579" }
        ],
        line: [
                { p: 0, c: "#f0c33b08" },
                { p: 0.2, c: "#f0b66713" },
                { p: 0.35, c: "#f0906c36" },
                { p: 0.45, c: "#f0798645" },
                { p: 0.55, c: "#f04f8842" },
                { p: 0.7, c: "#f042914f" },
                { p: 0.85, c: "#f0449757" },
                { p: 1, c: "#f049985e" }
        ]
    },
    wind: {
        fill: [
                { p: 0, c: "#ccc73719" },
                { p: 0.18, c: "#ccbe6223" },
                { p: 0.34, c: "#ccac8336" },
                { p: 0.48, c: "#c28b9253" },
                { p: 0.62, c: "#b8628066" },
                { p: 0.74, c: "#a0579098" },
                { p: 0.88, c: "#884d8ca5" },
                { p: 1, c: "#6e4888ac" }
        ],
        line: [
                { p: 0, c: "#f0c32d08" },
                { p: 0.18, c: "#f0b75813" },
                { p: 0.34, c: "#f0a07f2a" },
                { p: 0.48, c: "#f06e8146" },
                { p: 0.62, c: "#f0468574" },
                { p: 0.74, c: "#f0378899" },
                { p: 0.88, c: "#f02c83a7" },
                { p: 1, c: "#f02c7eac" }
        ]
    },
    humidity: {
        fill: [
                { p: 0, c: "#cc256dbf" },
                { p: 0.18, c: "#cc2e71b6" },
                { p: 0.34, c: "#cc3975a9" },
                { p: 0.48, c: "#c24a7897" },
                { p: 0.62, c: "#b85f7a80" },
                { p: 0.74, c: "#a06d7567" },
                { p: 0.88, c: "#88857759" },
                { p: 1, c: "#6e967348" }
        ],
        line: [
                { p: 0, c: "#f01562b5" },
                { p: 0.18, c: "#f01e64ab" },
                { p: 0.34, c: "#f02b679c" },
                { p: 0.48, c: "#f03e6a89" },
                { p: 0.62, c: "#f0556a70" },
                { p: 0.74, c: "#f06c6a58" },
                { p: 0.88, c: "#f0836a42" },
                { p: 1, c: "#f0956931" }
        ]
    },
    cloud: {
        fill: [
                { p: 0, c: "#cc475c7e" },
                { p: 0.18, c: "#cc466285" },
                { p: 0.34, c: "#cc46698c" },
                { p: 0.48, c: "#c2466f94" },
                { p: 0.62, c: "#b846749b" },
                { p: 0.74, c: "#a04778a1" },
                { p: 0.88, c: "#88487aa6" },
                { p: 1, c: "#6e457aad" }
        ],
        line: [
                { p: 0, c: "#f02d4e82" },
                { p: 0.18, c: "#f02d5087" },
                { p: 0.34, c: "#f02c558d" },
                { p: 0.48, c: "#f02b5a94" },
                { p: 0.62, c: "#f02b609a" },
                { p: 0.74, c: "#f02b649f" },
                { p: 0.88, c: "#f02c67a4" },
                { p: 1, c: "#f02e69a7" }
        ]
    },
    pressure: {
        fill: [
                { p: 0, c: "#cc4d2fa4" },
                { p: 0.18, c: "#cc3e359f" },
                { p: 0.34, c: "#cc38439e" },
                { p: 0.48, c: "#c23b58a1" },
                { p: 0.62, c: "#b83d69a4" },
                { p: 0.74, c: "#a03f77a6" },
                { p: 0.88, c: "#884183a8" },
                { p: 1, c: "#6e428cab" }
        ],
        line: [
                { p: 0, c: "#f043209c" },
                { p: 0.18, c: "#f02f2794" },
                { p: 0.34, c: "#f02b3d94" },
                { p: 0.48, c: "#f02c5396" },
                { p: 0.62, c: "#f02d6698" },
                { p: 0.74, c: "#f02f7699" },
                { p: 0.88, c: "#f030829a" },
                { p: 1, c: "#f0328c9c" }
        ]
    },
    visibility: {
        fill: [
                { p: 0, c: "#cc2b84ae" },
                { p: 0.18, c: "#cc3680a4" },
                { p: 0.34, c: "#cc447b98" },
                { p: 0.48, c: "#c253758a" },
                { p: 0.62, c: "#b8626d7c" },
                { p: 0.74, c: "#a0766a71" },
                { p: 0.88, c: "#88815f60" },
                { p: 1, c: "#6e8c5955" }
        ],
        line: [
                { p: 0, c: "#f01880aa" },
                { p: 0.18, c: "#f025789d" },
                { p: 0.34, c: "#f0366f8d" },
                { p: 0.48, c: "#f046677e" },
                { p: 0.62, c: "#f0565d6f" },
                { p: 0.74, c: "#f06c5b64" },
                { p: 0.88, c: "#f0765054" },
                { p: 1, c: "#f0824945" }
        ]
    },
    precip: {
        fill: [
                { p: 0, c: "#ff3f6fd8" },
                { p: 0.18, c: "#ff4879d9" },
                { p: 0.34, c: "#ff5285db" },
                { p: 0.48, c: "#ff5d91dd" },
                { p: 0.62, c: "#ff699edf" },
                { p: 0.74, c: "#ff77abe2" },
                { p: 0.88, c: "#ff86b8e5" },
                { p: 1, c: "#ff96c5e8" }
        ],
        line: [
                { p: 0, c: "#ff6f9be8" },
                { p: 1, c: "#ffb4d8f2" }
        ]
    },
    aqi: {
        fill: [
                { p: 0, c: "#ff8c1e4b" },
                { p: 0.2, c: "#ffc03050" },
                { p: 0.4, c: "#ffe85a50" },
                { p: 0.55, c: "#fff08a45" },
                { p: 0.7, c: "#ffe8c93f" },
                { p: 0.82, c: "#ffa8d15a" },
                { p: 0.92, c: "#ff5cc79a" },
                { p: 1, c: "#ff4ec3c8" }
        ],
        line: [
                { p: 0, c: "#ffb04a70" },
                { p: 1, c: "#ff8fd8dc" }
        ]
    },
    uv: {
        fill: [
                { p: 0, c: "#ff8b5fc4" },
                { p: 0.17, c: "#ffa155bd" },
                { p: 0.33, c: "#ffd6484e" },
                { p: 0.5, c: "#fff07038" },
                { p: 0.63, c: "#fff5a02f" },
                { p: 0.75, c: "#ffe8c73c" },
                { p: 0.88, c: "#ffa9cf55" },
                { p: 1, c: "#ff5ec18a" }
        ],
        line: [
                { p: 0, c: "#ffb98ede" },
                { p: 1, c: "#ff8fd4ac" }
        ]
    }
};
