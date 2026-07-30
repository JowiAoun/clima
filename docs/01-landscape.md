# 01 — Competitive Landscape

**Question we set out to answer:** is it actually true that there is no truly good
cross-platform weather app, especially on Linux?

**Verdict: substantially true, but the popular phrasing is too strong.**

There *are* good Linux weather apps. What does not exist is any single application —
open source or proprietary — that combines all five of the following:

1. Global coverage (not one or two countries)
2. An interactive radar / map layer
3. Severe-weather alerts
4. Rich, MSN-class charts and data breadth
5. Linux **and** Windows **and** macOS as first-class targets

Every existing app drops at least two of those. That gap is the product opportunity,
and it is a narrower, more defensible claim than "Linux weather apps are bad".

---

## 1.1 Linux-native and cross-platform apps

| App | Stack | Platforms | Data source | Global? | Radar/map | Charts | Alerts | AQI / pollen | Multi-model | License | Status |
|---|---|---|---|---|---|---|---|---|---|---|---|
| **GNOME Weather** | GTK4 / libadwaita, C | Linux only | libgweather → MET Norway | ✅ | ❌ | Minimal (temp line) | ❌ | ❌ | ❌ | GPL-2.0+ | Active |
| **Mousam** | GTK4 / libadwaita, Python | Linux only | Open-Meteo | ✅ | ❌ | Good (bento, bars, lines) | ❌ | AQI ✅ / pollen ❌ | ❌ | GPL-3.0 | Active |
| **KWeather** | Qt + Kirigami, C++ | Linux (Plasma desktop + mobile) | KWeatherCore (NOAA, MET.no) | ✅ | ❌ | Basic | Partial (CAP feeds) ⚠️ | ❌ | ❌ | GPL-2.0+ | Active (25.12.x, 2026) |
| **Vremenar** | Qt 6 / QML, C++ | Linux, Windows, macOS, iOS, Android | ARSO (SI) + DWD (DE) | ❌ **2 countries** | ✅ radar on map | Limited | Region-specific | ❌ | GPL-3.0 / MPL-2.0 | Active, small (~15★) |
| **Supercell Wx** | Qt 6 / C++ + MapLibre GL Native | Linux, Windows, macOS | NEXRAD L2/L3, NWS CAP | ❌ US-centric | ✅ **excellent** | ❌ (radar-first) | ✅ NWS | ❌ | ❌ | MIT ⚠️ | Active |
| **Meteo** | GTK3 / Vala | Linux only | OpenWeatherMap (key required) | ✅ | Basic OWM tiles | Basic | ❌ | ❌ | ❌ | GPL-3.0 | Aging |
| **Cumulus** | Qt5 / GTK | Linux only | Yahoo Weather (API retired) | — | ❌ | ❌ | ❌ | ❌ | ❌ | GPL-3.0 | Stale / broken |
| **GTK Meteo** | GTK4 | Linux only | Open-Meteo | ✅ | ❌ | Minimal | ❌ | ❌ | ❌ | GPL-3.0 | Low activity |
| **wttr.in** | Terminal / web | Everywhere | Multiple | ✅ | ASCII only | ASCII | ❌ | ❌ | ❌ | Apache-2.0 | Active |

⚠️ = claim not fully verified during research; confirm before quoting publicly.

## 1.2 The proprietary bar we are measuring against

| App | Platforms | Linux? | Radar | Charts | Alerts | Historical | AQI/pollen | Ads/news |
|---|---|---|---|---|---|---|---|---|
| **MSN Weather** | Windows, Android, iOS, Web | ❌ | ✅ obs + forecast radar, temp, cloud, satellite | ✅ strong | ✅ | ✅ 30-year same-day history | ✅ both | ⚠️ heavy MSN news injection |
| Weather & Radar (WetterOnline) | Android, iOS, Windows | ❌ | ✅ | ✅ | ✅ | Partial | ✅ | Ads |
| CARROT Weather | Apple, Android | ❌ | ✅ | ✅ | ✅ | ✅ | ✅ | Subscription |
| Windows 11 built-in Weather | Windows | ❌ | ✅ | ✅ | ✅ | ✅ | ✅ | Ads/news |

## 1.3 What each competitor teaches us

| Source | Lesson to steal | Mistake to avoid |
|---|---|---|
| MSN Weather | Bento-grid dashboard; "day drill-down"; 30-year history comparison; map layer switcher as a tab strip | News feed pollution; opaque single-model forecast with no confidence signal |
| Mousam | Density done well — a lot of numbers that still scan; GNOME HIG discipline | Linux-only; hard dependency on one API with no fallback → app shows nothing when API is down |
| Supercell Wx | MapLibre GL Native + Qt is a proven, performant combination for a radar app on Linux | Expert-only UI; no forecast product at all |
| Vremenar | Proof that Qt/QML ships to all five platforms from one codebase | Coverage tied to two national agencies, so it cannot grow globally |
| GNOME Weather | Excellent restraint and OS integration | Too little data for a weather enthusiast |

## 1.4 Documented user pain we can name in our README

- Recurring complaint of a "dearth of useful desktop weather radar apps for Linux"; some
  long-time Linux users keep a Windows machine specifically for radar software.
- Linux weather apps frequently only resolve major cities, making them useless for rural
  users — a geocoding/quality problem, not a forecast problem.
- Mousam reviewers note it fails to load *anything* when its single upstream API is
  unreachable — i.e. no offline cache, no fallback provider.
- Requested-but-missing features across apps: moon phase, rain alerts, customisation.

Those four map directly onto four of our design principles in
[`04-architecture.md`](04-architecture.md): radar-first map, high-quality geocoding,
offline-first cache, multi-provider fallback.

## 1.5 Positioning statement

> **Clima** is a native, ad-free, open-source weather app for Linux, Windows and macOS
> that shows you not just a forecast but *how much to trust it* — by comparing the
> world's major forecast models side by side, on top of a global radar map.

The differentiator is deliberately aimed at MSN's one genuine weakness: MSN has a good
UI and bad predictions, presented with false confidence. We cannot beat ECMWF at
forecasting, but we can beat MSN at *communicating forecast uncertainty*, because
Open-Meteo hands us ECMWF IFS, ECMWF AIFS, GFS, ICON, ARPEGE/AROME, UKMO and full
ensembles from a single API surface.

## Sources

- [Best Weather Apps for Linux — LinuxHint](https://linuxhint.com/best_linux_weather_apps/)
- [Mousam is the Ultimate Weather App for Linux Desktop — OMG! Ubuntu](https://www.omgubuntu.co.uk/2024/10/mousam-modern-weather-app-for-linux)
- [Mousam docs](https://amit9838.github.io/mousam-docs/)
- [KWeather — KDE Applications](https://apps.kde.org/kweather/)
- [KDE/kweather on GitHub](https://github.com/KDE/kweather)
- [Vremenar](https://vremenar.app/) · [ntadej/Vremenar](https://github.com/ntadej/Vremenar)
- [Supercell Wx](https://supercellwx.net/) · [dpaulat/supercell-wx](https://github.com/dpaulat/supercell-wx)
- [Welcome to MSN Weather — Microsoft Support](https://support.microsoft.com/en-us/msn/welcome-to-msn-weather)
- [MSN Weather on Google Play](https://play.google.com/store/apps/details?id=com.microsoft.amp.apps.bingweather)
- [libgweather NEWS (yr.no → met.no switch)](https://github.com/GNOME/libgweather/blob/main/NEWS)
- [Real Weather Radar software for Linux — Kubuntu Forums](https://www.kubuntuforums.net/forum/general/community-cafe/30874-real-weather-radar-software-for-linux)
- [12 Best Free and Open Source GUI Weather Tools — LinuxLinks](https://www.linuxlinks.com/excellent-free-weather-software/)
