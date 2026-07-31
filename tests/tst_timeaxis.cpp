// SPDX-FileCopyrightText: 2026 Jowi Aoun
// SPDX-License-Identifier: GPL-3.0-or-later
//
// The local-time helpers, without a provider in the way.
//
// tests/tst_openmeteoadapter.cpp exercises the same code against recorded
// payloads, which is the test that matters and also the test that would still
// pass if two errors cancelled. These are the properties stated on their own,
// with the inputs written out.

#include "libclima/domain/timeaxis.h"

#include <QTest>

using namespace clima;

namespace {

QTimeZone toronto()
{
    return QTimeZone("America/Toronto");
}

QDateTime utc(int year, int month, int day, int hour, int minute = 0, int second = 0)
{
    return QDateTime(QDate(year, month, day), QTime(hour, minute, second), QTimeZone::UTC);
}

} // namespace

class TestTimeAxis : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void aKnownZoneWins();
    void anUnknownZoneFallsBackToTheOffsetItWasGiven();

    void aNaiveLocalStampIsInvertedExactly();
    void aBareDateMeansLocalMidnight();
    void nothingUnparseableBecomesAnInstant();

    void aFallBackDayHasTwentyFiveHours();
    void aSpringForwardDayHasTwentyThreeHours();

    void minutesAreTheClockReadingAndNotTheElapsedTime();
    void anEventOnAnotherDayCarriesAWholeDayWithIt();
    void anAbsentInstantIsNotMidnight();
};

void TestTimeAxis::aKnownZoneWins()
{
    const QTimeZone zone = zoneFor(QStringLiteral("America/Toronto"), -14400);
    QVERIFY(zone.isValid());
    QCOMPARE(zone.id(), QByteArrayLiteral("America/Toronto"));

    // Which is the whole point: a real zone knows about transitions, and a
    // fixed offset by definition does not.
    QCOMPARE(zone.offsetFromUtc(utc(2026, 7, 15, 12)), -4 * 3600);
    QCOMPARE(zone.offsetFromUtc(utc(2026, 1, 15, 12)), -5 * 3600);
}

void TestTimeAxis::anUnknownZoneFallsBackToTheOffsetItWasGiven()
{
    // An old tzdata, a musl build without one, a provider inventing a name.
    // The fallback is Open-Meteo's own behaviour, so it is no worse than not
    // having tried — and it is a valid QTimeZone, so nothing downstream has to
    // test for it.
    for (const QString &name : { QStringLiteral("Mars/Olympus_Mons"), QString() }) {
        const QTimeZone zone = zoneFor(name, 19800);
        QVERIFY2(zone.isValid(), qPrintable(name));
        QCOMPARE(zone.offsetFromUtc(utc(2026, 7, 15, 12)), 19800);
    }
}

void TestTimeAxis::aNaiveLocalStampIsInvertedExactly()
{
    // The string was produced by adding the offset to a UTC instant, so
    // subtracting the same number recovers it exactly. No zone is consulted,
    // deliberately: asking a real zone to resolve this wall-clock reading
    // would ask it to pick between two answers on a fall-back day.
    QCOMPARE(utcFromNaiveLocal(QStringLiteral("2026-07-30T00:00"), -14400),
             utc(2026, 7, 30, 4));
    QCOMPARE(utcFromNaiveLocal(QStringLiteral("2026-07-30T00:00"), 0),
             utc(2026, 7, 30, 0));

    // A half-hour zone, because Kolkata is where a rounding bug would show.
    QCOMPARE(utcFromNaiveLocal(QStringLiteral("2026-07-30T09:30"), 19800),
             utc(2026, 7, 30, 4));

    // Seconds, which Open-Meteo does not send but a sibling provider might.
    QCOMPARE(utcFromNaiveLocal(QStringLiteral("2026-07-30T00:00:30"), -14400),
             utc(2026, 7, 30, 4, 0, 30));
}

void TestTimeAxis::aBareDateMeansLocalMidnight()
{
    // `daily.time` is written this way.
    QCOMPARE(utcFromNaiveLocal(QStringLiteral("2026-07-30"), -14400), utc(2026, 7, 30, 4));
}

void TestTimeAxis::nothingUnparseableBecomesAnInstant()
{
    // A null moonrise arrives as an empty string once the JSON reader has had
    // it, and it must stay absent rather than becoming the epoch.
    QVERIFY(!utcFromNaiveLocal(QString(), 0).isValid());
    QVERIFY(!utcFromNaiveLocal(QStringLiteral("never"), 0).isValid());
    QVERIFY(!utcFromNaiveLocal(QStringLiteral("2026-13-45T99:99"), 0).isValid());
}

void TestTimeAxis::aFallBackDayHasTwentyFiveHours()
{
    // A uniformly spaced UTC series — which is what Open-Meteo's is, whatever
    // its labels claim — re-expressed in a zone that goes back an hour.
    QList<QDateTime> series;
    for (int hour = 0; hour < 72; ++hour)
        series.append(utc(2025, 11, 1, 4).addSecs(hour * 3600));

    QCOMPARE(indicesOnLocalDate(series, toronto(), QDate(2025, 11, 1)).size(), 24);
    QCOMPARE(indicesOnLocalDate(series, toronto(), QDate(2025, 11, 2)).size(), 25);

    // Contiguous, so a caller wanting a span can take first() and last().
    const QList<int> day = indicesOnLocalDate(series, toronto(), QDate(2025, 11, 2));
    QCOMPARE(day.last() - day.first(), day.size() - 1);
}

void TestTimeAxis::aSpringForwardDayHasTwentyThreeHours()
{
    QList<QDateTime> series;
    for (int hour = 0; hour < 72; ++hour)
        series.append(utc(2026, 3, 7, 5).addSecs(hour * 3600));

    QCOMPARE(indicesOnLocalDate(series, toronto(), QDate(2026, 3, 7)).size(), 24);
    QCOMPARE(indicesOnLocalDate(series, toronto(), QDate(2026, 3, 8)).size(), 23);
    QCOMPARE(indicesOnLocalDate(series, toronto(), QDate(2026, 3, 9)).size(), 24);
}

void TestTimeAxis::minutesAreTheClockReadingAndNotTheElapsedTime()
{
    // An ordinary day: the two readings agree, which is why the difference
    // between them is so easy to miss.
    QCOMPARE(minutesFromLocalMidnight(utc(2026, 7, 30, 10, 4), toronto(), QDate(2026, 7, 30)),
             6 * 60 + 4);

    // The fall-back day, where they do not. Sunrise at 11:55 UTC is 06:55 EST
    // on a wall clock and 475 minutes after local midnight, because an extra
    // hour elapsed on the way. The card says "6:55" and the arc has to agree
    // with the card.
    QCOMPARE(minutesFromLocalMidnight(utc(2025, 11, 2, 11, 55), toronto(), QDate(2025, 11, 2)),
             6 * 60 + 55);

    // The spring-forward day, in the other direction.
    QCOMPARE(minutesFromLocalMidnight(utc(2026, 3, 8, 12, 41), toronto(), QDate(2026, 3, 8)),
             8 * 60 + 41);

    // Rounded rather than truncated, so a sunset at :59 does not lose a minute
    // against the string the provider printed.
    QCOMPARE(minutesFromLocalMidnight(utc(2026, 7, 31, 0, 42, 40), toronto(), QDate(2026, 7, 30)),
             20 * 60 + 43);
}

void TestTimeAxis::anEventOnAnotherDayCarriesAWholeDayWithIt()
{
    // Midnight sun: Open-Meteo reports the sunset as the NEXT day's midnight,
    // and a full arc is 1440 rather than 0.
    const QTimeZone svalbard("Arctic/Longyearbyen");
    QCOMPARE(minutesFromLocalMidnight(utc(2026, 7, 31, 22), svalbard, QDate(2026, 7, 31)),
             1440);

    // A moon that rose before the reference midnight is genuinely negative,
    // and clamping it to zero would put it at dawn.
    QCOMPARE(minutesFromLocalMidnight(utc(2026, 7, 30, 3, 25), toronto(), QDate(2026, 7, 30)),
             -35);
}

void TestTimeAxis::anAbsentInstantIsNotMidnight()
{
    // The moon fails to rise about once a month. If that answered 0 it would
    // be drawn rising at midnight, every month, and look deliberate.
    const int absent = minutesFromLocalMidnight(QDateTime(), toronto(), QDate(2026, 7, 30));
    QVERIFY(!hasMinuteOfDay(absent));
    QVERIFY(hasMinuteOfDay(0));

    QVERIFY(!hasMinuteOfDay(
        minutesFromLocalMidnight(utc(2026, 7, 30, 4), toronto(), QDate())));
}

QTEST_MAIN(TestTimeAxis)
#include "tst_timeaxis.moc"
