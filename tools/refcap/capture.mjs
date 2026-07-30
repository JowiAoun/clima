// SPDX-License-Identifier: GPL-3.0-or-later
//
// refcap — capture a reference UI component at a fidelity that is actually
// useful to work from.
//
// The point is *not* to take screenshots. A screenshot answers "roughly what
// does this look like"; replication needs "what exactly is this", and the
// browser already knows. So every capture emits four things side by side:
//
//   shot.png      the component, clipped, at the highest DPR that survives
//                 the model's vision pipeline without being downscaled
//   geometry.json every box in the subtree, in CSS px, relative to the
//                 component origin — sizes and spacing without eyeballing
//   styles.json   computed styles, so colours/radii/fonts/easings are read
//                 rather than guessed
//   *.svg         any vector in the subtree, verbatim, with its styling
//                 inlined so the file stands alone
//
// Captures are reference material from a third-party site: they stay out of
// git (see .gitignore). What gets committed is the distilled measurements.
//
// Usage:
//   node capture.mjs --list
//   node capture.mjs hourly-chart
//   node capture.mjs hourly-chart --city london --state precipitation

import { chromium } from 'playwright';
import { readFile, writeFile, mkdir, rm, rename } from 'node:fs/promises';
import { dirname, join, resolve } from 'node:path';
import { fileURLToPath } from 'node:url';

const HERE = dirname(fileURLToPath(import.meta.url));
const REPO = resolve(HERE, '../..');

// ---------------------------------------------------------------------------
// Vision budget
//
// Claude 4.7 and later read images in 28x28 patches, up to 4784 patches and a
// 2576 px long edge; anything larger is downscaled before the model ever sees
// it. Capturing above that ceiling costs time and returns nothing. Capturing
// below it throws away detail that was free.
//
// So rather than pick a device pixel ratio by feel, solve for the largest one
// that still lands inside both limits.
const MAX_LONG_EDGE = 2576;
const MAX_PATCHES = 4784;
const DPR_LADDER = [4, 3, 2.5, 2, 1.5, 1];

const patchesFor = (w, h) => Math.ceil(w / 28) * Math.ceil(h / 28);
const fits = (w, h) => Math.max(w, h) <= MAX_LONG_EDGE && patchesFor(w, h) <= MAX_PATCHES;

function bestScale(cssW, cssH) {
    for (const dpr of DPR_LADDER) if (fits(cssW * dpr, cssH * dpr)) return dpr;
    return 1;   // component is bigger than the budget even at 1x; caller warns
}

// ---------------------------------------------------------------------------
// Determinism
//
// A capture that changes with the machine's location, unit preference, clock
// or ad auction is not a reference — two runs would disagree for reasons that
// have nothing to do with the design. Pin every one of those.
const CITIES = {
    seattle: { l: 'Seattle', r: 'Washington', r2: 'King', c: 'United States', i: 'US', x: '-122.33207', y: '47.60621', tz: 'America/Los_Angeles' },
    london: { l: 'London', r: 'England', r2: 'Greater London', c: 'United Kingdom', i: 'GB', x: '-0.12574', y: '51.50853', tz: 'Europe/London' },
    phoenix: { l: 'Phoenix', r: 'Arizona', r2: 'Maricopa', c: 'United States', i: 'US', x: '-112.07404', y: '33.44838', tz: 'America/Phoenix' },
    reykjavik: { l: 'Reykjavik', r: 'Capital Region', r2: 'Reykjavik', c: 'Iceland', i: 'IS', x: '-21.89541', y: '64.13548', tz: 'Atlantic/Reykjavik' },
    singapore: { l: 'Singapore', r: 'Singapore', r2: 'Singapore', c: 'Singapore', i: 'SG', x: '103.85007', y: '1.28967', tz: 'Asia/Singapore' },
};

function locParam(city) {
    const c = CITIES[city];
    if (!c) throw new Error(`unknown city "${city}" — have: ${Object.keys(CITIES).join(', ')}`);
    const { tz, ...loc } = c;
    return Buffer.from(JSON.stringify({ ...loc, g: 'en-us' })).toString('base64');
}

// Ads are most of the page weight, they reflow the layout as they settle, and
// they are never what we came for.
const JUNK = [
    'doubleclick.net', 'googlesyndication.com', 'googletagmanager.com',
    'google-analytics.com', 'adnxs.com', 'amazon-adsystem.com', 'criteo',
    'taboola', 'outbrain', 'scorecardresearch', 'bing.com/api/ads',
    'c.msn.com/c.gif', 'browser.events.data.msn.com',
];

// Anything that floats over the component and would land in the clip.
const QUIET_CSS = `
    [class*="ad-"],[id*="ads-"],display-ads,iframe,[class*="cookie"],
    [class*="Toast"],[class*="toast"],[class*="tooltip-callout"] { display: none !important; }
    *,*::before,*::after { animation-play-state: paused !important; }`;

// ---------------------------------------------------------------------------
// What is worth recording about an element.
//
// A full computed-style dump is ~340 properties per node and unreadable. These
// are the ones a reimplementation actually has to match.
const VISUAL_PROPS = [
    'color', 'background-color', 'background-image', 'opacity',
    'border-top-width', 'border-right-width', 'border-bottom-width', 'border-left-width',
    'border-top-color', 'border-right-color', 'border-bottom-color', 'border-left-color',
    'border-top-left-radius', 'border-top-right-radius',
    'border-bottom-right-radius', 'border-bottom-left-radius',
    'box-shadow', 'filter', 'backdrop-filter', 'mix-blend-mode',
    'font-family', 'font-size', 'font-weight', 'font-style',
    'letter-spacing', 'line-height', 'text-transform', 'text-align',
    'padding-top', 'padding-right', 'padding-bottom', 'padding-left',
    'margin-top', 'margin-right', 'margin-bottom', 'margin-left',
    'display', 'flex-direction', 'align-items', 'justify-content', 'gap',
    'grid-template-columns', 'position', 'overflow',
    'transition-property', 'transition-duration', 'transition-timing-function',
    'transition-delay', 'animation-name', 'animation-duration',
    'animation-timing-function', 'transform', 'transform-origin',
    'fill', 'stroke', 'stroke-width', 'stroke-dasharray', 'stroke-linecap',
];

// Values that mean "nothing set here". Dropping them cuts the dump by ~80%
// without losing a single real decision.
const NOISE = [
    'none', 'normal', 'auto', '0px', 'rgba(0, 0, 0, 0)', 'static', 'visible',
    'start', '0s', 'ease', 'all', 'currentcolor', 'left', 'block', 'row',
    'nowrap', '400', 'inline', 'baseline', '0%', '1', 'butt', '1px',
    'rgb(0, 0, 0)',
];

// SVG paint properties are meaningful on vectors and pure noise on every HTML
// element, where they report inherited defaults nobody set.
const SVG_ONLY = new Set(['fill', 'stroke', 'stroke-width', 'stroke-dasharray', 'stroke-linecap']);

// ---------------------------------------------------------------------------

function parseArgs(argv) {
    const out = { _: [], city: 'seattle', units: 'C', theme: 'dark', width: '1600', height: '1100' };
    for (let i = 0; i < argv.length; i++) {
        const a = argv[i];
        if (a === '--list') out.list = true;
        else if (a.startsWith('--')) {
            const v = argv[i + 1];
            if (v === undefined || v.startsWith('--')) throw new Error(`${a} needs a value`);
            out[a.slice(2)] = v; i++;
        } else out._.push(a);
    }
    for (const k of ['width', 'height']) {
        const n = Number(out[k]);
        if (!Number.isFinite(n) || n < 320) throw new Error(`--${k} must be a number >= 320`);
        out[k] = n;
    }
    if (!['dark', 'light'].includes(out.theme)) throw new Error('--theme must be dark or light');
    out.units = String(out.units).toUpperCase();
    if (!['C', 'F'].includes(out.units)) throw new Error('--units must be C or F');
    return out;
}

// Resolve pad, which may be a number or per-side.
function padOf(target) {
    const p = target.pad ?? 0;
    if (typeof p === 'number') return { top: p, right: p, bottom: p, left: p };
    return { top: p.top ?? 0, right: p.right ?? 0, bottom: p.bottom ?? 0, left: p.left ?? 0 };
}

/**
 * Load the page at a given device pixel ratio, settle it, apply the requested
 * state, and tag the target element.
 *
 * Both passes go through here on purpose. When this was two copy-pasted
 * blocks, the measuring pass and the rendering pass could drift apart — and a
 * difference between them is invisible in the output but corrupts it, because
 * the screenshot and the JSON would then describe different renders.
 *
 * Returns { ctx, page, box } where box is document-relative (not viewport-
 * relative), so a clip stays correct regardless of scroll position.
 */
async function openAt(browser, { dpr, url, city, args, target }) {
    const ctx = await browser.newContext({
        viewport: { width: args.width, height: args.height },
        deviceScaleFactor: dpr,
        colorScheme: args.theme,
        locale: 'en-US',
        timezoneId: city.tz,
        userAgent: 'Mozilla/5.0 (X11; Linux x86_64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/151.0.0.0 Safari/537.36',
    });
    await ctx.route('**/*', route =>
        JUNK.some(j => route.request().url().includes(j)) ? route.abort() : route.continue());

    const page = await ctx.newPage();
    await page.goto(url, { waitUntil: 'domcontentloaded', timeout: 60000 });
    await page.waitForSelector(target.selector, { timeout: 45000 });
    await page.waitForTimeout(target.settleMs ?? 6000);
    await page.addStyleTag({ content: QUIET_CSS });
    await page.waitForTimeout(400);

    if (args.state) {
        const step = target.states?.[args.state];
        if (!step) throw new Error(
            `target has no state "${args.state}" — have: ${Object.keys(target.states ?? {}).join(', ') || '(none)'}`);
        if (step.click) await page.locator(step.click).first().click({ timeout: 15000 });
        if (step.hover) await page.locator(step.hover).first().hover({ timeout: 15000 });
        await page.waitForTimeout(step.settleMs ?? 1500);
    }

    // Tag the element in the page, and take every later measurement from that
    // tag. Using a Playwright locator for the screenshot and a separate
    // querySelector for the dump can resolve to *different* nodes when a
    // selector matches more than once — the image and the JSON would then
    // disagree with no sign that anything is wrong.
    const box = await page.evaluate((selector) => {
        // Parts of the page are web components; a flat querySelector walks
        // straight past whole subtrees.
        function deepQuery(sel, root = document) {
            const hit = root.querySelector(sel);
            if (hit) return hit;
            for (const e of root.querySelectorAll('*'))
                if (e.shadowRoot) { const h = deepQuery(sel, e.shadowRoot); if (h) return h; }
            return null;
        }
        document.querySelectorAll('[data-refcap]').forEach(e => e.removeAttribute('data-refcap'));
        const el = deepQuery(selector);
        if (!el) return null;
        el.setAttribute('data-refcap', '1');
        el.scrollIntoView({ block: 'center', inline: 'nearest' });
        const r = el.getBoundingClientRect();
        return { x: r.x + window.scrollX, y: r.y + window.scrollY, width: r.width, height: r.height,
                 docW: document.documentElement.scrollWidth,
                 docH: document.documentElement.scrollHeight };
    }, target.selector);

    if (!box) throw new Error(`selector matched nothing: ${target.selector}`);
    if (box.width < 1 || box.height < 1)
        throw new Error(`selector matched an element with no box: ${target.selector}`);
    await page.waitForTimeout(500);   // let the scroll settle
    return { ctx, page, box };
}

function clipFor(box, pad) {
    const x = Math.max(0, box.x - pad.left);
    const y = Math.max(0, box.y - pad.top);
    return {
        x, y,
        width: Math.min(box.width + pad.left + pad.right, box.docW - x),
        height: Math.min(box.height + pad.top + pad.bottom, box.docH - y),
    };
}

async function main() {
    const args = parseArgs(process.argv.slice(2));
    const targets = JSON.parse(await readFile(join(HERE, 'targets.json'), 'utf8'));

    if (args.list || args._.length === 0) {
        console.log('targets:');
        for (const [name, t] of Object.entries(targets)) {
            const states = Object.keys(t.states ?? {});
            console.log(`  ${name.padEnd(16)} ${t.description}`);
            if (states.length) console.log(`  ${' '.repeat(16)}   states: ${states.join(', ')}`);
        }
        console.log(`\ncities: ${Object.keys(CITIES).join(', ')}`);
        console.log('\nusage: node capture.mjs <target> [--city london] [--units F] [--theme light] [--state <name>]');
        return;
    }

    const name = args._[0];
    const target = targets[name];
    if (!target) throw new Error(`unknown target "${name}" — run --list`);

    const city = CITIES[args.city];
    if (!city) throw new Error(`unknown city "${args.city}" — have: ${Object.keys(CITIES).join(', ')}`);
    const url = target.url
        .replace('{loc}', encodeURIComponent(locParam(args.city)))
        .replace('{units}', args.units);
    const pad = padOf(target);

    // The state belongs in the path: without it a `--state uv` run silently
    // overwrites the default capture, and you lose the one you were comparing against.
    const slug = [name, args.city, args.theme, args.state].filter(Boolean).join('--');
    const outDir = join(REPO, 'reference', 'msn', slug);
    // Build beside the target and swap at the end, so a run that fails
    // halfway leaves the previous good capture intact.
    const tmpDir = outDir + '.partial';
    await rm(tmpDir, { recursive: true, force: true });
    await mkdir(tmpDir, { recursive: true });

    const browser = await chromium.launch({ headless: true });
    try {
        console.log(`→ ${url.slice(0, 100)}${url.length > 100 ? '…' : ''}`);

        // Pass 1 measures. Playwright fixes deviceScaleFactor per context, so
        // the scale cannot be chosen until the box is known.
        const first = await openAt(browser, { dpr: 1, url, city, args, target });
        const probe = clipFor(first.box, pad);
        await first.ctx.close();

        const dpr = bestScale(probe.width, probe.height);

        // Pass 2 renders and dumps. Everything recorded comes from this pass,
        // so the image and the JSON always describe the same render.
        const { ctx, page, box } = await openAt(browser, { dpr, url, city, args, target });
        const clip = clipFor(box, pad);

        const px = { w: Math.round(clip.width * dpr), h: Math.round(clip.height * dpr) };
        const patches = patchesFor(px.w, px.h);
        const downscaled = !fits(px.w, px.h);

        console.log(`  box ${Math.round(clip.width)}x${Math.round(clip.height)} css`
            + ` → dpr ${dpr} → ${px.w}x${px.h} px (${patches}/${MAX_PATCHES} patches`
            + `${downscaled ? ', WILL BE DOWNSCALED' : ', no downscale'})`);
        if (downscaled)
            console.warn(`  ! ${Math.round(clip.width)}x${Math.round(clip.height)} css exceeds the vision budget`
                + ` even at 1x. Capture sub-components separately, or narrow --width.`);

        // fullPage lets the clip reach past the viewport for tall components;
        // the box is already document-relative, so the two agree.
        await page.screenshot({ path: join(tmpDir, 'shot.png'), clip, fullPage: true });

        const dump = await page.evaluate(({ props, noise, svgOnly, maxNodes }) => {
            const NOISE = new Set(noise);
            const SVG_ONLY = new Set(svgOnly);
            const root = document.querySelector('[data-refcap]');
            if (!root) return { error: 'tag lost between screenshot and dump' };
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

            (function walk(e, depth, path) {
                if (depth > 12) return;
                // A 24-row table with a detail grid per row runs to thousands
                // of near-identical nodes and a multi-megabyte dump, which is
                // no more readable than the screenshot was. Cap it — but say
                // so, because a silent cap reads as "captured everything".
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
                    // A border colour with no border is a default nobody chose.
                    const side = /^border-(\w+)-color$/.exec(p);
                    if (side && parseFloat(cs.getPropertyValue(`border-${side[1]}-width`)) === 0) continue;
                    style[p] = v;
                }
                const own = [...e.childNodes]
                    .filter(n => n.nodeType === 3).map(n => n.textContent.trim())
                    .filter(Boolean).join(' ');

                nodes.push({
                    path, tag: label(e), depth,
                    box: {
                        x: +(r.x - origin.x).toFixed(1), y: +(r.y - origin.y).toFixed(1),
                        w: +r.width.toFixed(1), h: +r.height.toFixed(1),
                    },
                    ...(own ? { text: own.slice(0, 80) } : {}),
                    style,
                });

                if (e.tagName.toLowerCase() === 'svg') { svgs.push(e); return; }
                [...e.children].forEach((c, i) => walk(c, depth + 1, `${path}/${i}`));
                if (e.shadowRoot) [...e.shadowRoot.children].forEach((c, i) => walk(c, depth + 1, `${path}/s${i}`));
            })(root, 0, '');

            // Inline the styling that lives in stylesheets so each saved SVG
            // renders standalone in any viewer.
            const SVG_PAINT = ['fill', 'stroke', 'stroke-width', 'stroke-dasharray',
                'stroke-linecap', 'stroke-opacity', 'fill-opacity', 'opacity',
                'font-family', 'font-size', 'font-weight'];
            // Only elements that actually paint. Inlining onto <stop>, <mask>
            // and gradient defs is pure noise — it buries the stop list, which
            // is the single most valuable thing in a chart SVG.
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
                    index: i,
                    id: s.id || null,
                    label: s.getAttribute('aria-label') || null,
                    w: Math.round(r.width), h: Math.round(r.height),
                    markup: clone.outerHTML,
                };
            });

            // The site's own design tokens, if it has any. Cheaper and more
            // exact than reading colours back off a screenshot.
            const rs = getComputedStyle(document.documentElement);
            const tokens = {};
            for (let i = 0; i < rs.length; i++) {
                const p = rs[i];
                if (p.startsWith('--')) tokens[p] = rs.getPropertyValue(p).trim();
            }

            return {
                component: { w: +origin.width.toFixed(1), h: +origin.height.toFixed(1) },
                nodeCount: nodes.length, truncated, nodes, svgs: svgOut, tokens,
                fonts: [...new Set(nodes.map(n => n.style['font-family']).filter(Boolean))],
            };
        }, { props: VISUAL_PROPS, noise: NOISE, svgOnly: [...SVG_ONLY],
             maxNodes: target.maxNodes ?? 1200 });

        if (dump.error) throw new Error(dump.error);

        await writeFile(join(tmpDir, 'meta.json'), JSON.stringify({
            capturedFrom: url, target: name, city: args.city, units: args.units,
            theme: args.theme, viewport: { w: args.width, h: args.height },
            deviceScaleFactor: dpr, imagePx: px, visualTokens: patches, downscaled,
            componentCss: dump.component, state: args.state ?? null,
            nodeCount: dump.nodeCount, nodesTruncated: dump.truncated,
            note: 'Third-party reference capture. Not redistributable; gitignored. Distil measurements, do not copy assets.',
        }, null, 2));
        await writeFile(join(tmpDir, 'geometry.json'), JSON.stringify(
            dump.nodes.map(({ style, ...rest }) => rest), null, 2));
        await writeFile(join(tmpDir, 'styles.json'), JSON.stringify(
            dump.nodes.map(({ path, tag, text, style }) => ({ path, tag, ...(text ? { text } : {}), style })), null, 2));
        await writeFile(join(tmpDir, 'tokens.json'), JSON.stringify(dump.tokens, null, 2));
        for (const s of dump.svgs) {
            const fname = `svg-${s.index}${s.id ? '-' + s.id : ''}.svg`.replace(/[^\w.-]/g, '_');
            await writeFile(join(tmpDir, fname), s.markup);
        }

        await ctx.close();
        await rm(outDir, { recursive: true, force: true });
        await rename(tmpDir, outDir);

        console.log(`  ${dump.nodeCount} nodes, ${dump.svgs.length} svg, ${Object.keys(dump.tokens).length} css vars`);
        if (dump.truncated)
            console.warn(`  ! node dump capped at ${dump.nodeCount}; the tail of this subtree is missing.`
                + ` Raise "maxNodes" on this target, or capture a sub-component.`);
        if (dump.svgs.length)
            console.log(`  svg: ${dump.svgs.map(s => s.label || s.id || `#${s.index}`).join(' | ')}`);
        console.log(`  fonts: ${dump.fonts.slice(0, 3).join(' | ') || '(inherited)'}`);
        console.log(`✓ ${outDir.replace(REPO + '/', '')}`);
    } finally {
        await browser.close();
        await rm(tmpDir, { recursive: true, force: true });
    }
}

main().catch(e => { console.error('refcap failed:', e.message); process.exit(1); });
