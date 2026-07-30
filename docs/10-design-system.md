<!-- SPDX-License-Identifier: CC-BY-SA-4.0 -->

# 10 — Design system

The rules every Clima component follows. Measured off the reference where a
number was worth copying, decided on purpose where it was not.

`prototype/hourly-overview/theme.js` is the single source of truth for values.
**Never hard-code a token's value in a component** — if a colour or radius is
missing from `theme.js`, that is a change to `theme.js`, not a literal in a QML
file. The exception is a colour that belongs to one visualisation and one only
(a UV band, a wind rose needle); those live with their component.

## 10.1 Surfaces

There are no opaque cards. The page is one vertical gradient and every surface
is a thin white wash over it, so a card's colour is whatever the gradient is
doing behind it at that height.

| Level | Token | Alpha | Use |
|---|---|---|---|
| Recede | `surfaceRecede` | 0.05 | Unselected, inactive, backgrounded |
| Base | `surfaceBase` | 0.07 | The default card and panel |
| Raised | `surfaceRaised` | 0.10 | Hover, selection, emphasis |

Three consequences, all of which have bitten already:

- **Washes stack.** Two 0.07 surfaces overlapping give 0.135 — a patch visibly
  lighter than either. Never nest a surface inside a surface, and never overlap
  them to hide a seam. If two surfaces must read as one, make them *abut*.
  A dial face, a disc behind a number, a panel behind a chart: these are all
  surfaces, whatever they are called and whatever colour they are tinted. The
  test is whether it has an edge. A shape that fades to zero alpha at its rim
  is a glow and is fine; one you can trace the outline of is a stacked wash.
- **A white wash is not a colour you can put text on.** Ink for anything
  sitting on the accent is `onAccent`, never a surface token.
- **Borders are usually wrong.** A 1px outline across a junction is exactly the
  seam the junction exists to avoid. Contrast against the page defines a card;
  reach for a border only when nothing else will do.

`backdrop-filter: blur(60px)` is in the reference and deliberately not
reproduced: blurring a smooth vertical gradient returns the same gradient. It
would earn its keep over the radar map, and nowhere else we have built.

## 10.2 Page gradient

Five stops, in `theme.js` as `pageStop0`…`pageStop4`, applied in `Main.qml`.
QML cannot generate `GradientStop` elements from a `Repeater`, so they are
written out — if you add a stop, add it in both places.

## 10.3 Geometry

| Token | Value | |
|---|---|---|
| `cardRadius` | 14 | Outer cards |
| `panelRadius` | 12 | Panels within a card |
| `detailRadius` | 12 | Weather-detail cards |
| `controlRadius` | 8 | Pills, buttons, rows, readouts |
| `filletRadius` | 18 | The concave tab/panel junction |
| `cardPadding` | 22 | Outer card inset |
| `detailPadH` / `detailPadV` | 20 / 16 | Detail card inset |
| `detailCardWidth` × `Height` | 300 × 250 | Detail card, measured |
| `detailGap` | 16 | Grid gutter |

Radii are generous on purpose: the reference reads soft because almost nothing
in it meets at a hard edge.

## 10.4 Type

Sizes are integers. **`font.pixelSize` is an int in Qt** — assigning `12.5`
fails object creation and Qt reports it only as `Type X unavailable` from the
*parent* file, which is a miserable hour to lose.

Sizes live in `theme.js` as `Theme.type.*`, not as literals. The first pass at
this document gave a *range* for the reading, and twelve independently-written
cards came back with seven different sizes for it — 18 to 34 — which in a grid
reads as twelve authors rather than as one set. A range is not a rule.

| Role | Token | Size | Weight | Colour |
|---|---|---|---|---|
| Card title | `cardTitle` | 15 | bold | `textPrimary` |
| Detail card title | `detailTitle` | 14 | normal | `textPrimary` |
| Reading (the big number) | `reading` | 34 | bold | `textPrimary` |
| Reading, when there are two | `readingPair` | 26 | bold | `textPrimary` |
| Status line | `status` | 14 | bold | `textPrimary` |
| Body / description | `body` | 12 | normal | `textMuted` |
| Label beside a reading | `label` | 12 | normal | `textMuted` |
| Axis and tick labels | `axis` | 11 | normal | `textDim` |

`readingPair` is for a card whose subject is genuinely two co-equal numbers —
sunrise *and* sunset, speed *and* gust. It is not a licence to shrink a reading
that does not fit: if a single reading does not fit at 34, the layout around it
is what is wrong.

## 10.5 Colour meaning

Colour encodes a value, never decoration. Where an authority publishes bands —
WHO for UV, European AQI — the bands *are* the palette, and they are
categorical, so they get flat colours rather than a gradient. `detaildata.js`
carries them in `bands`.

Trend colours are fixed: `trendUp` warm, `trendDown` cool, `trendSteady`
neutral. A rising temperature and a rising pressure use the same up colour;
the card's words say whether that is good news.

## 10.6 Motion

| | |
|---|---|
| Colour / fill change | 140–160 ms |
| Size or position change | 190 ms, `Easing.OutCubic` |
| View transitions | 340 ms, `Easing.OutCubic` |

The reference uses `0.2s linear` for its selection fill. Ours is slightly
faster and eased, which reads better at these sizes; that is a deliberate
divergence, not drift.

Never animate on a timer that runs when nothing is happening. Everything here
is state-driven.

## 10.7 Weather-detail cards

Twelve cards share one anatomy, provided by `DetailCard.qml`: a quiet title, a
visualisation, a bold status line with a trend badge, and a sentence of
context. Only the visualisation differs.

```qml
DetailCard {
    title: qsTr("UV")
    status: d.status
    trend: d.trend          // "up" | "down" | "steady" | "none"
    body: d.body
    content: Item { /* the visualisation */ }
}
```

The `content` slot is given `contentWidth` × `contentHeight` and must stay
inside it — the card owns its padding. Read values from `detaildata.js`; do not
invent numbers and do not format them in the data file.

`DetailTemperatureCard.qml` is the worked example. Follow its shape.

The shell reserves two lines for the body whether the sentence fills them or
not, so all twelve status lines and content boxes line up. Keep body sentences
under about 85 characters or they elide mid-word, which reads as a bug.

### The anatomy is fixed

- **The reading sits at the bottom-left of the content box**, at
  `Theme.type.reading`. Not centred, not right-aligned; the grid has a left
  rhythm and one card breaking it is the one you notice. **One exception:** a
  dial puts its reading in the middle of the ring, because the ring is a scale
  drawn *around* the number and setting the number off to one side leaves the
  ring circling nothing. All three dials — UV, air quality, cloud cover — take
  it, and they share one geometry: 310° of arc from 115°, a 7 px stroke, a
  `trackLine` remainder, and the mark where the paint stops.
- **A card with a side-by-side layout still bottom-anchors its readout.**
  Centring the right-hand column vertically instead is what left row 2 of the
  grid with no baseline while rows 1 and 3 had one.
- **The content box is exactly `contentWidth` × `contentHeight`.** No negative
  margins to borrow a few pixels from the card's padding. If a visualisation
  needs more height, it needs to be simpler, not to hang outside its box.
- **The status line owns the word; the visualisation owns the number.** Never
  both. `Sunny` in the status line and `8%` in the dial — not `Sunny (8%)` in
  the status line and `Sunny` again in the dial.
- **One visualisation per card.** A card with two charts in it has neither the
  room to draw either properly nor a single answer to give.
- **The "now" mark is one thing everywhere**: a 14 px disc with a 2.5 px
  `textPrimary` ring. Not 12, not 20, and not a rule dangling below it.

### Rules for a visualisation

- **Draw the reading, not a decoration.** If the shape would look the same for
  a different value, it is not a visualisation. A dial whose arc is a fixed
  grey ring with a coloured dot on it has drawn a scale and left the reading
  to the dot; fill the traversed arc and the ring itself carries the value.
- **A gauge needs a visible track.** The unfilled remainder is `trackLine` —
  the reading only means something as a fraction of something.
- **Scales are data.** A ceiling that decides what the reader sees (`scaleMax`
  for precipitation, wind, visibility) belongs in `detaildata.js`, not as a
  literal in the card. And check what a field *means* before scaling against
  it: `visibility.peak` is today's best, so dividing by it drew "Excellent" as
  a third of a bar.
- **Distinguish observed from forecast.** Observed is solid; forecast is the
  same line at lower contrast. The past is real data and stays visible.
- **Bars for sums and bands, curves for continuous quantities.** Drawing
  0.4 mm then 0 mm as a smooth curve claims a shape the data does not have.
- **No `Math.random`.** Golden-image tests depend on determinism.
- Use `chartmath.js` for paths — `smooth`, `areaPath`, `ribbonPath`,
  `sampleRamp`, `moonPath` are already there and already correct.

## 10.8 Qt traps that have already cost time

- `font.pixelSize` is an **int**. See §10.4.
- `Item` declares some obvious names FINAL — `top` among them. A
  `readonly property real top` in a delegate fails with *Cannot override FINAL
  property*. Prefix delegate locals: `barTop`, `barValue`.
- **Qt Quick Shapes escape ancestor clipping.** `clip: true` on a Flickable or
  ListView does not bound them: scroll `WeatherDetails.qml` without a layer and
  the card rectangles clip while their sparklines paint straight out over the
  heading. `layer.enabled: true` on the *Flickable* fixes it, because a child
  outside the layer's texture is never drawn into it. Safe over a gradient even
  though every surface is translucent — source-over compositing is associative,
  so flattening a group and then laying it over the page gives the same pixels.
  Do not layer a rounded opaque item, where it notches the corners.
- QML cannot produce `GradientStop` or path elements from a `Repeater`.
  Gradients are declared statically; paths are generated as strings in JS and
  handed to `PathSvg`.
- Qt swallows QML errors when it decides stderr has no console.
  `QT_FORCE_STDERR_LOGGING=1` — which `run.sh` sets — is the difference between
  a real error and `qml: Did not load any objects, exiting.`

## 10.9 Checking your work

```sh
./run.sh --gallery                              # every component, one screen
./run.sh --gallery uv                           # …opened on one of them
./run.sh --grab g.png --gallery Colour --walk 5 # …stepped 5 on, headless
./run.sh --card Uv                              # one card, on the page gradient
./run.sh --grab shot.png --card Uv              # …headless, to look at pixels
./run.sh --grab g.png --details --size 1300x900 # the whole grid, all three rows
./run.sh                                        # the hourly screen
```

`--gallery` is the component library: every component on the gradient it is
composited over, including the states no current screen uses. Its **Colour**
and **Type** pages are generated from `theme.js`, so a token added there shows
up without anyone maintaining a list. Add a component to it in `gallery.js`.

Stage a component at a size no screen currently gives it. `HourlyOverview` had
its hour glyphs escaping the panel's clip — §10.8, again — and at the window's
own width the escapees land off-window where nobody sees them. The gallery
staged it at 1000 px and they were unmissable.

**Check the second screen, not just the first.** `--walk N` steps the gallery
on before grabbing, because every bug the gallery itself has had appeared only
on the component shown *after* another one: a specimen composited over its
predecessor, a deferred rebuild firing on a torn-down delegate, a scroll
position carried over from an entry three times taller. Rendering one thing
from a cold start exercises none of that.

Check a card in the grid, not only on its own. Every finding that mattered in
the first pass — seven reading sizes, one card sitting 23 px low, two dials
that disagreed about what a track means — was invisible in a single card and
obvious the moment twelve sat side by side. `--size` exists so the last row is
in the frame; reviewing through a viewport that cuts it off is how a broken
card in that row stays unnoticed.

Render it and look at it. Every defect found in this prototype so far was found
by rendering and looking — several of them were invisible in the code and
obvious in the image.
