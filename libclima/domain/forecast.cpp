// SPDX-FileCopyrightText: 2026 Jowi Aoun
// SPDX-License-Identifier: MPL-2.0

#include "libclima/domain/forecast.h"

#include <cmath>

namespace clima {

bool CurrentConditions::isEmpty() const
{
    // Emptiness is "no timestamp", not "no temperature". A provider that
    // answered for a place where it happens to have no temperature reading
    // still answered, and a caller that treated that as no answer at all would
    // fall through to the next provider in the chain for nothing.
    return !time.isValid();
}

bool Forecast::isEmpty() const
{
    return hourly.isEmpty() && daily.isEmpty() && current.isEmpty();
}

const HourlyPoint *Forecast::hourAt(const QDateTime &at) const
{
    if (!at.isValid())
        return nullptr;

    // The hour ENDING at `time` — see the convention section in the header —
    // so the point at 15:00 covers [14:00, 15:00) and 15:00 itself belongs to
    // the next one. Half-open, so that no instant belongs to two points and
    // none belongs to none.
    for (const HourlyPoint &point : hourly) {
        if (!point.time.isValid())
            continue;
        const qint64 delta = point.time.toUTC().secsTo(at.toUTC());
        if (delta >= -3600 && delta < 0)
            return &point;
    }
    return nullptr;
}

Reading moonIllumination(Reading moonPhase)
{
    if (!moonPhase)
        return std::nullopt;

    // Half the cosine swing, offset to [0, 1]. Exact at the four quarters —
    // 0 at new, 0.5 at both quarters, 1 at full — which is the property that
    // makes it worth writing rather than approximating with a triangle wave.
    return (1.0 - std::cos(2.0 * M_PI * *moonPhase)) / 2.0;
}

QString moonPhaseName(Reading moonPhase)
{
    if (!moonPhase)
        return {};

    // Wrapped rather than clamped: a provider is entitled to report 1.0 for a
    // new moon and some report just over it, and clamping would call that a
    // waning crescent on the one night it is new.
    double phase = std::fmod(*moonPhase, 1.0);
    if (phase < 0.0)
        phase += 1.0;

    // The four exact phases get a window of ±0.02 of the cycle — about
    // fourteen hours either side, so one calendar day is called "full" and
    // occasionally two. The naive alternative gives each of the eight names an
    // eighth of the month, which calls the moon full for three and a half days
    // and makes the label mean nothing on the night somebody looks up.
    //
    // Not tightened further: a provider steps this about 0.033 per day, so a
    // window narrower than that would produce months with no full moon at all,
    // which is a worse failure than a full moon that lasts two nights.
    constexpr double exact = 0.02;

    if (phase < exact || phase > 1.0 - exact)
        return QStringLiteral("new");
    if (std::abs(phase - 0.25) < exact)
        return QStringLiteral("first-quarter");
    if (std::abs(phase - 0.50) < exact)
        return QStringLiteral("full");
    if (std::abs(phase - 0.75) < exact)
        return QStringLiteral("last-quarter");

    if (phase < 0.25)
        return QStringLiteral("waxing-crescent");
    if (phase < 0.50)
        return QStringLiteral("waxing-gibbous");
    if (phase < 0.75)
        return QStringLiteral("waning-gibbous");
    return QStringLiteral("waning-crescent");
}

} // namespace clima
