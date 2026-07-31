<!-- SPDX-FileCopyrightText: 2026 Jowi Aoun -->
<!-- SPDX-License-Identifier: CC-BY-SA-4.0 -->

# 08 — Risk Register and Open Questions

## 8.1 Risks

Severity = impact × likelihood if unmitigated.

| # | Risk | Severity | Mitigation |
|---|---|---|---|
| R1 | **Radar licensing incident** — shipping RainViewer's free tier to thousands of users violates its "personal or educational use only" terms | 🔴 High | `IRadarProvider` abstraction from M3 day one; default to CC-BY LibreWXR and open national composites; RainViewer only as explicit user opt-in |
| R2 | **Accidental GPLv3 lock-in** — a casual `find_package(Qt6 Charts)` or Qt Lottie import silently forces the whole app to GPLv3 and permanently kills the iOS path | 🔴 High | D3/D10 forbid those modules; add a CI check that fails the build if any GPL-only Qt module appears in the link line |
| R3 | **LGPLv3 non-compliance** — no Qt corresponding-source offer, no relink info, static linking | 🔴 High | Dynamic linking enforced; release job generates the licence bundle and source offer; `reuse lint` in CI from M0 |
| R4 | **Scope explosion** — 38 weeks of plan, one developer, and radar/alerts/models each look like a whole product | 🔴 High | Every milestone ships a usable artefact; the parity matrix (§5) is the contract for what is *out* of scope; differentiators deferred to M5, not chased early |
| R5 | **Open-Meteo non-commercial terms** — if Clima ever takes donations-with-perks, sponsorship, or a paid tier, the free API licence no longer applies | 🟠 Med | Keep the app strictly free and ad-free; if that changes, either buy the commercial API tier or self-host (§2.8, ~50 GB selective / 500 GB+ global) |
| R6 | **Alerts coverage is thin outside US/EU/CA** — MeteoAlarm's REST portal appears aimed at member services, and other countries need per-issuer CAP endpoints | 🟠 Med | Verify MeteoAlarm third-party terms before M4; use the WMO SWIC source list to expand; be honest in-app about coverage rather than silently showing nothing |
| R7 | **MapLibre Native Qt integration friction** — GL/RHI interop, Wayland, fractional scaling, no distro packaging | 🟠 Med | Vendor it via FetchContent; Supercell Wx is the working precedent to study; spike this in M0 for one day before committing to the M3 estimate |
| R8 | **Qt version fragmentation** — floor at 6.8 costs us animated SVG (6.10) and the 6.11 GPU 2D work | 🟡 Low | Feature-guard newer APIs; Flatpak is primary so most users get modern Qt anyway |
| R9 | **Pollen and satellite parity gaps** cannot be closed from open data | 🟡 Low | Region-gate honestly; document in a "known gaps" page; never fabricate data |
| R10 | **Chart kit is a build-it-yourself risk** — bespoke scene-graph charts are more work than a library | 🟡 Low | Ship `TemperatureBand` + `PrecipBars` first and prove the pattern in M2 before writing eight more |
| R11 | **Being blocked by a provider** — MET Norway 403s generic User-Agents; providers may block IPs for misuse | 🟡 Low | UA policy and backoff live in `HttpClient` with tests (M0); hard-stop on 403 rather than retry |
| R12 | **Attribution drift** — a new provider gets added without its credit | 🟡 Low | `Attribution` is a required member of every provider interface; the About screen is generated from the registry, so it cannot go stale |

## 8.2 Open questions — I need your decisions

These four are cheap to answer now and expensive to change later. M0 is blocked on Q1 and
Q3.

### Q1 — Licence split
Recommendation (D6): **`libclima` MPL-2.0 + `clima` app GPL-3.0-or-later**, DCO sign-off, no
CLA. This mirrors Vremenar and keeps a future iOS/App Store build legally possible.
The alternatives are all-GPL-3.0-or-later (simpler, closes App Store forever) or
all-Apache-2.0 (maximally permissive, allows proprietary forks).

### Q2 — Project identity
"Clima" is the repo name; is it the product name? It is a common word, which makes search
discovery and a trademark position weak, and there are existing projects using it.
Worth deciding before the AppStream ID and domain are minted.

### Q3 — App ID and domain
The AppStream/Flatpak/D-Bus ID must be a namespace you control, and it is baked into the
settings path — changing it later loses users' saved locations. Options: `app.clima.Clima`
(needs the domain), or `io.github.<your-user>.Clima` (works immediately, harder to rebrand).

### Q4 — macOS signing budget
A notarised DMG needs an Apple Developer ID at $99/year. Without it, macOS users get a
Gatekeeper warning and a right-click-to-open dance, which realistically kills adoption on
that platform. Is macOS a real target, or is it "builds and runs, unsigned"?

### Q5 — Team shape (affects every date in §6)
The roadmap assumes one focused developer. If this is nights-and-weekends, the honest
9-month plan becomes ~18–24 months, and I would cut M5's map overlay layers and M6's
GNOME extension from 1.0.

## 8.3 Things I could not fully verify

Flagged so nobody quotes them as fact:

| Claim | Status |
|---|---|
| KWeather's severe-weather alert support via KWeatherCore | Believed present; not confirmed |
| Supercell Wx licence = MIT | Believed; confirm from the repo's LICENSE |
| MeteoAlarm's terms for third-party (non-member) API/feed consumers | **Unresolved — blocks M4 planning confidence** |
| Whether Open-Meteo's free-tier limits are per-IP (assumed) rather than per-app | Assumed; matters for §2.8's "no backend needed" conclusion |
| Reverse geocoding strategy (Nominatim policy vs. bundled offline admin polygons) | Undecided |
| Exact Flathub `org.kde.Platform` Qt version at our M7 date | Moves; recheck before freezing the Qt floor |
