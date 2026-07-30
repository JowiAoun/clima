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

| Role | Size | Weight | Colour |
|---|---|---|---|
| Card title | 15 | bold | `textPrimary` |
| Detail card title | 14 | normal | `textPrimary` |
| Reading (the big number) | 30–34 | bold | `textPrimary` |
| Status line | 14 | bold | `textPrimary` |
| Body / description | 12 | normal | `textMuted` |
| Axis and tick labels | 11–12 | normal | `textMuted` / `textDim` |

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

### Rules for a visualisation

- **Draw the reading, not a decoration.** If the shape would look the same for
  a different value, it is not a visualisation.
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
  ListView does not bound them. `layer.enabled: true` on an ancestor does — but
  only put a layer on an item with an opaque background, and never on a rounded
  one, where it notches the corners.
- QML cannot produce `GradientStop` or path elements from a `Repeater`.
  Gradients are declared statically; paths are generated as strings in JS and
  handed to `PathSvg`.
- Qt swallows QML errors when it decides stderr has no console.
  `QT_FORCE_STDERR_LOGGING=1` — which `run.sh` sets — is the difference between
  a real error and `qml: Did not load any objects, exiting.`

## 10.9 Checking your work

```sh
./run.sh --card Uv                    # one detail card, on the page gradient
./run.sh --grab shot.png --card Uv    # …headless, for a look at the pixels
./run.sh --details                    # the whole grid
./run.sh                              # the hourly screen
```

Render it and look at it. Every defect found in this prototype so far was found
by rendering and looking — several of them were invisible in the code and
obvious in the image.
