// SPDX-FileCopyrightText: 2026 Jowi Aoun
// SPDX-License-Identifier: GPL-3.0-or-later
//
// Derives the light theme's gradient ramps from the dark theme's, once.
//
//   node tools/theme/ramp-light.mjs
//
// It prints a JavaScript object literal. Paste it into themelight.js in place
// of `RAMP_PLACEHOLDER`. This is a recipe, not a runtime: the values have to be
// literals in the file because QML cannot build a GradientStop from a Repeater,
// and because a ramp nobody can read in the source is a ramp nobody reviews.
//
// ---- what it does and does not touch ----------------------------------------
//
// SIX CONTINUOUS ramps — temp, wind, humidity, cloud, pressure, visibility —
// are remapped. Their job is "more of this quantity, further along", and the
// direction of "more" reverses with the ground: on navy a high value is drawn
// bright, on white it has to be drawn dark. So lightness is INVERTED into the
// band a dark mark needs, while hue and chroma are preserved, because the hue
// is what says which quantity you are looking at.
//
// THREE CATEGORICAL ramps — precip, aqi, uv — are passed through unchanged
// except for their alpha envelope. Their colours are published authority bands
// (the WHO UV scale, the European AQI bands); a reader cross-checks them
// against the issuer, and §10.5 says that where an authority publishes bands,
// the bands are the palette. Re-hueing them to suit our background would make
// the app disagree with the source it is quoting.

import { readFileSync } from "node:fs";

const CATEGORICAL = new Set(["precip", "aqi", "uv"]);

// Where a remapped stop's lightness is allowed to land. The floor keeps the
// darkest stop off pure black, which on a light page reads as a hole rather
// than as a value; the ceiling keeps the lightest stop from disappearing into
// the card it is drawn on.
const L_FLOOR = 0.32;
const L_CEIL = 0.62;

// Alpha is preserved exactly, and that is a decision rather than an omission.
//
// The precipitation washes in themelight.js do get more alpha, because they sit
// at 0.13-0.27 and a wash that faint over white is nothing. These do not: the
// area fills are already at 0.8 and the strokes at 0.94, and the inversion
// below is what buys their visibility. Gaining them as well took temp.fill from
// 0.80 to 0.98, which is an area fill that has stopped being translucent — and
// the grid lines it is drawn over exist to be seen through it.
const ALPHA_GAIN = 1.0;

function parse(hex) {
    const s = hex.replace("#", "");
    if (s.length === 8) {
        return { a: parseInt(s.slice(0, 2), 16) / 255, r: parseInt(s.slice(2, 4), 16),
                 g: parseInt(s.slice(4, 6), 16), b: parseInt(s.slice(6, 8), 16), hadAlpha: true };
    }
    return { a: 1, r: parseInt(s.slice(0, 2), 16), g: parseInt(s.slice(2, 4), 16),
             b: parseInt(s.slice(4, 6), 16), hadAlpha: false };
}

function fmt({ a, r, g, b, hadAlpha }) {
    const two = (n) => Math.max(0, Math.min(255, Math.round(n))).toString(16).padStart(2, "0");
    const body = two(r) + two(g) + two(b);
    return hadAlpha ? "#" + two(a * 255) + body : "#" + body;
}

function toHsl({ r, g, b }) {
    r /= 255; g /= 255; b /= 255;
    const max = Math.max(r, g, b), min = Math.min(r, g, b);
    const l = (max + min) / 2;
    if (max === min) return { h: 0, s: 0, l };
    const d = max - min;
    const s = l > 0.5 ? d / (2 - max - min) : d / (max + min);
    let h;
    if (max === r) h = ((g - b) / d + (g < b ? 6 : 0)) / 6;
    else if (max === g) h = ((b - r) / d + 2) / 6;
    else h = ((r - g) / d + 4) / 6;
    return { h, s, l };
}

function toRgb({ h, s, l }) {
    if (s === 0) { const v = l * 255; return { r: v, g: v, b: v }; }
    const q = l < 0.5 ? l * (1 + s) : l + s - l * s;
    const p = 2 * l - q;
    const hue = (t) => {
        if (t < 0) t += 1;
        if (t > 1) t -= 1;
        if (t < 1 / 6) return p + (q - p) * 6 * t;
        if (t < 1 / 2) return q;
        if (t < 2 / 3) return p + (q - p) * (2 / 3 - t) * 6;
        return p;
    };
    return { r: hue(h + 1 / 3) * 255, g: hue(h) * 255, b: hue(h - 1 / 3) * 255 };
}

function relight(hex) {
    const c = parse(hex);
    // A fully transparent stop is a shape's tail, not a colour. Remapping it
    // would give it a hue nobody asked for the moment it is interpolated
    // towards, so it keeps whatever it was.
    if (c.a === 0) return hex;

    const hsl = toHsl(c);
    const inverted = 1 - hsl.l;
    const l = L_FLOOR + inverted * (L_CEIL - L_FLOOR);

    // Slightly more chroma, because a darker colour can carry it and because
    // the same saturation reads flatter once the lightness comes down.
    const s = Math.min(1, hsl.s * 1.08);

    const rgb = toRgb({ h: hsl.h, s, l });
    return fmt({ ...rgb, a: Math.min(1, c.a * ALPHA_GAIN), hadAlpha: c.hadAlpha });
}

function gainOnly(hex) {
    const c = parse(hex);
    if (c.a === 0 || !c.hadAlpha) return hex;
    return fmt({ ...c, a: Math.min(1, c.a * ALPHA_GAIN) });
}

const src = readFileSync(new URL("../../app/qml/Clima/theme.js", import.meta.url), "utf8")
    .replace(/^\.pragma library\s*$/m, "");
const { ramp } = new Function(src + "; return { ramp };")();

const out = [];
out.push("{");
for (const [name, def] of Object.entries(ramp)) {
    const map = CATEGORICAL.has(name) ? gainOnly : relight;
    out.push(`    ${name}: {`);
    for (const [kind, stops] of Object.entries(def)) {
        const body = stops
            .map((s) => `{ p: ${s.p}, c: "${map(s.c)}" }`)
            .join(",\n                ");
        out.push(`        ${kind}: [\n                ${body}\n        ],`);
    }
    out[out.length - 1] = out[out.length - 1].replace(/,$/, "");
    out.push("    },");
}
out[out.length - 1] = out[out.length - 1].replace(/,$/, "");
out.push("};");

process.stdout.write(out.join("\n") + "\n");
