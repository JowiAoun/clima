// SPDX-FileCopyrightText: 2026 Jowi Aoun
// SPDX-License-Identifier: MPL-2.0

#include "migrations.h"

#include "libclima/core/clock.h"

#include <QSqlDatabase>
#include <QSqlError>
#include <QSqlQuery>
#include <QVariant>

#include <algorithm>

namespace clima {

namespace {

Status execOrFail(QSqlDatabase &database, const QString &sql)
{
    QSqlQuery query(database);
    if (!query.exec(sql)) {
        return Error(ErrorKind::Storage,
                     QStringLiteral("%1 [while running: %2]")
                         .arg(query.lastError().text(), sql.simplified()));
    }
    return ok();
}

// ---- version 1 --------------------------------------------------------------
//
// Three tables, and the design decision behind the middle one is the one worth
// reading.
//
// forecast_blob stores the RAW PROVIDER PAYLOAD — the bytes exactly as they
// arrived — and not a parsed model. That is a deliberate inversion of the
// obvious design, and the reasoning is:
//
//   * Parsing is cheap and refetching is not. A hundred kilobytes of JSON
//     parses in single-digit milliseconds on a worker thread. Refetching it
//     costs a round trip, a user's mobile data, and one more request against a
//     free service's rate limit.
//
//   * A change to the domain model must not invalidate the cache. If the rows
//     held serialised HourlyPoints, then adding a field to HourlyPoint — which
//     will happen a dozen times before 1.0 — means either a migration that
//     cannot reconstruct the new field from the old rows, or throwing the
//     whole cache away on upgrade. Holding the payload means a new binary
//     re-parses what it already has and comes up warm.
//
//   * Golden-file provider tests (§4.11) want recorded responses. The cache is
//     already recording them, in the same form.
//
// The columns beside the payload are only what is needed to *find* it and to
// decide whether it is still good: the key, what produced it, when it arrived,
// when it stops being fresh, and the validators for a conditional GET.
Status createVersion1(QSqlDatabase &database)
{
    // ---- places -------------------------------------------------------------
    //
    // The user's saved locations. `id` is an alias for SQLite's rowid, so a
    // place keeps its identity across renames — a settings row pointing at
    // "the place we last showed" must not follow a name.
    //
    // The coordinate is stored at full precision even though every request
    // rounds it to four decimals. What was searched for and what is requested
    // are two different facts, and collapsing them means a reverse-geocode or
    // an elevation lookup that wants the original can no longer have it.
    //
    // ---- why only `name` and the coordinate are NOT NULL ---------------------
    //
    // Because a default-constructed QString is *null*, not empty, and Qt's
    // SQLite driver binds a null QString as SQL NULL. A place from the
    // geocoder with no admin1 — plenty of countries have none — therefore
    // arrives here as NULL, and `NOT NULL DEFAULT ''` rejects the insert with
    // "NOT NULL constraint failed" rather than applying the default. A DEFAULT
    // fills in a column the statement did not mention; it does not rewrite a
    // NULL the statement supplied.
    //
    // The alternative is coercing every optional string to a non-null empty
    // one at the binding site, which means eight call sites all having to
    // remember. Letting them be NULL is both less code and more honest: an
    // absent admin1 *is* absent, and QVariant::toString() maps it back to the
    // empty QString the caller started with. What stays NOT NULL is what a
    // null would actually be a bug in — a place has a name and a position.
    Status status = execOrFail(database, QStringLiteral(R"sql(
        CREATE TABLE places (
            id           INTEGER PRIMARY KEY,
            name         TEXT    NOT NULL,
            admin1       TEXT,
            country      TEXT,
            country_code TEXT,
            timezone     TEXT,
            latitude     REAL    NOT NULL,
            longitude    REAL    NOT NULL,
            elevation_m  REAL,
            favourite    INTEGER NOT NULL DEFAULT 0,
            sort_order   INTEGER NOT NULL DEFAULT 0,
            added_at     INTEGER
        )
    )sql"));
    if (!status)
        return status;

    status = execOrFail(database, QStringLiteral(
        "CREATE INDEX places_sort_order ON places (sort_order, id)"));
    if (!status)
        return status;

    // ---- forecast_blob ------------------------------------------------------
    //
    // `key` is RequestKey::toString() and is the primary key: one row per
    // (provider, endpoint, rounded coordinate, parameter set), which is exactly
    // the unit the network layer coalesces on. The two layers sharing one
    // notion of identity is what makes "was this already fetched?" and "is this
    // already cached?" the same question.
    //
    // `payload` is nullable, and a NULL payload is not a cache entry. It is the
    // row storeValidators() creates when an ETag arrives before anyone has
    // decided the body was worth keeping — see CacheStore, which treats a NULL
    // payload as a miss on read.
    //
    // `expires_at` NULL means never: the immutable row of §4.5's table, ERA5
    // reanalysis for a day that has already happened.
    status = execOrFail(database, QStringLiteral(R"sql(
        CREATE TABLE forecast_blob (
            key            TEXT NOT NULL PRIMARY KEY,
            provider_id    TEXT,
            endpoint       TEXT,
            kind           TEXT,
            latitude       REAL,
            longitude      REAL,
            payload        BLOB,
            content_type   TEXT,
            etag           BLOB,
            last_modified  BLOB,
            server_expires INTEGER,
            fetched_at     INTEGER,
            expires_at     INTEGER
        )
    )sql"));
    if (!status)
        return status;

    // Pruning walks this. Without it, every prune is a full table scan of the
    // largest table in the file.
    status = execOrFail(database, QStringLiteral(
        "CREATE INDEX forecast_blob_expires_at ON forecast_blob (expires_at)"));
    if (!status)
        return status;

    // ---- settings -----------------------------------------------------------
    //
    // The engine's own key/value store, and deliberately not the same thing as
    // app/settings.h.
    //
    // That file is a QSettings holding *preferences* — units, appearance,
    // window geometry — which belong to the GUI and are edited by a human. This
    // table holds engine state that has to travel with the cache it describes:
    // which place was last shown, when each provider was last successfully
    // reached, the id of the CAP message we already notified about. A Plasma
    // applet and a CLI reusing libclima have no QSettings of the app's and
    // should still agree with it about all of those, which is only possible if
    // the answer lives beside the data rather than beside the window.
    //
    // NOT NULL on the key because SQLite, for backward compatibility with a
    // bug it shipped in 2001, allows NULLs in a PRIMARY KEY column that does
    // not say otherwise — and a settings table with two NULL-keyed rows is a
    // preference that saved twice and loads neither.
    status = execOrFail(database, QStringLiteral(R"sql(
        CREATE TABLE settings (
            key   TEXT NOT NULL PRIMARY KEY,
            value TEXT
        )
    )sql"));
    if (!status)
        return status;

    return ok();
}

} // namespace

QList<Migration> defaultMigrations()
{
    return {
        Migration{ 1, QStringLiteral("places, forecast_blob, settings"), createVersion1 },
    };
}

int highestVersion(const QList<Migration> &migrations)
{
    int highest = 0;
    for (const Migration &migration : migrations)
        highest = std::max(highest, migration.version);
    return highest;
}

int currentVersion(QSqlDatabase &database)
{
    QSqlQuery query(database);
    if (!query.exec(QStringLiteral("PRAGMA user_version")) || !query.next())
        return 0;
    return query.value(0).toInt();
}

Status runMigrations(QSqlDatabase &database, const QList<Migration> &migrations, Clock *clock)
{
    // Sorted rather than trusted. The list is written by hand and an
    // out-of-order append would otherwise apply v3 before v2 and record a
    // user_version that lies about what the file contains.
    QList<Migration> ordered = migrations;
    std::sort(ordered.begin(), ordered.end(),
              [](const Migration &a, const Migration &b) { return a.version < b.version; });

    for (int i = 1; i < ordered.size(); ++i) {
        if (ordered[i].version == ordered[i - 1].version) {
            return Error(ErrorKind::Storage,
                         QStringLiteral("two migrations claim version %1")
                             .arg(ordered[i].version));
        }
    }

    // The runner's own bookkeeping, created before anything it records. IF NOT
    // EXISTS because this runs on every open, not only on a fresh file.
    Status status = execOrFail(database, QStringLiteral(R"sql(
        CREATE TABLE IF NOT EXISTS schema_version (
            version     INTEGER PRIMARY KEY,
            description TEXT    NOT NULL DEFAULT '',
            applied_at  INTEGER
        )
    )sql"));
    if (!status)
        return status;

    const int from = currentVersion(database);
    const int to = highestVersion(ordered);

    // Forward-only, and this is where that is enforced. A file written by a
    // newer Clima is refused: we do not know what its extra columns mean, and
    // writing into it with an older schema is how a cache ends up
    // self-inconsistent in a way no version can repair.
    if (from > to) {
        return Error(ErrorKind::Storage,
                     QStringLiteral("the cache database is at schema version %1 and this build "
                                    "understands up to %2. It was written by a newer Clima. "
                                    "Delete it to start fresh — it is a cache, and nothing in it "
                                    "is unrecoverable.")
                         .arg(from)
                         .arg(to));
    }

    for (const Migration &migration : ordered) {
        if (migration.version <= from)
            continue;

        if (!database.transaction()) {
            return Error(ErrorKind::Storage,
                         QStringLiteral("could not begin a transaction for migration %1: %2")
                             .arg(migration.version)
                             .arg(database.lastError().text()));
        }

        status = migration.apply(database);
        if (!status) {
            database.rollback();
            return Error(ErrorKind::Storage,
                         QStringLiteral("migration %1 (%2) failed: %3")
                             .arg(migration.version)
                             .arg(migration.description, status.error().message()));
        }

        QSqlQuery record(database);
        record.prepare(QStringLiteral(
            "INSERT OR REPLACE INTO schema_version (version, description, applied_at) "
            "VALUES (?, ?, ?)"));
        record.addBindValue(migration.version);
        record.addBindValue(migration.description);
        record.addBindValue(clock != nullptr ? QVariant(clock->now().toMSecsSinceEpoch())
                                             : QVariant());
        if (!record.exec()) {
            database.rollback();
            return Error(ErrorKind::Storage,
                         QStringLiteral("could not record migration %1: %2")
                             .arg(migration.version)
                             .arg(record.lastError().text()));
        }

        // PRAGMA takes no bound parameters, so the number is formatted in. It
        // is an int that came from a struct literal in this repository, not
        // from anything a server or a user typed.
        status = execOrFail(database, QStringLiteral("PRAGMA user_version = %1")
                                          .arg(migration.version));
        if (!status) {
            database.rollback();
            return status;
        }

        if (!database.commit()) {
            database.rollback();
            return Error(ErrorKind::Storage,
                         QStringLiteral("could not commit migration %1: %2")
                             .arg(migration.version)
                             .arg(database.lastError().text()));
        }
    }

    return ok();
}

} // namespace clima
