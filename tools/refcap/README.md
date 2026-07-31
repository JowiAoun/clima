<!-- SPDX-FileCopyrightText: 2026 Jowi Aoun -->
<!-- SPDX-License-Identifier: CC-BY-SA-4.0 -->

# refcap — reference capture

Captures a reference UI at a fidelity worth working from. Two modes:

```sh
npm install                       # playwright + chromium, once

node crawl.mjs                    # a whole page, split into per-component references
node capture.mjs hourly-chart     # one named component, with interaction states
```

Output lands in `reference/msn/…`, which is gitignored. See
[docs/09-reference-capture.md](../../docs/09-reference-capture.md) for why, and
for what to do with a capture once you have one.

## Why not just take a screenshot

A screenshot answers *roughly what does this look like*. Replication needs
*what exactly is this*, and the browser already knows — it computed every
number to lay the thing out. So each component capture emits:

| File | What it settles |
|---|---|
| `shot.png` | Appearance, clipped to the component, at a scale chosen to survive the vision pipeline un-downscaled |
| `geometry.json` | Every box in the subtree, CSS px, relative to the component. Sizes and spacing without measuring pixels |
| `styles.json` | Computed styles — colours, radii, fonts, shadows, transition easings — read rather than guessed |
| `svg-*.svg` | Any vector, verbatim, styling inlined so it stands alone |
| `assets.json` | Image and icon URLs, recorded — never downloaded |
| `meta.json` | Provenance, selector to re-capture by, instance count, node count |

The SVG is usually the jackpot. A chart's `<linearGradient>` stop list is the
designer's colour decision *as a list of numbers*, and its `x1/x2/y1/y2` say
which axis the colour is keyed to — a design decision you would otherwise have
to infer, and can easily infer wrong.

## `crawl.mjs` — a whole page

```sh
node crawl.mjs                                     # en-ca forecast, Toronto, dark, °C
node crawl.mjs --url https://…/weather/hourlyforecast/
node crawl.mjs --city phoenix --market en-us --theme light --dpr 4
```

The page is walked once and every component found, named, and captured on its
own. The handle that makes this possible is that MSN builds with CSS modules,
so each block carries a class like `weatherDailyForecastContainer-DS-a1b2c3`:
a readable base name, a marker, and a build hash. The base name is the
component's real name as its own authors wrote it — a better label than
anything we would invent, and stable across redeploys once the hash is dropped.

Discovery drops what is not design: wrappers sharing a box with what they
contain (outermost wins), repeated instances beyond the first exemplar (the
count is recorded instead), ad slots, comment overlays, and anything invisible
or `position: fixed`.

```
index.md                 inventory, nested by containment, in document order
index.json               the same, for tooling
page/tokens.json         CSS custom properties — once, not per component
page/palette.json        every colour actually chosen, with usage counts
page/typography.json     every distinct type style, with usage counts
page/assets.json         image and icon URLs
page/strips/NN.png       the page itself, one screenful per file
components/NN-name/      one directory per component
```

**Start at `index.md`.** It is the map: what exists, how big, how deeply nested,
how many instances, and which components carry vectors.

The page is delivered as strips rather than one tall image on purpose. A
12 000 px page squeezed under the 2576 px long-edge limit comes out about
340 px wide and is legible nowhere; a screenful at a time stays readable.

## `capture.mjs` — one component, with states

The crawl captures a page at rest. When you need a component in a *state* —
another metric tab selected, a hover readout showing — use the named-target
mode, where `targets.json` can drive clicks and hovers first:

```sh
node capture.mjs --list
node capture.mjs hourly-chart --state uv --city phoenix
```

MSN's class hashes change when they redeploy, so **always prefix-match** —
`[class*="hourlyChart-DS"]`, never the full name. If a selector stops matching,
that is the first thing to check. `crawl.mjs` records a usable selector in each
component's `meta.json`.

## The scale arithmetic

Claude 4.7 and later read images in 28×28 patches, capped at **4784 patches**
and a **2576 px long edge**; anything bigger is downscaled before the model
sees it. So there is an exact right answer for capture scale.

`capture.mjs` solves for it directly — the largest device pixel ratio where the
clip still fits both limits. A 936×286 card comes out at 2.5×; a 644×334 card
at 4×, landing on 2576 px exactly.

`crawl.mjs` cannot do that per component, because Playwright fixes the scale
per browser context and the page is loaded once. It renders everything at
`--dpr` (3 by default) and downscales anything over budget afterwards, which is
strictly better than re-rendering smaller: the result is supersampled, so edges
and text come out cleaner than a native low-DPR pass.

## Determinism

Location, units, timezone, locale, colour scheme and viewport are pinned; ad
and telemetry hosts are blocked; overlays are hidden and CSS animation paused
before the shutter. The page is scrolled end to end first so lazy modules mount
— without that the crawl finds the first few cards and reports the rest as
absent, which looks exactly like a page that only has a few cards.

That makes the *design* reproducible. It does not make captures byte-identical:
the weather underneath is live, so curves, axis ranges and day labels move.
Geometry and styles are stable; chart data is a sample.

Pinning location is also how you reach states you cannot see from your desk —
`--city phoenix` for an extreme-UV axis, `--city reykjavik` for sub-zero,
`--city singapore` for saturated humidity.

## Scope

This captures third-party UI for study. Layout, proportion, interaction and
information architecture are fair to learn from and reimplement. Icons,
illustrations, fonts and markup are not ours to ship — asset URLs are recorded
but never downloaded, `reference/` stays out of git, and Clima's own icons come
from Meteocons (MIT). Nothing here belongs in a release artifact.
