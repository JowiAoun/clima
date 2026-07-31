// SPDX-FileCopyrightText: 2026 Jowi Aoun
// SPDX-License-Identifier: GPL-3.0-or-later
//
// The MET Norway fallback: the symbol vocabulary, the hour shift, and the
// honest list of what it cannot supply.
//
// tests/fixtures/metno/toronto.json is a recorded `compact` response — 90
// timeseries entries, hourly for about two and a half days and then six-hourly
// out to nine and a half. Toronto rather than Oslo on purpose: this provider is
// the *global* fallback, and a fixture from Norway would let a bug that assumed
// a Nordic domain pass.
//
// The three things this file is really guarding:
//
//   1. Every symbol in MET's published legend maps to something. An unmapped
//      symbol is a missing icon in one weather condition, on the fallback path,
//      which is the least-looked-at combination in the app.
//   2. Precipitation lands on the hour it ends. Get it wrong and every rain bar
//      is one column out — only when the fallback is serving.
//   3. What it does not have is *absent*, not zero. That is the difference
//      between hiding the gust row and telling somebody in a gale that the wind
//      is steady.

#include "libclima/core/clock.h"
#include "libclima/net/httpclient.h"
#include "libclima/providers/metno/metnoforecastprovider.h"
#include "libclima/providers/metno/symbolcode.h"
#include "support/httpstub.h"
#include "support/networkguard.h"

#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSignalSpy>
#include <QTest>

using namespace clima;

namespace {

QByteArray fixture()
{
    QFile file(QStringLiteral(CLIMA_SOURCE_DIR)
               + QStringLiteral("/tests/fixtures/metno/toronto.json"));
    if (!file.open(QIODevice::ReadOnly))
        return {};
    return file.readAll();
}

const Coordinate kToronto{ 43.7001, -79.4163 };
const QDateTime  kRecordedAt{ QDate(2026, 7, 31), QTime(9, 13, 51), QTimeZone::UTC };

// MET's published legend (metno/weathericons, weather/legend.csv), all 44 base
// symbol IDs, transcribed including the two with the stray `s`. This list is
// the point of the exhaustiveness test: it is upstream's, not ours, so a symbol
// they add and we do not map shows up as a failure the next time it is updated.
const char *const kLegend[] = {
    "clearsky", "fair", "partlycloudy", "cloudy",
    "lightrainshowers", "rainshowers", "heavyrainshowers",
    "lightrainshowersandthunder", "rainshowersandthunder", "heavyrainshowersandthunder",
    "lightsleetshowers", "sleetshowers", "heavysleetshowers",
    "lightssleetshowersandthunder", "sleetshowersandthunder", "heavysleetshowersandthunder",
    "lightsnowshowers", "snowshowers", "heavysnowshowers",
    "lightssnowshowersandthunder", "snowshowersandthunder", "heavysnowshowersandthunder",
    "lightrain", "rain", "heavyrain",
    "lightrainandthunder", "rainandthunder", "heavyrainandthunder",
    "lightsleet", "sleet", "heavysleet",
    "lightsleetandthunder", "sleetandthunder", "heavysleetandthunder",
    "lightsnow", "snow", "heavysnow",
    "lightsnowandthunder", "snowandthunder", "heavysnowandthunder",
    "fog",
};

} // namespace

class TestMetNo : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void initTestCase();
    void init();
    void cleanup();

    // the symbol vocabulary
    void everySymbolInTheLegendMaps();
    void theTwoUpstreamTyposAreAccepted();
    void dayAndNightComeFromTheSuffixAndPolarTwilightIsNeither();
    void sleetGetsTheRealMixedPrecipitationCodes();
    void thunderIsNeverGivenHailItDidNotClaim();
    void anUnknownSymbolIsNoCodeRatherThanAGuess();

    // the adaptation
    void theInstantsBecomeHourlyPointsInOrder();
    void windIsConvertedFromMetresPerSecond();
    void precipitationLandsOnTheHourItEnds();
    void theSixHourlyTailIsNotCountedTwice();
    void theSeriesThinsAndTheTimestampsSaySo();
    void dailyIsDerivedInTheZoneTheCallerSupplied();

    // the honest absences
    void whatItCannotSupplyIsAbsentRatherThanZero();
    void capabilitiesOmitEverythingCompactDoesNotCarry();
    void nothingIsUndeterminedBecauseNothingHereIsRegional();

    // the wire
    void theRequestUsesLatAndLonTruncatedToFourDecimals();
    void aUnitChangeIsAParseErrorRatherThanAWrongNumber();
    void forbiddenIsATypedErrorAndTheProviderStopsAsking();

private:
    HttpStub    m_stub;
    FrozenClock m_clock{ kRecordedAt };
};

void TestMetNo::initTestCase()
{
    NetworkGuard::install();
    QVERIFY2(!fixture().isEmpty(), "met.no fixture missing");
}

void TestMetNo::init()
{
    m_stub.reset();
    QVERIFY2(m_stub.listen(), "could not listen on loopback");
    NetworkGuard::clearAttempts();
}

void TestMetNo::cleanup()
{
    QCOMPARE(NetworkGuard::externalAttempts(), QStringList());
}

// ---- the symbol vocabulary ------------------------------------------------------

void TestMetNo::everySymbolInTheLegendMaps()
{
    QStringList unmapped;
    for (const char *symbol : kLegend) {
        const SymbolCode parsed = parseSymbolCode(QString::fromLatin1(symbol));
        if (!parsed.isValid())
            unmapped.append(QString::fromLatin1(symbol));
    }

    QVERIFY2(unmapped.isEmpty(),
             qPrintable(QStringLiteral("MET symbols with no WMO code: %1\n\n"
                                       "  Add them to libclima/providers/metno/symbolcode.cpp. "
                                       "An unmapped symbol is a\n"
                                       "  missing icon in one weather condition on the fallback "
                                       "path, which is the\n"
                                       "  least-exercised combination in the app.")
                            .arg(unmapped.join(QStringLiteral(", ")))));

    // And the variants, which is what actually arrives in a payload.
    QVERIFY(parseSymbolCode(QStringLiteral("partlycloudy_day")).isValid());
    QVERIFY(parseSymbolCode(QStringLiteral("clearsky_night")).isValid());
    QVERIFY(parseSymbolCode(QStringLiteral("lightrainshowers_polartwilight")).isValid());
}

void TestMetNo::theTwoUpstreamTyposAreAccepted()
{
    // `lightssleetshowersandthunder` and `lightssnowshowersandthunder`, with a
    // stray `s`, are the strings upstream publishes and sends. A table that
    // spelled them correctly would match nothing.
    QVERIFY(parseSymbolCode(QStringLiteral("lightssleetshowersandthunder")).isValid());
    QVERIFY(parseSymbolCode(QStringLiteral("lightssnowshowersandthunder")).isValid());

    // Both spellings, so the day MET fix it nothing breaks.
    QVERIFY(parseSymbolCode(QStringLiteral("lightsleetshowersandthunder")).isValid());
    QVERIFY(parseSymbolCode(QStringLiteral("lightsnowshowersandthunder")).isValid());

    // And the trailing space their CSV carries inside one of the IDs.
    QVERIFY(parseSymbolCode(QStringLiteral("lightssleetshowersandthunder ")).isValid());
}

void TestMetNo::dayAndNightComeFromTheSuffixAndPolarTwilightIsNeither()
{
    QCOMPARE(parseSymbolCode(QStringLiteral("clearsky_day")).isDay, std::optional<bool>(true));
    QCOMPARE(parseSymbolCode(QStringLiteral("clearsky_night")).isDay, std::optional<bool>(false));

    // polartwilight is the long dusk above the Arctic circle — neither, and a
    // boolean that had to pick would be wrong in MET's own back yard.
    QVERIFY(!parseSymbolCode(QStringLiteral("clearsky_polartwilight")).isDay.has_value());

    // A symbol with no variants says nothing about daylight, and "no suffix"
    // must not be read as "night".
    QVERIFY(!parseSymbolCode(QStringLiteral("cloudy")).isDay.has_value());
}

void TestMetNo::sleetGetsTheRealMixedPrecipitationCodes()
{
    // 68/69 and 83/84 — rain and snow together, steady and showery. Open-Meteo
    // never emits these, which is exactly why they are worth a test: a UI icon
    // table built by reading Open-Meteo's documentation has holes here, and the
    // holes only show when the fallback is serving.
    QCOMPARE(parseSymbolCode(QStringLiteral("lightsleet")).code, WeatherCode(68));
    QCOMPARE(parseSymbolCode(QStringLiteral("sleet")).code, WeatherCode(69));
    QCOMPARE(parseSymbolCode(QStringLiteral("heavysleet")).code, WeatherCode(69));
    QCOMPARE(parseSymbolCode(QStringLiteral("lightsleetshowers_day")).code, WeatherCode(83));
    QCOMPARE(parseSymbolCode(QStringLiteral("sleetshowers_night")).code, WeatherCode(84));

    // Not freezing rain. 66/67 is liquid that freezes on contact and closes
    // roads; sleet is wet snow. Folding one onto the other would put a value in
    // the field that the UI already draws and that describes different weather.
    const QList<int> codes = metNoWeatherCodes();
    QVERIFY(!codes.contains(66));
    QVERIFY(!codes.contains(67));
    QVERIFY(codes.contains(68));
    QVERIFY(codes.contains(69));
    QVERIFY(codes.contains(83));
    QVERIFY(codes.contains(84));
}

void TestMetNo::thunderIsNeverGivenHailItDidNotClaim()
{
    // 96 and 99 mean thunderstorm *with hail*. MET's vocabulary does not
    // distinguish hail, so emitting them would be inventing a meteorological
    // claim out of the word "heavy".
    const QList<int> codes = metNoWeatherCodes();
    QVERIFY(codes.contains(95));
    QVERIFY(!codes.contains(96));
    QVERIFY(!codes.contains(99));

    QCOMPARE(parseSymbolCode(QStringLiteral("heavyrainandthunder")).code, WeatherCode(95));
    QCOMPARE(parseSymbolCode(QStringLiteral("lightsnowshowersandthunder")).code, WeatherCode(95));
}

void TestMetNo::anUnknownSymbolIsNoCodeRatherThanAGuess()
{
    QVERIFY(!parseSymbolCode(QStringLiteral("meteorshower_day")).isValid());
    QVERIFY(!parseSymbolCode(QString()).isValid());

    // The variant is still read, because it is structural rather than a guess
    // about the weather — but there is no code, so nothing draws.
    QCOMPARE(parseSymbolCode(QStringLiteral("meteorshower_night")).isDay,
             std::optional<bool>(false));
}

// ---- the adaptation ---------------------------------------------------------------

void TestMetNo::theInstantsBecomeHourlyPointsInOrder()
{
    const Result<Forecast> parsed =
        MetNoForecastProvider::parse(fixture(), QTimeZone::UTC, kRecordedAt);
    QVERIFY2(parsed.hasValue(), qPrintable(parsed.error().toString()));

    const Forecast &forecast = parsed.value();
    QCOMPARE(forecast.hourly.size(), 90);

    // GeoJSON is [longitude, latitude, altitude]. Reading it as [lat, lon]
    // produces a forecast for a plausible place in the wrong hemisphere.
    QCOMPARE(forecast.coordinate.latitude, 43.7001);
    QCOMPARE(forecast.coordinate.longitude, -79.4163);
    QVERIFY(forecast.elevation.has_value());

    const HourlyPoint &first = forecast.hourly.constFirst();
    QCOMPARE(first.time, QDateTime(QDate(2026, 7, 31), QTime(9, 0), QTimeZone::UTC));
    QCOMPARE(*first.temperature, 18.7);
    QCOMPARE(*first.relativeHumidity, 55.0);
    QCOMPARE(*first.pressureMsl, 1015.0);
    QCOMPARE(*first.cloudCover, 1.6);
    QCOMPARE(*first.windDirection, 292.0);

    for (int i = 1; i < forecast.hourly.size(); ++i)
        QVERIFY(forecast.hourly.at(i).time > forecast.hourly.at(i - 1).time);

    // "Now" is the first timestep — Locationforecast has no separate
    // observation product, so a second request would be a second forecast.
    QCOMPARE(forecast.current.time, first.time);
    QCOMPARE(forecast.current.temperature, first.temperature);
}

void TestMetNo::windIsConvertedFromMetresPerSecond()
{
    const Result<Forecast> parsed =
        MetNoForecastProvider::parse(fixture(), QTimeZone::UTC, kRecordedAt);
    QVERIFY(parsed.hasValue());

    // 3.1 m/s in the payload; the domain is km/h, because Open-Meteo is asked
    // for km/h and one domain cannot hold two units.
    QCOMPARE(*parsed.value().hourly.constFirst().windSpeed, 3.1 * 3.6);
}

void TestMetNo::precipitationLandsOnTheHourItEnds()
{
    const QJsonDocument document = QJsonDocument::fromJson(fixture());
    const QJsonArray    timeseries =
        document.object().value(QStringLiteral("properties")).toObject()
            .value(QStringLiteral("timeseries")).toArray();

    const Result<Forecast> parsed =
        MetNoForecastProvider::parse(fixture(), QTimeZone::UTC, kRecordedAt);
    QVERIFY(parsed.hasValue());
    const Forecast &forecast = parsed.value();

    // The first point describes the hour before the forecast starts, which is
    // the past, which MET is not forecasting. Absent, not zero.
    QVERIFY(!forecast.hourly.constFirst().precipitation.has_value());
    QVERIFY(!forecast.hourly.constFirst().weatherCode.has_value());

    // Entry i's next_1_hours block covers [t, t+1h) and therefore belongs to the
    // point at t+1h — which is entry i+1's timestamp. Checked against the raw
    // payload rather than against a transcribed number, so this is an assertion
    // about the shift rather than about one value.
    int checked = 0;
    for (int i = 0; i + 1 < timeseries.size(); ++i) {
        const QJsonObject data =
            timeseries.at(i).toObject().value(QStringLiteral("data")).toObject();
        const QJsonValue block = data.value(QStringLiteral("next_1_hours"));
        if (!block.isObject())
            continue;

        const QJsonValue amount = block.toObject().value(QStringLiteral("details")).toObject()
                                      .value(QStringLiteral("precipitation_amount"));
        if (!amount.isDouble())
            continue;

        QVERIFY(forecast.hourly.at(i + 1).precipitation.has_value());
        QCOMPARE(*forecast.hourly.at(i + 1).precipitation, amount.toDouble());
        ++checked;
    }
    QVERIFY2(checked > 50, "the fixture has stopped carrying an hourly head");
}

void TestMetNo::theSixHourlyTailIsNotCountedTwice()
{
    const QJsonDocument document = QJsonDocument::fromJson(fixture());
    const QJsonArray    timeseries =
        document.object().value(QStringLiteral("properties")).toObject()
            .value(QStringLiteral("timeseries")).toArray();

    const Result<Forecast> parsed =
        MetNoForecastProvider::parse(fixture(), QTimeZone::UTC, kRecordedAt);
    QVERIFY(parsed.hasValue());
    const Forecast &forecast = parsed.value();

    // In the hourly head both blocks exist, and the 6-hour one covers six of
    // the hours the 1-hour blocks already account for. The 1-hour block wins;
    // using both would count the transition day's rain roughly twice.
    int overlapping = 0;
    for (int i = 0; i + 1 < timeseries.size(); ++i) {
        const QJsonObject data =
            timeseries.at(i).toObject().value(QStringLiteral("data")).toObject();
        if (!data.value(QStringLiteral("next_1_hours")).isObject()
            || !data.value(QStringLiteral("next_6_hours")).isObject())
            continue;

        const QJsonValue oneHour = data.value(QStringLiteral("next_1_hours")).toObject()
                                       .value(QStringLiteral("details")).toObject()
                                       .value(QStringLiteral("precipitation_amount"));
        if (!oneHour.isDouble())
            continue;

        QVERIFY(forecast.hourly.at(i + 1).precipitation.has_value());
        QCOMPARE(*forecast.hourly.at(i + 1).precipitation, oneHour.toDouble());
        ++overlapping;
    }
    QVERIFY2(overlapping > 10, "the fixture no longer has a head where both blocks exist");

    // And the tail is still populated, from the 6-hour blocks alone.
    QVERIFY(forecast.hourly.constLast().time
            > forecast.hourly.constFirst().time.addDays(8));
    int tailWithPrecipitation = 0;
    for (int i = forecast.hourly.size() - 12; i < forecast.hourly.size(); ++i) {
        if (forecast.hourly.at(i).precipitation.has_value())
            ++tailWithPrecipitation;
    }
    QVERIFY2(tailWithPrecipitation >= 10, "the six-hourly tail lost its precipitation");
}

void TestMetNo::theSeriesThinsAndTheTimestampsSaySo()
{
    // A consumer that assumes uniform hourly spacing draws the second half of
    // this forecast six times too wide. HourlyPoint::time is explicit for this
    // reason, and this test is what says the assumption is unsafe.
    const Result<Forecast> parsed =
        MetNoForecastProvider::parse(fixture(), QTimeZone::UTC, kRecordedAt);
    QVERIFY(parsed.hasValue());
    const Forecast &forecast = parsed.value();

    int hourlySteps    = 0;
    int sixHourlySteps = 0;
    for (int i = 1; i < forecast.hourly.size(); ++i) {
        const qint64 seconds =
            forecast.hourly.at(i - 1).time.secsTo(forecast.hourly.at(i).time);
        if (seconds == 3600)
            ++hourlySteps;
        else if (seconds == 6 * 3600)
            ++sixHourlySteps;
        else
            QFAIL(qPrintable(QStringLiteral("unexpected step of %1 s").arg(seconds)));
    }
    QVERIFY(hourlySteps > 50);
    QVERIFY(sixHourlySteps > 20);
}

void TestMetNo::dailyIsDerivedInTheZoneTheCallerSupplied()
{
    // MET has no daily product and no time zone. The rollup is derived, and the
    // zone it was derived in is recorded, so a UI formatting from
    // Forecast::timeZone is self-consistent whatever the caller passed.
    const QTimeZone toronto("America/Toronto");
    QVERIFY(toronto.isValid());

    const Result<Forecast> local =
        MetNoForecastProvider::parse(fixture(), toronto, kRecordedAt);
    const Result<Forecast> utc = MetNoForecastProvider::parse(fixture(), QTimeZone(), kRecordedAt);
    QVERIFY(local.hasValue() && utc.hasValue());

    QCOMPARE(local.value().timeZone.id(), toronto.id());
    QCOMPARE(utc.value().timeZone.id(), QTimeZone(QTimeZone::UTC).id());

    QVERIFY(!local.value().daily.isEmpty());

    QCOMPARE(local.value().daily.constFirst().date, QDate(2026, 7, 31));
    QCOMPARE(local.value().daily.size(), utc.value().daily.size());

    // The two groupings span the same ten calendar dates and disagree about
    // what happened on four of them, because a four-hour shift moves samples
    // across midnight. That is the whole argument for carrying a zone: the
    // dates would look identical in a screenshot and the highs would be wrong.
    int daysThatDiffer = 0;
    for (int i = 0; i < local.value().daily.size(); ++i) {
        const DailyPoint &here  = local.value().daily.at(i);
        const DailyPoint &there = utc.value().daily.at(i);
        QCOMPARE(here.date, there.date);
        if (here.temperatureMax != there.temperatureMax
            || here.temperatureMin != there.temperatureMin)
            ++daysThatDiffer;
    }
    QVERIFY2(daysThatDiffer > 0,
             "grouping by Toronto's midnight and by UTC's produced identical days, which means "
             "the zone is not being used");

    for (const DailyPoint &day : local.value().daily) {
        QVERIFY(day.temperatureMax.has_value());
        QVERIFY(day.temperatureMin.has_value());
        QVERIFY(*day.temperatureMax >= *day.temperatureMin);

        // Derived from what is there. Not fabricated from what is not:
        // Locationforecast has no sun product, so the arc has no endpoints.
        QVERIFY(!day.sunrise.isValid());
        QVERIFY(!day.sunset.isValid());
        QVERIFY(!day.uvIndexMax.has_value());
        QVERIFY(!day.windGustMax.has_value());
        QVERIFY(!day.precipitationProbabilityMax.has_value());
    }
}

// ---- the honest absences ------------------------------------------------------------

void TestMetNo::whatItCannotSupplyIsAbsentRatherThanZero()
{
    const Result<Forecast> parsed =
        MetNoForecastProvider::parse(fixture(), QTimeZone::UTC, kRecordedAt);
    QVERIFY(parsed.hasValue());

    for (const HourlyPoint &point : parsed.value().hourly) {
        QVERIFY(!point.apparentTemperature.has_value());
        QVERIFY(!point.dewPoint.has_value());
        QVERIFY(!point.windGust.has_value());
        QVERIFY(!point.precipitationProbability.has_value());
        QVERIFY(!point.rain.has_value());
        QVERIFY(!point.showers.has_value());
        QVERIFY(!point.snowfall.has_value());
        QVERIFY(!point.uvIndex.has_value());
        QVERIFY(!point.visibility.has_value());
    }

    QVERIFY(!parsed.value().current.windGust.has_value());
    QVERIFY(!parsed.value().current.apparentTemperature.has_value());
    QVERIFY(!parsed.value().current.uvIndex.has_value());
}

void TestMetNo::capabilitiesOmitEverythingCompactDoesNotCarry()
{
    HttpClient            client(&m_clock);
    MetNoForecastProvider provider(&client, &m_clock);

    const Capabilities capabilities = provider.capabilitiesAt(kToronto);

    // What it has.
    QVERIFY(capabilities.has(Capability::CurrentConditions));
    QVERIFY(capabilities.has(Capability::Hourly));
    QVERIFY(capabilities.has(Capability::Daily));
    QVERIFY(capabilities.has(Capability::Temperature));
    QVERIFY(capabilities.has(Capability::Humidity));
    QVERIFY(capabilities.has(Capability::Precipitation));
    QVERIFY(capabilities.has(Capability::Wind));
    QVERIFY(capabilities.has(Capability::Pressure));
    QVERIFY(capabilities.has(Capability::CloudCover));
    QVERIFY(capabilities.has(Capability::WeatherCode));

    // What it does not, one line per row the UI must hide. The list is the
    // same one in metnoforecastprovider.h, and the point of writing it twice is
    // that the header can be edited without the code and the code cannot be
    // edited without this.
    for (const Capability absent : { Capability::ApparentTemperature, Capability::DewPoint,
                                     Capability::WindGust, Capability::PrecipitationProbability,
                                     Capability::PrecipitationType, Capability::UvIndex,
                                     Capability::Visibility, Capability::SunTimes,
                                     Capability::Minutely15, Capability::Ensemble,
                                     Capability::ModelSelection, Capability::HistoricalArchive,
                                     Capability::AirQualityIndex, Capability::Pollutants,
                                     Capability::Ammonia, Capability::Pollen }) {
        QVERIFY2(capabilities.isKnownAbsent(absent), qPrintable(capabilityName(absent)));
    }
}

void TestMetNo::nothingIsUndeterminedBecauseNothingHereIsRegional()
{
    HttpClient            client(&m_clock);
    MetNoForecastProvider provider(&client, &m_clock);

    // The three-valued capability answer exists for CAMS's pollen. This product
    // is uniform worldwide, so it never uses the third value — and saying so
    // with a test is what stops somebody adding an "undetermined" here out of
    // symmetry.
    for (const Coordinate coord : { kToronto, Coordinate{ 59.9139, 10.7522 },
                                    Coordinate{ -33.8688, 151.2093 } }) {
        QVERIFY(provider.covers(coord));
        QCOMPARE(provider.capabilitiesAt(coord).undetermined(), CapabilityFlags());
    }
}

// ---- the wire ---------------------------------------------------------------------------

void TestMetNo::theRequestUsesLatAndLonTruncatedToFourDecimals()
{
    m_stub.enqueue(StubResponse::ok(fixture()));

    HttpClient            client(&m_clock);
    MetNoForecastProvider provider(&client, &m_clock);
    provider.setBaseUrl(QUrl(m_stub.baseUrl() + QStringLiteral("/weatherapi/locationforecast/2.0/"
                                                               "compact")));

    ForecastRequest request;
    request.coord = Coordinate{ 43.70011234, -79.41634567 };

    auto future = provider.fetchForecast(request);
    QVERIFY(QTest::qWaitFor([&future]() { return future.isFinished(); }, 5000));
    QVERIFY2(future.result().hasValue(), qPrintable(future.result().error().toString()));

    const StubRequest &sent = m_stub.requests().constFirst();
    const QString      target = QUrl::fromPercentEncoding(sent.target);

    // `lat`/`lon`, and four decimals, which MET's terms ask for by name. The
    // provider does neither of those things itself — HttpRequest carries the
    // parameter names and HttpClient does the rounding, in one place, so a
    // provider cannot forget.
    QVERIFY2(target.contains(QLatin1String("lat=43.7001")), qPrintable(target));
    QVERIFY2(target.contains(QLatin1String("lon=-79.4163")), qPrintable(target));
    QVERIFY2(!target.contains(QLatin1String("latitude=")), qPrintable(target));

    // Their terms require an identifying User-Agent. This is the one provider
    // whose licence hangs off it.
    QCOMPARE(sent.header(QByteArrayLiteral("user-agent")), HttpClient::userAgent());
}

void TestMetNo::aUnitChangeIsAParseErrorRatherThanAWrongNumber()
{
    // The failure mode this guards: MET switches wind to knots, every gale
    // renders as a stiff breeze, and nothing anywhere errors. A unit we do not
    // recognise is a bug report instead.
    QJsonDocument document = QJsonDocument::fromJson(fixture());
    QJsonObject   root     = document.object();
    QJsonObject   properties = root.value(QStringLiteral("properties")).toObject();
    QJsonObject   meta       = properties.value(QStringLiteral("meta")).toObject();
    QJsonObject   units      = meta.value(QStringLiteral("units")).toObject();

    units.insert(QStringLiteral("wind_speed"), QStringLiteral("knots"));
    meta.insert(QStringLiteral("units"), units);
    properties.insert(QStringLiteral("meta"), meta);
    root.insert(QStringLiteral("properties"), properties);

    const Result<Forecast> parsed = MetNoForecastProvider::parse(
        QJsonDocument(root).toJson(QJsonDocument::Compact), QTimeZone::UTC, kRecordedAt);

    QVERIFY(!parsed.hasValue());
    QCOMPARE(parsed.errorKind(), ErrorKind::Parse);
    QVERIFY2(parsed.error().message().contains(QLatin1String("knots")),
             qPrintable(parsed.error().message()));
}

void TestMetNo::forbiddenIsATypedErrorAndTheProviderStopsAsking()
{
    // Their terms say a generic User-Agent earns a 403. It did not, when this
    // was tested against the live service — and the hard stop stays anyway,
    // because the difference between a client that is refused and one that is
    // banned is whether it kept asking.
    m_stub.enqueue(StubResponse::withStatus(403));

    HttpClient            client(&m_clock);
    MetNoForecastProvider provider(&client, &m_clock);
    provider.setBaseUrl(QUrl(m_stub.baseUrl() + QStringLiteral("/compact")));

    ForecastRequest request;
    request.coord = kToronto;

    auto first = provider.fetchForecast(request);
    QVERIFY(QTest::qWaitFor([&first]() { return first.isFinished(); }, 5000));
    QVERIFY(!first.result().hasValue());
    QCOMPARE(first.result().errorKind(), ErrorKind::UserAgentRejected);
    QCOMPARE(m_stub.requestCount(), 1);

    // Not retried, and the next request never reaches the socket. This is what
    // lets ProviderRegistry's chain fall through to the next provider at the
    // speed of a function call rather than a round trip.
    auto second = provider.fetchForecast(request);
    QVERIFY(QTest::qWaitFor([&second]() { return second.isFinished(); }, 5000));
    QVERIFY(!second.result().hasValue());
    QCOMPARE(second.result().errorKind(), ErrorKind::ProviderDisabled);
    QCOMPARE(m_stub.requestCount(), 1);
}

QTEST_MAIN(TestMetNo)
#include "tst_metno.moc"
