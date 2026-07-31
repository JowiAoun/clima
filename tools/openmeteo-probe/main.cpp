// SPDX-FileCopyrightText: 2026 Jowi Aoun
// SPDX-License-Identifier: GPL-3.0-or-later
//
// Fetch a real forecast and print what the adapter made of it.
//
// GPL-3.0-or-later and not MPL, despite living next to code that is: this is a
// development tool, not part of the reusable engine. It links libclima the way
// any consumer would.
//
// ---- why a tool and not another test -----------------------------------------
//
// Because the tests deliberately cannot answer the question this answers.
// Nothing in tests/ may reach a network (docs/04-architecture.md §4.11), so the
// suite proves the adapter is consistent with eight recorded responses — and
// would go on proving it after Open-Meteo renamed a field, changed a unit, or
// started sending a WMO code nothing here has a phrase for. Recorded fixtures
// are a guard against regression in *us*; they are not a guard against drift in
// *them*.
//
// So this prints the mapping in a form you can hold against the service's own
// documentation, or against api.open-meteo.com's response in a browser, and
// check by eye. That is a different kind of confidence and there is no
// automated substitute for it that does not involve a network in CI.
//
//     nix develop --command cmake --build build/dev --target clima-openmeteo-probe
//     ./build/dev/tools/openmeteo-probe/clima-openmeteo-probe
//     ./build/dev/tools/openmeteo-probe/clima-openmeteo-probe --lat 60.39 --lon 5.32 --hours 12
//
// It goes out over the real HttpClient, which means it also exercises the real
// User-Agent, the real coordinate rounding and the real conditional-GET path.
// If that is refused, this is where you find out, and the 403 arrives here
// rather than in front of a user.

#include "libclima/core/clock.h"
#include "libclima/domain/hourconvention.h"
#include "libclima/domain/timeaxis.h"
#include "libclima/domain/weathercode.h"
#include "libclima/net/httpclient.h"
#include "libclima/providers/openmeteo/openmeteoadapter.h"
#include "libclima/providers/openmeteo/openmeteoforecastprovider.h"

#include <QCommandLineParser>
#include <QCoreApplication>
#include <QFutureWatcher>
#include <QTextStream>
#include <QTimeZone>

using namespace clima;

namespace {

QTextStream &out()
{
    static QTextStream stream(stdout);
    return stream;
}

// A Reading, or a dash. The same choice app/qml/Clima/metrics.js's format()
// makes for a value it does not have, so that a column of dashes here looks
// like the column of dashes the UI would draw.
QString show(Reading value, int decimals = 1)
{
    return value ? QString::number(*value, 'f', decimals) : QStringLiteral("—");
}

QString showCode(WeatherCode code)
{
    if (!code)
        return QStringLiteral("—");
    return QStringLiteral("%1 %2").arg(*code, 3).arg(conditionText(*code, true));
}

QString clock(int minutes)
{
    if (!hasMinuteOfDay(minutes))
        return QStringLiteral("—");
    // Not clamped: a moon that rose last night is genuinely negative and a
    // polar sunset is genuinely 1440. Printing the raw number is the point.
    const int hh = minutes / 60;
    const int mm = qAbs(minutes % 60);
    return QStringLiteral("%1:%2").arg(hh, 2).arg(mm, 2, 10, QLatin1Char('0'));
}

void printForecast(const Forecast &forecast, int hours)
{
    const QTimeZone zone = forecast.timeZone;

    out() << "\n=== place =====================================================\n";
    out() << "  provider     " << forecast.providerId << "\n";
    out() << "  grid cell    " << forecast.coordinate.toKeyString() << "\n";
    out() << "  elevation    " << show(forecast.elevation, 0) << " m\n";
    out() << "  zone         " << QString::fromUtf8(zone.id()) << "\n";
    out() << "  fetched      " << forecast.fetchedAt.toString(Qt::ISODate) << "\n";

    out() << "\n=== current ===================================================\n";
    const CurrentConditions &now = forecast.current;
    out() << "  observed     " << now.time.toString(Qt::ISODate) << "  (local "
          << now.time.toTimeZone(zone).toString(QStringLiteral("HH:mm")) << ")\n";
    out() << "  temperature  " << show(now.temperature) << " °C   feels "
          << show(now.apparentTemperature) << " °C   dew " << show(now.dewPoint) << " °C\n";
    out() << "  humidity     " << show(now.relativeHumidity, 0) << " %    cloud "
          << show(now.cloudCover, 0) << " %\n";
    out() << "  wind         " << show(now.windSpeed) << " km/h  gust "
          << show(now.windGust) << " km/h  from " << show(now.windDirection, 0) << "°\n";
    out() << "  pressure     " << show(now.pressureMsl, 0) << " hPa\n";
    // The two the traps are about. Visibility must be a small number of
    // kilometres and not a five-digit count of metres.
    out() << "  visibility   " << show(now.visibility) << " km\n";
    out() << "  uv           " << show(now.uvIndex) << "\n";
    out() << "  condition    " << showCode(now.weatherCode) << "\n";
    out() << "  daylight     " << (now.isDay ? (*now.isDay ? "day" : "night") : "—") << "\n";

    // The hourly series as a chart sees it: accumulations moved onto the hour
    // they fall in. Printing the raw series instead would hide the one
    // transformation most worth eyeballing.
    const QList<HourlyPoint> chart = asHourStarting(forecast.hourly);

    out() << "\n=== hourly (as charted: amounts are for the hour STARTING at the label) ===\n";
    out() << "  local  temp  feel   rh  cloud  wind  gust   dir  press    uv   vis   "
             "pop    mm  code\n";

    const int count = qMin(hours, int(chart.size()));
    for (int i = 0; i < count; ++i) {
        const HourlyPoint &point = chart.at(i);
        out() << QStringLiteral("  %1")
                     .arg(point.time.toTimeZone(zone).toString(QStringLiteral("ddd HH:mm")), -10)
              << QStringLiteral("%1").arg(show(point.temperature), 5)
              << QStringLiteral("%1").arg(show(point.apparentTemperature), 6)
              << QStringLiteral("%1").arg(show(point.relativeHumidity, 0), 5)
              << QStringLiteral("%1").arg(show(point.cloudCover, 0), 7)
              << QStringLiteral("%1").arg(show(point.windSpeed), 6)
              << QStringLiteral("%1").arg(show(point.windGust), 6)
              << QStringLiteral("%1").arg(show(point.windDirection, 0), 6)
              << QStringLiteral("%1").arg(show(point.pressureMsl, 0), 7)
              << QStringLiteral("%1").arg(show(point.uvIndex), 6)
              << QStringLiteral("%1").arg(show(point.visibility), 6)
              << QStringLiteral("%1").arg(show(point.precipitationProbability, 0), 6)
              << QStringLiteral("%1").arg(show(point.precipitation, 2), 6)
              << QStringLiteral("  ") << showCode(point.weatherCode) << "\n";
    }

    out() << "\n=== daily =====================================================\n";
    out() << "  date        high   low    mm  pop   uv  sunrise  sunset  daylight  "
             "moonrise  moonset  phase  lit\n";

    for (const DailyPoint &day : forecast.daily) {
        const Reading illumination = moonIllumination(day.moonPhase);
        out() << QStringLiteral("  %1").arg(day.date.toString(Qt::ISODate), -12)
              << QStringLiteral("%1").arg(show(day.temperatureMax), 5)
              << QStringLiteral("%1").arg(show(day.temperatureMin), 6)
              << QStringLiteral("%1").arg(show(day.precipitationSum), 6)
              << QStringLiteral("%1").arg(show(day.precipitationProbabilityMax, 0), 5)
              << QStringLiteral("%1").arg(show(day.uvIndexMax, 0), 5)
              // Minutes past local midnight, the units detaildata.js places
              // these on an arc with. Above the Arctic circle a sunset of
              // 24:00 is the correct answer and not a bug.
              << QStringLiteral("%1").arg(
                     clock(minutesFromLocalMidnight(day.sunrise, zone, day.date)), 9)
              << QStringLiteral("%1").arg(
                     clock(minutesFromLocalMidnight(day.sunset, zone, day.date)), 8)
              << QStringLiteral("%1").arg(
                     day.daylightSeconds
                         ? QStringLiteral("%1h%2")
                               .arg(int(*day.daylightSeconds) / 3600)
                               .arg((int(*day.daylightSeconds) % 3600) / 60, 2, 10,
                                    QLatin1Char('0'))
                         : QStringLiteral("—"),
                     10)
              << QStringLiteral("%1").arg(
                     clock(minutesFromLocalMidnight(day.moonrise, zone, day.date)), 10)
              << QStringLiteral("%1").arg(
                     clock(minutesFromLocalMidnight(day.moonset, zone, day.date)), 9)
              << QStringLiteral("%1").arg(show(day.moonPhase, 3), 7)
              << QStringLiteral("%1").arg(
                     illumination ? QStringLiteral("%1%").arg(int(*illumination * 100 + 0.5))
                                  : QStringLiteral("—"),
                     5)
              << QStringLiteral("  ") << moonPhaseName(day.moonPhase) << "\n";
    }

    // The local day lengths, which is where a DST window shows itself. Every
    // day is 24 hours except the two a year that are not, and Open-Meteo's own
    // labels never say so — see libclima/domain/timeaxis.h.
    out() << "\n=== local day lengths (25 or 23 means a DST transition) =========\n  ";
    QList<QDateTime> instants;
    for (const HourlyPoint &point : forecast.hourly)
        instants.append(point.time);
    for (const DailyPoint &day : forecast.daily)
        out() << day.date.toString(QStringLiteral("MM-dd")) << ":"
              << indicesOnLocalDate(instants, zone, day.date).size() << "  ";
    out() << "\n";

    out() << "\n=== attribution ================================================\n";
    out() << "  " << openmeteo::attribution().creditLine << "\n";
    out() << "  " << openmeteo::attribution().licenceName << " · "
          << openmeteo::attribution().licenceUrl.toString() << "\n";
    out() << "  models: " << openmeteo::attribution().upstream.join(QStringLiteral(", "))
          << "\n\n";
    out().flush();
}

} // namespace

int main(int argc, char *argv[])
{
    QCoreApplication app(argc, argv);
    QCoreApplication::setApplicationName(QStringLiteral("clima-openmeteo-probe"));

    QCommandLineParser parser;
    parser.setApplicationDescription(
        QStringLiteral("Fetch one Open-Meteo forecast and print the adapted values."));
    parser.addHelpOption();

    QCommandLineOption latitude(QStringLiteral("lat"), QStringLiteral("Latitude"),
                                QStringLiteral("degrees"), QStringLiteral("43.6532"));
    QCommandLineOption longitude(QStringLiteral("lon"), QStringLiteral("Longitude"),
                                 QStringLiteral("degrees"), QStringLiteral("-79.3832"));
    QCommandLineOption hours(QStringLiteral("hours"),
                             QStringLiteral("How many hourly rows to print"),
                             QStringLiteral("n"), QStringLiteral("24"));
    QCommandLineOption days(QStringLiteral("days"), QStringLiteral("Forecast days, 1 to 16"),
                            QStringLiteral("n"), QStringLiteral("16"));
    QCommandLineOption models(QStringLiteral("models"),
                              QStringLiteral("Comma-separated model ids, e.g. ecmwf_ifs025"),
                              QStringLiteral("list"), QString());

    parser.addOption(latitude);
    parser.addOption(longitude);
    parser.addOption(hours);
    parser.addOption(days);
    parser.addOption(models);
    parser.process(app);

    SystemClock clock;
    HttpClient  http(&clock);

    OpenMeteoForecastProvider provider(&http, &clock);

    ForecastRequest request;
    request.coord.latitude  = parser.value(latitude).toDouble();
    request.coord.longitude = parser.value(longitude).toDouble();
    request.days            = parser.value(days).toInt();
    if (!parser.value(models).isEmpty())
        request.models = parser.value(models).split(QLatin1Char(','));

    out() << "GET " << composeUrl(provider.buildRequest(request)).toString() << "\n";
    out() << "UA  " << QString::fromUtf8(HttpClient::userAgent()) << "\n";
    out().flush();

    int exitCode = 0;

    QFutureWatcher<Result<Forecast>> watcher;
    QObject::connect(&watcher, &QFutureWatcherBase::finished, &app,
                     [&watcher, &app, &exitCode, &parser, &hours]() {
                         const Result<Forecast> result = watcher.result();
                         if (!result) {
                             out() << "\nFAILED: " << result.error().toString() << "\n";
                             out().flush();
                             exitCode = 1;
                         } else {
                             printForecast(result.value(), parser.value(hours).toInt());
                         }
                         app.quit();
                     });

    watcher.setFuture(provider.fetchForecast(request));

    app.exec();
    return exitCode;
}
