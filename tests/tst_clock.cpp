// SPDX-FileCopyrightText: 2026 Jowi Aoun
// SPDX-License-Identifier: GPL-3.0-or-later
//
// The clock, which is the smallest file in the engine and the one the rest of
// the test suite is built on. If FrozenClock is wrong, every TTL test below is
// testing the wrong instant and passing.

#include "libclima/core/clock.h"

#include <QTest>

using namespace clima;
using namespace std::chrono_literals;

class TestClock : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void frozenClockDoesNotMoveOnItsOwn();
    void advanceMovesBothHands();
    void setNowDoesNotMoveTheMonotonicHand();
    void systemClockIsUtcAndMonotonic();
};

void TestClock::frozenClockDoesNotMoveOnItsOwn()
{
    const QDateTime instant(QDate(2026, 3, 14), QTime(9, 26, 53), QTimeZone::UTC);
    FrozenClock     clock(instant);

    QCOMPARE(clock.now(), instant);

    // Deliberately doing something slow between the two reads. A clock that
    // leaked the wall clock would differ here on any machine and pass on a
    // fast one, which is the flake this whole mechanism exists to remove.
    QString churn;
    for (int i = 0; i < 100000; ++i)
        churn += QLatin1Char('x');
    QCOMPARE(churn.size(), 100000);

    QCOMPARE(clock.now(), instant);
    QCOMPARE(clock.elapsed(), 0ms);
}

void TestClock::advanceMovesBothHands()
{
    FrozenClock clock(QDateTime(QDate(2026, 3, 14), QTime(9, 0), QTimeZone::UTC));

    clock.advance(90s);
    QCOMPARE(clock.now(), QDateTime(QDate(2026, 3, 14), QTime(9, 1, 30), QTimeZone::UTC));
    QCOMPARE(clock.elapsed(), 90s);

    clock.advance(30min);
    QCOMPARE(clock.now(), QDateTime(QDate(2026, 3, 14), QTime(9, 31, 30), QTimeZone::UTC));
    QCOMPARE(clock.elapsed(), 90s + 30min);
}

void TestClock::setNowDoesNotMoveTheMonotonicHand()
{
    // The case the two-handed interface exists for: NTP corrects a drifting
    // laptop and the wall clock jumps backwards. A backoff measuring against
    // now() would compute a negative remaining delay and fire every pending
    // retry at once, at exactly the moment a server told us it was unhappy.
    FrozenClock clock(QDateTime(QDate(2026, 3, 14), QTime(9, 0), QTimeZone::UTC));
    clock.advance(5min);

    const auto elapsedBefore = clock.elapsed();
    clock.setNow(QDateTime(QDate(2026, 3, 14), QTime(8, 0), QTimeZone::UTC));

    QCOMPARE(clock.now(), QDateTime(QDate(2026, 3, 14), QTime(8, 0), QTimeZone::UTC));
    QCOMPARE(clock.elapsed(), elapsedBefore);
}

void TestClock::systemClockIsUtcAndMonotonic()
{
    SystemClock clock;

    // UTC, whatever the machine's zone. Every timestamp libclima stores is a
    // UTC instant and a clock that answered in local time would put a cache
    // entry's expiry an hour out for half of Europe.
    QCOMPARE(clock.now().timeSpec(), Qt::UTC);

    const auto first = clock.elapsed();
    QTest::qWait(5);
    const auto second = clock.elapsed();
    QVERIFY(second >= first);
}

QTEST_MAIN(TestClock)
#include "tst_clock.moc"
