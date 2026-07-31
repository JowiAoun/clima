// SPDX-FileCopyrightText: 2026 Jowi Aoun
// SPDX-License-Identifier: GPL-3.0-or-later
//
// refcap capture — one named component, optionally in an interaction state.
//
// `crawl.mjs` captures a whole page at rest and needs no selectors. This mode
// exists for what a crawl cannot reach: a component after a click or a hover —
// another metric tab selected, a chart scrubbed to show its readout. Those are
// declared per target in targets.json.
//
// The browser setup and the DOM dump are shared with the crawl (lib.mjs), so
// the same component captured either way comes out described the same.
//
// Usage:
//   node capture.mjs --list
//   node capture.mjs hourly-chart
//   node capture.mjs hourly-chart --city london --state precipitation

import { chromium } from 'playwright';
import { readFile, writeFile, mkdir, rm, rename } from 'node:fs/promises';
import { dirname, join, resolve } from 'node:path';
import { fileURLToPath } from 'node:url';
import {
    CITIES, locParam, newContext, QUIET_CSS,
    VISUAL_PROPS, NOISE, SVG_ONLY, dumpSubtree,
    bestScale, fits, patchesFor, MAX_PATCHES,
} from './lib.mjs';

const HERE = dirname(fileURLToPath(import.meta.url));
const REPO = resolve(HERE, '../..');

function parseArgs(argv) {
    const out = { _: [], city: 'seattle', market: 'en-us', units: 'C', theme: 'dark',
                  width: '1600', height: '1100' };
    for (let i = 0; i < argv.length; i++) {
        const a = argv[i];
        if (a === '--list') { out.list = true; continue; }
        if (a.startsWith('--')) {
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

// Pad may be a single number or per-side — per-side is how you show a seam
// between two components without capturing both in full.
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
 * Returns a document-relative box, so a clip stays correct regardless of scroll.
 */
async function openAt(browser, { dpr, url, city, args, target }) {
    const ctx = await newContext(browser, {
        dpr, theme: args.theme, tz: city.tz, width: args.width, height: args.height,
    });
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
        console.log('for a whole page with no selectors, use: node crawl.mjs');
        return;
    }

    const name = args._[0];
    const target = targets[name];
    if (!target) throw new Error(`unknown target "${name}" — run --list`);

    const city = CITIES[args.city];
    if (!city) throw new Error(`unknown city "${args.city}" — have: ${Object.keys(CITIES).join(', ')}`);
    const url = target.url
        .replace('{loc}', encodeURIComponent(locParam(args.city, args.market)))
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

        const dump = await page.evaluate(dumpSubtree, {
            rootSelector: '[data-refcap]', props: VISUAL_PROPS, noise: NOISE, svgOnly: SVG_ONLY,
            maxNodes: target.maxNodes ?? 1200, maxDepth: target.maxDepth ?? 12,
        });
        if (dump.error) throw new Error(dump.error);

        // The site's own design tokens. The crawl writes these once per page;
        // a single-target capture carries its own copy so it stands alone.
        const tokens = await page.evaluate(() => {
            const rs = getComputedStyle(document.documentElement);
            const out = {};
            for (let i = 0; i < rs.length; i++) {
                const p = rs[i];
                if (p.startsWith('--')) out[p] = rs.getPropertyValue(p).trim();
            }
            return out;
        });

        await writeFile(join(tmpDir, 'meta.json'), JSON.stringify({
            capturedFrom: url, target: name, selector: target.selector,
            city: args.city, market: args.market, units: args.units,
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
        await writeFile(join(tmpDir, 'tokens.json'), JSON.stringify(tokens, null, 2));
        if (dump.assets.length)
            await writeFile(join(tmpDir, 'assets.json'), JSON.stringify(dump.assets, null, 2));
        for (const s of dump.svgs) {
            const fname = `svg-${s.index}${s.id ? '-' + s.id : ''}.svg`.replace(/[^\w.-]/g, '_');
            await writeFile(join(tmpDir, fname), s.markup);
        }

        await ctx.close();
        await rm(outDir, { recursive: true, force: true });
        await rename(tmpDir, outDir);

        console.log(`  ${dump.nodeCount} nodes, ${dump.svgs.length} svg, ${Object.keys(tokens).length} css vars`);
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
