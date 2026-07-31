// SPDX-FileCopyrightText: 2026 Jowi Aoun
// SPDX-License-Identifier: MPL-2.0

#include "timeaxis.h"

#include <limits>

namespace clima {

QTimeZone zoneFor(const QString &ianaId, int fallbackOffsetSeconds)
{
    if (!ianaId.isEmpty()) {
        const QTimeZone named(ianaId.toUtf8());
        if (named.isValid())
            return named;
    }
    return QTimeZone::fromSecondsAheadOfUtc(fallbackOffsetSeconds);
}

QDateTime utcFromNaiveLocal(const QString &naiveLocal, int offsetSeconds)
{
    if (naiveLocal.isEmpty())
        return {};

    // Parsed as if it were UTC and then shifted, rather than parsed into a
    // zone. That is not a shortcut, it is the only correct reading: the string
    // was produced by adding `offsetSeconds` to a UTC instant, so subtracting
    // the same number inverts it exactly. Handing the string to a real zone
    // would ask that zone to resolve a wall-clock time that the zone never
    // used — and on a fall-back day it would have to pick between two answers.
    QDateTime local = QDateTime::fromString(naiveLocal, Qt::ISODate);
    if (!local.isValid()) {
        // A bare date. Open-Meteo writes daily.time that way, and it means
        // local midnight.
        const QDate date = QDate::fromString(naiveLocal, Qt::ISODate);
        if (!date.isValid())
            return {};
        local = QDateTime(date, QTime(0, 0));
    }

    local.setTimeZone(QTimeZone::UTC);
    return local.addSecs(-qint64(offsetSeconds));
}

QDate localDateOf(const QDateTime &utc, const QTimeZone &zone)
{
    if (!utc.isValid())
        return {};
    return utc.toTimeZone(zone).date();
}

QList<int> indicesOnLocalDate(const QList<QDateTime> &utc, const QTimeZone &zone, QDate date)
{
    QList<int> found;
    for (int i = 0; i < utc.size(); ++i) {
        if (localDateOf(utc.at(i), zone) == date)
            found.append(i);
    }
    return found;
}

int minutesFromLocalMidnight(const QDateTime &utc, const QTimeZone &zone, QDate reference)
{
    if (!utc.isValid() || !reference.isValid())
        return std::numeric_limits<int>::min();

    // The CLOCK reading, plus a whole day for every calendar day between the
    // event and the reference. Not the elapsed time since the reference's
    // midnight, and the difference is the entire subtlety of this function.
    //
    // Elapsed seconds would be the obvious implementation and it is wrong on
    // exactly the days this file exists for. On 2025-11-02 the clocks in
    // Toronto go back at 02:00, so seven hours and fifty-five minutes elapse
    // between local midnight and a sunrise that a clock on the wall reads as
    // 06:55. An arc placing the sun at 475 minutes puts it an hour after where
    // every label on the page says it is.
    //
    // The clock reading is 415, which is what "6:55 AM" means, and it is what
    // app/qml/Clima/detaildata.js's riseMin/setMin/nowMin are all in. The
    // missing or repeated hour is genuinely missing from or repeated on the
    // clock, and an arc drawn over 0..1440 is drawing clock positions.
    //
    // The whole-days term is what keeps the polar case right: a sunset that
    // Open-Meteo reports as the *next* day's midnight is one day plus zero
    // minutes, which is 1440, which is a full arc.
    const QDateTime local     = utc.toTimeZone(zone);
    const qint64    dayOffset = qint64(reference.daysTo(local.date())) * 1440;

    const QTime time = local.time();

    // Rounded to the nearest minute rather than truncated, so a sunset at
    // 20:42:59 is 20:43 and matches the string the same provider would have
    // printed. Truncation loses a minute on roughly half of all events, and
    // the one place anyone checks this is a Sun card against a clock.
    const int minuteOfDay = time.hour() * 60 + time.minute() + (time.second() >= 30 ? 1 : 0);

    return int(dayOffset + minuteOfDay);
}

bool hasMinuteOfDay(int minutes)
{
    return minutes != std::numeric_limits<int>::min();
}

} // namespace clima
