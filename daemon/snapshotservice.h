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
        QString              placeId;
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
    void     fetch(const QString &placeId);
    void     publish(const QString &placeId);
    void     onPollTimeout();

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
};
