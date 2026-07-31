// SPDX-FileCopyrightText: 2026 Jowi Aoun
// SPDX-License-Identifier: MPL-2.0

#include "libclima/providers/metno/metnoforecastprovider.h"

#include "libclima/core/clock.h"
#include "libclima/net/httpclient.h"
#include "libclima/providers/metno/symbolcode.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>
#include <QMap>

#include <algorithm>

namespace clima {

namespace {

const char kProviderId[] = "met-no";

// ---- reading the payload -----------------------------------------------------

Reading number(const QJsonValue &value)
{
    if (value.isUndefined() || value.isNull() || !value.isDouble())
        return std::nullopt;
    return value.toDouble();
}

// MET's timestamps are UTC with an explicit Z. Parsed as ISO and forced to UTC
// anyway, because a payload that ever arrives without the suffix would
// otherwise be reinterpreted in the machine's local zone — which is a fixture
// that parses differently in Oslo and in Toronto, and the determinism invariant
// is not a thing to leave to a remote server's formatting.
QDateTime instantAt(const QJsonValue &value)
{
    if (!value.isString())
        return {};
    const QDateTime parsed = QDateTime::fromString(value.toString(), Qt::ISODate);
    if (!parsed.isValid())
        return {};
    return parsed.toUTC();
}

// ---- units, checked rather than assumed --------------------------------------
//
// `properties.meta.units` says what everything is measured in, and this reads
// it. Not paranoia: the one conversion in this file is wind, m/s to km/h, and
// the failure mode of assuming it is that a 20 m/s gale renders as 20 km/h — a
// stiff breeze — with no error anywhere. A unit we do not recognise is a Parse
// error naming the unit, which is a bug report rather than a wrong number.
struct Units {
    bool    ok = true;
    QString offending;

    [[nodiscard]] static Units check(const QJsonObject &units)
    {
        static const QMap<QString, QString> expected = {
            { QStringLiteral("air_temperature"), QStringLiteral("celsius") },
            { QStringLiteral("air_pressure_at_sea_level"), QStringLiteral("hPa") },
            { QStringLiteral("cloud_area_fraction"), QStringLiteral("%") },
            { QStringLiteral("relative_humidity"), QStringLiteral("%") },
            { QStringLiteral("wind_from_direction"), QStringLiteral("degrees") },
            { QStringLiteral("wind_speed"), QStringLiteral("m/s") },
            { QStringLiteral("precipitation_amount"), QStringLiteral("mm") },
        };

        Units result;
        for (auto it = expected.cbegin(); it != expected.cend(); ++it) {
            const QJsonValue declared = units.value(it.key());
            // Absent is fine — `compact` omits the units for variables it did
            // not send. Present and different is not.
            if (declared.isUndefined() || declared.isNull())
                continue;
            if (declared.toString() != it.value()) {
                result.ok        = false;
                result.offending = it.key() + QStringLiteral(" is ") + declared.toString()
                    + QStringLiteral(", expected ") + it.value();
                return result;
            }
        }
        return result;
    }
};

constexpr double kMetresPerSecondToKilometresPerHour = 3.6;

// One forward-looking block: what it says, and how long it covers.
struct Period {
    Reading     precipitation;
    WeatherCode weatherCode;
    std::optional<bool> isDay;
    int         hours = 0;

    [[nodiscard]] bool isValid() const { return hours > 0; }
};

Period readPeriod(const QJsonObject &data, const QString &name, int hours)
{
    const QJsonValue block = data.value(name);
    if (!block.isObject())
        return {};

    const QJsonObject object = block.toObject();

    Period period;
    period.hours = hours;
    period.precipitation =
        number(object.value(QStringLiteral("details"))
                   .toObject()
                   .value(QStringLiteral("precipitation_amount")));

    const QString symbol = object.value(QStringLiteral("summary"))
                               .toObject()
                               .value(QStringLiteral("symbol_code"))
                               .toString();
    const SymbolCode parsed = parseSymbolCode(symbol);
    period.weatherCode      = parsed.code;
    period.isDay            = parsed.isDay;
    return period;
}

// The daily rollup's weather code: the most significant of the day.
//
// Approximated by the numerically largest WMO code, which is what Open-Meteo's
// own daily `weather_code` amounts to, and which works because table 4677 is
// ordered roughly by severity — clear below cloud below fog below rain below
// snow below showers below thunder. "Roughly" is doing real work in that
// sentence: 45 (fog) outranks 3 (overcast) correctly and 61 (light rain)
// outranks 45 arguably. It is an approximation and it is labelled as one, and
// the alternative — a bespoke severity ranking of a hundred codes — is a table
// nobody can check against anything.
WeatherCode mostSignificant(const QList<int> &codes)
{
    if (codes.isEmpty())
        return std::nullopt;
    return *std::max_element(codes.cbegin(), codes.cend());
}

// max/min over the Readings that are present. Absent when none are, which is
// the difference between "the high was 0 °C" and "there was no high".
Reading maximum(const QList<double> &values)
{
    if (values.isEmpty())
        return std::nullopt;
    return *std::max_element(values.cbegin(), values.cend());
}

Reading minimum(const QList<double> &values)
{
    if (values.isEmpty())
        return std::nullopt;
    return *std::min_element(values.cbegin(), values.cend());
}

} // namespace

// ---- construction --------------------------------------------------------------

MetNoForecastProvider::MetNoForecastProvider(HttpClient *http, Clock *clock, QObject *parent)
    : QObject(parent)
    , m_http(http)
    , m_clock(clock)
    , m_baseUrl(QUrl(QStringLiteral("https://api.met.no/weatherapi/locationforecast/2.0/compact")))
{
}

MetNoForecastProvider::~MetNoForecastProvider() = default;

void MetNoForecastProvider::setBaseUrl(const QUrl &url)
{
    m_baseUrl = url;
}

// ---- identity and credit ---------------------------------------------------------

QString MetNoForecastProvider::id() const
{
    return QString::fromLatin1(kProviderId);
}

QString MetNoForecastProvider::displayName() const
{
    return QStringLiteral("MET Norway");
}

Attribution MetNoForecastProvider::attribution() const
{
    // docs/02-data-sources.md §2.9. The note is not decoration: their terms make
    // the identifying User-Agent part of the deal, and the About screen showing
    // what we identify ourselves as is how a user who has been refused can see
    // why. HttpClient::userAgent() is public for exactly that screen.
    Attribution credit;
    credit.name        = QStringLiteral("MET Norway");
    credit.creditLine  = QStringLiteral(
        "Weather data from MET Norway (Norwegian Meteorological Institute)");
    credit.homepage    = QUrl(QStringLiteral("https://www.met.no/"));
    credit.licenceName = QStringLiteral("CC BY 4.0");
    credit.licenceUrl  = QUrl(QStringLiteral("https://creativecommons.org/licenses/by/4.0/"));
    credit.upstream    = { QStringLiteral("MET Nordic"), QStringLiteral("ECMWF") };
    credit.note        = QStringLiteral(
        "Locationforecast 2.0 requires an identifying User-Agent with a contact address and "
        "conditional requests; Clima sends both. Used as the automatic fallback when the "
        "primary provider cannot be reached.");
    return credit;
}

bool MetNoForecastProvider::covers(Coordinate coord) const
{
    // Global. Their alerts product is Norway-only, which is a different
    // provider and a different milestone; Locationforecast is worldwide.
    return coord.isValid();
}

Capabilities MetNoForecastProvider::capabilitiesAt(Coordinate) const
{
    // Uniform everywhere, which is worth stating rather than leaving implicit:
    // nothing here is undetermined, because unlike CAMS's pollen this product
    // does not vary by region. The whole three-valued apparatus in
    // iforecastprovider.h exists for the other provider's sake, and a provider
    // that does not need it says so by never returning an undetermined set.
    //
    // The absences are the point. Everything the `compact` product does not
    // carry is a flag that is NOT set, which makes the UI hide the row rather
    // than draw it full of zeros — the list, and what each one would cost, is
    // in the header.
    const CapabilityFlags available = Capability::CurrentConditions | Capability::Hourly
        | Capability::Daily | Capability::Temperature | Capability::Humidity
        | Capability::Precipitation | Capability::Wind | Capability::Pressure
        | Capability::CloudCover | Capability::WeatherCode;

    return Capabilities(available);
}

// ---- fetching ---------------------------------------------------------------------

QFuture<Result<Forecast>> MetNoForecastProvider::fetchForecast(const ForecastRequest &request)
{
    HttpRequest http;
    http.providerId = id();
    http.endpoint   = QStringLiteral("locationforecast");
    http.url        = m_baseUrl;
    http.kind       = DataKind::Forecast;
    http.coordinate = request.coord;

    // `lat`/`lon`, not `latitude`/`longitude`. This is why HttpRequest carries
    // the parameter names rather than letting a provider build its own URL:
    // the rounding that keeps a map drag from becoming a hundred requests
    // happens in one place, and that place needs to be told what this provider
    // calls its coordinate.
    http.latitudeParameter  = QStringLiteral("lat");
    http.longitudeParameter = QStringLiteral("lon");

    // conditional stays true. Their terms require it — see the header — and
    // there is deliberately no way for a caller to turn it off here.

    const QString key = RequestKey::forRequest(http).toString();

    const QTimeZone zone = request.timeZone;

    QFuture<Result<HttpResponse>> transfer = m_http->send(http);

    return transfer.then(
        this, [this, key, zone](const Result<HttpResponse> &result) -> Result<Forecast> {
            if (!result.hasValue())
                return result.error();

            const HttpResponse &response = result.value();

            if (response.notModified) {
                const auto remembered = m_lastParsed.constFind(key);
                if (remembered == m_lastParsed.cend()) {
                    Error error(ErrorKind::Parse,
                                QStringLiteral("304 for a payload this process never parsed"));
                    error.setProviderId(id());
                    error.setHttpStatus(response.status);
                    return error;
                }
                Forecast current = *remembered;
                current.fetchedAt = response.fetchedAt;
                return current;
            }

            Result<Forecast> parsed = parse(response.body, zone, response.fetchedAt);
            if (!parsed.hasValue()) {
                Error error = parsed.error();
                error.setProviderId(id());
                return error;
            }

            parsed.value().providerId = id();
            m_lastParsed.insert(key, parsed.value());
            return parsed;
        });
}

// ---- parsing -------------------------------------------------------------------------

Result<Forecast> MetNoForecastProvider::parse(const QByteArray &body, const QTimeZone &timeZone,
                                              const QDateTime &fetchedAt)
{
    QJsonParseError     parseError;
    const QJsonDocument document = QJsonDocument::fromJson(body, &parseError);
    if (document.isNull() || !document.isObject()) {
        return Error(ErrorKind::Parse,
                     QStringLiteral("locationforecast payload is not a JSON object: %1")
                         .arg(parseError.errorString()));
    }

    const QJsonObject root       = document.object();
    const QJsonObject properties = root.value(QStringLiteral("properties")).toObject();
    const QJsonObject meta       = properties.value(QStringLiteral("meta")).toObject();

    const Units units = Units::check(meta.value(QStringLiteral("units")).toObject());
    if (!units.ok) {
        return Error(ErrorKind::Parse,
                     QStringLiteral("MET Norway changed a unit: %1").arg(units.offending));
    }

    const QJsonArray timeseries = properties.value(QStringLiteral("timeseries")).toArray();
    if (timeseries.isEmpty()) {
        return Error(ErrorKind::Parse,
                     QStringLiteral("locationforecast payload carries no timeseries"));
    }

    Forecast forecast;
    forecast.fetchedAt = fetchedAt;
    forecast.issuedAt  = instantAt(meta.value(QStringLiteral("updated_at")));

    // The zone the days below are grouped by, recorded so a UI formatting from
    // it is self-consistent even when the caller supplied nothing. See header.
    forecast.timeZone = timeZone.isValid() ? timeZone : QTimeZone::UTC;

    // GeoJSON order is [longitude, latitude, altitude]. Not [lat, lon]. Getting
    // this backwards produces a forecast for a plausible-looking place in the
    // wrong hemisphere, which is the kind of bug that survives review.
    const QJsonArray coordinates =
        root.value(QStringLiteral("geometry")).toObject().value(QStringLiteral("coordinates"))
            .toArray();
    if (coordinates.size() >= 2) {
        forecast.coordinate = Coordinate{ coordinates.at(1).toDouble(),
                                          coordinates.at(0).toDouble() };
    }
    if (coordinates.size() >= 3)
        forecast.elevation = coordinates.at(2).toDouble();

    // ---- pass one: the instants -------------------------------------------
    //
    // One point per entry, carrying only what `instant` measured AT that time.
    // The forward-looking blocks are applied in pass two, one point later —
    // see the header for why they cannot be applied here.
    QHash<QDateTime, int> indexByTime;
    forecast.hourly.reserve(timeseries.size());

    for (const QJsonValue &entry : timeseries) {
        const QJsonObject object = entry.toObject();
        const QDateTime   time   = instantAt(object.value(QStringLiteral("time")));
        if (!time.isValid())
            continue;

        const QJsonObject details = object.value(QStringLiteral("data"))
                                        .toObject()
                                        .value(QStringLiteral("instant"))
                                        .toObject()
                                        .value(QStringLiteral("details"))
                                        .toObject();

        HourlyPoint point;
        point.time              = time;
        point.temperature       = number(details.value(QStringLiteral("air_temperature")));
        point.relativeHumidity  = number(details.value(QStringLiteral("relative_humidity")));
        point.pressureMsl       = number(details.value(QStringLiteral("air_pressure_at_sea_level")));
        point.cloudCover        = number(details.value(QStringLiteral("cloud_area_fraction")));
        point.windDirection     = number(details.value(QStringLiteral("wind_from_direction")));

        if (const Reading speed = number(details.value(QStringLiteral("wind_speed")));
            speed.has_value()) {
            point.windSpeed = *speed * kMetresPerSecondToKilometresPerHour;
        }

        indexByTime.insert(time, int(forecast.hourly.size()));
        forecast.hourly.append(point);
    }

    if (forecast.hourly.isEmpty()) {
        return Error(ErrorKind::Parse,
                     QStringLiteral("locationforecast carried %1 entries and none had a time")
                         .arg(timeseries.size()));
    }

    // ---- pass two: the periods, shifted onto the point that ends them ------
    for (const QJsonValue &entry : timeseries) {
        const QJsonObject object = entry.toObject();
        const QDateTime   time   = instantAt(object.value(QStringLiteral("time")));
        if (!time.isValid())
            continue;

        const QJsonObject data = object.value(QStringLiteral("data")).toObject();

        // The 1-hour block wins where both exist. Not a preference — using both
        // would count the transition hour twice, once in each regime.
        Period period = readPeriod(data, QStringLiteral("next_1_hours"), 1);
        if (!period.isValid())
            period = readPeriod(data, QStringLiteral("next_6_hours"), 6);
        if (!period.isValid())
            continue;

        const QDateTime endsAt = time.addSecs(qint64(period.hours) * 3600);
        const auto      target = indexByTime.constFind(endsAt);

        // The final block has no point to land on, and inventing one would put
        // a precipitation total on an hour with no temperature. Dropped, and
        // the header says so.
        if (target == indexByTime.cend())
            continue;

        HourlyPoint &point = forecast.hourly[*target];
        point.precipitation = period.precipitation;
        point.weatherCode   = period.weatherCode;
        point.isDay         = period.isDay;
    }

    // ---- current ----------------------------------------------------------
    //
    // The first entry's instant. MET has no separate "observations now"
    // product in Locationforecast — the first timestep IS now, rounded to the
    // hour — so `current` is that entry rather than a second request.
    const HourlyPoint &first = forecast.hourly.constFirst();
    forecast.current.time             = first.time;
    forecast.current.temperature      = first.temperature;
    forecast.current.relativeHumidity = first.relativeHumidity;
    forecast.current.pressureMsl      = first.pressureMsl;
    forecast.current.cloudCover       = first.cloudCover;
    forecast.current.windSpeed        = first.windSpeed;
    forecast.current.windDirection    = first.windDirection;

    // The weather code for "now" comes from the first entry's own next_1_hours
    // — the hour that is starting — rather than from the shifted value on the
    // first point, which is empty because it would describe the past.
    const QJsonObject firstData =
        timeseries.at(0).toObject().value(QStringLiteral("data")).toObject();
    Period nowPeriod = readPeriod(firstData, QStringLiteral("next_1_hours"), 1);
    if (!nowPeriod.isValid())
        nowPeriod = readPeriod(firstData, QStringLiteral("next_6_hours"), 6);
    forecast.current.weatherCode = nowPeriod.weatherCode;
    forecast.current.isDay       = nowPeriod.isDay;

    // ---- daily -------------------------------------------------------------
    //
    // Derived, not fetched: MET has no daily product. Grouped by calendar date
    // in `forecast.timeZone`, which is the caller's zone or UTC — the header
    // explains why that is the honest arrangement and what the alternative
    // would be lying about.
    struct DayAccumulator {
        QList<double> temperatures;
        QList<double> windSpeeds;
        QList<int>    weatherCodes;
        Reading       precipitation;
    };
    QMap<QDate, DayAccumulator> days;

    for (const HourlyPoint &point : std::as_const(forecast.hourly)) {
        const QDate date = point.time.toTimeZone(forecast.timeZone).date();
        DayAccumulator &day = days[date];

        if (point.temperature.has_value())
            day.temperatures.append(*point.temperature);
        if (point.windSpeed.has_value())
            day.windSpeeds.append(*point.windSpeed);
        if (point.weatherCode.has_value())
            day.weatherCodes.append(*point.weatherCode);
        if (point.precipitation.has_value())
            day.precipitation = day.precipitation.value_or(0.0) + *point.precipitation;
    }

    forecast.daily.reserve(int(days.size()));
    for (auto it = days.cbegin(); it != days.cend(); ++it) {
        DailyPoint daily;
        daily.date             = it.key();
        daily.temperatureMax   = maximum(it->temperatures);
        daily.temperatureMin   = minimum(it->temperatures);
        daily.precipitationSum = it->precipitation;
        daily.windSpeedMax     = maximum(it->windSpeeds);
        daily.weatherCode      = mostSignificant(it->weatherCodes);

        // sunrise and sunset stay invalid. Locationforecast has no sun product,
        // and Capability::SunTimes is not advertised, so a UI hides the arc
        // rather than drawing one from midnight to midnight.
        forecast.daily.append(daily);
    }

    return forecast;
}

} // namespace clima
