// SPDX-FileCopyrightText: 2026 Jowi Aoun
// SPDX-License-Identifier: GPL-3.0-or-later
//
// Shared machinery for refcap. Both the single-target capture and the
// whole-page crawl go through the same browser setup and the same DOM dump —
// if they diverged, two captures of the same component would disagree with no
// sign that anything was wrong.

import { execFile } from 'node:child_process';
import { promisify } from 'node:util';

const exec = promisify(execFile);

// ---------------------------------------------------------------------------
// Vision budget
//
// Claude 4.7 and later read images in 28x28 patches, up to 4784 patches and a
// 2576 px long edge; anything larger is downscaled before the model ever sees
// it. Capturing above that ceiling costs time and returns nothing. Capturing
// below it throws away detail that was free.
export const MAX_LONG_EDGE = 2576;
export const MAX_PATCHES = 4784;
const DPR_LADDER = [4, 3, 2.5, 2, 1.5, 1];

export const patchesFor = (w, h) => Math.ceil(w / 28) * Math.ceil(h / 28);
export const fits = (w, h) => Math.max(w, h) <= MAX_LONG_EDGE && patchesFor(w, h) <= MAX_PATCHES;

export function bestScale(cssW, cssH) {
    for (const dpr of DPR_LADDER) if (fits(cssW * dpr, cssH * dpr)) return dpr;
    return 1;   // bigger than the budget even at 1x; caller warns
}

/**
 * Shrink an already-rendered PNG to the largest size inside the vision budget.
 *
 * The crawl renders the whole page once at a high DPR and clips components out
 * of it, so a large component can land over budget. Downscaling from that
 * render beats re-rendering at a lower DPR — it is supersampled, so edges and
 * text come out cleaner than a native low-DPR pass would give.
 */
export async function fitToBudget(file, w, h) {
    if (fits(w, h)) return { w, h, resized: false };
    let scale = MAX_LONG_EDGE / Math.max(w, h);
    // The patch cap can still bite on a near-square image, so walk it down.
    while (patchesFor(Math.round(w * scale), Math.round(h * scale)) > MAX_PATCHES) scale *= 0.97;
    const nw = Math.round(w * scale), nh = Math.round(h * scale);
    await exec('ffmpeg', ['-v', 'error', '-y', '-i', file,
        '-vf', `scale=${nw}:${nh}:flags=lanczos`, file + '.tmp.png']);
    await exec('mv', [file + '.tmp.png', file]);
    return { w: nw, h: nh, resized: true };
}

// ---------------------------------------------------------------------------
// Determinism
//
// A capture that changes with the machine's location, unit preference, clock
// or ad auction is not a reference — two runs would disagree for reasons that
// have nothing to do with the design. Pin every one of those.
export const CITIES = {
    seattle: { l: 'Seattle', r: 'Washington', r2: 'King', c: 'United States', i: 'US', x: '-122.33207', y: '47.60621', tz: 'America/Los_Angeles' },
    toronto: { l: 'Toronto', r: 'Ontario', r2: 'Toronto', c: 'Canada', i: 'CA', x: '-79.38318', y: '43.65323', tz: 'America/Toronto' },
    london: { l: 'London', r: 'England', r2: 'Greater London', c: 'United Kingdom', i: 'GB', x: '-0.12574', y: '51.50853', tz: 'Europe/London' },
    phoenix: { l: 'Phoenix', r: 'Arizona', r2: 'Maricopa', c: 'United States', i: 'US', x: '-112.07404', y: '33.44838', tz: 'America/Phoenix' },
    reykjavik: { l: 'Reykjavik', r: 'Capital Region', r2: 'Reykjavik', c: 'Iceland', i: 'IS', x: '-21.89541', y: '64.13548', tz: 'Atlantic/Reykjavik' },
    singapore: { l: 'Singapore', r: 'Singapore', r2: 'Singapore', c: 'Singapore', i: 'SG', x: '103.85007', y: '1.28967', tz: 'Asia/Singapore' },
};

export function locParam(city, market = 'en-us') {
    const c = CITIES[city];
    if (!c) throw new Error(`unknown city "${city}" — have: ${Object.keys(CITIES).join(', ')}`);
    const { tz, ...loc } = c;
    return Buffer.from(JSON.stringify({ ...loc, g: market })).toString('base64');
}

// Ads are most of the page weight, they reflow the layout as they settle, and
// they are never what we came for.
const JUNK = [
    'doubleclick.net', 'googlesyndication.com', 'googletagmanager.com',
    'google-analytics.com', 'adnxs.com', 'amazon-adsystem.com', 'criteo',
    'taboola', 'outbrain', 'scorecardresearch', 'bing.com/api/ads',
    'c.msn.com/c.gif', 'browser.events.data.msn.com',
];

// Anything that floats over a component and would land in its clip.
export const QUIET_CSS = `
    [class*="ad-"],[id*="ads-"],display-ads,iframe,[class*="cookie"],
    [class*="Toast"],[class*="toast"],[class*="tooltip-callout"] { display: none !important; }
    *,*::before,*::after { animation-play-state: paused !important;
                           scroll-behavior: auto !important; }`;

const UA = 'Mozilla/5.0 (X11; Linux x86_64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/151.0.0.0 Safari/537.36';

export async function newContext(browser, { dpr, theme, tz, width, height }) {
    const ctx = await browser.newContext({
        viewport: { width, height },
        deviceScaleFactor: dpr,
        colorScheme: theme,
        locale: 'en-US',
        timezoneId: tz,
        userAgent: UA,
    });
    await ctx.route('**/*', route =>
        JUNK.some(j => route.request().url().includes(j)) ? route.abort() : route.continue());
    return ctx;
}

/**
 * Scroll the page end to end so lazy modules mount, then return to the top.
 *
 * Half of a long MSN page does not exist until it has been near the viewport.
 * Without this the crawl finds the first three cards and reports the rest as
 * absent, which looks exactly like a page that only has three cards.
 */
export async function primeLazyContent(page, { step = 700, settle = 260 } = {}) {
    const height = await page.evaluate(() => document.documentElement.scrollHeight);
    for (let y = 0; y < height; y += step) {
        await page.evaluate(_y => window.scrollTo(0, _y), y);
        await page.waitForTimeout(settle);
    }
    await page.evaluate(() => window.scrollTo(0, 0));
    await page.waitForTimeout(700);
}

// ---------------------------------------------------------------------------
// What is worth recording about an element.
//
// A full computed-style dump is ~340 properties per node and unreadable. These
// are the ones a reimplementation actually has to match.
export const VISUAL_PROPS = [
    'color', 'background-color', 'background-image', 'background-size',
    'background-position', 'opacity',
    'border-top-width', 'border-right-width', 'border-bottom-width', 'border-left-width',
    'border-top-color', 'border-right-color', 'border-bottom-color', 'border-left-color',
    'border-top-style',
    'border-top-left-radius', 'border-top-right-radius',
    'border-bottom-right-radius', 'border-bottom-left-radius',
    'box-shadow', 'filter', 'backdrop-filter', 'mix-blend-mode',
    'font-family', 'font-size', 'font-weight', 'font-style',
    'letter-spacing', 'line-height', 'text-transform', 'text-align',
    'text-overflow', 'white-space',
    'padding-top', 'padding-right', 'padding-bottom', 'padding-left',
    'margin-top', 'margin-right', 'margin-bottom', 'margin-left',
    'display', 'flex-direction', 'flex-wrap', 'align-items', 'justify-content',
    'gap', 'grid-template-columns', 'grid-template-rows',
    'position', 'overflow', 'z-index',
    'transition-property', 'transition-duration', 'transition-timing-function',
    'transition-delay', 'animation-name', 'animation-duration',
    'animation-timing-function', 'transform', 'transform-origin',
    'fill', 'stroke', 'stroke-width', 'stroke-dasharray', 'stroke-linecap',
];

// Values that mean "nothing set here". Dropping them cuts the dump by ~80%
// without losing a single real decision.
export const NOISE = [
    'none', 'normal', 'auto', '0px', 'rgba(0, 0, 0, 0)', 'static', 'visible',
    'start', '0s', 'ease', 'all', 'currentcolor', 'left', 'block', 'row',
    'nowrap', '400', 'inline', 'baseline', '0%', '1', 'butt', '1px',
    'rgb(0, 0, 0)', 'clip', 'stretch',
];

// SVG paint properties are meaningful on vectors and pure noise on every HTML
// element, where they report inherited defaults nobody set.
export const SVG_ONLY = ['fill', 'stroke', 'stroke-width', 'stroke-dasharray', 'stroke-linecap'];

/**
 * Dump one element's subtree: geometry, computed visual styles, and any SVG
 * verbatim with its styling inlined.
 *
 * Runs inside the page, so it must not close over anything — every constant
 * arrives as an argument. `rootSelector` is resolved fresh here rather than
 * passed in, because a handle and a selector can disagree.
 */
export function dumpSubtree({ rootSelector, props, noise, svgOnly, maxNodes, maxDepth }) {
    const NOISE = new Set(noise);
    const SVG_ONLY = new Set(svgOnly);
    // Must pierce shadow roots, because discovery does. A plain
    // document.querySelector silently loses every component that lives inside
    // a web component — they are found, tagged, screenshotted, and then fail
    // to dump, which looks like a selector typo rather than a scoping bug.
    const root = (function deepQuery(sel, node) {
        const hit = node.querySelector(sel);
        if (hit) return hit;
        for (const e of node.querySelectorAll('*'))
            if (e.shadowRoot) { const h = deepQuery(sel, e.shadowRoot); if (h) return h; }
        return null;
    })(rootSelector, document);
    if (!root) return { error: `dump root not found: ${rootSelector}` };
    const origin = root.getBoundingClientRect();
    let truncated = false;

    const label = e => {
        const cls = String(e.getAttribute?.('class') ?? '').trim().split(/\s+/).filter(Boolean);
        return e.tagName.toLowerCase()
            + (e.id ? '#' + e.id : '')
            + (cls.length ? '.' + cls.slice(0, 3).join('.') : '');
    };

    const nodes = [];
    const svgs = [];
    const assets = [];

    (function walk(e, depth, path) {
        if (depth > maxDepth) return;
        // A 24-row table with a detail grid per row runs to thousands of
        // near-identical nodes and a multi-megabyte dump, no more readable
        // than the screenshot was. Cap it — but say so, because a silent cap
        // reads as "captured everything".
        if (nodes.length >= maxNodes) { truncated = true; return; }
        const r = e.getBoundingClientRect();
        if (r.width < 1 && r.height < 1) return;

        const cs = getComputedStyle(e);
        const isSvg = e.namespaceURI === 'http://www.w3.org/2000/svg';
        const style = {};
        for (const p of props) {
            if (SVG_ONLY.has(p) && !isSvg) continue;
            const v = cs.getPropertyValue(p).trim();
            if (!v || NOISE.has(v) || NOISE.has(v.toLowerCase())) continue;
            // A border colour or style with no border is a default nobody chose.
            const side = /^border-(\w+)-(color|style)$/.exec(p);
            if (side && parseFloat(cs.getPropertyValue(`border-${side[1]}-width`)) === 0) continue;
            style[p] = v;
        }
        const own = [...e.childNodes]
            .filter(n => n.nodeType === 3).map(n => n.textContent.trim())
            .filter(Boolean).join(' ');

        if (e.tagName === 'IMG' && e.currentSrc)
            assets.push({ src: e.currentSrc, alt: e.alt || null,
                          w: Math.round(r.width), h: Math.round(r.height) });

        nodes.push({
            path, tag: label(e), depth,
            box: {
                x: +(r.x - origin.x).toFixed(1), y: +(r.y - origin.y).toFixed(1),
                w: +r.width.toFixed(1), h: +r.height.toFixed(1),
            },
            ...(own ? { text: own.slice(0, 120) } : {}),
            style,
        });

        if (e.tagName.toLowerCase() === 'svg') { svgs.push(e); return; }
        [...e.children].forEach((c, i) => walk(c, depth + 1, `${path}/${i}`));
        if (e.shadowRoot) [...e.shadowRoot.children].forEach((c, i) => walk(c, depth + 1, `${path}/s${i}`));
    })(root, 0, '');

    // Inline the styling that lives in stylesheets so each saved SVG renders
    // standalone in any viewer.
    const SVG_PAINT = ['fill', 'stroke', 'stroke-width', 'stroke-dasharray',
        'stroke-linecap', 'stroke-opacity', 'fill-opacity', 'opacity',
        'font-family', 'font-size', 'font-weight'];
    // Only elements that actually paint. Inlining onto <stop>, <mask> and
    // gradient defs is pure noise — it buries the stop list, which is the
    // single most valuable thing in a chart SVG.
    const PAINTS = new Set(['svg', 'g', 'path', 'rect', 'circle', 'ellipse',
        'line', 'polyline', 'polygon', 'text', 'tspan', 'use']);
    const svgOut = svgs.map((s, i) => {
        const clone = s.cloneNode(true);
        const src = [s, ...s.querySelectorAll('*')];
        const dst = [clone, ...clone.querySelectorAll('*')];
        src.forEach((o, j) => {
            if (!PAINTS.has(o.tagName.toLowerCase())) return;
            if (o.closest('defs, mask, clipPath, filter')) return;
            const cs = getComputedStyle(o);
            for (const p of SVG_PAINT) {
                const v = cs.getPropertyValue(p).trim();
                if (v && !NOISE.has(v)) dst[j].setAttribute(p, v);
            }
        });
        const r = s.getBoundingClientRect();
        return {
            index: i, id: s.id || null,
            label: s.getAttribute('aria-label') || null,
            w: Math.round(r.width), h: Math.round(r.height),
            markup: clone.outerHTML,
        };
    });

    return {
        component: { w: +origin.width.toFixed(1), h: +origin.height.toFixed(1) },
        nodeCount: nodes.length, truncated, nodes, svgs: svgOut, assets,
        fonts: [...new Set(nodes.map(n => n.style['font-family']).filter(Boolean))],
    };
}
