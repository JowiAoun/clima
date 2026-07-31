// SPDX-FileCopyrightText: 2026 Jowi Aoun
// SPDX-License-Identifier: GPL-3.0-or-later

#include "conditionsdata.h"

#include "appengine.h"
#include "forecastdata.h"
#include "units.h"

#include "libclima/domain/hourconvention.h"
#include "libclima/domain/timeaxis.h"
#include "libclima/domain/weathercode.h"

#include <QLocale>
#include <QStringList>
#include <QQmlEngine>

#include <cmath>

using namespace clima;

namespace {

// Six behind, five ahead. detaildata.js's window, kept because every card's
// sparkline geometry is built around where index 6 falls.
constexpr int kContextBefore = 6;
constexpr int kContextAfter  = 6;

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

int roundedDisplay(const Reading &reading, Units::Quantity quantity)
{
    const double converted = display(reading, quantity);
    return qIsNaN(converted) ? 0 : int(std::lround(converted));
}

QString clockLabel(const QDateTime &instant, const QTimeZone &zone)
{
    if (!instant.isValid())
        return {};
    return QLocale().toString(instant.toTimeZone(zone).time(), QStringLiteral("h:mm AP"));
}

// "3:00 p.m." — the reference's spelling for a sentence, as distinct from the
// "3:00 PM" a label uses. detaildata.js used both, in the same two places.
QString sentenceTime(const QDateTime &instant, const QTimeZone &zone)
{
    if (!instant.isValid())
        return {};
    const QTime time = instant.toTimeZone(zone).time();
    const int   hour = time.hour() % 12 == 0 ? 12 : time.hour() % 12;
    return QStringLiteral("%1:%2 %3")
        .arg(hour)
        .arg(time.minute(), 2, 10, QLatin1Char('0'))
        .arg(time.hour() < 12 ? ConditionsData::tr("a.m.") : ConditionsData::tr("p.m."));
}

// ---- the published scales ---------------------------------------------------
//
// Each of these is somebody else's table, transcribed. None of them is a
// judgement of ours, and none of them may be adjusted to make a card look
// better balanced.

// WHO: low 0-2, moderate 3-5, high 6-7, very high 8-10, extreme 11+.
QString uvBand(double index)
{
    if (index < 3)  return ConditionsData::tr("Low");
    if (index < 6)  return ConditionsData::tr("Moderate");
    if (index < 8)  return ConditionsData::tr("High");
    if (index < 11) return ConditionsData::tr("Very high");
    return ConditionsData::tr("Extreme");
}

// The European AQI's own bands: 0-20 good, 20-40 fair, 40-60 moderate,
// 60-80 poor, 80-100 very poor, 100+ extremely poor.
QString aqiBand(double index)
{
    if (index <= 20)  return ConditionsData::tr("Good");
    if (index <= 40)  return ConditionsData::tr("Fair");
    if (index <= 60)  return ConditionsData::tr("Moderate");
    if (index <= 80)  return ConditionsData::tr("Poor");
    if (index <= 100) return ConditionsData::tr("Very poor");
    return ConditionsData::tr("Extremely poor");
}

// The Beaufort scale, in km/h, from the standard v = 0.836 B^1.5 in m/s.
int beaufortFor(double kmh)
{
    if (qIsNaN(kmh))
        return 0;
    const double ms = kmh / 3.6;
    return qBound(0, int(std::floor(std::pow(ms / 0.836, 2.0 / 3.0) + 0.5)), 12);
}

QString beaufortName(int force)
{
    switch (force) {
    case 0:  return ConditionsData::tr("Calm");
    case 1:  return ConditionsData::tr("Light air");
    case 2:  return ConditionsData::tr("Light breeze");
    case 3:  return ConditionsData::tr("Gentle breeze");
    case 4:  return ConditionsData::tr("Moderate breeze");
    case 5:  return ConditionsData::tr("Fresh breeze");
    case 6:  return ConditionsData::tr("Strong breeze");
    case 7:  return ConditionsData::tr("Near gale");
    case 8:  return ConditionsData::tr("Gale");
    case 9:  return ConditionsData::tr("Severe gale");
    case 10: return ConditionsData::tr("Storm");
    case 11: return ConditionsData::tr("Violent storm");
    default: return ConditionsData::tr("Hurricane force");
    }
}

// The sixteen-point compass. Sixteen and not thirty-two: a forecast's wind
// direction is a model average and NNE-by-E is a precision it does not have.
QString compassPoint(double degrees)
{
    static const char *const points[] = { "N",  "NNE", "NE", "ENE", "E",  "ESE", "SE", "SSE",
                                          "S",  "SSW", "SW", "WSW", "W",  "WNW", "NW", "NNW" };
    if (qIsNaN(degrees))
        return {};
    const int index = int(std::lround(degrees / 22.5)) & 15;
    return QString::fromLatin1(points[index]);
}

QString visibilityBand(double km)
{
    if (qIsNaN(km))     return {};
    if (km >= 16)       return ConditionsData::tr("Excellent");
    if (km >= 10)       return ConditionsData::tr("Good");
    if (km >= 4)        return ConditionsData::tr("Moderate");
    if (km >= 1)        return ConditionsData::tr("Poor");
    return ConditionsData::tr("Very poor");
}

// up | down | steady, from the difference between now and three hours out.
// The badge tracks the *number*; whether that direction is good news is the
// body's job. docs/10-design-system.md §10.5.
QString trendOf(double from, double to, double deadband)
{
    if (qIsNaN(from) || qIsNaN(to))
        return QStringLiteral("steady");
    const double delta = to - from;
    if (delta > deadband)
        return QStringLiteral("up");
    if (delta < -deadband)
        return QStringLiteral("down");
    return QStringLiteral("steady");
}

QString toneFor(bool good, bool caution)
{
    return good ? QStringLiteral("good")
                : (caution ? QStringLiteral("caution") : QStringLiteral("poor"));
}


// ---- absent is not zero, and the screen has to say so ---------------------
//
// libclima/domain/forecast.h opens with this argument and it lands here: "a
// plain double for a field MET Norway does not carry means the gust row reads
// '0 km/h' during a gale and nothing anywhere goes red." Every block below
// therefore carries a `reading` — the value and its unit, already formatted, or
// an em dash — and every QML file that PRINTS a reading uses that rather than
// gluing `value` to `unit`. The numeric `value` beside it stays, because a bar
// and a colour ramp need something finite to scale against; zero is a harmless
// length and only a harmful sentence.
//
// Seen on screen before it was fixed: MET Norway served as the fallback and the
// hero read "Feels like 0°" on a 28 °C afternoon.
QString readingOf(const Reading &canonical, Units::Quantity quantity)
{
    if (!canonical.has_value())
        return QStringLiteral("\u2014");
    return Units::instance()->format(quantity, *canonical);
}

QVariantMap activity(const QString &name, const QString &status, const QString &tone)
{
    return QVariantMap{ { QStringLiteral("name"), name },
                        { QStringLiteral("status"), status },
                        { QStringLiteral("tone"), tone } };
}

QVariantMap stop(double p, const QString &colour)
{
    return QVariantMap{ { QStringLiteral("p"), p }, { QStringLiteral("c"), colour } };
}

} // namespace

ConditionsData::ConditionsData(QObject *parent)
    : QObject(parent)
{
    connect(Units::instance(), &Units::changed, this, [this]() {
        if (!m_forecast.isEmpty())
            setSnapshot(m_forecast, m_air, m_now, m_place, m_hasPollen);
    });
}

ConditionsData *ConditionsData::create(QQmlEngine *, QJSEngine *)
{
    ConditionsData *data = AppEngine::instance()->conditionsData();
    QQmlEngine::setObjectOwnership(data, QQmlEngine::CppOwnership);
    return data;
}

void ConditionsData::setPlace(const Place &place)
{
    m_place = place;
    m_location = QVariantMap{
        { QStringLiteral("name"), place.name },
        { QStringLiteral("region"), place.region() },
        { QStringLiteral("label"), place.label() },
        { QStringLiteral("isHome"), place.isHome },
    };
    Q_EMIT changed();
}

void ConditionsData::clear()
{
    m_hours.clear();
    m_hourNow = 0;
    m_from    = 0;
    m_observation = {};
    m_nowIndex = kContextBefore;
    m_observedAt.clear();
    m_observedOn.clear();
    m_current.clear();
    m_temperature.clear(); m_feelsLike.clear(); m_cloudCover.clear();
    m_precipitation.clear(); m_wind.clear(); m_humidity.clear();
    m_uv.clear(); m_airQuality.clear(); m_visibility.clear();
    m_pressure.clear(); m_sun.clear(); m_moon.clear();
    m_pollen.clear(); m_activities.clear();
}

void ConditionsData::setSnapshot(const Forecast &forecast, const AirQuality &airQuality,
                                 const QDateTime &now, const Place &place, bool hasPollen)
{
    m_forecast = forecast;
    m_air      = airQuality;
    m_now      = now;

    if (forecast.timeZone.isValid())
        m_zone = forecast.timeZone;
    else if (!place.timezone.isEmpty())
        m_zone = QTimeZone(place.timezone.toUtf8());
    else
        m_zone = QTimeZone::UTC;

    clear();
    setPlace(place);

    if (forecast.isEmpty()) {
        Q_EMIT changed();
        return;
    }

    m_hours = asHourStarting(forecast.hourly);

    buildContext();
    buildTemperature();
    buildFeelsLike();
    buildCloud();
    buildPrecipitation();
    buildWind();
    buildHumidity();
    buildUv();
    buildAirQuality();
    buildVisibility();
    buildPressure();
    buildSunMoon();
    buildPollen(hasPollen);
    buildActivities();
    buildSummary();

    Q_EMIT changed();
}

void ConditionsData::buildContext()
{
    for (int i = 0; i < m_hours.size(); ++i) {
        if (m_hours.at(i).time <= m_now)
            m_hourNow = i;
        else
            break;
    }

    m_from     = qMax(0, m_hourNow - kContextBefore);
    m_nowIndex = m_hourNow - m_from;

    // ---- which reading is "now" -------------------------------------------
    //
    // See the declaration of m_observation. Open-Meteo's `current` block is
    // stamped to the quarter hour, which is why the reference reads 12:28
    // rather than 12:00 — it is a reading and not a slot, and it is the right
    // one to show whenever it is actually current.
    const QDateTime provided = m_forecast.current.time;
    const bool      usable   = provided.isValid() && qAbs(provided.secsTo(m_now)) <= 3600;

    if (usable) {
        m_observation = m_forecast.current;
    } else if (m_hourNow < m_hours.size()) {
        // Rebuilt from the hour we are standing in. Field for field rather than
        // by any clever means, because the two structs differ in exactly one
        // way — an HourlyPoint has no dew point on some providers — and a memcpy
        // would not have noticed.
        const HourlyPoint &hour = m_hours.at(m_hourNow);
        m_observation                     = {};
        m_observation.time                = hour.time;
        m_observation.temperature         = hour.temperature;
        m_observation.apparentTemperature = hour.apparentTemperature;
        m_observation.relativeHumidity    = hour.relativeHumidity;
        m_observation.dewPoint            = hour.dewPoint;
        m_observation.precipitation       = hour.precipitation;
        m_observation.windSpeed           = hour.windSpeed;
        m_observation.windGust            = hour.windGust;
        m_observation.windDirection       = hour.windDirection;
        m_observation.pressureMsl         = hour.pressureMsl;
        m_observation.cloudCover          = hour.cloudCover;
        m_observation.visibility          = hour.visibility;
        m_observation.uvIndex             = hour.uvIndex;
        m_observation.weatherCode         = hour.weatherCode;
        m_observation.isDay               = hour.isDay;
    }

    const QDateTime observed = m_observation.time.isValid() ? m_observation.time : m_now;
    m_observedAt = clockLabel(observed, m_zone);
    m_observedOn = QLocale().toString(observed.toTimeZone(m_zone).date(),
                                      QStringLiteral("dddd, d MMMM yyyy"));
}

QVariantList ConditionsData::window(Reading HourlyPoint::*field, int quantity) const
{
    QVariantList out;
    for (int i = m_from; i < qMin(int(m_hours.size()), m_from + kContextBefore + kContextAfter); ++i) {
        const double converted =
            display(m_hours.at(i).*field, static_cast<Units::Quantity>(quantity));
        out.append(qIsNaN(converted) ? QVariant() : QVariant(int(std::lround(converted))));
    }
    return out;
}

DailyPoint ConditionsData::today() const
{
    const QDate date = m_now.toTimeZone(m_zone).date();
    for (const DailyPoint &day : m_forecast.daily) {
        if (day.date == date)
            return day;
    }
    return m_forecast.daily.isEmpty() ? DailyPoint{} : m_forecast.daily.constFirst();
}

// ---- temperature -------------------------------------------------------------------

void ConditionsData::buildTemperature()
{
    const CurrentConditions &now = m_observation;
    const DailyPoint         day = today();

    const int  reading = roundedDisplay(now.temperature, Units::Quantity::Temperature);
    const int  high    = roundedDisplay(day.temperatureMax, Units::Quantity::Temperature);
    const int  low     = roundedDisplay(day.temperatureMin, Units::Quantity::Temperature);

    // The hour today's extremes actually fall in, found rather than assumed.
    // "Peaks at 3 p.m." is true in July in Toronto and not in December in
    // Tromsø, and a card that says it anyway is a card nobody can trust about
    // anything else either.
    QDateTime peakAt;
    QDateTime lowAt;
    double    peak = -1e9;
    double    dip  = 1e9;
    for (const HourlyPoint &hour : m_hours) {
        if (hour.time.toTimeZone(m_zone).date() != day.date || !hour.temperature)
            continue;
        if (*hour.temperature > peak) { peak = *hour.temperature; peakAt = hour.time; }
        if (*hour.temperature < dip)  { dip  = *hour.temperature; lowAt  = hour.time; }
    }

    const QString unit  = Units::instance()->symbol(Units::Quantity::Temperature);
    const double  later = display(m_hours.value(qMin(m_hourNow + 3, int(m_hours.size()) - 1))
                                      .temperature,
                                  Units::Quantity::Temperature);
    const QString trend = trendOf(display(now.temperature, Units::Quantity::Temperature), later, 0.5);

    m_temperature = QVariantMap{
        { QStringLiteral("value"), reading },
        { QStringLiteral("reading"), readingOf(now.temperature, Units::Quantity::Temperature) },
        { QStringLiteral("unit"), unit },
        { QStringLiteral("series"), window(&HourlyPoint::temperature,
                                           int(Units::Quantity::Temperature)) },
        { QStringLiteral("high"), high },
        { QStringLiteral("low"), low },
        { QStringLiteral("peakAt"), sentenceTime(peakAt, m_zone) },
        { QStringLiteral("lowAt"), sentenceTime(lowAt, m_zone) },
        { QStringLiteral("trend"), trend },
        { QStringLiteral("status"), trend == QLatin1String("up")     ? tr("Rising")
                                    : trend == QLatin1String("down") ? tr("Falling")
                                                                     : tr("Steady") },
        { QStringLiteral("body"),
          tr("Peaks at %1%2 around %3. Overnight low of %4%2 at %5.")
              .arg(high).arg(unit).arg(sentenceTime(peakAt, m_zone))
              .arg(low).arg(sentenceTime(lowAt, m_zone)) },
    };
}

void ConditionsData::buildFeelsLike()
{
    const CurrentConditions &now = m_observation;

    const int    apparent = roundedDisplay(now.apparentTemperature, Units::Quantity::Temperature);
    const int    actual   = roundedDisplay(now.temperature, Units::Quantity::Temperature);
    const double deltaC   = value(now.apparentTemperature) - value(now.temperature);

    // Which factor is doing it, from the numbers rather than from a guess.
    // Humidity makes warm air feel warmer, wind makes cold air feel colder, and
    // the sign of the difference says which of the two is in play.
    const bool    warmer = deltaC > 0;
    const QString factor = warmer ? (value(now.relativeHumidity) >= 60
                                         ? QStringLiteral("humidity")
                                         : QStringLiteral("sun"))
                                  : QStringLiteral("wind");

    m_feelsLike = QVariantMap{
        { QStringLiteral("value"), apparent },
        { QStringLiteral("reading"), readingOf(now.apparentTemperature, Units::Quantity::Temperature) },
        { QStringLiteral("actual"), actual },
        { QStringLiteral("unit"), Units::instance()->symbol(Units::Quantity::Temperature) },
        { QStringLiteral("series"), window(&HourlyPoint::apparentTemperature,
                                           int(Units::Quantity::Temperature)) },
        { QStringLiteral("dominantFactor"), factor },
        { QStringLiteral("trend"), trendOf(0, deltaC, 0.5) },
        { QStringLiteral("status"), std::abs(deltaC) < 1 ? tr("As it looks")
                                    : warmer            ? tr("Warmer than it is")
                                                        : tr("Colder than it is") },
        { QStringLiteral("body"),
          std::abs(deltaC) < 1
              ? tr("It feels about as warm as the thermometer says.")
              : warmer ? tr("Feels warmer than the actual temperature because of the %1.")
                             .arg(factor == QLatin1String("humidity") ? tr("humidity")
                                                                      : tr("sunshine"))
                       : tr("Feels colder than the actual temperature because of the wind.") },
    };
}

void ConditionsData::buildCloud()
{
    const CurrentConditions &now  = m_observation;
    const int                cover = int(std::lround(value(now.cloudCover)));

    const bool day = now.isDay.value_or(true);
    const QString condition = now.weatherCode
        ? clima::conditionText(*now.weatherCode, day)
        : QStringLiteral("—");

    const double later = value(m_hours.value(qMin(m_hourNow + 3, int(m_hours.size()) - 1)).cloudCover);
    const QString trend = trendOf(value(now.cloudCover), later, 5);

    m_cloudCover = QVariantMap{
        { QStringLiteral("value"), qIsNaN(value(now.cloudCover)) ? 0 : cover },
        { QStringLiteral("reading"), readingOf(now.cloudCover, Units::Quantity::Percentage) },
        { QStringLiteral("unit"), QStringLiteral("%") },
        { QStringLiteral("condition"), condition },
        { QStringLiteral("trend"), trend },
        { QStringLiteral("status"), condition },
        { QStringLiteral("body"), trend == QLatin1String("up")
                                      ? tr("Clouding over through the next few hours.")
                                  : trend == QLatin1String("down")
                                      ? tr("Clearing through the next few hours.")
                                      : tr("Little change in cloud cover expected.") },
    };
}

// ---- precipitation -----------------------------------------------------------------

void ConditionsData::buildPrecipitation()
{
    // Twenty-four hours from now, summed in millimetres and converted once.
    double totalMm = 0;
    for (int i = m_hourNow; i < qMin(int(m_hours.size()), m_hourNow + 24); ++i)
        totalMm += m_hours.at(i).precipitation.value_or(0.0);

    const Units *units = Units::instance();
    const auto   kind  = Units::Quantity::Precipitation;

    // The next wet run: when it starts and how hard it gets. This is the
    // sentence's source and the card's, so the two cannot disagree.
    int    startsAt = -1;
    double peakMm   = 0;
    for (int i = m_hourNow; i < qMin(int(m_hours.size()), m_hourNow + 24); ++i) {
        const double mm = m_hours.at(i).precipitation.value_or(0.0);
        if (mm < 0.1)
            continue;
        if (startsAt < 0)
            startsAt = i;
        peakMm = qMax(peakMm, mm);
    }

    QVariantList series;
    for (int i = m_from; i < qMin(int(m_hours.size()), m_from + kContextBefore + kContextAfter); ++i) {
        const Reading probability = m_hours.at(i).precipitationProbability;
        series.append(probability ? int(std::lround(*probability)) : 0);
    }

    const QString body = startsAt < 0
        ? tr("Nothing falling in the next 24 hours.")
        : (startsAt == m_hourNow
               ? tr("Falling now, easing later.")
               : tr("Dry now. Starts around %1.")
                     .arg(sentenceTime(m_hours.at(startsAt).time, m_zone)));

    m_precipitation = QVariantMap{
        { QStringLiteral("value"), int(std::lround(units->convert(kind, totalMm))) },
        { QStringLiteral("reading"), units->format(kind, totalMm) },
        { QStringLiteral("unit"), units->bareSymbol(kind) },
        { QStringLiteral("window"), tr("In next 24h") },
        // A thoroughly wet day, in the display unit: the ceiling the card draws
        // the amount against. 25 mm, which is detaildata.js's number and the
        // one its comment argues for.
        { QStringLiteral("scaleMax"), units->convert(kind, 25.0) },
        { QStringLiteral("series"), series },
        { QStringLiteral("trend"), startsAt >= 0 ? QStringLiteral("up")
                                                 : QStringLiteral("steady") },
        { QStringLiteral("status"), startsAt < 0 ? tr("Dry")
                                    : peakMm >= 7.6 ? tr("Heavy rain expected")
                                    : peakMm >= 2.5 ? tr("Rain expected")
                                                    : tr("Light rain expected") },
        { QStringLiteral("body"), body },
    };
}

void ConditionsData::buildWind()
{
    const CurrentConditions &now = m_observation;
    const Units             *units = Units::instance();

    const double  kmh   = value(now.windSpeed);
    const int     force = beaufortFor(kmh);
    const double  degrees = value(now.windDirection);

    m_wind = QVariantMap{
        { QStringLiteral("speed"), roundedDisplay(now.windSpeed, Units::Quantity::Wind) },
        { QStringLiteral("reading"), readingOf(now.windSpeed, Units::Quantity::Wind) },
        { QStringLiteral("gustReading"), readingOf(now.windGust, Units::Quantity::Wind) },
        { QStringLiteral("gust"), roundedDisplay(now.windGust, Units::Quantity::Wind) },
        { QStringLiteral("unit"), units->bareSymbol(Units::Quantity::Wind) },
        // Beaufort 5 — a fresh breeze, when loose paper starts blowing about.
        // detaildata.js's ceiling, converted rather than re-chosen because it is
        // a wind speed and not an axis.
        { QStringLiteral("scaleMax"), int(std::lround(units->convert(Units::Quantity::Wind, 30.0))) },
        { QStringLiteral("directionDeg"), qIsNaN(degrees) ? 0 : int(std::lround(degrees)) },
        { QStringLiteral("directionLabel"), compassPoint(degrees) },
        { QStringLiteral("beaufort"), force },
        { QStringLiteral("beaufortName"), beaufortName(force) },
        { QStringLiteral("trend"),
          trendOf(kmh, value(m_hours.value(qMin(m_hourNow + 3, int(m_hours.size()) - 1)).windSpeed),
                  3) },
        { QStringLiteral("status"), beaufortName(force) },
        { QStringLiteral("body"),
          tr("%1 from the %2, gusting to %3 %4.")
              .arg(beaufortName(force))
              .arg(compassPoint(degrees))
              .arg(roundedDisplay(now.windGust, Units::Quantity::Wind))
              .arg(units->bareSymbol(Units::Quantity::Wind)) },
    };
}

void ConditionsData::buildHumidity()
{
    const CurrentConditions &now = m_observation;

    // Eight columns, matching the reference's bar array — a shorter window than
    // the twelve every other card draws, and deliberately: this one is bars, and
    // twelve bars at this width are stripes.
    QVariantList series;
    for (int i = m_from; i < qMin(int(m_hours.size()), m_from + 8); ++i) {
        const Reading reading = m_hours.at(i).relativeHumidity;
        series.append(reading ? int(std::lround(*reading)) : 0);
    }

    const double later =
        value(m_hours.value(qMin(m_hourNow + 3, int(m_hours.size()) - 1)).relativeHumidity);
    const double reading = value(now.relativeHumidity);

    m_humidity = QVariantMap{
        { QStringLiteral("value"), qIsNaN(reading) ? 0 : int(std::lround(reading)) },
        { QStringLiteral("reading"), readingOf(now.relativeHumidity, Units::Quantity::Percentage) },
        { QStringLiteral("dewReading"), readingOf(now.dewPoint, Units::Quantity::Temperature) },
        { QStringLiteral("unit"), QStringLiteral("%") },
        { QStringLiteral("dewPoint"), roundedDisplay(now.dewPoint, Units::Quantity::Temperature) },
        { QStringLiteral("dewUnit"), Units::instance()->symbol(Units::Quantity::Temperature) },
        { QStringLiteral("series"), series },
        { QStringLiteral("trend"), trendOf(reading, later, 3) },
        { QStringLiteral("status"), reading >= 80 ? tr("Humid")
                                    : reading >= 40 ? tr("Normal")
                                                    : tr("Dry") },
        { QStringLiteral("body"), tr("Dew point %1%2.")
                                      .arg(roundedDisplay(now.dewPoint,
                                                          Units::Quantity::Temperature))
                                      .arg(Units::instance()->symbol(
                                          Units::Quantity::Temperature)) },
    };
}

void ConditionsData::buildUv()
{
    const DailyPoint day  = today();
    const double     nowUv = value(m_observation.uvIndex);
    const double     maxUv = value(day.uvIndexMax);

    QDateTime peakAt;
    double    peak = -1;
    for (const HourlyPoint &hour : m_hours) {
        if (hour.time.toTimeZone(m_zone).date() != day.date || !hour.uvIndex)
            continue;
        if (*hour.uvIndex > peak) { peak = *hour.uvIndex; peakAt = hour.time; }
    }

    m_uv = QVariantMap{
        { QStringLiteral("value"), qIsNaN(nowUv) ? 0 : int(std::lround(nowUv)) },
        { QStringLiteral("reading"), qIsNaN(nowUv) ? QStringLiteral("\u2014")
                                                    : QString::number(int(std::lround(nowUv))) },
        // 11 is where the WHO scale stops naming levels and starts saying
        // "extreme", which makes it the right ceiling for a bar.
        { QStringLiteral("max"), 11 },
        { QStringLiteral("band"), uvBand(qIsNaN(nowUv) ? 0 : nowUv) },
        { QStringLiteral("peakAt"), sentenceTime(peakAt, m_zone) },
        { QStringLiteral("trend"), trendOf(nowUv, peak, 0.5) },
        { QStringLiteral("status"), uvBand(qIsNaN(nowUv) ? 0 : nowUv) },
        { QStringLiteral("body"),
          peakAt.isValid()
              ? tr("Today's maximum exposure is %1, expected at %2.")
                    .arg(uvBand(qIsNaN(maxUv) ? peak : maxUv).toLower())
                    .arg(sentenceTime(peakAt, m_zone))
              : tr("No ultraviolet reading for today.") },
    };
}

void ConditionsData::buildAirQuality()
{
    const AirQualityPoint &now = m_air.current;

    const double index = now.europeanAqi ? double(*now.europeanAqi) : qQNaN();
    const auto   worst = now.dominantPollutant();

    // Up, because the *index* is rising — which for air quality is the bad
    // direction. The trend tracks the number; the body says whether that is
    // good news. docs/10-design-system.md §10.5.
    double later = qQNaN();
    for (const AirQualityPoint &point : m_air.hourly) {
        if (point.time > m_now.addSecs(3 * 3600) && point.europeanAqi) {
            later = double(*point.europeanAqi);
            break;
        }
    }

    m_airQuality = QVariantMap{
        { QStringLiteral("value"), qIsNaN(index) ? 0 : int(index) },
        // The number as text, because 0 is a perfectly good European AQI and
        // "no air-quality product here" is not it. Every place that PRINTS the
        // index reads this; the numeric `value` above is for the ramps and the
        // bar, which need something finite to scale against.
        { QStringLiteral("reading"), qIsNaN(index) ? QStringLiteral("—")
                                                   : QString::number(int(index)) },
        { QStringLiteral("max"), 100 },
        { QStringLiteral("band"), qIsNaN(index) ? QString() : aqiBand(index) },
        { QStringLiteral("pollutant"), worst ? pollutantId(*worst).toUpper() : QString() },
        { QStringLiteral("pollutantValue"),
          now.dominantConcentration().value_or(0.0) },
        { QStringLiteral("pollutantUnit"),
          worst ? pollutantUnit(*worst) : QString() },
        { QStringLiteral("trend"), trendOf(index, later, 3) },
        { QStringLiteral("status"), qIsNaN(index) ? tr("No reading") : aqiBand(index) },
        { QStringLiteral("body"),
          worst ? tr("%1 is the main pollutant right now.")
                      .arg(pollutantId(*worst).toUpper())
                : tr("No pollutant breakdown available here.") },
    };
}

void ConditionsData::buildVisibility()
{
    const Units *units = Units::instance();
    const double km    = value(m_observation.visibility);
    const double shown = display(m_observation.visibility, Units::Quantity::Visibility);

    QDateTime clearestAt;
    double    best = -1;
    for (int i = m_hourNow; i < qMin(int(m_hours.size()), m_hourNow + 12); ++i) {
        const Reading reading = m_hours.at(i).visibility;
        if (reading && *reading > best) { best = *reading; clearestAt = m_hours.at(i).time; }
    }

    m_visibility = QVariantMap{
        { QStringLiteral("value"), qIsNaN(shown) ? 0 : int(std::lround(shown)) },
        { QStringLiteral("reading"), readingOf(m_observation.visibility, Units::Quantity::Visibility) },
        { QStringLiteral("unit"), units->bareSymbol(Units::Quantity::Visibility) },
        // As far as a public forecast bothers to distinguish: past 20 km the
        // answer is just "you can see".
        { QStringLiteral("scaleMax"),
          int(std::lround(units->convert(Units::Quantity::Visibility, 20.0))) },
        { QStringLiteral("band"), visibilityBand(km) },
        { QStringLiteral("peak"),
          best < 0 ? 0 : int(std::lround(units->convert(Units::Quantity::Visibility, best))) },
        { QStringLiteral("peakAt"), sentenceTime(clearestAt, m_zone) },
        { QStringLiteral("trend"), trendOf(km, best, 1) },
        { QStringLiteral("status"), visibilityBand(km) },
        { QStringLiteral("body"),
          clearestAt.isValid() ? tr("Clearest around %1.").arg(sentenceTime(clearestAt, m_zone))
                               : tr("No visibility reading for the hours ahead.") },
    };
}

void ConditionsData::buildPressure()
{
    const Units *units = Units::instance();
    const double hpa   = value(m_observation.pressureMsl);
    const double shown = display(m_observation.pressureMsl, Units::Quantity::Pressure);

    const double before =
        value(m_hours.value(qMax(0, m_hourNow - 3)).pressureMsl);
    const double after =
        value(m_hours.value(qMin(m_hourNow + 3, int(m_hours.size()) - 1)).pressureMsl);

    const QString trend = trendOf(before, hpa, 0.7);
    const int     decimals = units->decimals(Units::Quantity::Pressure);

    m_pressure = QVariantMap{
        { QStringLiteral("value"), qIsNaN(shown) ? QString()
                                                 : QString::number(shown, 'f', decimals) },
        { QStringLiteral("reading"), readingOf(m_observation.pressureMsl, Units::Quantity::Pressure) },
        { QStringLiteral("unit"), units->bareSymbol(Units::Quantity::Pressure) },
        { QStringLiteral("at"), tr("%1 (Now)").arg(m_observedAt) },
        { QStringLiteral("series"), window(&HourlyPoint::pressureMsl,
                                           int(Units::Quantity::Pressure)) },
        { QStringLiteral("min"),
          int(std::lround(units->convert(Units::Quantity::Pressure, 995.0))) },
        { QStringLiteral("max"),
          int(std::lround(units->convert(Units::Quantity::Pressure, 1030.0))) },
        { QStringLiteral("trend"), trend },
        { QStringLiteral("status"), trend == QLatin1String("up")     ? tr("Rising slowly")
                                    : trend == QLatin1String("down") ? tr("Falling slowly")
                                                                     : tr("Steady") },
        { QStringLiteral("body"), after > hpa ? tr("Expected to rise over the next 3 hours.")
                                  : after < hpa ? tr("Expected to fall over the next 3 hours.")
                                                : tr("Expected to hold steady.") },
    };
}

// ---- the sun and the moon -----------------------------------------------------------

void ConditionsData::buildSunMoon()
{
    const DailyPoint day       = today();
    const QDate      reference = m_now.toTimeZone(m_zone).date();

    // Minutes from local midnight, measured from ONE reference date — not from
    // each instant's own midnight. libclima/domain/timeaxis.h exists for this:
    // above the Arctic circle in summer, sunrise is that day's midnight and
    // sunset is the *next* day's, and an arc measured from two midnights comes
    // out zero minutes long.
    const int riseMin = minutesFromLocalMidnight(day.sunrise, m_zone, reference);
    const int setMin  = minutesFromLocalMidnight(day.sunset, m_zone, reference);
    const int nowMin  = minutesFromLocalMidnight(m_now, m_zone, reference);

    const Reading daylight = day.daylightSeconds;
    const int     dayHours = daylight ? int(*daylight) / 3600 : 0;
    const int     dayMins  = daylight ? (int(*daylight) % 3600) / 60 : 0;

    // "8:42", not "20:42". QLocale's "h" is a 24-hour hour unless the format
    // also carries AP, and the suffix is a separate field here — so asking for
    // "h:mm" and appending "PM" produced "20:42 PM" on every sunset after noon.
    // Computed rather than formatted, because the arithmetic is two lines and
    // the format string that gets this right is a thing to be remembered.
    const auto hhmm = [&](const QDateTime &instant) {
        if (!instant.isValid())
            return QString();
        const QTime time = instant.toTimeZone(m_zone).time();
        const int   hour = time.hour() % 12 == 0 ? 12 : time.hour() % 12;
        return QStringLiteral("%1:%2").arg(hour).arg(time.minute(), 2, 10, QLatin1Char('0'));
    };
    const auto suffix = [&](const QDateTime &instant) {
        return instant.isValid()
                   ? (instant.toTimeZone(m_zone).time().hour() < 12 ? tr("AM") : tr("PM"))
                   : QString();
    };

    m_sun = QVariantMap{
        { QStringLiteral("riseMin"), riseMin },
        { QStringLiteral("setMin"), setMin },
        { QStringLiteral("nowMin"), nowMin },
        { QStringLiteral("riseLabel"), hhmm(day.sunrise) },
        { QStringLiteral("riseSuffix"), suffix(day.sunrise) },
        { QStringLiteral("setLabel"), hhmm(day.sunset) },
        { QStringLiteral("setSuffix"), suffix(day.sunset) },
        { QStringLiteral("dayLength"), tr("%1 hrs %2 mins").arg(dayHours).arg(dayMins) },
        { QStringLiteral("trend"), QStringLiteral("none") },
        { QStringLiteral("status"), (nowMin >= riseMin && nowMin < setMin) ? tr("Daylight")
                                                                          : tr("Night") },
        { QStringLiteral("body"),
          day.sunset.isValid()
              ? tr("The sun is up for %1 hours and %2 minutes today, setting at %3.")
                    .arg(dayHours).arg(dayMins).arg(sentenceTime(day.sunset, m_zone))
              : tr("The sun does not set here today.") },
    };

    const int moonRise = minutesFromLocalMidnight(day.moonrise, m_zone, reference);
    const int moonSet  = minutesFromLocalMidnight(day.moonset, m_zone, reference);
    const Reading lit  = moonIllumination(day.moonPhase);

    // The moon fails to rise on about one calendar day a month — its rising
    // drifts fifty minutes later each day and eventually skips a midnight — so
    // an absent moonrise is ordinary and the card must not read as broken.
    const int upMinutes = (day.moonrise.isValid() && day.moonset.isValid())
        ? qAbs(moonSet - moonRise)
        : 0;

    m_moon = QVariantMap{
        { QStringLiteral("riseMin"), moonRise },
        { QStringLiteral("setMin"), moonSet },
        { QStringLiteral("nowMin"), nowMin },
        { QStringLiteral("riseLabel"), hhmm(day.moonrise) },
        { QStringLiteral("riseSuffix"), suffix(day.moonrise) },
        { QStringLiteral("setLabel"), hhmm(day.moonset) },
        { QStringLiteral("setSuffix"), suffix(day.moonset) },
        { QStringLiteral("upLength"),
          tr("%1 hrs %2 mins").arg(upMinutes / 60).arg(upMinutes % 60) },
        { QStringLiteral("phase"), ForecastData::moonPhaseLabel(moonPhaseName(day.moonPhase)) },
        { QStringLiteral("illumination"), lit.value_or(0.0) },
        { QStringLiteral("trend"), QStringLiteral("none") },
        { QStringLiteral("status"), ForecastData::moonPhaseLabel(moonPhaseName(day.moonPhase)) },
        { QStringLiteral("body"),
          day.moonrise.isValid()
              ? tr("The moon is %1% illuminated and rises at %2 tonight.")
                    .arg(int(std::lround(lit.value_or(0.0) * 100)))
                    .arg(sentenceTime(day.moonrise, m_zone))
              : tr("The moon is %1% illuminated. It does not rise today.")
                    .arg(int(std::lround(lit.value_or(0.0) * 100))) },
    };
}

// ---- pollen: gated, not computed ------------------------------------------------------

void ConditionsData::buildPollen(bool hasPollen)
{
    m_hasPollen = hasPollen;
    m_pollen[QStringLiteral("available")] = hasPollen;
    if (!hasPollen || m_air.hourly.isEmpty())
        return;

    // The hour we are in, from the air-quality series' own axis.
    const AirQualityPoint *point = nullptr;
    for (const AirQualityPoint &candidate : m_air.hourly) {
        if (candidate.time <= m_now)
            point = &candidate;
        else
            break;
    }
    if (point == nullptr || !point->pollen)
        return;

    // Grains per cubic metre, banded the way every European pollen service
    // bands them. The three shown are the three a European reader recognises;
    // alder, birch and olive are seasonal and out of season most of the year,
    // and a card with six rows of "Low" is a card nobody reads.
    struct Row { PollenSpecies species; const char *label; double moderate; double high; };
    static const Row rows[] = {
        { PollenSpecies::Grass,   QT_TR_NOOP("Grass"),   20, 50 },
        { PollenSpecies::Birch,   QT_TR_NOOP("Tree"),    20, 50 },
        { PollenSpecies::Ragweed, QT_TR_NOOP("Weed"),    10, 20 },
    };

    QVariantList items;
    double       worstLevel = 0;
    QString      worstName;
    QString      worstLabel = tr("Low");
    QString      worstTone  = QStringLiteral("good");

    for (const Row &row : rows) {
        const auto found = point->pollen->constFind(row.species);
        if (found == point->pollen->cend())
            continue;

        const double grains = *found;
        const bool   high   = grains >= row.high;
        const bool   medium = grains >= row.moderate;

        const QString label = high ? tr("High") : medium ? tr("Moderate") : tr("Low");
        const QString tone  = toneFor(!medium, medium && !high);
        const double  level = qBound(0.0, grains / (row.high * 2.0), 1.0);

        items.append(QVariantMap{ { QStringLiteral("name"), tr(row.label) },
                                  { QStringLiteral("label"), label },
                                  { QStringLiteral("tone"), tone },
                                  { QStringLiteral("level"), level } });

        if (level > worstLevel) {
            worstLevel = level;
            worstName  = tr(row.label);
            worstLabel = label;
            worstTone  = tone;
        }
    }

    if (items.isEmpty())
        return;

    m_pollen[QStringLiteral("band")]  = worstLabel;
    m_pollen[QStringLiteral("tone")]  = worstTone;
    m_pollen[QStringLiteral("main")]  = worstName;
    m_pollen[QStringLiteral("items")] = items;
    m_pollen[QStringLiteral("body")]  =
        worstTone == QLatin1String("good")
            ? tr("Pollen is low across the board today.")
            : tr("%1 pollen is the main allergen today. %2 risk for sensitive individuals.")
                  .arg(worstName, worstLabel);
}

// ---- activities: ours, and labelled as ours -------------------------------------------

void ConditionsData::buildActivities()
{
    // NOTHING PUBLISHES THIS. There is no "do I need an umbrella" product at
    // Open-Meteo, at MET Norway or anywhere else, so every row below is a rule
    // written here, applied to numbers that are on the same screen. The card
    // says so in its own subtitle; see MobileActivitiesCard.qml.
    //
    // Each rule is one line and each is deliberately dull. A verdict a reader
    // cannot reconstruct from what they can see is a verdict they cannot
    // disagree with, which is worse than a crude one.

    const CurrentConditions &now = m_observation;

    // ---- umbrella: the highest probability in the next twelve hours ---------
    double wettest = 0;
    for (int i = m_hourNow; i < qMin(int(m_hours.size()), m_hourNow + 12); ++i)
        wettest = qMax(wettest, m_hours.at(i).precipitationProbability.value_or(0.0));

    const QString umbrella = wettest >= 60 ? tr("Take one")
                             : wettest >= 30 ? tr("Maybe")
                                             : tr("No need");

    // ---- UV: the WHO band, as it stands now ---------------------------------
    const double uvNow = value(now.uvIndex);

    // ---- heat: apparent temperature, which is the one that matters ----------
    const double feels = value(now.apparentTemperature);
    const QString heat = feels >= 32 ? tr("Take care")
                         : feels >= 27 ? tr("Caution")
                         : feels <= -10 ? tr("Take care")
                         : feels <= 0   ? tr("Caution")
                                        : tr("Comfortable");

    // ---- clothing: apparent temperature again, in four bands ----------------
    const QString clothing = feels >= 24 ? tr("Light wear")
                             : feels >= 15 ? tr("A layer")
                             : feels >= 5  ? tr("A jacket")
                                           : tr("Wrap up");

    // ---- outdoors: the composite, and the only row that reads the others ----
    const bool wet   = wettest >= 50;
    const bool burnt = uvNow >= 8;
    const bool harsh = feels >= 32 || feels <= 0;
    const QString outdoors = (wet || burnt || harsh) ? (wet && harsh ? tr("Poor") : tr("Fair"))
                                                     : tr("Good");

    m_activities = {
        activity(tr("Outdoors"), outdoors,
                 toneFor(outdoors == tr("Good"), outdoors == tr("Fair"))),
        activity(tr("Clothing"), clothing,
                 toneFor(feels >= 15 && feels < 27, true)),
        activity(tr("Heat"), heat,
                 toneFor(heat == tr("Comfortable"), heat == tr("Caution"))),
        activity(tr("Umbrella"), umbrella,
                 toneFor(wettest < 30, wettest < 60)),
        activity(tr("UV index"), qIsNaN(uvNow) ? tr("No reading") : uvBand(uvNow),
                 toneFor(uvNow < 6, uvNow < 8)),
    };
}

// ---- the headline sentence -------------------------------------------------------------

void ConditionsData::buildSummary()
{
    const CurrentConditions &now = m_observation;
    const DailyPoint         day = today();
    const Units             *units = Units::instance();

    const bool    daylight  = now.isDay.value_or(true);
    const QString condition = now.weatherCode ? clima::conditionText(*now.weatherCode, daylight)
                                              : QString();

    // The next run of precipitation, if there is one in the next twelve hours,
    // and how hard it gets. Both come out of the same series the chart draws,
    // which is what stops the sentence and the wash under it disagreeing about
    // whether it is going to rain.
    int    startsAt = -1;
    double peakMm   = 0;
    for (int i = m_hourNow + 1; i < qMin(int(m_hours.size()), m_hourNow + 13); ++i) {
        const double mm = m_hours.at(i).precipitation.value_or(0.0);
        if (mm < 0.1)
            continue;
        if (startsAt < 0)
            startsAt = i;
        peakMm = qMax(peakMm, mm);
    }

    // Assembled as whole sentences and joined, rather than concatenated with
    // punctuation glued on. "9:00 a.m." already ends in a full stop, so a
    // template that added one produced "from 9:00 a.m.." — the sort of thing
    // that is invisible while the sentence is a literal in a mock file and
    // unavoidable the moment it is generated.
    QStringList sentences;

    if (!condition.isEmpty())
        sentences.append(tr("%1 now.").arg(condition));

    if (startsAt >= 0) {
        // The NWS bands precip.js classifies with, said in words. Capitalised
        // here because it opens its own sentence — which it does not do in
        // every language, and is why the whole clause is one translatable unit
        // rather than a noun slotted into a frame.
        const QString weight = peakMm >= 7.6 ? tr("Heavy rain")
                               : peakMm >= 2.5 ? tr("Rain")
                                               : tr("Light rain");
        sentences.append(
            tr("%1 from %2").arg(weight, sentenceTime(m_hours.at(startsAt).time, m_zone)));
    }

    if (day.temperatureMax) {
        sentences.append(tr("The high will be %1%2.")
                             .arg(roundedDisplay(day.temperatureMax, Units::Quantity::Temperature))
                             .arg(units->symbol(Units::Quantity::Temperature)));
    }

    m_current = QVariantMap{
        { QStringLiteral("conditionKind"),
          now.weatherCode
              ? conditionKindName(drawableToday(clima::conditionFor(*now.weatherCode, daylight)))
              : QString() },
        { QStringLiteral("unitLabel"), units->bareSymbol(Units::Quantity::Temperature) },
        { QStringLiteral("summary"), sentences.join(QLatin1Char(' ')) },
    };
}

// ---- the two constants ------------------------------------------------------------------

QVariantList ConditionsData::order() const
{
    return { QStringLiteral("temperature"), QStringLiteral("feelsLike"),
             QStringLiteral("cloudCover"),  QStringLiteral("precipitation"),
             QStringLiteral("wind"),        QStringLiteral("humidity"),
             QStringLiteral("uv"),          QStringLiteral("airQuality"),
             QStringLiteral("visibility"),  QStringLiteral("pressure"),
             QStringLiteral("sun"),         QStringLiteral("moon") };
}

QVariantMap ConditionsData::bands() const
{
    // Colour bands published by the relevant authority, normalised over each
    // card's own range. detaildata.js's table, moved rather than changed.
    return QVariantMap{
        { QStringLiteral("uv"),
          QVariantList{ stop(0.00, QStringLiteral("#5ec18a")), stop(0.27, QStringLiteral("#e8c73c")),
                        stop(0.50, QStringLiteral("#f5a02f")), stop(0.72, QStringLiteral("#d6484e")),
                        stop(1.00, QStringLiteral("#8b5fc4")) } },
        { QStringLiteral("aqi"),
          QVariantList{ stop(0.00, QStringLiteral("#4ec3c8")), stop(0.25, QStringLiteral("#5cc79a")),
                        stop(0.50, QStringLiteral("#e8c93f")), stop(0.75, QStringLiteral("#f08a45")),
                        stop(1.00, QStringLiteral("#c03050")) } },
        { QStringLiteral("visibility"),
          QVariantList{ stop(0.00, QStringLiteral("#5f8f78")), stop(0.50, QStringLiteral("#6fbf95")),
                        stop(1.00, QStringLiteral("#8fe0b4")) } },
    };
}
