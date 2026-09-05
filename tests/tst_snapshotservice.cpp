// SPDX-FileCopyrightText: 2026 Jowi Aoun
// SPDX-License-Identifier: GPL-3.0-or-later
//
// What a daemon that has never fetched says to the first widget that asks.
//
// The widget host subscribes and calls GetSnapshot in the same turn of the
// event loop — see SnapshotService::subscribe for why it cannot be pushed —
// and until warmFromCache() existed the answer in that turn was always empty.
// Not because there was nothing: the cache on disk had yesterday's forecast in
// it. Because fetch() reads the cache through the registry, and every future
// the registry hands back is settled on the event loop, one turn too late for
// the call that was already on the wire.
//
// So the tiles came up saying the weather service was not running, on a
// machine where it was, until the poll five minutes later. docs/known-gaps.md
// carried it as "a widget host that starts cold has nothing to draw".
//
// ---- the test is against the live path, not a fixture ----------------------
//
// Fixture mode would prove nothing here. Its providers answer from memory and
// answer synchronously, so a fixture daemon was never cold. What is exercised
// below is the live construction — real providers, real cache, a real request
// key — with the cache seeded the way a previous run would have left it and
// the network unreachable, which is what a runner is and what NetworkGuard
// guarantees.

#include "daemon/snapshotservice.h"

#include "libclima/cache/cachestore.h"
#include "libclima/cache/payloadcache.h"
#include "libclima/core/clock.h"
#include "libclima/net/httpclient.h"
#include "libclima/net/requestkey.h"
#include "libclima/places/locationcontroller.h"
#include "libclima/providers/openmeteo/openmeteoforecastprovider.h"
#include "support/networkguard.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QStandardPaths>
#include <QTimeZone>
#include <QtTest>

using namespace clima;

namespace {

QByteArray recordedForecast()
{
    QFile file(QStringLiteral(CLIMA_SOURCE_DIR)
               + QStringLiteral("/tests/fixtures/openmeteo/toronto-summer.json"));
    if (!file.open(QIODevice::ReadOnly))
        return {};
    return file.readAll();
}

Place toronto()
{
    Place made;
    made.name        = QStringLiteral("Toronto");
    made.admin1      = QStringLiteral("Ontario");
    made.country     = QStringLiteral("Canada");
    made.countryCode = QStringLiteral("CA");
    made.geonamesId  = 6167865;
    made.timezone    = QStringLiteral("America/Toronto");
    made.coordinate  = Coordinate{ 43.70643, -79.39864 };
    return made;
}

// The request the daemon will make for this place, built the way the daemon
// builds it. The cache key is derived from it, so any drift between this and
// SnapshotService::fetch is a cache miss — which is exactly what the assertion
// below would then report.
ForecastRequest daemonRequest(const Place &place)
{
    ForecastRequest request;
    request.coord    = place.coordinate;
    request.days     = SnapshotService::forecastDays;
    request.timeZone = QTimeZone(place.timezone.toUtf8());
    return request;
}

} // namespace

class TestSnapshotService : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void initTestCase();
    void init();

    void aColdDaemonAnswersFromTheCacheBeforeItsFirstFetch();
    void aColdDaemonWithNothingCachedSaysSo();

private:
    void seedAPreviousRun();
};

void TestSnapshotService::initTestCase()
{
    // The daemon opens CacheStore::defaultDatabasePath(), which is under
    // AppDataLocation. Test mode moves that somewhere harmless before anything
    // computes it.
    QStandardPaths::setTestModeEnabled(true);
    NetworkGuard::install();

    QVERIFY2(!recordedForecast().isEmpty(), "toronto-summer fixture missing");
}

void TestSnapshotService::init()
{
    // Cold means cold: no database from the previous test.
    const QString database = CacheStore::defaultDatabasePath();
    QDir(QFileInfo(database).absolutePath()).removeRecursively();
}

// A previous run of the daemon, as it leaves the disk: a saved home place and
// a forecast payload under the key the provider would have stored it by.
void TestSnapshotService::seedAPreviousRun()
{
    SystemClock clock;
    CacheStore  cache(&clock);
    QVERIFY(cache.open(CacheStore::defaultDatabasePath()).hasValue());

    LocationController places(&cache);
    QVERIFY(places.load().hasValue());
    QCOMPARE(places.addPlace(toronto()), 0);
    QVERIFY(places.placeAt(0).isHome);

    // The key comes from the provider's own request builder rather than from
    // a string written here, so this test cannot pass by agreeing with itself.
    HttpClient                http(&clock);
    OpenMeteoForecastProvider provider(&http, &clock);
    const HttpRequest         wire = provider.buildRequest(daemonRequest(toronto()));
    QVERIFY(wire.coordinate.has_value());

    HttpResponse response;
    response.status      = 200;
    response.body        = recordedForecast();
    response.contentType = QByteArrayLiteral("application/json");
    // Forty minutes ago: stale by the forecast row's thirty-minute TTL, which
    // is the case that matters. A fresh row would be served by fetch() too,
    // one turn late; a stale one is served ONLY by the warm-up.
    response.fetchedAt = clock.now().addSecs(-40 * 60);

    payloadcache::store(&cache, RequestKey::forRequest(wire).toString(), wire.providerId,
                        wire.endpoint, wire.kind, *wire.coordinate, response);
}

void TestSnapshotService::aColdDaemonAnswersFromTheCacheBeforeItsFirstFetch()
{
    seedAPreviousRun();

    SnapshotService service;
    service.configure(QString());

    // Subscribe and GetSnapshot in one turn, which is what DaemonLink does and
    // is the only order the bus allows — there is no event-loop spin between
    // these two lines, and that is the point of the test.
    const QString token = service.subscribe(QStringLiteral("home"), {}, -1, -1);
    QVERIFY(!token.isEmpty());

    const QByteArray json = service.snapshot(QStringLiteral("home"), {}, -1, -1);
    QVERIFY(!json.isEmpty());

    const QJsonObject snapshot = QJsonDocument::fromJson(json).object();

    // "cached", not "live" and not "unknown": a reading that was true forty
    // minutes ago, said so. The tile draws it and ages it.
    QCOMPARE(snapshot.value(QStringLiteral("state")).toString(), QStringLiteral("cached"));
    QCOMPARE(snapshot.value(QStringLiteral("servedBy")).toString(), QStringLiteral("open-meteo"));
    QVERIFY2(snapshot.contains(QStringLiteral("current")),
             "a cached forecast should have put a current reading in the snapshot");
}

void TestSnapshotService::aColdDaemonWithNothingCachedSaysSo()
{
    // The honest half. No previous run, so nothing to warm from — and the
    // snapshot must say "unknown" rather than invent a reading or fail to
    // answer. The place has to exist for there to be a snapshot at all.
    {
        SystemClock clock;
        CacheStore  cache(&clock);
        QVERIFY(cache.open(CacheStore::defaultDatabasePath()).hasValue());
        LocationController places(&cache);
        QVERIFY(places.load().hasValue());
        QCOMPARE(places.addPlace(toronto()), 0);
    }

    SnapshotService service;
    service.configure(QString());

    const QByteArray  json     = service.snapshot(QStringLiteral("home"), {}, -1, -1);
    const QJsonObject snapshot = QJsonDocument::fromJson(json).object();

    QCOMPARE(snapshot.value(QStringLiteral("state")).toString(), QStringLiteral("unknown"));
}

QTEST_GUILESS_MAIN(TestSnapshotService)
#include "tst_snapshotservice.moc"
