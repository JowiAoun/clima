// SPDX-FileCopyrightText: 2026 Jowi Aoun
// SPDX-License-Identifier: GPL-3.0-or-later
//
// The two things a phase reading cannot say on its own.
//
// `moonPhase` is one number and the moon phase card asks it two questions it
// does not answer:
//
//   * WHICH LIMB IS LIT. The illuminated fraction is symmetric about full — a
//     waxing and a waning gibbous are the same number and mirror images — so a
//     disc drawn from the fraction alone is backwards for half of every month.
//     It reads as nothing at all until you put it beside a moon that is right.
//
//   * WHEN IT IS NEXT FULL. The cycle is 29.5 days and a forecast horizon is
//     sixteen at best, so a little under half the time the answer is past the
//     end of the series and has to be extrapolated. What is asserted here is
//     that the series wins whenever it reaches, and that the extrapolation
//     lands inside one cycle whenever it does not.
//
// The interesting failure in `nextFullMoon` is not arithmetic, it is the pair
// that straddles the NEW moon: phases run 0.97 then 0.02, the difference is
// negative, and a bracket test written on the difference reads that gap as
// containing 0.5. It does not — the moon is dark in the middle of it.

#include "libclima/domain/forecast.h"

#include <QDate>
#include <QTest>

#include <cmath>
#include <utility>

using namespace clima;

namespace {

// A run of days whose phase advances by one mean day per row, starting at
// `from` and at cycle position `phase`. The shape a provider hands over, with
// nothing else on the rows: this function is about the moon and the moon is all
// these rows carry.
QList<DailyPoint> series(const QDate &start, double phase, int days)
{
    QList<DailyPoint> out;
    for (int i = 0; i < days; ++i) {
        DailyPoint day;
        day.date      = start.addDays(i);
        day.moonPhase = std::fmod(phase + i / kSynodicMonth, 1.0);
        out.append(day);
    }
    return out;
}

} // namespace

class TestMoonPhase : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void theLitLimbFollowsTheHalfOfTheCycle();
    void anAbsentPhaseIsDrawnAsWaxing();

    void aFullMoonInsideTheHorizonComesFromTheSeries();
    void theNightItIsFullIsTheNightNearestFull();
    void aFullMoonPastTheHorizonIsExtrapolated();
    void theGapAroundTheNewMoonIsNotReadAsFull();
    void aSeriesWithNoPhaseAtAllHasNoAnswer();
    void everyStartingPhaseLandsInsideOneCycle();
};

// ---- which limb -------------------------------------------------------------------

void TestMoonPhase::theLitLimbFollowsTheHalfOfTheCycle()
{
    QVERIFY(isWaxing(0.01));    // just past new
    QVERIFY(isWaxing(0.25));    // first quarter
    QVERIFY(isWaxing(0.49));

    QVERIFY(!isWaxing(0.51));
    QVERIFY(!isWaxing(0.75));   // last quarter
    QVERIFY(!isWaxing(0.99));

    // Wrapped rather than clamped, the way moonPhaseName wraps it: a provider
    // is entitled to answer 1.0, or a hair over, for a new moon.
    QVERIFY(isWaxing(1.0));
    QVERIFY(isWaxing(1.02));
}

void TestMoonPhase::anAbsentPhaseIsDrawnAsWaxing()
{
    // The default the disc had before the question was asked, kept so that a
    // provider with no moon product draws what it always drew rather than
    // flipping the picture on a missing value.
    QVERIFY(isWaxing(std::nullopt));
}

// ---- the next full moon -----------------------------------------------------------

void TestMoonPhase::aFullMoonInsideTheHorizonComesFromTheSeries()
{
    // Six days short of full, sixteen days of horizon: the series reaches it,
    // so the answer is the provider's own reading and not our mean cycle.
    const QDate start(2026, 3, 1);
    const auto  days = series(start, 0.30, 16);

    const std::optional<QDate> full = nextFullMoon(days, start);
    QVERIFY(full.has_value());

    // 0.5 - 0.30 is 0.20 of a cycle, which is 5.9 days.
    QCOMPARE(*full, start.addDays(6));
}

void TestMoonPhase::theNightItIsFullIsTheNightNearestFull()
{
    // The pair brackets 0.5 at 0.48 and 0.514. The first is 0.02 short and the
    // second 0.014 past, so the second is the night the moon is full — and a
    // rule that simply took the earlier of the bracket would name the first.
    QList<DailyPoint> days;
    for (const auto &[date, phase] : QList<std::pair<QDate, double>>{
             { QDate(2026, 5, 10), 0.446 },
             { QDate(2026, 5, 11), 0.480 },
             { QDate(2026, 5, 12), 0.514 },
             { QDate(2026, 5, 13), 0.548 } }) {
        DailyPoint day;
        day.date      = date;
        day.moonPhase = phase;
        days.append(day);
    }

    QCOMPARE(nextFullMoon(days, QDate(2026, 5, 10)), QDate(2026, 5, 12));
}

void TestMoonPhase::aFullMoonPastTheHorizonIsExtrapolated()
{
    // Full two days ago, so the next one is twenty-seven days out and no
    // sixteen-day series can hold it. The mean cycle answers instead.
    const QDate start(2026, 7, 31);
    const auto  days = series(start, 0.568, 16);

    const std::optional<QDate> full = nextFullMoon(days, start);
    QVERIFY(full.has_value());

    // 1 - 0.068 of a cycle is 27.5 days, and the card prints a date rather than
    // an hour, so what is asserted is the day it rounds to.
    QCOMPARE(*full, start.addDays(28));
}

void TestMoonPhase::theGapAroundTheNewMoonIsNotReadAsFull()
{
    // The defect this guards. 0.97 → 0.01 is a negative difference across a
    // gap that contains new, not full — and a bracket test written on the
    // difference would return the day the moon is DARK.
    QList<DailyPoint> days;
    for (const auto &[date, phase] : QList<std::pair<QDate, double>>{
             { QDate(2026, 9, 1), 0.94 },
             { QDate(2026, 9, 2), 0.97 },
             { QDate(2026, 9, 3), 0.01 },
             { QDate(2026, 9, 4), 0.04 } }) {
        DailyPoint day;
        day.date      = date;
        day.moonPhase = phase;
        days.append(day);
    }

    const std::optional<QDate> full = nextFullMoon(days, QDate(2026, 9, 1));
    QVERIFY(full.has_value());
    QVERIFY2(*full > QDate(2026, 9, 4),
             qPrintable(QStringLiteral("a new moon was reported as full, on %1")
                            .arg(full->toString(Qt::ISODate))));
}

void TestMoonPhase::aSeriesWithNoPhaseAtAllHasNoAnswer()
{
    // MET Norway carries no moon. A card that read a missing phase as a new one
    // would say the next full moon is a fortnight away every day of the month,
    // which is a wrong answer rather than an absent one.
    QList<DailyPoint> days;
    for (int i = 0; i < 10; ++i) {
        DailyPoint day;
        day.date = QDate(2026, 4, 1).addDays(i);
        days.append(day);
    }

    QVERIFY(!nextFullMoon(days, QDate(2026, 4, 1)).has_value());
    QVERIFY(!nextFullMoon({}, QDate(2026, 4, 1)).has_value());
}

void TestMoonPhase::everyStartingPhaseLandsInsideOneCycle()
{
    // Whichever of the two branches answers, the answer is a date between today
    // and one cycle from today. Walked across the whole cycle rather than
    // sampled, because the two branches hand over somewhere in the middle of it
    // and the seam is exactly where an off-by-one would hide.
    const QDate start(2026, 1, 15);

    for (int step = 0; step < 100; ++step) {
        const double phase = step / 100.0;
        const auto   days  = series(start, phase, 16);

        const std::optional<QDate> full = nextFullMoon(days, start);
        QVERIFY2(full.has_value(),
                 qPrintable(QStringLiteral("no answer at phase %1").arg(phase)));

        const qint64 out = start.daysTo(*full);
        QVERIFY2(out >= 0 && out <= 30,
                 qPrintable(QStringLiteral("phase %1 puts the next full moon %2 days out")
                                .arg(phase).arg(out)));
    }
}

QTEST_APPLESS_MAIN(TestMoonPhase)
#include "tst_moonphase.moc"
