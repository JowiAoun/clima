// SPDX-FileCopyrightText: 2026 Jowi Aoun
// SPDX-License-Identifier: GPL-3.0-or-later
//
// The retry schedule, tested as arithmetic rather than as elapsed time. Nothing
// here sleeps: a backoff test that waits for its own delays is a test that
// takes thirty minutes to prove the cap.

#include "libclima/net/backoff.h"

#include <QSet>
#include <QTest>

using namespace clima;
using namespace std::chrono_literals;

class TestBackoff : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void ceilingDoubles();
    void ceilingIsCappedAtThirtyMinutes();
    void delayStaysInsideItsWindow();
    void jitterActuallySpreads();
    void sameSeedSameSchedule();
    void retryAfterIsClamped();
    void retryCountIsBounded();
};

void TestBackoff::ceilingDoubles()
{
    Backoff backoff(BackoffPolicy{ 1000ms, 2.0, 30min, 5 }, 1);

    QCOMPARE(backoff.ceilingForRetry(0), 1000ms);
    QCOMPARE(backoff.ceilingForRetry(1), 2000ms);
    QCOMPARE(backoff.ceilingForRetry(2), 4000ms);
    QCOMPARE(backoff.ceilingForRetry(3), 8000ms);
    QCOMPARE(backoff.ceilingForRetry(4), 16000ms);
}

void TestBackoff::ceilingIsCappedAtThirtyMinutes()
{
    Backoff backoff(BackoffPolicy{ 1000ms, 2.0, 30min, 100 }, 1);

    // Unbounded doubling reaches half an hour at retry 11 and would be past a
    // day by retry 17. The cap is what keeps a provider outage from parking a
    // location for longer than the outage lasted.
    QCOMPARE(backoff.ceilingForRetry(11), 30min);
    QCOMPARE(backoff.ceilingForRetry(40), 30min);

    // And it does not go negative. The naive `base << retry` overflows here;
    // the double-then-clamp in ceilingForRetry saturates instead.
    QCOMPARE(backoff.ceilingForRetry(1000), 30min);
    QVERIFY(backoff.ceilingForRetry(1000).count() > 0);
}

void TestBackoff::delayStaysInsideItsWindow()
{
    Backoff backoff(BackoffPolicy{ 1000ms, 2.0, 30min, 5 }, 20260731);

    for (int retry = 0; retry < 8; ++retry) {
        for (int draw = 0; draw < 200; ++draw) {
            const auto delay = backoff.delayForRetry(retry);
            QVERIFY2(delay >= 0ms, qPrintable(QStringLiteral("negative delay at retry %1")
                                                  .arg(retry)));
            QVERIFY2(delay <= backoff.ceilingForRetry(retry),
                     qPrintable(QStringLiteral("delay %1 ms exceeded its ceiling %2 ms at retry %3")
                                    .arg(delay.count())
                                    .arg(backoff.ceilingForRetry(retry).count())
                                    .arg(retry)));
        }
    }
}

void TestBackoff::jitterActuallySpreads()
{
    // Full jitter, not "the ceiling plus or minus a bit". The property that
    // matters when a thousand clients see the same 503 is that their delays do
    // not cluster, so this asserts the draws are spread across the window
    // rather than merely varied.
    Backoff backoff(BackoffPolicy{ 1000ms, 2.0, 30min, 5 }, 4242);

    int lowerHalf = 0;
    int upperHalf = 0;
    for (int draw = 0; draw < 400; ++draw) {
        const auto delay = backoff.delayForRetry(3);   // ceiling 8000 ms
        if (delay < 4000ms)
            ++lowerHalf;
        else
            ++upperHalf;
    }

    // A uniform draw puts roughly 200 in each half. The bound is loose because
    // this is a statistical property of a seeded generator, not an exact one —
    // but a schedule with no jitter at all, or one that only wobbles near the
    // ceiling, lands 0 in one of the two and fails.
    QVERIFY2(lowerHalf > 120 && upperHalf > 120,
             qPrintable(QStringLiteral("delays clustered: %1 below the midpoint, %2 above")
                            .arg(lowerHalf)
                            .arg(upperHalf)));
}

void TestBackoff::sameSeedSameSchedule()
{
    // The reason the jitter source is injected rather than global: two clients
    // with the same seed produce the same sequence, which is what lets every
    // assertion above be exact instead of approximate.
    Backoff first(BackoffPolicy{}, 7);
    Backoff second(BackoffPolicy{}, 7);

    for (int retry = 0; retry < 6; ++retry)
        QCOMPARE(first.delayForRetry(retry), second.delayForRetry(retry));

    Backoff different(BackoffPolicy{}, 8);
    QSet<qint64> seen;
    for (int retry = 0; retry < 6; ++retry) {
        seen.insert(first.delayForRetry(retry).count());
        seen.insert(different.delayForRetry(retry).count());
    }
    QVERIFY(seen.size() > 1);
}

void TestBackoff::retryAfterIsClamped()
{
    Backoff backoff(BackoffPolicy{ 1000ms, 2.0, 30min, 5 }, 1);

    QCOMPARE(backoff.clampToCap(5s), 5s);
    QCOMPARE(backoff.clampToCap(7 * 24h), 30min);

    // A negative Retry-After — a server sending a date in the past, which
    // happens — becomes "now" rather than "before now".
    QCOMPARE(backoff.clampToCap(-10s), 0ms);
}

void TestBackoff::retryCountIsBounded()
{
    Backoff backoff(BackoffPolicy{ 1000ms, 2.0, 30min, 3 }, 1);

    QVERIFY(backoff.shouldRetry(0));
    QVERIFY(backoff.shouldRetry(2));
    QVERIFY(!backoff.shouldRetry(3));
    QVERIFY(!backoff.shouldRetry(99));
}

QTEST_MAIN(TestBackoff)
#include "tst_backoff.moc"
