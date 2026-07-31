// SPDX-FileCopyrightText: 2026 Jowi Aoun
// SPDX-License-Identifier: GPL-3.0-or-later
// Path generation for the temperature curve.
//
// Paths are emitted as SVG path strings and handed to a single PathSvg element,
// because QML's declarative path elements cannot be produced by a Repeater. The
// production implementation moves this to a QQuickItem writing QSGGeometryNodes
// (decision D3 in docs/03-tech-stack.md); the maths below is what it will port.
.pragma library

function clamp(v, lo, hi) { return v < lo ? lo : (v > hi ? hi : v); }

function n(v) { return Math.round(v * 100) / 100; }

function offsetY(pts, dy) {
    var out = [];
    for (var i = 0; i < pts.length; ++i)
        out.push({ x: pts[i].x, y: pts[i].y + dy });
    return out;
}

function reverse(pts) {
    var out = [];
    for (var i = pts.length - 1; i >= 0; --i)
        out.push(pts[i]);
    return out;
}

// Catmull-Rom spline emitted as cubic Béziers.
//
// Control-point y is clamped to the span of its own segment, which makes the
// curve effectively monotone between samples. That matters here: an unclamped
// spline overshoots on a flat-then-rising series and invents a dip the forecast
// does not contain. A weather chart must not draw data that isn't there.
function smooth(pts, startCmd) {
    if (!pts || pts.length === 0)
        return "";

    var d = (startCmd || "M") + " " + n(pts[0].x) + " " + n(pts[0].y);
    if (pts.length === 1)
        return d;

    for (var i = 0; i < pts.length - 1; ++i) {
        var p0 = pts[i > 0 ? i - 1 : 0];
        var p1 = pts[i];
        var p2 = pts[i + 1];
        var p3 = pts[i + 2 < pts.length ? i + 2 : i + 1];

        var lo = Math.min(p1.y, p2.y);
        var hi = Math.max(p1.y, p2.y);

        var c1x = p1.x + (p2.x - p0.x) / 6;
        var c1y = clamp(p1.y + (p2.y - p0.y) / 6, lo, hi);
        var c2x = p2.x - (p3.x - p1.x) / 6;
        var c2y = clamp(p2.y - (p3.y - p1.y) / 6, lo, hi);

        d += " C " + n(c1x) + " " + n(c1y)
           + " "   + n(c2x) + " " + n(c2y)
           + " "   + n(p2.x) + " " + n(p2.y);
    }
    return d;
}

// Closed area between the curve and the value-axis baseline.
function areaPath(pts, baselineY) {
    if (!pts || pts.length < 2)
        return "";
    return smooth(pts, "M")
         + " L " + n(pts[pts.length - 1].x) + " " + n(baselineY)
         + " L " + n(pts[0].x) + " " + n(baselineY)
         + " Z";
}

// ShapePath can gradient-fill but not gradient-stroke, so the line is drawn as a
// thin closed ribbon around the curve and filled with the same vertical gradient.
// A pure vertical offset is accurate enough because temperature curves are shallow.
function ribbonPath(pts, halfWidth) {
    if (!pts || pts.length < 2)
        return "";
    return smooth(offsetY(pts, -halfWidth), "M")
         + " " + smooth(reverse(offsetY(pts, halfWidth)), "L")
         + " Z";
}

// Horizontal value-axis gridlines, one subpath per tick.
function gridPath(width, ticks, yFn) {
    var d = "";
    for (var i = 0; i < ticks.length; ++i) {
        var y = n(yFn(ticks[i]));
        d += " M 0 " + y + " L " + n(width) + " " + y;
    }
    return d.trim();
}

// Faint vertical guides at every label position.
function guidePath(indices, height, xFn) {
    var d = "";
    for (var i = 0; i < indices.length; ++i) {
        var x = n(xFn(indices[i]));
        d += " M " + x + " 0 L " + x + " " + n(height);
    }
    return d.trim();
}

// Diagonal hatch fill, used for "this is the past, there is no forecast here".
function hatchPath(width, height, spacing, slope) {
    if (width <= 0 || height <= 0 || spacing <= 0)
        return "";
    var d = "";
    var run = height * slope;
    for (var x = -run; x < width + spacing; x += spacing)
        d += " M " + n(x) + " " + n(height) + " L " + n(x + run) + " 0";
    return d.trim();
}

// Sample a colour ramp at normalised axis position p (0 = axis top, 1 = bottom).
// Bars need one flat colour per value, so they cannot use the gradient path and
// interpolate here instead. Ramp entries are "#aarrggbb".
function sampleRamp(stops, p) {
    if (!stops || stops.length === 0)
        return "#ffffffff";
    p = clamp(p, 0, 1);
    if (p <= stops[0].p)
        return stops[0].c;
    if (p >= stops[stops.length - 1].p)
        return stops[stops.length - 1].c;

    for (var i = 0; i < stops.length - 1; ++i) {
        var a = stops[i], b = stops[i + 1];
        if (p >= a.p && p <= b.p) {
            var t = (b.p - a.p) === 0 ? 0 : (p - a.p) / (b.p - a.p);
            return _mixHex(a.c, b.c, t);
        }
    }
    return stops[stops.length - 1].c;
}

function _mixHex(ca, cb, t) {
    var a = _parseHex(ca), b = _parseHex(cb);
    var out = "#";
    for (var i = 0; i < 4; ++i) {
        var v = Math.round(a[i] + (b[i] - a[i]) * t);
        out += (v < 16 ? "0" : "") + v.toString(16);
    }
    return out;
}

// "#aarrggbb" or "#rrggbb" -> [a, r, g, b]
function _parseHex(c) {
    var s = c.charAt(0) === "#" ? c.substring(1) : c;
    if (s.length === 6)
        s = "ff" + s;
    return [parseInt(s.substr(0, 2), 16), parseInt(s.substr(2, 2), 16),
            parseInt(s.substr(4, 2), 16), parseInt(s.substr(6, 2), 16)];
}

// Lit limb of the moon, centred on (cx, cy), lit side to the left.
// illum is the illuminated fraction: 0.5 = half, >0.5 = gibbous, <0.5 = crescent.
function moonPath(cx, cy, r, illum) {
    var k = clamp(illum, 0.03, 0.97);
    var rx = n(r * Math.abs(2 * k - 1));
    var sweep = k > 0.5 ? 0 : 1;   // gibbous bulges away from the lit side
    return "M " + n(cx) + " " + n(cy - r)
         + " A " + n(r) + " " + n(r) + " 0 0 0 " + n(cx) + " " + n(cy + r)
         + " A " + rx + " " + n(r) + " 0 0 " + sweep + " " + n(cx) + " " + n(cy - r)
         + " Z";
}
