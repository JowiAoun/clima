// SPDX-FileCopyrightText: 2026 Jowi Aoun
// SPDX-License-Identifier: MPL-2.0
//
// How long to wait before asking again, when the answer was "not now".
//
// ---- full jitter, and why the naive schedule is worse than none -------------
//
// The obvious schedule is 1 s, 2 s, 4 s, 8 s. It is also the schedule that
// turns a five-minute outage into a synchronised stampede: every client that
// hit the same 503 waits the same number of seconds and they all come back at
// the same instant, which is how a recovering server gets knocked over by the
// clients that were waiting for it to recover.
//
// The fix is jitter, and the variant here is *full* jitter — a uniform draw
// from [0, ceiling] rather than ceiling ± a few percent. It spreads a fleet
// across the whole window instead of around a spike, and it is the variant
// AWS's architecture blog measured as strictly better than "equal jitter" on
// both total work and completion time. The cost is that an individual retry
// can come back almost immediately; the ceiling doubling underneath is what
// keeps the *expected* wait growing anyway.
//
// ---- thirty minutes ---------------------------------------------------------
//
// The cap. docs/04-architecture.md §4.5 asks for "exponential backoff with
// jitter on 5xx/429" without naming a number, and thirty minutes is chosen
// against the TTLs in the same table: the longest thing we refresh on a timer
// is an hour, so a backoff that grew past that would mean a provider outage
// leaves an entry stale for longer than the outage lasted. Half the longest
// TTL keeps the recovery inside one refresh cycle.
//
// ---- Retry-After beats all of it --------------------------------------------
//
// When a server sends Retry-After it has told us what it wants, and guessing
// over the top of that is both rude and worse-informed. HttpClient honours the
// header when present, clamped to the same cap so a hostile or broken value
// cannot park a request for a week.
//
// ---- jitter versus determinism ----------------------------------------------
//
// The repo rule is that nothing reads a nondeterministic source. Jitter is the
// one place where randomness is the feature, so it is injected rather than
// reached for: `Backoff` owns a QRandomGenerator seeded from a value the caller
// supplies. Production seeds it once from the system generator; a test passes a
// literal and gets the same schedule on every run, which is what makes the
// bounds below assertable rather than approximately assertable.

#pragma once

#include <QRandomGenerator>

#include <chrono>

namespace clima {

struct BackoffPolicy {
    // The first ceiling. Doubles from here.
    std::chrono::milliseconds base{ 1000 };

    double factor = 2.0;

    // Thirty minutes. See the header comment for where the number comes from.
    std::chrono::milliseconds cap{ std::chrono::minutes(30) };

    // How many times a single request may be retried before the failure is
    // handed to the caller. Five attempts against a 1 s base reaches a 16 s
    // ceiling, which is about as long as a user will wait for a screen to
    // reconcile before deciding the app is broken.
    int maxRetries = 5;
};

class Backoff
{
public:
    // `seed` is the whole of the randomness. Two Backoffs with the same seed
    // and the same policy produce the same sequence, which is the property
    // tests are written against.
    Backoff(BackoffPolicy policy, quint32 seed);

    [[nodiscard]] const BackoffPolicy &policy() const { return m_policy; }

    // The exponential ceiling for a retry, before jitter. `retry` is 0 for the
    // first retry after the first failure. Deterministic, and public because
    // asserting "the delay landed inside its window" needs the window.
    [[nodiscard]] std::chrono::milliseconds ceilingForRetry(int retry) const;

    // A uniform draw from [0, ceilingForRetry(retry)]. Not const: it advances
    // the generator, and hiding that behind mutable would make two identical
    // calls return different answers with no signal in the signature.
    std::chrono::milliseconds delayForRetry(int retry);

    [[nodiscard]] bool shouldRetry(int retriesSoFar) const
    {
        return retriesSoFar < m_policy.maxRetries;
    }

    // Applied to a server's Retry-After so that a header of 0 or of one week
    // both land somewhere sane.
    [[nodiscard]] std::chrono::milliseconds clampToCap(std::chrono::milliseconds delay) const;

private:
    BackoffPolicy    m_policy;
    QRandomGenerator m_random;
};

} // namespace clima
