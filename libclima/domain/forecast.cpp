// SPDX-FileCopyrightText: 2026 Jowi Aoun
// SPDX-License-Identifier: MPL-2.0

#include "libclima/domain/forecast.h"

#include <cmath>
#include <optional>
#include <utility>

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

bool isWaxing(Reading moonPhase)
{
    if (!moonPhase)
        return true;

    double phase = std::fmod(*moonPhase, 1.0);
    if (phase < 0.0)
        phase += 1.0;
    return phase < 0.5;
}

std::optional<QDate> nextFullMoon(const QList<DailyPoint> &days, const QDate &from)
{
    // Wrapped the way moonPhaseName wraps it, and for the same reason: a
    // provider is entitled to answer 1.0, or a hair over it, for a new moon.
    const auto cyclePosition = [](Reading phase) {
        double p = std::fmod(*phase, 1.0);
        if (p < 0.0)
            p += 1.0;
        return p;
    };

    // Rows at or after `from` that actually carry a phase, in date order.
    QList<std::pair<QDate, double>> readings;
    for (const DailyPoint &day : days) {
        if (!day.date.isValid() || !day.moonPhase)
            continue;
        if (from.isValid() && day.date < from)
            continue;
        readings.append({ day.date, cyclePosition(day.moonPhase) });
    }

    if (readings.isEmpty())
        return std::nullopt;

    // The first day the series says the moon is full: the pair that brackets
    // 0.5, and of that pair the reading nearer to full.
    //
    // `phaseA <= 0.5 <= phaseB` is the whole test, and it is written on the raw
    // positions rather than on their difference because the difference is what
    // gets a pair straddling the NEW moon wrong: 0.97 then 0.02 is a negative
    // step across a gap in which the moon is dark, and a bracket test on the
    // step reads it as containing 0.5. Here `phaseA > 0.5` rejects it outright.
    //
    // `readings` is in the order the daily series arrived in, which every
    // provider we have gives ascending by date. A shuffled series would find no
    // bracket and fall through to the extrapolation below.
    for (int i = 0; i + 1 < readings.size(); ++i) {
        const auto &[dateA, phaseA] = readings.at(i);
        const auto &[dateB, phaseB] = readings.at(i + 1);
        if (phaseA > 0.5 || phaseB < 0.5)
            continue;
        return (0.5 - phaseA) <= (phaseB - 0.5) ? dateA : dateB;
    }

    // Past the horizon. How far round the cycle the first reading still has to
    // go, on the mean month — zero when it is already full, a whole month when
    // it has just been.
    const auto &[date, phase] = readings.constFirst();
    const double toFull = std::fmod(0.5 - phase + 1.0, 1.0) * kSynodicMonth;
    return date.addDays(qRound(toFull));
}

} // namespace clima
