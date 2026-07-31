// SPDX-FileCopyrightText: 2026 Jowi Aoun
// SPDX-License-Identifier: MPL-2.0
//
// Open-Meteo's `/v1/forecast`, as an IForecastProvider. The primary.
//
// docs/02-data-sources.md §2.1 picks it and says why: global, no API key ever,
// CC-BY 4.0, eighteen models, and a free tier whose limits are per *user*
// rather than per application because every client calls it from its own IP —
// which is the same fact that lets Clima ship with no server at all (§2.8).
// Our worst case is about 150 calls a day against a 10 000 a day ceiling.
//
// ============================================================================
// WHAT THIS CLASS DOES, WHICH IS LESS THAN IT LOOKS
//
// Build a URL, hand it to HttpClient, hand the bytes to a pure function, stamp
// a timestamp on the result. The three things that are actually hard live
// elsewhere on purpose:
//
//   the mapping      libclima/providers/openmeteo/openmeteoadapter.h, a free
//                    function over a QByteArray, so eight recorded responses
//                    can be replayed through it with no event loop.
//   the compliance   libclima/net/httpclient.h owns the User-Agent, the 403
//                    hard stop, request coalescing and conditional GET. This
//                    class does not construct a QNetworkAccessManager and must
//                    never be allowed to.
//   the time         libclima/domain/timeaxis.h, because `timezone=auto` does
//                    not mean what it says.
//
// ============================================================================
// CAPABILITIES ARE LEARNED, NOT DECLARED
//
// iforecastprovider.h argues that capabilities belong per (provider, location)
// rather than per provider, and Open-Meteo is the case that proves it twice
// over. `models=ecmwf_ifs025` returns `uv_index` and `visibility` as null for
// every hour because IFS does not carry them — a recorded example is
// tests/fixtures/openmeteo/toronto-ecmwf-gaps.json — and a provider-level
// "Open-Meteo has UV" would draw an empty UV tab with a full axis.
//
// So: before this provider has seen a payload for a place it answers
// `undetermined` for every variable, and after one it answers from what
// actually arrived. The verdict is remembered against the rounded coordinate,
// the same quantisation HttpClient hashes with (four decimals,
// libclima/domain/coordinate.h) — so a map drag asks about one place rather
// than a hundred, and a remembered verdict is reused rather than re-derived.
//
// ============================================================================
// SIXTEEN DAYS, ONE PAST DAY
//
// `forecast_days=16` is the maximum and it costs about 54 kB. `past_days=1` is
// what makes the hourly strip able to start before "now" — app/qml/Clima's
// chart shows fifteen observed hours behind the marker and mockdata.js's
// series starts at 21:00 the previous evening, which is not reachable from a
// forecast that begins at today's midnight.
//
// A caller asking for fewer days gets fewer; `ForecastRequest::days` clamps
// into [1, 16] rather than failing, because a fallback that refused a request
// the primary would have accepted is not a fallback.

#pragma once

#include "libclima/net/httprequest.h"
#include "libclima/providers/iforecastprovider.h"

#include <QHash>
#include <QObject>
#include <QString>

namespace clima {

class Clock;
class HttpClient;

class OpenMeteoForecastProvider : public QObject, public IForecastProvider
{
    Q_OBJECT

public:
    // Neither pointer is owned and both must outlive this. The clock is what
    // stamps `Forecast::fetchedAt`; there is no other source of "now" here,
    // and libclima/core/clock.h explains why that is a rule with a test behind
    // it rather than a preference.
    OpenMeteoForecastProvider(HttpClient *http, Clock *clock, QObject *parent = nullptr);
    ~OpenMeteoForecastProvider() override;

    // The id a 403 disables and a cache row is keyed by. Never changes.
    static QString providerId();

    [[nodiscard]] QString      id() const override;
    [[nodiscard]] QString      displayName() const override;
    [[nodiscard]] Attribution  attribution() const override;
    [[nodiscard]] bool         covers(Coordinate coord) const override;
    [[nodiscard]] Capabilities capabilitiesAt(Coordinate coord) const override;

    QFuture<Result<Forecast>> fetchForecast(const ForecastRequest &request) override;

    // The base URL, without a query. Overridable so that a test can point the
    // provider at tests/support/httpstub.h's loopback server — which is the
    // only way to exercise this class at all, since no test may reach the
    // internet (docs/04-architecture.md §4.11).
    void          setBaseUrl(const QUrl &url);
    [[nodiscard]] QUrl baseUrl() const;

    // The request that would be sent, without sending it. Public because it is
    // the half of this class worth asserting on directly: that the variable
    // lists, the day clamp and the coordinate rounding are what we think they
    // are is a question with an answer that needs no network.
    [[nodiscard]] HttpRequest buildRequest(const ForecastRequest &request) const;

    // How many days this provider will actually ask for, given a request.
    static int clampDays(int requested);

private:
    void rememberCapabilities(Coordinate coord, const Forecast &forecast);

    HttpClient *m_http  = nullptr;
    Clock      *m_clock = nullptr;

    QUrl m_baseUrl;

    // Keyed by Coordinate::toKeyString(), the same spelling the cache and the
    // request coalescer use. mutable because capabilitiesAt() is const and
    // this is a memo rather than state — the answer it caches is a fact about
    // the world, not about this object.
    mutable QHash<QString, Capabilities> m_learned;
};

} // namespace clima
