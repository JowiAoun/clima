// SPDX-FileCopyrightText: 2026 Jowi Aoun
// SPDX-License-Identifier: MPL-2.0
//
// How long each kind of weather data stays worth believing.
//
// The table in cachepolicy.cpp is a transcription of docs/04-architecture.md
// §4.5 and nothing else. It is not tuned, it is not guessed, and a change to a
// number here is a change to that document first — the numbers are downstream
// of what the providers actually publish (CAMS updates twelve-hourly, a radar
// frame has a five-minute lifetime, ERA5 reanalysis never changes once it is
// written) rather than of anybody's opinion about freshness.
//
// ---- three fields, because a TTL alone is not a caching policy --------------
//
// Every row in §4.5 has three columns, and dropping either of the other two
// loses something the app depends on:
//
//   ttl                   when the entry stops being fresh.
//
//   revalidation          what we send to ask "is it still the same?".
//     Cheap for us and cheaper for them — MET Norway's terms *require*
//     conditional requests, and a 304 costs a few hundred bytes against a
//     hundred-kilobyte forecast. `None` means the endpoint offers no
//     validator worth sending; `CapLifetime` means the payload carries its own
//     validity window and that window wins over anything we would compute.
//
//   staleWhileRevalidate  whether an expired entry may still be shown while a
//     fresh one is fetched.
//     This is design principle 1 in §4.1 — "The UI must never show an empty
//     screen because an API is down" — expressed as a per-kind flag. It is
//     true for everything except alerts, and the exception is the reason the
//     flag exists at all: showing a stale forecast reads as "updated 25
//     minutes ago", and showing a stale tornado warning reads as a tornado
//     warning. §4.5 marks that row with a warning sign and the words "never
//     show an expired alert".
//
// ---- immutable is not a very large TTL --------------------------------------
//
// The historical archive row says "Immutable, cache forever". Expressing that
// as, say, a hundred years of seconds means every freshness comparison in the
// codebase does arithmetic on a number chosen to be too big to matter, and the
// day one of them overflows or one of them is compared with `<` instead of
// `<=` there is no test that notices. So it is a separate flag, and an
// immutable entry gets an *invalid* expiry timestamp in the database — a value
// SQLite stores as NULL and QDateTime reports as invalid, which every read
// path already has to handle.

#pragma once

#include <QDateTime>
#include <QString>

#include <chrono>

namespace clima {

// The kinds of thing the cache holds. One per row of §4.5, in the order they
// appear there, so the two can be read side by side.
enum class DataKind {
    CurrentConditions,
    Forecast,           // hourly and daily; one payload, one row
    Nowcast,            // minutely_15
    Ensemble,           // ensemble members and model comparison
    AirQuality,
    Alerts,
    RadarFrame,
    BasemapTile,
    HistoricalArchive,
    Geocoding,
};

enum class Revalidation {
    None,          // the endpoint offers nothing to revalidate against
    EntityTag,     // If-None-Match / If-Modified-Since
    CapLifetime,   // the CAP message's own <sent>/<expires> decide
};

struct CachePolicy {
    std::chrono::seconds ttl{0};
    Revalidation         revalidation        = Revalidation::None;
    bool                 staleWhileRevalidate = false;

    // "Cache forever." `ttl` is meaningless when this is set and is left at
    // zero so that a caller who reads it anyway gets an obviously wrong answer
    // rather than a plausible one.
    bool immutable = false;
};

// The table. Total over DataKind — adding a kind without adding its row is a
// compile error under -Wswitch, which is on for this library.
CachePolicy policyFor(DataKind kind);

// Stable strings for the database and for log output. Stored in the `kind`
// column as text rather than as the enum's integer, because an integer column
// silently changes meaning the day somebody inserts a value in the middle of
// the enum, and a cache full of rows whose kind shifted by one is worse than
// a cache miss.
QString  dataKindName(DataKind kind);
DataKind dataKindFromName(const QString &name, bool *ok = nullptr);

// When an entry fetched at `fetchedAt` stops being fresh. Returns an invalid
// QDateTime for an immutable kind — see the header comment; invalid means
// "never".
QDateTime expiryFor(DataKind kind, const QDateTime &fetchedAt);

} // namespace clima
