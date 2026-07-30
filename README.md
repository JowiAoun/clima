<!-- SPDX-FileCopyrightText: 2026 Clima contributors -->
<!-- SPDX-License-Identifier: CC-BY-SA-4.0 -->

# Clima

> A native, ad-free, open-source weather app for **Linux, Windows and macOS** that shows you
> not just a forecast, but *how much to trust it* — by comparing the world's major forecast
> models side by side, on top of a global radar map.

**Status: planning.** No code yet. The complete plan lives in [`docs/`](docs/README.md).

## Why

There is no weather app — open source or proprietary — that combines all five of:

1. global coverage, 2. an interactive radar map, 3. severe-weather alerts,
4. MSN-class charts and data breadth, 5. Linux **and** Windows **and** macOS as
first-class targets.

The best Linux apps are Linux-only with no map and no alerts. The best cross-platform
open-source app covers two countries. The best open-source radar viewer is a US-centric
expert tool with no forecast UI. Linux users still keep a Windows machine around for
weather radar. See [`docs/01-landscape.md`](docs/01-landscape.md) for the evidence.

## What makes it different

- **Model comparison and ensemble confidence.** ECMWF IFS, ECMWF AIFS, GFS, ICON,
  AROME, UKMO — side by side, with p10–p90 ranges. When the models disagree, you see it.
- **Radar that works on Linux**, over a vector basemap, with a scrubable timeline.
- **Offline-first.** It renders from cache and reconciles with the network. It never shows
  you an empty screen because an API is down.
- **No ads, no news feed, no telemetry, no account, no API key.**
- **Real desktop citizenship** — Plasma applet, GNOME Shell extension, tray, and a
  scriptable CLI, all sharing one engine.

## Stack

C++20 engine (`libclima`, GUI-free) + **Qt 6.8+ / QML** interface, custom scene-graph chart
kit, MapLibre Native for maps, Open-Meteo as the primary data provider with MET Norway
fallback and region-routed radar and CAP alert providers.

## Data

Weather data from [Open-Meteo](https://open-meteo.com/) (CC-BY 4.0), which aggregates
ECMWF, NOAA, DWD, Météo-France, UK Met Office and 14 more national services;
[MET Norway](https://api.met.no/) as fallback; alerts from NWS, MeteoAlarm and ECCC;
basemap from OpenStreetMap contributors. Full attribution in
[`docs/02-data-sources.md`](docs/02-data-sources.md) §2.9 and, at runtime, in
**About → Data sources**.

## Licence

Planned (pending sign-off — see [`docs/08-risks.md`](docs/08-risks.md) Q1):
`libclima` under **MPL-2.0**, the application under **GPL-3.0-or-later**, authored assets
under **CC-BY-SA-4.0**. Contributions by DCO sign-off, no CLA.

## Contributing

Not yet — the plan needs sign-off first. Start at
[`docs/README.md`](docs/README.md); the decisions that need answering are in
[`docs/08-risks.md`](docs/08-risks.md) §8.2.
