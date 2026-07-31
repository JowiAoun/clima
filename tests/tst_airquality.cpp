// SPDX-FileCopyrightText: 2026 Jowi Aoun
// SPDX-License-Identifier: GPL-3.0-or-later
//
// The air-quality provider, against two recorded responses and a loopback
// server. No network — tests/support/networkguard.h makes that a property of
// the process.
//
// The fixtures are the whole point of this file. They were recorded from the
// live service on the same afternoon, three forecast days each, at two places
// chosen because they differ in exactly one way:
//
//     tests/fixtures/airquality/toronto.json    72 hours. Six pollen series and
//                                               ammonia are null 72/72.
//     tests/fixtures/airquality/berlin.json     72 hours. The same seven series
//                                               are null 0/72 — and four of the
//                                               six pollens are 0.0 throughout,
//                                               because it is July.
//
// So Toronto proves the gate closes, Berlin proves it opens, and Berlin's
// out-of-season zeroes prove it is testing the right thing: a gate written as
// "any value above zero" passes the first two and fails here.

#include "libclima/core/clock.h"
#include "libclima/domain/airquality.h"
#include "libclima/net/httpclient.h"
#include "libclima/providers/airquality/openmeteoairqualityprovider.h"
#include "support/httpstub.h"
#include "support/networkguard.h"

#include <QFile>
#include <QTest>
#include <QUrl>

using namespace clima;

namespace {

QByteArray fixture(const QString &name)
{
    QFile file(QStringLiteral(CLIMA_SOURCE_DIR) + QStringLiteral("/tests/fixtures/airquality/")
               + name);
    if (!file.open(QIODevice::ReadOnly))
        return {};
    return file.readAll();
}

const Coordinate kToronto{ 43.7001, -79.4163 };
const Coordinate kBerlin{ 52.5200, 13.4050 };

const QDateTime kRecordedAt{ QDate(2026, 7, 31), QTime(9, 13, 0), QTimeZone::UTC };

} // namespace

class TestAirQuality : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void initTestCase();
    void init();
    void cleanup();

    // the gate
    void torontoHasNoPollenAndBerlinDoes();
    void zeroIsAMeasurementAndNullIsNot();
    void ammoniaIsGatedByTheSameRuleAsPollen();
    void aGatedSeriesLeavesNoValueBehindToMisread();

    // the indices
    void bothIndicesAreNonNullOnBothContinents();
    void theEuropeanIndexIsTheMaximumOfItsPublishedSubIndices();
    void theDominantPollutantIsTheArgmaxOfThoseSubIndices();
    void theLocalComputationAgreesOnGasesAndNotOnParticulates();
    void carbonMonoxideCanNeverBeDominant();

    // capabilities
    void pollenIsUndeterminedBeforeTheFirstFetch();
    void torontoSettlesToKnownAbsentAndBerlinToAvailable();
    void theVerdictIsRememberedPerCamsCell();
    void theIndicesAreNeverUndetermined();

    // the request, and failure
    void theRequestAsksForEverythingThisFileParses();
    void aTruncatedPayloadIsATypedParseError();

private:
    Result<AirQuality> fetchThrough(const QByteArray &body, Coordinate coord,
                                    OpenMeteoAirQualityProvider &provider);

    HttpStub    m_stub;
    FrozenClock m_clock{ kRecordedAt };
};

void TestAirQuality::initTestCase()
{
    NetworkGuard::install();

    // A fixture that did not load reads as a payload with no hourly axis, which
    // is a parse failure with a confusing message. Fail here instead.
    QVERIFY2(!fixture(QStringLiteral("toronto.json")).isEmpty(), "toronto fixture missing");
    QVERIFY2(!fixture(QStringLiteral("berlin.json")).isEmpty(), "berlin fixture missing");
}

void TestAirQuality::init()
{
    m_stub.reset();
    QVERIFY2(m_stub.listen(), "could not listen on loopback");
    NetworkGuard::clearAttempts();
}

void TestAirQuality::cleanup()
{
    QCOMPARE(NetworkGuard::externalAttempts(), QStringList());
}

Result<AirQuality> TestAirQuality::fetchThrough(const QByteArray &body, Coordinate coord,
                                                OpenMeteoAirQualityProvider &provider)
{
    m_stub.enqueue(StubResponse::ok(body));
    provider.setBaseUrl(QUrl(m_stub.baseUrl() + QStringLiteral("/v1/air-quality")));

    ForecastRequest request;
    request.coord = coord;

    auto future = provider.fetchAirQuality(request);
    if (!QTest::qWaitFor([&future]() { return future.isFinished(); }, 5000))
        return Error(ErrorKind::Timeout, QStringLiteral("the fetch never finished"));
    return future.result();
}

// ---- the gate ----------------------------------------------------------------

void TestAirQuality::torontoHasNoPollenAndBerlinDoes()
{
    const Result<AirQuality> toronto =
        OpenMeteoAirQualityProvider::parse(fixture(QStringLiteral("toronto.json")), kRecordedAt);
    QVERIFY2(toronto.hasValue(), qPrintable(toronto.error().toString()));

    const Result<AirQuality> berlin =
        OpenMeteoAirQualityProvider::parse(fixture(QStringLiteral("berlin.json")), kRecordedAt);
    QVERIFY2(berlin.hasValue(), qPrintable(berlin.error().toString()));

    QVERIFY(!toronto.value().hasPollen);
    QVERIFY(berlin.value().hasPollen);

    // And the consequence: no hour of the Toronto series carries a pollen map
    // at all. Not an empty map — no map. A card driven by this cannot draw.
    for (const AirQualityPoint &point : toronto.value().hourly)
        QVERIFY(!point.pollen.has_value());

    for (const AirQualityPoint &point : berlin.value().hourly) {
        QVERIFY(point.pollen.has_value());
        QCOMPARE(point.pollen->size(), int(PollenSpecies::Count));
    }
}

void TestAirQuality::zeroIsAMeasurementAndNullIsNot()
{
    // The test that keeps the gate from being rewritten as "any value above
    // zero". In a July fixture, Berlin's alder, birch, olive and ragweed are
    // 0.0 for every hour — out of season, measured, real — and a gate that read
    // zero as absence would hide a working card for two thirds of the year.
    const Result<AirQuality> berlin =
        OpenMeteoAirQualityProvider::parse(fixture(QStringLiteral("berlin.json")), kRecordedAt);
    QVERIFY(berlin.hasValue());

    double alderTotal = 0.0;
    int    alderHours = 0;
    for (const AirQualityPoint &point : berlin.value().hourly) {
        QVERIFY(point.pollen.has_value());
        const auto alder = point.pollen->constFind(PollenSpecies::Alder);
        QVERIFY(alder != point.pollen->cend());
        alderTotal += *alder;
        ++alderHours;
    }

    QCOMPARE(alderHours, berlin.value().hourly.size());
    QCOMPARE(alderTotal, 0.0);          // every sample is zero
    QVERIFY(berlin.value().hasPollen);  // and the gate is open anyway
}

void TestAirQuality::ammoniaIsGatedByTheSameRuleAsPollen()
{
    // Ammonia is the second Europe-only series, and it is gated by the same
    // all-null test rather than by a second copy of a bounding box. If the two
    // ever disagree, one of them was written by hand.
    const Result<AirQuality> toronto =
        OpenMeteoAirQualityProvider::parse(fixture(QStringLiteral("toronto.json")), kRecordedAt);
    const Result<AirQuality> berlin =
        OpenMeteoAirQualityProvider::parse(fixture(QStringLiteral("berlin.json")), kRecordedAt);
    QVERIFY(toronto.hasValue() && berlin.hasValue());

    QVERIFY(!toronto.value().hasAmmonia);
    QVERIFY(berlin.value().hasAmmonia);

    for (const AirQualityPoint &point : toronto.value().hourly)
        QVERIFY(!point.ammonia.has_value());

    bool anyBerlinAmmonia = false;
    for (const AirQualityPoint &point : berlin.value().hourly)
        anyBerlinAmmonia = anyBerlinAmmonia || point.ammonia.has_value();
    QVERIFY(anyBerlinAmmonia);
}

void TestAirQuality::aGatedSeriesLeavesNoValueBehindToMisread()
{
    // The failure this whole design exists to prevent: a zero where there is no
    // data. Nothing in the Toronto series may be readable as a pollen or
    // ammonia number, by any route, including a default-constructed one.
    const Result<AirQuality> toronto =
        OpenMeteoAirQualityProvider::parse(fixture(QStringLiteral("toronto.json")), kRecordedAt);
    QVERIFY(toronto.hasValue());

    QVERIFY(!toronto.value().current.pollen.has_value());
    for (const AirQualityPoint &point : toronto.value().hourly) {
        QVERIFY(!point.pollen.has_value());
        QVERIFY(!point.ammonia.has_value());
    }
}

// ---- the indices --------------------------------------------------------------

void TestAirQuality::bothIndicesAreNonNullOnBothContinents()
{
    // docs/02-data-sources.md §2.6 says both indices are global; this is the
    // check, because the Air Quality tab being global and the pollen card being
    // European is the distinction the whole feature rests on.
    for (const QString &name : { QStringLiteral("toronto.json"), QStringLiteral("berlin.json") }) {
        const Result<AirQuality> parsed =
            OpenMeteoAirQualityProvider::parse(fixture(name), kRecordedAt);
        QVERIFY2(parsed.hasValue(), qPrintable(name));

        QVERIFY(parsed.value().current.europeanAqi.has_value());
        QVERIFY(parsed.value().current.usAqi.has_value());

        for (const AirQualityPoint &point : parsed.value().hourly) {
            QVERIFY2(point.europeanAqi.has_value(), qPrintable(name));
            QVERIFY2(point.usAqi.has_value(), qPrintable(name));
        }
    }
}

void TestAirQuality::theEuropeanIndexIsTheMaximumOfItsPublishedSubIndices()
{
    // The identity the dominant-pollutant design rests on. 144 hours across two
    // continents; if this ever fails, either CAMS changed the definition or we
    // are reading the wrong series, and both of those change which pollutant
    // gets named on the card.
    int checked = 0;
    for (const QString &name : { QStringLiteral("toronto.json"), QStringLiteral("berlin.json") }) {
        const Result<AirQuality> parsed =
            OpenMeteoAirQualityProvider::parse(fixture(name), kRecordedAt);
        QVERIFY(parsed.hasValue());

        for (const AirQualityPoint &point : parsed.value().hourly) {
            QVERIFY(!point.europeanSubIndices.isEmpty());
            QVERIFY(point.europeanAqi.has_value());

            double highest = -1.0;
            for (const double subIndex : point.europeanSubIndices)
                highest = qMax(highest, subIndex);

            QCOMPARE(int(qRound(highest)), *point.europeanAqi);
            ++checked;
        }
    }
    QCOMPARE(checked, 144);
}

void TestAirQuality::theDominantPollutantIsTheArgmaxOfThoseSubIndices()
{
    const Result<AirQuality> berlin =
        OpenMeteoAirQualityProvider::parse(fixture(QStringLiteral("berlin.json")), kRecordedAt);
    QVERIFY(berlin.hasValue());

    for (const AirQualityPoint &point : berlin.value().hourly) {
        const std::optional<Pollutant> dominant = point.dominantPollutant();
        QVERIFY(dominant.has_value());

        // It is the argmax…
        const double winning = point.europeanSubIndices.value(*dominant);
        for (const double subIndex : point.europeanSubIndices)
            QVERIFY(subIndex <= winning);

        // …and it is the one the published index is reporting.
        QCOMPARE(int(qRound(winning)), *point.europeanAqi);

        // The card shows the concentration, not the sub-index. Those are
        // different numbers and confusing them is how "PM2.5 51 µg/m³" ends up
        // under a reading of 10.4.
        QVERIFY(point.dominantConcentration().has_value());
        QCOMPARE(*point.dominantConcentration(), point.pollutants.value(*dominant));
        QVERIFY(!pollutantUnit(*dominant).isEmpty());
    }
}

void TestAirQuality::theLocalComputationAgreesOnGasesAndNotOnParticulates()
{
    // The measurement that decided the design. libclima/domain/airquality.h
    // asserts it in prose; here it is as a number, so that anybody tempted to
    // delete the request for the published sub-indices and compute them locally
    // finds out what it would cost before shipping it.
    //
    // The EAQI defines the gases on hourly concentrations, which a forecast
    // response contains, and the particulates on a 24-hour running mean, which
    // it does not.
    const Result<AirQuality> berlin =
        OpenMeteoAirQualityProvider::parse(fixture(QStringLiteral("berlin.json")), kRecordedAt);
    QVERIFY(berlin.hasValue());

    double worstGas       = 0.0;
    double worstParticate = 0.0;

    for (const AirQualityPoint &point : berlin.value().hourly) {
        for (auto it = point.europeanSubIndices.cbegin(); it != point.europeanSubIndices.cend();
             ++it) {
            const std::optional<double> local =
                europeanSubIndex(it.key(), point.pollutants.value(it.key()));
            QVERIFY(local.has_value());
            const double error = qAbs(*local - it.value());

            if (it.key() == Pollutant::Pm2_5 || it.key() == Pollutant::Pm10)
                worstParticate = qMax(worstParticate, error);
            else
                worstGas = qMax(worstGas, error);
        }
    }

    // The gases: within the rounding step of an integer-published index.
    QVERIFY2(worstGas <= 0.51, qPrintable(QStringLiteral("gas error %1").arg(worstGas)));

    // The particulates: not close, and not close by enough to change a band.
    QVERIFY2(worstParticate > 5.0,
             qPrintable(QStringLiteral("particulate error %1 — if this has become small, the "
                                       "published sub-indices may no longer be needed")
                            .arg(worstParticate)));
}

void TestAirQuality::carbonMonoxideCanNeverBeDominant()
{
    // CO is reported in the hundreds of µg/m³ while SO2 is reported in ones, so
    // the largest concentration is CO in every hour of both fixtures. It is
    // never the dominant pollutant, because the EAQI does not define a CO
    // sub-index — which is the difference between comparing sub-indices and
    // comparing concentrations, made visible.
    for (const QString &name : { QStringLiteral("toronto.json"), QStringLiteral("berlin.json") }) {
        const Result<AirQuality> parsed =
            OpenMeteoAirQualityProvider::parse(fixture(name), kRecordedAt);
        QVERIFY(parsed.hasValue());

        for (const AirQualityPoint &point : parsed.value().hourly) {
            QVERIFY(point.pollutants.contains(Pollutant::CarbonMonoxide));

            double largestConcentration = -1.0;
            std::optional<Pollutant> byConcentration;
            for (auto it = point.pollutants.cbegin(); it != point.pollutants.cend(); ++it) {
                if (it.value() > largestConcentration) {
                    largestConcentration = it.value();
                    byConcentration      = it.key();
                }
            }

            QVERIFY(byConcentration.has_value());
            QVERIFY(*byConcentration == Pollutant::CarbonMonoxide);

            const std::optional<Pollutant> dominant = point.dominantPollutant();
            QVERIFY(dominant.has_value());
            QVERIFY(*dominant != Pollutant::CarbonMonoxide);
        }

        QVERIFY(!europeanSubIndex(Pollutant::CarbonMonoxide, 204.0).has_value());
    }
}

// ---- capabilities -------------------------------------------------------------

void TestAirQuality::pollenIsUndeterminedBeforeTheFirstFetch()
{
    HttpClient                  client(&m_clock);
    OpenMeteoAirQualityProvider provider(&client, &m_clock);

    const Capabilities before = provider.capabilitiesAt(kBerlin);

    // Not available, and not known-absent. A UI that read "no" here would hide
    // Berlin's pollen card for the two seconds before the payload landed and
    // then pop it in.
    QVERIFY(!before.has(Capability::Pollen));
    QVERIFY(before.isUndetermined(Capability::Pollen));
    QVERIFY(!before.isKnownAbsent(Capability::Pollen));

    QVERIFY(before.isUndetermined(Capability::Ammonia));
}

void TestAirQuality::torontoSettlesToKnownAbsentAndBerlinToAvailable()
{
    HttpClient                  client(&m_clock);
    OpenMeteoAirQualityProvider provider(&client, &m_clock);

    const Result<AirQuality> toronto =
        fetchThrough(fixture(QStringLiteral("toronto.json")), kToronto, provider);
    QVERIFY2(toronto.hasValue(), qPrintable(toronto.error().toString()));

    const Capabilities here = provider.capabilitiesAt(kToronto);
    QVERIFY(here.isKnownAbsent(Capability::Pollen));
    QVERIFY(!here.isUndetermined(Capability::Pollen));
    QVERIFY(here.isKnownAbsent(Capability::Ammonia));

    // …and the tab that does work here still does.
    QVERIFY(here.has(Capability::AirQualityIndex));
    QVERIFY(here.has(Capability::Pollutants));

    const Result<AirQuality> berlin =
        fetchThrough(fixture(QStringLiteral("berlin.json")), kBerlin, provider);
    QVERIFY2(berlin.hasValue(), qPrintable(berlin.error().toString()));

    const Capabilities there = provider.capabilitiesAt(kBerlin);
    QVERIFY(there.has(Capability::Pollen));
    QVERIFY(there.has(Capability::Ammonia));

    // Two places, two verdicts, one provider. The Toronto answer did not move.
    QVERIFY(provider.capabilitiesAt(kToronto).isKnownAbsent(Capability::Pollen));
}

void TestAirQuality::theVerdictIsRememberedPerCamsCell()
{
    HttpClient                  client(&m_clock);
    OpenMeteoAirQualityProvider provider(&client, &m_clock);

    QCOMPARE(provider.rememberedVerdictCount(), 0);

    QVERIFY(fetchThrough(fixture(QStringLiteral("toronto.json")), kToronto, provider).hasValue());
    QCOMPARE(provider.rememberedVerdictCount(), 1);
    QCOMPARE(m_stub.requestCount(), 1);

    // A point 300 metres away is the same CAMS cell — the verdict is keyed at
    // one decimal, ~11 km, which is the grid CAMS Europe is published on. The
    // answer is decided without a second request, which is the whole reason
    // the verdict is remembered at a coarser resolution than the request is
    // made at.
    const Coordinate nextStreet{ kToronto.latitude + 0.0027, kToronto.longitude + 0.0027 };
    QVERIFY(provider.capabilitiesAt(nextStreet).isKnownAbsent(Capability::Pollen));
    QCOMPARE(provider.rememberedVerdictCount(), 1);
    QCOMPARE(m_stub.requestCount(), 1);

    // A point in another cell is undetermined again rather than assumed.
    const Coordinate acrossTheLake{ kToronto.latitude - 0.4, kToronto.longitude };
    QVERIFY(provider.capabilitiesAt(acrossTheLake).isUndetermined(Capability::Pollen));
}

void TestAirQuality::theIndicesAreNeverUndetermined()
{
    // The other half of the three-valued design: a capability that does not
    // depend on the payload must never be reported as unknown, or the tab bar
    // flickers for something that was always going to be there.
    HttpClient                  client(&m_clock);
    OpenMeteoAirQualityProvider provider(&client, &m_clock);

    for (const Coordinate coord : { kToronto, kBerlin }) {
        const Capabilities capabilities = provider.capabilitiesAt(coord);
        QVERIFY(capabilities.has(Capability::AirQualityIndex));
        QVERIFY(capabilities.has(Capability::Pollutants));
        QVERIFY(capabilities.has(Capability::CurrentConditions));
        QVERIFY(capabilities.has(Capability::Hourly));
        QVERIFY(!capabilities.isUndetermined(Capability::AirQualityIndex));
    }
}

// ---- the request, and failure ---------------------------------------------------

void TestAirQuality::theRequestAsksForEverythingThisFileParses()
{
    HttpClient                  client(&m_clock);
    OpenMeteoAirQualityProvider provider(&client, &m_clock);

    QVERIFY(fetchThrough(fixture(QStringLiteral("berlin.json")), kBerlin, provider).hasValue());
    QCOMPARE(m_stub.requestCount(), 1);

    const QString target =
        QUrl::fromPercentEncoding(m_stub.requests().constFirst().target);

    // Both scales, because the app shows both.
    QVERIFY2(target.contains(QLatin1String("european_aqi")), qPrintable(target));
    QVERIFY2(target.contains(QLatin1String("us_aqi")), qPrintable(target));

    // The published sub-indices, which is what makes the dominant pollutant
    // right rather than approximately right.
    QVERIFY2(target.contains(QLatin1String("european_aqi_pm2_5")), qPrintable(target));
    QVERIFY2(target.contains(QLatin1String("european_aqi_ozone")), qPrintable(target));

    // Every pollutant and every pollen species, generated from the enums rather
    // than typed — so this assertion is really about the generator.
    for (int i = 0; i < int(Pollutant::Count); ++i) {
        QVERIFY2(target.contains(pollutantId(static_cast<Pollutant>(i))),
                 qPrintable(pollutantId(static_cast<Pollutant>(i))));
    }
    for (int i = 0; i < int(PollenSpecies::Count); ++i) {
        QVERIFY2(target.contains(pollenSpeciesId(static_cast<PollenSpecies>(i))),
                 qPrintable(pollenSpeciesId(static_cast<PollenSpecies>(i))));
    }

    QVERIFY2(target.contains(QLatin1String("ammonia")), qPrintable(target));
    QVERIFY2(target.contains(QLatin1String("dust")), qPrintable(target));
    QVERIFY2(target.contains(QLatin1String("aerosol_optical_depth")), qPrintable(target));
    QVERIFY2(target.contains(QLatin1String("timezone=auto")), qPrintable(target));

    // Four decimals, from HttpClient, without this provider doing anything.
    QVERIFY2(target.contains(QLatin1String("latitude=52.5200")), qPrintable(target));
    QVERIFY2(target.contains(QLatin1String("longitude=13.4050")), qPrintable(target));
}

void TestAirQuality::aTruncatedPayloadIsATypedParseError()
{
    // §4.4: never a partial success. Half a payload is an Error(Parse), not an
    // AirQuality with three hours in it — the registry's chain branches on the
    // kind, and it cannot branch on a success that is secretly a failure.
    const QByteArray whole = fixture(QStringLiteral("berlin.json"));
    const Result<AirQuality> truncated =
        OpenMeteoAirQualityProvider::parse(whole.left(whole.size() / 2), kRecordedAt);

    QVERIFY(!truncated.hasValue());
    QCOMPARE(truncated.errorKind(), ErrorKind::Parse);

    // And an in-band refusal, which Open-Meteo answers with rather than with a
    // shapeless body. The reason is usually a parameter name we got wrong,
    // which is a bug here and not an outage there.
    const Result<AirQuality> refused = OpenMeteoAirQualityProvider::parse(
        QByteArrayLiteral(R"({"error":true,"reason":"Cannot initialize Variable from 'nope'"})"),
        kRecordedAt);
    QVERIFY(!refused.hasValue());
    QCOMPARE(refused.errorKind(), ErrorKind::Parse);
    QVERIFY(refused.error().message().contains(QLatin1String("nope")));
}

QTEST_MAIN(TestAirQuality)
#include "tst_airquality.moc"
