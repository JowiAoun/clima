// SPDX-FileCopyrightText: 2026 Jowi Aoun
// SPDX-License-Identifier: MPL-2.0

#include "openmeteoforecastprovider.h"

#include "libclima/core/clock.h"
#include "libclima/net/httpclient.h"
#include "libclima/providers/openmeteo/openmeteoadapter.h"
#include "libclima/providers/openmeteo/openmeteovariables.h"

#include <QFutureWatcher>
#include <QPromise>

namespace clima {
namespace {

// Sixteen is the documented maximum for `/v1/forecast`; one is the minimum
// that produces a series at all.
constexpr int kMaxForecastDays = 16;

// Yesterday, always. See the header: the hourly strip shows observed hours
// behind the "now" marker and there is nowhere else to get them.
constexpr int kPastDays = 1;

// Which capabilities this provider could conceivably have somewhere. Anything
// outside this set is known-absent everywhere and is never reported as
// undetermined — a UI must not hold space for an ensemble tab on the grounds
// that `/v1/forecast` might one day return one.
CapabilityFlags supportedEverywhere()
{
    return Capability::CurrentConditions | Capability::Hourly | Capability::Daily
         | Capability::Temperature | Capability::ApparentTemperature | Capability::DewPoint
         | Capability::Humidity | Capability::Precipitation | Capability::PrecipitationType
         | Capability::PrecipitationProbability | Capability::Wind | Capability::WindGust
         | Capability::Pressure | Capability::CloudCover | Capability::Visibility
         | Capability::UvIndex | Capability::WeatherCode | Capability::SunTimes;
}

// Whether any hour in the series has a value for this field.
//
// This is the question that decides whether a metric tab exists, and it has to
// be asked of the whole column rather than of the first row: the recorded
// Toronto response has a null hour in the middle of an otherwise complete
// series, and a first-row test would hide the temperature tab for a forecast
// that begins with one gap.
template <typename Field>
bool anyHourHas(const QList<HourlyPoint> &hourly, Field field)
{
    for (const HourlyPoint &point : hourly) {
        if ((point.*field).has_value())
            return true;
    }
    return false;
}

Capabilities learnFrom(const Forecast &forecast)
{
    CapabilityFlags available;

    if (!forecast.current.isEmpty())
        available |= Capability::CurrentConditions;
    if (!forecast.hourly.isEmpty())
        available |= Capability::Hourly;
    if (!forecast.daily.isEmpty())
        available |= Capability::Daily;

    const QList<HourlyPoint> &hourly = forecast.hourly;

    if (anyHourHas(hourly, &HourlyPoint::temperature))
        available |= Capability::Temperature;
    if (anyHourHas(hourly, &HourlyPoint::apparentTemperature))
        available |= Capability::ApparentTemperature;
    if (anyHourHas(hourly, &HourlyPoint::dewPoint))
        available |= Capability::DewPoint;
    if (anyHourHas(hourly, &HourlyPoint::relativeHumidity))
        available |= Capability::Humidity;
    if (anyHourHas(hourly, &HourlyPoint::precipitation))
        available |= Capability::Precipitation;
    if (anyHourHas(hourly, &HourlyPoint::precipitationProbability))
        available |= Capability::PrecipitationProbability;
    if (anyHourHas(hourly, &HourlyPoint::windSpeed))
        available |= Capability::Wind;
    if (anyHourHas(hourly, &HourlyPoint::windGust))
        available |= Capability::WindGust;
    if (anyHourHas(hourly, &HourlyPoint::pressureMsl))
        available |= Capability::Pressure;
    if (anyHourHas(hourly, &HourlyPoint::cloudCover))
        available |= Capability::CloudCover;
    if (anyHourHas(hourly, &HourlyPoint::uvIndex))
        available |= Capability::UvIndex;

    // The two that ECMWF IFS does not carry, and the reason this function
    // exists rather than a constant. toronto-ecmwf-gaps.json is 72 hours of
    // null for both.
    if (anyHourHas(hourly, &HourlyPoint::visibility))
        available |= Capability::Visibility;

    // The rain / showers / snow split, which is what lets a chart colour a
    // band by phase rather than guessing from temperature.
    if (anyHourHas(hourly, &HourlyPoint::rain) || anyHourHas(hourly, &HourlyPoint::showers)
        || anyHourHas(hourly, &HourlyPoint::snowfall))
        available |= Capability::PrecipitationType;

    for (const HourlyPoint &point : hourly) {
        if (point.weatherCode) {
            available |= Capability::WeatherCode;
            break;
        }
    }

    for (const DailyPoint &day : forecast.daily) {
        if (day.sunrise.isValid() || day.sunset.isValid()) {
            available |= Capability::SunTimes;
            break;
        }
    }

    // Nothing is undetermined once a payload has been seen: the response is
    // the witness, and it has now testified.
    return Capabilities(available);
}

} // namespace

OpenMeteoForecastProvider::OpenMeteoForecastProvider(HttpClient *http, Clock *clock,
                                                     QObject *parent)
    : QObject(parent)
    , m_http(http)
    , m_clock(clock)
    , m_baseUrl(QStringLiteral("https://api.open-meteo.com/v1/forecast"))
{
}

OpenMeteoForecastProvider::~OpenMeteoForecastProvider() = default;

QString OpenMeteoForecastProvider::providerId()
{
    return QStringLiteral("open-meteo");
}

QString OpenMeteoForecastProvider::id() const
{
    return providerId();
}

QString OpenMeteoForecastProvider::displayName() const
{
    // Not translated here. iforecastprovider.h: "For humans. Translated by the
    // app, not here." — and this one is a proper noun anyway.
    return QStringLiteral("Open-Meteo");
}

Attribution OpenMeteoForecastProvider::attribution() const
{
    return openmeteo::attribution();
}

bool OpenMeteoForecastProvider::covers(Coordinate coord) const
{
    // Global. The only thing that is not covered is a coordinate that is not
    // one — a NaN out of a half-initialised map, a longitude of 400 — and
    // `covers` is the right place to catch it, because the alternative is a
    // request that goes out and comes back 400.
    return coord.isValid();
}

int OpenMeteoForecastProvider::clampDays(int requested)
{
    if (requested < 1)
        return 1;
    if (requested > kMaxForecastDays)
        return kMaxForecastDays;
    return requested;
}

void OpenMeteoForecastProvider::setBaseUrl(const QUrl &url)
{
    m_baseUrl = url;
}

QUrl OpenMeteoForecastProvider::baseUrl() const
{
    return m_baseUrl;
}

HttpRequest OpenMeteoForecastProvider::buildRequest(const ForecastRequest &request) const
{
    HttpRequest out;

    out.providerId = providerId();
    out.endpoint   = QStringLiteral("forecast");
    out.url        = m_baseUrl;
    out.kind       = DataKind::Forecast;

    // Handed over as a coordinate rather than baked into the parameters, so
    // that HttpClient rounds it — once, in one place, before it is hashed and
    // before it is sent. libclima/net/httprequest.h is emphatic about why.
    out.coordinate         = request.coord;
    out.latitudeParameter  = QStringLiteral("latitude");
    out.longitudeParameter = QStringLiteral("longitude");

    out.parameters = {
        // The zone resolved from the coordinate. We do not trust the naive
        // timestamps it produces — libclima/domain/timeaxis.h — but we do need
        // the IANA id and the offset it used, and this is the only parameter
        // that reports them.
        { QStringLiteral("timezone"), QStringLiteral("auto") },

        { QStringLiteral("forecast_days"), QString::number(clampDays(request.days)) },
        { QStringLiteral("past_days"), QString::number(kPastDays) },

        { QStringLiteral("current"), openmeteo::currentParameter() },
        { QStringLiteral("hourly"), openmeteo::hourlyParameter() },
        { QStringLiteral("daily"), openmeteo::dailyParameter() },
    };

    // No `temperature_unit`, `wind_speed_unit` or `precipitation_unit`. See
    // openmeteoadapter.h: unit-tagged responses make the cache unit-keyed, so
    // a °C-to-°F toggle would refetch every forecast the user has.

    if (!request.models.isEmpty()) {
        out.parameters.append({ QStringLiteral("models"),
                                request.models.join(QLatin1Char(',')) });
    }

    return out;
}

Capabilities OpenMeteoForecastProvider::capabilitiesAt(Coordinate coord) const
{
    const auto learned = m_learned.constFind(coord.rounded().toKeyString());
    if (learned != m_learned.constEnd())
        return *learned;

    // Nothing fetched here yet, so nothing is known. Not "no" — a UI that
    // renders "no" hides the UV tab for the two seconds before the payload
    // lands and then pops it in, which looks like a bug and is one.
    return Capabilities({}, supportedEverywhere());
}

void OpenMeteoForecastProvider::rememberCapabilities(Coordinate coord, const Forecast &forecast)
{
    m_learned.insert(coord.rounded().toKeyString(), learnFrom(forecast));
}

QFuture<Result<Forecast>> OpenMeteoForecastProvider::fetchForecast(const ForecastRequest &request)
{
    auto promise = std::make_shared<QPromise<Result<Forecast>>>();
    promise->start();
    QFuture<Result<Forecast>> future = promise->future();

    if (!covers(request.coord)) {
        Error error(ErrorKind::Unsupported, QStringLiteral("not a valid coordinate"));
        error.setProviderId(providerId());
        promise->addResult(Result<Forecast>(error));
        promise->finish();
        return future;
    }

    const Coordinate coord = request.coord;

    QFuture<Result<HttpResponse>> transfer = m_http->send(buildRequest(request));

    // A watcher parented to this, deleted when it finishes. The alternative —
    // `.then()` — runs its continuation on whichever thread finished the
    // future, and this object's QHash of learned capabilities is not thread
    // safe. A watcher delivers on the thread that owns it, which is the thread
    // that owns us, which is the rule libclima/net/httpclient.h already sets.
    auto *watcher = new QFutureWatcher<Result<HttpResponse>>(this);

    connect(watcher, &QFutureWatcherBase::finished, this,
            [this, watcher, promise, coord]() {
                watcher->deleteLater();

                const Result<HttpResponse> transferred = watcher->result();
                if (!transferred) {
                    promise->addResult(Result<Forecast>(transferred.error()));
                    promise->finish();
                    return;
                }

                const HttpResponse &response = transferred.value();

                // A 304 is a success with no bytes: the cache entry we already
                // hold is confirmed current. There is nothing to parse and it
                // is not this class's job to read the cache, so the caller is
                // told plainly rather than handed an empty Forecast that would
                // read as "the provider has nothing here".
                if (response.notModified) {
                    Error error(ErrorKind::Cancelled,
                                QStringLiteral("not modified; the cached forecast stands"));
                    error.setProviderId(providerId());
                    error.setHttpStatus(response.status);
                    promise->addResult(Result<Forecast>(error));
                    promise->finish();
                    return;
                }

                Result<Forecast> adapted =
                    openmeteo::adaptForecast(response.body, providerId());

                if (adapted) {
                    // The one timestamp this class produces, and it comes from
                    // the injected clock. In fixture mode that makes "updated
                    // 25 minutes ago" say what it said on the afternoon the
                    // fixtures were recorded.
                    adapted.value().fetchedAt = m_clock->now();
                    rememberCapabilities(coord, adapted.value());
                }

                promise->addResult(adapted);
                promise->finish();
            });

    watcher->setFuture(transfer);
    return future;
}

} // namespace clima
