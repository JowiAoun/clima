<!--
SPDX-FileCopyrightText: 2026 Jowi Aoun
SPDX-License-Identifier: CC-BY-SA-4.0
-->

# tools/geonames

The packer that turns GeoNames' `cities15000` dump into the index Clima reverse-geocodes
against **offline**, and a harness for looking at what geocoding actually returns.

```
libclima/providers/geocoding/data/cities15000.cgx   the output, committed
tools/geonames/pack.mjs                             the packer
tools/geonames/main.cpp                             clima-geocode, the harness
```

## Why the lookup is offline

Reverse geocoding — a coordinate in, "Toronto, Ontario" out — has one obvious implementation
and it does not work. Nominatim was tested from a developer machine on 2026-07-31 with a
properly identifying `User-Agent` naming the project and a contact address. It answered
**HTTP 403 "Access denied" to the first request**. Not the eleventh, not after a burst: the
first. The OSM Foundation's usage policy is enforced rather than aspirational.

Four things follow from doing it locally instead, and each would be worth the four hundred
kilobytes on its own:

1. **It dodges the 403.** There is no request to refuse.
2. **It is deterministic.** The same coordinate gives the same city on every machine,
   forever. [`docs/04-architecture.md`](../../docs/04-architecture.md) §4.11 wants
   golden-image chart tests, and a golden image with a place name in the corner is only
   golden if that name cannot change under it.
3. **It works offline**, which is the app's headline property. §4.1's "renders from cache,
   then reconciles" would be quietly broken by a network call in the *labelling* path: the
   forecast would come up from cache and the place name would not.
4. **It is the same dataset the forward geocoder searches.** Open-Meteo's geocoding API is
   GeoNames; this index is GeoNames. Type "Toronto" and get geonameid `6167865`; stand at
   43.65 N, 79.38 W and get geonameid `6167865`. One entity, one `Place::geonamesId`, one
   row in the places table. Nominatim would have answered with an OSM relation id, which
   has no correspondence to a GeoNames id at all, so the two paths could never have been
   reconciled.

## Running it

The output is **committed**, so a normal build and CI need neither this tool nor a network.
Re-run it when the upstream dump is worth refreshing — GeoNames rebuilds daily, and a yearly
refresh is plenty for city names.

```sh
mkdir -p /tmp/geonames
curl -o /tmp/geonames/cities15000.zip      https://download.geonames.org/export/dump/cities15000.zip
curl -o /tmp/geonames/admin1CodesASCII.txt https://download.geonames.org/export/dump/admin1CodesASCII.txt

nix develop -c node tools/geonames/pack.mjs \
    --cities /tmp/geonames/cities15000.zip \
    --admin1 /tmp/geonames/admin1CodesASCII.txt \
    --out    libclima/providers/geocoding/data/cities15000.cgx
```

The zip is unpacked by the tool itself, so `unzip` is not needed — the devshell's package
list is about building Clima and does not carry an archiver.

Then commit the result together with the provenance block the tool prints, and update the
numbers below. `tests/tst_reversegeocode.cpp` asserts the row count and bounds the file
size, so a refresh that changes either fails a test rather than turning up in a download
size six weeks later.

## Provenance of the committed index

```
source   cities15000.zip       sha256 15963baf7f9f3b15c8a42555fbe0c6e41ea7780f77d0fe337e293000c99145b0
source   admin1CodesASCII.txt  sha256 34784457b76b988a669dff7c3e4b104e4902c0875643cff019281ac79dfa2992
fetched  2026-07-31
rows     31673 kept of 34066 in the dump
payload  758633 bytes before deflate
output   cities15000.cgx  421933 bytes
         sha256 6cbe951840386e6a37cdedb01bf60debb8d1cddd79aa39013f47b84f96113328
```

3.3 MB of upstream zip becomes 412 KiB.

## What is kept and what is thrown away

Upstream has nineteen tab-separated columns; seven survive: `geonameid`, `latitude`,
`longitude`, `name`, `country code`, `admin1 code`, `timezone`. The alternate-names column
alone is two thirds of the file, and it is what the *forward* geocoder is for — that runs
against Open-Meteo's hosted copy, which can afford to index a hundred spellings of Toronto
in forty scripts.

Population is not stored but is not discarded either: it is folded into one byte per row,
the settlement's modelled radius. See below.

Rows whose feature code is `PPLX`, `PPLH`, `PPLQ` or `PPLW` are dropped — 2 393 of them.
`PPLX` is a *section* of a populated place ("Moss Park", "Bay Street Corridor"), and the
nearest row to downtown Toronto is a `PPLX`; the other three are historical, abandoned and
destroyed places, which are not somewhere you can be standing.

Coordinates are stored to **four** decimals, not five, because `Coordinate::keyDecimals` is
4 — every outbound request in the engine is quantised to four decimals before it is hashed
or sent ([`libclima/domain/coordinate.h`](../../libclima/domain/coordinate.h) explains why,
and MET Norway's terms ask for it by name). A fifth decimal would be a digit no cache key,
no URL and no comparison in the product could ever see. The packer rounds half away from
zero, exactly as `Coordinate::rounded` does, so a place found by reverse geocoding and the
same place found by searching round to bit-identical doubles.

## Nearest is the wrong answer

Stand at Yonge and Queen in downtown Toronto. The nearest surviving row is Moss Park at
880 m, then Etobicoke, then Thornhill; Toronto itself is 6.4 km away, because a city's row
sits at its centroid and a city is bigger than a point. The same happens in Singapore (Ang
Mo Kio New Town wins), Tokyo (Asagaya-minami) and Paris (Paris 04 Hôtel-de-Ville).

So each row carries a **reach**, derived from population by treating the settlement as a
disc at a typical urban density: `r = sqrt(P / (π · ρ))` with ρ = 2 000 people/km². Toronto
gets 21 km, Tokyo 39 km, Reykjavík 4.4 km, a 16 000-person town 1.6 km. The density is on
the low side of real urban density on purpose — the model is not drawing city limits, it is
covering the suburbs the dataset has no row for, and every candidate is inflated by the same
rule so only the ranking matters.

Ranking is then two steps:

* Among candidates inside the cutoff, take the smallest `d / reach` — the settlement you are
  furthest *inside*. If that ratio is ≤ 1, that is the answer.
* If every ratio exceeds 1 you are in open country, and the ranking falls back to plain
  distance. Without this, a point 30 km from a village and 60 km from a city would be
  labelled with the city, because a big reach forgives a big distance — right when you are
  inside the city, wrong when you are not.

Beyond the cutoff (250 km by default) the answer is `ErrorKind::Unsupported` and the UI shows
the coordinate. Point Nemo — 48.8767 S, 123.3933 W, the oceanic pole of inaccessibility — is
2 711 km from the nearest row, which is Adamstown, population 46.

## The file format

Little-endian, one uncompressed 100-byte header and one zlib stream in `qCompress`'s wire
format. [`libclima/providers/geocoding/geonamesindex.h`](../../libclima/providers/geocoding/geonamesindex.h)
documents it from the reading end and `pack.mjs` from the writing end; the two must be read
side by side.

The payload is **columnar** — every latitude, then every longitude, then every id — with the
integer columns zigzag-varint delta-coded. That is worth a third of the file: a row-major
layout interleaves four unrelated kinds of number and deflate finds no runs in it. Measured
on this data, 664 KB row-major against 413 KB columnar.

| section | bytes |
|---|---:|
| `cellStarts` | 2 745 |
| `latitudes` | 76 651 |
| `longitudes` | 78 407 |
| `geonamesIds` | 57 747 |
| `reach` | 31 673 |
| `countryIndex` | 31 673 |
| `admin1Index` | 61 723 |
| `timezoneIndex` | 55 273 |
| `countryCodes` | 488 |
| `admin1Names` | 32 312 |
| `timezoneNames` | 5 748 |
| `cityNames` | 324 193 |

## Licence

GeoNames data is **CC BY 4.0**. The packed index is a transformation of a CC-BY work and is
under the same licence; trimming columns and changing the container does not make it ours.

`REUSE.toml` declares `cities15000.cgx` with **GeoNames** as the copyright holder — not
"Jowi Aoun" — and `LICENSES/CC-BY-4.0.txt` carries the terms. The user-visible half of the
obligation is discharged by `OfflineReverseGeocoder::attribution()` and
`OpenMeteoGeocoder::attribution()`, whose lines belong on the About → Data sources screen:

```
Geocoding by Open-Meteo.com (https://open-meteo.com), CC BY 4.0 (https://creativecommons.org/licenses/by/4.0/)
Place names from GeoNames (https://www.geonames.org), CC BY 4.0 (https://creativecommons.org/licenses/by/4.0/)
```

## clima-geocode

```sh
nix develop -c cmake --build --preset dev
nix develop -c ./build/dev/tools/geonames/clima-geocode Kigali
nix develop -c ./build/dev/tools/geonames/clima-geocode 43.65,-79.38
nix develop -c ./build/dev/tools/geonames/clima-geocode --id 6167865
```

An argument that parses as `lat,lon` is a reverse lookup and anything else is a search. The
reverse path sends nothing; the search path is the one thing in this repository besides
`clima-openmeteo-probe` that talks to a real service on purpose, which is why it is a
`CLIMA_DEV_TOOLS` executable and not a test.
