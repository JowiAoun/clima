// SPDX-FileCopyrightText: 2026 Jowi Aoun
// SPDX-License-Identifier: GPL-3.0-or-later

#include "snapshotservice.h"

#include "libclima/cache/cachestore.h"
#include "libclima/core/clock.h"
#include "libclima/net/httpclient.h"
#include "libclima/places/locationcontroller.h"
#include "libclima/providers/airquality/openmeteoairqualityprovider.h"
#include "libclima/providers/eccc/ecccalertprovider.h"
#include "libclima/providers/metno/metnoforecastprovider.h"
#include "libclima/providers/nws/nwsalertprovider.h"
#include "libclima/providers/openmeteo/openmeteoforecastprovider.h"
#include "libclima/providers/registry.h"

#include <QFile>
#include <QFutureWatcher>
#include <QTimer>

using namespace clima;

namespace {

// One timer for every watched place. It is deliberately not the TTL: the
// providers consult libclima's own cache policy and only open a socket when
// what they hold has actually gone stale, so this is how often we *ask*, not
// how often we fetch. Putting a second TTL here would be a second policy that
// could disagree with the first.
constexpr int kPollIntervalMs = 5 * 60 * 1000;

// What the daemon keeps for a place that nobody has narrowed. A widget asks
// for what it needs through the mask; these are the ceilings behind it.
constexpr int kForecastDays = 10;

QString tokenFor(quint64 n)
{
    return QStringLiteral("s%1").arg(n);
}

} // namespace

SnapshotService::SnapshotService(QObject *parent)
    : QObject(parent)
{
}

SnapshotService::~SnapshotService() = default;

void SnapshotService::configure(const QString &fixtureName)
{
    m_fixtureName = fixtureName;

    // The clock first, and everything else is handed it — the same single
    // branch AppEngine::configure takes, for the same reason.
    if (isFixtureMode()) {
        m_fixture = fixtures::load(fixtureName);
        m_clock   = std::make_unique<FrozenClock>(m_fixture.recordedAt);
    } else {
        m_clock = std::make_unique<SystemClock>();
    }

    m_cache              = std::make_unique<CacheStore>(m_clock.get());
    const Status opened  = m_cache->open(CacheStore::defaultDatabasePath());
    if (!opened) {
        // Same posture as the app: a cache that will not open is a daemon that
        // asks the network more often, not a daemon that cannot serve.
        qWarning("clima-daemon: the cache could not be opened (%s); "
                 "widgets will not survive going offline",
                 qPrintable(opened.error().toString()));
    }

    m_http = std::make_unique<HttpClient>(m_clock.get());
    m_http->setValidatorStore(m_cache.get());

    m_registry = std::make_unique<ProviderRegistry>();

    if (isFixtureMode())
        buildFixtureProviders();
    else
        buildLiveProviders();

    registerProviders();

    m_places = new LocationController(m_cache.get(), this);
    m_places->load();

    m_poll = new QTimer(this);
    m_poll->setInterval(kPollIntervalMs);
    connect(m_poll, &QTimer::timeout, this, &SnapshotService::onPollTimeout);
    m_poll->start();
}

void SnapshotService::buildLiveProviders()
{
    m_openMeteo = new OpenMeteoForecastProvider(m_http.get(), m_clock.get(), this);
    m_openMeteo->setCache(m_cache.get());

    m_metNo = new MetNoForecastProvider(m_http.get(), m_clock.get(), this);
    m_metNo->setCache(m_cache.get());

    m_openMeteoAq = new OpenMeteoAirQualityProvider(m_http.get(), m_clock.get(), this);
    m_openMeteoAq->setCache(m_cache.get());

    m_eccc = new EcccAlertProvider(m_http.get(), m_clock.get(), this);
    m_eccc->setCache(m_cache.get());

    m_nws = new NwsAlertProvider(m_http.get(), m_clock.get(), this);
    m_nws->setCache(m_cache.get());
}

void SnapshotService::buildFixtureProviders()
{
    m_fixtureForecast = new FixtureForecastProvider(m_fixture, this);
    m_fixtureAir      = new FixtureAirQualityProvider(m_fixture, this);
    m_fixtureAlerts   = new FixtureAlertProvider(m_fixture, this);
}

void SnapshotService::registerProviders()
{
    const auto complain = [](const Status &status, const char *what) {
        if (!status) {
            qFatal("clima-daemon: %s could not be registered: %s", what,
                   qPrintable(status.error().toString()));
        }
    };

    if (isFixtureMode()) {
        complain(m_registry->addForecastProvider(m_fixtureForecast, 100), "the fixture provider");
        complain(m_registry->addAirQualityProvider(m_fixtureAir, 100), "the fixture air quality");
        complain(m_registry->addAlertProvider(m_fixtureAlerts, 0), "the fixture alerts");
        return;
    }

    complain(m_registry->addForecastProvider(m_openMeteo, 100), "Open-Meteo");
    complain(m_registry->addForecastProvider(m_metNo, 200), "MET Norway");
    complain(m_registry->addAirQualityProvider(m_openMeteoAq, 100), "Open-Meteo air quality");
    complain(m_registry->addAlertProvider(m_eccc, 0), "ECCC alerts");
    complain(m_registry->addAlertProvider(m_nws, 0), "NWS alerts");
}

// ---- places -----------------------------------------------------------------

Place SnapshotService::resolve(const QString &placeId) const
{
    if (isFixtureMode())
        return m_fixture.place;

    // "home" and "" both mean the home place, because that is what a widget
    // wants to say and it has to keep working when the user renames a city.
    if (placeId.isEmpty() || placeId == QLatin1String("home")) {
        const QList<Place> all = m_places->places();
        for (const Place &p : all) {
            if (p.isHome)
                return p;
        }
        return m_places->currentPlace();
    }

    bool         ok = false;
    const qint64 id = placeId.toLongLong(&ok);
    if (ok) {
        const QList<Place> all = m_places->places();
        for (const Place &p : all) {
            if (p.id == id)
                return p;
        }
    }
    return {};
}

QString SnapshotService::canonical(const QString &placeId) const
{
    const Place place = resolve(placeId);
    if (isFixtureMode())
        return QStringLiteral("fixture");
    return place.id != 0 ? QString::number(place.id) : QString();
}

QStringList SnapshotService::placeIds() const
{
    if (isFixtureMode())
        return {QStringLiteral("fixture")};

    QStringList ids;
    const QList<Place> all = m_places->places();
    ids.reserve(all.size());
    for (const Place &p : all)
        ids.append(QString::number(p.id));
    return ids;
}

// ---- the bus ----------------------------------------------------------------

SnapshotService::Watched &SnapshotService::ensureWatched(const QString &placeId)
{
    auto it = m_watched.find(placeId);
    if (it != m_watched.end())
        return *it;

    Watched fresh;
    fresh.place = resolve(placeId);
    it          = m_watched.insert(placeId, fresh);

    // A place nobody has asked about before has nothing in memory. Kick a
    // fetch, which will come back from the cache immediately if there is one —
    // the first answer a widget gets should be a stale reading rather than a
    // gap, which is the same rule the app's first frame follows.
    fetch(placeId);
    return *it;
}

QByteArray SnapshotService::snapshot(const QString    &placeId,
                                     const QStringList &fields,
                                     int                hours,
                                     int                days)
{
    const QString key = canonical(placeId);
    if (key.isEmpty())
        return {};

    const Watched &watched = ensureWatched(key);

    wire::SnapshotSource source;
    source.placeId    = key;
    source.place      = watched.place;
    source.forecast   = watched.forecast;
    source.airQuality = watched.air;
    source.alerts     = watched.alerts;
    source.now        = m_clock->now();
    source.hours      = hours;
    source.days       = days;
    source.servedBy   = watched.servedBy;
    source.fromCache  = watched.fromCache;

    return wire::encode(wire::buildSnapshot(source, wire::FieldMask::fromFields(fields)));
}

QString SnapshotService::subscribe(const QString    &placeId,
                                   const QStringList &fields,
                                   int                hours,
                                   int                days)
{
    const QString key = canonical(placeId);
    if (key.isEmpty())
        return {};

    Subscription sub;
    sub.placeId = key;
    sub.mask    = wire::FieldMask::fromFields(fields);
    sub.hours   = hours;
    sub.days    = days;

    const QString token = tokenFor(m_nextToken++);
    m_subscriptions.insert(token, sub);

    ensureWatched(key);

    // ---- there is deliberately no first publish here ------------------------
    //
    // There used to be: a QTimer::singleShot(0) that emitted one snapshot on the
    // next return to the event loop, so that "a subscriber which connects to the
    // signal after this call still gets its first snapshot". That does not work,
    // and the way it fails is invisible from this side.
    //
    // The subscriber is blocked in this method call. It cannot add its match
    // rule until the reply reaches it, and adding one is itself a round trip to
    // the bus daemon — while singleShot(0) fires here as soon as the reply is
    // *written*. So the race is not close: the first snapshot is normally
    // emitted before anybody is listening for it, and a widget then shows its
    // waiting skeleton until the next poll five minutes later.
    //
    // Measured, not reasoned about. Every tile in clima-widget came up blank
    // against a live daemon while `gdbus monitor` showed the signals going out.
    //
    // The subscriber calls GetSnapshot once after its match rule is in place,
    // which is deterministic and is one call it is already making a connection
    // for. See DaemonLink::resubscribe.

    return token;
}

bool SnapshotService::unsubscribe(const QString &token)
{
    return m_subscriptions.remove(token) > 0;
}

void SnapshotService::requestRefresh(const QString &placeId)
{
    const QString key = canonical(placeId);
    if (key.isEmpty())
        return;
    ensureWatched(key);
    fetch(key);
}

QByteArray SnapshotService::catalogue() const
{
    // Compiled in as a resource rather than read from an install path, so that
    // ListWidgets answers the same thing whether the daemon was started from a
    // build directory, a .deb or inside a Flatpak. It is the same bytes the
    // test in tests/tst_wiresnapshot.cpp checks.
    QFile file(QStringLiteral(":/clima/catalogue.json"));
    if (!file.open(QIODevice::ReadOnly))
        return {};
    return file.readAll();
}

// ---- fetching ---------------------------------------------------------------

void SnapshotService::onPollTimeout()
{
    // Only what somebody is actually looking at. A place with no subscription
    // and no recent GetSnapshot is not refreshed, which is what keeps a daemon
    // with ten saved cities from making ten requests for the nine nobody has
    // on their desktop.
    QSet<QString> wanted;
    for (const Subscription &sub : std::as_const(m_subscriptions))
        wanted.insert(sub.placeId);

    for (const QString &placeId : std::as_const(wanted))
        fetch(placeId);
}

void SnapshotService::fetch(const QString &placeId)
{
    auto it = m_watched.find(placeId);
    if (it == m_watched.end())
        return;
    if (it->inFlight)
        return;
    if (!it->place.coordinate.isValid())
        return;

    it->inFlight = true;

    ForecastRequest request;
    request.coord    = it->place.coordinate;
    request.days     = kForecastDays;
    request.timeZone = QTimeZone(it->place.timezone.toUtf8());

    auto *forecast = new QFutureWatcher<Result<ForecastAnswer>>(this);
    connect(forecast, &QFutureWatcherBase::finished, this, [this, forecast, placeId]() {
        forecast->deleteLater();
        auto watched = m_watched.find(placeId);
        if (watched == m_watched.end())
            return;
        watched->inFlight = false;

        const Result<ForecastAnswer> result = forecast->result();
        if (result) {
            watched->forecast  = result.value().value;
            watched->servedBy  = result.value().servedBy;
            watched->fromCache = false;
            publish(placeId);
        } else if (!watched->forecast.isEmpty()) {
            // A failed refresh does not clear what we already have. The widget
            // keeps its reading and its timestamp, and the timestamp is what
            // tells the truth about the failure.
            watched->fromCache = true;
            publish(placeId);
        }
    });
    forecast->setFuture(m_registry->fetchForecast(request));

    auto *air = new QFutureWatcher<Result<AirQualityAnswer>>(this);
    connect(air, &QFutureWatcherBase::finished, this, [this, air, placeId]() {
        air->deleteLater();
        auto watched = m_watched.find(placeId);
        if (watched == m_watched.end())
            return;
        const Result<AirQualityAnswer> result = air->result();
        if (result) {
            watched->air = result.value().value;
            publish(placeId);
        }
    });
    air->setFuture(m_registry->fetchAirQuality(request));

    AlertRequest alertRequest;
    alertRequest.coord = it->place.coordinate;

    auto *alerts = new QFutureWatcher<Result<AlertAnswer>>(this);
    connect(alerts, &QFutureWatcherBase::finished, this, [this, alerts, placeId]() {
        alerts->deleteLater();
        auto watched = m_watched.find(placeId);
        if (watched == m_watched.end())
            return;
        const Result<AlertAnswer> result = alerts->result();
        if (result) {
            watched->alerts = result.value().value;
            publish(placeId);
        }
    });
    alerts->setFuture(m_registry->fetchAlerts(alertRequest));
}

void SnapshotService::publish(const QString &placeId)
{
    for (auto it = m_subscriptions.cbegin(); it != m_subscriptions.cend(); ++it) {
        if (it->placeId != placeId)
            continue;
        const QByteArray json = snapshot(placeId, it->mask.fields(), it->hours, it->days);
        if (!json.isEmpty())
            Q_EMIT snapshotChanged(it.key(), QString::fromUtf8(json));
    }
}
