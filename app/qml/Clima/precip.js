// SPDX-FileCopyrightText: 2026 Jowi Aoun
// SPDX-License-Identifier: GPL-3.0-or-later
// Precipitation as an interval, not as a number per hour.
//
// A column of millimetres answers "how much" and makes you read an axis to find
// out "when" — but "when" is the question a forecast is actually opened to
// answer. So precipitation gets a second, non-numeric encoding: the hours it
// falls in are washed and textured, and the texture says what kind and how
// hard. That reads at a glance, on every metric tab, without displacing the
// series the tab is about.
//
// This file is the whole model — thresholds, the runs they group into, and a
// deterministic particle field. It draws nothing and names no colours:
// `PrecipBands` and `PrecipField` render what this describes, `theme.js` says
// in what colour.
.pragma library

// ---------------------------------------------------------------------------
// Classification
// ---------------------------------------------------------------------------
// Rates are millimetres of liquid water per hour, which is what every provider
// reports and the only unit that makes rain and snow comparable at all. The
// rain thresholds are the US NWS bands. The snow ones are roughly a third of
// them, because the same water arrives as about ten times the depth: a
// millimetre an hour is already a centimetre of snow on the ground, which
// nobody would call light.

var TRACE         = 0.1;    // below this the hour is dry
var RAIN_MODERATE = 2.5;
var RAIN_HEAVY    = 7.6;
var SNOW_MODERATE = 0.8;
var SNOW_HEAVY    = 2.5;

// Types are the visual vocabulary, not a meteorological taxonomy: two kinds of
// falling water that look different get two entries, and two that look the same
// share one. `thunder` and `hail` are in the list and were never derivable from
// an amount and a temperature — they come from the provider's weather code.
var TYPES = ["drizzle", "rain", "sleet", "snow", "hail", "thunder"];
var LEVELS = ["light", "moderate", "heavy"];

function intensityFor(type, mm) {
    var mod = type === "snow" ? SNOW_MODERATE : RAIN_MODERATE;
    var hi  = type === "snow" ? SNOW_HEAVY    : RAIN_HEAVY;
    return mm >= hi ? "heavy" : (mm >= mod ? "moderate" : "light");
}

// One entry per hour, null where it is dry. This is the shape everything below
// takes, and the shape the view model produces the inputs for.
//
// ---- where the type comes from now -----------------------------------------
//
// This file used to derive it, from the amount and the temperature, with a
// `code` argument sitting unused for the day a provider sent one. That day has
// arrived: `libclima/domain/weathercode.h` maps the WMO code to exactly these
// six names, and `Data.precipTypes` is the result, one per hour, empty where
// the hour is dry.
//
// The old fallback is gone rather than kept as a backstop, and deliberately.
// Guessing snow from a sub-zero temperature was defensible when nothing better
// existed; beside a code that says 71 it is a second answer to a question that
// already has one, and the two disagree on exactly the hours that matter —
// freezing rain, and a thunderstorm at 24 °C that the fallback calls ordinary
// rain. An hour with an amount and no code is dry here, which reads as "we do
// not know what was falling", because we do not.
//
// Intensity stays. It is a statement about MILLIMETRES PER HOUR — the NWS
// bands above — so `mmArr` must be millimetres whatever unit the reader has
// asked for, and app/viewmodels/forecastdata.h keeps `precipMm` canonical for
// this reason and this reason only.
function cellsTyped(mmArr, typeArr) {
    var out = [];
    for (var i = 0; i < mmArr.length; ++i) {
        var type = typeArr && typeArr[i] ? typeArr[i] : "";
        var mm = mmArr[i];
        out.push(type === "" || !(mm >= TRACE)
                     ? null
                     : { type: type, intensity: intensityFor(type, mm), mm: mm });
    }
    return out;
}

// A run of identical hours, for building a specimen or a test out of a level
// rather than out of a weather situation.
//
// The amount is chosen to land in the band asked for rather than passed
// through, because `spans` reclassifies from the amount anyway — so a cell
// claiming "heavy" at 1 mm would be quietly overruled and the specimen would
// be lying about which level it is showing.
//
// Drizzle has one level by definition: it is rain under 0.4 mm/h, which is
// under every threshold there is. Asking for heavy drizzle gets light drizzle,
// and that is the model being right rather than the call being ignored.
function uniform(count, type, intensity) {
    var table = type === "snow"    ? { light: 0.4,  moderate: 1.5,  heavy: 4.0 }
              : type === "drizzle" ? { light: 0.30, moderate: 0.30, heavy: 0.30 }
                                   : { light: 1.2,  moderate: 4.5,  heavy: 12.0 };
    var mm = table[intensity] !== undefined ? table[intensity] : table.moderate;

    var out = [];
    for (var i = 0; i < count; ++i)
        out.push({ type: type, intensity: intensityFor(type, mm), mm: mm });
    return out;
}

var _NOUN = {
    drizzle: "drizzle", rain: "rain", sleet: "sleet",
    snow: "snow", hail: "hail", thunder: "thunderstorms"
};

function label(type, intensity) {
    var noun = _NOUN[type] ? _NOUN[type] : type;
    // Drizzle is already the light end of rain and a thunderstorm is already
    // the heavy end of it. Qualifying either reads as a contradiction, so the
    // qualifier is dropped rather than the type.
    var qualified = type !== "drizzle" && type !== "thunder";
    var lead = !qualified ? ""
             : intensity === "light" ? "light "
             : intensity === "heavy" ? "heavy " : "";
    var s = lead + noun;
    return s.charAt(0).toUpperCase() + s.substring(1);
}

// ---------------------------------------------------------------------------
// Runs
// ---------------------------------------------------------------------------

// What a run is grouped by. Not the type — the *kind of weather event* the type
// belongs to.
//
// Drizzle and rain are the same event seen at two strengths, and a provider
// switches between their codes hour by hour through one wet afternoon: 51, 51,
// 61, 51, 51 is a perfectly ordinary Open-Meteo day. Split on the type, that
// afternoon becomes three runs, and three runs abutting look exactly like one
// wash laid twice over the same hours — each draws its own pair of edges, so
// every seam is a doubled line, and each is captioned, so "Drizzle" is printed
// twice over what is plainly one spell. That is a real report from a real
// screen, and it is the reason this function groups by family.
//
// Everything else keeps its own family, because everything else is genuinely a
// different event. Rain turning to snow is the thing the seam was invented to
// say. Thunder has to stay separate for a second reason as well: PrecipField
// flashes a band whose type is `thunder`, and a storm hour folded into the rain
// around it would light up the whole afternoon.
var _FAMILY = {
    drizzle: "rain", rain: "rain",
    sleet: "sleet", snow: "snow", hail: "hail", thunder: "thunder"
};

function family(type) {
    return _FAMILY[type] ? _FAMILY[type] : type;
}

// Contiguous hours of one family. Intensity varies hour to hour inside a run and
// the field shows that variation; the run as a whole is labelled by its peak,
// because "heavy rain, 2 to 6" is what you would say out loud about a spell
// with one heavy hour in it.
//
// Runs split on family and not on intensity: rain easing off is still the same
// rain, and cutting the wash at every step change would draw four events where
// there is one.
//
// The peak carries the type as well as the amount, so a drizzly stretch with one
// proper rain hour in it is named for that hour rather than for the drizzle
// either side. Both spell the wash the same colour (theme.js gives drizzle and
// rain one hue on purpose), so this decides the caption and the alpha, not the
// paint.
function spans(cs) {
    var out = [];
    var cur = null;
    for (var i = 0; i < cs.length; ++i) {
        var c = cs[i];
        if (c && cur && family(c.type) === cur.family) {
            cur.to = i;
            if (c.mm > cur.peakMm) {
                cur.peakMm = c.mm;
                cur.type   = c.type;
            }
        } else {
            if (cur)
                out.push(cur);
            cur = c ? { from: i, to: i, family: family(c.type), type: c.type, peakMm: c.mm }
                    : null;
        }
    }
    if (cur)
        out.push(cur);

    for (var k = 0; k < out.length; ++k) {
        out[k].intensity = intensityFor(out[k].type, out[k].peakMm);
        out[k].label = label(out[k].type, out[k].intensity);
    }
    return out;
}

// An hour's sample is an instant, but the rain it reports is an interval: every
// provider gives the total for the hour *starting* at that timestamp. So hour i
// occupies [i, i+1) on the time axis. Centring the wash on the sample instead
// would claim rain for the half hour before it starts, which is precisely the
// half hour someone is deciding whether to leave in.
function bandX(span, hourWidth) {
    return span.from * hourWidth;
}

function bandW(span, hourWidth, maxX) {
    var x0 = span.from * hourWidth;
    var x1 = (span.to + 1) * hourWidth;
    if (maxX > 0)
        x1 = Math.min(x1, maxX);
    return Math.max(0, x1 - x0);
}

// ---------------------------------------------------------------------------
// The field
// ---------------------------------------------------------------------------
// Every drop's position, size and timing is a hash of its hour and its index
// within that hour. Nothing here calls Math.random, so the same forecast draws
// the same rain on every run — the promise the view models already make, and the
// one that lets a headless grab be a golden image.

function _hash(x) {
    var t = x >>> 0;
    t = Math.imul(t ^ (t >>> 16), 2246822507) >>> 0;
    t = Math.imul(t ^ (t >>> 13), 3266489909) >>> 0;
    return (t ^ (t >>> 16)) >>> 0;
}

function rnd(a, b) {
    return _hash(_hash(a >>> 0) + (b >>> 0)) / 4294967296;
}

function _between(a, b, lo, hi) {
    return lo + rnd(a, b) * (hi - lo);
}

// The field is animated by one clock that runs 0 → LOOP and repeats, and every
// particle's own progress is `(clock * rate + offset) mod 1`. That wraps
// seamlessly only where `LOOP * rate` is a whole number, so rates are quantised
// to LOOP steps below. Without it the whole field jumps once per loop — a
// glitch rare enough to be blamed on anything.
var LOOP = 60;

function _quantise(rate) {
    return Math.max(1, Math.round(rate * LOOP)) / LOOP;
}

// Per type: what one drop of it looks like and how it moves.
//
//   kind    "streak" a falling line | "flake" drifting | "pellet" hard and fast
//   count   drops per hour at moderate intensity
//   len     streak length in px; `size` is a flake or pellet's diameter
//   rate    falls per second — 1.0 crosses the plot once a second
//   splash  splashes per drop; frozen things do not splash
//   slant   degrees off vertical, so the field reads as falling rather than as
//           a picket fence
var STYLE = {
    drizzle: { kind: "streak", count:  7, len: [4, 10],    width: 1.0, alpha: 0.42,
               rate: [0.50, 0.70], splash: 0.25, slant: 5 },
    rain:    { kind: "streak", count:  4, len: [14, 30],   width: 1.0, alpha: 0.62,
               rate: [0.90, 1.30], splash: 0.55, slant: 4 },
    // Sleet is not one thing falling, it is rain with ice in it — and drawn as
    // short rain it reads as rain in a hurry. `mix` puts that share of its
    // drops down as pellets instead, which at this size is the only thing that
    // separates the two.
    sleet:   { kind: "streak", count:  5, len: [7, 15],    width: 1.4, alpha: 0.64,
               rate: [1.00, 1.40], splash: 0.45, slant: 8,
               mixShare: 0.45, mixSize: [1.8, 3.2] },
    thunder: { kind: "streak", count:  8, len: [20, 40],   width: 1.2, alpha: 0.64,
               rate: [1.20, 1.70], splash: 0.60, slant: 3 },
    // Floor of 2.8 rather than 2: light snow scaled to 1.8 px, and a rounded
    // rectangle under two pixels across is a square whatever its radius says.
    snow:    { kind: "flake",  count:  8, size: [2.8, 4.8], alpha: 0.85,
               rate: [0.14, 0.24], splash: 0, sway: [7, 18] },
    hail:    { kind: "pellet", count:  6, size: [2.8, 4.4], alpha: 0.92,
               rate: [1.50, 2.10], splash: 0.35, slant: 6 }
};

// Intensity scales the field rather than switching it. Twice the drops, longer
// and faster, is what heavier weather looks like out of a window — and it means
// six named levels are six points on a continuum rather than six pictures
// somebody has to keep in sync.
var GAIN = {
    light:    { count: 0.50, size: 0.75, rate: 0.82 },
    moderate: { count: 1.00, size: 1.00, rate: 1.00 },
    heavy:    { count: 1.90, size: 1.40, rate: 1.25 }
};

function _styleOf(c) { return STYLE[c.type] ? STYLE[c.type] : STYLE.rain }
function _gainOf(c)  { return GAIN[c.intensity] ? GAIN[c.intensity] : GAIN.moderate }

function drops(cs, hourWidth, plotHeight) {
    var out = [];
    if (!cs || hourWidth <= 0 || plotHeight <= 0)
        return out;

    for (var i = 0; i < cs.length; ++i) {
        var c = cs[i];
        if (!c)
            continue;

        var s = _styleOf(c), g = _gainOf(c);
        var n = Math.max(1, Math.round(s.count * g.count));

        for (var k = 0; k < n; ++k) {
            var seed = i * 977 + k;
            var mixed = s.mixShare > 0 && rnd(seed, 8) < s.mixShare;
            var kind = mixed ? "pellet" : s.kind;

            var d = {
                type:   c.type,
                kind:   kind,
                x:      (i + rnd(seed, 1)) * hourWidth,
                alpha:  Math.min(1, s.alpha * _between(seed, 2, 0.70, 1.15)),
                rate:   _quantise(_between(seed, 3, s.rate[0], s.rate[1]) * g.rate),
                offset: rnd(seed, 4),
                slant:  s.slant ? s.slant : 0,
                sway:   0,
                swayPhase: 0
            };

            if (kind === "streak") {
                d.width = s.width;
                d.len = _between(seed, 5, s.len[0], s.len[1]) * g.size;
            } else {
                var range = mixed ? s.mixSize : s.size;
                var size = _between(seed, 5, range[0], range[1]) * g.size;
                d.width = size;
                d.len = size;
                if (s.sway) {
                    d.sway = _between(seed, 6, s.sway[0], s.sway[1]);
                    d.swayPhase = rnd(seed, 7);
                }
            }
            out.push(d);
        }
    }
    return out;
}

// Splashes are the half of the reference's effect that sells it: without them
// the drops fall through the chart and nothing arrives anywhere. They are not
// tied to a particular drop — matching each one to its own impact would need
// the field to know where the ground is, and on a temperature chart there is no
// ground. Scattering them over the plot reads the same and costs nothing.
function splashes(cs, hourWidth, plotHeight) {
    var out = [];
    if (!cs || hourWidth <= 0 || plotHeight <= 0)
        return out;

    for (var i = 0; i < cs.length; ++i) {
        var c = cs[i];
        if (!c)
            continue;

        var s = _styleOf(c), g = _gainOf(c);
        if (!s.splash)
            continue;

        var n = Math.round(s.count * g.count * s.splash);
        for (var k = 0; k < n; ++k) {
            var seed = i * 613 + k + 7919;
            out.push({
                type:   c.type,
                x:      (i + rnd(seed, 1)) * hourWidth,
                y:      plotHeight * _between(seed, 2, 0.12, 0.96),
                size:   _between(seed, 3, 5, 9) * g.size,
                alpha:  Math.min(1, s.alpha * 1.15),
                rate:   _quantise(_between(seed, 4, 0.55, 0.90)),
                offset: rnd(seed, 5)
            });
        }
    }
    return out;
}
