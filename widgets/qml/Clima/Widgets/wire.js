// SPDX-FileCopyrightText: 2026 Jowi Aoun
// SPDX-License-Identifier: GPL-3.0-or-later
//
// Reading a snapshot without turning "we do not know" into a number.
//
// ============================================================================
// THE ONE RULE
//
// A value on the wire is a number, `null` (the provider carries no reading) or
// `undefined` (the field mask never asked for it). JavaScript will happily
// treat the last two as 0 — `null + 1` is 1, `Math.max(null, 5)` is 5,
// `(0).toFixed(0)` is "0" — and every one of those produces a tile that states
// a fact nobody measured. libclima/wire/snapshot.h calls this rule 2 and
// spends a paragraph on it; this file is the QML-side half.
//
// So: nothing here ever substitutes a default silently. `num()` answers NaN,
// which propagates through arithmetic and which `text()` renders as a dash. A caller that genuinely wants a fallback has to pass one and can be
// found by grep.
//
// ============================================================================
// WHY .pragma library
//
// Because it is pure computation over values that are handed in, with no state
// and no bindings of its own — the case a shared JS library is actually for.
// (Theme.qml is the opposite case and its header explains why: a `.pragma
// library` produces no change notification, so a *token table* could not live
// in one. Nothing here is watched.)

.pragma library

// ---- presence ---------------------------------------------------------------

// True for a real number. NaN fails its own equality test, which is the whole
// trick and the reason this is not `!== null && !== undefined`.
function has(v) {
    return v !== null && v !== undefined && !(typeof v === "number" && isNaN(v))
}

// A number, or NaN. Never 0 by accident.
function num(v) {
    if (v === null || v === undefined)
        return NaN
    var n = Number(v)
    return isNaN(n) ? NaN : n
}

// A number, or the fallback the caller asked for by name.
function numOr(v, fallback) {
    var n = num(v)
    return isNaN(n) ? fallback : n
}

// A real JavaScript array, whatever shape the value arrived in.
//
// `Array.isArray()` alone is not enough and the way it fails is silent. A JSON
// array reaches QML through QVariantMap::toVariantMap() as a **QVariantList**,
// which QML hands to JavaScript as a sequence wrapper: it has a `length`, it
// indexes, and `Array.isArray()` returns FALSE for it. A guard written as
// `Array.isArray(v) ? v : []` therefore turns every series in the snapshot into
// an empty array — the tiles lay out correctly, draw nothing, and look exactly
// like a tile waiting for its first snapshot.
//
// Copied into a fresh array rather than returned as the wrapper, because the
// wrapper is not an Array and has none of its methods: `.concat()` and `.map()`
// on one throw, and the throw surfaces as a binding that silently evaluates to
// undefined.
function arr(v) {
    if (v === null || v === undefined)
        return []
    if (Array.isArray(v))
        return v
    if (typeof v === "object" && typeof v.length === "number") {
        var out = []
        for (var i = 0; i < v.length; ++i)
            out.push(v[i])
        return out
    }
    return []
}

function obj(v) {
    return (v !== null && typeof v === "object" && !Array.isArray(v)) ? v : ({})
}

// A dotted lookup that never throws on a missing branch:
// `at(snap, "current.temperature")`.
function at(root, path) {
    var node = root
    var parts = path.split(".")
    for (var i = 0; i < parts.length; ++i) {
        if (node === null || node === undefined)
            return undefined
        node = node[parts[i]]
    }
    return node
}

// ---- printing ---------------------------------------------------------------

// The en dash Units::formatDisplay already answers for a NaN, so a tile that
// mixes a unit-bearing reading with a bare one shows one dash and not two.
var DASH = "–"

// The one place a reading becomes text. An absent value is a dash and not
// an empty string, because a blank space where a number goes reads as a layout
// bug and a dash reads as "the provider does not carry this" — which is what it
// means.
function text(v, decimals, suffix) {
    var n = num(v)
    if (isNaN(n))
        return DASH
    var body = n.toFixed(decimals === undefined ? 0 : decimals)
    return suffix === undefined ? body : body + suffix
}

// A temperature, in whatever unit the reader chose. `convert` is
// Units.convert bound by the caller, because this library may not import a
// singleton.
function degrees(v, convert) {
    var n = num(v)
    if (isNaN(n))
        return DASH
    return Math.round(convert(n)) + "°"
}

// ---- series -----------------------------------------------------------------

// The largest and smallest real values in a series, ignoring the holes.
// Returns null when there is nothing real in it at all — which a caller has to
// handle, because an axis over no data is not an axis.
function extent(values) {
    var lo = Infinity, hi = -Infinity, seen = false
    for (var i = 0; i < values.length; ++i) {
        var n = num(values[i])
        if (isNaN(n))
            continue
        seen = true
        if (n < lo) lo = n
        if (n > hi) hi = n
    }
    return seen ? { lo: lo, hi: hi } : null
}

// Chart points for a series, with the holes left out rather than drawn as
// zero. A gap in the line is honest; a dive to the baseline is not.
//
// `w` and `h` are the plot box; `lo` and `hi` the value axis.
function points(values, w, h, lo, hi) {
    var out = []
    var span = (hi - lo) || 1
    var step = values.length > 1 ? w / (values.length - 1) : 0
    for (var i = 0; i < values.length; ++i) {
        var n = num(values[i])
        if (isNaN(n))
            continue
        out.push({ x: i * step, y: h - ((n - lo) / span) * h })
    }
    return out
}

// A little headroom above and below, so a flat day is a line across the middle
// rather than a line along the top edge.
function padded(range, minimumSpan) {
    if (range === null)
        return { lo: 0, hi: 1 }
    var span = range.hi - range.lo
    var floor = minimumSpan === undefined ? 2 : minimumSpan
    if (span < floor) {
        var middle = (range.hi + range.lo) / 2
        return { lo: middle - floor / 2, hi: middle + floor / 2 }
    }
    var pad = span * 0.12
    return { lo: range.lo - pad, hi: range.hi + pad }
}

// Where in [0,1] a value sits on a fixed scale, clamped. For a colour ramp,
// which has to be given something even when the reading is silly.
function fraction(v, lo, hi) {
    var n = num(v)
    if (isNaN(n))
        return NaN
    var t = (n - lo) / ((hi - lo) || 1)
    return t < 0 ? 0 : (t > 1 ? 1 : t)
}
