<!-- SPDX-FileCopyrightText: 2026 Jowi Aoun -->
<!-- SPDX-License-Identifier: CC-BY-SA-4.0 -->

# 03 — Tech Stack and Key Decisions

## 3.0 First, a correction to the premise

> "It will be written in C++ (I think that's what Qt 6 allows for, correct me if wrong)."

Almost right, with an important nuance:

- Qt 6's own implementation language is **C++17/20**, and C++ is the first-class API.
- But **Qt 6 UIs are not normally written in C++.** The modern path is **QML** — a
  declarative, JavaScript-flavoured, memory-safe UI language — with C++ underneath for
  logic, networking, models and performance-critical rendering. Qt's own documentation
  calls this the "hybrid application" model, and it exists precisely because C++ is a
  poor language for expressing animated, fluid, data-bound UIs.
- Qt 6 also has **official Python bindings (Qt for Python / PySide6, currently 6.11)** and
  community bindings for Rust (CXX-Qt), Go, C#, etc.

So the answer to "is it C++?" is: **C++20 for the engine, QML for the interface.** That
split is not a compromise — it is what makes a weather app with 60 fps charts and
animated icons tractable.

### Why not the alternatives

| Option | Verdict for this project |
|---|---|
| **Qt 6, C++20 + QML** | ✅ **Chosen.** GPU-accelerated custom drawing (Qt Quick Shapes / scene graph), single codebase for Linux+Windows+macOS+Android+iOS, MapLibre Native integration proven by Supercell Wx, no runtime to bundle, small memory footprint. |
| Qt 6 + PySide6 (Python) | ❌ Ships a Python runtime, slower startup, painful single-file distribution, and Mousam already demonstrates the ceiling of the Python+GTK approach. |
| GTK4 / libadwaita | ❌ Linux-first by design; Windows/macOS support exists but is second-class and looks alien. Contradicts "cross-platform". |
| Flutter | ⭕ Good rendering, but Linux desktop support is the weakest target, no native look anywhere, Dart ecosystem for weather/geo is thin. |
| Avalonia / .NET | ⭕ Genuinely cross-platform, but bundles the .NET runtime and has a small Linux desktop community. |
| Electron / Tauri | ❌ Contradicts "native"; charts would be great, memory and startup would not. This is the thing Linux users complain about. |
| Rust + CXX-Qt / Slint | ⭕ Attractive long term; today it adds binding risk on top of an already ambitious scope. |

---

## 3.1 The licensing trap — Qt module licence matrix

**This is the most consequential finding of the research.** Qt's essential modules are
LGPLv3, but several add-ons — including *both charting modules* — are **GPLv3-only**.
Choosing them silently forces our whole app to GPLv3 and permanently closes the iOS App
Store (and complicates the Mac App Store).

| Module | Open-source licence | Verified | Use in Clima |
|---|---|---|---|
| Qt Core, GUI, Network, QML, Quick, Quick Controls, Quick Layouts, Widgets, Test, Concurrent, D-Bus, Sql | **LGPLv3 / GPLv2+** | ✅ | ✅ Core of the app |
| **Qt SVG** | **LGPLv3 / GPLv2** | ✅ quoted | ✅ Icon rendering |
| **Qt Positioning** | **LGPLv3 / GPLv2** | ✅ quoted | ✅ "My location" |
| **Qt Location** | **LGPLv3 / GPLv2** | ✅ quoted | ⭕ Map plugin host (tech-preview status) |
| **Qt Charts** | **GPLv3 only** | ✅ | ❌ **Avoid.** Also deprecated in favour of Qt Graphs |
| **Qt Graphs** | **GPLv3 only** | ✅ | ❌ **Avoid** |
| **Qt Lottie Animation** | **GPLv3 only** | ✅ | ❌ **Avoid** (and it renders via QPainter software path anyway) |
| **Qt Quick 3D** | **GPLv3 only** | ✅ quoted | ❌ Not needed |
| Qt Virtual Keyboard | GPLv3 only | ✅ | ❌ Not needed |
| Qt WebEngine | LGPLv3 + Chromium third-party | — | ❌ Not needed; huge |
| Qt Multimedia | LGPLv3 | — | ⭕ Only if we add audible alerts |

### LGPLv3 compliance checklist (obligations we must actually meet)

- **Link Qt dynamically.** Static linking can pull the application itself under LGPL.
- Ship the **complete corresponding source of the Qt libraries used, including any of our
  patches**, or a written offer — hosted *by us*. A link to qt.io is **not sufficient**.
- Provide **relink/installation information** so a user can substitute their own build of
  Qt and still run Clima. (Practically: dynamic linking + documented build instructions +
  no library pinning tricks.)
- Ship the **LGPLv3 licence text** and a prominent in-app notice ("About → Licences").
- Non-compliance terminates our distribution rights, so treat this as CI-enforced, not
  best-effort: a `licences/` directory generated at build time.

## 3.2 Decision log

### D1 — Qt 6 + C++20 core + QML UI
**Accepted.** See §3.0.

### D2 — Minimum Qt version: 6.8 LTS; develop against 6.9+
Rationale: Qt 6.8 is the LTS and is what Debian 13 and the current `org.kde.Platform`
Flatpak runtime ship; requiring 6.10+ would exclude most distro builds. Newer features
(animated SVG in `VectorImage` from 6.10, GPU 2D rendering improvements and box
gradients/shadows in 6.11, Qt OpenAPI in 6.11) are used behind version guards only.
Because Flatpak is our primary Linux channel, users get a modern Qt regardless.

### D3 — Build our own chart kit ("ClimaCharts"). Do not use Qt Charts or Qt Graphs
Three independent reasons:
1. **Licence** — both are GPLv3-only (§3.1).
2. **Design** — MSN-class weather charts (temperature bands with day/night shading,
   precipitation bars overlaid with probability, wind barbs, sun arcs, decade-over-decade
   history) are not "a line chart with a legend". Generic charting libraries fight you.
3. **Performance** — a `QQuickItem` subclass writing `QSGGeometryNode`s in
   `updatePaintNode()` renders on the scene-graph render thread with no per-frame
   allocation. That is faster than any general-purpose chart widget and is the documented
   Qt way to do custom visuals.

Implementation: `QSGGeometryNode` for lines/areas/bars; `Qt Quick Shapes` (part of
qtdeclarative, LGPL) for curved paths and gradients; QML `Text` for labels; animation via
QML property animations. Keep geometry as a member of the node subclass to avoid
reallocation.

### D4 — Maps: MapLibre Native Qt
`maplibre-native-qt` provides both raw Qt bindings (`QMapLibre::Core`, `::Widgets`) and a
**Qt Location geoservices plugin**, supports Qt 6.5+, and renders vector tiles via OpenGL
into a Qt Quick framebuffer object. It is BSD-licensed and **already proven on Linux by
Supercell Wx**, which is the closest technical precedent to what we want to build.
Qt Location's own `Map` type is thin (no gesture area, MapObjects removed, still marked
technology preview), so we depend on MapLibre for the real work and treat Qt Location as
optional glue.

### D5 — Provider abstraction from commit one
Interfaces (`IForecastProvider`, `IAirQualityProvider`, `IAlertProvider`, `IRadarProvider`,
`IGeocodeProvider`) with a region-aware registry. Non-negotiable, because:
- Mousam's documented failure mode is "one API down → app shows nothing".
- Radar and alerts have **no single global source** (§2.4, §2.5).
- It gives us the model-comparison feature for free.

### D6 — Licensing of our own code: GPL-3.0-or-later app, MPL-2.0 core
| Component | Licence | Why |
|---|---|---|
| `libclima` (engine: providers, models, cache, units) | **MPL-2.0** | File-level copyleft. Reusable by a Plasma applet, a GNOME extension, a CLI, or a future App Store build. Keeps the iOS door open. |
| `clima` (QML UI, app shell) | **GPL-3.0-or-later** | Standard for Linux desktop apps; strong copyleft where it matters. |
| Assets we author (icons, styles) | **CC-BY-SA-4.0** | |

This mirrors Vremenar's GPL-3.0/MPL-2.0 dual approach. **This decision is cheap now and
very expensive later** (relicensing needs every contributor's consent), so it is flagged in
[`08-risks.md`](08-risks.md) as needing your sign-off before M1. Note that GPLv3 is
incompatible with the Apple App Store's terms; keeping the engine MPL-2.0 and avoiding
GPL-only Qt modules (D3) is what preserves that option.
Require a **DCO sign-off** (`Signed-off-by:`) on contributions, not a CLA.

### D7 — No mandatory backend service
See §2.8. Per-user API calls stay far under Open-Meteo's 10 000/day non-commercial limit,
so the app works with zero infrastructure and zero telemetry. `clima-relay` stays an
optional, self-hostable component for radar/alert normalisation.

### D8 — Build system: CMake ≥ 3.21, minimal vendored dependencies
Qt6 CMake API, `qt_add_qml_module` for QML type registration, `FetchContent` only where a
system package is unavailable. Avoid Conan/vcpkg as a hard requirement so distro packagers
can build with system libraries — a hard requirement for getting into Fedora/Debian/AUR.

### D9 — Bespoke design system, platform-adaptive
Do not adopt one Qt Quick Controls style per platform (FluentWinUI3 / macOS / Basic) for the
main surfaces — the app is mostly custom cards and charts, and per-platform styles would
fragment the design. Instead: one **Clima design system** (tokens for colour, type, spacing,
elevation, motion) that *reads* platform signals — system dark/light, accent colour,
reduced-motion, font DPI — and uses native styles only for standard dialogs and the
settings surface. FluentWinUI3 remains available for Windows chrome; note it is still
under development with several unsupported controls.

The **typeface** is the one platform signal we deliberately do not read by default: the app
bundles Inter and installs it as the application font, because "whatever the host picks"
means different metrics, different wrap points and a layout that only fits on the machine it
was designed on — and golden images that cannot be compared across machines at all. Font DPI
is still the platform's to decide, and `Theme.type.family` is a live read of the application
font rather than a constant, so a settings toggle can hand the choice back.

### D10 — Icons: Meteocons (MIT), converted to QML at build time
Meteocons ships 200+ hand-crafted animated weather icons under **MIT**, as animated SVG
and Lottie JSON, in fill/flat/line/monochrome styles. We consume the **SVG** variants and
convert them with `svgtoqml` (ships with qtdeclarative, LGPL) into Qt Quick Shapes for
scalable, GPU-accelerated rendering. We deliberately do **not** use the Lottie path,
because `vectorimageformats`/Qt Lottie is GPLv3-only (§3.1). Weather Icons (SIL OFL 1.1)
is a backup for glyph-style needs.

## 3.3 Dependency table

| Dependency | Version | Licence | Purpose | Bundled? |
|---|---|---|---|---|
| Qt 6 | ≥ 6.8 LTS | LGPLv3 | Framework | System / Flatpak runtime; dynamic link |
| maplibre-native-qt | ≥ 3.x | BSD-2 | Map rendering | Vendored via FetchContent (upstream packaging is thin) |
| SQLite | via Qt Sql or system | Public domain | Cache + history store | System |
| Meteocons | 2.1+ | MIT | Weather icons | Vendored, converted at build |
| Inter | 4.1 | OFL-1.1 | UI typeface | Vendored, Regular + Bold, in the binary |
| Catch2 or Qt Test | — | BSL-1.0 / LGPL | Unit tests | Test-only |
| `libcap` CAP parsing | — | — | Use Qt XML — **no new dependency** | — |

Deliberate non-dependencies: no Boost, no protobuf (JSON is enough at our volumes), no
Qt WebEngine, no charting library, no Python.

## 3.4 Performance targets (a Linux-native app must earn its "native" claim)

| Metric | Target | Rationale |
|---|---|---|
| Cold start to first paint | < 400 ms | Beat every Electron weather app decisively |
| Cached data on screen | < 100 ms after paint | Offline-first means the UI never waits on network |
| Idle RSS | < 120 MB without map, < 250 MB with map open | GTK apps sit around 100–150 MB |
| Chart scrub / pan | 60 fps (120 fps where display allows) | This is the whole reason we chose the scene graph |
| Idle CPU | ~0 % | No polling timers when window is unfocused/hidden |
| Binary size (Linux, dynamic Qt) | < 15 MB | |

## Sources

- [QML and C++ Integration overview](https://doc.qt.io/qt-6/qtqml-cppintegration-overview.html) · [Qt Languages](https://doc.qt.io/qt-6/qtlanguages.html) · [Qt 6 modules](https://doc.qt.io/qt-6/qtmodules.html)
- [Qt Licensing](https://doc.qt.io/qt-6/licensing.html) · [Obligations of the GPL and LGPL](https://www.qt.io/development/open-source-lgpl-obligations) · [Qt open-source licensing FAQ](https://www.qt.io/faq/qt-open-source-licensing)
- Module licence pages: [Qt Graphs](https://doc.qt.io/qt-6/qtgraphs-index.html) · [Qt Charts](https://doc.qt.io/qt-6/qtcharts-index.html) · [Qt Lottie](https://doc.qt.io/qt-6/qtlottieanimation-index.html) · [Qt Quick 3D](https://doc.qt.io/qt-6/qtquick3d-index.html) · [Qt Location](https://doc.qt.io/qt-6/qtlocation-index.html) · [Qt Positioning](https://doc.qt.io/qt-6/qtpositioning-index.html) · [Qt SVG](https://doc.qt.io/qt-6/qtsvg-index.html)
- [Qt 6.11 released](https://www.qt.io/blog/qt-6.11-released) · [Qt 6.11.1 released](https://www.qt.io/blog/qt-6.11.1-released) · [Qt for Python 6.11](https://www.qt.io/blog/qt-for-python-release-6.11-is-out)
- [Scene Graph — Custom Geometry example](https://doc.qt.io/qt-6/qtquick-scenegraph-customgeometry-example.html) · [Efficient custom shapes in Qt Quick — KDAB](https://www.kdab.com/efficient-custom-shapes-in-qt-quick/)
- [Vector Image Formats in Qt](https://doc.qt.io/qt-6/topics-vectorimageformats.html) · [Animated Vector Graphics in Qt 6.10](https://www.qt.io/blog/animated-vector-graphics-in-qt-6.10) · [VectorImage QML type](https://doc.qt.io/qt-6/qml-qtquick-vectorimage-vectorimage.html)
- [maplibre-native-qt](https://github.com/maplibre/maplibre-native-qt) · [build docs](https://maplibre.org/maplibre-native-qt/docs/md_docs_2Building.html)
- [Styling Qt Quick Controls](https://doc.qt.io/qt-6/qtquickcontrols-styles.html) · [FluentWinUI3 style](https://doc.qt.io/qt-6/qtquickcontrols-fluentwinui3.html)
- [GPLv3 vs App Store — HN discussion](https://news.ycombinator.com/item?id=10896773) · [Apple developer forum: Qt GPL on the App Store](https://developer.apple.com/forums/thread/4783)
