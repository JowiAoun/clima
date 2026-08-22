// SPDX-FileCopyrightText: 2026 Jowi Aoun
// SPDX-License-Identifier: GPL-3.0-or-later

#include "forecastdata.h"

#include "appengine.h"
#include "timeformat.h"
#include "units.h"

#include "libclima/domain/hourconvention.h"
#include "libclima/domain/weathercode.h"

#include <QHash>
#include <QLocale>
#include <QQmlEngine>

using namespace clima;

namespace {

// The window mockdata.js drew, in the two numbers that describe it.
constexpr int kWindowHours = 48;
constexpr int kPastHours   = 15;

// And the window for any day that is not today: the day itself, midnight to
// midnight. Not 48 with the day in the middle — a chart of Friday that opens on
// Thursday evening is a chart of Friday you have to scroll to find.
constexpr int kDayHours = 24;

// Below this an hour is dry. precip.js's TRACE, and the only number from that
// file repeated here — repeated because the question "is this hour wet" is
// asked before precip.js sees the data, when the type is being read off the
// weather code.
constexpr double kTrace = 0.1;

// An absent Reading becomes NaN rather than 0. Zero is a temperature, a wind
// speed and a rainfall; NaN is not, and every formatter in this app already
// draws it as an em dash. libclima/domain/reading.h makes the same argument at
// greater length.
double value(const Reading &reading)
{
    return reading.has_value() ? *reading : qQNaN();
}

double display(const Reading &reading, Units::Quantity quantity)
{
    if (!reading.has_value())
        return qQNaN();
    return Units::instance()->convert(quantity, *reading);
}

// Rounded to whole units for the series a UI prints as an integer — humidity,
// cloud cover, probability. Absent stays absent.
double rounded(const Reading &reading)
{
    return reading.has_value() ? std::round(*reading) : qQNaN();
}

QString shortWeekday(QDate date)
{
    return QLocale().dayName(date.dayOfWeek(), QLocale::ShortFormat);
}


// "3 PM", or "15:00" for a reader who asked for a 24-hour clock. The reference's
// spelling was the only one this returned until the preferences screen landed,
// and the arithmetic that produced it now lives in one file for the whole app —
// see app/viewmodels/timeformat.h, whose header says why five copies of it was
// not a tidiness problem.
//
// Not called `hourLabel`: ForecastData has a member of that name, and a member
// hides a namespace-scope function of the same name at every call site inside
// the class.
QString hourOf(const QDateTime &local)
{
    return TimeFormat::instance()->hour(local.time());
}

// ---- the two maps, and why only they need this ------------------------------
//
// Almost everything this class publishes is a QVariantList or an int, and both
// are well formed empty: an empty array answers `.length` with 0 and a Repeater
// bound to it draws nothing. A QVariantMap is not. `Data.moonPhase.illuminated`
// on an empty map is `undefined`, which is a failed assignment to a real
// property and one line of console on every start that has to fetch.
//
// app/viewmodels/conditionsdata.cpp carries the full argument — this is the same
// rule applied to the two maps on this side of the boundary.
QVariantMap neutralMonth()
{
    return QVariantMap{
        { QStringLiteral("name"), QString() },
        { QStringLiteral("year"), 0 },
        { QStringLiteral("number"), 0 },
        { QStringLiteral("length"), 0 },
        { QStringLiteral("firstWeekday"), 0 },
        { QStringLiteral("today"), 0 },
    };
}

QVariantMap neutralMoonPhase()
{
    return QVariantMap{
        { QStringLiteral("name"), QString() },
        { QStringLiteral("illuminated"), 0.0 },
    };
}

} // namespace

ForecastData::ForecastData(QObject *parent)
    : QObject(parent)
{
    // Born with the shape. `Data.moonPhase` is read while HourlyOverview is
    // being constructed, which precedes the first snapshot on any start that
    // has to fetch one.
    clear();

    // Everything on screen that carries a unit is rebuilt when a preference
    // changes. Cheaper would be to convert lazily in the getters; that would
    // also mean nothing notifies, and a settings screen whose effect appears
    // after the next refresh is a settings screen that looks broken.
    const auto rebuild = [this]() {
        if (!m_forecast.isEmpty())
            setSnapshot(m_forecast, m_air, m_now, m_place);
    };
    connect(Units::instance(), &Units::changed, this, rebuild);

    // The same treatment for the clock, and for the same reason: every hour
    // label on the chart and in the list is a formatted string held in a
    // snapshot, so a format changed while the app is open has to rebuild it or
    // the preference appears to do nothing until the next fetch.
    connect(TimeFormat::instance(), &TimeFormat::changed, this, rebuild);
}

ForecastData *ForecastData::create(QQmlEngine *, QJSEngine *)
{
    // Not a second instance: the one AppEngine owns and pushes into. A
    // default-constructed singleton here would register, resolve, evaluate
    // every binding and report an empty forecast for ever, with nothing
    // warning — see the note on private constructors in app/appoptions.h.
    ForecastData *data = AppEngine::instance()->forecastData();
    QQmlEngine::setObjectOwnership(data, QQmlEngine::CppOwnership);
    return data;
}

// The traditional phase name, translated. libclima returns an identifier for
// exactly this reason — the table is data, and the CLI and the applet in D6
// need the same sentence in the same language as the app.
QString ForecastData::moonPhaseLabel(const QString &identifier)
{
    if (identifier == QLatin1String("new"))             return tr("New Moon");
    if (identifier == QLatin1String("waxing-crescent")) return tr("Waxing Crescent");
    if (identifier == QLatin1String("first-quarter"))   return tr("First Quarter");
    if (identifier == QLatin1String("waxing-gibbous"))  return tr("Waxing Gibbous");
    if (identifier == QLatin1String("full"))            return tr("Full Moon");
    if (identifier == QLatin1String("waning-gibbous"))  return tr("Waning Gibbous");
    if (identifier == QLatin1String("last-quarter"))    return tr("Last Quarter");
    if (identifier == QLatin1String("waning-crescent")) return tr("Waning Crescent");
    return {};
}

QVariantList ForecastData::weekdayNames() const
{
    // Sunday first, because `weekdayOf()` returns 0 for Sunday and the calendar
    // grid indexes this list with it. Localised: the names move, the order is
    // the grid's.
    QVariantList names;
    const QLocale locale;
    for (int day = 0; day < 7; ++day)
        names.append(locale.dayName(day == 0 ? 7 : day, QLocale::ShortFormat));
    return names;
}

// Everything a day change invalidates, and nothing else. Every builder below
// appends, so this is what makes calling one twice mean "rebuild" rather than
// "append a second copy" — which is what a day change does four times over.
void ForecastData::clearWindow()
{
    m_count = 0;
    m_nowIndex = 0;
    m_start = 0;
    m_temperature.clear(); m_apparent.clear(); m_precipProb.clear(); m_precipMm.clear();
    m_cloud.clear(); m_humidity.clear(); m_windSpeed.clear(); m_windGust.clear();
    m_windDirection.clear(); m_pressure.clear(); m_uvIndex.clear(); m_visibility.clear();
    m_airQuality.clear(); m_precipTypes.clear();
    m_hasApparent = false;
    m_sunEvents.clear();
    m_labelIndices.clear(); m_precipBuckets.clear();
}

void ForecastData::clear()
{
    m_hours.clear();
    m_nowAbsolute = 0;
    clearWindow();
    m_days.clear(); m_dayDates.clear(); m_todayIndex = 0; m_selectedDay = 0;
    m_monthDays.clear();

    // Reset to a shape rather than emptied, for the reason the two functions
    // give. buildMonth() and buildSunEvents() overwrite these keys in place, so
    // the neutral values survive only until there is a forecast to replace them.
    m_month     = neutralMonth();
    m_moonPhase = neutralMoonPhase();
}

void ForecastData::setSnapshot(const Forecast &forecast, const AirQuality &airQuality,
                               const QDateTime &now, const Place &place)
{
    // Captured before clear() zeroes it. A refresh that leaves the selection
    // where it was must not announce a change: the chart re-opens on
    // `selectedDayChanged` and a signal every ten minutes would haul a reader
    // who had scrolled back to this morning forward again, on a timer, for no
    // reason they could see.
    const int previousDay = m_selectedDay;

    m_forecast = forecast;
    m_air      = airQuality;
    m_place    = place;
    m_now      = now;

    // The forecast's own zone first, the place's second, UTC last. Not the
    // machine's, ever: a saved location in Tokyo renders its own evening while
    // the app runs in Toronto, and the moment this falls back to the system
    // zone that stops being true without anything looking wrong.
    if (forecast.timeZone.isValid())
        m_zone = forecast.timeZone;
    else if (!place.timezone.isEmpty())
        m_zone = QTimeZone(place.timezone.toUtf8());
    else
        m_zone = QTimeZone::UTC;

    clear();

    if (forecast.isEmpty()) {
        Q_EMIT changed();
        return;
    }

    // THE SHIFT. Once, here, at the boundary where domain data becomes chart
    // data — libclima/domain/hourconvention.h says to call it exactly once and
    // explains what calling it twice does. Everything below indexes `m_hours`,
    // in which an accumulated quantity describes the hour STARTING at its
    // timestamp.
    m_hours = asHourStarting(forecast.hourly);

    // Days first now, and that ordering is load-bearing: the window is of a day
    // and has to be able to look its date up. A fresh snapshot always opens on
    // today — the rows have moved, so a remembered index would point at a
    // different date, and the one thing worse than losing a selection is
    // keeping the number and silently changing what it means.
    buildDays(now);
    m_selectedDay = m_todayIndex;

    buildWindow(now);
    buildSeries();
    buildMonth(now);
    buildSunEvents();
    buildBuckets();

    if (m_selectedDay != previousDay)
        Q_EMIT selectedDayChanged();
    Q_EMIT changed();
}

void ForecastData::setSelectedDay(int index)
{
    const int clamped = m_days.isEmpty() ? 0 : qBound(0, index, int(m_days.size()) - 1);
    if (clamped == m_selectedDay)
        return;

    m_selectedDay = clamped;
    if (!m_hours.isEmpty())
        retarget();

    Q_EMIT selectedDayChanged();
    Q_EMIT changed();
}

void ForecastData::retarget()
{
    clearWindow();
    buildWindow(m_now);
    buildSeries();
    buildSunEvents();
    buildBuckets();
}

// ---- the window ------------------------------------------------------------------

void ForecastData::buildWindow(const QDateTime &now)
{
    if (m_hours.isEmpty())
        return;

    // The hour containing `now`. A linear scan over a few hundred entries, for
    // the reason Forecast::hourAt gives: a binary search would need the sort
    // order to be a documented invariant rather than a thing that happens to be
    // true.
    int current = 0;
    for (int i = 0; i < m_hours.size(); ++i) {
        if (m_hours.at(i).time <= now)
            current = i;
        else
            break;
    }
    m_nowAbsolute = current;

    // Today keeps the window described at the top of the header, to the hour.
    // Any other day is that day, and `firstOfDay` returning -1 falls back to
    // today's rather than to an empty chart: the strip draws eleven cards off
    // the *daily* series and a provider whose hourly horizon is shorter than
    // its daily one — MET Norway's is, by days — would otherwise have cards on
    // it that select nothing.
    const QDate today = now.toTimeZone(m_zone).date();
    const QDate wanted = m_dayDates.value(m_selectedDay);

    int firstOfDay = -1;
    if (wanted.isValid() && wanted != today) {
        for (int i = 0; i < m_hours.size(); ++i) {
            if (m_hours.at(i).time.toTimeZone(m_zone).date() == wanted) {
                firstOfDay = i;
                break;
            }
        }
    }

    if (firstOfDay < 0) {
        m_start = qMax(0, current - kPastHours);
        m_count = qMin(kWindowHours, int(m_hours.size()) - m_start);
    } else {
        m_start = firstOfDay;
        m_count = qMin(kDayHours, int(m_hours.size()) - m_start);
    }

    // Deliberately not clamped. See the header: this is an offset to the
    // present, and the chart's past veil is `xForIndex(nowIndex)` wide — so a
    // day still ahead of us produces a negative width and veils nothing, and a
    // day behind us produces one wider than the plot and veils all of it,
    // without a single branch in the QML.
    m_nowIndex = current - m_start;

    // "Now" has to be a labelled column or the word is never drawn, and the
    // labels run every `labelStep` hours from `firstLabelIndex`. So the phase
    // of the label sequence is chosen by where now landed rather than fixed —
    // which is the one thing that has to move when the window's start does.
    //
    // On a day window there is no "Now" to land on, so the phase is chosen by
    // the other rule the chart lives by: the header band centres a two-column
    // entry on each label, so a label in column 0 is a label half outside the
    // plot. A day window opens at column 0 — it has nowhere further left to go —
    // so its first label is column 1, one column inside the edge, which is
    // exactly where today's window puts its first one.
    //
    // The cost is odd hours: 1 AM, 3 AM, and so on. That is not a new look. A
    // today window takes its phase from where the present landed and is odd half
    // the time already, and the alternative here is a chart of Friday that opens
    // with "AM" sliced down the middle.
    m_labelStep       = 2;
    m_firstLabelIndex = nowInWindow() ? m_nowIndex % m_labelStep : 1;

    const QDateTime first = localTimeAt(0);
    m_startHour = first.isValid() ? first.time().hour() : 0;

    m_labelIndices.clear();
    for (int i = m_firstLabelIndex; i < m_count; i += m_labelStep)
        m_labelIndices.append(i);
}

QDateTime ForecastData::localTimeAt(int index) const
{
    const int absolute = m_start + index;
    if (absolute < 0 || absolute >= m_hours.size())
        return {};
    return m_hours.at(absolute).time.toTimeZone(m_zone);
}

// ---- the series ------------------------------------------------------------------

void ForecastData::buildSeries()
{
    // Air quality arrives from a different endpoint on a different time axis
    // and at a different horizon — five days against sixteen. Matched by
    // timestamp rather than by index, because the two series do not start at
    // the same hour and lining them up by position would put yesterday's
    // pollution under tomorrow's temperature.
    QHash<qint64, double> aqiByHour;
    for (const AirQualityPoint &point : m_air.hourly) {
        if (point.europeanAqi)
            aqiByHour.insert(point.time.toSecsSinceEpoch(), double(*point.europeanAqi));
    }

    for (int i = 0; i < m_count; ++i) {
        const HourlyPoint &hour = m_hours.at(m_start + i);

        m_temperature.append(display(hour.temperature, Units::Quantity::Temperature));
        m_apparent.append(display(hour.apparentTemperature, Units::Quantity::Temperature));
        if (hour.apparentTemperature.has_value())
            m_hasApparent = true;
        m_precipProb.append(rounded(hour.precipitationProbability));
        m_cloud.append(rounded(hour.cloudCover));
        m_humidity.append(rounded(hour.relativeHumidity));
        m_windSpeed.append(display(hour.windSpeed, Units::Quantity::Wind));
        m_windGust.append(display(hour.windGust, Units::Quantity::Wind));
        m_windDirection.append(rounded(hour.windDirection));
        m_pressure.append(display(hour.pressureMsl, Units::Quantity::Pressure));
        m_uvIndex.append(value(hour.uvIndex));
        m_visibility.append(display(hour.visibility, Units::Quantity::Visibility));

        // Millimetres, always. See the header: precip.js's intensity bands are
        // statements about millimetres and converting them would reclassify
        // every rain band in the app.
        const double mm = value(hour.precipitation);
        m_precipMm.append(mm);

        // The type comes from the weather code and nothing else. An hour with
        // rain in it and no code is dry as far as the wash is concerned, which
        // is the honest reading: we do not know what was falling.
        QString type;
        if (hour.weatherCode && mm >= kTrace) {
            const PrecipitationType kind = precipitationTypeFor(*hour.weatherCode);
            if (kind != PrecipitationType::None)
                type = precipitationTypeName(kind);
        }
        m_precipTypes.append(type);

        const auto aqi = aqiByHour.constFind(hour.time.toSecsSinceEpoch());
        m_airQuality.append(aqi == aqiByHour.cend() ? qQNaN() : *aqi);
    }
}

// ---- days ------------------------------------------------------------------------

void ForecastData::buildDays(const QDateTime &now)
{
    const QDate today = now.toTimeZone(m_zone).date();

    QVariantList all;
    QList<QDate> allDates;
    int          todayRow = -1;

    for (const DailyPoint &day : m_forecast.daily) {
        QVariantMap entry;
        entry[QStringLiteral("date")]    = day.date.day();
        entry[QStringLiteral("month")]   = day.date.month();
        entry[QStringLiteral("weekday")] = shortWeekday(day.date);

        const qint64 offset = today.daysTo(day.date);
        entry[QStringLiteral("label")] = offset == 0   ? tr("Today")
                                       : offset == -1  ? tr("Yesterday")
                                       : offset == 1   ? tr("Tomorrow")
                                                       : shortWeekday(day.date);

        entry[QStringLiteral("high")] = display(day.temperatureMax, Units::Quantity::Temperature);
        entry[QStringLiteral("low")]  = display(day.temperatureMin, Units::Quantity::Temperature);
        entry[QStringLiteral("precip")] = rounded(day.precipitationProbabilityMax);

        // Two glyphs per day, the same code read twice. A day card shows the
        // daytime one; selecting it reveals the pair, which is the shape
        // DayStrip has drawn since the prototype.
        const int code = day.weatherCode ? *day.weatherCode : -1;
        entry[QStringLiteral("icon")] =
            code < 0 ? QString() : conditionKindName(clima::conditionFor(code, true));
        entry[QStringLiteral("nightIcon")] =
            code < 0 ? QString() : conditionKindName(clima::conditionFor(code, false));

        if (day.date == today)
            todayRow = int(all.size());
        all.append(entry);
        allDates.append(day.date);
    }

    if (todayRow < 0) {
        m_days       = all;
        m_dayDates   = allDates;
        m_todayIndex = 0;
        return;
    }

    // Yesterday first, then today, then nine more — eleven cards, which is what
    // the day strip has always drawn and what the "10 Day" screen slices ten
    // out of. Trimmed rather than passed through whole because a provider that
    // sends two past days would put the day strip's selection on the wrong card
    // and a provider that sends none would leave it off the end.
    const int from = qMax(0, todayRow - 1);
    const int to   = qMin(int(all.size()), todayRow + 10);
    for (int i = from; i < to; ++i) {
        m_days.append(all.at(i));
        m_dayDates.append(allDates.at(i));
    }
    m_todayIndex = todayRow - from;
}

// ---- the calendar ----------------------------------------------------------------

void ForecastData::buildMonth(const QDateTime &now)
{
    const QDate today = now.toTimeZone(m_zone).date();
    const QDate first(today.year(), today.month(), 1);

    m_month[QStringLiteral("name")]   = QLocale().monthName(today.month(), QLocale::LongFormat);
    m_month[QStringLiteral("year")]   = today.year();
    m_month[QStringLiteral("number")] = today.month();
    m_month[QStringLiteral("length")] = today.daysInMonth();
    // 0 = Sunday, which is the convention weekdayNames() and the calendar grid
    // share. Qt counts Monday as 1 and Sunday as 7.
    m_month[QStringLiteral("firstWeekday")] = first.dayOfWeek() % 7;
    m_month[QStringLiteral("today")]        = today.day();

    for (int d = 1; d <= today.daysInMonth(); ++d) {
        const QDate      date  = QDate(today.year(), today.month(), d);
        const QVariantMap known = dayFor(d, today.month());

        QVariantMap cell;
        cell[QStringLiteral("date")]    = d;
        cell[QStringLiteral("weekday")] = shortWeekday(date);
        cell[QStringLiteral("isToday")] = date == today;

        // A day outside the forecast horizon carries no numbers, and the
        // calendar draws the cell empty. mockdata.js generated a plausible
        // seasonal shape for those days; a real calendar must not, because a
        // high for the 29th that nobody forecast is a number a reader will
        // plan around. docs/08-risks.md R9.
        cell[QStringLiteral("high")] = known.value(QStringLiteral("high"), QVariant());
        cell[QStringLiteral("low")]  = known.value(QStringLiteral("low"), QVariant());
        cell[QStringLiteral("icon")] = known.value(QStringLiteral("icon"), QString());
        cell[QStringLiteral("known")] = !known.isEmpty();

        m_monthDays.append(cell);
    }
}

QVariantMap ForecastData::dayFor(int date, int month) const
{
    for (const QVariant &entry : m_days) {
        const QVariantMap day = entry.toMap();
        if (day.value(QStringLiteral("date")).toInt() == date
            && day.value(QStringLiteral("month")).toInt() == month) {
            return day;
        }
    }
    return {};
}

int ForecastData::weekdayOf(int date) const
{
    return (m_month.value(QStringLiteral("firstWeekday")).toInt() + date - 1) % 7;
}

// ---- the sun and the moon ---------------------------------------------------------

void ForecastData::buildSunEvents()
{
    const QDateTime windowStart = m_count > 0 ? m_hours.at(m_start).time : QDateTime();
    if (!windowStart.isValid())
        return;

    const auto place = [&](const QDateTime &instant, const QString &kind) {
        if (!instant.isValid())
            return;
        // Fractional, so a marker can sit between two samples — the sun does
        // not rise on the hour.
        const double index = windowStart.secsTo(instant) / 3600.0;
        if (index < 0 || index > m_count - 1)
            return;

        QVariantMap event;
        event[QStringLiteral("index")] = index;
        event[QStringLiteral("kind")]  = kind;
        // The badge on a sunrise or sunset marker in the hourly chart. It was
        // the sixth and last clock in this application to be formatted by hand,
        // and the one that made the case for TimeFormat: it is two badges on a
        // chart whose axis is right above them, so a 24-hour axis over a 12-hour
        // badge is a contradiction inside one card.
        event[QStringLiteral("text")] =
            TimeFormat::instance()->clock(instant.toTimeZone(m_zone).time());
        m_sunEvents.append(event);
    };

    for (const DailyPoint &day : m_forecast.daily) {
        place(day.sunrise, QStringLiteral("sunrise"));
        place(day.sunset, QStringLiteral("sunset"));
    }

    // The moon of the day the window is of, which is today until the day strip
    // says otherwise. It is read by one thing — the chart's own legend, right
    // under the plot — so a legend naming today's phase over Friday's hours
    // would be the chart contradicting itself. The phase is a position in the
    // cycle and the illumination is the lit fraction, which is not linear in
    // it: libclima computes the second from the first so that a waxing crescent
    // is not reported as a quarter lit.
    const QDate today = m_now.toTimeZone(m_zone).date();
    const QDate wanted = m_dayDates.value(m_selectedDay, today);
    for (const DailyPoint &day : m_forecast.daily) {
        if (day.date != wanted)
            continue;
        const Reading lit = moonIllumination(day.moonPhase);
        m_moonPhase[QStringLiteral("name")]        = moonPhaseLabel(moonPhaseName(day.moonPhase));
        m_moonPhase[QStringLiteral("illuminated")] = lit.has_value() ? *lit : 0.0;
        break;
    }
}

// ---- the precipitation strip -------------------------------------------------------

void ForecastData::buildBuckets()
{
    // One bucket per label interval, carrying the interval's peak probability —
    // mockdata.js's rule, kept because the strip is a row of two-hour columns
    // and the honest number for a column is the worst hour in it.
    for (int i = m_firstLabelIndex; i < m_count - 1; i += m_labelStep) {
        double peak = m_precipProb.value(i).toDouble();
        for (int k = 1; k < m_labelStep && i + k < m_count; ++k)
            peak = qMax(peak, m_precipProb.value(i + k).toDouble());

        QVariantMap bucket;
        bucket[QStringLiteral("index")] = i;
        bucket[QStringLiteral("span")]  = m_labelStep;
        bucket[QStringLiteral("prob")]  = qIsNaN(peak) ? 0 : int(peak);
        m_precipBuckets.append(bucket);
    }
}

// ---- the helpers -------------------------------------------------------------------

bool ForecastData::isNight(int index) const
{
    const int absolute = m_start + index;
    if (absolute < 0 || absolute >= m_hours.size())
        return false;

    const std::optional<bool> day = m_hours.at(absolute).isDay;

    // Absent means we were not told, and the honest default is day: a night
    // glyph on an hour that turns out to be noon is the more wrong of the two,
    // and it is also the one a reader notices.
    return day.has_value() && !*day;
}

QString ForecastData::conditionFor(int index) const
{
    const int absolute = m_start + index;
    if (absolute < 0 || absolute >= m_hours.size())
        return {};

    const WeatherCode code = m_hours.at(absolute).weatherCode;
    if (!code)
        return {};

    return conditionKindName(clima::conditionFor(*code, !isNight(index)));
}

QString ForecastData::conditionForLabel(int index) const
{
    // The span this label stands for: itself and the columns up to the next
    // label. Clamped to the window, so the last label answers for whatever is
    // left rather than reading past the end of the day.
    const int last = qMin(index + m_labelStep, m_count);

    QList<int> codes;
    for (int i = index; i < last; ++i) {
        const int absolute = m_start + i;
        if (absolute < 0 || absolute >= m_hours.size())
            continue;
        if (const WeatherCode code = m_hours.at(absolute).weatherCode)
            codes.append(*code);
    }

    const std::optional<int> worst = codeForLabelledSpan(codes);
    if (!worst)
        return {};

    // Whichever hour carried the winning code decides day or night. Taking the
    // first hour's would put a moon over an evening thunderstorm whenever the
    // span opened after sunset and the storm was in its second hour.
    int at = index;
    for (int i = index; i < last; ++i) {
        const int absolute = m_start + i;
        if (absolute < 0 || absolute >= m_hours.size())
            continue;
        const WeatherCode code = m_hours.at(absolute).weatherCode;
        if (code && *code == *worst) {
            at = i;
            break;
        }
    }

    return conditionKindName(clima::conditionFor(*worst, !isNight(at)));
}

QString ForecastData::conditionText(int index) const
{
    const int absolute = m_start + index;
    if (absolute < 0 || absolute >= m_hours.size())
        return QStringLiteral("—");

    const WeatherCode code = m_hours.at(absolute).weatherCode;
    if (!code)
        return QStringLiteral("—");

    const QString text = clima::conditionText(*code, !isNight(index));
    return text.isEmpty() ? QStringLiteral("—") : text;
}

QString ForecastData::hourLabel(int index) const
{
    // `nowInWindow()` and not just the comparison: on a day window `nowIndex`
    // is an offset that can land anywhere, and 0 == 0 would print "Now" over
    // Friday midnight on the one afternoon a year the arithmetic agreed.
    if (nowInWindow() && index == m_nowIndex)
        return tr("Now");
    return clockLabel(index);
}

QVariantMap ForecastData::ahead(int offset) const
{
    QVariantMap out;

    const int absolute = m_nowAbsolute + offset;
    if (absolute < 0 || absolute >= m_hours.size())
        return out;

    const HourlyPoint &hour = m_hours.at(absolute);
    const bool night = hour.isDay.has_value() && !*hour.isDay;

    out[QStringLiteral("temperature")] =
        display(hour.temperature, Units::Quantity::Temperature);
    out[QStringLiteral("apparent")] =
        display(hour.apparentTemperature, Units::Quantity::Temperature);
    out[QStringLiteral("precipProb")] = rounded(hour.precipitationProbability);
    out[QStringLiteral("night")]      = night;
    out[QStringLiteral("condition")] =
        hour.weatherCode
            ? conditionKindName(clima::conditionFor(*hour.weatherCode, !night))
            : QString();
    out[QStringLiteral("label")] =
        offset == 0 ? tr("Now") : hourOf(hour.time.toTimeZone(m_zone));

    return out;
}

QString ForecastData::clockLabel(int index) const
{
    const QDateTime local = localTimeAt(index);
    if (!local.isValid())
        return {};
    return hourOf(local);
}
