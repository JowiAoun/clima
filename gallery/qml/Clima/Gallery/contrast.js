// SPDX-FileCopyrightText: 2026 Jowi Aoun
// SPDX-License-Identifier: GPL-3.0-or-later
// WCAG 2.1 contrast, for the palette page.
//
// This lives in the gallery rather than in Clima because the app never asks
// what a contrast ratio is — it asks for a colour and paints with it. The
// measuring belongs to the instrument.
//
// Two things here are easy to get wrong and are the reason this is a file
// rather than three lines inline:
//
// Compositing comes first. Most of this palette is translucent — `surface.base`
// is 7% white, `line.card` is 8% black — and a ratio taken against the literal
// `#12ffffff` is a ratio against a colour nothing ever renders. Every value is
// composited down onto an opaque background before it is measured, and the
// background is itself composited the same way, because a card is translucent
// white over the page and a nav line is a translucent white over that.
//
// And luminance is not lightness. WCAG's relative luminance linearises each
// channel through the sRGB transfer function before weighting them 0.2126 /
// 0.7152 / 0.0722, which is why a saturated yellow and a saturated blue of the
// same HSL lightness are nowhere near the same ratio against white — the
// finding that `accent.fill` at `#e8a900` reaches only 1.62:1 on a light card
// comes straight out of that weighting.
.pragma library

// "#rgb" is not accepted on purpose: theme.js writes every colour long-form,
// and quietly accepting a short form here would let one in.
function _parse(c) {
    if (c === "transparent")
        return { r: 0, g: 0, b: 0, a: 0 };

    var h = String(c).replace("#", "");
    if (h.length === 6)
        h = "ff" + h;
    if (h.length !== 8)
        return null;

    return {
        r: parseInt(h.substr(2, 2), 16),
        g: parseInt(h.substr(4, 2), 16),
        b: parseInt(h.substr(6, 2), 16),
        a: parseInt(h.substr(0, 2), 16) / 255
    };
}

function _hex2(n) {
    var s = Math.round(Math.max(0, Math.min(255, n))).toString(16);
    return s.length === 1 ? "0" + s : s;
}

// Source-over, with the backdrop taken as opaque. Returns "#rrggbb".
function compose(fg, bg) {
    var f = _parse(fg), b = _parse(bg);
    if (f === null || b === null)
        return "#000000";
    return "#" + _hex2(f.r * f.a + b.r * (1 - f.a))
               + _hex2(f.g * f.a + b.g * (1 - f.a))
               + _hex2(f.b * f.a + b.b * (1 - f.a));
}

function _luminance(c) {
    var p = _parse(c);
    if (p === null)
        return 0;
    var ch = [p.r / 255, p.g / 255, p.b / 255];
    for (var i = 0; i < 3; ++i)
        ch[i] = ch[i] <= 0.03928 ? ch[i] / 12.92
                                 : Math.pow((ch[i] + 0.055) / 1.055, 2.4);
    return 0.2126 * ch[0] + 0.7152 * ch[1] + 0.0722 * ch[2];
}

// Both arguments must already be opaque; compose() first.
function ratio(a, b) {
    var la = _luminance(a), lb = _luminance(b);
    var hi = Math.max(la, lb), lo = Math.min(la, lb);
    return (hi + 0.05) / (lo + 0.05);
}

// "surface.base" -> the raw value in `table`.
function _at(table, path) {
    var parts = path.split(".");
    var group = table[parts[0]];
    return group === undefined ? undefined : group[parts[1]];
}

// The opaque colour a token is actually drawn on, resolved through however many
// translucent layers stand between it and the page.
//
// `control.pagerGlyph` is the deep case and the reason this recurses: the
// chevron sits on `control.pagerFill`, which is 60% of a near-black over the
// card, which is 7% white over the page. Stopping at the first layer would
// measure the chevron against a colour that is 40% transparent, and the answer
// would be wrong in the direction that hides a failure.
//
// `rule` is Theme.contrastRule. `seen` guards a cycle in the rule table — two
// tokens each declaring the other as ground would otherwise recurse until the
// stack gave out, and a typo in a hand-maintained table should not take the
// gallery down with it.
function groundOf(table, rule, path, seen) {
    var page = _at(table, "page.bg");
    if (!path)
        return page;

    seen = seen || {};
    if (seen[path])
        return page;
    seen[path] = true;

    var value = _at(table, path);
    if (value === undefined)
        return page;

    return compose(value, groundOf(table, rule, rule(path).on, seen));
}

// One row of the palette page.
//
// Returns the composited pair actually measured, the ratio between them, and a
// verdict of "pass" | "fail" | "" — empty for an incidental token, which has no
// minimum to meet and so must not be shown as having passed one.
//
// `floor` is Theme.contrastFloor. A paired token is scored on the better of the
// two stops: see the note on `pair` in Theme.qml.
function audit(table, rule, floor, path) {
    var r = rule(path);
    var value = _at(table, path);

    if (value === undefined || r.on === null)
        return { on: null, fg: null, ratio: 0, verdict: "", duty: r.duty };

    var ground = groundOf(table, rule, r.on);
    var fg = compose(value, ground);
    var own = ratio(fg, ground);

    var min = floor(r.duty);
    if (min === 0)
        return { on: ground, fg: fg, ratio: own, verdict: "", duty: r.duty };

    var best = own;
    if (r.pair !== undefined) {
        var sr = rule(r.pair);
        var sv = _at(table, r.pair);
        if (sv !== undefined) {
            var sg = groundOf(table, rule, sr.on);
            best = Math.max(best, ratio(compose(sv, sg), sg));
        }
    }

    return {
        on: ground,
        fg: fg,
        ratio: own,
        paired: r.pair,
        verdict: best >= min ? "pass" : "fail",
        floor: min,
        duty: r.duty
    };
}
