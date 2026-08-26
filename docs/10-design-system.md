<!-- SPDX-FileCopyrightText: 2026 Jowi Aoun -->
<!-- SPDX-License-Identifier: CC-BY-SA-4.0 -->

# 10 — Design system

The rules every Clima component follows. Measured off the reference where a
number was worth copying, decided on purpose where it was not.

`app/qml/Clima/theme.js` is the single source of truth for values, and
`Theme.qml` republishes it as the singleton every component reads.
**Never hard-code a token's value in a component** — if a colour or radius is
missing from `theme.js`, that is a change to `theme.js`, not a literal in a QML
file. The exception is a colour that belongs to one visualisation and one only
(a UV band, a wind rose needle); those live with their component.

### Colour tokens are named for their role

A token's name says what job it does, not what it looks like. `surface.raised`
is the third rung of the wash ladder; that it is currently `#1affffff` is an
answer, not the question. This is the difference between a palette that can be
re-valued and one that cannot: the old names described a white-over-dark
rendering — `surfaceRaised`, `textDim`, `pastVeil` — so half the table was not a
decision but a description of a decision made somewhere else, and no light
values could be written into it.

| Role | What belongs in it |
|---|---|
| `page` | the ground everything else is composited on |
| `surface` | the §10.1 wash ladder, plus the opaque exceptions §10.12 allows |
| `ink` | text |
| `line` | anything a pixel wide that separates or measures |
| `accent` | the one saturated colour, and the only ink legible on it |
| `control` | interactive chrome: toggles, pagers, nav glyphs |
| `overlay` | drawn *over* content it must not let through |
| `state` | a verdict — a trend direction, a good/caution/poor band |
| `glyph` | the paints a weather glyph is drawn in |
| `badge` | the day/night disc behind a glyph |
| `scaffold` | deliberately off-palette: something not built yet |

`metric`, `type`, `motion` and `scale` are theme-invariant — a 14 px radius is
14 px in any palette — so they are not roles and are read as before.

Two tokens with the same value are not thereby the same token. `line.card` and
`surface.raised` are both `#1affffff` today and stay separate, because a light
theme darkens a line and keeps a surface a wash. What was folded together in the
role pass was the reverse case: six names — `cardBg`, `dayCardBg`, `stripBg`,
`stripPast`, `pillHover`, `switchActive` — for three rungs of one ladder, each
naming the *place* a wash was used rather than the level it sits at.

## 10.1 Surfaces

There are no opaque cards. The page is one vertical gradient and every surface
is a thin white wash over it, so a card's colour is whatever the gradient is
doing behind it at that height.

| Level | Token | Alpha | Use |
|---|---|---|---|
| Recede | `surface.recede` | 0.05 | Unselected, inactive, backgrounded |
| Base | `surface.base` | 0.07 | The default card and panel |
| Raised | `surface.raised` | 0.10 | Hover, selection, emphasis |

Three consequences, all of which have bitten already:

- **Washes stack.** Two 0.07 surfaces overlapping give 0.135 — a patch visibly
  lighter than either. Never nest a surface inside a surface, and never overlap
  them to hide a seam. If two surfaces must read as one, make them *abut*.
  A dial face, a disc behind a number, a panel behind a chart: these are all
  surfaces, whatever they are called and whatever colour they are tinted. The
  test is whether it has an edge. A shape that fades to zero alpha at its rim
  is a glow and is fine; one you can trace the outline of is a stacked wash.
- **A white wash is not a colour you can put text on.** Ink for anything
  sitting on the accent is `accent.ink`, never a surface token.
- **Borders are usually wrong — in dark mode.** A 1px outline across a junction
  is exactly the seam the junction exists to avoid. Contrast against the page
  defines a card; reach for a border only when nothing else will do.

  **In light mode the outer card edge is the exception**, and it is not a
  matter of taste. The same ladder inverted is a black wash over a near-white
  page, and 6% black over `#eef1f7` is a 1.14:1 step — measured on the palette
  page, not estimated. Contrast that small does not define anything, so
  `line.card` exists and is load-bearing in exactly one of the two themes. The
  rule generalises: on a pale ground a filled shape can be found by its edge or
  by nothing at all. It is also why `accent.fill` had to darken rather than
  gain an outline — it paints text and hairlines as well as pills, and an edge
  cannot rescue a word.

`backdrop-filter: blur(60px)` is in the reference and deliberately not
reproduced: blurring a smooth vertical gradient returns the same gradient. It
would earn its keep over the radar map, and nowhere else we have built.

## 10.2 Page gradient

Five stops, in `theme.js` as `sky.<phase>.stops`, applied in `PageBackdrop.qml`
— see §10.12's *The sky* for the phases and why every one of them is dark.
QML cannot generate `GradientStop` elements from a `Repeater`, so the five
*positions* are written out in the file that draws them and only the colours
come from the table; if you add a stop, add it in both places.

`page.bg` is the flat fallback for the one case that cannot take a gradient. It
is not a sixth stop. There used to be a `pageStop0`…`pageStop4` group here as
well, a second copy of `sky.dusk.stops` left from when dusk was the only sky
there was; nothing read it and it is gone.

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

The face is **Inter**, bundled. Two static files — `app/fonts/Inter-Regular.ttf`
and `app/fonts/Inter-Bold.ttf`, Inter 4.1 under the SIL Open Font License 1.1 —
registered in `main()` and made the application font, so every `Text` in the tree
gets it without naming it. `Theme.type.family` reads the family back off the
running application for the cases that have to name one: text drawn into a Canvas
or a QPainter, and any surface that opts back out to the platform.

This is not a preference, it is the difference between a product and a
screenshot. Before it, nothing in the QML named a family, which does not mean
"the default font" — it means fontconfig chose on Linux, DirectWrite on Windows
and CoreText on macOS. Different metrics, so different wrap points, so a body
that fits its card on one machine overflows it on another; and golden images that
cannot be compared across machines at all, because text is most of the pixels.

Two static faces rather than the one variable file, and the reason is `font.bold`.
Qt's font database registers a variable font's default instance and does not
expand its named instances, so `Inter Variable` arrives with exactly one style and
`font.bold: true` — which 58 lines in the tree set — synthesises a bold instead of
selecting one. Synthetic bold is the real face's weight in the wrong face's
spacing: at 34 px it measures 654.875 px where Inter Bold measures 674.297.
`app/appfont.cpp` has the measurements and the longer argument.

Sizes are integers. **`font.pixelSize` is an int in Qt** — assigning `12.5`
fails object creation and Qt reports it only as `Type X unavailable` from the
*parent* file, which is a miserable hour to lose.

Sizes live in `theme.js` as `Theme.type.*`, not as literals. The first pass at
this document gave a *range* for the reading, and twelve independently-written
cards came back with seven different sizes for it — 18 to 34 — which in a grid
reads as twelve authors rather than as one set. A range is not a rule.

| Role | Token | Size | Weight | Colour |
|---|---|---|---|---|
| Section heading | `sectionTitle` | 18 | bold | `ink.primary` |
| Card title | `cardTitle` | 15 | bold | `ink.primary` |
| Detail card title | `detailTitle` | 14 | normal | `ink.primary` |
| Reading (the big number) | `reading` | 34 | bold | `ink.primary` |
| Reading, when there are two | `readingPair` | 26 | bold | `ink.primary` |
| Status line | `status` | 14 | bold | `ink.primary` |
| Body / description | `body` | 12 | normal | `ink.muted` |
| Label beside a reading | `label` | 12 | normal | `ink.muted` |
| Axis and tick labels | `axis` | 11 | normal | `ink.dim` |

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

Trend colours are fixed: `state.trendUp` warm, `state.trendDown` cool, `state.trendSteady`
neutral. A rising temperature and a rising pressure use the same up colour;
the card's words say whether that is good news.

**A published band keeps its hue under both themes.** Six of the nine metric
ramps are continuous and ours, so the light theme inverts their lightness while
holding hue and chroma. The other three — `precip`, `aqi`, `uv` — are authority
bands, and re-hueing them would make the app disagree with the source it is
quoting. `Theme.categoricalRamps` is that list, and the gallery's Ramps page
labels each ramp with which kind it is.

### Every token declares what it owes its background

`Theme.contrastDefaults` and `Theme.contrastOverrides` record, per token, the
background it is actually composited over and which of three duties it does:
`text` (4.5:1), `essential` (3:1 — you must see it to read the content or work
a control), or `incidental` (no floor). The gallery's Colour page measures every
token against that contract in both schemes and prints the ratio, red where it
misses.

Three things that pass had to be learned, and they are the reason the contract
is per-token rather than one threshold:

- **A ratio is only meaningful against the right ground.** `accent.ink` looked
  broken at 1.48:1 against a card it is never drawn on. It is the label *on* the
  pill, and against `accent.fill` it is 11.42:1.
- **Gradients are scored as a pair.** A badge plate or a cloud is legible if
  *either* end of it separates from the ground, and which end does that flips
  between the schemes. Scored per stop, the same object reads as broken in one
  theme and fine in the other.
- **A WCAG ratio is luminance only, and some things are found by hue.** The
  light day badge is a pale gold disc on a pale grey card — 1.11:1, and plainly
  visible. That is why `badge.*` is `incidental` and the requirement it really
  owes, the glyph *on* the plate, is measured one role up. Holding the plate
  itself to 3:1 would have turned a sunny day's badge bronze.

Both schemes currently have zero red rows. Adding a token means giving it a
duty and a ground; leaving it out gets it its role's default, which is usually
what you want.

### Severity is a fourth categorical group, not a longer status scale

The five CAP grades an alert can carry — Extreme, Severe, Moderate, Minor and
**Unknown** — live in `Theme.severity`, keyed by grade the way `precip` is keyed
by type.

The tempting alternative is to stretch `state.good` / `caution` / `poor` to five.
It is wrong for the reason the paragraph above gives about published bands: the
status trio is a three-point verdict *this app computes* about a number it is
holding, and CAP's five are *somebody else's scale*, with somebody else's
meaning, which we display rather than decide. `detaildata.js` already says a
fourth status level would turn a set of named states into a scale.

Four tokens per grade, because §4.10 forbids colour-only encoding and a warning
is the worst place to break that:

| token | what it is | floor |
|---|---|---|
| `wash` | the banner plate, over `page.bg`. Alpha rises with grade | 1.2:1 vs the page |
| `edge` | the rail down the leading edge — the ECCC-recognisable red/orange/yellow | 3:1 |
| `glyph` | the severity mark, which is a different **shape** per grade | 3:1 |
| `ink` | the severity word | 4.5:1 |

Every one of those is measured against the **composited plate**, not the page,
because that is what the text is drawn on. `tests/qml/tst_theme.qml` asserts all
twenty values in both schemes — this table is keyed by data, so the gallery's
Colour page, which walks flat groups, does not see it.

Three things worth knowing before they look like mistakes:

- **`Unknown` is a real grade and it is not "probably fine".** Six of the nine
  alerts recorded in `tests/fixtures/alerts/` say Unknown, and all six are Air
  Quality Alerts. It means the issuer declined to classify. It therefore sorts
  *below* Minor, and it is still drawn — neutral, but not the quietest thing on
  the screen.
- **`Minor` is blue, not a fourth warm hue.** A ladder that stays warm all the
  way down leaves nothing for "this is not an emergency" to look like.
- **In dark, `moderate`'s plate sits 0.05 above `extreme`'s.** Amber on navy is
  brighter than red on navy; buying the ordering back would need a red too pale
  to read as red. The rail, the glyph and the word all order correctly, which is
  what a reader actually scans.

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

The one exception to the token rule is the same one §10.1 makes for colour: a
duration that belongs to **one** visualisation and could not describe anything
else stays in that file, named, with the argument beside it. `DetailWindCard`'s
drift period is the case — it is set by the wind speed, so it is a reading
rather than the length of a transition, and no token could hold it. If a second
component ever wants a number like it, that is the signal it was a token all
along.

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
  or looping. If it moves while the user is doing nothing, it is wrong — and
  "doing nothing" is the whole of the test, which is why the wind rose's drift
  under a pointer is not covered by this and the precipitation field is. This
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
- **Nothing is drawn that only makes sense at one end of the transition.** The
  day strip's tab fillet is the worked example: it is the corner between a
  selected card and the chart panel, so it has no meaning at all while the card
  is still travelling toward the panel. Given its own Behavior it reached two
  thirds of its radius 20 px short of anything to join, and a rounded wedge sat
  in the page background for the length of the animation. Both ends were
  correct, which is why it survived 48 golden images and shipped.

  The rule that follows: when several properties describe **one** change, animate
  **one** number and derive them from it. Three Behaviors are three clocks, and
  three clocks drift — the card said it was 20 px up, the fillet said the join
  was built, and both were telling the truth about themselves. If the parts have
  to arrive in an order, split the *range* of that one number rather than giving
  each part a delay: it is then the same expression run backwards that takes the
  join apart before the card lifts, and nothing has to ask which direction the
  change is going. `DayStrip.qml`'s `merge`, `landed` and `joined` are this.
- **A Behavior must not branch on the property that triggered it.**
  `duration: selected ? move : 0` is the shape to watch for. A Behavior can fire
  before the binding feeding its duration has been re-evaluated, so it holds on
  some runs and not others — the worst possible failure, since it looks correct
  every time you check it.

**One exception, and it is a real one: precipitation** (§10.11). Rain is not a
transition between two states, it is a state, and the only honest way to draw it
is moving. So the rule is kept everywhere it can be — the clock does not run
when there is no precipitation, when the chart is behind the list view, or under
`--grab`, and one clock drives the whole field however heavy the weather gets.
The golden images survive it because nothing calls `Math.random`: every drop is
seeded from its hour, so a frozen frame is a deterministic one rather than an
empty one. Anything else wanting a standing animation has to make the same
argument.

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

### The hover

The paragraph above opens on a premise that is no longer true. A detail card
does have an interaction: the reader can point at it. That is a question, and it
deserves a narrow answer.

**A card moves on hover only where the still card is silent about something the
reading itself does.** Not a replay of the arrival — the reader has seen that —
and not a flourish, and emphatically not one per card. A grid where twelve
things stir under a pointer is a grid nobody can read.

Exactly one card qualifies today. The wind rose draws a bearing and two speeds,
all three correct and all three standing still, and standing still is the one
thing wind is not: a vane on a roof tells you the air is *going* somewhere
before you have read anything off it. So on hover the wedge does what the air
does — it travels downwind, and when it has crossed the dial it starts again
from where it came in. The rate is the speed, which is the reading the still
card cannot make: 6 km/h ambles across and anything near the ceiling crosses in
a third of the time.

Two things it deliberately does not do. It does not **turn**: a bearing is a
measurement and rotating the wedge off it draws a wind that is not blowing. And
it does not ease **back**: the reset is instant, because a wedge sliding
upwind is air going the wrong way, and half of every loop spent doing that is
worse than a seam.

Everything else on the page is a level, a history or a fraction — quantities
that do not *do* anything — and a level that jiggles under a pointer is
decoration. Nine of them were given gestures once and it read as a page of
fidgets; they were taken out again the same week.

`DetailCard` provides two hooks and two rules. `hovered` is the state.
`hoverPhase` is the envelope, 0 → 1 over `Theme.motion.move`.

- **Everything a card moves on hover is multiplied by `hoverPhase`.** It is
  exactly 0 at rest, which is what keeps a resting card identical to the card
  before hover existed — what the golden images assert — and what retires a
  gesture along the shortest path from wherever it had got to when the pointer
  left. No gesture has to know how to finish itself.
- **`hoverPhase` is pinned to 0 under `Theme.stillness`.** A reader who asked
  their desktop for less movement gets nothing moving, and a capture cannot be
  caught mid-gesture. This is what keeps §10.6's ban on standing animation
  honest: nothing here runs unless someone is pointing at it, and nothing runs
  for a reader who has asked it not to.

Reviewing it needs pixels rather than a contact sheet, because `--poke` does not
carry a pointer. `tests/qml/tst_detailhover.qml` grabs the card at rest, again
mid-drift and again after the pointer leaves; it asserts the wedge translates
along the wind's own axis rather than turning, and that the other twelve cards
do not move at all.

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
`--poke` drives `metric`, `day`, `list`, `feels`, `scroll`, `picker`, `prefs`
and — in the gallery — `remount`, which rebuilds the specimen and replays
whatever it does on mount. `picker` and `prefs` are the two sheets: they are the
only surfaces in the app reachable no other way in a capture, which is what a
poke is for. `prefs` is the desktop shell's, and says so rather than doing
nothing when the mobile shell is loaded — the phone shows the same preferences
inline, and `--tab me` is how they are photographed there.

If every frame on the sheet looks identical, either nothing is animating or the
whole thing finished inside one interval.

**`--every` is a floor, not a rate.** Grabbing a frame costs more than a frame
does: asked for 8 ms between frames, this machine delivered nearer 50, so a
190 ms transition is sampled four or five times however small the number goes.
That is enough to see whether something moves and not enough to prove that
nothing goes wrong in between — the fillet defect above sat in exactly that
blind spot. Where a transition has an invariant that must hold at *every* point,
write it down as one: `tests/qml/tst_daystrip.qml` sweeps the driver directly,
with `Theme.stillness` on so the Behavior does not intercept the assignment, and
checks a hundred points in eleven milliseconds.

**`--every` has a floor of roughly 90 ms per frame offscreen**, whatever you ask
for: `grabToImage` plus a PNG encode costs more than the interval you set, so
`--every 20` and `--every 60` produce the same sheet. A `tint` at 150 ms is
therefore about two usable frames, and anything shorter cannot be resolved by
filming at all. For those, stretch the duration temporarily, film, and put it
back — or sample the pixel directly and check the curve is monotonic.

Two states are currently unreachable by `--poke`, so their transitions cannot be
filmed as they stand:

- **A pager fade on the hourly list.** `PagerButton.enabledState` there derives
  partly from a Flickable's `contentX`, which only a drag or a pager press
  changes. The chart's own pagers are reachable: they fade off `Data.selectedDay`,
  and `--poke day=` sets it.
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
  `line.track` remainder, and the mark where the paint stops.
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
  `ink.primary` ring. Not 12, not 20, and not a rule dangling below it.

### Rules for a visualisation

- **Draw the reading, not a decoration.** If the shape would look the same for
  a different value, it is not a visualisation. A dial whose arc is a fixed
  grey ring with a coloured dot on it has drawn a scale and left the reading
  to the dot; fill the traversed arc and the ring itself carries the value.
- **A gauge needs a visible track.** The unfilled remainder is `line.track` —
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

## 10.11 Precipitation

Everything else on this chart answers *how much*. Precipitation is the one
variable people mostly ask *when* about — the question is nearly always "can I
leave at four", not "how many millimetres" — and a column of millimetres answers
that only after you have read an axis. So it gets a second encoding, on top of
the bars it already has and on every metric tab: the hours it falls in are
washed and textured.

Three layers, and each carries a different piece of the reading:

| Layer | Says | Where |
|---|---|---|
| **Wash** | *when* — a tinted band edge to edge of the spell | `PrecipBands.qml`, under the series |
| **Field** | *what and how hard* — falling particles | `PrecipField.qml`, over the series |
| **Caption** | the words, for spells wide enough | `PrecipField.qml` |

### Rules

- **The wash goes under the series; the field goes over it.** This is why they
  are two components rather than one, and it is not a layering preference. A
  fill's colour on this chart *is* its value (§10.5), and on the banded metrics
  those colours are a published scale — a blue wash over a UV bar would be
  stating a different number. Underneath, the wash still reads, because every
  fill here is translucent. The reference does the same thing and it is
  measurable in a capture: its rainy stretch shifts the empty plot by about
  three times what it shifts the area fill.
- **An hour is an interval, not an instant.** Providers report the amount for
  the hour *starting* at a timestamp, so hour *i* occupies `[i, i+1)` and the
  wash is drawn on that. Centring it on the sample would claim rain for the half
  hour before it starts — which is the half hour someone is deciding in.
- **Type picks the hue, intensity picks the alpha, and the ladder is narrow**
  (0.13 / 0.20 / 0.27). "Is it raining here" has to read the same at every
  level; how hard it is raining is the field's job, and the field has far more
  range to say it with.
- **Spells split on type, not on intensity.** Rain easing off is still the same
  rain; cutting the band at every step change would draw four events where there
  is one. The spell is labelled by its peak, because "heavy rain, 2 to 6" is
  what you would say out loud about a spell with one heavy hour in it.
- **Both edges, always.** A wash says roughly when. A line on its first and last
  minute says exactly when, and exactly when is the entire point.
- **Six named levels are six points on a continuum.** `precip.js` crosses a type
  with an intensity and scales one particle model — count, size, speed. There is
  no switch statement with six pictures in it to keep in sync, which is also how
  sleet, hail and thunderstorms came for free.
- **Nothing calls `Math.random`.** Every drop is a hash of its hour and its index
  in that hour, so the same forecast draws the same rain on every run and
  `--grab` is still a golden image. Two consecutive grabs are byte-identical;
  that is the check.
- **A storm deepens in light mode; it cannot flash.** `precip.flash` is a
  brightening on a night sky, and there is nothing brighter than a near-white
  page to brighten it towards — so the light theme's flash is a *darkening*
  (`#2a3f66`) and the storm band gets heavier rather than whiter for an instant.
  The general form of this is worth keeping in mind for any effect defined as
  "add light": under an inverted ladder it has to be redefined as "add
  contrast", and the direction is a property of the theme.
- **The loop wraps seamlessly or it is a glitch you will blame on something
  else.** One clock runs `0 → LOOP` and every particle's progress is
  `(clock × rate + offset) mod 1`, which is only continuous across the wrap
  where `LOOP × rate` is whole — so `precip.js` quantises every rate to `LOOP`
  steps. Without that the entire field jumps at once, once a minute.

### Notes

- **Rectangles, not Shapes.** A streak is a thin rounded rectangle, a flake a
  round one, a splash four of them. Shapes escape ancestor clipping (§10.8) and
  would each need a layer to bound; rectangles are batched, and the Flickable
  that already clips the chart clips them too.
- **`antialiasing: true` is not optional at this size.** Qt only antialiases a
  rounded rectangle when asked, and every snowflake on the chart is a little
  square without it.
- **The cost is proportional to the weather, not to the chart.** Particles are
  generated per wet hour, so a dry forecast draws nothing. The mock's four
  spells came to 54 drops and 20 splashes across the forty-eight-hour window the
  chart drew then; it draws one day now, and the arithmetic is the same per hour.
- **A splash is four rectangles and the arrangement matters.** Puddle line,
  rebound jet, and two droplets thrown clear of it. Left at the jet's own height
  and leaning the same few degrees, the three resolve into one downward arrow —
  a rendering that says the opposite of what a splash is. This is only visible
  at 4× on a capture, which is where it was found.

## 10.12 Viewports

`Viewports.qml` is the single source of truth for what counts as a phone. Both
the app and the component gallery read it, and they must not disagree: a gallery
that frames a component at 390 px while the app switches shells at 420 is
reviewing a layout the app never renders.

| Class | From | Shell |
|---|---|---|
| `mobile` | 0 | `MobileShell` — five tabs under a bottom nav, one column |
| `tablet` | 600 | `MobileShell`, two columns past 720 px of content, a rail in landscape |
| `desktop` | 1024 | `WeatherPage` — four sections in one scrolling column |

**There is no mobile build.** The window width picks the shell and nothing else
in the app knows which one is running. `--viewport <id>` pins a class and
resizes to match; it is a review convenience, not a mode.

**Tablet is not a third layout**, and that survived the tablet work. The five
destinations, the cards and the pages are the phone's; what a tablet has is more
room, and what the room buys is answered by two functions beside
`usesMobileShell()` rather than by it:

| Question | Function | Answer |
|---|---|---|
| how many columns | `contentColumns(cls, usableWidth)` | 2 for a tablet with ≥ 720 px of content, else 1 |
| where navigation lives | `navStyle(cls, w, h)` | `rail` for a tablet ≥ 900 px and wider than tall, else `bottom` |

Keeping them separate is the point: `usesMobileShell` means "the five-tab
shell", and folding either arrangement into it would make the answer to *which
shell* depend on how wide the window happened to be when a page was loaded.

**Width cannot always answer.** A tablet held in landscape is 1112 px across,
which is past the desktop threshold, and it is not a desktop; a desktop window
dragged to 1112 px is. Same number, two answers, nothing in the geometry to
separate them. So `classOf` answers the question width *can* answer and two
callers override it — `--viewport tablet-landscape` in review, and the platform
at run time, where a handheld is never a desktop whatever its width. The
`tablet-landscape` preset is marked `pinned` for exactly this reason and is the
only one that is.

### Spans, not a masonry pass

A section states its width in columns — `root.spanWidth(1)` or `spanWidth(2)` —
and `MobilePage` lays them out in a `Flow`. A full-width child takes a row to
itself and two half-width children share one, which is the two-column
arrangement with no code to arrange it; at one column every child is full width
and a `Flow` *is* a `Column`, which is what let the phone's eight golden images
stay byte for byte what they were.

The hero, the hourly strip, the ten-day strip, the calendar, the map and the
alert banner are always `span: 2`. Prose is always `span: 1` — the hourly
screen's daily summary is half width on a tablet and leaves the right column
empty, because the alternative is a 95-character measure, which is half again
the widest line typography has ever called comfortable.

What this is not is a masonry pass over `children` at resize time. §10.6 rules
that out for the reason it always does: a layout computed from whatever happens
to be in it is a layout nobody can predict from reading the file.

### What changes on a phone, and what does not

Everything in §10.1 through §10.9 applies unchanged. Surfaces are still washes,
durations are still tokens, nothing still animates on a timer. What the phone
gets is a different *arrangement*, and each difference has to be a consequence of
the width rather than a restyling. The five that exist:

- **The page splits into tabs.** Scrolling is not navigation. The desktop column
  at 390 px is roughly eight screens deep.
- **The hero loses its card.** It sits on the page gradient. On the desktop a
  wash separates it from the three sections around it; at the top of a phone
  screen there is nothing above it to be separated from.
- **A control that does not fit becomes a disclosure.** Ten metric pills become
  one button and a list. The test is whether the row still shows every option at
  rest — a horizontally scrolling row of controls above a horizontally scrolling
  chart is two things that move sideways under the same thumb.
- **A grid re-columns to a count that divides its content.** Six slugs go 3 × 2,
  never 5 + 1. The calendar goes to four columns because a forecast is scanned
  for warm stretches rather than looked up by weekday.
- **Chrome that floats over content may be opaque.** The bottom nav and the
  metric menu use `surface.nav` and `surface.menu` rather than a 0.07 wash, and the nav
  carries a hairline along its top edge. This is the same exception the pager
  buttons already had: §10.1's ban on borders is about seams a junction exists
  to hide, and this is not a junction — it is the edge where a floating bar
  stops and scrolling content begins, and it is the only cue that the content
  continues behind it.

### 44 px, and what it applies to

`Theme.metric.hitMin` is 44. Every platform guideline that has measured a
fingertip lands within a few pixels of the same number — Apple says 44, Google
says 48 dp, WCAG 2.2's enhanced target size is 44 — because the thing being
measured is a contact patch of about 8 mm and not a design opinion.

**It applies to the target and never to the mark.** A 44 px dismiss cross is a
shape shouting at the reader. `TouchTarget.qml` is an invisible area that
centres on its parent and grows to the floor in whichever direction the parent
is short of it, so raising the floor moves nothing on screen. Where a control's
size genuinely *is* its affordance — a settings row, a menu item, a collapsed
alert strip, a button — the control grows instead, because a bigger surface to
aim at is a better one.

Neighbours are the case neither answer solves. Two marks 29 px apart cannot both
have a 44 px target, and growing them anyway means each steals half the other's
taps — which is worse than leaving both small. The answer there is to move them
apart, and `LocationBar` is where it came up: its disclosure chevron and its
home toggle are two different actions that sat 10 px apart with a 14 px target
and a 24 px one.

The audit is `tests/qml/tst_hittargets.qml`, which measures every tappable area
on every screen the mobile shell reaches, and the gallery's **Touch targets**
toggle, which draws them. The first run of the overlay found nine, including a
14 × 14 disclosure chevron on every phone screen in the app. See
`docs/known-gaps.md` for what it deliberately does not measure.

### Two more surfaces to defend

The temptations here are specific enough to name:

- **A lighter strip behind a row of hour labels.** The reference tints it. Inside
  a card that is 0.07 over 0.07 — the stacked wash §10.1 exists to prevent. A
  hairline separates an axis from a plot without claiming to be a surface.
- **A raised cell for today in the calendar.** Same arithmetic, 0.165. This is
  the case §10.1's note about borders allows for: a 1 px accent outline and the
  date in `accent.fill`, because nothing else will do.

### The sky

`PageBackdrop` takes a `phase` — `night`, `dawn`, `day` or `dusk` — and paints
five gradient stops from `Theme.sky`. The phone follows the clock; the desktop is
`dusk` permanently, which is the palette the prototype has always had, and its
render is unchanged to the byte.

Two rules, and both are the general rules applied here rather than new ones:

- **Every phase is dark — within a theme.** Surfaces are white washes at
  0.05–0.10 and a wash is only a surface if something darker is behind it, so a
  literal daylight sky would make every card on every screen invisible at once.
  The phases differ in hue and clarity, not lightness. The surface ladder
  decides the background, not the other way round.

  The light theme is the same rule read the other way, and it is worth stating
  because it looks like an exception and is not. There the ladder is *black*
  washes, so every one of the four phases is bright — and all four keep their
  hue relationships, dawn still warmer than night. What the rule actually says
  is that the sky and the ladder must agree on which way round the contrast
  runs; it never said which way that is.

- **In light mode there are no stars.** `stars` is 0.00 in all four phases, so
  the 130 Rectangles and 12 Shapes are never instantiated and the light theme
  is cheaper to draw than the dark one. This is not a performance trick: a star
  is a bright point on a dark ground, and the same pinpoint on a pale sky is a
  speck of dirt on the screen.
- **Nothing in it moves.** §10.6's one standing exception is precipitation
  (§10.11), which is weather rather than decoration, and a twinkling star field
  is the worst possible place to make a second: it would run forever, behind
  every screen, while the reader
  is trying to read a number off a chart — and it would make every golden image
  of every mobile screen a coin toss. `sky.js` therefore contains no
  `Math.random`, in positions, sizes or brightness.

The stars do not scroll with the page either. That would be motion tied to a
scroll rather than to a timer, which is a different rule — but it would still put
drifting pinpoints behind a chart being read.

## 10.13 Reviewing at a width

The gallery stages any component inside a device-sized frame: **Free**, **Mobile**,
**Tablet**, **Desktop**, from the rail or from `--viewport <id>` alongside
`--gallery`. The page gradient — and on a phone frame, the sky — is painted
*inside* the frame, because a specimen composited over the wrong slice of a
window-sized gradient is a specimen reviewed on a background it never gets.

What a specimen is given depends on what the catalogue declares:

| Entry declares | In a frame it gets |
|---|---|
| `fills: true` | the whole device — screens and shells |
| `stage: { w, … }` | the width that viewport's shell would hand it |
| neither | its natural size, centred, inset by the page margin |

A stage width was only ever a stand-in for a host that was not there, so when a
frame is there the frame wins.

**Review every new component at the narrowest class it will run in.** Almost
every layout defect this prototype has had was a component that was fine at the
width its author happened to try and wrong at the width the app gives it — the
hour glyphs escaping the panel's clip were found exactly that way. A frame makes
that width something you choose rather than something you inherit.
