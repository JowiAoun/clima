<!-- SPDX-FileCopyrightText: 2026 Jowi Aoun -->
<!-- SPDX-License-Identifier: CC-BY-SA-4.0 -->

# 05 — MSN Weather Parity Matrix

The stated goal is "up to par with MSN Weather, other than the predictions". This document
turns that into a checklist. Milestone codes refer to [`06-roadmap.md`](06-roadmap.md).

Legend: ✅ ship it · ⭕ partial / region-gated · ❌ deliberately not doing · ⚠️ blocked by data

## 5.1 Core forecast

| MSN feature | Clima | Milestone | Data source | Notes |
|---|---|---|---|---|
| Current conditions (temp, feels-like, condition icon) | ✅ | M1 | Open-Meteo `current` | |
| Hourly forecast | ✅ **better** — 15-min where available | M2 | `hourly`, `minutely_15` | 15-min is Central Europe + North America only |
| 10-day forecast | ✅ **better** — up to **16 days** | M2 | `forecast_days=16` | |
| Day drill-down (tap a day → its hours) | ✅ | M2 | | MSN does this well; copy the interaction |
| Wind, gusts, direction | ✅ | M1 | | Wind at 10/80/120/180 m available |
| Humidity, dew point | ✅ | M1 | | |
| Barometric pressure + trend | ✅ | M2 | `pressure_msl`, `surface_pressure` | Trend arrow computed from history |
| Visibility | ✅ | M2 | | |
| Chance of precipitation | ✅ | M1 | `precipitation_probability` | |
| Precipitation amount, rain/shower/snow split | ✅ | M2 | | |
| Cloud cover (total/low/mid/high) | ✅ **better** | M2 | | MSN shows total only |
| UV index | ✅ | M2 | forecast + AQI endpoints | |
| Sunrise / sunset / moon phase | ✅ | M2 | `daylight_duration`, `sunshine_duration` + computed ephemeris | Moon phase computed locally; a top user request on Linux apps |
| Severe weather alerts | ✅ | **M4** | NWS / MeteoAlarm / ECCC CAP | Region-routed; no global free source |
| Multiple saved locations + tabs | ✅ | M1 | | |
| "My location" auto-detect | ✅ | M2 | Qt Positioning → GeoClue2 | |
| Location search | ✅ **better** — postcodes, any language, global | M1 | Open-Meteo Geocoding (GeoNames) | Fixes the "only knows big cities" complaint |

## 5.2 Maps

| MSN feature | Clima | Milestone | Source |
|---|---|---|---|
| Radar observation (animated past 2 h) | ✅ | M3 | LibreWXR / IEM WMS-T / ECCC GeoMet |
| Radar **forecast** (nowcast) | ⭕ | M3 | Nowcast frames where the provider offers them |
| Temperature map layer | ✅ | M5 | Rendered client-side from gridded Open-Meteo multi-point requests |
| Precipitation map layer | ✅ | M5 | as above |
| Wind map layer | ✅ **better** — animated particle flow | M5 | as above |
| Cloud layer | ⭕ | M5 | |
| Satellite imagery | ⚠️ | post-1.0 | Needs an open source; EUMETSAT/GOES ingest is a project of its own |
| Humidity / AQI map layers | ⭕ | M5 | AQI grid from Open-Meteo multi-point |
| Layer switcher tab strip | ✅ | M3 | MSN's pattern; it works |

## 5.3 Air quality and health

| MSN feature | Clima | Milestone | Notes |
|---|---|---|---|
| AQI with pollutant breakdown | ✅ **better** — both US and European AQI | M4 | |
| AQI forecast | ✅ | M4 | 4–5 days depending on CAMS domain |
| Pollen tab (tree/grass/weed) | ⭕ **Europe only** | M4 | **Parity gap.** CAMS pollen is Europe-only, in-season, 4-day. Region-gate honestly; do not fabricate |
| "Life index" / lifestyle (running, golf, gardening) | ⭕ | M6 | Derivable from our own data; will differ from MSN's proprietary indices |
| Health/allergy advisories | ❌ | — | Editorial content, not data |

## 5.4 Historical and climate

| MSN feature | Clima | Milestone | Notes |
|---|---|---|---|
| Historical averages for the location | ✅ | M5 | ERA5 via Open-Meteo Archive (1940→) |
| Same-day history across ~3 decades | ✅ **equal** | M5 | `HistoryDecades` chart |
| Monthly view / calendar of past weather | ✅ | M5 | |
| Records (all-time high/low) | ✅ | M5 | Computed from archive |
| Climate projections | ✅ **beyond MSN** | post-1.0 | Open-Meteo Climate API (CMIP6) |

## 5.5 Where we deliberately diverge from MSN

| MSN behaviour | Clima |
|---|---|
| MSN news feed injected into the weather app | ❌ **Never.** This is the most-complained-about thing about the app |
| Ads | ❌ Never |
| Telemetry / account sign-in | ❌ Never |
| Single opaque forecast presented with false confidence | ❌ Replaced by explicit uncertainty (§5.6) |
| Web-view-based UI | ❌ Native scene graph |
| Videos / editorial weather stories | ❌ Out of scope |

## 5.6 Beyond MSN — our differentiators

These are the reasons someone switches, not just the reasons they don't complain.

| Feature | Why it matters | Milestone |
|---|---|---|
| **Multi-model comparison** — ECMWF IFS, AIFS, GFS, ICON, ARPEGE/AROME, UKMO side by side | Directly targets MSN's weakest point. When models disagree, the user deserves to know | **M5** |
| **Ensemble confidence bands** — p10–p90 fan, optional spaghetti | "18 °C" vs "14–23 °C" is a different decision | **M5** |
| **Forecast accuracy scoreboard** — replay past forecasts against observations, per model, for *this* location | Nothing mainstream does this. It is the honest answer to "which model should I trust here?" Uses Open-Meteo's Historical Forecast API | **M6** |
| **Radar on Linux, properly** | The single loudest documented gap; people keep Windows machines for this | **M3** |
| **Offline-first** | Works on a plane, in a tent, on hotel wifi. No competitor on Linux does this | **M1** |
| **Plasma applet + GNOME extension from the same engine** | Real desktop citizenship, not a window that happens to run on Linux | **M6** |
| **`clima-cli`** — scriptable forecast for status bars, waybar, tmux, scripts | The Linux audience will love this and it costs us little | M4 |
| **Per-quantity units** (°C with mph, mm with inHg) | Everyone gets this wrong | M2 |
| **Full data export** (CSV/JSON of everything on screen) | Enthusiasts and researchers | M6 |
| **Accessible charts** (screen-reader data tables, colour-blind-safe, keyboard scrubbing) | Nobody in this category does it | M2 onward |
| **Reproducible, packager-friendly build** | Gets us into Fedora, Debian, AUR, nixpkgs — distribution is a feature | M7 |

## 5.7 Parity scorecard target at 1.0

| Category | Target |
|---|---|
| Core forecast | **110 %** of MSN (16-day, 15-min, more cloud/wind detail) |
| Maps | **~70 %** of MSN (no satellite imagery) |
| Air quality | **95 %** (pollen Europe-only) |
| Historical | **100 %** |
| Alerts | **~85 %** (excellent US/EU/CA, thin elsewhere) |
| Forecast trust / uncertainty | **far beyond** MSN |
| Polish and motion | the bar to clear; see [`07-packaging.md`](07-packaging.md) for how we get judged |
