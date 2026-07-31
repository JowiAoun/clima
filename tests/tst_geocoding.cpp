// SPDX-FileCopyrightText: 2026 Jowi Aoun
// SPDX-License-Identifier: GPL-3.0-or-later
//
// Forward geocoding: the parser against recorded responses, and the provider
// against a loopback stub.
//
// The fixtures in tests/fixtures/geocoding/ were recorded from
// geocoding-api.open-meteo.com on 2026-07-31 and are the exact bytes the
// service sent:
//
//   search-kigali.json      five results, the first of which is the capital,
//                           with admin1, country and timezone all present.
//   search-toronto.json     five results, all named Toronto, in five different
//                           first-level divisions. This is the case that
//                           makes admin1 load-bearing rather than decorative.
//   search-no-results.json  what a search that matched nothing ACTUALLY
//                           returns, which is not an empty array — it is an
//                           object with no `results` key at all. Recorded on
//                           purpose, because a parser written against the
//                           documentation would get this wrong and appear to
//                           work.
//   get-toronto.json        /v1/get?id=6167865, one bare object, no envelope.
//
// No network: tests/support/networkguard.h makes that a property of the
// process, and OpenMeteoGeocoder::setBaseUrl points the provider at the stub.

#include "libclima/cache/cachestore.h"
#include "libclima/core/clock.h"
#include "libclima/net/httpclient.h"
#include "libclima/places/placesearchmodel.h"
#include "libclima/providers/geocoding/geocodingparser.h"
#include "libclima/providers/geocoding/openmeteogeocoder.h"
#include "support/httpstub.h"
#include "support/networkguard.h"

#include <QFile>
#include <QSignalSpy>
#include <QTest>

using namespace clima;
using namespace std::chrono_literals;

namespace {

QByteArray fixture(const QString &name)
{
    QFile file(QStringLiteral(CLIMA_SOURCE_DIR) + QStringLiteral("/tests/fixtures/geocoding/")
               + name);
    if (!file.open(QIODevice::ReadOnly))
        return {};
    return file.readAll();
}

GeocodeQuery query(const QString &name)
{
    GeocodeQuery q;
    q.name = name;
    q.count = 5;
    return q;
}

// Runs the event loop until the future finishes, then reads it. Every call
// below is against loopback and finishes in single-digit milliseconds; the
// timeout is a deadlock bound, not a latency one.
template <typename T>
Result<T> await(QFuture<Result<T>> future)
{
    if (!QTest::qWaitFor([&future] { return future.isFinished(); }, 5000))
        return Error(ErrorKind::Timeout, QStringLiteral("the future never finished"));
    if (future.resultCount() == 0)
        return Error(ErrorKind::Cancelled, QStringLiteral("the future produced no result"));
    return future.result();
}

} // namespace

class TestGeocoding : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void initTestCase();
    void init();
    void cleanup();

    // ---- the parser, with no client and no event loop ----------------------
    void searchResultsBecomePlaces();
    void twelveTorontosAreToldApartByAdmin1();
    void aSearchThatMatchedNothingIsAnEmptyListAndNotAnError();
    void malformedBytesAreAParseError();
    void getReturnsOneBarePlace();
    void aRowWithNoIdIsDroppedRatherThanStored();

    // ---- the provider ------------------------------------------------------
    void aShortQuerySendsNothing();
    void aSearchIsCachedForSevenDaysAndThenRefetched();
    void aStaleAnswerBeatsAFailedRequest();
    void aFailedRequestWithNothingCachedIsAnError();
    void resolveReadsOnePlaceById();
    void theGeocoderHasItsOwnProviderIdSoA403DoesNotStopForecasts();
    void bothAttributionsAreCarried();

    // ---- the search model --------------------------------------------------
    void typingSendsOneRequestPerPauseAndNotPerKeystroke();
    void clearingTheBoxEmptiesTheListAtOnce();

private:
    FrozenClock                      m_clock{ QDateTime(QDate(2026, 7, 31), QTime(9, 0),
                                                        QTimeZone::UTC) };
    HttpStub                        *m_stub = nullptr;
    HttpClient                      *m_http = nullptr;
    CacheStore                      *m_cache = nullptr;
    OpenMeteoGeocoder               *m_geocoder = nullptr;
};

void TestGeocoding::initTestCase()
{
    NetworkGuard::install();
}

void TestGeocoding::init()
{
    NetworkGuard::clearAttempts();

    m_stub = new HttpStub(this);
    QVERIFY(m_stub->listen());

    m_http = new HttpClient(&m_clock, this);
    m_cache = new CacheStore(&m_clock);
    QVERIFY(m_cache->open(QStringLiteral(":memory:")).hasValue());

    m_geocoder = new OpenMeteoGeocoder(m_http, m_cache, &m_clock, this);
    m_geocoder->setBaseUrl(QUrl(m_stub->baseUrl()));
}

void TestGeocoding::cleanup()
{
    QVERIFY2(NetworkGuard::externalAttempts().isEmpty(),
             qPrintable(NetworkGuard::externalAttempts().join(QStringLiteral(", "))));

    delete m_geocoder;
    m_geocoder = nullptr;
    delete m_cache;
    m_cache = nullptr;
    delete m_http;
    m_http = nullptr;
    delete m_stub;
    m_stub = nullptr;
}

// ---- the parser -------------------------------------------------------------

void TestGeocoding::searchResultsBecomePlaces()
{
    const Result<QList<Place>> parsed =
        parseGeocodingSearch(fixture(QStringLiteral("search-kigali.json")));
    QVERIFY2(parsed.hasValue(), qPrintable(parsed.error().toString()));
    QCOMPARE(parsed.value().size(), 5);

    const Place &kigali = parsed.value().first();
    QCOMPARE(kigali.geonamesId, Q_INT64_C(202061));
    QCOMPARE(kigali.name, QStringLiteral("Kigali"));
    QCOMPARE(kigali.admin1, QStringLiteral("Kigali"));
    QCOMPARE(kigali.country, QStringLiteral("Rwanda"));
    QCOMPARE(kigali.countryCode, QStringLiteral("RW"));
    QCOMPARE(kigali.timezone, QStringLiteral("Africa/Kigali"));
    QCOMPARE(kigali.coordinate.latitude, -1.94995);
    QCOMPARE(kigali.coordinate.longitude, 30.05885);
    QVERIFY(kigali.elevationMetres.has_value());
    QCOMPARE(*kigali.elevationMetres, 1542.0);

    // Not saved yet, so no local id. The GeoNames id is the one it arrives
    // with and the one that survives a rename upstream.
    QCOMPARE(kigali.id, Q_INT64_C(0));
}

void TestGeocoding::twelveTorontosAreToldApartByAdmin1()
{
    const Result<QList<Place>> parsed =
        parseGeocodingSearch(fixture(QStringLiteral("search-toronto.json")));
    QVERIFY(parsed.hasValue());
    QCOMPARE(parsed.value().size(), 5);

    // Five rows, one name, five different answers. Without admin1 a search
    // popover shows "Toronto" five times and the user picks one at random.
    QStringList labels;
    for (const Place &place : parsed.value())
        labels.append(place.label());

    QCOMPARE(labels,
             (QStringList{ QStringLiteral("Toronto, Ontario"), QStringLiteral("Toronto, Ohio"),
                           QStringLiteral("Toronto, Kansas"),
                           QStringLiteral("Toronto, South Dakota"),
                           QStringLiteral("Toronto, Iowa") }));

    QCOMPARE(parsed.value().first().geonamesId, Q_INT64_C(6167865));
    QCOMPARE(parsed.value().at(1).country, QStringLiteral("United States"));
}

void TestGeocoding::aSearchThatMatchedNothingIsAnEmptyListAndNotAnError()
{
    // The recorded bytes are `{"generationtime_ms":0.44560432}` — no `results`
    // key of any kind. A person who typed three letters that match nothing has
    // not caused a failure, and a search box that showed them a network error
    // would be lying about what happened.
    const Result<QList<Place>> parsed =
        parseGeocodingSearch(fixture(QStringLiteral("search-no-results.json")));
    QVERIFY2(parsed.hasValue(), qPrintable(parsed.error().toString()));
    QVERIFY(parsed.value().isEmpty());
}

void TestGeocoding::malformedBytesAreAParseError()
{
    QCOMPARE(parseGeocodingSearch(QByteArrayLiteral("{not json")).errorKind(), ErrorKind::Parse);
    QCOMPARE(parseGeocodingSearch(QByteArrayLiteral("[1,2,3]")).errorKind(), ErrorKind::Parse);

    // `results` present and not an array is a real breakage rather than an
    // empty answer, and is reported as one.
    QCOMPARE(parseGeocodingSearch(QByteArrayLiteral(R"({"results":"nope"})")).errorKind(),
             ErrorKind::Parse);
}

void TestGeocoding::getReturnsOneBarePlace()
{
    const Result<Place> parsed = parseGeocodingPlace(fixture(QStringLiteral("get-toronto.json")));
    QVERIFY2(parsed.hasValue(), qPrintable(parsed.error().toString()));
    QCOMPARE(parsed.value().geonamesId, Q_INT64_C(6167865));
    QCOMPARE(parsed.value().admin1, QStringLiteral("Ontario"));

    QCOMPARE(parseGeocodingPlace(QByteArrayLiteral("{}")).errorKind(), ErrorKind::NotFound);
}

void TestGeocoding::aRowWithNoIdIsDroppedRatherThanStored()
{
    // One unusable row must not fail the whole search: nine of the ten places
    // the user could have meant is better than an error. But it must not be
    // *kept* either — a Place with identity zero is a place nothing can ever
    // reconcile, and the places table's unique index has an opinion about it.
    const QByteArray mixed = QByteArrayLiteral(
        R"({"results":[{"name":"Nowhere","latitude":1,"longitude":2},)"
        R"({"id":6167865,"name":"Toronto","latitude":43.70643,"longitude":-79.39864}]})");

    const Result<QList<Place>> parsed = parseGeocodingSearch(mixed);
    QVERIFY(parsed.hasValue());
    QCOMPARE(parsed.value().size(), 1);
    QCOMPARE(parsed.value().first().name, QStringLiteral("Toronto"));

    // No `country` in that row, so the parser fills it from the country code
    // the way the offline reverse geocoder does — one mapping, so the two
    // paths cannot disagree about how a country is spelled.
    QVERIFY(parsed.value().first().country.isEmpty());
}

// ---- the provider -----------------------------------------------------------

void TestGeocoding::aShortQuerySendsNothing()
{
    // One character is below Open-Meteo's own minimum and would match a large
    // fraction of the planet. The future is finished before it is returned.
    const Result<QList<Place>> single = await(m_geocoder->search(query(QStringLiteral("K"))));
    QVERIFY(single.hasValue());
    QVERIFY(single.value().isEmpty());

    const Result<QList<Place>> blank = await(m_geocoder->search(query(QString())));
    QVERIFY(blank.hasValue());

    QCOMPARE(m_stub->requestCount(), 0);
}

void TestGeocoding::aSearchIsCachedForSevenDaysAndThenRefetched()
{
    m_stub->enqueue(StubResponse::ok(fixture(QStringLiteral("search-kigali.json"))));

    const Result<QList<Place>> first = await(m_geocoder->search(query(QStringLiteral("Kigali"))));
    QVERIFY2(first.hasValue(), qPrintable(first.error().toString()));
    QCOMPARE(first.value().size(), 5);
    QCOMPARE(m_stub->requestCount(), 1);

    // The query and the language are in the URL, so they are in the RequestKey,
    // so §4.5's "keyed by query+lang" is structural rather than remembered.
    const QByteArray target = m_stub->requests().first().target;
    QVERIFY2(target.contains(QByteArrayLiteral("name=Kigali")), target.constData());
    QVERIFY2(target.contains(QByteArrayLiteral("language=en")), target.constData());

    // Six days later: still fresh, still no request.
    m_clock.advance(6 * 24h);
    const Result<QList<Place>> cached = await(m_geocoder->search(query(QStringLiteral("Kigali"))));
    QVERIFY(cached.hasValue());
    QCOMPARE(cached.value().size(), 5);
    QCOMPARE(m_stub->requestCount(), 1);

    // Two days after that, the seven-day TTL has passed.
    m_clock.advance(2 * 24h);
    const Result<QList<Place>> refetched =
        await(m_geocoder->search(query(QStringLiteral("Kigali"))));
    QVERIFY(refetched.hasValue());
    QCOMPARE(m_stub->requestCount(), 2);
}

void TestGeocoding::aStaleAnswerBeatsAFailedRequest()
{
    m_stub->enqueue(StubResponse::ok(fixture(QStringLiteral("search-kigali.json"))));
    QVERIFY(await(m_geocoder->search(query(QStringLiteral("Kigali")))).hasValue());

    // Past the TTL, and now the service is unreachable. 404 rather than 500
    // because a 404 is not retryable, so this test is about the fallback and
    // not about the backoff schedule.
    m_clock.advance(8 * 24h);
    m_stub->enqueue(StubResponse::withStatus(404));

    const Result<QList<Place>> stale = await(m_geocoder->search(query(QStringLiteral("Kigali"))));

    // §4.5 marks geocoding stale-while-revalidate, and §4.1 principle 1 says
    // the UI must never show an empty screen because an API is down. A
    // week-old answer to "Kigali" is the same answer.
    QVERIFY2(stale.hasValue(), qPrintable(stale.error().toString()));
    QCOMPARE(stale.value().size(), 5);
    QCOMPARE(stale.value().first().name, QStringLiteral("Kigali"));
    QCOMPARE(m_stub->requestCount(), 2);
}

void TestGeocoding::aFailedRequestWithNothingCachedIsAnError()
{
    m_stub->enqueue(StubResponse::withStatus(404));

    const Result<QList<Place>> failed = await(m_geocoder->search(query(QStringLiteral("Kigali"))));
    QVERIFY(!failed.hasValue());
    QCOMPARE(failed.errorKind(), ErrorKind::NotFound);
}

void TestGeocoding::resolveReadsOnePlaceById()
{
    m_stub->enqueue(StubResponse::ok(fixture(QStringLiteral("get-toronto.json"))));

    const Result<Place> resolved = await(m_geocoder->resolve(6167865));
    QVERIFY2(resolved.hasValue(), qPrintable(resolved.error().toString()));
    QCOMPARE(resolved.value().name, QStringLiteral("Toronto"));
    QCOMPARE(resolved.value().admin1, QStringLiteral("Ontario"));

    QVERIFY(m_stub->requests().first().target.contains(QByteArrayLiteral("id=6167865")));

    // A search and a get are different endpoints, so they are different cache
    // keys, so one cannot answer the other.
    QCOMPARE(m_stub->requestCount(), 1);

    const Result<Place> nonsense = await(m_geocoder->resolve(0));
    QVERIFY(!nonsense.hasValue());
    QCOMPARE(nonsense.errorKind(), ErrorKind::NotFound);
    QCOMPARE(m_stub->requestCount(), 1);
}

void TestGeocoding::theGeocoderHasItsOwnProviderIdSoA403DoesNotStopForecasts()
{
    // HttpClient disables a provider for the life of the process on a 403 and
    // never sends again. The unit it disables is the provider id, so the
    // geocoder having its own means that a refusal from
    // geocoding-api.open-meteo.com does not take api.open-meteo.com's
    // forecasts down with it. A user who cannot search for a city should still
    // get the weather for the city they already saved.
    QCOMPARE(m_geocoder->id(), QStringLiteral("open-meteo-geocoding"));
    QVERIFY(m_geocoder->id() != QStringLiteral("open-meteo"));

    m_stub->enqueue(StubResponse::withStatus(403));
    QSignalSpy disabled(m_http, &HttpClient::providerDisabled);

    const Result<QList<Place>> refused = await(m_geocoder->search(query(QStringLiteral("Kigali"))));
    QVERIFY(!refused.hasValue());
    QCOMPARE(refused.errorKind(), ErrorKind::UserAgentRejected);

    QCOMPARE(disabled.count(), 1);
    QCOMPARE(disabled.first().first().toString(), QStringLiteral("open-meteo-geocoding"));
    QVERIFY(m_http->isProviderDisabled(QStringLiteral("open-meteo-geocoding")));
    QVERIFY(!m_http->isProviderDisabled(QStringLiteral("open-meteo")));
}

void TestGeocoding::bothAttributionsAreCarried()
{
    // A licence obligation, not a nicety: docs/02-data-sources.md §2.9 lists
    // Open-Meteo and GeoNames separately, both CC-BY 4.0, and About → Data
    // sources has to show both. Open-Meteo hosts and serves the index;
    // GeoNames is whose data it is.
    const QStringList credits = m_geocoder->attribution();
    QCOMPARE(credits.size(), 2);
    QVERIFY(credits.at(0).contains(QStringLiteral("Open-Meteo")));
    QVERIFY(credits.at(1).contains(QStringLiteral("GeoNames")));
    for (const QString &credit : credits)
        QVERIFY2(credit.contains(QStringLiteral("CC BY 4.0")), qPrintable(credit));
}

// ---- the search model -------------------------------------------------------

void TestGeocoding::typingSendsOneRequestPerPauseAndNotPerKeystroke()
{
    PlaceSearchModel model(m_geocoder);

    // Short enough that the test does not sleep for a quarter of a second, long
    // enough that six setQuery calls in a row land inside one window.
    model.setDebounceInterval(60);
    model.setMaximumResults(5);

    m_stub->enqueue(StubResponse::ok(fixture(QStringLiteral("search-kigali.json"))));

    // "Kigali", a character at a time, the way a person types it.
    const QString typed = QStringLiteral("Kigali");
    for (int i = 1; i <= typed.size(); ++i)
        model.setQuery(typed.left(i));

    QVERIFY(QTest::qWaitFor([&model] { return model.count() > 0; }, 5000));

    // Six keystrokes, five of them above the minimum length, one request.
    QCOMPARE(m_stub->requestCount(), 1);
    QCOMPARE(model.count(), 5);
    // "Kigali", not "Kigali, Kigali": Kigali's first-level division is also
    // called Kigali, and Place::label() drops an admin1 that repeats the name.
    QCOMPARE(model.data(model.index(0, 0), PlaceSearchModel::LabelRole).toString(),
             QStringLiteral("Kigali"));
    QCOMPARE(model.data(model.index(0, 0), PlaceSearchModel::RegionRole).toString(),
             QStringLiteral("Kigali, Rwanda"));
    QVERIFY(!model.isSearching());
    QVERIFY(model.errorMessage().isEmpty());
}

void TestGeocoding::clearingTheBoxEmptiesTheListAtOnce()
{
    PlaceSearchModel model(m_geocoder);
    model.setDebounceInterval(0);
    model.setMaximumResults(5);

    m_stub->enqueue(StubResponse::ok(fixture(QStringLiteral("search-kigali.json"))));

    model.setQuery(QStringLiteral("Kigali"));
    QVERIFY(QTest::qWaitFor([&model] { return model.count() > 0; }, 5000));

    // Below the minimum the list empties immediately rather than after the
    // debounce: a box that has just been cleared must not go on showing
    // yesterday's answer, and no request is sent for one character.
    const int before = m_stub->requestCount();
    model.setQuery(QStringLiteral("K"));
    QCOMPARE(model.count(), 0);

    model.setQuery(QString());
    QCOMPARE(model.count(), 0);
    QCOMPARE(m_stub->requestCount(), before);
}

QTEST_MAIN(TestGeocoding)
#include "tst_geocoding.moc"
