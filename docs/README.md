<!-- SPDX-FileCopyrightText: 2026 Jowi Aoun -->
<!-- SPDX-License-Identifier: CC-BY-SA-4.0 -->

# Clima — Plan Index

Planning baseline: **2026-07-30**.

## Executive summary

**The gap is real.** No weather app — open source or proprietary — combines global coverage,
a radar map, severe-weather alerts, MSN-class charts, and first-class Linux + Windows + macOS
support. The closest cross-platform open-source app (Vremenar, Qt 6) covers two countries;
the closest radar app (Supercell Wx, Qt 6 + MapLibre) is a US-centric expert tool with no
forecast UI; the best Linux apps (Mousam, GNOME Weather) are Linux-only with no map and no
alerts. See [`01-landscape.md`](01-landscape.md).

**The stack is right, with one correction.** Qt 6's core is C++, but Qt 6 UIs are written in
**QML** with C++ underneath — so this is a **C++20 engine + QML interface**, not pure C++.
See [`03-tech-stack.md`](03-tech-stack.md) §3.0.

**The biggest technical finding is a licensing one.** Qt Charts, Qt Graphs, Qt Lottie and
Qt Quick 3D are **GPLv3-only, not LGPL**. Using any of them silently forces the whole app to
GPLv3 and permanently closes the iOS App Store. We therefore build our own scene-graph chart
kit — which we wanted anyway, because MSN-class weather charts are not "a line chart with a
legend". See §3.1 and D3.

**The hardest data problem is radar, not forecast.** Open-Meteo gives us everything for
forecasts, air quality and history with no key and no backend. But it has **no alerts
product**, and the convenient global radar source (RainViewer) is licensed for
personal/educational use only — so radar and alerts both need region-routed provider chains.
See [`02-data-sources.md`](02-data-sources.md).

**The differentiator is trust, not features.** You noted MSN is good except its predictions.
We can't out-forecast ECMWF, but Open-Meteo exposes ECMWF IFS, ECMWF AIFS, GFS, ICON,
AROME, UKMO and full ensembles from one API — so Clima can show *model disagreement and
confidence ranges* instead of one confident wrong number. Nothing mainstream does this.
See [`05-feature-parity.md`](05-feature-parity.md) §5.6.

**Timeline: ≈38 weeks to 1.0** for one focused developer, with a shippable artefact at every
one of the eight milestones. See [`06-roadmap.md`](06-roadmap.md).

**Five decisions are yours to make before M0 ends** — licence split, product name, app ID,
macOS signing budget, and team shape. See [`08-risks.md`](08-risks.md) §8.2.

## Reading order

| Doc | What it answers |
|---|---|
| [`01-landscape.md`](01-landscape.md) | Is the premise true? Who are we up against, and what do they teach us? |
| [`02-data-sources.md`](02-data-sources.md) | Where does every number come from, what does it cost, and what must we attribute? |
| [`03-tech-stack.md`](03-tech-stack.md) | Qt 6, C++ vs QML, the Qt licence trap, and ten recorded decisions |
| [`04-architecture.md`](04-architecture.md) | Layers, repo layout, provider interfaces, caching, threading, chart kit, desktop integration |
| [`05-feature-parity.md`](05-feature-parity.md) | The MSN checklist, row by row, plus what we do that MSN can't |
| [`06-roadmap.md`](06-roadmap.md) | Eight milestones, effort, dates, exit criteria, and the concrete first week |
| [`07-packaging.md`](07-packaging.md) | How it reaches users on each platform, and the CI that builds it |
| [`08-risks.md`](08-risks.md) | Twelve risks, five decisions I need from you, six things I could not verify |
| [`09-reference-capture.md`](09-reference-capture.md) | How we measure MSN instead of guessing at it, and what the first captures corrected |

## The documents that describe the app rather than the plan

The nine above are a **planning baseline**, frozen at 2026-07-30 and amended in
place with dated corrections when the code contradicts them. These describe
what exists now, and they change whenever it does.

| Doc | What it answers |
|---|---|
| [`known-gaps.md`](known-gaps.md) | What the app does not do, what it costs, and what would close it |
| [`releasing.md`](releasing.md) | How a version gets from `main` to a download, and the one step that is deliberately manual |
| [`10-design-system.md`](10-design-system.md) | The tokens, the layout rules, and what a tablet's extra room buys |

## Non-negotiables

Ranked, so that when something has to give, it is clear what gives last.

1. **Never show an empty screen.** Offline-first cache + provider fallback. This is the
   documented failure mode of the current best Linux weather app.
2. **No ads, no news feed, no telemetry, no account, no API key.** The MSN news injection is
   the single most-complained-about thing about the app we are chasing.
3. **Honest uncertainty.** A range when models disagree; never a confident wrong number.
4. **Honest coverage.** Region-gate pollen and radar where the data does not exist. Never
   fabricate, never show a broken feature.
5. **Licence compliance is CI-enforced**, not best-effort.
6. **Native performance.** < 400 ms to first paint, 60 fps charts, ~0 % idle CPU.
