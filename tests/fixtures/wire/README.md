<!-- SPDX-FileCopyrightText: 2026 Jowi Aoun -->
<!-- SPDX-License-Identifier: CC-BY-SA-4.0 -->

# Recorded snapshots, as `clima-daemon` sends them

Four complete snapshots off the wire — the exact bytes `SnapshotChanged` carries — captured by
running the daemon against `tests/fixtures/openmeteo/` and friends at their frozen clocks.
`record.sh` is the command that captured them.

They are what `clima-widget --snapshot <file>` reads, which is how a tile is developed, reviewed
and photographed without a session bus, a daemon or a network.

## Why these are recorded rather than hand-written

The same argument as `tests/fixtures/openmeteo/README.md`, one layer out. A snapshot written by
hand encodes the same beliefs as the code that reads it and agrees with it happily; one produced
by the daemon does not. Two of the four defects found in the first render of the tiles were
visible only because the data was real — an air-quality index that is genuinely `null` in Kampala,
and four Seattle alerts whose `issuer` field turned out to be the severity word rather than the
agency.

They are compact single lines because that is what the daemon emits and the point is fidelity.
`python3 -m json.tool < toronto.json` when you need to read one.

## The files

| File | Place | What it is for |
|---|---|---|
| `toronto.json` | Toronto · `America/Toronto` | The ordinary case, and the one used for the widget goldens. AQI 15, `alertsKnown: true` with an **empty** alert list — the tile has to say "No warnings in force" and not "unavailable". |
| `seattle.json` | Seattle · `America/Los_Angeles` | **Four alerts**, one Heat Advisory at `moderate` outranking three Air Quality Alerts at `unknown`. The alerts tile shows the first and "+3 more". AQI 64, "Poor". |
| `berlin.json` | Berlin · `Europe/Berlin` | `alertsKnown: false` — no alert provider covers Germany yet, and the tile must say **"Warnings unavailable"** rather than claiming there are none. |
| `kampala.json` | Kampala · `Africa/Kampala` | **No air-quality product.** `airquality.index` is `null`, so the dial draws its track and no arc, and prints a dash rather than "Good". The null-is-not-zero case, with real data behind it. |

## Two things they are not

**They are not a mask test.** Each is a full snapshot with every field in it, because one file has
to serve every tile. Over the bus each subscription gets only the fields and the horizon it asked
for; in file mode `DaemonLink` trims the series to the tile's horizon and ignores the rest, which
is enough to make a six-hour rain tile add up six hours instead of three hundred.

**They are not frozen against the clock.** The instants inside are frozen — the fixture clock is —
but a tile that draws "now" (the sun arc's mark, the staleness footer) reads the real clock, so
those move between runs. Any golden image over these has to pin the tile set accordingly.
