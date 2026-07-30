<!-- SPDX-License-Identifier: CC-BY-SA-4.0 -->

# Prototype — Hourly Overview chart

A Qt Quick rebuild of MSN Weather's **Hourly → Overview** chart, the single most-liked
thing in that app. Nothing else from the MSN screen is here: no day cards, no metric tab
bar, no location header. Just the chart.

![The prototype](screenshot.png)

## Run it

No build step. It is pure QML, executed by Qt 6's `qml` runtime.

```sh
./run.sh                     # open the window
./run.sh --grab shot.png     # render one frame headless and exit
```

`run.sh` finds a `qml` binary on `PATH`, or falls back to the newest one in the Nix store,
and wires up `QML_IMPORT_PATH` / `QT_PLUGIN_PATH` when the binary comes straight out of
`/nix/store` (those builds are not env-wrapped, so they cannot otherwise find their own
modules). Overrides:

```sh
CLIMA_QML=/path/to/qml ./run.sh     # a specific Qt
QT_QPA_PLATFORM=xcb ./run.sh        # force X11 if Wayland misbehaves
```

Verified on Qt **6.11.1**, both offscreen (software) and Wayland (GPU). Requires
`QtQuick` and `QtQuick.Shapes` only — no Qt Charts, no Qt Graphs, no Qt Lottie, so
nothing here pulls in a GPL-only module (see `docs/03-tech-stack.md` §3.1).

## What it does

| | |
|---|---|
| **Temperature curve** | Catmull-Rom spline with control-point clamping, so it is monotone between samples and never invents a dip the data does not contain |
| **Gradient fill** | Vertical gradient over the *value* axis — colour encodes absolute temperature, so 19° reads green and 27° reads tan at the same place on the same chart. Alpha falls off toward the bottom so gridlines stay legible through the fill |
| **Gradient curve** | `ShapePath` can gradient-*fill* but not gradient-*stroke*, so the line is a thin closed ribbon around the curve, filled with the same ramp |
| **Hour header** | Label + procedural condition icon + temperature, every 2 h, scrolling in lockstep with the plot |
| **The past** | Veiled and hatched *over* the curve. Observed hours are real data so they stay visible, but they are visibly not forecast. (This is what those faint marks in the MSN screenshot are.) |
| **Precipitation strip** | One cell per label interval, value = bucket maximum, hatched before "now" |
| **Sun events** | Sunrise/sunset markers at fractional hour positions |
| **Feels like** | Toggling *morphs* the curve — the path is regenerated from interpolated points each frame, so it is a genuine tween rather than a crossfade. Header values and the legend follow |
| **Interaction** | Horizontal flick + drag, animated pager buttons that disable at the bounds, hover crosshair with a time/temp/precip readout |
| **Headless capture** | `--grab` renders one frame to a PNG — for design review now, golden-image tests later |

## Data

`mockdata.js` stands in for the Open-Meteo provider. Its shape deliberately mirrors what
`libclima`'s forecast provider will return: parallel per-hour arrays plus derived helpers,
with no formatting decisions baked in. The values at the labelled hours are tuned to match
the MSN screenshot exactly (19, 19, 18, 18, 18, 19, 22, 24, 25, 26, 25, 23 and precipitation
18, 18, 20, 10, 6, 4, 9, 22, 30, 13) so the two can be compared side by side.

## Deliberately not done yet

- **Real data.** No network layer; that is milestone M1.
- **Shipped icons.** Icons are drawn procedurally to keep this a single `qml` invocation.
  Production uses Meteocons (MIT) converted with `svgtoqml` — which ships with
  qtdeclarative and is present in this Qt, so the pipeline is confirmed available.
- **Observed-precipitation streaks** inside the past region. MSN draws these; here the
  hatch carries that meaning on its own.
- **C++ scene-graph rendering.** Paths are generated in JS and handed to `PathSvg`, because
  QML's declarative path elements cannot be produced by a `Repeater`. Decision D3 in
  `docs/03-tech-stack.md` moves the curve to a `QQuickItem` writing `QSGGeometryNode`s once
  there is a perf reason; `chartmath.js` is the maths that will port.
- **Accessibility.** No screen-reader data table, no keyboard scrubbing. Both are M2
  requirements and neither is retrofittable for free — do not let this prototype set the
  precedent.

## Notes for whoever picks this up

- `font.pixelSize` is an **int** in Qt. Assigning `12.5` fails object creation, and Qt
  reports it only as `Type X unavailable` from the *parent* file.
- Qt suppresses QML error output entirely when it decides stderr has no console.
  `QT_FORCE_STDERR_LOGGING=1` (which `run.sh` sets) is the difference between a useful
  error and the useless `qml: Did not load any objects, exiting.`
- `Shape.CurveRenderer` gives visibly better antialiasing on the GPU and falls back
  cleanly under the software backend, so it is safe to leave on.
