// SPDX-FileCopyrightText: 2026 Jowi Aoun
// SPDX-License-Identifier: MPL-2.0
//
// What time it is — asked, never assumed.
//
// ============================================================================
// THE RULE
//
//   Nothing in libclima/ or app/ may call QDateTime::currentDateTime(),
//   QDateTime::currentDateTimeUtc(), QDateTime::currentSecsSinceEpoch(),
//   QDateTime::currentMSecsSinceEpoch(), QDate::currentDate(),
//   QTime::currentTime(), time(), or JavaScript's Date.now(). The two
//   implementations at the bottom of this file are the only exceptions, and
//   they are exceptions because they are the mechanism.
//
//   tests/tst_sourcerules.cpp enforces this by scanning the tree, so the rule
//   is a failing test rather than a paragraph nobody reads.
// ============================================================================
//
// ---- why a clock is an interface and not a function -------------------------
//
// Because the alternative is `if (testing)`, and it does not stay in one place.
//
// A cache decides freshness by comparing now against an expiry. A backoff
// decides when to retry by comparing now against a deadline. An alert decides
// whether it has passed its CAP <expires>. A forecast strip decides which hour
// is "now" and paints it differently. Every one of those is a wall-clock read,
// and every one of them is a place a test would otherwise have to either sleep
// through or bypass with a flag. Five flags in, "fixture mode" is a cross-
// cutting concern that has grown into the cache, the network layer and the
// view models, and no single file describes what it does.
//
// One injected object collapses all of that. `--fixture` constructs a
// FrozenClock at the timestamp the recorded payloads were captured, hands it to
// everything that is constructed afterwards, and the entire product — TTLs,
// backoff, alert expiry, the "now" marker on the hourly strip — behaves exactly
// as it did on the afternoon the fixtures were recorded. Downstream code has no
// idea it is in fixture mode and never asks.
//
// That is also what makes the golden images comparable. docs/04-architecture.md
// §4.11 asks for golden-file provider tests and golden-image chart tests; both
// are photographs of a moment, and a photograph of a moment taken by something
// reading the system clock is a different photograph every time.
//
// ---- two clocks, and why the second one is not just a QDateTime -------------
//
// Wall-clock time is not monotonic. It steps at a DST boundary, it jumps when
// NTP corrects a drifting laptop, and it can go backwards. A backoff that
// measures "wait thirty seconds" against wall-clock time therefore has a
// failure mode where a clock correction turns thirty seconds into a negative
// number and every retry fires at once — a thundering herd at exactly the
// moment a server has told us it is unhappy.
//
// So the interface has two hands. `now()` answers "what time is it", which is
// what a TTL and a CAP expiry are about, and it is allowed to jump. `elapsed()`
// answers "how long since", counts from an unspecified origin, and never goes
// backwards. Code that measures a duration uses the second one. Code that
// compares against a timestamp somebody else wrote down uses the first.
//
// ---- ownership --------------------------------------------------------------
//
// A Clock is a non-owning dependency. HttpClient and CacheStore take a raw
// `Clock *` and require that it outlive them, which in practice means one clock
// constructed in main() (or in a test's initTestCase) and handed to everything.
// It is not a singleton, because a test that runs two frozen clocks at two
// different instants in one process is a test worth being able to write.

#pragma once

#include <QDateTime>
#include <QElapsedTimer>
#include <QTimeZone>

#include <chrono>

namespace clima {

class Clock
{
public:
    virtual ~Clock();

    // The wall clock, always in UTC.
    //
    // UTC and not local time, deliberately: every timestamp libclima stores or
    // compares is a UTC instant, and the only place a local zone belongs is the
    // moment a string is formatted for a human. A QDateTime whose time spec
    // varies with the machine's zone is a comparison that silently changes
    // meaning when a user flies somewhere.
    [[nodiscard]] virtual QDateTime now() const = 0;

    // Monotonic elapsed time from an unspecified origin. Only differences
    // between two readings mean anything; the absolute value does not.
    [[nodiscard]] virtual std::chrono::milliseconds elapsed() const = 0;
};

// ---- production -------------------------------------------------------------

class SystemClock final : public Clock
{
public:
    SystemClock();
    ~SystemClock() override;

    [[nodiscard]] QDateTime now() const override;
    [[nodiscard]] std::chrono::milliseconds elapsed() const override;

private:
    QElapsedTimer m_since;
};

// ---- tests, fixtures, and --fixture -----------------------------------------
//
// Time does not pass unless something asks for it to. `advance()` moves both
// hands together, which is the property that makes a TTL test read like the
// thing it is testing:
//
//     clock.advance(9min);   QVERIFY(store.isFresh(entry));
//     clock.advance(2min);   QVERIFY(!store.isFresh(entry));
//
// No sleeping, no tolerance, no flake on a loaded runner.
class FrozenClock final : public Clock
{
public:
    // Defaults to 2026-01-01T00:00:00Z — an arbitrary instant, chosen only so
    // that a default-constructed FrozenClock is still a valid one and a test
    // that forgot to say when it is does not silently start at the epoch, where
    // half the arithmetic in this codebase would go negative.
    explicit FrozenClock(QDateTime at = QDateTime(QDate(2026, 1, 1), QTime(0, 0), QTimeZone::UTC));
    ~FrozenClock() override;

    [[nodiscard]] QDateTime now() const override;
    [[nodiscard]] std::chrono::milliseconds elapsed() const override;

    // Moves both hands forward by the same amount.
    void advance(std::chrono::milliseconds by);

    // Moves the wall clock without moving the monotonic one. This is a clock
    // correction, an NTP step, a DST boundary — the case the two-handed
    // interface exists for, and the only way to write a test for code that
    // must survive it.
    void setNow(QDateTime at);

private:
    QDateTime                 m_now;
    std::chrono::milliseconds m_elapsed{0};
};

} // namespace clima
