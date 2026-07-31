// SPDX-FileCopyrightText: 2026 Jowi Aoun
// SPDX-License-Identifier: MPL-2.0

#include "backoff.h"

#include <algorithm>
#include <cmath>

namespace clima {

Backoff::Backoff(BackoffPolicy policy, quint32 seed)
    : m_policy(policy)
    , m_random(seed)
{
}

std::chrono::milliseconds Backoff::ceilingForRetry(int retry) const
{
    if (retry < 0)
        retry = 0;

    // In double, then clamped, rather than shifting an integer. `base << retry`
    // is the tidier spelling and it overflows at retry 54 on a 64-bit count —
    // which is unreachable today because maxRetries is 5, and which would be a
    // silently negative delay the day somebody raises it. std::pow saturates to
    // infinity instead, and the min() below turns infinity into the cap.
    const double grown = static_cast<double>(m_policy.base.count())
        * std::pow(m_policy.factor, static_cast<double>(retry));

    const double capped = std::min(grown, static_cast<double>(m_policy.cap.count()));
    return std::chrono::milliseconds(static_cast<qint64>(capped));
}

std::chrono::milliseconds Backoff::delayForRetry(int retry)
{
    const qint64 ceiling = ceilingForRetry(retry).count();
    if (ceiling <= 0)
        return std::chrono::milliseconds(0);

    // bounded(qint64) is exclusive of its argument, so +1 makes the draw
    // inclusive of the ceiling. That matters only at the boundary and only for
    // the test that asserts the window, but a half-open window documented as
    // closed is the sort of thing that costs an hour later.
    return std::chrono::milliseconds(m_random.bounded(ceiling + 1));
}

std::chrono::milliseconds Backoff::clampToCap(std::chrono::milliseconds delay) const
{
    if (delay.count() < 0)
        return std::chrono::milliseconds(0);
    return std::min(delay, m_policy.cap);
}

} // namespace clima
