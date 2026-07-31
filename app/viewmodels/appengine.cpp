// SPDX-FileCopyrightText: 2026 Jowi Aoun
// SPDX-License-Identifier: GPL-3.0-or-later

#include "appengine.h"

#include "conditionsdata.h"
#include "forecastdata.h"

#include "libclima/cache/cachestore.h"
#include "libclima/core/clock.h"
#include "libclima/net/httpclient.h"
#include "libclima/places/devicelocator.h"
#include "libclima/places/locationcontroller.h"
#include "libclima/places/placesearchmodel.h"
#include "libclima/providers/airquality/openmeteoairqualityprovider.h"
#include "libclima/providers/geocoding/offlinereversegeocoder.h"
#include "libclima/providers/geocoding/openmeteogeocoder.h"
#include "libclima/providers/metno/metnoforecastprovider.h"
#include "libclima/providers/openmeteo/openmeteoforecastprovider.h"
#include "libclima/providers/registry.h"

#include <QCoreApplication>
#include <QEventLoop>
#include <QFutureWatcher>
#include <QProcessEnvironment>
#include <QTimeZone>
#include <QTimer>
#include <QUrl>

using namespace clima;

namespace {

// Eleven days: yesterday plus today plus nine. The ten-day strip is called
// "10 Day" and the day strip carries yesterday as its first card, which is one
// more than ten and the reason this is not 10.
constexpr int kForecastDays = 11;

// ---- forcing a failure, for review ------------------------------------------
//
// `CLIMA_FAIL_PROVIDERS=open-meteo` points the named providers at a port
// nothing listens on, so the chain fails over for real: a genuine connection
// refusal through the real HttpClient, the real backoff and the real registry.
//
// A flag that made the registry *skip* a provider would test a different thing
// — it would prove the chain can be reordered, not that it survives an outage —
// and the difference is exactly the one docs/06-roadmap.md is worried about
// when it argues for building the fallback early: "a path nobody can see is a
// path nobody tests".
//
// Environment rather than a command-line flag, because it belongs to the run
// and not to the product: `CLIMA_FAIL_PROVIDERS=open-meteo ./clima` is a
// sentence about this terminal.
QStringList forcedFailures()
{
    const QString value =
        QProcessEnvironment::systemEnvironment().value(QStringLiteral("CLIMA_FAIL_PROVIDERS"));
    if (value.isEmpty())
        return {};
    return value.split(QLatin1Char(','), Qt::SkipEmptyParts);
}

QUrl deadEnd()
{
    // Port 1 on the loopback interface. Refused immediately rather than timing
    // out, so a reviewer sees the fallback happen rather than waiting for it.
    return QUrl(QStringLiteral("http://127.0.0.1:1/"));
}

QVariantMap attributionMap(const Attribution &credit)
{
    return QVariantMap{
        { QStringLiteral("name"), credit.name },
        { QStringLiteral("creditLine"), credit.creditLine },
        { QStringLiteral("homepage"), credit.homepage.toString() },
        { QStringLiteral("licenceName"), credit.licenceName },
        { QStringLiteral("licenceUrl"), credit.licenceUrl.toString() },
        { QStringLiteral("upstream"), credit.upstream },
        { QStringLiteral("note"), credit.note },
    };
}

} // namespace

// ---- construction ----------------------------------------------------------------

AppEngine::AppEngine()
    : m_forecastData(new ForecastData(this))
    , m_conditionsData(new ConditionsData(this))
{
}

AppEngine::~AppEngine()
{
    delete m_reverse;
}

AppEngine *AppEngine::instance()
{
    static AppEngine engine;
    return &engine;
}

AppEngine *AppEngine::create(QQmlEngine *, QJSEngine *)
{
    AppEngine *engine = instance();
    QQmlEngine::setObjectOwnership(engine, QQmlEngine::CppOwnership);
    return engine;
}

void AppEngine::configure(const QString &fixtureName)
{
    // ---- 1. the clock ------------------------------------------------------
    //
    // First, and everything else is handed it. This is the single branch the
    // whole of fixture mode consists of.
    if (!fixtureName.isEmpty()) {
        m_fixture = fixtures::load(fixtureName);
        m_clock   = std::make_unique<FrozenClock>(m_fixture.recordedAt);
    } else {
        m_clock = std::make_unique<SystemClock>();
    }

    // ---- 2. storage --------------------------------------------------------
    m_cache = std::make_unique<CacheStore>(m_clock.get());
    const Status opened = m_cache->open(CacheStore::defaultDatabasePath());
    if (!opened) {
        // A cache that will not open is a slower app, not a broken one: every
        // read through payloadcache.h treats a closed store as a miss. Say so
        // once and carry on.
        qWarning("clima: the forecast cache could not be opened (%s); "
                 "this session will not survive going offline",
                 qPrintable(opened.error().toString()));
    }

    // ---- 3. the network ----------------------------------------------------
    m_http = std::make_unique<HttpClient>(m_clock.get());
    m_http->setValidatorStore(m_cache.get());

    m_registry = std::make_unique<ProviderRegistry>();

    if (isFixtureMode())
        buildFixtureProviders();
    else
        buildLiveProviders();

    registerProviders();

    // ---- 4. places ---------------------------------------------------------
    //
    // The geocoder is live even in fixture mode, and that is deliberate rather
    // than an oversight: searching for a place is a different product from
    // fetching its weather, a `--grab` never types in a search box, and a
    // reviewer who opens the picker under `--fixture` should still be able to
    // find Kigali. Nothing it returns can change what the fixture provider
    // answers, so no capture depends on it.
    m_geocoder = new OpenMeteoGeocoder(m_http.get(), m_cache.get(), m_clock.get(), this);
    // Not parented and not loaded here. Decoding the bundled index is about a
    // megabyte of allocation, and the cold-start budget is 400 ms for a screen
    // that comes out of SQLite — so the index is read the first time somebody
    // presses "use my location" and never on a start that does not.
    m_reverse  = new OfflineReverseGeocoder();
    m_places   = new LocationController(m_cache.get(), this);
    m_search   = new PlaceSearchModel(m_geocoder, this);
    m_locator  = DeviceLocator::create(this);

    m_places->load();

    connect(m_places, &LocationController::currentChanged, this, [this]() {
        Q_EMIT placeChanged();
        load();
    });
    connect(m_places, &LocationController::homeChanged, this, &AppEngine::placeChanged);

    connect(m_locator, &DeviceLocator::located, this,
            [this](const Coordinate &coordinate, double) {
                m_reverse->load();
                const Result<ReverseMatch> match = m_reverse->reverse(coordinate);
                if (!match) {
                    Q_EMIT locationFailed(tr("No town or city within range of where you are."));
                    return;
                }
                // The coordinate the device gave us, named by the bundled
                // index. The *name* is the index's; the coordinate stays the
                // device's, because a forecast for the centre of a city 30 km
                // away is a forecast for somewhere else.
                Place found      = match.value().place;
                found.coordinate = coordinate;
                m_places->addPlace(found, /*makeCurrent=*/true);
            });

    connect(m_locator, &DeviceLocator::failed, this,
            [this](DeviceLocator::Failure, const QString &reason) {
                Q_EMIT locationFailed(reason);
            });

    // ---- 5. the first place ------------------------------------------------
    //
    // A fixture always opens on its own recorded place, whatever is saved,
    // because the whole promise of `--fixture toronto` is that it produces the
    // same screen on every machine — and "whatever this developer last looked
    // at" is the one input that would break it.
    if (isFixtureMode()) {
        const int row = m_places->addPlace(m_fixture.place, /*makeCurrent=*/true);
        if (row < 0) {
            // The places table would not take it. Publish the forecast anyway:
            // a fixture with no row is still a fixture, and a blank window is
            // the worst possible outcome of a flag whose job is to produce a
            // picture.
            m_conditionsData->setPlace(m_fixture.place);
        }
    }

    load();
}

void AppEngine::buildLiveProviders()
{
    m_openMeteo = new OpenMeteoForecastProvider(m_http.get(), m_clock.get(), this);
    m_openMeteo->setCache(m_cache.get());

    m_metNo = new MetNoForecastProvider(m_http.get(), m_clock.get(), this);
    m_metNo->setCache(m_cache.get());

    m_openMeteoAq = new OpenMeteoAirQualityProvider(m_http.get(), m_clock.get(), this);
    m_openMeteoAq->setCache(m_cache.get());

    const QStringList fail = forcedFailures();
    if (fail.contains(m_openMeteo->id())) {
        m_openMeteo->setBaseUrl(deadEnd());
        m_openMeteoAq->setBaseUrl(deadEnd());
    }
    if (fail.contains(m_metNo->id()))
        m_metNo->setBaseUrl(deadEnd());
}

void AppEngine::buildFixtureProviders()
{
    m_fixtureForecast = new FixtureForecastProvider(m_fixture, this);
    m_fixtureAq       = new FixtureAirQualityProvider(m_fixture, this);
}

void AppEngine::registerProviders()
{
    // 100 for the global primary, 200 for the global fallback — the convention
    // ProviderRegistry::addForecastProvider documents, leaving 0 for a national
    // service when the alert providers land.
    const auto complain = [](const Status &status, const char *what) {
        if (!status) {
            // A refused registration is almost always a missing credit line,
            // which is a licence breach the registry is deliberately loud
            // about. Fatal here rather than a warning: a provider that did not
            // register cannot serve, and a silent one would come up as a blank
            // screen with no explanation.
            qFatal("clima: %s could not be registered: %s", what,
                   qPrintable(status.error().toString()));
        }
    };

    if (isFixtureMode()) {
        complain(m_registry->addForecastProvider(m_fixtureForecast, 100), "the fixture provider");
        complain(m_registry->addAirQualityProvider(m_fixtureAq, 100), "the fixture air quality");
        return;
    }

    complain(m_registry->addForecastProvider(m_openMeteo, 100), "Open-Meteo");
    complain(m_registry->addForecastProvider(m_metNo, 200), "MET Norway");
    complain(m_registry->addAirQualityProvider(m_openMeteoAq, 100), "Open-Meteo air quality");
}

// ---- the loop --------------------------------------------------------------------

void AppEngine::load()
{
    ++m_generation;

    m_forecast   = {};
    m_airQuality = {};
    m_problem.clear();
    m_sourceName.clear();
    m_fromFallback = false;

    // Published before anything is fetched, and that is not a formality: the
    // location bar reads `Detail.location`, and a frame drawn between choosing
    // a place and its forecast arriving would have a hole in it where the name
    // goes. The snapshot is empty; the place is not.
    publish();

    if (!hasPlace())
        return;

    // Step 1, then step 2. The first is synchronous in practice — a cache hit
    // resolves its future inside fetch() — so the window's first frame already
    // has data in it whenever there is any to have.
    fetch(/*cachedOnly=*/true);
    fetch(/*cachedOnly=*/false);
}

void AppEngine::refresh()
{
    if (!hasPlace())
        return;
    fetch(/*cachedOnly=*/false);
}

void AppEngine::fetch(bool cachedOnly)
{
    const Place current = place();

    ForecastRequest request;
    request.coord      = current.coordinate;
    request.days       = kForecastDays;
    request.cachedOnly = cachedOnly;

    // The zone is the *place's*, not the machine's. A saved location in Tokyo
    // renders its own evening while the app runs in Toronto — which is what
    // makes DailyPoint::date mean anything, and is why Place carries an IANA id
    // rather than an offset.
    if (!current.timezone.isEmpty())
        request.timeZone = QTimeZone(current.timezone.toUtf8());

    const quint64 generation = m_generation;

    if (!cachedOnly)
        setInFlight(+2);

    // ---- forecast ----------------------------------------------------------
    auto *forecastWatcher = new QFutureWatcher<Result<ForecastAnswer>>(this);
    connect(forecastWatcher, &QFutureWatcherBase::finished, this,
            [this, forecastWatcher, cachedOnly, generation]() {
                forecastWatcher->deleteLater();
                if (!cachedOnly)
                    setInFlight(-1);

                // An answer about a place the user has left. Dropped rather
                // than applied — the same rule PlaceSearchModel uses on
                // out-of-order geocoder replies.
                if (generation != m_generation)
                    return;

                const Result<ForecastAnswer> result = forecastWatcher->result();
                if (!result) {
                    // A cache miss is not news: it is the ordinary state of a
                    // place being opened for the first time, and step 2 is
                    // already on its way.
                    if (!cachedOnly) {
                        qWarning("clima: forecast failed: %s",
                                 qPrintable(result.error().toString()));
                        m_problem = tr("Could not refresh: %1")
                                        .arg(result.error().message());
                        Q_EMIT forecastChanged();
                    }
                    return;
                }

                applyForecast(result.value().value, result.value().servedBy,
                              result.value().fromFallback);
            });
    forecastWatcher->setFuture(m_registry->fetchForecast(request));

    // ---- air quality -------------------------------------------------------
    auto *airWatcher = new QFutureWatcher<Result<AirQualityAnswer>>(this);
    connect(airWatcher, &QFutureWatcherBase::finished, this,
            [this, airWatcher, cachedOnly, generation]() {
                airWatcher->deleteLater();
                if (!cachedOnly)
                    setInFlight(-1);

                if (generation != m_generation)
                    return;

                const Result<AirQualityAnswer> result = airWatcher->result();
                if (!result) {
                    // Air quality failing is not the forecast failing. It gets
                    // no `problem` line of its own: the card simply does not
                    // draw, which is R9 — hide, never fabricate.
                    return;
                }
                applyAirQuality(result.value().value);
            });
    airWatcher->setFuture(m_registry->fetchAirQuality(request));
}

void AppEngine::applyForecast(const Forecast &forecast, const QString &servedBy, bool fromFallback)
{
    if (forecast.isEmpty())
        return;

    // A cached answer that arrives after a newer network answer must not
    // overwrite it. Ordinarily step 1 finishes first; a slow disk and a warm
    // socket can invert that, and the timestamps are the only ordering that is
    // true regardless.
    if (m_forecast.fetchedAt.isValid() && forecast.fetchedAt.isValid()
        && forecast.fetchedAt < m_forecast.fetchedAt) {
        return;
    }

    m_forecast     = forecast;
    m_fromFallback = fromFallback;
    m_problem.clear();

    // The provider's display name, looked up rather than switched on. §4.1
    // principle 2 — "no provider name appears in UI code" — is a rule about
    // *branching*, and this is a lookup: the string is shown, never tested.
    m_sourceName = servedBy;
    for (const IProvider *provider : m_registry->providers()) {
        if (provider->id() == servedBy) {
            m_sourceName = provider->displayName();
            break;
        }
    }

    publish();
}

void AppEngine::applyAirQuality(const AirQuality &airQuality)
{
    if (airQuality.isEmpty())
        return;
    if (m_airQuality.fetchedAt.isValid() && airQuality.fetchedAt.isValid()
        && airQuality.fetchedAt < m_airQuality.fetchedAt) {
        return;
    }

    m_airQuality = airQuality;
    publish();
}

void AppEngine::publish()
{
    const Place current = place();

    m_forecastData->setSnapshot(m_forecast, m_airQuality, m_clock->now(), current);
    m_conditionsData->setSnapshot(m_forecast, m_airQuality, m_clock->now(), current,
                                  hasPollen());

    Q_EMIT forecastChanged();
}

void AppEngine::setInFlight(int delta)
{
    const bool was = isLoading();
    m_inFlight     = qMax(0, m_inFlight + delta);
    if (was != isLoading())
        Q_EMIT loadingChanged();
}

// ---- places ----------------------------------------------------------------------

QObject *AppEngine::placesModel() const { return m_places; }
QObject *AppEngine::searchModel() const { return m_search; }

Place AppEngine::place() const
{
    if (m_places != nullptr && m_places->currentIndex() >= 0)
        return m_places->currentPlace();
    return m_fixture.place;
}

bool AppEngine::hasPlace() const
{
    // The name as well as the coordinate, and the name is the load-bearing
    // half. A default-constructed Place has coordinate 0,0 — which
    // Coordinate::isValid() accepts, correctly, because null island is a
    // latitude and a longitude — so a start with nothing saved fetched the
    // forecast for a point in the Gulf of Guinea and cached it. Found in the
    // cache table, not on screen: the request succeeded and the answer was
    // never drawn.
    return !place().name.isEmpty() && place().coordinate.isValid();
}

QString AppEngine::placeLabel() const  { return place().label(); }
QString AppEngine::placeRegion() const { return place().region(); }

bool AppEngine::placeIsHome() const
{
    return m_places != nullptr && m_places->currentIsHome();
}

void AppEngine::selectPlace(int row)
{
    if (m_places != nullptr)
        m_places->setCurrentIndex(row);
}

void AppEngine::chooseSearchResult(int row)
{
    if (m_search == nullptr || m_places == nullptr)
        return;

    const Place found = m_search->resultAt(row);
    if (found.name.isEmpty())
        return;

    // addPlace() returns the existing row when the place is already saved —
    // Place::isSameEntity decides — so searching for somewhere you already have
    // selects it rather than duplicating it.
    m_places->addPlace(found, /*makeCurrent=*/true);
    m_search->clear();
}

void AppEngine::removePlace(int row)
{
    if (m_places != nullptr)
        m_places->removeAt(row);
}

void AppEngine::toggleHome(int row)
{
    if (m_places != nullptr)
        m_places->toggleHome(row);
}

bool AppEngine::locationAvailable() const
{
    return m_locator != nullptr && m_locator->isAvailable();
}

void AppEngine::useMyLocation()
{
    if (m_locator == nullptr || !m_locator->isAvailable()) {
        Q_EMIT locationFailed(tr("This system has no location service Clima can use."));
        return;
    }
    m_locator->requestPosition();
}

void AppEngine::selectByQuery(const QString &query, int timeoutMs)
{
    if (m_geocoder == nullptr || query.isEmpty())
        return;

    GeocodeQuery ask;
    ask.name  = query;
    ask.count = 1;

    QFutureWatcher<Result<QList<Place>>> watcher;
    QEventLoop                           loop;
    QTimer                               deadline;

    deadline.setSingleShot(true);
    connect(&deadline, &QTimer::timeout, &loop, &QEventLoop::quit);
    connect(&watcher, &QFutureWatcherBase::finished, &loop, &QEventLoop::quit);

    watcher.setFuture(m_geocoder->search(ask));
    deadline.start(timeoutMs);
    loop.exec();

    if (!watcher.isFinished()) {
        qWarning("clima: --place %s timed out", qPrintable(query));
        return;
    }

    const Result<QList<Place>> found = watcher.result();
    if (!found || found.value().isEmpty()) {
        qWarning("clima: --place %s found nothing", qPrintable(query));
        return;
    }

    m_places->addPlace(found.value().constFirst(), /*makeCurrent=*/true);
}

// ---- status ----------------------------------------------------------------------

bool AppEngine::hasData() const
{
    return !m_forecast.isEmpty();
}

QString AppEngine::updatedLabel() const
{
    if (!m_forecast.fetchedAt.isValid())
        return {};

    const qint64 seconds = m_forecast.fetchedAt.secsTo(m_clock->now());

    // Under a minute is "just now" and not "0 minutes ago", which is a phrase
    // nobody says. Over a day the count of days is the useful number, and the
    // exact hour stops being one.
    // Written out rather than run through tr()'s %n plural machinery, and that
    // is a deliberate downgrade. %n resolves its plural form from the
    // *translation*, so with no catalogue loaded — which is every English build
    // — the source string is returned verbatim and the line reads "Updated 3
    // hour(s) ago". Seen on screen, in the offline capture this affordance was
    // built for. Two strings per unit is the price of English being a language
    // rather than a fallback.
    if (seconds < 60)
        return tr("Updated just now");

    if (seconds < 3600) {
        const int minutes = int(seconds / 60);
        return minutes == 1 ? tr("Updated a minute ago")
                            : tr("Updated %1 minutes ago").arg(minutes);
    }

    if (seconds < 86400) {
        const int hours = int(seconds / 3600);
        return hours == 1 ? tr("Updated an hour ago") : tr("Updated %1 hours ago").arg(hours);
    }

    const int days = int(seconds / 86400);
    return days == 1 ? tr("Updated yesterday") : tr("Updated %1 days ago").arg(days);
}

bool AppEngine::isStale() const
{
    if (!m_forecast.fetchedAt.isValid())
        return false;

    // The same TTL the cache uses, asked of the same table. A second number
    // here — "call it stale after an hour" — would be a second opinion about
    // freshness, and the two would disagree the day §4.5 is edited.
    const QDateTime expires = expiryFor(DataKind::Forecast, m_forecast.fetchedAt);
    return expires.isValid() && m_clock->now() > expires;
}

QVariantList AppEngine::sources() const
{
    QVariantList out;
    if (m_registry == nullptr)
        return out;

    for (const Attribution &credit : m_registry->attributions())
        out.append(attributionMap(credit));

    // The place index is not a provider — it answers no forecast and is in no
    // chain — and it is bundled data under CC BY 4.0 all the same, so its
    // credit is as much of an obligation as Open-Meteo's. Appended here rather
    // than made into an IProvider, because inventing a provider interface for
    // something that provides no product would be the wrong shape for the sake
    // of one row.
    if (m_reverse != nullptr) {
        Attribution geonames;
        geonames.name        = QStringLiteral("GeoNames");
        geonames.creditLine  = QStringLiteral("Place names from the GeoNames geographical database");
        geonames.homepage    = QUrl(QStringLiteral("https://www.geonames.org/"));
        geonames.licenceName = QStringLiteral("CC BY 4.0");
        geonames.licenceUrl  = QUrl(QStringLiteral("https://creativecommons.org/licenses/by/4.0/"));
        geonames.note = tr("Bundled offline and read only when \"use my location\" is pressed, "
                           "so a coordinate is named without being sent anywhere.");
        out.append(attributionMap(geonames));
    }

    return out;
}

// ---- capabilities ----------------------------------------------------------------

bool AppEngine::hasPollen() const
{
    if (m_registry == nullptr || !hasPlace())
        return false;
    return m_registry->airQualityCapabilitiesAt(place().coordinate).has(Capability::Pollen);
}

bool AppEngine::isPollenUndetermined() const
{
    if (m_registry == nullptr || !hasPlace())
        return true;
    return m_registry->airQualityCapabilitiesAt(place().coordinate)
        .isUndetermined(Capability::Pollen);
}

bool AppEngine::hasAirQuality() const
{
    if (m_registry == nullptr || !hasPlace())
        return false;
    return m_registry->airQualityCapabilitiesAt(place().coordinate)
        .has(Capability::AirQualityIndex);
}
