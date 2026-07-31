<!-- SPDX-FileCopyrightText: 2026 Jowi Aoun -->
<!-- SPDX-License-Identifier: CC-BY-SA-4.0 -->

# 06 — Roadmap

**Planning baseline: 2026-07-30.** Calendar dates assume one focused developer working
steadily; halve the elapsed time with two. Effort is given in person-weeks so you can
re-derive dates for whatever the real capacity turns out to be.

Every milestone ends with a **shippable artefact**. There is no milestone whose output is
"internal refactor" — that is how ambitious side projects die.

## 6.1 Milestone overview

| # | Milestone | Effort | Target | Shippable artefact |
|---|---|---|---|---|
| **M0** | Foundations | 2 w | 2026-08-13 | Repo that builds on 3 OSes in CI, empty window, licence hygiene green |
| **M1** | Vertical slice | 4 w | 2026-09-10 | **Alpha 0.1** — real current conditions for a searched location, offline cache, attribution screen |
| **M2** | Full forecast surface | 5 w | 2026-10-15 | **Alpha 0.2** — hourly/16-day/day-drill-down, ClimaCharts, units, a11y |
| **M3** | Radar map | 5 w | 2026-11-19 | **Beta 0.3** — animated radar over a vector basemap, region-routed |
| **M4** | Alerts · AQI · CLI | 5 w | 2026-12-24 | **Beta 0.4** — CAP alerts + notifications, air quality, `clima-cli` |
| **M5** | Trust layer + history + map layers | 7 w | 2027-02-11 | **Beta 0.5** — model comparison, ensemble fans, 30-year history, overlay layers |
| **M6** | Desktop citizenship | 5 w | 2027-03-18 | **RC 0.9** — Plasma applet, GNOME extension, tray, exports, life index |
| **M7** | Hardening + launch | 5 w | 2027-04-22 | **1.0** on Flathub, winget, Homebrew, AppImage, AUR |
| — | Post-1.0 | — | — | Satellite, marine/flood, climate projections, Android, iOS |

Total to 1.0: **≈38 weeks / 9 months**.

---

## 6.2 M0 — Foundations (2 weeks → 2026-08-13)

**Goal:** make every subsequent week cheap. Get the boring, high-leverage infrastructure
right before there is any code to migrate.

Deliverables
- CMake ≥ 3.21 skeleton: `libclima`, `app`, `cli`, `tests`. Qt 6.8 minimum.
- CI matrix green: Ubuntu 24.04 (system Qt + aqtinstall), Windows (MSVC), macOS (arm64).
- `libclima` shell with the five provider interfaces and the registry, no implementations.
- `HttpClient` with User-Agent policy, backoff+jitter, request coalescing — plus tests.
- SQLite `CacheStore` with schema v1 and a migration harness.
- **REUSE/SPDX compliance from commit one**: `LICENSES/`, per-file SPDX headers,
  `reuse lint` in CI, generated third-party licence bundle.
- Contributing docs: DCO sign-off, `clang-format`, `clang-tidy`, commit convention.
- Decide and record the licence split (D6) — see [`08-risks.md`](08-risks.md) Q1.

Exit criteria
- `cmake --build` succeeds on all three OSes from a clean checkout.
- `ctest` runs (even if only HttpClient/cache tests exist).
- `reuse lint` passes.
- An empty Qt Quick window opens on all three platforms.

## 6.3 M1 — Vertical slice (4 weeks → 2026-09-10) · **Alpha 0.1**

**Goal:** one thin slice through every layer, so the architecture is proven before it is
load-bearing.

Deliverables
- `OpenMeteoForecastProvider`: current conditions + basic daily, with golden-file tests.
- `OpenMeteoGeocodeProvider`: search-as-you-type, postcodes, localised names, debounced.
- `MetNoForecastProvider` as the fallback — **built now**, not later, so the fallback path is
  exercised from the beginning (this is the bug Mousam has).
- Domain model: `Observation`, `DailyPoint`, `Place`, WMO code → localised condition text.
- Offline-first loop: render cache → revalidate → reconcile, with a visible "updated N min
  ago" affordance.
- Design system v1: colour/type/spacing/motion tokens, dark+light, system accent pickup.
- Meteocons → QML pipeline via `svgtoqml`, with day/night variants.
- **About → Data sources & Licences** screen (licence obligation, §2.9).
- Multiple saved locations with tabs.

Exit criteria
- Search "Kigali", get today's real weather, kill the network, restart, still see it.
- Force Open-Meteo to fail → app transparently serves MET Norway.
- Cold start to first paint < 400 ms on a mid-range Linux laptop.

## 6.4 M2 — Full forecast surface (5 weeks → 2026-10-15) · **Alpha 0.2**

**Goal:** MSN core-forecast parity (§5.1).

Deliverables
- Hourly view, 16-day view, day drill-down; `minutely_15` nowcast ribbon where available.
- **ClimaCharts v1**: `TemperatureBand`, `PrecipBars`, `NowcastRibbon`, `SunArc`,
  `WindRose`, shared `AxisModel` + synchronised `Crosshair`.
- Full variable set: pressure + trend, dew point, visibility, cloud layers, gusts, UV.
- Moon phase and twilight, computed locally.
- Per-quantity unit system + settings surface.
- Qt Positioning "my location" via GeoClue2 / Windows Location / CoreLocation.
- Accessibility pass: keyboard scrubbing, screen-reader data tables, reduced motion,
  colour-blind-safe palettes.
- i18n scaffolding (`.ts` catalogues, Weblate wired up).

Exit criteria
- Side-by-side screenshot review against MSN: every §5.1 row ✅ except alerts.
- 60 fps while scrubbing a 384-point hourly chart; golden-image chart tests in CI.
- Startup and RSS budgets from §3.4 asserted in CI.

## 6.5 M3 — Radar map (5 weeks → 2026-11-19) · **Beta 0.3**

**Goal:** close the loudest documented gap on Linux. This is the milestone that earns
attention.

Deliverables
- `maplibre-native-qt` integrated; vector basemap from OpenFreeMap with a custom
  desaturated light/dark style shipped as a resource (works offline from tile cache).
- `IRadarProvider` implementations: LibreWXR (global default), IEM WMS-T (US),
  ECCC GeoMet (Canada) — selected by bounding box, hidden where coverage is absent.
- Animated timeline: past 2 h + nowcast frames, scrubber, 8 fps playback, ±2 frame prefetch.
- Layer switcher tab strip; map opens on the active location.
- Size-capped LRU tile cache.
- **Explicit RainViewer decision**: not bundled as default (personal/educational licence);
  optional opt-in with an in-app explanation.

Exit criteria
- Animated radar renders smoothly on Wayland and X11, Windows, and macOS.
- Coverage-absent regions degrade to "radar unavailable here", never to a blank tile grid.
- Attribution for basemap (OSM/ODbL) and each radar source displayed on the map.

## 6.6 M4 — Alerts, air quality, CLI (5 weeks → 2026-12-24) · **Beta 0.4**

Deliverables
- **CAP 1.2 parser** in `libclima` (Qt XML, no new dependency) with a conformance suite.
- `IAlertProvider`: NWS (US), MeteoAlarm (EU/UK/IL), ECCC (Canada), MET Alerts (Norway).
- Alert UI: severity-ranked banner, detail sheet, polygons on the map, expiry handling.
- Desktop notifications via XDG portal / Windows toast / macOS Notification Center, with
  per-severity opt-in and a background-run permission flow.
- Air Quality view: dual AQI gauges, pollutant breakdown, forecast, **Europe-gated pollen**.
- `clima-cli`: `clima-cli now|hourly|daily [--json|--csv|--format …]` for status bars.

Exit criteria
- A live US and a live European alert render correctly, including polygon geometry.
- No expired alert is ever displayed (tested with fixtures around `expires`).
- CLI output is stable enough to document as an interface.

## 6.7 M5 — Trust layer, history, map overlays (7 weeks → 2027-02-11) · **Beta 0.5**

**Goal:** the differentiators. This is where Clima stops being "a good weather app" and
becomes the one people recommend.

Deliverables
- Model-specific providers (`/v1/ecmwf`, `/v1/gfs`, `/v1/dwd-icon`, …) and a **Models view**:
  `ModelDivergence` small multiples + a disagreement heat strip.
- Ensemble API integration and `EnsembleFan` (p10/p25/p50/p75/p90 + optional spaghetti).
- Confidence communicated on the *main* screens, not hidden in a tab — e.g. tomorrow's high
  shown as a range when models disagree.
- Historical: ERA5 archive integration, `HistoryDecades` (30-year same-day), monthly
  calendar, records.
- Map overlay layers: temperature, precipitation, animated wind particles, AQI — rendered
  client-side from gridded multi-point Open-Meteo requests.

Exit criteria
- For a location where ICON-D2 and AIFS disagree on rain, the UI makes that legible in
  under 3 seconds of looking.
- Historical charts load from immutable cache instantly on second visit.
- Multi-point gridded requests stay within the free-tier call budget (measure it).

## 6.8 M6 — Desktop citizenship (5 weeks → 2027-03-18) · **RC 0.9**

Deliverables
- **Plasma 6 applet** and **GNOME Shell extension**, both consuming `libclima` (this is what
  the MPL-2.0 engine split was for).
- Tray / menu-bar item with live temperature.
- KRunner plugin + GNOME search provider ("weather in Lisbon").
- Forecast accuracy scoreboard (Historical Forecast API) — replay past forecasts vs.
  observations, per model, for the user's own location.
- Data export (CSV/JSON of any view), lifestyle indices derived from our own data.
- Windows: jump list, Mica, taskbar; macOS: menu-bar extra, vibrancy.
- Documentation site + screenshots + an honest "what we can't do" page.

Exit criteria
- Applet and extension both installable and both show the same data as the app.
- No regression in the §3.4 performance budgets with the applet running.

## 6.9 M7 — Hardening and launch (5 weeks → 2027-04-22) · **1.0**

Deliverables
- Packaging complete per [`07-packaging.md`](07-packaging.md): Flathub, AppImage, AUR,
  Debian/Fedora specs, winget + MSIX, notarised DMG + Homebrew cask.
- AppStream metadata, screenshots, release notes, reproducible-build check.
- Crash handling with **local-only** reports the user chooses to attach.
- Full a11y audit, i18n freeze + translation drive, security review of all network paths.
- Launch: Flathub, r/linux, Hacker News, OMG!Ubuntu, LinuxLinks, Phoronix, KDE/GNOME blogs.

Exit criteria
- Clean install on Ubuntu, Fedora, Arch, Windows 11, macOS 14+ with no manual steps.
- Every §5.7 parity target met or explicitly documented as not met.

## 6.10 Post-1.0 backlog

Satellite imagery (needs an open ingest story) · marine forecasts · flood/GloFAS ·
CMIP6 climate projections · Android and iOS (note the GPLv3/App Store constraint in D6) ·
lightning · webcams · personal weather station ingest (WeeWX, Ecowitt) ·
`clima-relay` self-hostable proxy · Home Assistant integration.

## 6.11 Concrete first week

1. `git mv` nothing — start from the CMake skeleton and directory layout in §4.3.
2. Answer the four open questions in [`08-risks.md`](08-risks.md), especially the licence
   split (Q1) and the app ID / domain (Q3) — both get expensive to change.
3. Write `HttpClient` + its tests first. Every provider depends on it, and its
   User-Agent/backoff policy is a *compliance* requirement, not a nicety (MET Norway
   returns 403 for a generic UA).
4. Stand up CI on all three OSes before writing the second provider.
5. Record real Open-Meteo and MET Norway responses into `tests/fixtures/` on day one, so CI
   never needs the network.
