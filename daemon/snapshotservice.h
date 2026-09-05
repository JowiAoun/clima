// SPDX-FileCopyrightText: 2026 Jowi Aoun
// SPDX-License-Identifier: GPL-3.0-or-later
//
// The single writer. One process fetches, one process writes the cache, and
// everything else on the desktop reads from it over the session bus.
//
// ============================================================================
// WHY THIS EXISTS AND THE APP DOES NOT SIMPLY SERVE IT
//
// Three reasons, in the order they bite:
//
//   1. **SQLite has one writer.** Six widgets, a tray and the app is eight
//      processes opening the same database. libclima's cache would survive it
//      — it is WAL and the writes are small — but "survive" is the wrong bar
//      for something a user leaves running for a month.
//
//   2. **The free tier is per-client, not per-window.** Open-Meteo's terms are
//      non-commercial and rate-limited (R5), and eight processes each honouring
//      a 15-minute TTL is eight times the requests for one desktop's worth of
//      weather. That is the difference between a good citizen and a scraper.
//
//   3. **The alert poll has to be in one place.** docs/04-architecture.md §4.5
//      budgets it at roughly 264 KB a day on the assumption that there is one
//      of it. Eight independent pollers is eight tombstone state machines that
//      can disagree about whether a warning was cancelled.
//
// So: this process owns the network, the cache and the clock. Widgets own
// pixels. The app will eventually read from here too, but it does not yet and
// nothing here assumes it does — the daemon is additive, and a desktop with no
// daemon running is exactly the app that shipped before this existed.
//
// ============================================================================
// WHAT A DEAD DAEMON LOOKS LIKE
//
// Last-known data and an honest timestamp, never a blank tile. Every snapshot
// carries `fetchedAt`, so a widget whose daemon has gone away keeps drawing
// what it has and says "updated 40 minutes ago" — and goes on being right
// about that for as long as it is up. This is the same rule as non-negotiable
// 1 in docs/README.md, applied one process further out.
//
// ============================================================================
// PLACE IDS
//
// The canonical id is Place::id in decimal, which is what the cache assigns.
// "home" and the empty string both resolve to the home place, because that is
// what a widget wants to say and it must keep working when the user renames or
// re-adds a city. A snapshot always reports the canonical id it resolved to,
// so a subscriber can tell which place it actually got.

#pragma once

#include "libclima/domain/airquality.h"
#include "libclima/domain/alert.h"
#include "libclima/domain/forecast.h"
#include "libclima/domain/place.h"
#include "libclima/providers/fixture/fixtureprovider.h"
#include "libclima/wire/snapshot.h"

#include <QByteArray>
#include <QHash>
#include <QObject>
#include <QString>
#include <QStringList>

#include <memory>

namespace clima {
class CacheStore;
class Clock;
class HttpClient;
class LocationController;
class ProviderRegistry;
class OpenMeteoForecastProvider;
class MetNoForecastProvider;
class OpenMeteoAirQualityProvider;
class EcccAlertProvider;
class NwsAlertProvider;
class FixtureForecastProvider;
class FixtureAirQualityProvider;
class FixtureAlertProvider;
} // namespace clima

class QFileSystemWatcher;
class QTimer;

class SnapshotService : public QObject
{
    Q_OBJECT

public:
    explicit SnapshotService(QObject *parent = nullptr);
    ~SnapshotService() override;

    // Mirrors AppEngine::configure: one branch, taken once, deciding which
    // clock and which providers. Everything downstream is identical.
    void configure(const QString &fixtureName);

    // How far ahead every fetch asks. Public because the cache key is derived
    // from the request, and a test that seeds the cache has to ask for the
    // same thing this process will.
    static constexpr int forecastDays = 10;

    [[nodiscard]] bool isFixtureMode() const { return !m_fixtureName.isEmpty(); }

    // ---- what the bus calls ------------------------------------------------

    [[nodiscard]] QByteArray snapshot(const QString    &placeId,
                                      const QStringList &fields,
                                      int                hours,
                                      int                days);

    [[nodiscard]] QString subscribe(const QString    &placeId,
                                    const QStringList &fields,
                                    int                hours,
                                    int                days);

    bool unsubscribe(const QString &token);

    void requestRefresh(const QString &placeId);

    [[nodiscard]] QByteArray catalogue() const;

    [[nodiscard]] QStringList placeIds() const;

Q_SIGNALS:
    // One signal per subscription, with the token first so a reader can filter
    // on arg0 in its match rule and never be woken for somebody else's widget.
    void snapshotChanged(const QString &token, const QString &json);

    // The saved places changed under us: one was added, removed, moved or made
    // home. Existing subscriptions are re-pointed here before this goes out, so
    // a reader that ignores it still ends up with the right city — what it is
    // for is the reader whose Subscribe FAILED, which is every widget on a
    // desktop where the tiles were put up before anybody chose a place. There
    // is no subscription to re-point for those, and this is the only thing that
    // will ever tell them to ask again.
    void placesChanged();

private:
    // Everything held for one place. The three payloads are kept in memory
    // rather than re-read from the cache on every call, because a widget host
    // starting up asks ten times in a second and the cache is not the thing
    // that should absorb that.
    struct Watched {
        clima::Place      place;
        clima::Forecast   forecast;
        clima::AirQuality air;
        clima::AlertSet   alerts;
        QString           servedBy;
        bool              fromCache = true;
        bool              inFlight  = false;
    };

    struct Subscription {
        // What this subscription resolved to when it was made, and what was
        // asked for. Both, because they answer different questions and the
        // second one used to be thrown away.
        //
        // A reader subscribes to "home". That is canonicalised to a row id here
        // so the fetch, the cache and the publish all key on one string — and
        // for as long as only the id was kept, "home" meant *whichever place
        // was home the moment you asked*. Change home in the app and every
        // widget on the desktop went on drawing the old city, correctly
        // serving a subscription nobody would have made on purpose.
        QString              placeId;
        QString              requested;
        clima::wire::FieldMask mask;
        int                  hours = 0;
        int                  days  = 0;
    };

    void buildLiveProviders();
    void buildFixtureProviders();
    void registerProviders();

    [[nodiscard]] clima::Place resolve(const QString &placeId) const;
    [[nodiscard]] QString      canonical(const QString &placeId) const;

    Watched &ensureWatched(const QString &placeId);

    // The cache, synchronously, before the network. A widget host that starts
    // beside a daemon that has never fetched — a login, an upgrade, a D-Bus
    // activation — calls GetSnapshot in the same event-loop turn it subscribed
    // in, and fetch() below cannot answer inside that turn: every provider
    // future, even one served from the cache, is settled through the event
    // loop. So this asks each provider in the chain for its cached bytes
    // directly, takes the first answer that is already finished, and leaves
    // the tile a stale reading rather than a gap. `fromCache` is set, so the
    // snapshot says "cached" and the tile ages it honestly.
    void warmFromCache(Watched &watched);

    void     fetch(const QString &placeId);
    void     publish(const QString &placeId);
    void     onPollTimeout();

    // ---- keeping up with the app -------------------------------------------
    //
    // The places table belongs to the app: it is where somebody searches for a
    // city, sets a home and deletes the one they mistyped. This process reads
    // it and nothing tells it when it changes — deliberately, because the app
    // does not know this daemon exists (see the header) and a bus call from it
    // would be the first line of it finding out.
    //
    // So the database is watched instead, and the list is re-read on the poll
    // as well. The watcher is the fast path and the poll is the guarantee: file
    // notifications are best-effort across filesystems, containers and network
    // homes, and a widget that follows a change within five minutes is a
    // different bug from one that never follows it at all.
    void watchPlaces();
    void reloadPlaces();

    // Everything a resolution could turn on: which rows exist, which is home,
    // which is current, and where each one is. Compared rather than trusted,
    // because this process writes to the same database on every fetch and
    // would otherwise re-resolve the world every time it cached a forecast.
    [[nodiscard]] QString placesFingerprint() const;

    QString                                m_fixtureName;
    clima::Fixture                         m_fixture;
    std::unique_ptr<clima::Clock>          m_clock;
    std::unique_ptr<clima::CacheStore>     m_cache;
    std::unique_ptr<clima::HttpClient>     m_http;
    std::unique_ptr<clima::ProviderRegistry> m_registry;

    clima::LocationController *m_places = nullptr;

    clima::OpenMeteoForecastProvider   *m_openMeteo   = nullptr;
    clima::MetNoForecastProvider       *m_metNo       = nullptr;
    clima::OpenMeteoAirQualityProvider *m_openMeteoAq = nullptr;
    clima::EcccAlertProvider           *m_eccc        = nullptr;
    clima::NwsAlertProvider            *m_nws         = nullptr;

    clima::FixtureForecastProvider   *m_fixtureForecast = nullptr;
    clima::FixtureAirQualityProvider *m_fixtureAir      = nullptr;
    clima::FixtureAlertProvider      *m_fixtureAlerts   = nullptr;

    QHash<QString, Watched>      m_watched;
    QHash<QString, Subscription> m_subscriptions;
    quint64                      m_nextToken = 1;

    QTimer *m_poll = nullptr;

    QFileSystemWatcher *m_placesWatch    = nullptr;
    QTimer             *m_placesSettle   = nullptr;
    QString             m_placesFingerprint;
};
