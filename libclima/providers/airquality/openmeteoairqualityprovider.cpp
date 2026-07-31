// SPDX-FileCopyrightText: 2026 Jowi Aoun
// SPDX-License-Identifier: MPL-2.0

#include "libclima/providers/airquality/openmeteoairqualityprovider.h"

#include "libclima/core/clock.h"
#include "libclima/net/httpclient.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>
#include <QLocale>
#include <QPromise>
#include <QTimeZone>

namespace clima {

namespace {

// The one place the provider's own name is written down. HttpClient disables by
// this string on a 403 and the cache keys by it, so the forecast provider on the
// other host must use the same one — see the header.
const char kProviderId[] = "open-meteo";

// ---- the hourly series we ask for -------------------------------------------
//
// Built from the domain enums rather than typed as a literal, so that adding a
// pollutant to Pollutant means asking for it, parsing it and gating it, all
// from one edit. A hand-written parameter string is a fourth place to keep in
// sync and the one that fails silently: an unknown parameter is not an error to
// Open-Meteo, it is just a series that never arrives.
//
// The series that are not in an enum — dust, aerosol optical depth, ammonia,
// UV — are listed here because there is nothing to enumerate them over. They
// are single readings, not families.
QStringList pollutantSeries()
{
    QStringList names;
    for (int i = 0; i < int(Pollutant::Count); ++i)
        names.append(pollutantId(static_cast<Pollutant>(i)));
    return names;
}

QStringList subIndexSeries()
{
    QStringList names;
    for (int i = 0; i < int(Pollutant::Count); ++i) {
        const QString name = europeanSubIndexId(static_cast<Pollutant>(i));
        if (!name.isEmpty())
            names.append(name);
    }
    return names;
}

QStringList pollenSeries()
{
    QStringList names;
    for (int i = 0; i < int(PollenSpecies::Count); ++i)
        names.append(pollenSpeciesId(static_cast<PollenSpecies>(i)));
    return names;
}

QString currentParameters()
{
    QStringList names{ QStringLiteral("european_aqi"), QStringLiteral("us_aqi") };
    names += pollutantSeries();
    names += subIndexSeries();
    return names.join(QLatin1Char(','));
}

QString hourlyParameters()
{
    QStringList names{ QStringLiteral("european_aqi"), QStringLiteral("us_aqi") };
    names += pollutantSeries();
    names += subIndexSeries();
    names += QStringList{ QStringLiteral("dust"), QStringLiteral("aerosol_optical_depth"),
                          QStringLiteral("ammonia"), QStringLiteral("uv_index") };
    names += pollenSeries();
    return names.join(QLatin1Char(','));
}

// ---- reading the payload -----------------------------------------------------

// Open-Meteo writes a missing value as JSON null, and QJsonValue::toDouble()
// turns null into 0.0 without complaining. Every numeric read in this file goes
// through here instead, because that silent 0.0 is precisely the bug the whole
// header is about.
Reading number(const QJsonValue &value)
{
    if (value.isUndefined() || value.isNull() || !value.isDouble())
        return std::nullopt;
    return value.toDouble();
}

std::optional<int> integer(const QJsonValue &value)
{
    const Reading reading = number(value);
    if (!reading.has_value())
        return std::nullopt;
    return int(qRound(*reading));
}

// "2026-07-31T05:00" in the location's own zone, plus the offset the response
// reported, becomes an instant. Qt::ISODate on a string with no zone suffix
// yields a QDateTime in *local* time — the machine's — so the offset has to be
// applied explicitly or a fixture parsed in Toronto and in Berlin produces two
// different answers from the same bytes.
QDateTime instantAt(const QJsonValue &value, int utcOffsetSeconds)
{
    if (!value.isString())
        return {};
    QDateTime parsed = QDateTime::fromString(value.toString(), Qt::ISODate);
    if (!parsed.isValid())
        return {};
    parsed.setTimeZone(QTimeZone::fromSecondsAheadOfUtc(utcOffsetSeconds));
    return parsed.toUTC();
}

// One hourly series. Reports both the values and the one fact the gate needs:
// whether *any* hour of it was non-null.
struct Series {
    QList<Reading> values;
    bool           present = false;   // at least one non-null sample

    [[nodiscard]] Reading at(int index) const
    {
        if (index < 0 || index >= values.size())
            return std::nullopt;
        return values.at(index);
    }
};

Series readSeries(const QJsonObject &hourly, const QString &name)
{
    Series series;
    const QJsonValue value = hourly.value(name);
    if (!value.isArray())
        return series;

    const QJsonArray array = value.toArray();
    series.values.reserve(array.size());
    for (const QJsonValue &entry : array) {
        const Reading reading = number(entry);
        // present, not "non-zero". Berlin's alder pollen is 0.0 for all 72
        // hours of a July fixture and it is a measurement; Toronto's is null
        // and it is the absence of a product. Confusing the two hides a working
        // card for two thirds of the year. See the header.
        if (reading.has_value())
            series.present = true;
        series.values.append(reading);
    }
    return series;
}

} // namespace

// ---- construction -------------------------------------------------------------

OpenMeteoAirQualityProvider::OpenMeteoAirQualityProvider(HttpClient *http, Clock *clock,
                                                         QObject *parent)
    : QObject(parent)
    , m_http(http)
    , m_clock(clock)
    , m_baseUrl(QUrl(QStringLiteral("https://air-quality-api.open-meteo.com/v1/air-quality")))
{
}

OpenMeteoAirQualityProvider::~OpenMeteoAirQualityProvider() = default;

void OpenMeteoAirQualityProvider::setBaseUrl(const QUrl &url)
{
    m_baseUrl = url;
}

// ---- identity and credit -------------------------------------------------------

QString OpenMeteoAirQualityProvider::id() const
{
    return QString::fromLatin1(kProviderId);
}

QString OpenMeteoAirQualityProvider::displayName() const
{
    return QStringLiteral("Open-Meteo");
}

Attribution OpenMeteoAirQualityProvider::attribution() const
{
    // docs/02-data-sources.md §2.9, transcribed. The credit line is the exact
    // wording asked for and not a paraphrase; `upstream` names the model owners
    // behind the aggregator, which §2.9 requires separately and which is the
    // part a paraphrase would drop.
    Attribution credit;
    credit.name        = QStringLiteral("Open-Meteo");
    credit.creditLine  = QStringLiteral("Weather data by Open-Meteo.com");
    credit.homepage    = QUrl(QStringLiteral("https://open-meteo.com/"));
    credit.licenceName = QStringLiteral("CC BY 4.0");
    credit.licenceUrl  = QUrl(QStringLiteral("https://creativecommons.org/licenses/by/4.0/"));
    credit.upstream    = { QStringLiteral("Copernicus Atmosphere Monitoring Service (CAMS)"),
                           QStringLiteral("ECMWF") };
    credit.note        = QStringLiteral(
        "Air quality and pollen from the CAMS European and global reanalysis and forecast. "
        "Pollen and ammonia are produced for the CAMS European domain only.");
    return credit;
}

bool OpenMeteoAirQualityProvider::covers(Coordinate coord) const
{
    // Global. The interesting question at this provider is not whether it
    // answers here — it always does — but what is in the answer, which is what
    // capabilitiesAt() is for.
    return coord.isValid();
}

// ---- capabilities --------------------------------------------------------------

QString OpenMeteoAirQualityProvider::verdictKey(Coordinate coord)
{
    // One decimal place: ~11 km, one CAMS Europe grid cell. The header argues
    // the number.
    return coord.toKeyString(1);
}

Capabilities OpenMeteoAirQualityProvider::capabilitiesAt(Coordinate coord) const
{
    if (!coord.isValid())
        return {};

    // Global, verified: both indices and all six pollutants are non-null in
    // Toronto as well as Berlin. These do not depend on a payload, so they are
    // never undetermined — a UI can put the Air Quality tab up before the first
    // byte arrives and it will not have to take it down again.
    const CapabilityFlags always = Capability::CurrentConditions | Capability::Hourly
        | Capability::AirQualityIndex | Capability::Pollutants | Capability::UvIndex;

    const auto verdict = m_verdicts.constFind(verdictKey(coord));
    if (verdict == m_verdicts.cend()) {
        // Nothing fetched here yet. Not "no" — see iforecastprovider.h: a "no"
        // that becomes a "yes" two seconds later is a card that pops in, which
        // reads as a bug because it is one.
        return Capabilities(always, Capability::Pollen | Capability::Ammonia);
    }

    CapabilityFlags available = always;
    if (verdict->pollen)
        available |= Capability::Pollen;
    if (verdict->ammonia)
        available |= Capability::Ammonia;
    return Capabilities(available);
}

int OpenMeteoAirQualityProvider::rememberedVerdictCount() const
{
    return int(m_verdicts.size());
}

void OpenMeteoAirQualityProvider::remember(const AirQuality &airQuality)
{
    Verdict verdict;
    verdict.pollen  = airQuality.hasPollen;
    verdict.ammonia = airQuality.hasAmmonia;

    // Keyed by the coordinate the PROVIDER answered for, not the one we asked
    // about. Open-Meteo snaps to its grid cell and reports where it landed, and
    // that cell is the thing the verdict is actually about.
    m_verdicts.insert(verdictKey(airQuality.coordinate), verdict);
}

// ---- fetching --------------------------------------------------------------------

QFuture<Result<AirQuality>>
OpenMeteoAirQualityProvider::fetchAirQuality(const ForecastRequest &request)
{
    HttpRequest http;
    http.providerId = id();
    http.endpoint   = QStringLiteral("air-quality");
    http.url        = m_baseUrl;
    http.kind       = DataKind::AirQuality;
    http.coordinate = request.coord;
    http.parameters = {
        { QStringLiteral("timezone"), QStringLiteral("auto") },
        { QStringLiteral("current"), currentParameters() },
        { QStringLiteral("hourly"), hourlyParameters() },
        // CAMS publishes 5 days globally and 4 in Europe (§2.6). Asking for
        // more is not an error and does not produce more; asking for what the
        // caller wants, clamped, keeps the payload honest about its horizon.
        { QStringLiteral("forecast_days"), QString::number(qBound(1, request.days, 7)) },
    };

    // The key the last parsed payload for this exact request is filed under.
    // Same string the coalescer and the validator store use, because "the same
    // request" has to mean one thing — libclima/net/requestkey.h.
    const QString key = RequestKey::forRequest(http).toString();

    QFuture<Result<HttpResponse>> transfer = m_http->send(http);

    return transfer.then(this,
                         [this, key](const Result<HttpResponse> &result) -> Result<AirQuality> {
        if (!result.hasValue())
            return result.error();

        const HttpResponse &response = result.value();

        // ---- 304 -------------------------------------------------------
        //
        // A conditional GET that comes back Not Modified has succeeded: the
        // payload we already parsed is confirmed current, and the body is
        // deliberately empty (HttpResponse::notModified). Answering it with the
        // last parsed value — its expiry moved forward — is what makes the
        // conditional request worth sending at all, and it is the whole of the
        // saving: a few hundred bytes instead of ten kilobytes, several times
        // an hour, per user.
        //
        // The memo is per-process and per-request-key. It is not a substitute
        // for CacheStore, which persists across launches under §4.5's
        // 60-minute air-quality TTL; it is the piece that has to exist inside
        // the provider because the provider is what turns bytes into an
        // AirQuality, and a 304 carries no bytes.
        if (response.notModified) {
            const auto remembered = m_lastParsed.constFind(key);
            if (remembered == m_lastParsed.cend()) {
                // 304 for something we never had. The server is revalidating
                // against a validator somebody else stored. Not retryable and
                // not our data — say so rather than returning an empty answer.
                Error error(ErrorKind::Parse,
                            QStringLiteral("304 for a payload this process never parsed"));
                error.setProviderId(id());
                error.setHttpStatus(response.status);
                return error;
            }
            AirQuality current = *remembered;
            current.fetchedAt  = response.fetchedAt;
            return current;
        }

        Result<AirQuality> parsed = parse(response.body, response.fetchedAt);
        if (!parsed.hasValue()) {
            Error error = parsed.error();
            error.setProviderId(id());
            return error;
        }

        parsed.value().providerId = id();
        remember(parsed.value());
        m_lastParsed.insert(key, parsed.value());
        return parsed;
    });
}

// ---- parsing ------------------------------------------------------------------------

Result<AirQuality> OpenMeteoAirQualityProvider::parse(const QByteArray &body,
                                                      const QDateTime  &fetchedAt)
{
    QJsonParseError   parseError;
    const QJsonDocument document = QJsonDocument::fromJson(body, &parseError);
    if (document.isNull() || !document.isObject()) {
        return Error(ErrorKind::Parse,
                     QStringLiteral("air-quality payload is not a JSON object: %1")
                         .arg(parseError.errorString()));
    }

    const QJsonObject root = document.object();

    // Open-Meteo reports an error in-band, with HTTP 400 and `{"error": true,
    // "reason": "..."}`. Worth reading rather than reporting "no hourly data":
    // the reason is usually a parameter name we got wrong, and that is a bug in
    // this file rather than an outage.
    if (root.value(QStringLiteral("error")).toBool()) {
        return Error(ErrorKind::Parse,
                     QStringLiteral("Open-Meteo refused the request: %1")
                         .arg(root.value(QStringLiteral("reason")).toString()));
    }

    AirQuality airQuality;
    airQuality.fetchedAt = fetchedAt;
    airQuality.coordinate = Coordinate{ root.value(QStringLiteral("latitude")).toDouble(),
                                        root.value(QStringLiteral("longitude")).toDouble() };

    const int utcOffset = root.value(QStringLiteral("utc_offset_seconds")).toInt();
    const QString zoneName = root.value(QStringLiteral("timezone")).toString();
    if (!zoneName.isEmpty()) {
        const QTimeZone zone(zoneName.toUtf8());
        // An IANA name the host's tzdata does not know is not a reason to
        // discard a perfectly good air-quality reading. Fall back to the fixed
        // offset, which is right until the next DST transition and is what the
        // response's own `utc_offset_seconds` says anyway.
        airQuality.timeZone = zone.isValid() ? zone : QTimeZone::fromSecondsAheadOfUtc(utcOffset);
    } else {
        airQuality.timeZone = QTimeZone::fromSecondsAheadOfUtc(utcOffset);
    }

    // ---- current ----------------------------------------------------------
    const QJsonObject current = root.value(QStringLiteral("current")).toObject();
    if (!current.isEmpty()) {
        AirQualityPoint point;
        point.time        = instantAt(current.value(QStringLiteral("time")), utcOffset);
        point.europeanAqi = integer(current.value(QStringLiteral("european_aqi")));
        point.usAqi       = integer(current.value(QStringLiteral("us_aqi")));

        for (int i = 0; i < int(Pollutant::Count); ++i) {
            const auto    pollutant = static_cast<Pollutant>(i);
            const Reading value     = number(current.value(pollutantId(pollutant)));
            if (value.has_value())
                point.pollutants.insert(pollutant, *value);

            const QString subIndexName = europeanSubIndexId(pollutant);
            if (subIndexName.isEmpty())
                continue;
            const Reading subIndex = number(current.value(subIndexName));
            if (subIndex.has_value())
                point.europeanSubIndices.insert(pollutant, *subIndex);
        }

        airQuality.current = point;
    }

    // ---- hourly -----------------------------------------------------------
    const QJsonObject hourly = root.value(QStringLiteral("hourly")).toObject();
    const QJsonArray  times  = hourly.value(QStringLiteral("time")).toArray();
    if (times.isEmpty()) {
        return Error(ErrorKind::Parse,
                     QStringLiteral("air-quality payload carries no hourly time axis"));
    }

    // Read every series once, up front. Each one knows whether it was present
    // at any hour, which is the gate — computed here, for all series, by the
    // same rule, with nothing in it that knows the word "Europe".
    QList<Series> pollutantValues;
    QList<Series> subIndexValues;
    for (int i = 0; i < int(Pollutant::Count); ++i) {
        const auto pollutant = static_cast<Pollutant>(i);
        pollutantValues.append(readSeries(hourly, pollutantId(pollutant)));
        subIndexValues.append(readSeries(hourly, europeanSubIndexId(pollutant)));
    }

    QList<Series> pollenValues;
    for (int i = 0; i < int(PollenSpecies::Count); ++i)
        pollenValues.append(readSeries(hourly, pollenSpeciesId(static_cast<PollenSpecies>(i))));

    const Series europeanAqi = readSeries(hourly, QStringLiteral("european_aqi"));
    const Series usAqi       = readSeries(hourly, QStringLiteral("us_aqi"));
    const Series dust        = readSeries(hourly, QStringLiteral("dust"));
    const Series aerosol     = readSeries(hourly, QStringLiteral("aerosol_optical_depth"));
    const Series ammonia     = readSeries(hourly, QStringLiteral("ammonia"));
    const Series uvIndex     = readSeries(hourly, QStringLiteral("uv_index"));

    // THE GATE. One line each, and neither of them mentions a bounding box.
    airQuality.hasAmmonia = ammonia.present;
    for (const Series &species : pollenValues)
        airQuality.hasPollen = airQuality.hasPollen || species.present;

    airQuality.hourly.reserve(times.size());
    for (int index = 0; index < times.size(); ++index) {
        AirQualityPoint point;
        point.time = instantAt(times.at(index), utcOffset);
        if (!point.time.isValid())
            continue;

        if (const Reading value = europeanAqi.at(index); value.has_value())
            point.europeanAqi = int(qRound(*value));
        if (const Reading value = usAqi.at(index); value.has_value())
            point.usAqi = int(qRound(*value));

        for (int i = 0; i < int(Pollutant::Count); ++i) {
            const auto pollutant = static_cast<Pollutant>(i);
            if (const Reading value = pollutantValues.at(i).at(index); value.has_value())
                point.pollutants.insert(pollutant, *value);
            if (const Reading value = subIndexValues.at(i).at(index); value.has_value())
                point.europeanSubIndices.insert(pollutant, *value);
        }

        point.dust                = dust.at(index);
        point.aerosolOpticalDepth = aerosol.at(index);
        point.ammonia             = ammonia.at(index);
        point.uvIndex             = uvIndex.at(index);

        // The optional-whole, not six optional species. Absent outside the CAMS
        // European domain, which is the difference between "no pollen product"
        // and "no pollen today" — see airquality.h.
        if (airQuality.hasPollen) {
            QMap<PollenSpecies, double> pollen;
            for (int i = 0; i < int(PollenSpecies::Count); ++i) {
                if (const Reading value = pollenValues.at(i).at(index); value.has_value())
                    pollen.insert(static_cast<PollenSpecies>(i), *value);
            }
            point.pollen = pollen;
        }

        airQuality.hourly.append(point);
    }

    if (airQuality.hourly.isEmpty()) {
        return Error(ErrorKind::Parse,
                     QStringLiteral("air-quality payload carried %1 timestamps and none parsed")
                         .arg(times.size()));
    }

    return airQuality;
}

} // namespace clima
