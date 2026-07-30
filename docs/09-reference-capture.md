<!-- SPDX-License-Identifier: CC-BY-SA-4.0 -->

# 09 — Reference capture

How we work from MSN Weather without guessing, and what the first capture
already corrected.

The harness is [`tools/refcap`](../tools/refcap/); its README covers usage. This
document is the *why*, plus the distilled measurements — captures themselves are
gitignored, so anything worth keeping has to end up here.

## 9.1 The problem with screenshots

Up to now this prototype was built from screenshots pasted into a chat. That
works, and got us surprisingly far, but it caps out for three reasons.

**Resolution.** Claude 4.7 and later read images in 28×28 patches, capped at
4784 patches and a 2576 px long edge. A component photographed off a 1× screen
at 600×300 arrives as ~230 patches. The same component captured at DPR 2.5
arrives as ~2200 — an order of magnitude more detail, for free, and still
un-downscaled. Most reference images are far *below* the ceiling, not above it.

**Colour is lossy through pixels.** Sampling a screenshot gives the colour after
compositing: gradient over translucent card over page gradient, times antialiasing.
Reading it back gives a plausible-looking value that is not the one anybody chose.

**Some decisions are invisible in a raster.** Which axis a gradient is keyed to,
how long a transition runs, what easing it uses, what a hidden element is for.
These are not subtle-in-the-image; they are absent from it.

The browser has already computed every one of these numbers in order to lay the
page out. Capturing them is strictly easier than inferring them.

## 9.2 Why not just download the HTML/CSS/JS

It is the obvious move and it is the wrong one. MSN Weather is a React app: the
served HTML is a shell, and the chart does not exist until script runs. The CSS
is minified with hashed CSS-module names (`hourlyChart-DS-J5A_4k`), so finding
one card's radius means reading megabytes of rules to work out which of forty
selectors wins. The JS is bundled and minified.

We do not want their source. We want the *computed result* — and only a running
browser has that. So: render the page, then dump what the engine decided.

| Approach | Gets geometry | Gets true colour | Gets motion | Survives a redeploy |
|---|---|---|---|---|
| Pasted screenshot | eyeballed | approximate | no | n/a |
| `wget` the HTML/CSS/JS | no | no | no | no |
| Browser MCP, interactive | yes | yes | yes | no — re-driven each time |
| **Scripted capture (this)** | **exact** | **exact** | **exact** | **yes — re-run it** |

A browser MCP would answer the same questions. The reason this is a checked-in
script instead is that a capture should be a build artifact, not a conversation:
re-runnable, diffable when MSN ships a redesign, and — because artifacts land on
disk rather than in the context window — free to produce in bulk and read
selectively.

## 9.3 What a capture contains

Per component: `shot.png` (clipped, DPR solved so it is never downscaled),
`geometry.json` (every box, CSS px, relative to the component), `styles.json`
(computed visual properties), `svg-*.svg` (vectors verbatim, styling inlined),
`tokens.json` (the site's CSS custom properties), `meta.json` (provenance).

Location, units, timezone, locale, theme and viewport are pinned; ads and
telemetry are blocked; animation is paused before the shutter. Two runs of the
same target agree.

Pinning location also reaches states we cannot see locally: `--city phoenix` for
an extreme-UV axis, `--city reykjavik` for sub-zero, `--city singapore` for
saturated humidity.

## 9.4 First capture: `hourly-chart`

Seattle, dark, °C. Card 920×270 CSS, captured at DPR 2.5 → 2340×715 px.

### Chrome

| | Measured |
|---|---|
| Card fill | `rgba(255,255,255,0.08)` over `backdrop-filter: blur(60px)` |
| Card radius | 6 px |
| Header band | 61 px, content inset 16 px |
| Selected pill | `#FFD02C`, radius 25 px, padding 4×12 px, icon/label gap 4 px |
| Unselected pill | no fill, `#FFFFFF` text, same box |
| Body font | `"Segoe UI", "Segoe WP", Arial, sans-serif` |

The card is not an opaque panel. It is a **translucent white wash over a heavy
backdrop blur** — an acrylic surface that samples the page gradient behind it.
That is why the card reads as a different colour at the top of the page than
further down, and it is a materially different model from a flat fill.

### Chart geometry

`viewBox="0 0 890 172"`, y-axis gutter 41 px, plot 849 px wide, 24 hourly
samples at a pitch of 36.913 px (`849/23`), x-labels on every second tick, the
first labelled `Now`. Five y-gridlines 32 px apart.

The y labels read −5, 3, 10, 18, 25 °C — uneven steps, because the axis divides
the range into four *equal* intervals (7.5 °C) and rounds each label for display.

The curve is cubic Bézier with control points at exactly ⅓ and ⅔ of each
segment — a Catmull-Rom conversion, which is what `chartmath.js` already does.

### The area fill — and where we got it wrong

Two gradients, crossed:

```
<linearGradient id="gradientareachart"   x1="0" x2="1" y1="0" y2="0">  ← horizontal
<linearGradient id="gradientareaopacity" x1="0" x2="0" y1="0" y2="1">  ← vertical
```

- **Colour runs horizontally.** 24 stops at 1/23 spacing — one per hour. Each
  hour's temperature is quantised to a band colour and the gradient interpolates
  between neighbours. Only four distinct colours appeared in this capture:
  `#83EFD4` cold · `#A6FFC0` cool · `#C2FFA1` mild · `#FFDB8C` warm.
- **Transparency runs vertically**, applied as a `<mask>` over the plot box:
  fully opaque for the top 25 %, then an S-curve fade to zero at the baseline.
  It is anchored to the plot rectangle, not to the curve.
- **There is no stroke.** The second path carries `class="area-curve-hide"`.
  The soft top edge is the fill's own antialiased boundary.

Our implementation does the opposite: a single **vertical** ramp keyed to
normalised value-axis position, plus a gradient-filled ribbon standing in for a
stroked line. Both produce a handsome chart, but they encode different things.
MSN's says *this hour was warm*; ours says *this height is warm*. On a flat
stretch of curve the two agree; where the curve climbs steeply they diverge, and
MSN's is the more honest reading — the colour tracks the datum, not the pixel.

This is not a defect we could have seen in a screenshot. In the raster it looks
like a soft warm glow over the afternoon peak. It is in fact a vertical band
spanning the full column height between 4 PM and 6 PM, and the gradient
definition says so in one line.

## 9.5 Second capture: `day-junction`

Worth recording because it contradicts an assumption the prototype is built on.

**The live day cards do not merge into the panel below them.** The selected card
is a fully rounded box — 6 px on all four corners — with a 1 px light outline, a
`rgba(255,255,255,0.08)` fill, 14×12 px padding, and a clear gap before the panel
starts. No tab seam, no fillet.

| | Measured |
|---|---|
| Card radius | 6 px, all corners |
| Selected fill | `rgba(255,255,255,0.08)` |
| Padding | 14 px vertical, 12 px horizontal |
| Selection transition | `background-color 0.2s linear` |

Our day strip instead grows the selected card into the panel and fillets the
reflex corner either side. That came from reference crops showing exactly that
merge, so MSN has evidently shipped both; the crops and the current live build
disagree. The merged treatment is the better of the two and we are keeping it —
but it is now a deliberate divergence rather than an imitation, and this is the
note that says so.

The one number worth adopting outright is the transition: 0.2 s linear on fill
alone. Ours animates fill, width and height together over 0.16–0.19 s.

### Not yet resolved

- MSN's dark presentation does not follow `prefers-color-scheme`; the SVG carries
  explicit `-dark` class variants while `:root` still reports a light `--fill-color`.
  The switch is elsewhere — a cookie or account setting. Captures currently come
  out in the page's own palette, which is the dark one, so this has not bitten us.
- `tokens.json` holds 122 Fluent design tokens (`--neutral-fill-*`, `--type-ramp-*`,
  `--accent-*`). They are the *site chrome's* system, not the weather cards' —
  worth mining for the type ramp, not for the chart palette.

## 9.6 Working rule

Capture → measure → decide → implement. The capture is evidence, not a target:
we are matching MSN's *quality*, not cloning its output, and where we think it
is wrong (see [05-feature-parity](05-feature-parity.md)) we should diverge on
purpose and say so.

Layout, proportion, interaction and information architecture are fair to learn
from. Icons, illustrations, fonts and markup are not ours to ship — `reference/`
is gitignored for that reason, and Clima's icons come from Meteocons (MIT).
