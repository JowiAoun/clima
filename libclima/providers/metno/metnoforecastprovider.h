// SPDX-FileCopyrightText: 2026 Jowi Aoun
// SPDX-License-Identifier: MPL-2.0
//
// MET Norway Locationforecast 2.0, the fallback — built now, not later.
//
//     https://api.met.no/weatherapi/locationforecast/2.0/compact?lat=&lon=
//
// ============================================================================
// WHY THIS EXISTS BEFORE ANYTHING NEEDS IT
//
// docs/06-roadmap.md is explicit that shipping the fallback late is the
// documented bug in a competing app: the primary goes down, the fallback path
// runs for the first time in production, and it does not work. An untested
// fallback is not a fallback — it is a second way to fail, written down.
//
// So it is here, in the same commit as the interface it implements, with a
// recorded fixture and a test that forces the primary to a typed error and
// asserts this provider serves the request. tests/tst_providerregistry.cpp,
// `aFailedPrimaryFallsThroughToMetNorway`.
//
// ============================================================================
// THEIR TERMS ARE A CONTRACT, AND HttpClient IS WHERE WE KEEP IT
//
// docs/02-data-sources.md §2.9: MET Norway requires an identifying User-Agent
// with a contact, and their terms require conditional requests. Both are
// implemented in libclima/net/httpclient.h and this provider goes through it
// rather than around it. There is no QNetworkAccessManager in this file and
// there must never be one.
//
//   * the User-Agent is built from CMake identity and cannot be overridden per
//     request — HttpRequest has no route to it,
//   * `conditional` is left at its default of true, so If-None-Match goes out
//     whenever a validator is on file and a 304 is answered from the last
//     parsed payload,
//   * coordinates are truncated to four decimals before they are sent, which
//     MET asks for by name (libclima/domain/coordinate.h),
//   * a 403 disables the provider for the life of the process, with no retry.
//
// A note on that last one, recorded because it would otherwise look like
// superstition: a generic User-Agent did NOT earn a 403 when this was tested
// against the live service. Their terms still say it will, and the difference
// between a client that is refused and one that is banned is whether it kept
// asking. The hard stop stays. We do not get to decide that a documented policy
// is not enforced today, and a policy that is enforced tomorrow finds a client
// that has been hammering it.
//
// ============================================================================
// A DIFFERENT SHAPE, ADAPTED — NOT A DIFFERENT MODEL
//
// The payload is GeoJSON: one Feature whose `properties.timeseries` is a list
// of entries, each carrying an `instant` block and up to three forward-looking
// blocks — `next_1_hours`, `next_6_hours`, `next_12_hours`.
//
// ---- the hour a number belongs to -------------------------------------------
//
// This is the one thing in the adaptation that is easy to get wrong and
// impossible to see afterwards. A `next_1_hours` block hanging off the entry at
// T describes [T, T+1h). The domain records accumulations on the point that
// ENDS the period, because that is Open-Meteo's convention and Open-Meteo is
// the primary — libclima/domain/forecast.h argues it.
//
// So the block at T lands on the point at T+1h, which in this payload is
// always the next entry. Two consequences, both deliberate:
//
//   * the first point has no precipitation and no weather code. It would
//     describe the hour before the forecast starts, which is the past, which
//     MET is not forecasting.
//   * the last entry's block has no point to land on and is dropped, costing
//     the final six hours of a nine-day forecast. Inventing a trailing point
//     with a precipitation total and no temperature would fill a hole in the
//     data by putting a hole in a chart.
//
// Get this wrong and every rain bar is one column out, on the fallback only,
// which nobody looks at until the primary is down.
//
// ---- hourly for two and a half days, then six-hourly ------------------------
//
// MET thins the series: about 60 hourly entries, then one every six hours out
// to roughly nine and a half days. `HourlyPoint::time` is explicit for exactly
// this reason — a consumer that assumes uniform spacing draws the second half
// of this forecast six times too wide.
//
// Where both blocks exist the 1-hour one wins, and the 6-hour one is used only
// where there is no 1-hour block. That is not a preference; it is what stops
// the transition hour from being counted twice.
//
// ============================================================================
// WHAT IT CANNOT SUPPLY — SAID THROUGH capabilitiesAt(), NOT THROUGH ZEROS
//
// The `compact` product carries seven variables. Everything below is genuinely
// absent, and every one of them is a flag this provider does not set:
//
//     apparent temperature      no "feels like" anywhere in the product
//     dew point                 `complete` has it; `compact` does not
//     wind gust                 `complete` has it; `compact` does not
//     precipitation probability `complete` has it; `compact` does not
//     rain / showers / snow     one total plus a symbol; no split
//     UV index                  `complete` has clear-sky UV only
//     visibility                in neither product
//     sunrise / sunset / moon   a separate product (Sunrise 3.0)
//     15-minute nowcast         not offered
//     ensembles, model choice   MET Nordic / ECMWF blend, take it or leave it
//     air quality, pollen       not this API at all
//     historical archive        not this API at all
//     a time zone               timestamps are UTC and it has no opinion about
//                               local midnight — see below
//
// Half of that list is available from the `complete` product instead of
// `compact`, at roughly three times the payload. That is a deliberate trade and
// the one to revisit first if the fallback ever becomes something users see
// often: `compact` is 39 kB for nine days and the app is asking for it at the
// worst possible moment, which is when the primary is already failing.
//
// ---- the time zone, and why ForecastRequest carries one ---------------------
//
// A daily series needs a definition of midnight. Open-Meteo resolves one from
// the coordinate (`timezone=auto`) and reports it back; MET Norway returns UTC
// instants and nothing else. So the caller supplies it — ForecastRequest::
// timeZone — and the Forecast records the zone its days were actually grouped
// by, so a UI formatting from `Forecast::timeZone` is always self-consistent
// even when the caller supplied nothing and got UTC.
//
// That is the honest version. The dishonest version is to group by UTC and
// label the result "Tuesday".

#pragma once

#include "libclima/providers/iforecastprovider.h"

#include <QHash>
#include <QObject>
#include <QString>

namespace clima {

class CacheStore;
class Clock;
class HttpClient;

class MetNoForecastProvider : public QObject, public IForecastProvider
{
    Q_OBJECT

public:
    // Neither is owned; both must outlive this. See openmeteoairqualityprovider.h
    // — a provider that built its own network client would be a provider outside
    // the User-Agent and 403 policy, which for this provider specifically is the
    // policy their terms of service are about.
    MetNoForecastProvider(HttpClient *http, Clock *clock, QObject *parent = nullptr);
    ~MetNoForecastProvider() override;

    void setBaseUrl(const QUrl &url);

    // Where the last good payload is kept. Same contract as the primary's:
    // not owned, must outlive this, null turns stale-while-revalidate off.
    void setCache(CacheStore *cache);

    [[nodiscard]] QString      id() const override;
    [[nodiscard]] QString      displayName() const override;
    [[nodiscard]] Attribution  attribution() const override;
    [[nodiscard]] bool         covers(Coordinate coord) const override;
    [[nodiscard]] Capabilities capabilitiesAt(Coordinate coord) const override;

    QFuture<Result<Forecast>> fetchForecast(const ForecastRequest &request) override;

    // Parsing, with no network and no event loop — the golden-file test surface
    // of docs/04-architecture.md §4.11, and what tools/provider-probe calls to
    // print a recorded fixture.
    //
    // `timeZone` is the zone the daily rollup is grouped by; invalid means UTC,
    // and the returned Forecast records whichever was used. `fetchedAt` is
    // passed in rather than read from the clock so that one fixture parsed
    // twice gives two identical Forecasts.
    static Result<Forecast> parse(const QByteArray &body, const QTimeZone &timeZone,
                                  const QDateTime &fetchedAt);

private:
    HttpClient *m_http = nullptr;
    Clock      *m_clock = nullptr;
    CacheStore *m_cache = nullptr;
    QUrl        m_baseUrl;

    // The last successfully parsed payload per RequestKey, so a 304 — which
    // their terms ask us to make possible — has something to answer with.
    QHash<QString, Forecast> m_lastParsed;
};

} // namespace clima
