<!-- SPDX-FileCopyrightText: 2026 Jowi Aoun -->
<!-- SPDX-License-Identifier: CC-BY-SA-4.0 -->

# Recorded afternoons

Three places, captured from the live Open-Meteo endpoints on **2026-07-31** with the request in
`tests/fixtures/openmeteo/record.sh`, and compiled into `libclima` at `:/clima/fixtures/<name>/`.

These are not the golden files in `tests/fixtures/` and they are not a second copy of them. Those
are read by the parser tests and name specific indices; these are read by the **app**, and their job
is to make `clima --fixture toronto` come up with a screen that is the same screen next month.

Each directory is one fixture and contains three files:

| File | What it is |
|---|---|
| `fixture.json` | The manifest: the place, and `recordedAt` — the instant the `FrozenClock` is set to. |
| `forecast.json` | `/v1/forecast`, verbatim. |
| `airquality.json` | `/v1/air-quality`, verbatim. Optional. |

`recordedAt` is the load-bearing field. Everything the UI derives from "now" — which hour is
`nowIndex`, which hours are behind the past veil, whether it is day or night, which of the four sky
phases the phone paints, how many minutes ago the data was fetched — is a comparison against that
one timestamp, made through `libclima/core/clock.h`. Freeze it and all of them freeze together, with
no `if (testing)` anywhere.

## The three

| Fixture | Frozen at | Why it exists |
|---|---|---|
| `toronto` | 2026-07-30 12:28 EDT | The default. That instant is the one `detaildata.js` always claimed to be observing, so the screenshots in `docs/` stay comparable to the ones taken before there was live data. |
| `berlin` | 2026-07-31 12:28 CEST | Inside the CAMS European domain, so `grass_pollen` is a number rather than null and the pollen card is **drawn**. Toronto is where it is **hidden**. The pair is what makes R9 — "region-gate honestly, never fabricate" — reviewable by looking rather than by reading an assertion. |
| `kampala` | 2026-07-31 07:28 EAT | One isolated wet hour. `precipitation[10] = 0.40 mm` stamped `10:00`, with `[9]` and `[11]` at zero — which by Open-Meteo's preceding-hour convention is the hour **starting at 09:00**, and that is the column the wash must sit on. Copied from `tests/fixtures/openmeteo/kampala-precip-spike.json`, which the adapter tests use for the same reason at a different layer. It carries no air quality, which is also useful: it is the fixture that proves a missing product hides a card instead of emptying one. |

## Re-recording

Don't, casually. `tests/tst_fixtureprovider.cpp` pins Kampala's spike to its hour and Toronto's
frozen instant to the minute, and the committed screenshots are photographs of these numbers. A
fresh capture is a different afternoon, and every one of those has to be re-checked by hand.

The one good reason to re-record is a variable being added to
`libclima/providers/openmeteo/openmeteovariables.cpp` — a fixture that does not carry a series the
app now asks for will hide that metric's tab, correctly and confusingly.
