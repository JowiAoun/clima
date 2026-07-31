// SPDX-FileCopyrightText: 2026 Jowi Aoun
// SPDX-License-Identifier: GPL-3.0-or-later
//
// CacheStore: round trip, TTL expiry against a FrozenClock, and the migration
// runner taken from v1 to v2.
//
// Every freshness assertion here is an equality rather than a tolerance,
// because nothing in it sleeps. That is the whole return on the injected clock:
// "eleven minutes later" is a method call.

#include "libclima/cache/cachestore.h"
#include "libclima/core/clock.h"

#include <QSqlDatabase>
#include <QSqlError>
#include <QSqlQuery>
#include <QTemporaryDir>
#include <QTest>

using namespace clima;
using namespace std::chrono_literals;

class TestCacheStore : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void openCreatesTheSchema();
    void payloadRoundTrips();
    void aMissIsNotFoundAndNotAnEmptyEntry();

    void freshnessFollowsTheTtlTable();
    void ttlExpiryIsExactAgainstAFrozenClock();
    void anImmutableEntryNeverExpires();
    void anExpiredAlertIsUnusableAndAnExpiredForecastIsNot();
    void pruneRemovesOnlyWhatMayNotBeShownStale();
    void touchMovesTheExpiryWithoutTouchingThePayload();

    void placesRoundTripAndKeepFullPrecision();
    void settingsRoundTrip();

    void validatorsRoundTripThroughTheStore();
    void aValidatorOnlyRowIsNotACacheHit();

    void migrationFromV1ToV2KeepsTheData();
    void aFailingMigrationRollsBackEntirely();
    void aDatabaseFromTheFutureIsRefused();

private:
    static CacheEntry entry(const QString &key, DataKind kind, const QDateTime &fetchedAt);
};

CacheEntry TestCacheStore::entry(const QString &key, DataKind kind, const QDateTime &fetchedAt)
{
    CacheEntry cached;
    cached.key = key;
    cached.providerId = QStringLiteral("open-meteo");
    cached.endpoint = QStringLiteral("forecast");
    cached.kind = kind;
    cached.coordinate = Coordinate{ 52.520008, 13.404954 };
    cached.payload = QByteArrayLiteral(R"({"hourly":{"temperature_2m":[3.1,3.4]}})");
    cached.contentType = QByteArrayLiteral("application/json");
    cached.fetchedAt = fetchedAt;
    cached.expiresAt = expiryFor(kind, fetchedAt);
    return cached;
}

void TestCacheStore::openCreatesTheSchema()
{
    FrozenClock clock;
    CacheStore  store(&clock);

    const Status status = store.open(QStringLiteral(":memory:"));
    QVERIFY2(status.hasValue(), qPrintable(status.error().toString()));
    QVERIFY(store.isOpen());
    QCOMPARE(store.schemaVersion(), 1);

    // The four tables the engine needs on day one, proved by using them rather
    // than by reading sqlite_master: a table that exists but whose columns do
    // not match what the code binds is the failure worth catching, and only a
    // round trip catches it.
    //
    // schema_version is the runner's own bookkeeping and is created before
    // migration 1 runs; the other three are what migration 1 is. The version
    // above having reached 1 is the schema_version write having succeeded.
    Place place;
    place.name = QStringLiteral("Berlin");
    place.coordinate = Coordinate{ 52.52, 13.405 };
    const Status saved = store.savePlace(place);
    QVERIFY2(saved.hasValue(), qPrintable(saved.error().toString()));
    QVERIFY(store.setSetting(QStringLiteral("last-place"), QStringLiteral("1")).hasValue());
    QVERIFY(store.put(entry(QStringLiteral("k"), DataKind::Forecast, clock.now())).hasValue());
}

void TestCacheStore::payloadRoundTrips()
{
    FrozenClock clock(QDateTime(QDate(2026, 3, 14), QTime(9, 26), QTimeZone::UTC));
    CacheStore  store(&clock);
    QVERIFY(store.open(QStringLiteral(":memory:")).hasValue());

    CacheEntry written = entry(QStringLiteral("open-meteo/forecast@52.5200,13.4050#abcd"),
                               DataKind::Forecast, clock.now());
    written.validators.entityTag = QByteArrayLiteral("\"e-1\"");
    written.validators.lastModified = QByteArrayLiteral("Sat, 14 Mar 2026 09:00:00 GMT");
    QVERIFY(store.put(written).hasValue());

    const Result<CacheEntry> read = store.get(written.key);
    QVERIFY2(read.hasValue(), qPrintable(read.error().toString()));

    const CacheEntry &got = read.value();

    // The raw bytes, byte for byte. Storing the payload rather than a parsed
    // model is the decision this asserts: what comes out is what the provider
    // sent, so a later binary with a different domain model re-parses instead
    // of refetching.
    QCOMPARE(got.payload, written.payload);
    QCOMPARE(got.contentType, written.contentType);
    QCOMPARE(got.providerId, written.providerId);
    QCOMPARE(got.endpoint, written.endpoint);
    QCOMPARE(got.kind, DataKind::Forecast);
    QCOMPARE(got.fetchedAt, written.fetchedAt);
    QCOMPARE(got.expiresAt, written.expiresAt);
    QCOMPARE(got.validators.entityTag, written.validators.entityTag);
    QCOMPARE(got.validators.lastModified, written.validators.lastModified);
    QVERIFY(got.coordinate.has_value());
    QCOMPARE(got.coordinate->latitude, 52.520008);
}

void TestCacheStore::aMissIsNotFoundAndNotAnEmptyEntry()
{
    FrozenClock clock;
    CacheStore  store(&clock);
    QVERIFY(store.open(QStringLiteral(":memory:")).hasValue());

    const Result<CacheEntry> read = store.get(QStringLiteral("nothing/here@-#0"));
    QVERIFY(!read.hasValue());
    QCOMPARE(read.errorKind(), ErrorKind::NotFound);
}

// ---- the TTL table ----------------------------------------------------------

void TestCacheStore::freshnessFollowsTheTtlTable()
{
    // docs/04-architecture.md §4.5, asserted rather than described. A number
    // changed here without changing that table is a failing test.
    QCOMPARE(policyFor(DataKind::CurrentConditions).ttl, std::chrono::seconds(10min));
    QCOMPARE(policyFor(DataKind::Forecast).ttl, std::chrono::seconds(30min));
    QCOMPARE(policyFor(DataKind::Nowcast).ttl, std::chrono::seconds(5min));
    QCOMPARE(policyFor(DataKind::Ensemble).ttl, std::chrono::seconds(60min));
    QCOMPARE(policyFor(DataKind::AirQuality).ttl, std::chrono::seconds(60min));
    QCOMPARE(policyFor(DataKind::Alerts).ttl, std::chrono::seconds(3min));
    QCOMPARE(policyFor(DataKind::RadarFrame).ttl, std::chrono::seconds(5min));
    QCOMPARE(policyFor(DataKind::BasemapTile).ttl, std::chrono::seconds(24h * 30));
    QCOMPARE(policyFor(DataKind::Geocoding).ttl, std::chrono::seconds(24h * 7));
    QVERIFY(policyFor(DataKind::HistoricalArchive).immutable);

    // Revalidation, which is the second column of the same table.
    QCOMPARE(policyFor(DataKind::CurrentConditions).revalidation, Revalidation::EntityTag);
    QCOMPARE(policyFor(DataKind::Forecast).revalidation, Revalidation::EntityTag);
    QCOMPARE(policyFor(DataKind::Alerts).revalidation, Revalidation::CapLifetime);

    // And the third. Alerts is the only false, and it is a safety property:
    // a stale forecast reads as "updated 25 minutes ago", a stale tornado
    // warning reads as a tornado warning.
    QVERIFY(!policyFor(DataKind::Alerts).staleWhileRevalidate);
    QVERIFY(policyFor(DataKind::Forecast).staleWhileRevalidate);
    QVERIFY(policyFor(DataKind::CurrentConditions).staleWhileRevalidate);
}

void TestCacheStore::ttlExpiryIsExactAgainstAFrozenClock()
{
    FrozenClock clock(QDateTime(QDate(2026, 3, 14), QTime(9, 0), QTimeZone::UTC));
    CacheStore  store(&clock);
    QVERIFY(store.open(QStringLiteral(":memory:")).hasValue());

    const CacheEntry current = entry(QStringLiteral("current"), DataKind::CurrentConditions,
                                     clock.now());
    QVERIFY(store.put(current).hasValue());

    // Ten minutes for current conditions.
    QVERIFY(store.isFresh(store.get(QStringLiteral("current")).value()));

    clock.advance(9min);
    QVERIFY(store.isFresh(store.get(QStringLiteral("current")).value()));

    clock.advance(59s);
    QVERIFY(store.isFresh(store.get(QStringLiteral("current")).value()));

    // The exact boundary. Fresh is `now < expiresAt`, so the instant the TTL
    // names is already expired — no sleeping, no tolerance, no flake.
    clock.advance(1s);
    QVERIFY(!store.isFresh(store.get(QStringLiteral("current")).value()));
}

void TestCacheStore::anImmutableEntryNeverExpires()
{
    FrozenClock clock(QDateTime(QDate(2026, 3, 14), QTime(9, 0), QTimeZone::UTC));
    CacheStore  store(&clock);
    QVERIFY(store.open(QStringLiteral(":memory:")).hasValue());

    const CacheEntry archive = entry(QStringLiteral("era5"), DataKind::HistoricalArchive,
                                     clock.now());
    QVERIFY(!archive.expiresAt.isValid());   // NULL in the column, "never" in the code
    QVERIFY(store.put(archive).hasValue());

    clock.advance(24h * 365 * 5);
    const Result<CacheEntry> read = store.get(QStringLiteral("era5"));
    QVERIFY(read.hasValue());
    QVERIFY(!read.value().expiresAt.isValid());
    QVERIFY(store.isFresh(read.value()));
}

void TestCacheStore::anExpiredAlertIsUnusableAndAnExpiredForecastIsNot()
{
    FrozenClock clock(QDateTime(QDate(2026, 3, 14), QTime(9, 0), QTimeZone::UTC));
    CacheStore  store(&clock);
    QVERIFY(store.open(QStringLiteral(":memory:")).hasValue());

    QVERIFY(store.put(entry(QStringLiteral("f"), DataKind::Forecast, clock.now())).hasValue());
    QVERIFY(store.put(entry(QStringLiteral("a"), DataKind::Alerts, clock.now())).hasValue());

    clock.advance(2h);

    const CacheEntry forecast = store.get(QStringLiteral("f")).value();
    const CacheEntry alert = store.get(QStringLiteral("a")).value();

    QVERIFY(!store.isFresh(forecast));
    QVERIFY(!store.isFresh(alert));

    // Stale-while-revalidate, per kind. An hour-old forecast is the best thing
    // to show on a train; an hour-old severe-weather warning is a lie.
    QVERIFY(store.isUsable(forecast));
    QVERIFY(!store.isUsable(alert));
}

void TestCacheStore::pruneRemovesOnlyWhatMayNotBeShownStale()
{
    FrozenClock clock(QDateTime(QDate(2026, 3, 14), QTime(9, 0), QTimeZone::UTC));
    CacheStore  store(&clock);
    QVERIFY(store.open(QStringLiteral(":memory:")).hasValue());

    QVERIFY(store.put(entry(QStringLiteral("f"), DataKind::Forecast, clock.now())).hasValue());
    QVERIFY(store.put(entry(QStringLiteral("a"), DataKind::Alerts, clock.now())).hasValue());
    QVERIFY(store.put(entry(QStringLiteral("e"), DataKind::HistoricalArchive,
                            clock.now())).hasValue());

    clock.advance(2h);

    const Result<int> pruned = store.pruneUnusable();
    QVERIFY2(pruned.hasValue(), qPrintable(pruned.error().toString()));
    QCOMPARE(pruned.value(), 1);

    QVERIFY(store.get(QStringLiteral("f")).hasValue());
    QVERIFY(store.get(QStringLiteral("e")).hasValue());
    QVERIFY(!store.get(QStringLiteral("a")).hasValue());
}

void TestCacheStore::touchMovesTheExpiryWithoutTouchingThePayload()
{
    // What happens after a 304: the bytes are unchanged and the clock has moved
    // on, so the entry is fresh again without a byte being transferred.
    FrozenClock clock(QDateTime(QDate(2026, 3, 14), QTime(9, 0), QTimeZone::UTC));
    CacheStore  store(&clock);
    QVERIFY(store.open(QStringLiteral(":memory:")).hasValue());

    const CacheEntry written = entry(QStringLiteral("f"), DataKind::Forecast, clock.now());
    QVERIFY(store.put(written).hasValue());

    clock.advance(45min);
    QVERIFY(!store.isFresh(store.get(QStringLiteral("f")).value()));

    QVERIFY(store.touch(QStringLiteral("f"), clock.now(),
                        expiryFor(DataKind::Forecast, clock.now())).hasValue());

    const CacheEntry after = store.get(QStringLiteral("f")).value();
    QVERIFY(store.isFresh(after));
    QCOMPARE(after.payload, written.payload);
    QCOMPARE(after.fetchedAt, clock.now());
}

// ---- places and settings ----------------------------------------------------

void TestCacheStore::placesRoundTripAndKeepFullPrecision()
{
    FrozenClock clock(QDateTime(QDate(2026, 3, 14), QTime(9, 0), QTimeZone::UTC));
    CacheStore  store(&clock);
    QVERIFY(store.open(QStringLiteral(":memory:")).hasValue());

    Place berlin;
    berlin.name = QStringLiteral("Berlin");
    berlin.admin1 = QStringLiteral("Berlin");
    berlin.country = QStringLiteral("Germany");
    berlin.countryCode = QStringLiteral("DE");
    berlin.timezone = QStringLiteral("Europe/Berlin");
    berlin.coordinate = Coordinate{ 52.5200066, 13.4049540 };
    berlin.elevationMetres = 74.0;
    berlin.favourite = true;
    berlin.sortOrder = 0;

    QVERIFY(store.savePlace(berlin).hasValue());
    QVERIFY(berlin.id != 0);
    QCOMPARE(berlin.addedAt, clock.now());

    Place tromso;
    tromso.name = QStringLiteral("Tromsø");
    tromso.countryCode = QStringLiteral("NO");
    tromso.coordinate = Coordinate{ 69.6492, 18.9553 };
    tromso.sortOrder = 1;
    const Status savedTromso = store.savePlace(tromso);
    QVERIFY2(savedTromso.hasValue(), qPrintable(savedTromso.error().toString()));

    const Result<QList<Place>> saved = store.places();
    QVERIFY2(saved.hasValue(), qPrintable(saved.error().toString()));
    QCOMPARE(saved.value().size(), 2);
    QCOMPARE(saved.value().at(0).name, QStringLiteral("Berlin"));

    // Full precision, and this is deliberate: requests round to four decimals,
    // but what the user chose is a different fact from what we ask for and only
    // one of the two can be recovered from the other.
    QCOMPARE(saved.value().at(0).coordinate.latitude, 52.5200066);
    QCOMPARE(saved.value().at(0).elevationMetres.value(), 74.0);
    QVERIFY(saved.value().at(0).favourite);

    // Non-ASCII survives the round trip. It is a weather app; half the place
    // names in Europe have a diacritic in them.
    QCOMPARE(saved.value().at(1).name, QStringLiteral("Tromsø"));

    // An update keeps the id.
    berlin.favourite = false;
    QVERIFY(store.savePlace(berlin).hasValue());
    QCOMPARE(store.places().value().size(), 2);
    QVERIFY(!store.places().value().at(0).favourite);

    QVERIFY(store.removePlace(berlin.id).hasValue());
    QCOMPARE(store.places().value().size(), 1);
}

void TestCacheStore::settingsRoundTrip()
{
    FrozenClock clock;
    CacheStore  store(&clock);
    QVERIFY(store.open(QStringLiteral(":memory:")).hasValue());

    QVERIFY(!store.setting(QStringLiteral("missing")).hasValue());
    QCOMPARE(store.setting(QStringLiteral("missing")).errorKind(), ErrorKind::NotFound);

    QVERIFY(store.setSetting(QStringLiteral("selected-place"), QStringLiteral("7")).hasValue());
    QCOMPARE(store.setting(QStringLiteral("selected-place")).value(), QStringLiteral("7"));

    QVERIFY(store.setSetting(QStringLiteral("selected-place"), QStringLiteral("8")).hasValue());
    QCOMPARE(store.setting(QStringLiteral("selected-place")).value(), QStringLiteral("8"));
}

// ---- the ValidatorStore side ------------------------------------------------

void TestCacheStore::validatorsRoundTripThroughTheStore()
{
    FrozenClock clock(QDateTime(QDate(2026, 3, 14), QTime(9, 0), QTimeZone::UTC));
    CacheStore  store(&clock);
    QVERIFY(store.open(QStringLiteral(":memory:")).hasValue());

    QVERIFY(!store.validatorsFor(QStringLiteral("k")).has_value());

    store.storeValidators(QStringLiteral("k"),
                          Validators{ QByteArrayLiteral("W/\"weak-tag\""),
                                      QByteArrayLiteral("Sat, 14 Mar 2026 09:00:00 GMT"),
                                      clock.now().addSecs(3600) });

    const auto read = store.validatorsFor(QStringLiteral("k"));
    QVERIFY(read.has_value());

    // Byte for byte, weak-comparison prefix and quotes included. An ETag is an
    // opaque token: stripping `W/` to be tidy turns every future 304 into a
    // 200, silently and forever.
    QCOMPARE(read->entityTag, QByteArrayLiteral("W/\"weak-tag\""));
    QCOMPARE(read->lastModified, QByteArrayLiteral("Sat, 14 Mar 2026 09:00:00 GMT"));
    QCOMPARE(read->expires, clock.now().addSecs(3600));
}

void TestCacheStore::aValidatorOnlyRowIsNotACacheHit()
{
    // HttpClient records an ETag the moment a 200 arrives, which is before the
    // caller has parsed the body and decided to keep it. That leaves a row with
    // a NULL payload, and a row with no payload is not a cache entry — reading
    // it as one would hand a parser zero bytes.
    FrozenClock clock;
    CacheStore  store(&clock);
    QVERIFY(store.open(QStringLiteral(":memory:")).hasValue());

    store.storeValidators(QStringLiteral("k"),
                          Validators{ QByteArrayLiteral("\"tag\""), QByteArray(), QDateTime() });

    QVERIFY(store.validatorsFor(QStringLiteral("k")).has_value());

    const Result<CacheEntry> read = store.get(QStringLiteral("k"));
    QVERIFY(!read.hasValue());
    QCOMPARE(read.errorKind(), ErrorKind::NotFound);

    // And a later put() fills the same row in without losing the validator.
    QVERIFY(store.put(entry(QStringLiteral("k"), DataKind::Forecast, clock.now())).hasValue());
    QVERIFY(store.get(QStringLiteral("k")).hasValue());
}

// ---- migrations -------------------------------------------------------------

void TestCacheStore::migrationFromV1ToV2KeepsTheData()
{
    // A real file rather than :memory:, because the point of this test is that
    // a database written by one build is opened by the next one.
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const QString path = directory.filePath(QStringLiteral("cache.sqlite"));

    FrozenClock clock(QDateTime(QDate(2026, 3, 14), QTime(9, 0), QTimeZone::UTC));

    // The v2 the product does not have yet. Passing the migration list in is
    // what makes the *runner* testable without waiting for the schema to need
    // a second version — see libclima/cache/migrations.h.
    QList<Migration> withV2 = defaultMigrations();
    withV2.append(Migration{
        2, QStringLiteral("places.notes"), [](QSqlDatabase &database) -> Status {
            QSqlQuery query(database);
            if (!query.exec(QStringLiteral(
                    "ALTER TABLE places ADD COLUMN notes TEXT NOT NULL DEFAULT ''")))
                return Error(ErrorKind::Storage, query.lastError().text());
            return ok();
        } });

    // Open at v1 and write something worth keeping.
    {
        CacheStore store(&clock);
        QVERIFY2(store.open(path, defaultMigrations()).hasValue(), "v1 open");
        QCOMPARE(store.schemaVersion(), 1);

        Place place;
        place.name = QStringLiteral("Berlin");
        place.coordinate = Coordinate{ 52.52, 13.405 };
        QVERIFY(store.savePlace(place).hasValue());
        QVERIFY(store.put(entry(QStringLiteral("f"), DataKind::Forecast, clock.now())).hasValue());
    }

    // Reopen with v2 available.
    {
        CacheStore store(&clock);
        const Status status = store.open(path, withV2);
        QVERIFY2(status.hasValue(), qPrintable(status.error().toString()));
        QCOMPARE(store.schemaVersion(), 2);

        // Forward-only means the data is carried, not rebuilt.
        const Result<QList<Place>> places = store.places();
        QVERIFY(places.hasValue());
        QCOMPARE(places.value().size(), 1);
        QCOMPARE(places.value().at(0).name, QStringLiteral("Berlin"));

        const Result<CacheEntry> cached = store.get(QStringLiteral("f"));
        QVERIFY(cached.hasValue());
        QCOMPARE(cached.value().payload,
                 QByteArrayLiteral(R"({"hourly":{"temperature_2m":[3.1,3.4]}})"));
    }

    // Reopening at v2 again applies nothing and stays at v2 — migrations run
    // once, and running them twice would fail on the duplicate column.
    {
        CacheStore store(&clock);
        QVERIFY2(store.open(path, withV2).hasValue(), "second v2 open");
        QCOMPARE(store.schemaVersion(), 2);
    }
}

void TestCacheStore::aFailingMigrationRollsBackEntirely()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const QString path = directory.filePath(QStringLiteral("cache.sqlite"));

    FrozenClock clock;

    QList<Migration> broken = defaultMigrations();
    broken.append(Migration{ 2, QStringLiteral("deliberately broken"),
                             [](QSqlDatabase &database) -> Status {
                                 QSqlQuery query(database);
                                 query.exec(QStringLiteral(
                                     "ALTER TABLE places ADD COLUMN notes TEXT DEFAULT ''"));
                                 if (!query.exec(QStringLiteral("SELECT nonsense FROM nowhere")))
                                     return Error(ErrorKind::Storage,
                                                  QStringLiteral("as designed"));
                                 return ok();
                             } });

    {
        CacheStore store(&clock);
        QVERIFY(store.open(path, defaultMigrations()).hasValue());
    }

    {
        CacheStore   store(&clock);
        const Status status = store.open(path, broken);
        QVERIFY(!status.hasValue());
        QCOMPARE(status.errorKind(), ErrorKind::Storage);
        QVERIFY2(status.error().message().contains(QStringLiteral("migration 2")),
                 qPrintable(status.error().message()));
    }

    // Still v1, and still openable. A half-applied migration would leave the
    // file at v2 with a column the code does not know about, or at v1 with a
    // column it does — both unrecoverable without deleting the cache.
    {
        CacheStore store(&clock);
        QVERIFY2(store.open(path, defaultMigrations()).hasValue(), "reopen at v1");
        QCOMPARE(store.schemaVersion(), 1);
    }
}

void TestCacheStore::aDatabaseFromTheFutureIsRefused()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const QString path = directory.filePath(QStringLiteral("cache.sqlite"));

    FrozenClock clock;

    QList<Migration> withV2 = defaultMigrations();
    withV2.append(Migration{ 2, QStringLiteral("places.notes"),
                             [](QSqlDatabase &database) -> Status {
                                 QSqlQuery query(database);
                                 query.exec(QStringLiteral(
                                     "ALTER TABLE places ADD COLUMN notes TEXT DEFAULT ''"));
                                 return ok();
                             } });

    {
        CacheStore store(&clock);
        QVERIFY(store.open(path, withV2).hasValue());
        QCOMPARE(store.schemaVersion(), 2);
    }

    // Now the older build opens it. Forward-only: it is refused rather than
    // written into with a schema that does not match, because two versions
    // disagreeing about a file is the one failure a cache cannot recover from
    // by refetching.
    {
        CacheStore   store(&clock);
        const Status status = store.open(path, defaultMigrations());
        QVERIFY(!status.hasValue());
        QCOMPARE(status.errorKind(), ErrorKind::Storage);
        QVERIFY2(status.error().message().contains(QStringLiteral("newer Clima")),
                 qPrintable(status.error().message()));
    }
}

QTEST_MAIN(TestCacheStore)
#include "tst_cachestore.moc"
