// SPDX-FileCopyrightText: 2026 Jowi Aoun
// SPDX-License-Identifier: MPL-2.0
//
// The warm start. SQLite, at QStandardPaths::AppDataLocation.
//
// Design principle 1 in docs/04-architecture.md §4.1 is "offline-first: the UI
// renders from cache, then reconciles with the network", and the second half of
// that sentence explains why this class exists rather than a QNetworkDiskCache:
// "The app must never show an empty screen because an API is down — the
// documented failure mode of the current best Linux weather app." An HTTP cache
// answers "may I reuse this?". This answers "what is the best thing I can show
// right now, and how old is it?", which is a different question and the one the
// UI actually asks.
//
// ---- what is stored: the bytes, not the model -------------------------------
//
// The raw provider payload, unparsed. The reasoning is written out in
// libclima/cache/migrations.cpp beside the table that holds it; the summary is
// that parsing is cheap, refetching is not, and a domain model that gains a
// field must not throw away everybody's cache.
//
// ---- freshness is three states, not two -------------------------------------
//
//   fresh          inside its TTL. Show it and do nothing.
//   stale          past its TTL, and its kind permits stale-while-revalidate.
//                  Show it, say how old it is, and fetch in the background.
//   unusable       past its TTL and its kind forbids stale. Show nothing.
//
// The third state exists for exactly one row of §4.5's table — alerts — and
// that row is why the distinction is modelled at all. A stale forecast reads as
// "updated 25 minutes ago". A stale severe-weather warning reads as a warning.
// See libclima/cache/cachepolicy.h.
//
// ---- it is also the ValidatorStore ------------------------------------------
//
// The ETag lives next to the payload it validates, because that is the only
// place the two cannot drift apart. HttpClient reaches it through the narrow
// ValidatorStore interface rather than through this class, so the network layer
// stays testable without a database — see libclima/net/validatorstore.h.
//
// ---- threading ---------------------------------------------------------------
//
// One thread. QSqlDatabase connections are owned by the thread that opened
// them and using one from another thread is undefined behaviour that usually
// looks like it works. §4.8 puts SQLite writes on a cache thread of their own;
// this class is what would move there, whole, with its connection.

#pragma once

#include "libclima/cache/cachepolicy.h"
#include "libclima/cache/migrations.h"
#include "libclima/core/result.h"
#include "libclima/domain/coordinate.h"
#include "libclima/domain/place.h"
#include "libclima/net/validatorstore.h"

#include <QByteArray>
#include <QDateTime>
#include <QList>
#include <QSqlDatabase>
#include <QString>

#include <optional>

namespace clima {

class Clock;

// One cached provider response.
struct CacheEntry {
    QString  key;            // RequestKey::toString()
    QString  providerId;
    QString  endpoint;
    DataKind kind = DataKind::Forecast;

    std::optional<Coordinate> coordinate;

    QByteArray payload;      // exactly what the provider sent
    QByteArray contentType;

    QDateTime  fetchedAt;
    QDateTime  expiresAt;    // invalid means never — the immutable row of §4.5
    Validators validators;
};

// A saved location. Declared in libclima/domain/place.h — it moved out of this
// header when the geocoder and the location model turned out to need it, and
// including QSqlDatabase to name a place was the wrong trade.

class CacheStore final : public ValidatorStore
{
public:
    // `clock` must outlive the store. Every freshness decision below goes
    // through it, which is what makes a TTL test a matter of advancing a
    // FrozenClock rather than of sleeping.
    explicit CacheStore(Clock *clock);
    ~CacheStore() override;

    CacheStore(const CacheStore &) = delete;
    CacheStore &operator=(const CacheStore &) = delete;

    // <AppDataLocation>/cache.sqlite. Does not create anything.
    static QString defaultDatabasePath();

    // Creates the parent directory, opens the file, applies migrations.
    // ":memory:" is honoured and skips the directory step, which is what tests
    // use.
    Status open(const QString &databasePath);

    // The overload that takes a migration list. Present for tests — see
    // migrations.h for why the runner is testable against a schema that is not
    // the product's.
    Status open(const QString &databasePath, const QList<Migration> &migrations);

    void close();

    [[nodiscard]] bool    isOpen() const;
    [[nodiscard]] int     schemaVersion() const;
    [[nodiscard]] QString databasePath() const { return m_path; }

    // ---- payloads -----------------------------------------------------------

    // Insert or replace. `expiresAt` is taken from the entry rather than
    // recomputed, because the network layer may have honoured a longer Expires
    // than our table would have chosen — HttpClient does exactly that.
    Status put(const CacheEntry &entry);

    // The entry whatever its age. Freshness is the caller's decision and it
    // needs the row to make it: "stale but showable" is a state, not a miss.
    // ErrorKind::NotFound when there is no row, or when the row exists only to
    // hold validators and has no payload.
    [[nodiscard]] Result<CacheEntry> get(const QString &key) const;

    Status remove(const QString &key);

    // After a 304: the bytes are unchanged, the clock has moved on. Rewrites
    // fetched_at and expires_at without touching the payload.
    Status touch(const QString &key, const QDateTime &fetchedAt, const QDateTime &expiresAt);

    // Rows whose expiry has passed and whose kind may not be served stale.
    // Everything else is kept: an expired forecast is still the best thing to
    // show on a train. Returns how many rows went.
    Result<int> pruneUnusable();

    [[nodiscard]] bool isFresh(const CacheEntry &entry) const;
    [[nodiscard]] bool isUsable(const CacheEntry &entry) const;

    // ---- places -------------------------------------------------------------

    // Inserts when id is 0 and fills it in; updates otherwise.
    Status savePlace(Place &place);

    [[nodiscard]] Result<QList<Place>> places() const;

    Status removePlace(qint64 id);

    // ---- engine settings ----------------------------------------------------
    //
    // Not the app's preferences. See the settings table's comment in
    // migrations.cpp for the distinction and why it matters to a Plasma applet.

    Status                          setSetting(const QString &key, const QString &value);
    [[nodiscard]] Result<QString>   setting(const QString &key) const;

    // ---- ValidatorStore -----------------------------------------------------

    [[nodiscard]] std::optional<Validators> validatorsFor(const QString &key) const override;
    void storeValidators(const QString &key, const Validators &validators) override;

private:
    Status configureConnection();

    Clock        *m_clock = nullptr;
    QString       m_path;
    QString       m_connectionName;
    QSqlDatabase  m_database;
    int           m_schemaVersion = 0;
};

} // namespace clima
