// SPDX-FileCopyrightText: 2026 Jowi Aoun
// SPDX-License-Identifier: GPL-3.0-or-later
//
// docs/04-architecture.md §4.5, checked against the code that claims to
// transcribe it.
//
// A caching policy is the kind of table that is never read again once it
// compiles. Every field in it is plausible at every value: a forecast cached
// for three minutes is wasteful and invisible, one cached for thirty days is
// wrong and looks identical from the outside until somebody notices the
// temperature has not moved since Tuesday.
//
// ---- the row that is a safety property and not a preference ------------------
//
// One line in this file matters more than the rest of it put together:
//
//     Alerts.staleWhileRevalidate == false
//
// Every other kind may be served past its TTL while a fresh copy is fetched,
// because "the UI must never show an empty screen because an API is down" is
// design principle 1. Alerts are the exception, and §4.5 marks that row with a
// warning sign and the words "never show an expired alert" — showing a stale
// forecast reads as "updated 25 minutes ago", and showing a stale tornado
// warning reads as a tornado warning.
//
// So it is asserted twice below: once as that row's value, and once as a
// property of the whole table — Alerts is the ONLY kind with the flag off. The
// second form is the one that survives a well-meant "make the caching
// consistent" pass, because it fails when a second row is turned off just as
// loudly as when this one is turned on.
//
// ---- and the one that is a database property --------------------------------
//
// `dataKindName` is stored in the cache's `kind` column as text. A round trip
// that loses a kind is a cache that reads a row back as the wrong sort of
// thing, and cachepolicy.h is explicit that text was chosen over the enum's
// integer precisely so inserting a value in the middle of the enum cannot
// silently reinterpret every stored row.

#include "libclima/cache/cachepolicy.h"

#include <QSet>
#include <QTest>

#include <chrono>

using namespace clima;
using namespace std::chrono_literals;

namespace {

// Every kind, as a list, so a loop cannot fall out of step with the enum. There
// is no Count sentinel on DataKind — adding one now would be changing the
// subject to suit the test — so this is written out and the totality check
// below is what catches a kind added without a row here.
QList<DataKind> allKinds()
{
    return {
        DataKind::CurrentConditions,
        DataKind::Forecast,
        DataKind::Nowcast,
        DataKind::Ensemble,
        DataKind::AirQuality,
        DataKind::Alerts,
        DataKind::RadarFrame,
        DataKind::BasemapTile,
        DataKind::HistoricalArchive,
        DataKind::Geocoding,
    };
}

const QDateTime kFetchedAt{ QDate(2026, 8, 5), QTime(14, 5), QTimeZone::UTC };

} // namespace

class TestCachePolicy : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void theTableIsTheOneInTheArchitectureDocument_data();
    void theTableIsTheOneInTheArchitectureDocument();

    void alertsAreTheOnlyKindThatMayNotBeServedStale();
    void alertsDeferToTheCapMessagesOwnLifetime();
    void onlyTheEndpointsWithAValidatorAskForOne();

    void theArchiveIsTheOnlyImmutableKind();
    void anImmutableKindHasNoExpiryRatherThanADistantOne();
    void anImmutableKindLeavesItsTtlAtZeroSoAReaderCannotBelieveIt();

    void everyKindHasItsOwnStableName();
    void everyNameSurvivesTheRoundTripThroughTheDatabase_data();
    void everyNameSurvivesTheRoundTripThroughTheDatabase();
    void aNameFromANewerClimaIsRejectedRatherThanGuessed();
    void theOkPointerIsOptional();
    void aKindOutsideTheEnumIsNamedRatherThanLeftEmpty();

    void expiryIsFetchTimePlusTtl_data();
    void expiryIsFetchTimePlusTtl();
    void anInvalidFetchTimeCannotProduceAValidExpiry();
    void expiryKeepsTheTimeZoneItWasGiven();
};

// ============================================================================
// The table itself, transcribed a second time — deliberately, and from the
// document rather than from the code. Two independent transcriptions of one
// source disagree loudly; a test that read policyFor() to decide what
// policyFor() should return would agree with anything.
// ============================================================================

void TestCachePolicy::theTableIsTheOneInTheArchitectureDocument_data()
{
    QTest::addColumn<int>("kind");
    QTest::addColumn<int>("ttlSeconds");
    QTest::addColumn<int>("revalidation");
    QTest::addColumn<bool>("staleWhileRevalidate");
    QTest::addColumn<bool>("immutable");

    const auto row = [](const char *name, DataKind kind, std::chrono::seconds ttl,
                        Revalidation revalidation, bool swr, bool immutable) {
        QTest::newRow(name) << int(kind) << int(ttl.count()) << int(revalidation)
                            << swr << immutable;
    };

    //     §4.5 "Data"                  TTL      Revalidation             SWR
    row("current conditions", DataKind::CurrentConditions,
        10min,    Revalidation::EntityTag,   true,  false);
    row("hourly / daily forecast", DataKind::Forecast,
        30min,    Revalidation::EntityTag,   true,  false);
    row("15-minute nowcast", DataKind::Nowcast,
        5min,     Revalidation::None,        true,  false);
    row("ensemble / model comparison", DataKind::Ensemble,
        60min,    Revalidation::None,        true,  false);
    row("air quality", DataKind::AirQuality,
        60min,    Revalidation::None,        true,  false);
    row("alerts", DataKind::Alerts,
        3min,     Revalidation::CapLifetime, false, false);
    row("radar frames", DataKind::RadarFrame,
        5min,     Revalidation::None,        true,  false);
    row("basemap tiles", DataKind::BasemapTile,
        24h * 30, Revalidation::None,        true,  false);
    row("historical archive / ERA5", DataKind::HistoricalArchive,
        0s,       Revalidation::None,        true,  true);
    row("geocoding results", DataKind::Geocoding,
        24h * 7,  Revalidation::None,        true,  false);
}

void TestCachePolicy::theTableIsTheOneInTheArchitectureDocument()
{
    QFETCH(int, kind);
    QFETCH(int, ttlSeconds);
    QFETCH(int, revalidation);
    QFETCH(bool, staleWhileRevalidate);
    QFETCH(bool, immutable);

    const CachePolicy policy = policyFor(static_cast<DataKind>(kind));

    QCOMPARE(int(policy.ttl.count()), ttlSeconds);
    QCOMPARE(int(policy.revalidation), revalidation);
    QCOMPARE(policy.staleWhileRevalidate, staleWhileRevalidate);
    QCOMPARE(policy.immutable, immutable);
}

// ============================================================================
// The safety property, as a property of the whole table.
// ============================================================================

void TestCachePolicy::alertsAreTheOnlyKindThatMayNotBeServedStale()
{
    QList<DataKind> refused;
    for (DataKind kind : allKinds()) {
        if (!policyFor(kind).staleWhileRevalidate)
            refused.append(kind);
    }

    QCOMPARE(refused.size(), 1);
    QCOMPARE(refused.constFirst(), DataKind::Alerts);

    // Said the other way round as well, because the two failures read
    // differently: this one fires when somebody turns the alert row back on,
    // the count above fires when somebody turns a second row off.
    QVERIFY2(!policyFor(DataKind::Alerts).staleWhileRevalidate,
             "§4.5: never show an expired alert");
}

void TestCachePolicy::alertsDeferToTheCapMessagesOwnLifetime()
{
    // The three-minute TTL decides how often we ASK. What decides how long a
    // warning is valid is the CAP message's own <sent>/<expires>, and
    // Revalidation::CapLifetime is how that is said in this table — anything
    // else here would mean a computed expiry could outlive an issuer's.
    QCOMPARE(policyFor(DataKind::Alerts).revalidation, Revalidation::CapLifetime);
    QCOMPARE(policyFor(DataKind::Alerts).ttl, std::chrono::seconds(3min));

    // And nothing else uses it. CapLifetime is meaningless for a payload with
    // no CAP message in it.
    for (DataKind kind : allKinds()) {
        if (kind == DataKind::Alerts)
            continue;
        QVERIFY2(policyFor(kind).revalidation != Revalidation::CapLifetime,
                 qPrintable(dataKindName(kind) + QStringLiteral(" claims a CAP lifetime")));
    }
}

void TestCachePolicy::onlyTheEndpointsWithAValidatorAskForOne()
{
    // A conditional request against an endpoint that sends no validator is a
    // full-price fetch with an extra header on it. §4.5 gives ETags to the two
    // forecast rows and to nothing else — verified against the live services
    // and recorded in tests/fixtures/alerts/README.md, which notes that
    // api.weather.gc.ca sends no ETag, no Last-Modified and no Cache-Control.
    QSet<int> tagged;
    for (DataKind kind : allKinds()) {
        if (policyFor(kind).revalidation == Revalidation::EntityTag)
            tagged.insert(int(kind));
    }

    QCOMPARE(tagged, QSet<int>({ int(DataKind::CurrentConditions), int(DataKind::Forecast) }));
}

// ============================================================================
// Immutability. cachepolicy.h argues at length that "cache forever" must be a
// flag and not a very large TTL, because "the day one of them overflows or one
// of them is compared with `<` instead of `<=` there is no test that notices".
// This is that test.
// ============================================================================

void TestCachePolicy::theArchiveIsTheOnlyImmutableKind()
{
    // Not named `forever`, however much it wants to be: qglobal.h defines that
    // as `for (;;)` unless QT_NO_KEYWORDS is set, and this build does not set
    // it. The declaration expands to a for-loop and the errors point at the
    // three lines after it.
    QList<DataKind> immutableKinds;
    for (DataKind kind : allKinds()) {
        if (policyFor(kind).immutable)
            immutableKinds.append(kind);
    }

    QCOMPARE(immutableKinds.size(), 1);
    QCOMPARE(immutableKinds.constFirst(), DataKind::HistoricalArchive);
}

void TestCachePolicy::anImmutableKindHasNoExpiryRatherThanADistantOne()
{
    // An INVALID QDateTime, which SQLite stores as NULL and every read path
    // already handles. Not a year in the far future, which every freshness
    // comparison would then have to do arithmetic on.
    const QDateTime expiry = expiryFor(DataKind::HistoricalArchive, kFetchedAt);
    QVERIFY2(!expiry.isValid(),
             qPrintable(QStringLiteral("immutable expiry came back as %1")
                            .arg(expiry.toString(Qt::ISODate))));

    // And every other kind does get a real one, so the invalid answer above
    // means "never" rather than "something went wrong".
    for (DataKind kind : allKinds()) {
        if (kind == DataKind::HistoricalArchive)
            continue;
        QVERIFY2(expiryFor(kind, kFetchedAt).isValid(),
                 qPrintable(dataKindName(kind) + QStringLiteral(" has no expiry")));
    }
}

void TestCachePolicy::anImmutableKindLeavesItsTtlAtZeroSoAReaderCannotBelieveIt()
{
    // "`ttl` is meaningless when this is set and is left at zero so that a
    // caller who reads it anyway gets an obviously wrong answer rather than a
    // plausible one" — cachepolicy.h. A caller that ignores `immutable` and
    // adds the TTL gets the fetch instant back, which is expired, which fails
    // in the direction of refetching rather than of caching forever by
    // accident.
    const CachePolicy policy = policyFor(DataKind::HistoricalArchive);
    QVERIFY(policy.immutable);
    QCOMPARE(policy.ttl.count(), 0);
}

// ============================================================================
// The names, which are a database schema.
// ============================================================================

void TestCachePolicy::everyKindHasItsOwnStableName()
{
    QSet<QString> seen;
    for (DataKind kind : allKinds()) {
        const QString name = dataKindName(kind);

        QVERIFY2(!name.isEmpty(), "a kind with no name cannot be stored");

        // "unknown" is the fallback for a value outside the enum. A real kind
        // reaching it means a switch lost a case, and every row that kind ever
        // wrote is now indistinguishable from every other lost kind's.
        QVERIFY2(name != QStringLiteral("unknown"),
                 qPrintable(QStringLiteral("kind %1 falls through to \"unknown\"")
                                .arg(int(kind))));

        QVERIFY2(!seen.contains(name),
                 qPrintable(QStringLiteral("two kinds are both stored as \"%1\"").arg(name)));
        seen.insert(name);
    }

    QCOMPARE(seen.size(), allKinds().size());
}

void TestCachePolicy::everyNameSurvivesTheRoundTripThroughTheDatabase_data()
{
    QTest::addColumn<int>("kind");
    QTest::addColumn<QString>("name");

    // Spelled out rather than taken from dataKindName(), so that renaming a
    // kind fails here. These strings are on disk in every user's cache; a
    // rename is a migration, not an edit.
    const struct { DataKind kind; const char *name; } rows[] = {
        { DataKind::CurrentConditions, "current"            },
        { DataKind::Forecast,          "forecast"           },
        { DataKind::Nowcast,           "nowcast"            },
        { DataKind::Ensemble,          "ensemble"           },
        { DataKind::AirQuality,        "air-quality"        },
        { DataKind::Alerts,            "alerts"             },
        { DataKind::RadarFrame,        "radar-frame"        },
        { DataKind::BasemapTile,       "basemap-tile"       },
        { DataKind::HistoricalArchive, "historical-archive" },
        { DataKind::Geocoding,         "geocoding"          },
    };

    for (const auto &row : rows)
        QTest::newRow(row.name) << int(row.kind) << QString::fromLatin1(row.name);
}

void TestCachePolicy::everyNameSurvivesTheRoundTripThroughTheDatabase()
{
    QFETCH(int, kind);
    QFETCH(QString, name);

    QCOMPARE(dataKindName(static_cast<DataKind>(kind)), name);

    bool ok = false;
    QCOMPARE(int(dataKindFromName(name, &ok)), kind);
    QVERIFY(ok);
}

void TestCachePolicy::aNameFromANewerClimaIsRejectedRatherThanGuessed()
{
    // "An unrecognised name is a row written by a newer Clima than this one, or
    // by a corrupted file. Reporting it as Forecast with ok=false lets the
    // caller drop the row rather than crash, and callers do drop it."
    //
    // The value matters less than the flag, but both are asserted: a caller
    // that forgets to check `ok` gets a Forecast policy, which is a
    // thirty-minute TTL with revalidation — the safe direction for an unknown
    // payload, not a thirty-day one.
    for (const QString &name : { QStringLiteral("nowcast-v2"), QStringLiteral(""),
                                 QStringLiteral("Forecast"), QStringLiteral("ALERTS"),
                                 QStringLiteral("radar frame") }) {
        bool ok = true;
        const DataKind kind = dataKindFromName(name, &ok);
        QVERIFY2(!ok, qPrintable(QStringLiteral("\"%1\" was accepted").arg(name)));
        QCOMPARE(kind, DataKind::Forecast);
    }

    // Case sensitive, and that is the point of the two rows above that differ
    // only in case: these are exact strings in a column, not a user's input.
    bool ok = false;
    dataKindFromName(QStringLiteral("forecast"), &ok);
    QVERIFY(ok);
}

void TestCachePolicy::theOkPointerIsOptional()
{
    // It defaults to nullptr, and the loop that fills it must not dereference
    // one. Two calls, one hit and one miss, because the write happens on both
    // paths and only one of them is on the common route.
    QCOMPARE(dataKindFromName(QStringLiteral("alerts")), DataKind::Alerts);
    QCOMPARE(dataKindFromName(QStringLiteral("nothing at all")), DataKind::Forecast);
}

void TestCachePolicy::aKindOutsideTheEnumIsNamedRatherThanLeftEmpty()
{
    // A value read back from a corrupt row, or from a build that had one more
    // kind than this one. dataKindName() falls out of its switch and answers
    // "unknown", which is a string a log line can carry; an empty one would
    // read as a missing field.
    QCOMPARE(dataKindName(static_cast<DataKind>(99)), QStringLiteral("unknown"));

    // And the policy for it caches nothing and revalidates nothing, which fails
    // safe: it fetches every time.
    const CachePolicy policy = policyFor(static_cast<DataKind>(99));
    QCOMPARE(policy.ttl.count(), 0);
    QCOMPARE(policy.revalidation, Revalidation::None);
    QVERIFY(!policy.immutable);
    QVERIFY(!policy.staleWhileRevalidate);
}

// ============================================================================
// expiryFor(), which is two lines and one of them is the immutable check.
// ============================================================================

void TestCachePolicy::expiryIsFetchTimePlusTtl_data()
{
    QTest::addColumn<int>("kind");
    QTest::addColumn<int>("minutes");

    QTest::newRow("current")      << int(DataKind::CurrentConditions) << 10;
    QTest::newRow("forecast")     << int(DataKind::Forecast)          << 30;
    QTest::newRow("nowcast")      << int(DataKind::Nowcast)           << 5;
    QTest::newRow("alerts")       << int(DataKind::Alerts)            << 3;
    QTest::newRow("air-quality")  << int(DataKind::AirQuality)        << 60;
    QTest::newRow("basemap-tile") << int(DataKind::BasemapTile)       << 30 * 24 * 60;
    QTest::newRow("geocoding")    << int(DataKind::Geocoding)         << 7 * 24 * 60;
}

void TestCachePolicy::expiryIsFetchTimePlusTtl()
{
    QFETCH(int, kind);
    QFETCH(int, minutes);

    QCOMPARE(expiryFor(static_cast<DataKind>(kind), kFetchedAt),
             kFetchedAt.addSecs(qint64(minutes) * 60));
}

void TestCachePolicy::anInvalidFetchTimeCannotProduceAValidExpiry()
{
    // A row whose `fetched_at` column was NULL or unparseable. QDateTime::
    // addSecs on an invalid instant stays invalid, so such a row reads as
    // "never expires" to a caller testing validity — which is why every read
    // path checks the fetch instant too, and why this is asserted rather than
    // assumed: the failure is a cache entry that is never refreshed again.
    const QDateTime nothing;
    for (DataKind kind : allKinds())
        QVERIFY2(!expiryFor(kind, nothing).isValid(),
                 qPrintable(dataKindName(kind)));
}

void TestCachePolicy::expiryKeepsTheTimeZoneItWasGiven()
{
    // addSecs preserves the time spec, and the cache compares expiry against
    // Clock::now(), which is UTC. A local-time expiry compared against a UTC
    // now is wrong by the offset — five hours in Toronto, which is ten TTLs for
    // current conditions.
    const QDateTime local{ QDate(2026, 8, 5), QTime(14, 5), QTimeZone("America/Toronto") };
    const QDateTime expiry = expiryFor(DataKind::CurrentConditions, local);

    QCOMPARE(expiry.timeZone(), local.timeZone());
    QCOMPARE(expiry.toUTC(), local.toUTC().addSecs(10 * 60));
}

QTEST_MAIN(TestCachePolicy)
#include "tst_cachepolicy.moc"
