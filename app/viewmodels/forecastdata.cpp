// SPDX-FileCopyrightText: 2026 Jowi Aoun
// SPDX-License-Identifier: GPL-3.0-or-later

#include "forecastdata.h"

#include "appengine.h"
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


// "3 PM". The reference's spelling, and the one mockdata.js used.
QString twelveHour(const QDateTime &local)
{
    const int hour   = local.time().hour();
    const int shown  = hour % 12 == 0 ? 12 : hour % 12;
    return QString::number(shown) + QLatin1Char(' ')
         + (hour < 12 ? ForecastData::tr("AM") : ForecastData::tr("PM"));
}

} // namespace

ForecastData::ForecastData(QObject *parent)
    : QObject(parent)
{
    // Everything on screen that carries a unit is rebuilt when a preference
    // changes. Cheaper would be to convert lazily in the getters; that would
    // also mean nothing notifies, and a settings screen whose effect appears
    // after the next refresh is a settings screen that looks broken.
    connect(Units::instance(), &Units::changed, this, [this]() {
        if (!m_forecast.isEmpty())
            setSnapshot(m_forecast, m_air, m_now, m_place);
    });
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

void ForecastData::clear()
{
    m_hours.clear();
    m_count = 0;
    m_nowIndex = 0;
    m_start = 0;
    m_temperature.clear(); m_apparent.clear(); m_precipProb.clear(); m_precipMm.clear();
    m_cloud.clear(); m_humidity.clear(); m_windSpeed.clear(); m_windGust.clear();
    m_windDirection.clear(); m_pressure.clear(); m_uvIndex.clear(); m_visibility.clear();
    m_airQuality.clear(); m_precipTypes.clear();
    m_hasApparent = false;
    m_days.clear(); m_todayIndex = 0;
    m_month.clear(); m_monthDays.clear();
    m_sunEvents.clear(); m_moonPhase.clear();
    m_labelIndices.clear(); m_precipBuckets.clear();
}

void ForecastData::setSnapshot(const Forecast &forecast, const AirQuality &airQuality,
                               const QDateTime &now, const Place &place)
{
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

    buildWindow(now);
    buildSeries();
    buildDays(now);
    buildMonth(now);
    buildSunEvents();
    buildBuckets();

    Q_EMIT changed();
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

    m_start = qMax(0, current - kPastHours);
    m_count = qMin(kWindowHours, int(m_hours.size()) - m_start);
    m_nowIndex = current - m_start;

    // "Now" has to be a labelled column or the word is never drawn, and the
    // labels run every `labelStep` hours from `firstLabelIndex`. So the phase
    // of the label sequence is chosen by where now landed rather than fixed —
    // which is the one thing that has to move when the window's start does.
    m_labelStep       = 2;
    m_firstLabelIndex = m_nowIndex % m_labelStep;

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
            code < 0 ? QString()
                     : conditionKindName(drawableToday(clima::conditionFor(code, true)));
        entry[QStringLiteral("nightIcon")] =
            code < 0 ? QString()
                     : conditionKindName(drawableToday(clima::conditionFor(code, false)));

        if (day.date == today)
            todayRow = int(all.size());
        all.append(entry);
    }

    if (todayRow < 0) {
        m_days       = all;
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
    for (int i = from; i < to; ++i)
        m_days.append(all.at(i));
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
        event[QStringLiteral("text")]  = QLocale().toString(instant.toTimeZone(m_zone).time(),
                                                            QStringLiteral("h:mm AP"));
        m_sunEvents.append(event);
    };

    for (const DailyPoint &day : m_forecast.daily) {
        place(day.sunrise, QStringLiteral("sunrise"));
        place(day.sunset, QStringLiteral("sunset"));
    }

    // Today's moon. The phase is a position in the cycle and the illumination
    // is the lit fraction, which is not linear in it — libclima computes the
    // second from the first so that a waxing crescent is not reported as a
    // quarter lit.
    const QDate today = m_now.toTimeZone(m_zone).date();
    for (const DailyPoint &day : m_forecast.daily) {
        if (day.date != today)
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

    // drawableToday() degrades to the seven kinds WeatherGlyph.qml can
    // actually draw. Without it, snow and fog render as an empty item, which
    // reads as "no data" rather than as "weather we have no picture for".
    return conditionKindName(drawableToday(clima::conditionFor(*code, !isNight(index))));
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
    if (index == m_nowIndex)
        return tr("Now");
    return clockLabel(index);
}

QString ForecastData::clockLabel(int index) const
{
    const QDateTime local = localTimeAt(index);
    if (!local.isValid())
        return {};
    return twelveHour(local);
}
