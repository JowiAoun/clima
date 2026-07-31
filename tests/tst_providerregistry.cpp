// SPDX-FileCopyrightText: 2026 Jowi Aoun
// SPDX-License-Identifier: GPL-3.0-or-later
//
// Routing, attribution, and the fallback path actually being walked.
//
// docs/06-roadmap.md's argument for building the fallback in the same commit as
// the interface is that an untested fallback is not a fallback — the competing
// app's documented bug is that its second provider ran for the first time in
// production. So the central test here is not "does the chain sort correctly";
// it is `aFailedPrimaryFallsThroughToTheRealMetNorwayProvider`, which forces a
// typed failure out of the primary and then asserts that the real MET Norway
// adapter, parsing the real recorded fixture, served the request.
//
// The other half is docs/08-risks.md R12: a provider without its credit is
// refused at registration rather than rendered blank on the About screen. The
// About screen is generated from this registry, so "not registered" is the only
// enforcement that reaches every screen at once.

#include "libclima/core/clock.h"
#include "libclima/net/httpclient.h"
#include "libclima/providers/metno/metnoforecastprovider.h"
#include "libclima/providers/registry.h"
#include "support/httpstub.h"
#include "support/networkguard.h"

#include <QFile>
#include <QPromise>
#include <QSignalSpy>
#include <QTest>

using namespace clima;

namespace {

const Coordinate kToronto{ 43.7001, -79.4163 };
const Coordinate kDenver{ 39.7392, -104.9903 };
const Coordinate kBerlin{ 52.5200, 13.4050 };

const QDateTime kRecordedAt{ QDate(2026, 7, 31), QTime(9, 13, 51), QTimeZone::UTC };

QByteArray metNoFixture()
{
    QFile file(QStringLiteral(CLIMA_SOURCE_DIR)
               + QStringLiteral("/tests/fixtures/metno/toronto.json"));
    if (!file.open(QIODevice::ReadOnly))
        return {};
    return file.readAll();
}

Attribution completeAttribution(const QString &name)
{
    Attribution credit;
    credit.name        = name;
    credit.creditLine  = QStringLiteral("Data by %1").arg(name);
    credit.homepage    = QUrl(QStringLiteral("http://127.0.0.1/%1").arg(name));
    credit.licenceName = QStringLiteral("CC BY 4.0");
    credit.licenceUrl  = QUrl(QStringLiteral("http://127.0.0.1/licence"));
    return credit;
}

// A provider that answers with whatever it was told to answer with, without
// touching a socket. It is not a mock of MET Norway — the real MET Norway
// provider is used for that below — it is a way to put a *specific typed
// failure* at the head of a chain, which is the input the fallback loop is
// defined in terms of and which a real provider cannot be asked for on demand.
class ScriptedForecastProvider : public IForecastProvider
{
public:
    ScriptedForecastProvider(QString id, Result<Forecast> answer)
        : m_id(std::move(id))
        , m_answer(std::move(answer))
    {
    }

    [[nodiscard]] QString id() const override { return m_id; }
    [[nodiscard]] QString displayName() const override { return m_id; }
    [[nodiscard]] Attribution attribution() const override { return m_attribution; }
    [[nodiscard]] bool covers(Coordinate coord) const override
    {
        return m_region == Region::Other ? coord.isValid() : regionContains(m_region, coord);
    }
    [[nodiscard]] Capabilities capabilitiesAt(Coordinate) const override
    {
        return m_capabilities;
    }

    QFuture<Result<Forecast>> fetchForecast(const ForecastRequest &) override
    {
        ++m_calls;
        QPromise<Result<Forecast>> promise;
        promise.start();
        promise.addResult(m_answer);
        promise.finish();
        return promise.future();
    }

    void setAttribution(Attribution credit) { m_attribution = std::move(credit); }
    void setRegion(Region region) { m_region = region; }
    void setCapabilities(Capabilities capabilities) { m_capabilities = capabilities; }

    [[nodiscard]] int calls() const { return m_calls; }

private:
    QString          m_id;
    Result<Forecast> m_answer;
    Attribution      m_attribution = completeAttribution(QStringLiteral("Scripted"));
    Region           m_region      = Region::Other;
    Capabilities     m_capabilities{ Capability::Hourly | Capability::Temperature };
    int              m_calls = 0;
};

Result<Forecast> aForecastFrom(const QString &providerId)
{
    Forecast forecast;
    forecast.providerId = providerId;
    forecast.coordinate = kToronto;
    forecast.current.time = kRecordedAt;
    forecast.current.temperature = 21.0;

    HourlyPoint point;
    point.time        = kRecordedAt;
    point.temperature = 21.0;
    forecast.hourly.append(point);

    return forecast;
}

Error typedFailure(ErrorKind kind, const QString &message)
{
    return Error(kind, message);
}

} // namespace

class TestProviderRegistry : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void initTestCase();
    void init();
    void cleanup();

    // R12
    void aProviderWithoutItsCreditIsRefused();
    void theRejectionNamesTheMissingField();
    void aRefusedProviderIsNotInAnyChainOrCredit();
    void theRealProvidersAllCarryACompleteAttribution();
    void oneSourceServingTwoProductsIsCreditedOnce();

    // routing
    void theChainIsOrderedByPriority();
    void aProviderThatDoesNotCoverThePlaceIsNotInItsChain();
    void aNationalProviderLeadsOnlyWhereItApplies();
    void duplicateIdsAreRefused();
    void anEmptyChainReportsNothingAvailable();

    // the fallback loop
    void aFailedPrimaryFallsThroughToTheRealMetNorwayProvider();
    void aDisabledPrimaryFallsThroughWithoutTouchingTheNetwork();
    void aHealthyPrimaryIsTheOnlyOneAsked();
    void whenEveryProviderFailsThePrimarysErrorSurvives();
    void aCancelledRequestIsNotHandedToTheNextProvider();

    // capabilities through the registry
    void capabilitiesFollowTheServingProviderAndNotTheUnion();

private:
    HttpStub    m_stub;
    FrozenClock m_clock{ kRecordedAt };
};

void TestProviderRegistry::initTestCase()
{
    NetworkGuard::install();
    QVERIFY2(!metNoFixture().isEmpty(), "met.no fixture missing");
}

void TestProviderRegistry::init()
{
    m_stub.reset();
    QVERIFY2(m_stub.listen(), "could not listen on loopback");
    NetworkGuard::clearAttempts();
}

void TestProviderRegistry::cleanup()
{
    QCOMPARE(NetworkGuard::externalAttempts(), QStringList());
}

// ---- R12: attribution ------------------------------------------------------------

void TestProviderRegistry::aProviderWithoutItsCreditIsRefused()
{
    ProviderRegistry         registry;
    ScriptedForecastProvider provider(QStringLiteral("anonymous"),
                                      aForecastFrom(QStringLiteral("anonymous")));
    provider.setAttribution({});

    const Status added = registry.addForecastProvider(&provider, 100);

    QVERIFY2(!added, "a provider with an empty Attribution was accepted — R12 is unenforced");
    QCOMPARE(added.errorKind(), ErrorKind::Unsupported);
}

void TestProviderRegistry::theRejectionNamesTheMissingField()
{
    // The author of a new provider is the person who reads this message.
    // "Attribution is incomplete" sends them to the interface; the field name
    // sends them to their own file.
    ProviderRegistry registry;

    Attribution partial = completeAttribution(QStringLiteral("Halfway"));
    partial.licenceUrl  = QUrl();

    ScriptedForecastProvider provider(QStringLiteral("halfway"),
                                      aForecastFrom(QStringLiteral("halfway")));
    provider.setAttribution(partial);

    const Status added = registry.addForecastProvider(&provider, 100);
    QVERIFY(!added);
    QVERIFY2(added.error().message().contains(QLatin1String("licenceUrl")),
             qPrintable(added.error().message()));
    QVERIFY2(added.error().message().contains(QLatin1String("halfway")),
             qPrintable(added.error().message()));
}

void TestProviderRegistry::aRefusedProviderIsNotInAnyChainOrCredit()
{
    // The point of refusing at registration rather than filtering at render: an
    // uncredited provider's *data* must not reach a screen either.
    ProviderRegistry registry;

    ScriptedForecastProvider anonymous(QStringLiteral("anonymous"),
                                       aForecastFrom(QStringLiteral("anonymous")));
    anonymous.setAttribution({});
    QVERIFY(!registry.addForecastProvider(&anonymous, 0));

    QVERIFY(registry.forecastChain(kToronto).isEmpty());
    QVERIFY(registry.providers().isEmpty());
    QVERIFY(registry.attributions().isEmpty());
}

void TestProviderRegistry::theRealProvidersAllCarryACompleteAttribution()
{
    // Not a hypothetical. The providers that actually ship go through the same
    // gate, and this is the test that fails when somebody adds the third one.
    HttpClient            client(&m_clock);
    MetNoForecastProvider metNo(&client, &m_clock);

    ProviderRegistry registry;
    const Status added = registry.addForecastProvider(&metNo, 200);
    QVERIFY2(added, qPrintable(added.error().message()));

    const QList<Attribution> credits = registry.attributions();
    QCOMPARE(credits.size(), 1);

    for (const Attribution &credit : credits) {
        QVERIFY2(credit.isComplete(), qPrintable(credit.firstMissingField()));
        QVERIFY(!credit.creditLine.isEmpty());
        QVERIFY(credit.licenceUrl.isValid());
    }

    // docs/02-data-sources.md §2.9 requires the model owners behind an
    // aggregator to be named, not just the aggregator.
    QVERIFY(!credits.constFirst().upstream.isEmpty());
}

void TestProviderRegistry::oneSourceServingTwoProductsIsCreditedOnce()
{
    // Open-Meteo is the forecast provider and the air-quality provider on two
    // hosts under one id. The About screen credits a source once; a duplicated
    // row reads as a bug and, worse, as two licences.
    class AirQualityStub : public IAirQualityProvider
    {
    public:
        [[nodiscard]] QString id() const override { return QStringLiteral("twofold"); }
        [[nodiscard]] QString displayName() const override { return QStringLiteral("Twofold"); }
        [[nodiscard]] Attribution attribution() const override
        {
            return completeAttribution(QStringLiteral("Twofold"));
        }
        [[nodiscard]] bool covers(Coordinate coord) const override { return coord.isValid(); }
        [[nodiscard]] Capabilities capabilitiesAt(Coordinate) const override { return {}; }
        QFuture<Result<AirQuality>> fetchAirQuality(const ForecastRequest &) override
        {
            return {};
        }
    };

    ProviderRegistry         registry;
    ScriptedForecastProvider forecast(QStringLiteral("twofold"),
                                      aForecastFrom(QStringLiteral("twofold")));
    forecast.setAttribution(completeAttribution(QStringLiteral("Twofold")));
    AirQualityStub airQuality;

    QVERIFY(registry.addForecastProvider(&forecast, 100));
    QVERIFY(registry.addAirQualityProvider(&airQuality, 100));

    QCOMPARE(registry.attributions().size(), 1);
    QCOMPARE(registry.providers().size(), 1);
}

// ---- routing ------------------------------------------------------------------------

void TestProviderRegistry::theChainIsOrderedByPriority()
{
    ProviderRegistry registry;

    ScriptedForecastProvider fallback(QStringLiteral("fallback"),
                                      aForecastFrom(QStringLiteral("fallback")));
    ScriptedForecastProvider primary(QStringLiteral("primary"),
                                     aForecastFrom(QStringLiteral("primary")));

    // Registered in the wrong order on purpose: the chain is decided by
    // priority, not by who was added first.
    QVERIFY(registry.addForecastProvider(&fallback, 200));
    QVERIFY(registry.addForecastProvider(&primary, 100));

    const QList<IForecastProvider *> chain = registry.forecastChain(kToronto);
    QCOMPARE(chain.size(), 2);
    QCOMPARE(chain.at(0)->id(), QStringLiteral("primary"));
    QCOMPARE(chain.at(1)->id(), QStringLiteral("fallback"));
}

void TestProviderRegistry::aProviderThatDoesNotCoverThePlaceIsNotInItsChain()
{
    ProviderRegistry registry;

    ScriptedForecastProvider american(QStringLiteral("us-only"),
                                      aForecastFrom(QStringLiteral("us-only")));
    american.setRegion(Region::UnitedStates);

    QVERIFY(registry.addForecastProvider(&american, 0));

    // Not tried and failed — absent. §4.4's ∅ case, which the UI must render as
    // a hidden feature rather than a broken one.
    QVERIFY(registry.forecastChain(kBerlin).isEmpty());
    QCOMPARE(registry.forecastChain(kDenver).size(), 1);
    QCOMPARE(american.calls(), 0);
}

void TestProviderRegistry::aNationalProviderLeadsOnlyWhereItApplies()
{
    // §4.4's "if US → nws ; else …", without the word "US" appearing in the
    // registry. covers() plus a priority is the whole mechanism.
    ProviderRegistry registry;

    ScriptedForecastProvider american(QStringLiteral("us-only"),
                                      aForecastFrom(QStringLiteral("us-only")));
    american.setRegion(Region::UnitedStates);
    ScriptedForecastProvider global(QStringLiteral("global"),
                                    aForecastFrom(QStringLiteral("global")));

    QVERIFY(registry.addForecastProvider(&global, 100));
    QVERIFY(registry.addForecastProvider(&american, 0));

    const QList<IForecastProvider *> denver = registry.forecastChain(kDenver);
    QCOMPARE(denver.size(), 2);
    QCOMPARE(denver.at(0)->id(), QStringLiteral("us-only"));

    const QList<IForecastProvider *> berlin = registry.forecastChain(kBerlin);
    QCOMPARE(berlin.size(), 1);
    QCOMPARE(berlin.at(0)->id(), QStringLiteral("global"));

    // And the boxes themselves, since a provider's covers() will lean on them.
    QCOMPARE(regionsFor(kDenver), QList<Region>{ Region::UnitedStates });
    QCOMPARE(regionsFor(kBerlin), QList<Region>{ Region::Europe });
    QCOMPARE(regionsFor(Coordinate{ -33.8688, 151.2093 }), QList<Region>());

    // Toronto is in both North American boxes, and that is the correct answer
    // rather than a defect. The border runs through two Great Lakes; no
    // rectangle follows it, so the box says "plausibly, ask them" and the
    // service says whether it actually has data. registry.h argues why a
    // single-winner regionFor() would be wrong in three different ways.
    QCOMPARE(regionsFor(kToronto),
             (QList<Region>{ Region::UnitedStates, Region::Canada }));
    QVERIFY(regionContains(Region::Canada, kToronto));
    QVERIFY(!regionContains(Region::Canada, kDenver));
}

void TestProviderRegistry::duplicateIdsAreRefused()
{
    ProviderRegistry         registry;
    ScriptedForecastProvider first(QStringLiteral("twice"), aForecastFrom(QStringLiteral("twice")));
    ScriptedForecastProvider second(QStringLiteral("twice"),
                                    aForecastFrom(QStringLiteral("twice")));

    QVERIFY(registry.addForecastProvider(&first, 100));
    QVERIFY(!registry.addForecastProvider(&second, 200));
    QCOMPARE(registry.forecastChain(kToronto).size(), 1);
}

void TestProviderRegistry::anEmptyChainReportsNothingAvailable()
{
    ProviderRegistry registry;

    QCOMPARE(registry.forecastCapabilitiesAt(kToronto).available(), CapabilityFlags());
    QCOMPARE(registry.forecastCapabilitiesAt(kToronto).undetermined(), CapabilityFlags());

    auto future = registry.fetchForecast({});
    QVERIFY(QTest::qWaitFor([&future]() { return future.isFinished(); }, 5000));
    QVERIFY(!future.result().hasValue());
    QCOMPARE(future.result().errorKind(), ErrorKind::Unsupported);
}

// ---- the fallback loop --------------------------------------------------------------

void TestProviderRegistry::aFailedPrimaryFallsThroughToTheRealMetNorwayProvider()
{
    // THE test this whole file exists for. The primary returns a typed
    // ServerError; the real MET Norway adapter, pointed at a loopback server
    // serving the real recorded payload, has to produce the forecast — parsed,
    // adapted, and labelled as having come from the fallback.
    m_stub.enqueue(StubResponse::ok(metNoFixture()));

    HttpClient            client(&m_clock);
    MetNoForecastProvider metNo(&client, &m_clock);
    metNo.setBaseUrl(QUrl(m_stub.baseUrl() + QStringLiteral("/compact")));

    ScriptedForecastProvider primary(
        QStringLiteral("primary"),
        typedFailure(ErrorKind::ServerError, QStringLiteral("502 from the primary")));

    ProviderRegistry registry;
    QVERIFY(registry.addForecastProvider(&primary, 100));
    QVERIFY(registry.addForecastProvider(&metNo, 200));

    QSignalSpy fellBack(&registry, &ProviderRegistry::servedByFallback);

    ForecastRequest request;
    request.coord    = kToronto;
    request.timeZone = QTimeZone("America/Toronto");

    auto future = registry.fetchForecast(request);
    QVERIFY(QTest::qWaitFor([&future]() { return future.isFinished(); }, 5000));

    const Result<ForecastAnswer> result = future.result();
    QVERIFY2(result.hasValue(), qPrintable(result.error().toString()));

    const ForecastAnswer &answer = result.value();

    // Served by the fallback, and it says so — which is what the UI's "showing
    // MET Norway" line reads, and the reason the flag exists at all.
    QCOMPARE(answer.servedBy, QStringLiteral("met-no"));
    QVERIFY(answer.fromFallback);
    QCOMPARE(answer.value.providerId, QStringLiteral("met-no"));

    // The primary was tried first, and why it failed is preserved rather than
    // swallowed.
    QCOMPARE(primary.calls(), 1);
    QCOMPARE(answer.failures.size(), 1);
    QCOMPARE(answer.failures.constFirst().providerId, QStringLiteral("primary"));
    QCOMPARE(answer.failures.constFirst().error.kind(), ErrorKind::ServerError);

    // And the data is real, adapted, and complete — not a placeholder that
    // happens to have the right provider id on it.
    QCOMPARE(answer.value.hourly.size(), 90);
    QVERIFY(answer.value.hourly.constFirst().temperature.has_value());
    QVERIFY(!answer.value.daily.isEmpty());
    QCOMPARE(answer.value.timeZone.id(), QTimeZone("America/Toronto").id());
    QCOMPARE(m_stub.requestCount(), 1);

    QCOMPARE(fellBack.count(), 1);
    QCOMPARE(fellBack.constFirst().at(1).toString(), QStringLiteral("met-no"));
    QCOMPARE(fellBack.constFirst().at(2).toString(), QStringLiteral("primary"));
}

void TestProviderRegistry::aDisabledPrimaryFallsThroughWithoutTouchingTheNetwork()
{
    // A 403 disables a provider for the process. HttpClient answers its later
    // requests with an already-finished ProviderDisabled, so the chain moves on
    // at the speed of a function call rather than a round trip — which is why
    // the hard stop and this loop are one design and not two.
    m_stub.enqueue(StubResponse::ok(metNoFixture()));

    HttpClient            client(&m_clock);
    MetNoForecastProvider metNo(&client, &m_clock);
    metNo.setBaseUrl(QUrl(m_stub.baseUrl() + QStringLiteral("/compact")));

    ScriptedForecastProvider primary(
        QStringLiteral("primary"),
        typedFailure(ErrorKind::ProviderDisabled,
                     QStringLiteral("disabled by an earlier 403")));

    ProviderRegistry registry;
    QVERIFY(registry.addForecastProvider(&primary, 100));
    QVERIFY(registry.addForecastProvider(&metNo, 200));

    ForecastRequest request;
    request.coord = kToronto;

    auto future = registry.fetchForecast(request);
    QVERIFY(QTest::qWaitFor([&future]() { return future.isFinished(); }, 5000));
    QVERIFY(future.result().hasValue());
    QCOMPARE(future.result().value().servedBy, QStringLiteral("met-no"));

    // One request on the wire: the fallback's. The disabled primary cost none.
    QCOMPARE(m_stub.requestCount(), 1);
}

void TestProviderRegistry::aHealthyPrimaryIsTheOnlyOneAsked()
{
    ProviderRegistry registry;

    ScriptedForecastProvider primary(QStringLiteral("primary"),
                                     aForecastFrom(QStringLiteral("primary")));
    ScriptedForecastProvider fallback(QStringLiteral("fallback"),
                                      aForecastFrom(QStringLiteral("fallback")));

    QVERIFY(registry.addForecastProvider(&primary, 100));
    QVERIFY(registry.addForecastProvider(&fallback, 200));

    ForecastRequest request;
    request.coord = kToronto;

    auto future = registry.fetchForecast(request);
    QVERIFY(QTest::qWaitFor([&future]() { return future.isFinished(); }, 5000));
    QVERIFY(future.result().hasValue());

    QCOMPARE(future.result().value().servedBy, QStringLiteral("primary"));
    QVERIFY(!future.result().value().fromFallback);
    QVERIFY(future.result().value().failures.isEmpty());
    QCOMPARE(fallback.calls(), 0);
}

void TestProviderRegistry::whenEveryProviderFailsThePrimarysErrorSurvives()
{
    // The primary's failure is the news; the fallback failing too is
    // corroboration. It also means a UserAgentRejected — which error.h says
    // must reach a human because it means our code is wrong — cannot be buried
    // under somebody else's timeout.
    ProviderRegistry registry;

    ScriptedForecastProvider primary(
        QStringLiteral("primary"),
        typedFailure(ErrorKind::UserAgentRejected, QStringLiteral("403: fix the User-Agent")));
    ScriptedForecastProvider fallback(
        QStringLiteral("fallback"),
        typedFailure(ErrorKind::Timeout, QStringLiteral("no answer in 20 s")));

    QVERIFY(registry.addForecastProvider(&primary, 100));
    QVERIFY(registry.addForecastProvider(&fallback, 200));

    ForecastRequest request;
    request.coord = kToronto;

    auto future = registry.fetchForecast(request);
    QVERIFY(QTest::qWaitFor([&future]() { return future.isFinished(); }, 5000));

    const Result<ForecastAnswer> result = future.result();
    QVERIFY(!result.hasValue());
    QCOMPARE(result.errorKind(), ErrorKind::UserAgentRejected);
    QCOMPARE(result.error().providerId(), QStringLiteral("primary"));

    // Both stories are in the message, so a log line explains the whole walk.
    QVERIFY2(result.error().message().contains(QLatin1String("fix the User-Agent")),
             qPrintable(result.error().message()));
    QVERIFY2(result.error().message().contains(QLatin1String("fallback")),
             qPrintable(result.error().message()));

    QCOMPARE(primary.calls(), 1);
    QCOMPARE(fallback.calls(), 1);
}

void TestProviderRegistry::aCancelledRequestIsNotHandedToTheNextProvider()
{
    // Cancellation is the caller changing its mind. Asking somebody else on
    // their behalf would be answering a question that was withdrawn — and it
    // would make a cancelled location switch fetch from every provider in the
    // chain before giving up.
    ProviderRegistry registry;

    ScriptedForecastProvider primary(
        QStringLiteral("primary"),
        typedFailure(ErrorKind::Cancelled, QStringLiteral("the caller moved on")));
    ScriptedForecastProvider fallback(QStringLiteral("fallback"),
                                      aForecastFrom(QStringLiteral("fallback")));

    QVERIFY(registry.addForecastProvider(&primary, 100));
    QVERIFY(registry.addForecastProvider(&fallback, 200));

    ForecastRequest request;
    request.coord = kToronto;

    auto future = registry.fetchForecast(request);
    QVERIFY(QTest::qWaitFor([&future]() { return future.isFinished(); }, 5000));

    QVERIFY(!future.result().hasValue());
    QCOMPARE(future.result().errorKind(), ErrorKind::Cancelled);
    QCOMPARE(fallback.calls(), 0);
}

// ---- capabilities through the registry --------------------------------------------------

void TestProviderRegistry::capabilitiesFollowTheServingProviderAndNotTheUnion()
{
    // A union would promise a UV tab while the primary is healthy and empty it
    // the moment the fallback took over — a tab that breaks exactly when
    // everything else is already going wrong.
    ProviderRegistry registry;

    ScriptedForecastProvider primary(QStringLiteral("primary"),
                                     aForecastFrom(QStringLiteral("primary")));
    primary.setCapabilities(Capabilities(Capability::Hourly | Capability::Temperature));

    ScriptedForecastProvider fallback(QStringLiteral("fallback"),
                                      aForecastFrom(QStringLiteral("fallback")));
    fallback.setCapabilities(Capabilities(Capability::Hourly | Capability::Temperature
                                          | Capability::UvIndex));

    QVERIFY(registry.addForecastProvider(&primary, 100));
    QVERIFY(registry.addForecastProvider(&fallback, 200));

    const Capabilities capabilities = registry.forecastCapabilitiesAt(kToronto);
    QVERIFY(capabilities.has(Capability::Temperature));
    QVERIFY2(!capabilities.has(Capability::UvIndex),
             "the registry unioned the chain's capabilities; it must report the serving "
             "provider's");
}

QTEST_MAIN(TestProviderRegistry)
#include "tst_providerregistry.moc"
