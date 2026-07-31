// SPDX-FileCopyrightText: 2026 Jowi Aoun
// SPDX-License-Identifier: GPL-3.0-or-later
//
// refcap crawl — capture a whole page as a set of per-component references.
//
// The single-target mode needs a hand-written selector per component, which
// does not scale to a page with forty of them and goes stale every time the
// site ships. This finds the components itself.
//
// The handle is that MSN builds with CSS modules, so every component block
// carries a class like `weatherDailyForecastContainer-DS-a1b2c3`: a readable
// base name, a marker, and a build hash. The base name is the component's real
// name as its own authors wrote it, which makes it a better label than anything
// we would invent, and a stable key across redeploys once the hash is dropped.
//
// Output, per page:
//
//   index.md                human-readable inventory, nested by containment
//   index.json              the same, for tooling
//   page/strips/NN.png      the page itself, one screenful per file
//   page/tokens.json        CSS custom properties (once, not per component)
//   page/palette.json       every colour actually chosen, with usage counts
//   page/typography.json    every distinct type style, with usage counts
//   page/assets.json        image and icon URLs — recorded, never downloaded
//   components/NN-name/     shot.png, meta.json, geometry.json, styles.json, svg-*.svg
//
// Usage:
//   node crawl.mjs                                   # en-ca forecast, Toronto
//   node crawl.mjs --url https://…/weather/hourlyforecast/
//   node crawl.mjs --city phoenix --market en-us --theme light

import { chromium } from 'playwright';
import { writeFile, mkdir, rm, rename } from 'node:fs/promises';
import { dirname, join, resolve } from 'node:path';
import { fileURLToPath } from 'node:url';
import {
    CITIES, locParam, newContext, primeLazyContent, QUIET_CSS,
    VISUAL_PROPS, NOISE, SVG_ONLY, dumpSubtree,
    fitToBudget,
} from './lib.mjs';

const HERE = dirname(fileURLToPath(import.meta.url));
const REPO = resolve(HERE, '../..');

const DEFAULTS = {
    url: 'https://www.msn.com/en-ca/weather/forecast/',
    city: 'toronto', market: 'en-ca', units: 'C', theme: 'dark',
    width: '1600', height: '1200', dpr: '3',
    minWidth: '160', minHeight: '48', maxHeight: '2400', maxNodes: '900', maxDepth: '10',
};

function parseArgs(argv) {
    const out = { ...DEFAULTS };
    for (let i = 0; i < argv.length; i++) {
        const a = argv[i];
        if (a === '--help') { out.help = true; continue; }
        if (!a.startsWith('--')) throw new Error(`unexpected argument "${a}"`);
        const v = argv[i + 1];
        if (v === undefined || v.startsWith('--')) throw new Error(`${a} needs a value`);
        out[a.slice(2)] = v; i++;
    }
    for (const k of ['width', 'height', 'minWidth', 'minHeight', 'maxHeight', 'maxNodes', 'maxDepth']) {
        const n = Number(out[k]);
        if (!Number.isFinite(n) || n <= 0) throw new Error(`--${k} must be a positive number`);
        out[k] = n;
    }
    out.dpr = Number(out.dpr);
    if (!(out.dpr >= 1 && out.dpr <= 4)) throw new Error('--dpr must be between 1 and 4');
    if (!['dark', 'light'].includes(out.theme)) throw new Error('--theme must be dark or light');
    out.units = String(out.units).toUpperCase();
    if (!['C', 'F'].includes(out.units)) throw new Error('--units must be C or F');
    if (!CITIES[out.city]) throw new Error(`unknown city "${out.city}" — have: ${Object.keys(CITIES).join(', ')}`);
    return out;
}

// A stable directory name from a component's own base name.
const slugify = s => s.replace(/([a-z0-9])([A-Z])/g, '$1-$2').toLowerCase()
    .replace(/[^a-z0-9]+/g, '-').replace(/^-|-$/g, '').slice(0, 48);

// ---------------------------------------------------------------------------
// Runs in the page. Finds every component-sized block that carries a CSS-module
// name, drops wrappers that share a box with something they contain, collapses
// repeated instances to one exemplar, and tags what survives.
function discoverComponents({ minWidth, minHeight, maxHeight }) {
    const found = [];
    const seenBox = new Set();

    function scan(root) {
        for (const e of root.querySelectorAll('*')) {
            if (e.shadowRoot) scan(e.shadowRoot);

            const cls = String(e.getAttribute?.('class') ?? '');
            const m = /(?:^|\s)([A-Za-z_][\w]*)-DS-/.exec(cls);
            // Custom elements are components by definition even without a
            // module class — MSN wraps a few modules in them.
            const isCustom = e.tagName.includes('-') && !e.tagName.startsWith('MSN-VERTICALS');
            if (!m && !isCustom) continue;

            const name = m ? m[1] : e.tagName.toLowerCase();
            // Ad slots and comment overlays are page furniture, not design.
            // Split on camel case rather than matching a substring: "Ads" is a
            // word inside "DisplayAdsBoard", but \bad matches nothing there,
            // and a loose /ad/ would eat "adaptive", "shadow" and "radar".
            const words = name.split(/(?=[A-Z])|[_-]/).map(w => w.toLowerCase());
            if (words.some(w => ['ad', 'ads', 'advert', 'advertisement'].includes(w))) continue;
            if (/^comment/i.test(name)) continue;

            // Hidden overlays report a plausible box but cannot be clipped out
            // of the render, so they fail at screenshot time with a confusing
            // "clipped area is outside the image".
            if (e.checkVisibility && !e.checkVisibility({ checkOpacity: true, checkVisibilityCSS: true })) continue;
            // Fixed elements sit outside the document flow, so a document-
            // relative clip does not address them.
            if (getComputedStyle(e).position === 'fixed') continue;

            const r = e.getBoundingClientRect();
            if (r.width < minWidth || r.height < minHeight || r.height > maxHeight) continue;
            if (r.width < 1 || r.height < 1) continue;

            // A component is usually wrapped in two or three divs with the
            // exact same box. Tree order means the outermost is seen first,
            // and it is the one worth keeping.
            const key = [r.x, r.y, r.width, r.height].map(Math.round).join(',');
            if (seenBox.has(key)) continue;
            seenBox.add(key);

            found.push({
                el: e,
                name,
                selector: m ? `[class*="${m[1]}-DS"]` : e.tagName.toLowerCase(),
                docX: r.x + window.scrollX, docY: r.y + window.scrollY,
                w: r.width, h: r.height,
            });
        }
    }
    scan(document);

    // Repeated instances — day cards, hourly rows — teach us nothing new after
    // the first. Keep one exemplar and record how many there were.
    const byName = new Map();
    for (const c of found) {
        const seen = byName.get(c.name);
        if (!seen) { byName.set(c.name, { ...c, instances: 1 }); continue; }
        seen.instances++;
        // Prefer the largest instance: an expanded row shows more than a collapsed one.
        if (c.w * c.h > seen.w * seen.h) Object.assign(seen, c, { instances: seen.instances });
    }

    const kept = [...byName.values()].sort((a, b) => a.docY - b.docY || a.docX - b.docX);

    // Containment, so the index can be read as a tree rather than a flat list.
    kept.forEach((c, i) => {
        c.index = i;
        c.parent = null;
        for (let j = 0; j < kept.length; j++) {
            if (i === j) continue;
            if (!kept[j].el.contains(c.el)) continue;
            // Nearest containing ancestor wins.
            if (c.parent === null || kept[c.parent].el.contains(kept[j].el)) c.parent = j;
        }
        c.el.setAttribute('data-refcap-i', String(i));
    });

    return kept.map(({ el, ...rest }) => rest);
}

// ---------------------------------------------------------------------------
// Runs in the page. Page-wide inventories: the raw material for a theme file.
function pageInventory() {
    const palette = new Map();
    const type = new Map();
    const assets = new Map();

    const bump = (map, key, extra) => {
        const hit = map.get(key);
        if (hit) { hit.count++; return hit; }
        const rec = { count: 1, ...extra };
        map.set(key, rec);
        return rec;
    };

    for (const e of document.querySelectorAll('*')) {
        const r = e.getBoundingClientRect();
        if (r.width < 2 || r.height < 2) continue;
        const cs = getComputedStyle(e);

        const parentCs = e.parentElement ? getComputedStyle(e.parentElement) : null;
        for (const prop of ['color', 'background-color', 'border-top-color']) {
            const v = cs.getPropertyValue(prop).trim();
            if (!v || v === 'rgba(0, 0, 0, 0)') continue;
            if (prop === 'border-top-color' && parseFloat(cs.borderTopWidth) === 0) continue;
            // `color` inherits, so every element reports one whether or not
            // anybody chose it. Counting all of them buries the palette under
            // the UA default: 700-odd "black" from elements that simply never
            // set a colour. Only a value that differs from the parent's was
            // decided *here*.
            if (prop === 'color') {
                if (parentCs && parentCs.color.trim() === v) continue;
                // …and only where it actually paints text.
                if (![...e.childNodes].some(n => n.nodeType === 3 && n.textContent.trim())) continue;
            }
            const rec = bump(palette, v, { value: v, uses: {} });
            rec.uses[prop] = (rec.uses[prop] ?? 0) + 1;
        }

        const bg = cs.backgroundImage;
        if (bg && bg !== 'none' && bg.includes('gradient'))
            bump(palette, bg, { value: bg, gradient: true, uses: {} });

        if ([...e.childNodes].some(n => n.nodeType === 3 && n.textContent.trim())) {
            const key = [cs.fontSize, cs.fontWeight, cs.lineHeight, cs.letterSpacing, cs.fontFamily].join('|');
            bump(type, key, {
                fontSize: cs.fontSize, fontWeight: cs.fontWeight,
                lineHeight: cs.lineHeight, letterSpacing: cs.letterSpacing,
                fontFamily: cs.fontFamily,
            });
        }

        if (e.tagName === 'IMG' && e.currentSrc)
            bump(assets, e.currentSrc, {
                src: e.currentSrc, alt: e.alt || null,
                w: Math.round(r.width), h: Math.round(r.height),
            });
    }

    const rs = getComputedStyle(document.documentElement);
    const tokens = {};
    for (let i = 0; i < rs.length; i++) {
        const p = rs[i];
        if (p.startsWith('--')) tokens[p] = rs.getPropertyValue(p).trim();
    }

    const body = getComputedStyle(document.body);
    return {
        tokens,
        palette: [...palette.values()].sort((a, b) => b.count - a.count),
        typography: [...type.values()].sort((a, b) =>
            parseFloat(b.fontSize) - parseFloat(a.fontSize) || b.count - a.count),
        assets: [...assets.values()].sort((a, b) => b.count - a.count),
        page: {
            title: document.title,
            background: body.backgroundColor,
            backgroundImage: body.backgroundImage,
            fontFamily: body.fontFamily,
            scrollHeight: document.documentElement.scrollHeight,
        },
    };
}

// ---------------------------------------------------------------------------

function renderIndex(meta, comps) {
    const L = [];
    L.push(`<!-- Generated by tools/refcap/crawl.mjs. Do not edit. -->`);
    L.push(`# ${meta.pageTitle}`, '');
    L.push(`\`${meta.url}\``, '');
    L.push(`${meta.city} · ${meta.market} · °${meta.units} · ${meta.theme} · `
        + `viewport ${meta.viewport.w}×${meta.viewport.h} · captured at ${meta.dpr}×`, '');
    L.push(`${comps.length} components over ${meta.pageHeight} px of page.`, '');
    L.push(`Page-wide inventories are in [\`page/\`](page/): design tokens, the full`
        + ` colour palette with usage counts, the type scale, and asset URLs.`
        + ` The page itself is in [\`page/strips/\`](page/strips/), one screenful per file —`
        + ` a single image of a ${meta.pageHeight} px page would have to be squeezed to`
        + ` ~340 px wide to fit, and would be legible nowhere.`, '');
    L.push('## Components', '');
    L.push('Nested by containment, in document order.', '');
    L.push('| | Component | Size | Instances | Nodes | SVG |');
    L.push('|---|---|---|---|---|---|');

    const childrenOf = new Map();
    for (const c of comps) {
        const k = c.parent ?? -1;
        if (!childrenOf.has(k)) childrenOf.set(k, []);
        childrenOf.get(k).push(c);
    }
    (function emit(parent, depth) {
        for (const c of childrenOf.get(parent) ?? []) {
            const indent = '&nbsp;'.repeat(depth * 4);
            L.push(`| \`${String(c.n).padStart(2, '0')}\` | ${indent}[${c.name}](components/${c.dir}/)`
                + ` | ${Math.round(c.w)}×${Math.round(c.h)} | ${c.instances > 1 ? c.instances : ''}`
                + ` | ${c.nodeCount ?? ''}${c.truncated ? ' *(capped)*' : ''} | ${c.svgCount || ''} |`);
            emit(c.index, depth + 1);
        }
    })(-1, 0);

    L.push('', '## Re-capturing one component', '');
    L.push('Each component directory records the selector it was found by. To pull a',
        'single component again — with a state applied, or a different city — add it',
        'to `targets.json` and use `capture.mjs`, which supports clicks and hovers.', '');
    L.push('## Provenance', '');
    L.push('Third-party reference material, captured for study. Layout, proportion and',
        'interaction are fair to learn from; icons, illustrations, fonts and markup are',
        'not ours to ship. This tree is gitignored — distil measurements into',
        '`docs/09-reference-capture.md` rather than committing captures.');
    return L.join('\n') + '\n';
}

async function main() {
    const args = parseArgs(process.argv.slice(2));
    if (args.help) {
        console.log('usage: node crawl.mjs [--url URL] [--city toronto] [--market en-ca]');
        console.log('                      [--units C] [--theme dark] [--dpr 3]');
        console.log('                      [--width 1600] [--height 1200]');
        console.log(`\ncities: ${Object.keys(CITIES).join(', ')}`);
        return;
    }

    const city = CITIES[args.city];
    const sep = args.url.includes('?') ? '&' : '?';
    const url = `${args.url}${sep}loc=${encodeURIComponent(locParam(args.city, args.market))}`
        + `&weadegreetype=${args.units}`;

    const pageSlug = (new URL(args.url).pathname.replace(/^\/|\/$/g, '').replace(/\//g, '-') || 'root');
    const outDir = join(REPO, 'reference', 'msn', `${pageSlug}--${args.city}--${args.theme}`);
    const tmpDir = outDir + '.partial';
    await rm(tmpDir, { recursive: true, force: true });
    await mkdir(join(tmpDir, 'page'), { recursive: true });
    await mkdir(join(tmpDir, 'components'), { recursive: true });

    const browser = await chromium.launch({ headless: true });
    try {
        console.log(`→ ${url.slice(0, 96)}…`);
        const ctx = await newContext(browser, {
            dpr: args.dpr, theme: args.theme, tz: city.tz,
            width: args.width, height: args.height,
        });
        const page = await ctx.newPage();
        await page.goto(url, { waitUntil: 'domcontentloaded', timeout: 90000 });
        await page.waitForTimeout(6000);
        await page.addStyleTag({ content: QUIET_CSS });

        console.log('  priming lazy content…');
        await primeLazyContent(page);
        await page.addStyleTag({ content: QUIET_CSS });   // re-assert over late arrivals
        await page.waitForTimeout(1200);

        // --- page-wide inventories ---------------------------------------
        const inv = await page.evaluate(pageInventory);
        await writeFile(join(tmpDir, 'page', 'tokens.json'), JSON.stringify(inv.tokens, null, 2));
        await writeFile(join(tmpDir, 'page', 'palette.json'), JSON.stringify(inv.palette, null, 2));
        await writeFile(join(tmpDir, 'page', 'typography.json'), JSON.stringify(inv.typography, null, 2));
        await writeFile(join(tmpDir, 'page', 'assets.json'), JSON.stringify(inv.assets, null, 2));
        await writeFile(join(tmpDir, 'page', 'page.json'), JSON.stringify(inv.page, null, 2));
        console.log(`  page: ${Object.keys(inv.tokens).length} tokens, ${inv.palette.length} colours,`
            + ` ${inv.typography.length} type styles, ${inv.assets.length} assets`);

        // --- discovery ----------------------------------------------------
        const comps = await page.evaluate(discoverComponents, {
            minWidth: args.minWidth, minHeight: args.minHeight, maxHeight: args.maxHeight,
        });
        console.log(`  found ${comps.length} components`);

        // --- whole page, for context --------------------------------------
        //
        // Not as one image: a 12 000 px page squeezed under the 2576 px long
        // edge comes out ~340 px wide and legible nowhere. Slice it into
        // screenfuls instead, each of which fits the budget on its own.
        await mkdir(join(tmpDir, 'page', 'strips'), { recursive: true });
        const strips = [];
        for (let y = 0, i = 1; y < inv.page.scrollHeight; y += args.height, i++) {
            const h = Math.min(args.height, inv.page.scrollHeight - y);
            if (h < 40) break;
            const f = join(tmpDir, 'page', 'strips', `${String(i).padStart(2, '0')}.png`);
            await page.screenshot({ path: f, clip: { x: 0, y, width: args.width, height: h }, fullPage: true });
            const px = await fitToBudget(f, Math.round(args.width * args.dpr), Math.round(h * args.dpr));
            strips.push({ file: `page/strips/${String(i).padStart(2, '0')}.png`, y, h, px });
        }
        console.log(`  page: ${strips.length} strips`);

        // --- per component -------------------------------------------------
        const usedDirs = new Set();
        const records = [];
        for (const c of comps) {
            let dir = slugify(c.name) || 'component';
            let n = 2;
            while (usedDirs.has(dir)) dir = `${slugify(c.name)}-${n++}`;
            usedDirs.add(dir);
            const num = String(c.index + 1).padStart(2, '0');
            const cDir = join(tmpDir, 'components', `${num}-${dir}`);
            await mkdir(cDir, { recursive: true });

            const sel = `[data-refcap-i="${c.index}"]`;

            // React re-renders lazy modules while the crawl is running, which
            // replaces the tagged node and loses the attribute. Re-establish
            // it from the component's own selector and recorded box before
            // relying on it — otherwise the component is found and
            // screenshotted, then fails to dump, which reads like a bad
            // selector rather than a node swapped out underneath us.
            await page.evaluate(({ tag, selector, box, index }) => {
                if (document.querySelector(tag)) return true;
                const all = [];
                (function scan(root) {
                    root.querySelectorAll(selector).forEach(e => all.push(e));
                    root.querySelectorAll('*').forEach(e => { if (e.shadowRoot) scan(e.shadowRoot); });
                })(document);
                if (!all.length) return false;
                // Nearest by geometry: a re-render keeps the layout, so the
                // box identifies the same component even when the node differs.
                let best = null, bestD = Infinity;
                for (const e of all) {
                    const r = e.getBoundingClientRect();
                    const d = Math.abs(r.x + window.scrollX - box.x) + Math.abs(r.y + window.scrollY - box.y)
                        + Math.abs(r.width - box.w) + Math.abs(r.height - box.h);
                    if (d < bestD) { bestD = d; best = e; }
                }
                if (!best || bestD > 40) return false;
                best.setAttribute('data-refcap-i', String(index));
                return true;
            }, { tag: sel, selector: c.selector, index: c.index,
                 box: { x: c.docX, y: c.docY, w: c.w, h: c.h } });

            const clip = { x: c.docX, y: c.docY, width: c.w, height: c.h };
            const png = join(cDir, 'shot.png');
            let shot = null;
            try {
                await page.screenshot({ path: png, clip, fullPage: true });
                shot = await fitToBudget(png, Math.round(c.w * args.dpr), Math.round(c.h * args.dpr));
            } catch (e) {
                console.warn(`  ! ${c.name}: screenshot failed (${e.message.split('\n')[0]})`);
            }

            const dump = await page.evaluate(dumpSubtree, {
                rootSelector: sel, props: VISUAL_PROPS, noise: NOISE, svgOnly: SVG_ONLY,
                maxNodes: args.maxNodes, maxDepth: args.maxDepth,
            });
            if (dump.error) { console.warn(`  ! ${c.name}: ${dump.error}`); continue; }

            await writeFile(join(cDir, 'meta.json'), JSON.stringify({
                name: c.name, index: c.index, selector: c.selector,
                instances: c.instances, parent: c.parent,
                pagePosition: { x: Math.round(c.docX), y: Math.round(c.docY) },
                cssSize: { w: +c.w.toFixed(1), h: +c.h.toFixed(1) },
                renderedAt: args.dpr, imagePx: shot, nodeCount: dump.nodeCount,
                nodesTruncated: dump.truncated, fonts: dump.fonts,
                capturedFrom: url,
                note: 'Third-party reference capture. Not redistributable; gitignored.',
            }, null, 2));
            await writeFile(join(cDir, 'geometry.json'), JSON.stringify(
                dump.nodes.map(({ style, ...rest }) => rest), null, 2));
            await writeFile(join(cDir, 'styles.json'), JSON.stringify(
                dump.nodes.map(({ path, tag, text, style }) =>
                    ({ path, tag, ...(text ? { text } : {}), style })), null, 2));
            if (dump.assets.length)
                await writeFile(join(cDir, 'assets.json'), JSON.stringify(dump.assets, null, 2));
            for (const s of dump.svgs) {
                const fname = `svg-${s.index}${s.id ? '-' + s.id : ''}.svg`.replace(/[^\w.-]/g, '_');
                await writeFile(join(cDir, fname), s.markup);
            }

            records.push({ ...c, n: c.index + 1, dir: `${num}-${dir}`,
                           nodeCount: dump.nodeCount, truncated: dump.truncated,
                           svgCount: dump.svgs.length });
            process.stdout.write(`\r  captured ${records.length}/${comps.length}   `);
        }
        process.stdout.write('\n');

        const meta = {
            url, pageTitle: inv.page.title, city: args.city, market: args.market,
            units: args.units, theme: args.theme, dpr: args.dpr,
            viewport: { w: args.width, h: args.height }, pageHeight: inv.page.scrollHeight,
            componentCount: records.length,
        };
        await writeFile(join(tmpDir, "index.json"),
            JSON.stringify({ meta, components: records }, null, 2));
        await writeFile(join(tmpDir, 'index.md'), renderIndex(meta, records));

        await ctx.close();
        await rm(outDir, { recursive: true, force: true });
        await rename(tmpDir, outDir);

        const capped = records.filter(r => r.truncated).length;
        if (capped) console.warn(`  ! ${capped} component(s) hit the node cap;`
            + ` raise --maxNodes if you need their full subtree`);
        console.log(`✓ ${outDir.replace(REPO + '/', '')}`);
        console.log(`  start at index.md`);
    } finally {
        await browser.close();
        await rm(tmpDir, { recursive: true, force: true });
    }
}

main().catch(e => { console.error('refcap crawl failed:', e.message); process.exit(1); });
