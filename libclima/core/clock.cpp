// SPDX-FileCopyrightText: 2026 Jowi Aoun
// SPDX-License-Identifier: MPL-2.0

#include "clock.h"

namespace clima {

Clock::~Clock() = default;

// ---- SystemClock ------------------------------------------------------------

SystemClock::SystemClock()
{
    m_since.start();
}

SystemClock::~SystemClock() = default;

QDateTime SystemClock::now() const
{
    // The one permitted call in the tree. tests/tst_sourcerules.cpp knows about
    // this file by name; every other occurrence anywhere is a test failure.
    return QDateTime::currentDateTimeUtc();
}

std::chrono::milliseconds SystemClock::elapsed() const
{
    return std::chrono::milliseconds(m_since.elapsed());
}

// ---- FrozenClock ------------------------------------------------------------

FrozenClock::FrozenClock(QDateTime at)
    : m_now(at.toUTC())
{
}

FrozenClock::~FrozenClock() = default;

QDateTime FrozenClock::now() const
{
    return m_now;
}

std::chrono::milliseconds FrozenClock::elapsed() const
{
    return m_elapsed;
}

void FrozenClock::advance(std::chrono::milliseconds by)
{
    m_now = m_now.addMSecs(by.count());
    m_elapsed += by;
}

void FrozenClock::setNow(QDateTime at)
{
    // Deliberately does not touch m_elapsed. See the header: this models a
    // clock correction, and the whole point of a monotonic hand is that a
    // correction does not move it.
    m_now = at.toUTC();
}

} // namespace clima
