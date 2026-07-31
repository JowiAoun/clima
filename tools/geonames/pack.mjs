// SPDX-FileCopyrightText: 2026 Jowi Aoun
// SPDX-License-Identifier: GPL-3.0-or-later
//
// Turns GeoNames' cities15000 dump into the index Clima reverse-geocodes
// against offline, and writes it where CMake will compile it into libclima.
//
//   nix develop -c node tools/geonames/pack.mjs \
//       --cities  work/cities15000.zip \
//       --admin1  work/admin1CodesASCII.txt \
//       --out     libclima/providers/geocoding/data/cities15000.cgx
//
// ---- why this tool exists at all, and why its output is committed -----------
//
// Reverse geocoding — turning the coordinate GeoClue2 hands us into the words
// "Toronto, Ontario" — has one obvious implementation, which is to ask
// Nominatim. That implementation does not work. Asked once, today, from this
// machine, with a properly identifying User-Agent naming the project and a
// contact address, Nominatim answered HTTP 403 "Access denied" to the *first*
// request. The OSM Foundation's usage policy is enforced rather than
// aspirational, and a desktop app that ships a Nominatim call ships a feature
// that is broken for every user at once the day the policy tightens further.
//
// So the lookup is offline, and this is the tool that makes it possible. The
// packed output is COMMITTED to the repository — 3.3 MB of upstream zip
// reduced to a few hundred kilobytes — for the same reason tests/fixtures/ is
// committed: a build must not need a network. A packaging build on a Debian
// buildd, a Flathub builder and a CI runner all have the same amount of
// internet, which is none.
//
// Re-run it when the upstream dump is worth refreshing (GeoNames rebuilds
// daily; a yearly refresh is plenty for city names) and commit the result with
// the provenance block this tool prints.
//
// ---- what is kept, and what is thrown away ----------------------------------
//
// Upstream has nineteen tab-separated columns. Seven survive: geonameid,
// latitude, longitude, name, country code, admin1 code and timezone. The
// alternate-names column alone is two thirds of the file and it is what the
// *forward* geocoder is for — that runs against Open-Meteo's hosted copy of
// the same dataset, which can afford to index a hundred spellings of Toronto.
//
// Population is not stored, but it is not discarded either: it is folded into
// one byte per row, the settlement's modelled radius. See `reachMetres`.
//
// Rows whose feature code is PPLX, PPLH, PPLQ or PPLW are dropped. PPLX is a
// *section* of a populated place — "Moss Park", "Bay Street Corridor" — and
// the nearest row to downtown Toronto is a PPLX, so keeping them would answer
// "where am I" with the name of a neighbourhood nobody outside the city has
// heard of. The other three are historical, abandoned and destroyed places:
// somewhere you cannot be standing.
//
// ---- the file format --------------------------------------------------------
//
// Little-endian throughout, one uncompressed header followed by one zlib
// stream. libclima/providers/geocoding/geonamesindex.h documents the layout
// from the reading end and the two must be read side by side; what follows is
// only the encoder's half.
//
// The payload is COLUMNAR — every latitude, then every longitude, then every
// id — rather than row-major, and the difference is a third of the file. A
// row-major layout interleaves four unrelated kinds of number, so deflate sees
// a repeating 25-byte pattern with no runs in it; a columnar one gives it a
// long stretch of values that differ from their neighbour in the low bits
// only. Measured on this dataset: 664 KB row-major, 413 KB columnar with the
// integer columns delta-coded. Same numbers, same order, same compressor.

import { createHash } from "node:crypto";
import { readFileSync, writeFileSync } from "node:fs";
import { basename } from "node:path";
import { deflateSync, inflateRawSync } from "node:zlib";

// ---- the packing constants, which the reader also has to know ---------------
//
// COORDINATE_SCALE is 10 000 — four decimal places — and it is not a guess
// about how much precision a city centre deserves. It is
// `clima::Coordinate::keyDecimals`, the precision every outbound request in
// this codebase is quantised to before it is hashed or sent
// (libclima/domain/coordinate.h explains why four). Storing five decimals here
// would mean storing a digit that no request, no cache key and no comparison
// in the engine can ever see. The reader rounds the same way `Coordinate` does
// — half away from zero — so a reverse-geocoded place and a searched one round
// to bit-identical doubles.
const COORDINATE_SCALE = 10000;

// The grid. Five degrees is 2 592 cells for 31 673 rows, about twelve rows a
// cell, and a bounding-box query at the default 250 km cutoff touches a few
// hundred of them. Finer cells make the offset table bigger without making the
// scan meaningfully shorter at this row count; coarser ones start scanning
// thousands of rows in Europe.
const CELL_DEGREES = 5;
const LAT_CELLS = 180 / CELL_DEGREES;
const LON_CELLS = 360 / CELL_DEGREES;

// One byte per row holds how far a settlement reaches, in 250 m steps, so the
// largest representable reach is 63.75 km — comfortably past Tokyo's 39 km and
// short of nothing that matters. See `reachMetres` for the model.
const REACH_STEP_METRES = 250;

// A place you can stand in and be in. Everything else in cities15000 is either
// a piece of a place we already have (PPLX) or a place that is no longer there
// (PPLH historical, PPLQ abandoned, PPLW destroyed).
const DROPPED_FEATURE_CODES = new Set(["PPLX", "PPLH", "PPLQ", "PPLW"]);

// ---- how big is a city ------------------------------------------------------
//
// The whole difficulty of "which of these places am I in" is that nearest is
// the wrong answer. Stand at 43.65 N, 79.38 W — the corner of Yonge and Queen,
// downtown Toronto — and the nearest surviving row is Moss Park at 880 m, then
// Etobicoke, then Thornhill. Toronto itself is 6.4 km away, because a city's
// row sits at its centroid and a city is bigger than that. The same thing
// happens in Singapore (Ang Mo Kio New Town wins), in Tokyo (Asagaya-minami
// wins) and in Paris (Paris 04 Hôtel-de-Ville wins). Nearest-neighbour on this
// dataset answers a question nobody asked.
//
// So each row carries a radius, and the radius comes from population. If a
// settlement of P people is a disc at some typical urban density rho, then
// pi*r^2*rho = P and r = sqrt(P / (pi * rho)). At rho = 2 000 people per square
// kilometre — the low end of urban, which is deliberate, see below — that puts
// Toronto's reach at 21 km, Tokyo's at 39 km, Reykjavík's at 4.4 km and a
// 16 000-person town's at 1.6 km. Those are the right order of magnitude for
// "how far out do people say they live in X".
//
// The density is on the low side of real urban density (Toronto proper is
// about 4 400/km²) and that is the point: the model is not trying to draw the
// city limits, it is trying to cover the suburbs the dataset has no row for.
// Being generous costs nothing, because the *ranking* is what is used and
// every candidate is inflated by the same rule.
//
// libclima/providers/geocoding/geonamesindex.cpp does the ranking. This file
// only decides the radius.
const URBAN_DENSITY_PER_KM2 = 2000;

// A row with no population — a handful of administrative seats in the dump
// have zero — would otherwise get a zero radius and lose every comparison.
// A thousand people is 400 m of reach, which is about right for a hamlet and
// is small enough that it never beats a real town.
const MINIMUM_MODELLED_POPULATION = 1000;

function reachMetres(population) {
    const people = Math.max(population, MINIMUM_MODELLED_POPULATION);
    return Math.sqrt(people / (Math.PI * URBAN_DENSITY_PER_KM2)) * 1000;
}

// ---- the encoders -----------------------------------------------------------

// Half away from zero, matching `roundTo` in libclima/domain/coordinate.cpp.
// JavaScript's Math.round is half *up*, which disagrees on exactly the
// negative halves — Math.round(-0.5) is -0, std::round(-0.5) is -1 — and a
// coordinate in the southern or western hemisphere is where that would show
// up. Quietly, on one row in ten thousand.
function roundHalfAwayFromZero(value) {
    return value >= 0 ? Math.floor(value + 0.5) : -Math.floor(-value + 0.5);
}

function quantise(degrees) {
    return roundHalfAwayFromZero(degrees * COORDINATE_SCALE);
}

// LEB128, and zigzag first for anything that can go backwards. Deltas within a
// column are mostly small and occasionally large — a delta crosses a grid cell
// boundary a few thousand times — which is the distribution a varint is for.
function zigzag(value) {
    return value < 0 ? -2 * value - 1 : 2 * value;
}

function writeVarint(out, value) {
    let remaining = value;
    for (;;) {
        const septet = remaining & 0x7f;
        remaining = Math.floor(remaining / 128);
        if (remaining === 0) {
            out.push(septet);
            return;
        }
        out.push(septet | 0x80);
    }
}

function deltaVarints(values) {
    const out = [];
    let previous = 0;
    for (const value of values) {
        writeVarint(out, zigzag(value - previous));
        previous = value;
    }
    return Buffer.from(out);
}

function plainVarints(values) {
    const out = [];
    for (const value of values)
        writeVarint(out, value);
    return Buffer.from(out);
}

// ---- reading the upstream dumps ---------------------------------------------
//
// The cities file is accepted as either the .zip upstream publishes or the
// .txt inside it. Unzipping it here rather than asking for `unzip` keeps the
// tool runnable inside `nix develop`, whose package list is deliberately about
// building Clima and does not carry an archiver.
//
// This reads exactly one entry, and it reads it through the CENTRAL DIRECTORY
// rather than through the local file header at the front of the archive.
// That is not thoroughness, it is necessity: GeoNames' zips are written by a
// streaming producer, so the local header declares both sizes as zero and puts
// the real ones in a data descriptor *after* the compressed bytes — which
// cannot be found without already knowing where the compressed bytes end. The
// central directory at the tail has both numbers written down.
//
// It is not a zip implementation and it is not meant to become one: no Zip64,
// no encryption, no multi-disk, no second entry. Anything it cannot read, it
// refuses to guess at.
function readZipEntry(bytes) {
    // The end-of-central-directory record is 22 bytes plus a comment of up to
    // 64 KB, so it is found by scanning backwards for its signature rather
    // than by arithmetic.
    let eocd = -1;
    for (let at = bytes.length - 22; at >= 0 && at > bytes.length - 22 - 65536; --at) {
        if (bytes.readUInt32LE(at) === 0x06054b50) {
            eocd = at;
            break;
        }
    }
    if (eocd < 0)
        throw new Error("not a zip file: no end-of-central-directory record");

    const entryCount = bytes.readUInt16LE(eocd + 10);
    if (entryCount < 1)
        throw new Error("the zip archive is empty");

    const directory = bytes.readUInt32LE(eocd + 16);
    if (bytes.readUInt32LE(directory) !== 0x02014b50)
        throw new Error("the central directory does not start with a file header");

    const method = bytes.readUInt16LE(directory + 10);
    const compressedSize = bytes.readUInt32LE(directory + 20);
    const localHeader = bytes.readUInt32LE(directory + 42);

    if (bytes.readUInt32LE(localHeader) !== 0x04034b50)
        throw new Error("the central directory points at something that is not a local header");

    const start = localHeader + 30 + bytes.readUInt16LE(localHeader + 26)
        + bytes.readUInt16LE(localHeader + 28);
    const payload = bytes.subarray(start, start + compressedSize);

    if (method === 0)
        return payload;
    if (method === 8)
        return inflateRawSync(payload);
    throw new Error(`unsupported zip compression method ${method}`);
}

function readCities(path) {
    const bytes = readFileSync(path);
    const text = path.endsWith(".zip") ? readZipEntry(bytes) : bytes;
    return text.toString("utf8");
}

// admin1CodesASCII.txt: "<country>.<code>\t<name>\t<ascii name>\t<geonameid>".
// Column 1 and not column 2, because column 2 is the ASCII fold — "Ile-de-
// France" rather than "Île-de-France" — and the forward geocoder returns the
// real spelling. Two paths that disagree about an accent are two places as far
// as a string comparison is concerned.
function readAdmin1Names(path) {
    const names = new Map();
    for (const line of readFileSync(path, "utf8").split("\n")) {
        if (line.length === 0 || line.startsWith("#"))
            continue;
        const columns = line.split("\t");
        if (columns.length < 2)
            continue;
        names.set(columns[0], columns[1]);
    }
    return names;
}

// ---- the columns ------------------------------------------------------------

function cellIndex(latitude, longitude) {
    const row = Math.min(Math.floor((latitude + 90) / CELL_DEGREES), LAT_CELLS - 1);
    // Modulo rather than a clamp, so that a longitude of exactly 180 lands in
    // the first cell rather than one past the last.
    const column = ((Math.floor((longitude + 180) / CELL_DEGREES) % LON_CELLS) + LON_CELLS)
        % LON_CELLS;
    return row * LON_CELLS + column;
}

function parseCities(text) {
    const rows = [];
    let considered = 0;
    for (const line of text.split("\n")) {
        if (line.length === 0)
            continue;
        const c = line.split("\t");
        if (c.length < 18)
            continue;
        considered += 1;
        if (DROPPED_FEATURE_CODES.has(c[7]))
            continue;

        rows.push({
            id: Number(c[0]),
            name: c[1],
            latitude: Number(c[4]),
            longitude: Number(c[5]),
            countryCode: c[8],
            admin1Code: c[10],
            population: Number(c[14] || 0),
            timezone: c[17],
        });
    }
    return { rows, considered };
}

// Sorted by cell, then by geonameid inside the cell.
//
// The cell order is what the grid index needs. The order *within* a cell is
// free, and sorting by id is worth 20 KB over sorting by latitude: exactly one
// of the three delta-coded columns can be made cheap by the choice, and ids in
// one cell are clustered — GeoNames allocates them by country — while
// latitudes inside a five-degree box are not much more ordered than random.
function sortForPacking(rows) {
    return rows.slice().sort((a, b) => {
        const cellA = cellIndex(a.latitude, a.longitude);
        const cellB = cellIndex(b.latitude, b.longitude);
        return cellA - cellB || a.id - b.id;
    });
}

function buildSections(rows, admin1Names) {
    const countries = new Map();
    // Index 0 is the empty admin1 name, so that a row in a country with no
    // first-level divisions — Singapore, Monaco, the Vatican — indexes
    // something real rather than carrying a sentinel the reader has to know
    // about.
    const admin1 = new Map([["", 0]]);
    const timezones = new Map();

    const intern = (table, key) => {
        let index = table.get(key);
        if (index === undefined) {
            index = table.size;
            table.set(key, index);
        }
        return index;
    };

    const countryIndex = Buffer.alloc(rows.length);
    const reach = Buffer.alloc(rows.length);
    const admin1Index = [];
    const timezoneIndex = [];
    const names = [];

    rows.forEach((row, i) => {
        const country = intern(countries, row.countryCode);
        if (country > 255)
            throw new Error("more than 256 country codes; the country column needs widening");
        countryIndex[i] = country;

        reach[i] = Math.min(255, Math.round(reachMetres(row.population) / REACH_STEP_METRES));

        const admin1Key = `${row.countryCode}.${row.admin1Code}`;
        admin1Index.push(intern(admin1, admin1Names.get(admin1Key) ?? ""));
        timezoneIndex.push(intern(timezones, row.timezone));
        names.push(row.name);
    });

    const counts = new Array(LAT_CELLS * LON_CELLS).fill(0);
    for (const row of rows)
        counts[cellIndex(row.latitude, row.longitude)] += 1;

    const cellStarts = [0];
    for (const count of counts)
        cellStarts.push(cellStarts[cellStarts.length - 1] + count);

    // A newline-separated blob rather than an offset table. It costs one byte
    // a row and saves four, and a length column would have cost 18 KB
    // compressed on its own — deflate is better at finding the separator than
    // we are at encoding where it would have been. No GeoNames name contains a
    // newline; the assertion below is what keeps that a fact rather than an
    // assumption.
    const joinLines = (values) => {
        for (const value of values) {
            if (value.includes("\n"))
                throw new Error(`a name contains a newline and would break the blob: ${value}`);
        }
        return Buffer.from(values.join("\n"), "utf8");
    };

    return {
        sections: [
            deltaVarints(cellStarts),
            deltaVarints(rows.map((row) => quantise(row.latitude))),
            deltaVarints(rows.map((row) => quantise(row.longitude))),
            deltaVarints(rows.map((row) => row.id)),
            reach,
            countryIndex,
            plainVarints(admin1Index),
            plainVarints(timezoneIndex),
            Buffer.from([...countries.keys()].join(""), "ascii"),
            joinLines([...admin1.keys()]),
            joinLines([...timezones.keys()]),
            joinLines(names),
        ],
        countryCount: countries.size,
        admin1Count: admin1.size,
        timezoneCount: timezones.size,
    };
}

const SECTION_COUNT = 12;
const HEADER_BYTES = 52 + 4 * SECTION_COUNT;

function pack(rows, admin1Names) {
    const built = buildSections(rows, admin1Names);
    const payload = Buffer.concat(built.sections);

    // qCompress's wire format: four bytes of big-endian uncompressed length,
    // then a zlib stream. Qt's qUncompress is documented to accept exactly
    // what qCompress produced, and this is what it produced. Writing it here
    // rather than a bare deflate stream means the reader is one library call
    // with no hand-rolled inflate behind it.
    const compressed = Buffer.alloc(4);
    compressed.writeUInt32BE(payload.length, 0);

    const header = Buffer.alloc(HEADER_BYTES);
    header.write("CLGX", 0, "ascii");
    header.writeUInt32LE(1, 4);                       // format version
    header.writeUInt32LE(rows.length, 8);
    header.writeUInt32LE(CELL_DEGREES, 12);
    header.writeUInt32LE(LAT_CELLS, 16);
    header.writeUInt32LE(LON_CELLS, 20);
    header.writeUInt32LE(built.countryCount, 24);
    header.writeUInt32LE(built.admin1Count, 28);
    header.writeUInt32LE(built.timezoneCount, 32);
    header.writeUInt32LE(COORDINATE_SCALE, 36);
    header.writeUInt32LE(REACH_STEP_METRES, 40);
    header.writeUInt32LE(SECTION_COUNT, 44);
    header.writeUInt32LE(payload.length, 48);
    built.sections.forEach((section, i) => header.writeUInt32LE(section.length, 52 + 4 * i));

    return {
        bytes: Buffer.concat([header, compressed, deflateSync(payload, { level: 9 })]),
        payloadBytes: payload.length,
        sections: built.sections,
    };
}

// ---- entry point ------------------------------------------------------------

function option(name, fallback) {
    const at = process.argv.indexOf(`--${name}`);
    if (at >= 0 && at + 1 < process.argv.length)
        return process.argv[at + 1];
    if (fallback !== undefined)
        return fallback;
    throw new Error(`missing required option --${name}`);
}

function sha256(path) {
    return createHash("sha256").update(readFileSync(path)).digest("hex");
}

const citiesPath = option("cities");
const admin1Path = option("admin1");
const outPath = option("out", "libclima/providers/geocoding/data/cities15000.cgx");

const parsed = parseCities(readCities(citiesPath));
const rows = sortForPacking(parsed.rows);
const packed = pack(rows, readAdmin1Names(admin1Path));

writeFileSync(outPath, packed.bytes);

// The provenance block. Paste it into tools/geonames/README.md when the data
// is refreshed: a committed binary whose origin is not written down anywhere
// is a binary nobody can audit or reproduce.
const labels = [
    "cellStarts", "latitudes", "longitudes", "geonamesIds", "reach", "countryIndex",
    "admin1Index", "timezoneIndex", "countryCodes", "admin1Names", "timezoneNames",
    "cityNames",
];

process.stdout.write([
    `source   ${basename(citiesPath)}  sha256 ${sha256(citiesPath)}`,
    `source   ${basename(admin1Path)}  sha256 ${sha256(admin1Path)}`,
    `rows     ${rows.length} kept of ${parsed.considered} in the dump`,
    `payload  ${packed.payloadBytes} bytes before deflate`,
    `output   ${outPath}  ${packed.bytes.length} bytes  sha256 ${sha256(outPath)}`,
    "sections",
    ...packed.sections.map((section, i) => `  ${labels[i].padEnd(14)} ${section.length}`),
    "",
].join("\n"));
