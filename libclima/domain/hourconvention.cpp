// SPDX-FileCopyrightText: 2026 Jowi Aoun
// SPDX-License-Identifier: MPL-2.0

#include "hourconvention.h"

namespace clima {

QList<HourlyPoint> asHourStarting(const QList<HourlyPoint> &hourEnding)
{
    if (hourEnding.size() < 2)
        return {};

    QList<HourlyPoint> out;
    out.reserve(hourEnding.size() - 1);

    for (int i = 0; i + 1 < hourEnding.size(); ++i) {
        // Start from the sample at i, which already carries the correct
        // instantaneous readings and the correct timestamp, and replace only
        // the fields that describe an interval. Written as an explicit list
        // rather than as a copy of `next` with the instants patched back,
        // because the list of accumulated fields is the thing a reader has to
        // check against libclima/domain/forecast.h and it should be readable
        // in one place.
        HourlyPoint       point = hourEnding.at(i);
        const HourlyPoint next  = hourEnding.at(i + 1);

        point.precipitation            = next.precipitation;
        point.rain                     = next.rain;
        point.showers                  = next.showers;
        point.snowfall                 = next.snowfall;
        point.precipitationProbability = next.precipitationProbability;

        // The code moves with the precipitation it describes, and this is the
        // field where the two conventions do the most damage if they are
        // separated. precip.js's `cellFor(mm, tempC, code)` takes an amount and
        // a type together and builds one cell out of them; an amount from the
        // hour starting at t paired with a code from the hour ending at t
        // produces a cell that says "0.0 mm of heavy snow" at the edge of every
        // spell, and neither number is wrong on its own.
        //
        // forecast.h groups the code with the accumulations for the same
        // reason, in its own words: it "describes a stretch of weather rather
        // than a moment".
        point.weatherCode = next.weatherCode;

        out.append(point);
    }

    return out;
}

Forecast asHourStarting(const Forecast &forecast)
{
    Forecast out = forecast;
    out.hourly   = asHourStarting(forecast.hourly);
    return out;
}

} // namespace clima
