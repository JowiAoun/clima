// SPDX-FileCopyrightText: 2026 Jowi Aoun
// SPDX-License-Identifier: MPL-2.0
//
// Forward-only schema migrations, and the runner that applies them.
//
// docs/04-architecture.md §4.5: "schema-versioned with forward-only
// migrations". Forward-only is the important half. There is no `down()`, there
// is no rollback to a previous version, and a database written by a newer Clima
// than the one opening it is *refused* rather than opened optimistically —
// because the alternative is a v3 binary reading a v4 file, finding a column
// missing, and either crashing or silently writing rows the v4 binary will
// misread. Refusing is recoverable; a corrupted cache written by two versions
// that disagreed is not.
//
// The cost of refusing is one downgrade scenario: a user who tries a nightly
// and goes back to stable finds the cache unopenable. That is a cache. It is
// rebuilt by fetching, and CacheStore says so in the error rather than leaving
// the caller to guess.
//
// ---- two records of the version, and why both -------------------------------
//
//   PRAGMA user_version   the authority. One integer in the file header, read
//                         without a query, written inside the same transaction
//                         as the migration it describes. This is what the
//                         runner compares against.
//
//   schema_version table  the log. One row per migration ever applied, with
//                         its description and when it ran. Nothing branches on
//                         it; it exists so that a user who mails in a broken
//                         cache file can be asked what happened to it, and so
//                         that "which migration did this database actually
//                         see" is answerable rather than inferred.
//
// The table is the runner's own bookkeeping rather than part of any schema, so
// the runner creates it before migration 1 runs. Migration 1 owns the three
// tables that are actually about weather.
//
// ---- how to add one ---------------------------------------------------------
//
// Append to defaultMigrations() with the next integer. Never edit an existing
// migration — the databases that already ran it will not run it again, so an
// edit produces two different schemas both claiming the same version, and the
// difference only shows up on machines that installed at the wrong moment.

#pragma once

#include "libclima/core/result.h"

#include <QList>
#include <QString>

#include <functional>

class QSqlDatabase;

namespace clima {

class Clock;

struct Migration {
    int     version = 0;
    QString description;

    // Runs inside a transaction the runner opened. Returning an error rolls
    // the whole step back, including the user_version bump, so a failed
    // migration leaves the database exactly as it was.
    std::function<Status(QSqlDatabase &)> apply;
};

// The production schema. Version 1 creates places, forecast_blob and settings.
QList<Migration> defaultMigrations();

// The highest version in `migrations`, which is what a freshly migrated
// database's user_version will read.
int highestVersion(const QList<Migration> &migrations);

// Applies everything newer than the database's current user_version, in
// ascending order, each in its own transaction.
//
// `migrations` is a parameter rather than a call to defaultMigrations() so that
// the runner can be tested against a schema that is not the product's — a test
// that wants to prove v1 → v2 works does not have to wait for the product to
// need a v2, and a test that wants to prove a *failing* migration rolls back
// cannot write one into the shipping list.
Status runMigrations(QSqlDatabase &database, const QList<Migration> &migrations, Clock *clock);

// The database's current version. Zero for a database this has never touched.
int currentVersion(QSqlDatabase &database);

} // namespace clima
