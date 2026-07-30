<!-- SPDX-License-Identifier: CC-BY-SA-4.0 -->

# refcap — reference capture

Captures a reference UI component at a fidelity worth working from.

```sh
npm install                      # playwright + chromium, once
node capture.mjs --list
node capture.mjs hourly-chart
node capture.mjs hourly-chart --city phoenix --state uv --units F
```

Output lands in `reference/msn/<target>--<city>--<theme>/`, which is
gitignored. See [docs/09-reference-capture.md](../../docs/09-reference-capture.md)
for why, and for what to do with a capture once you have one.

## Why not just take a screenshot

A screenshot answers *roughly what does this look like*. Replication needs
*what exactly is this*, and the browser already knows — it computed every
number to lay the thing out. So each capture emits four artifacts together:

| File | What it settles |
|---|---|
| `shot.png` | Appearance. Clipped to the component, at the largest DPR that survives the vision pipeline un-downscaled |
| `geometry.json` | Every box in the subtree, CSS px, relative to the component origin. Sizes and spacing without measuring pixels |
| `styles.json` | Computed styles — colours, radii, fonts, shadows, transition easings — read rather than guessed |
| `svg-*.svg` | Any vector in the subtree, verbatim, styling inlined so it stands alone |
| `tokens.json` | The site's own CSS custom properties, if it has them |
| `meta.json` | Provenance: URL, city, units, theme, DPR, token cost |

The SVG is usually the jackpot. A chart's `<linearGradient>` stop list is the
designer's colour decision *as a list of numbers* — no amount of pixel-peeping
recovers that reliably, and the gradient's `x1/x2/y1/y2` tell you which axis
the colour is keyed to, which is a design decision you would otherwise have to
infer and can easily infer wrong.

## The DPR arithmetic

Claude 4.7 and later read images in 28×28 patches, capped at **4784 patches**
and a **2576 px long edge**; anything bigger is downscaled before the model
sees it. So there is an exact right answer for capture scale, and `bestScale()`
solves for it: the largest device pixel ratio where the clip still fits both
limits.

Capturing above the ceiling wastes time and returns nothing. Capturing below it
throws away detail that was free. A 936×286 card comes out at DPR 2.5 —
2340×715, 2184 of 4784 patches, not resized by a single pixel. The same card
photographed off a 1× screen would carry roughly a sixth of that detail.

## Determinism

A capture that moves with the machine's location, unit preference, clock or ad
auction is not a reference: two runs disagree for reasons that have nothing to
do with the design. So the harness pins location (via MSN's own base64 `loc`
parameter), units, timezone, locale, colour scheme and viewport, blocks ad and
telemetry hosts, hides late-arriving overlays, and pauses CSS animation before
the shutter.

Pinning location is also how you reach states you cannot see from your desk —
`--city phoenix` for an extreme-UV chart, `--city reykjavik` for a sub-zero
axis, `--city singapore` for a saturated humidity curve.

## Adding a target

`targets.json`, one entry per component:

```json
"day-carousel": {
  "description": "Day cards strip — the tab-into-panel junction",
  "url": "https://…/hourlyforecast/in-Seattle,Washington?loc={loc}&weadegreetype={units}",
  "selector": "[class*=\"carouselContainer-DS\"]",
  "pad": 16,
  "settleMs": 7000,
  "states": { "uv": { "click": "…>> text=UV" } }
}
```

MSN ships hashed CSS-module class names (`hourlyChart-DS-J5A_4k`). The hash
changes when they redeploy, so **always prefix-match** — `[class*="hourlyChart-DS"]`,
never the full name. If a selector stops matching, that is the first thing to check.

Parts of the page are web components, so the DOM walk pierces shadow roots. A
plain `querySelectorAll` walks straight past whole subtrees.

## Scope

This captures third-party UI for study. Layout, proportion, interaction and
information architecture are fair to learn from and reimplement. Icons,
illustrations, fonts and markup are not ours to ship — `reference/` stays out
of git, and Clima's own icons come from Meteocons (MIT). Nothing here belongs
in a release artifact.
