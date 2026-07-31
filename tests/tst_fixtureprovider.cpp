// SPDX-FileCopyrightText: 2026 Jowi Aoun
// SPDX-License-Identifier: GPL-3.0-or-later
//
// The fixtures the app replays, and the one number in them that decides whether
// every rain band in the product is drawn an hour late.
//
// ============================================================================
// WHY THE PRECIPITATION HOUR IS ASSERTED HERE AS WELL AS IN THE ADAPTER TEST
//
// tests/tst_openmeteoadapter.cpp already pins Kampala's isolated wet hour, and
// that assertion is about the ADAPTER: it proves the parser puts 0.40 mm on the
// sample stamped 10:00, which is Open-Meteo's preceding-hour convention read
// correctly.
//
// This one is about the fixture reaching the app with that fact intact. Between
// the two lives a shift — libclima/domain/hourconvention.h — that has to happen
// exactly once, and the failure when it does not is invisible: the chart draws,
// the axis is right, the curve is right, and the wash sits one column over. The
// forecast says it starts raining at ten when it starts raining at nine, and
// the only way anyone finds out is by getting wet.
//
// So: the recorded bytes, through the real provider, shifted the way the view
// model shifts them, and then an assertion about which hour is wet.
//
// ============================================================================
// AND THE FROZEN INSTANT, TO THE MINUTE
//
// `recordedAt` is what `--fixture` sets the FrozenClock to, and it is the only
// input to which hour is "Now", which hours are behind the past veil, whether
// it is day or night, and which of the four sky phases the phone paints. Every
// committed screenshot is a photograph of that number. Pinning it here is what
// turns "somebody edited a manifest" from a silent re-rendering of every golden
// image into a failing test.

#include "libclima/domain/hourconvention.h"
#include "libclima/providers/fixture/fixtureprovider.h"

#include "support/networkguard.h"

#include <QTest>
#include <QTimeZone>

using namespace clima;

class TestFixtureProvider : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void initTestCase();

    void theCatalogueIsTheDirectoryListing();
    void torontoIsFrozenAtTheInstantTheMockDescribed();
    void everyFixtureParses();

    void kampalaHasOneWetHourAndItIsNineInTheMorning();

    void aFixtureWithNoAirQualityHidesTheCardRatherThanEmptyingIt();
    void berlinHasPollenAndTorontoDoesNot();

    void theCreditIsOpenMeteosAndSaysItIsARecording();

    // Last, so that it sees everything the others did.
    void nothingReachedTheNetwork();

private:
    static Forecast forecastOf(const QString &name);
};

void TestFixtureProvider::initTestCase()
{
    // Nothing here may reach a network. The fixture provider opens no socket by
    // construction; the guard is what makes that a proof rather than a claim,
    // and `externalAttempts()` is checked at the end of the run.
    NetworkGuard::install();
}

Forecast TestFixtureProvider::forecastOf(const QString &name)
{
    FixtureForecastProvider provider(fixtures::load(name));

    ForecastRequest request;
    request.coord = fixtures::load(name).place.coordinate;

    QFuture<Result<Forecast>> future = provider.fetchForecast(request);

    // Already finished when it returns — no event loop, no thread. That is the
    // property the app's first frame depends on, so it is worth asserting
    // rather than waiting on.
    Q_ASSERT(future.isFinished());

    const Result<Forecast> result = future.result();
    if (!result)
        return {};
    return result.value();
}

void TestFixtureProvider::theCatalogueIsTheDirectoryListing()
{
    const QStringList names = fixtures::names();

    QVERIFY(names.contains(QStringLiteral("toronto")));
    QVERIFY(names.contains(QStringLiteral("berlin")));
    QVERIFY(names.contains(QStringLiteral("kampala")));

    QVERIFY(fixtures::exists(QStringLiteral("toronto")));
    QVERIFY(!fixtures::exists(QStringLiteral("atlantis")));
    QVERIFY(!fixtures::exists(QString()));

    // The default has to be one of them or `--grab` comes up empty.
    QVERIFY(names.contains(fixtures::defaultName()));
}

void TestFixtureProvider::torontoIsFrozenAtTheInstantTheMockDescribed()
{
    const Fixture fixture = fixtures::load(QStringLiteral("toronto"));
    QVERIFY(fixture.isValid());

    // 2026-07-30, 12:28 PM in Toronto — the observation app/qml/Clima's mock
    // data always claimed to be describing, and therefore the instant that
    // keeps the committed screenshots comparable to the ones taken before there
    // was any live data.
    QCOMPARE(fixture.recordedAt,
             QDateTime(QDate(2026, 7, 30), QTime(16, 28), QTimeZone::UTC));

    QCOMPARE(fixture.place.name, QStringLiteral("Toronto"));
    QCOMPARE(fixture.place.timezone, QStringLiteral("America/Toronto"));
}

void TestFixtureProvider::everyFixtureParses()
{
    for (const QString &name : fixtures::names()) {
        const Forecast forecast = forecastOf(name);

        QVERIFY2(!forecast.isEmpty(), qPrintable(name));
        QVERIFY2(forecast.hourly.size() >= 48, qPrintable(name));
        QVERIFY2(forecast.timeZone.isValid(), qPrintable(name));

        // The clock the app will be frozen at has to fall inside the series, or
        // the window has no "now" in it and the whole page is a forecast for
        // somewhere in the past.
        const Fixture fixture = fixtures::load(name);
        QVERIFY2(forecast.hourly.constFirst().time <= fixture.recordedAt, qPrintable(name));
        QVERIFY2(forecast.hourly.constLast().time > fixture.recordedAt, qPrintable(name));

        // And it must carry the timestamp the bytes were captured at, not the
        // moment they were read — that is what "updated N minutes ago" reads.
        QCOMPARE(forecast.fetchedAt, fixture.recordedAt);
    }
}

// ---- THE PRECIPITATION HOUR --------------------------------------------------

void TestFixtureProvider::kampalaHasOneWetHourAndItIsNineInTheMorning()
{
    const Fixture  fixture  = fixtures::load(QStringLiteral("kampala"));
    const Forecast forecast = forecastOf(QStringLiteral("kampala"));
    QVERIFY(!forecast.isEmpty());

    const QTimeZone zone(fixture.place.timezone.toUtf8());
    QVERIFY(zone.isValid());

    // ---- as the provider reports it: the hour ENDING at 10:00 --------------
    int wetEnding = -1;
    for (int i = 0; i < forecast.hourly.size(); ++i) {
        if (forecast.hourly.at(i).precipitation.value_or(0.0) >= 0.4) {
            QCOMPARE(wetEnding, -1);   // exactly one, which is what makes this a ruler
            wetEnding = i;
        }
    }
    QVERIFY(wetEnding > 0);
    QCOMPARE(forecast.hourly.at(wetEnding).time.toTimeZone(zone).time().hour(), 10);

    // Dry on both sides. An off-by-one is unmistakable against an isolated
    // spike and invisible inside a long band.
    QCOMPARE(forecast.hourly.at(wetEnding - 1).precipitation.value_or(0.0), 0.0);
    QCOMPARE(forecast.hourly.at(wetEnding + 1).precipitation.value_or(0.0), 0.0);

    // ---- as the chart draws it: the hour STARTING at 09:00 -----------------
    //
    // This is the shift app/viewmodels/forecastdata.cpp applies once, on the way
    // to the UI, and the assertion is the whole point of this file: the same
    // 0.40 mm, one index earlier, stamped 09:00.
    const QList<HourlyPoint> shifted = asHourStarting(forecast.hourly);

    int wetStarting = -1;
    for (int i = 0; i < shifted.size(); ++i) {
        if (shifted.at(i).precipitation.value_or(0.0) >= 0.4) {
            QCOMPARE(wetStarting, -1);
            wetStarting = i;
        }
    }
    QVERIFY(wetStarting >= 0);

    QCOMPARE(wetStarting, wetEnding - 1);
    QCOMPARE(shifted.at(wetStarting).time.toTimeZone(zone).time().hour(), 9);
    QCOMPARE(shifted.at(wetStarting).precipitation.value_or(0.0), 0.4);

    // And the hour it is drawn on is in the FUTURE at the frozen clock, so the
    // band is not under the past veil in the committed capture. 07:28 local
    // against a 09:00 spell.
    QVERIFY(shifted.at(wetStarting).time > fixture.recordedAt);
}

// ---- absent products ----------------------------------------------------------

void TestFixtureProvider::aFixtureWithNoAirQualityHidesTheCardRatherThanEmptyingIt()
{
    const Fixture kampala = fixtures::load(QStringLiteral("kampala"));
    QVERIFY(kampala.airQuality.isEmpty());

    FixtureAirQualityProvider provider(kampala);

    // Known-absent, not undetermined: there is no later fetch that could change
    // the answer, and a card that waits for ever for a payload that is never
    // coming is worse than one that is simply not there.
    const Capabilities here = provider.capabilitiesAt(kampala.place.coordinate);
    QVERIFY(!here.has(Capability::AirQualityIndex));
    QVERIFY(!here.isUndetermined(Capability::AirQualityIndex));
    QVERIFY(here.isKnownAbsent(Capability::AirQualityIndex));

    QVERIFY(!provider.covers(kampala.place.coordinate));
}

void TestFixtureProvider::berlinHasPollenAndTorontoDoesNot()
{
    const Fixture berlin  = fixtures::load(QStringLiteral("berlin"));
    const Fixture toronto = fixtures::load(QStringLiteral("toronto"));

    FixtureAirQualityProvider inEurope(berlin);
    FixtureAirQualityProvider inCanada(toronto);

    // The pair is the whole of docs/08-risks.md R9 in two lines: CAMS produces
    // pollen for its European domain and nowhere else, the payload is the
    // witness, and the card is hidden rather than drawn empty.
    QVERIFY(inEurope.capabilitiesAt(berlin.place.coordinate).has(Capability::Pollen));
    QVERIFY(inCanada.capabilitiesAt(toronto.place.coordinate).isKnownAbsent(Capability::Pollen));

    // Both have the indices and the pollutants, which are global.
    QVERIFY(inEurope.capabilitiesAt(berlin.place.coordinate).has(Capability::AirQualityIndex));
    QVERIFY(inCanada.capabilitiesAt(toronto.place.coordinate).has(Capability::AirQualityIndex));
}

void TestFixtureProvider::nothingReachedTheNetwork()
{
    // The claim this whole class is built on, asserted rather than assumed.
    QCOMPARE(NetworkGuard::externalAttempts(), QStringList());
}

void TestFixtureProvider::theCreditIsOpenMeteosAndSaysItIsARecording()
{
    const FixtureForecastProvider provider(fixtures::load(QStringLiteral("toronto")));
    const Attribution             credit = provider.attribution();

    // Complete, or ProviderRegistry::add() would refuse it — which is the gate
    // that makes the About screen impossible to leave stale.
    QVERIFY2(credit.isComplete(), qPrintable(credit.firstMissingField()));

    // The numbers are Open-Meteo's and the credit line is theirs verbatim. What
    // is ours is the note, which has to say this is a photograph and of when.
    QCOMPARE(credit.name, QStringLiteral("Open-Meteo"));
    QCOMPARE(credit.creditLine, QStringLiteral("Weather data by Open-Meteo.com"));
    QVERIFY(credit.note.contains(QStringLiteral("Recorded")));
    QVERIFY(credit.note.contains(QStringLiteral("2026-07-30")));
}

QTEST_MAIN(TestFixtureProvider)
#include "tst_fixtureprovider.moc"
