// SPDX-FileCopyrightText: 2026 Jowi Aoun
// SPDX-License-Identifier: MPL-2.0
//
// Which hour a number belongs to — the one-line conversion that decides
// whether every rain band in the app is drawn an hour late.
//
// ============================================================================
// TWO CONVENTIONS, BOTH REASONABLE, ONE PIXEL APART
//
// An hourly sample is stamped with an instant, but half the quantities in it
// describe an *interval*, and there are two defensible ways to say which:
//
//     hour ENDING at t     the value covers [t - 1h, t)
//     hour STARTING at t   the value covers [t, t + 1h)
//
// libclima/domain/forecast.h picks the first, because Open-Meteo picks the
// first and Open-Meteo is the primary; its documentation for `precipitation`
// reads "Preceding hour sum", and libclima/providers/metno/ shifts MET Norway's
// `next_1_hours` blocks into it.
//
// app/qml/Clima/precip.js picks the second, and not casually — its comment
// above `bandX`/`bandW` argues for it:
//
//     "An hour's sample is an instant, but the rain it reports is an interval
//      … So hour i occupies [i, i+1) on the time axis. Centring the wash on
//      the sample instead would claim rain for the half hour before it starts,
//      which is precisely the half hour someone is deciding whether to leave
//      in."
//
// It is right, and its geometry is built on it: `bandX` returns
// `span.from * hourWidth` and `bandW` runs to `(span.to + 1) * hourWidth`.
//
// So somewhere between the JSON and the chart, exactly one shift has to
// happen. This function is that shift, and it is a named function in domain/
// rather than three lines inside a view model because of how it fails when it
// is missing: nothing throws, nothing is empty, no test that is not looking
// for it goes red. The wash simply sits one column to the right of the rain,
// under a chart whose temperature curve is correct, and it looks entirely
// plausible. The forecast says it starts raining at three when it starts
// raining at two, and the only way anyone finds out is by getting wet.
//
// ============================================================================
// WHICH WAY ROUND, MEASURED RATHER THAN READ
//
// Open-Meteo's documentation says "preceding". It is also demonstrable, and
// the demonstration is worth recording because the cost of having it backwards
// is a shift in the wrong direction, which is two hours of error rather than
// one.
//
// Asking the same model for hourly and 15-minute precipitation at Miami over
// 49 hours and summing the quarter-hours two ways:
//
//     hourly[t] vs sum of minutely_15 over (t - 1h, t]   total error  6.25 mm
//     hourly[t] vs sum of minutely_15 over [t, t + 1h)   total error  177.45 mm
//
// with exact agreement on the heavy hours (13.90, 32.50, 68.80 mm). The same
// experiment on `temperature_2m` — instantaneous, and therefore the control —
// agrees to 0.000 °C at lag zero. Verified live 2026-07-31, `models=gfs_hrrr`
// so that both series come from one model.
//
// Therefore the value for the hour starting at t is the sample stamped t + 1h,
// which is what `asHourStarting` does: index i takes its accumulations from
// index i + 1.
//
// ---- the series loses its last hour, and that is correct ---------------------
//
// The last sample has no successor, so there is no measured accumulation for
// the hour it starts. It is dropped rather than filled with an absent Reading,
// because a final column with a real temperature and no rain is worse than no
// final column: it draws, it looks like data, and it is the one column where
// "no rain" means "not asked". Sixteen days of forecast become sixteen days
// minus one hour, which nobody has ever needed.

#pragma once

#include "libclima/domain/forecast.h"

#include <QList>

namespace clima {

// The same hours, re-read so that every accumulated quantity describes the
// hour STARTING at its timestamp — the convention app/qml/Clima/precip.js
// draws on. Returns one fewer point than it is given; empty in, empty out.
//
// Instantaneous quantities are untouched: they are readings at the timestamp,
// and the timestamp does not move.
//
// Call this once, at the boundary where domain data becomes chart data. Calling
// it twice shifts twice.
[[nodiscard]] QList<HourlyPoint> asHourStarting(const QList<HourlyPoint> &hourEnding);

// The whole forecast, with its hourly series converted. The daily series is
// untouched — a daily total is already keyed to a calendar date rather than to
// an interval boundary — and so is `current`, whose precipitation is the
// preceding hour by definition and is displayed as "in the last hour".
[[nodiscard]] Forecast asHourStarting(const Forecast &forecast);

} // namespace clima
