<!-- SPDX-FileCopyrightText: 2026 Jowi Aoun -->
<!-- SPDX-License-Identifier: CC-BY-SA-4.0 -->

# Recorded Open-Meteo responses

Bytes the live service actually sent, captured with `curl` on **2026-07-31** and committed.
`record.sh` in this directory is the command that captured them; it is here so the provenance
is executable rather than remembered, and it is **not** run by the build or by CI.

Nothing in `tests/` may reach a network — `docs/04-architecture.md` §4.11, enforced at run time
by `tests/support/networkguard.h` and statically by `tests/tst_sourcerules.cpp`. These files are
how the parser is tested against reality anyway.

The data is Open-Meteo's, under CC BY 4.0. `REUSE.toml` declares that, with Open-Meteo as the
copyright holder rather than us — the same distinction the bundled typeface gets, and for the
same reason.

## Why recorded and not written by hand

The three defects `tst_openmeteoadapter.cpp` exists to catch are all defects of *belief* — about
a unit, about which hour a number belongs to, about what a timestamp means. A fixture written by
hand encodes the same beliefs as the parser and agrees with it happily. A recording does not.

Two of the eight are synthesised in a narrow sense and it is worth being precise about which:
`toronto-dst-fall.json` and `toronto-dst-spring.json` come from the **archive** endpoint rather
than the forecast one, because the forecast API only serves 92 days back and 16 days forward and
Toronto's DST transitions are outside that window in July. The envelope is identical — same
`hourly` / `daily` / `utc_offset_seconds` / `timezone` shape — and the *behaviour under test* is
a property of `timezone=auto`, which both endpoints share. They carry fewer variables because the
archive serves fewer.

## The files

| File | What it is | What it is for |
|---|---|---|
| `toronto-summer.json` | 43.6532,-79.3832 · `past_days=1&forecast_days=16` · 408 hourly, 17 daily, 54 kB | The canonical mapping. Also carries a **null hour at index 405** in the middle of the series, a mostly-null final daily row, and a **null `moonrise` at index 7**. |
| `kampala-precip-spike.json` | 0.3152,32.5816 · 2 days | **The precipitation-shift proof.** One isolated wet hour: `precipitation[10] = 0.40 mm` stamped `10:00`, with `[9]` and `[11]` at zero. An off-by-one is unmistakable against an isolated spike and invisible inside a long band. |
| `miami-thunder.json` | 25.7617,-80.1918 · 7 days | WMO codes 51, 53, 55, 63, 65, 80, 81, 82, **95, 96** — thunder and hail, the two types no amount-plus-temperature fallback can derive. Peak 68.8 mm/h. |
| `andes-snow.json` | -33.45,-70.05 · 7 days, 2514 m | WMO 71, 73, 75, 85, 86, and the **cm-versus-mm** case: `snowfall = 0.98` cm beside `precipitation = 1.40` mm in the same object. |
| `svalbard-midnight-sun.json` | 78.2232,15.6267 · 4 days | Polar day. `sunrise` is that day's midnight and `sunset` is the **next** day's midnight, both valid, `daylight_duration = 86400`. A Sun card that measures them from their own midnights draws an arc of zero length. Also a null `moonrise`/`moonset` pair on day 0. |
| `toronto-ecmwf-gaps.json` | Toronto · `models=ecmwf_ifs025` · 3 days | `uv_index` and `visibility` null for **all 72 hours**, because IFS does not carry them, while temperature is complete. "No such variable here" rather than "no value this hour" — the difference that decides whether a metric tab is drawn. |
| `toronto-dst-fall.json` | Archive · 2025-11-01 … 11-03 | The clocks go back on 2025-11-02. The payload labels that day with **24 rows** and `utc_offset_seconds = -14400` throughout; the real local day has **25**. |
| `toronto-dst-spring.json` | Archive · 2026-03-07 … 03-09 | The clocks go forward on 2026-03-08. Payload: **24 rows**, including an `02:00` that never happened. Real local day: **23**. |

## The thing these last two are evidence of

`timezone=auto` resolves the IANA zone and then applies **one fixed offset** to the whole window.
It is not a bug we are working around so much as a documented-by-omission property, and it is
invisible for ten and a half months of the year. The cleanest demonstration is not even in these
files — it is a January request answered in July:

```
$ …/v1/archive?…&timezone=auto&start_date=2026-01-10&end_date=2026-01-12
{"utc_offset_seconds":-14400, "timezone_abbreviation":"GMT-4",
 "daily":{"sunrise":["2026-01-10T08:50", …]}}
```

Toronto is UTC-5 in January and the sun rose at 07:50.

`libclima/domain/timeaxis.h` carries the reasoning and the fix: reconstruct the UTC instant
(`naive − utc_offset_seconds`, exact, because that is how the label was made) and re-express it in
the real zone. Doing so is what turns a uniformly spaced series into a local day of 25 or 23 hours,
which is what `tst_openmeteoadapter.cpp` asserts.

## Re-recording

Don't, unless a variable is being added. These are golden files: the assertions name specific
indices, specific millimetres and specific minutes, and a fresh capture changes all of them for no
gain. If you must, run `record.sh`, then expect `tst_openmeteoadapter.cpp` to need every literal
re-checked by hand — which is the cost that makes "don't" the right default.
