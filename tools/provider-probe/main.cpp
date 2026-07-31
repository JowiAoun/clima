// SPDX-FileCopyrightText: 2026 Jowi Aoun
// SPDX-License-Identifier: GPL-3.0-or-later
//
// What the adapters make of a recorded payload, printed.
//
//     nix develop --command cmake --build --preset dev --target clima-provider-probe
//     ./build/dev/tools/provider-probe/clima-provider-probe
//
// A test asserts. This prints, which is a different job: the assertions in
// tests/tst_airquality.cpp say the pollen gate is closed in Toronto and open in
// Berlin, and this puts the two side by side so a human can *see* that one card
// is missing rather than empty. docs/08-risks.md R9 — "region-gate honestly,
// never fabricate" — is the kind of rule that is easy to satisfy in an assertion
// and still get visibly wrong on a screen, and this is the cheapest place to
// look before there is a screen.
//
// No network, ever. It reads the same fixtures the tests read, through the same
// static parse() functions, so what it prints is what the app would hold.
// CLIMA_SOURCE_DIR is baked in at configure time for the same reason the tests
// take it: a path computed from the binary's own location breaks the day the
// build directory moves.
//
// It is a dev tool. CLIMA_DEV_TOOLS=OFF and this directory is never entered.

#include "libclima/domain/airquality.h"
#include "libclima/domain/forecast.h"
#include "libclima/providers/airquality/openmeteoairqualityprovider.h"
#include "libclima/providers/metno/metnoforecastprovider.h"
#include "libclima/providers/metno/symbolcode.h"

#include <QCoreApplication>
#include <QDate>
#include <QDateTime>
#include <QFile>
#include <QTextStream>
#include <QTimeZone>

using namespace clima;

namespace {

QTextStream &out()
{
    static QTextStream stream(stdout);
    return stream;
}

QByteArray fixture(const QString &relative)
{
    QFile file(QStringLiteral(CLIMA_SOURCE_DIR) + QStringLiteral("/tests/fixtures/") + relative);
    if (!file.open(QIODevice::ReadOnly)) {
        out() << "cannot read " << file.fileName() << "\n";
        return {};
    }
    return file.readAll();
}

QString show(const Reading &reading, int decimals = 1, const QString &unit = {})
{
    // The whole point of the domain being optional everywhere, made visible: an
    // absent reading prints as an em dash, never as 0. If this tool ever prints
    // "0.0 km/h" for MET Norway's wind gust, something has started fabricating.
    if (!reading.has_value())
        return QStringLiteral("—");
    return QString::number(*reading, 'f', decimals) + unit;
}

QString localTime(const QDateTime &instant, const QTimeZone &zone)
{
    if (!instant.isValid())
        return QStringLiteral("—");
    return instant.toTimeZone(zone).toString(QStringLiteral("ddd HH:mm"));
}

void rule(const QString &title)
{
    out() << "\n" << title << "\n" << QString(78, QLatin1Char('=')) << "\n";
}

// ---- air quality -----------------------------------------------------------------

void probeAirQuality(const QString &place, const QString &file)
{
    const Result<AirQuality> parsed =
        OpenMeteoAirQualityProvider::parse(fixture(QStringLiteral("airquality/") + file),
                                           QDateTime(QDate(2026, 7, 31), QTime(9, 13),
                                                     QTimeZone::UTC));
    if (!parsed.hasValue()) {
        out() << place << ": " << parsed.error().toString() << "\n";
        return;
    }

    const AirQuality &air = parsed.value();
    const AirQualityPoint &now = air.current;

    rule(place);
    out() << "  coordinate      " << air.coordinate.toKeyString() << "   zone "
          << QString::fromUtf8(air.timeZone.id()) << "\n";
    out() << "  hours           " << air.hourly.size() << "\n";
    out() << "  European AQI    "
          << (now.europeanAqi ? QString::number(*now.europeanAqi) : QStringLiteral("—"))
          << "        US AQI  "
          << (now.usAqi ? QString::number(*now.usAqi) : QStringLiteral("—")) << "\n";

    if (const std::optional<Pollutant> dominant = now.dominantPollutant()) {
        out() << "  dominant        " << pollutantId(*dominant) << "  "
              << show(now.dominantConcentration(), 1) << " " << pollutantUnit(*dominant)
              << "   (sub-index " << show(now.dominantSubIndex(), 1) << ")\n";
    }

    out() << "\n  pollutants\n";
    for (int i = 0; i < int(Pollutant::Count); ++i) {
        const auto pollutant = static_cast<Pollutant>(i);
        const auto value     = now.pollutants.constFind(pollutant);
        out() << "    " << pollutantId(pollutant).leftJustified(18)
              << (value == now.pollutants.cend() ? QStringLiteral("—")
                                                 : QString::number(*value, 'f', 1))
              << "  " << pollutantUnit(pollutant) << "\n";
    }

    // THE GATE, printed. Toronto must show the card as absent — not as six
    // zeroes — and Berlin must show real numbers, four of which are legitimately
    // 0.0 because it is July and those species are out of season.
    out() << "\n  pollen          ";
    if (!air.hasPollen) {
        out() << "NOT AVAILABLE HERE — the card is hidden, not empty\n";
        out() << "                  (every species was null at every hour; CAMS produces "
                 "pollen for\n"
                 "                   its European domain only)\n";
    } else {
        out() << "available\n";
        const std::optional<QMap<PollenSpecies, double>> &pollen = air.hourly.constFirst().pollen;
        for (int i = 0; i < int(PollenSpecies::Count); ++i) {
            const auto species = static_cast<PollenSpecies>(i);
            const auto value   = pollen ? pollen->constFind(species) : QMap<PollenSpecies,
                                                                           double>::const_iterator();
            out() << "    " << pollenSpeciesId(species).leftJustified(18)
                  << (!pollen || value == pollen->cend() ? QStringLiteral("—")
                                                         : QString::number(*value, 'f', 1))
                  << "  grains/m³\n";
        }
    }

    // From the hourly series, not from `current`: the `current` block is asked
    // for the two indices and the six pollutants only, so its ammonia would
    // print as an absence for the wrong reason and make the gate look broken in
    // Berlin.
    out() << "\n  ammonia         "
          << (air.hasAmmonia ? show(air.hourly.constFirst().ammonia, 1, QStringLiteral(" µg/m³"))
                             : QStringLiteral("NOT AVAILABLE HERE"))
          << "\n";
}

// ---- MET Norway ---------------------------------------------------------------------

void probeMetNorway()
{
    const QTimeZone zone("America/Toronto");
    const Result<Forecast> parsed =
        MetNoForecastProvider::parse(fixture(QStringLiteral("metno/toronto.json")), zone,
                                     QDateTime(QDate(2026, 7, 31), QTime(9, 13), QTimeZone::UTC));
    if (!parsed.hasValue()) {
        out() << "MET Norway: " << parsed.error().toString() << "\n";
        return;
    }

    const Forecast &forecast = parsed.value();

    rule(QStringLiteral("MET Norway Locationforecast 2.0 (compact) — the fallback"));
    out() << "  coordinate      " << forecast.coordinate.toKeyString() << "   elevation "
          << show(forecast.elevation, 0, QStringLiteral(" m")) << "\n";
    out() << "  issued          " << forecast.issuedAt.toString(Qt::ISODate) << "\n";
    out() << "  grouped by      " << QString::fromUtf8(forecast.timeZone.id())
          << "   (MET reports no zone of its own; the caller supplied this)\n";
    out() << "  hours           " << forecast.hourly.size() << "   days "
          << forecast.daily.size() << "\n";

    out() << "\n  the first eight hours — note that the first has no precipitation and no\n"
             "  weather code, because those would describe the hour BEFORE the forecast\n"
             "  starts, which is the past.\n\n";
    out() << "    time        temp    wind    gust    precip  code  humidity  UV\n";
    for (int i = 0; i < qMin(8, forecast.hourly.size()); ++i) {
        const HourlyPoint &point = forecast.hourly.at(i);
        out() << "    " << localTime(point.time, zone).leftJustified(12)
              << show(point.temperature, 1, QStringLiteral("°")).leftJustified(8)
              << show(point.windSpeed, 1).leftJustified(8)
              << show(point.windGust, 1).leftJustified(8)
              << show(point.precipitation, 1).leftJustified(8)
              << (point.weatherCode ? QString::number(*point.weatherCode)
                                    : QStringLiteral("—")).leftJustified(6)
              << show(point.relativeHumidity, 0).leftJustified(10)
              << show(point.uvIndex, 1) << "\n";
    }

    out() << "\n  the six-hourly tail — the series thins after about two and a half days,\n"
             "  which is why HourlyPoint carries an explicit timestamp.\n\n";
    for (int i = forecast.hourly.size() - 4; i < forecast.hourly.size(); ++i) {
        const HourlyPoint &point = forecast.hourly.at(i);
        out() << "    " << point.time.toString(Qt::ISODate).leftJustified(24)
              << show(point.temperature, 1, QStringLiteral("°")).leftJustified(8)
              << "precip " << show(point.precipitation, 1) << "\n";
    }

    out() << "\n  derived daily — max, min and sums over the hours in each Toronto day.\n"
             "  sunrise and sunset are absent because Locationforecast has no sun product,\n"
             "  and the UI hides the arc rather than drawing one from midnight to midnight.\n\n";
    out() << "    date          high    low     precip  code  sunrise\n";
    for (const DailyPoint &day : forecast.daily) {
        out() << "    " << day.date.toString(Qt::ISODate).leftJustified(14)
              << show(day.temperatureMax, 1, QStringLiteral("°")).leftJustified(8)
              << show(day.temperatureMin, 1, QStringLiteral("°")).leftJustified(8)
              << show(day.precipitationSum, 1).leftJustified(8)
              << (day.weatherCode ? QString::number(*day.weatherCode)
                                  : QStringLiteral("—")).leftJustified(6)
              << (day.sunrise.isValid() ? day.sunrise.toString(Qt::ISODate)
                                        : QStringLiteral("—"))
              << "\n";
    }

    out() << "\n  the WMO codes this adapter can produce:\n    ";
    for (const int code : metNoWeatherCodes())
        out() << code << " ";
    out() << "\n  68, 69, 83 and 84 are sleet — rain and snow together. Open-Meteo never\n"
             "  emits them, so a code-to-icon table built from Open-Meteo's documentation\n"
             "  has four holes that only appear while the fallback is serving.\n";
}

} // namespace

int main(int argc, char **argv)
{
    QCoreApplication app(argc, argv);

    out() << "clima provider probe — recorded fixtures, no network\n";

    rule(QStringLiteral("AIR QUALITY — the Europe pollen gate, seen rather than asserted"));
    probeAirQuality(QStringLiteral("Toronto — outside the CAMS European domain"),
                    QStringLiteral("toronto.json"));
    probeAirQuality(QStringLiteral("Berlin — inside it"), QStringLiteral("berlin.json"));

    probeMetNorway();

    out() << "\n";
    out().flush();
    return 0;
}
