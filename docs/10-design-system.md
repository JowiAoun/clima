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
| Section heading | `sectionTitle` | 18 | bold | `textPrimary` |
| Card title | `cardTitle` | 15 | bold | `textPrimary` |
| Detail card title | `detailTitle` | 14 | normal | `textPrimary` |
| Reading (the big number) | `reading` | 34 | bold | `textPrimary` |
| Reading, when there are two | `readingPair` | 26 | bold | `textPrimary` |
| Status line | `status` | 14 | bold | `textPrimary` |
| Body / description | `body` | 12 | normal | `textMuted` |
| Label beside a reading | `label` | 12 | normal | `textMuted` |
| Axis and tick labels | `axis` | 11 | normal | `textDim` |

The current-conditions card is the page headline and has its own five, none of
which appear anywhere else:

| Role | Token | Size | Weight |
|---|---|---|---|
| The temperature | `heroReading` | 64 | **normal** |
| Its degree suffix | `heroUnit` | 34 | normal |
| The condition beside it | `heroCaption` | 32 | bold |
| Outlook sentence, slug values | `heroDetail` | 18 | normal |
| Slug labels | `heroLabel` | 14 | normal |

`heroReading` is set in book weight, not bold. It is the one deliberate
exception to "a reading is bold": at 64 px bold stops reading as a number and
starts reading as a shout, and the reference agrees — its own is weight 400.

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

Durations are tokens in `theme.js` as `Theme.motion.*`. **Never write a literal
duration in a component.** The first version of this section gave *ranges* —
"140–160 ms" — and the eight components that animated at all came back with
130, 140, 150, 160, 170, 190, 340 and 430 ms. Eight durations for four jobs.
A range is not a rule; this is the same lesson §10.4 learned about type.

| Job | Token | ms |
|---|---|---|
| A fill, a text colour, a border | `tint` | 150 |
| Something moved or changed size | `move` | 190 |
| One view becoming another | `view` | 340 |
| A value finding its place | `reveal` | 520 |
| Between one sibling's reveal and the next | `stagger` | 45 |

**Easing is `Easing.OutCubic` unless you have a stated reason.** Things
decelerate into place because they are arriving, not departing. It is written
literally rather than tokenised because `Easing.OutCubic` is a QML enum and
`theme.js` is a plain JS library — a name is not a magic number, and it greps.

The reference uses `0.2s linear` for its selection fill. Ours is slightly
faster and eased, which reads better at these sizes; that is a deliberate
divergence, not drift.

### What must not animate

Restraint is most of the work here. A page where everything moves is a page
where nothing is legible, and every one of these has a specific failure mode.

- **Nothing animates on a timer.** No pulsing, breathing, shimmering, drifting
  or looping. If it moves while the user is doing nothing, it is wrong. This
  also destroys golden-image tests, which are the only regression net this
  prototype has.
- **No reveal is ever re-triggered.** In particular, nothing fires on scrolling
  into view. A grid that re-animates each time it passes the fold turns a page
  of information into a slot machine, and makes scrolling back to re-read a
  value actively unpleasant.
- **Everything settles inside a second.** `--grab` waits 1600 ms; anything
  still moving after that makes every golden image a coin toss.
- **Text does not fly, fade or slide.** A reading may count up; a label may
  change colour. Nothing else. A card whose title fades in is a card the reader
  is waiting for.
- **The reader can read it at rest position zero.** If a component is
  meaningless until its animation finishes, the animation is load-bearing and
  the component is broken. Titles, statuses and bodies are present immediately.
- **No `Math.random`, anywhere, including in timing.** Determinism is not
  negotiable.
- **Layout does not animate on resize.** Reflow is not a transition; a window
  drag that makes twelve cards ease to new positions reads as lag.

### The reveal

Detail cards have no interaction and no changing data — the provider values are
fixed for the life of the process — so no card ever transitions between states.
Their one honest piece of motion is *arrival*: a dial sweeping up to its
reading, a bar growing off its baseline, a curve drawing itself in. It earns its
place by showing where the value sits on its scale rather than merely asserting
it.

`DetailCard` provides the hook. `reveal` runs 0 → 1 once, shortly after the card
is built, over `Theme.motion.reveal`; bind whatever should grow or sweep to it.
`WeatherDetails` sets `revealDelay` per card so the grid arrives as one wave
rather than twelve separate events.

### Reviewing motion

A still frame cannot show motion, and `--grab` lands wherever the animation
happened to be — usually after it finished. **An animation that is missing
grabs identically to one that is right.** `film.sh` is the tool:

```sh
./film.sh out.png -- --size 1000x700 --scroll 430 --poke feels=true
./film.sh out.png --frames 12 --every 45 -- --gallery UV --poke remount=1
./film.sh out.png -- --poke metric=uv
./film.sh out.png -- --poke day=4
```

It films N frames and tiles them into one contact sheet, reading left to right.
Frame 00 is the state *before* the poke; every frame after it is the transition.
`--poke` drives `metric`, `day`, `list`, `feels`, `scroll` and — in the gallery
— `remount`, which rebuilds the specimen and replays whatever it does on mount.

If every frame on the sheet looks identical, either nothing is animating or the
whole thing finished inside one interval.

**`--every` has a floor of roughly 90 ms per frame offscreen**, whatever you ask
for: `grabToImage` plus a PNG encode costs more than the interval you set, so
`--every 20` and `--every 60` produce the same sheet. A `tint` at 150 ms is
therefore about two usable frames, and anything shorter cannot be resolved by
filming at all. For those, stretch the duration temporarily, film, and put it
back — or sample the pixel directly and check the curve is monotonic.

Two states are currently unreachable by `--poke`, so their transitions cannot be
filmed as they stand:

- **A pager fade.** `PagerButton.enabledState` derives from a Flickable's
  `contentX`, which only a drag or a pager press changes.
- **The page's scroll thumb.** `--poke scroll=` assigns `contentY`, and
  `QQuickFlickable::setContentY()` calls `movementEnding()`, so `moving` never
  becomes true. `--poke flick=` exists for this: it uses `flick()` so the view
  really is in motion.

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
  heading. It is worse than "does not bound them" — with `clip: true` and no
  layer, a sparkline revealed by a travelling clip window **paints all twelve
  hours for the first two frames** and the clip only starts biting part-way
  through. An animation that works only once it is nearly over. `layer.enabled: true` on the *Flickable* fixes it, because a child
  outside the layer's texture is never drawn into it. Safe over a gradient even
  though every surface is translucent — source-over compositing is associative,
  so flattening a group and then laying it over the page gives the same pixels.
  Do not layer a rounded opaque item, where it notches the corners.
- **`visible: false` and `opacity: 0` are not interchangeable here.** Hiding the
  chart subtree corrupts clip state for unrelated nodes — that is why the chart
  stays loaded underneath the list. But *loaded* was silently taken to mean
  *painted*, and once surfaces went translucent the chart started showing
  through the list rows. `opacity: 0` leaves the scene graph untouched and only
  stops it painting, which is what was wanted all along. Pair it with
  `enabled: false` or the invisible thing still takes clicks.
- QML cannot produce `GradientStop` or path elements from a `Repeater`.
  Gradients are declared statically; paths are generated as strings in JS and
  handed to `PathSvg`.
- Qt swallows QML errors when it decides stderr has no console.
  `QT_FORCE_STDERR_LOGGING=1` — which `run.sh` sets — is the difference between
  a real error and `qml: Did not load any objects, exiting.`

## 10.9 Checking your work

```sh
./run.sh                                        # the page
./run.sh --grab p.png --scroll 800              # …scrolled down, headless
./run.sh --gallery                              # every component, one screen
./run.sh --gallery uv                           # …opened on one of them
./run.sh --grab g.png --gallery Colour --walk 5 # …stepped 5 on, headless
./run.sh --card Uv                              # one card, on the page gradient
./run.sh --grab shot.png --card Uv              # …headless, to look at pixels
./run.sh --grab g.png --details --size 1300x900 # the whole grid, all three rows
```

**Scroll before you grab.** The page is taller than any window it runs in, so a
grab of the first screenful reviews maybe a third of it — and the sections below
the fold are the ones with twelve charts in them. `--scroll` exists for the same
reason `--walk` does.

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

## 10.10 The page

`WeatherPage.qml` stacks four sections in one scrolling column:

| | |
|---|---|
| Location bar | where this is for |
| Current conditions | the headline |
| Hourly | metric tabs → day strip → chart or list |
| Weather details | twelve cards, one per measurable |

The order is the reference's and it is an argument rather than a habit: what is
it doing now, what will it do today, tell me about one thing in particular.
Each section asks a narrower question than the one above it.

### Rules

- **A section is not a surface.** Sections are separated by `sectionGap` and
  nothing else — no rules, no wrapper panels. A panel around a section would be
  a wash containing washes, so every card inside it would composite to 0.135 and
  the whole block would read as a lighter patch. Contrast against the page is
  what defines a surface here (§10.1), which leaves a section as pure layout.
- **One thing scrolls.** The page owns the scroll; nothing inside it scrolls
  vertically. Two nested vertical scroll areas give the reader two things to
  drag and no way to predict which one they get. Horizontal scrolling inside a
  section — the day strip, the chart — does not conflict and is fine.
- **A section reports its height.** Every component on the page has an
  `implicitHeight`; none is anchored to the bottom of anything. A component that
  derives its layout from the height it is handed cannot go in a column, and
  `HourlyOverview` had to be given the inverse relation before it could.
- **The page's Flickable carries the layer.** Not the sections. One layer bounds
  every Shape on the page (§10.8); a layer per section is that many offscreen
  textures for the same result.
- **Two headings on one page must be the same heading.** They were 18 and 15,
  set independently, and neither author was wrong on their own. `sectionTitle`
  and `SectionHeader.qml` exist so it cannot happen again.
- **The headline reads its numbers from the cards' own data.** The hero shows
  the temperature, the condition and the feels-like that `detaildata.js` gives
  the detail cards three sections below. A hero carrying its own copy is a hero
  that will eventually contradict the card it is summarising.

### What assembling it found

Worth recording, because none of it was visible in any single component:

- The sun glyph's "soft halo" was a flat disc at 16 % alpha — an edge you can
  trace, which §10.1 defines as a stacked wash rather than a glow. At the 26 px
  the glyph is normally drawn at, the edge is two pixels and invisible. At the
  72 px the hero draws it at, it is a hard-rimmed ring.
- The list view had the chart painting through it, on `main`, since the day
  surfaces became translucent. See §10.8.
- The two section headings disagreed by 3 px.
- `Theme.metric.plotHeight` had been in the token table the whole time, unused.

Three of the four are of one kind: a decision that was correct in the context it
was made in and wrong once something else changed. That is what a page is for —
it is the only place the contexts meet.
