// SPDX-FileCopyrightText: 2026 Jowi Aoun
// SPDX-License-Identifier: MPL-2.0
//
// Air quality from Open-Meteo's CAMS product. A separate host, no key, and one
// interesting problem.
//
//     https://air-quality-api.open-meteo.com/v1/air-quality
//
// A separate host from the forecast API and therefore, as far as HttpClient is
// concerned, still the same provider id: `open-meteo`. That is deliberate. A
// 403 is a statement about our User-Agent, not about a hostname, so a 403 from
// either host should disable both — which is what sharing the id buys, for
// free, in the one place it matters.
//
// ============================================================================
// THE PROBLEM: A WELL-FORMED RESPONSE FULL OF NOTHING
//
// Ask for pollen in Toronto and the request succeeds. HTTP 200, valid JSON,
// `hourly.grass_pollen` present, seventy-two entries long — and every one of
// them `null`. Verified against the live service and recorded in
// tests/fixtures/airquality/toronto.json: six pollen series and ammonia, 72
// nulls out of 72 each. The same request against Berlin returns 0 nulls out of
// 72 for all seven.
//
// CAMS produces pollen and ammonia for its European domain and for nowhere
// else. Open-Meteo does not error on a request outside that domain; it answers
// with the shape you asked for and no data in it, which is a defensible API
// design and a trap for a parser.
//
// The failure mode is not a crash. It is a pollen card in Toronto reading
// "Grass 0 · Birch 0 · Alder 0 — Low", which is *plausible*, which is worse.
// docs/08-risks.md R9: region-gate honestly, never fabricate.
//
// ============================================================================
// THE GATE IS A PROPERTY OF THE SERIES, NOT A BOUNDING BOX
//
// The obvious fix is a rectangle: if longitude is between -25 and 45 and
// latitude between 30 and 72, claim pollen. Three reasons not to.
//
//   1. It is a copy of somebody else's fact. CAMS's domain is theirs to change,
//      and when they extend it our rectangle silently keeps hiding a feature
//      that started working. Nothing fails; the app is just quietly worse than
//      the data it is holding.
//   2. A rectangle is wrong at the edges by construction. Iceland, Cyprus, the
//      Azores, the eastern Turkish border — every one of them is a coin flip
//      between an empty card and a hidden working one, and no test we can write
//      knows which.
//   3. The response already answers the question, exactly, for the point being
//      asked about. A witness beats a model.
//
// So the rule is stated about series rather than about geography:
//
//     A SERIES THAT IS NULL AT EVERY HOUR IS A SERIES THIS PROVIDER DOES NOT
//     HAVE HERE.
//
// Pollen and ammonia are simply the series where that happens to be true today.
// Nothing in the parser knows the word "Europe". If CAMS extends the domain, we
// gain the card the first time somebody looks; if they add a series with the
// same property, it is gated the same way without a code change.
//
// ---- null is not zero, and this is the one that would bite ------------------
//
// Berlin in late July: alder, birch, olive and ragweed are all 0.0 for all 72
// hours, because those species are out of season. Zero grains per cubic metre
// is a measurement. `null` is the absence of one. A gate written as "is any
// value greater than zero" would hide Berlin's pollen card for two thirds of
// the year and would be indistinguishable, in a screenshot, from working.
//
// The test in tests/tst_airquality.cpp asserts exactly that case, because it is
// the one an author fixing this file in a hurry would break.
//
// ============================================================================
// WHAT IS REMEMBERED, AND AT WHAT RESOLUTION
//
// capabilitiesAt() must answer synchronously — it is called while building a
// tab bar — but the verdict above comes out of a payload. Before the first
// fetch for a place, the honest answer is Capabilities::isUndetermined(), and
// after it the verdict is remembered so the answer is instant thereafter.
//
// The verdict is keyed by the coordinate rounded to ONE decimal place, which is
// ~11 km, which is the resolution of the CAMS Europe grid itself
// (docs/02-data-sources.md §2.6: "Europe 11 km"). One remembered verdict per
// CAMS cell. Not four decimals — that is the *request* quantisation from
// libclima/domain/coordinate.h, and at four decimals a user who moved the map
// eleven metres would re-learn a continental fact. Not whole degrees either:
// a 111 km cell straddles the domain boundary, and this cache is allowed to be
// stale but not to be wrong.
//
// It is in-memory and per-process on purpose. Persisting it would mean a
// migration, an expiry policy and a bug report that begins "the pollen card
// stopped appearing after I travelled", to save one request per location per
// launch. The parsed payload itself is cached by CacheStore under §4.5's
// 60-minute air-quality TTL, which is the saving that actually matters.
//
// ---- the verdict never overrides the payload --------------------------------
//
// A remembered "yes" makes a tab appear before data arrives. It does not make a
// card render. AirQuality::hasPollen and AirQualityPoint::pollen are recomputed
// from every response, so the worst a stale verdict can do is reserve space for
// a card that then does not draw. It cannot invent a number, which is the only
// property this cache actually has to have.

#pragma once

#include "libclima/providers/iforecastprovider.h"

#include <QHash>
#include <QObject>
#include <QString>

namespace clima {

class CacheStore;
class Clock;
class HttpClient;

class OpenMeteoAirQualityProvider : public QObject, public IAirQualityProvider
{
    Q_OBJECT

public:
    // Neither is owned and both must outlive this. Same rule as everything else
    // that takes a Clock — libclima/core/clock.h — and the same reason: a
    // provider that constructed its own network client would be a provider
    // outside the User-Agent, coalescing and 403 policy that client exists to
    // enforce.
    OpenMeteoAirQualityProvider(HttpClient *http, Clock *clock, QObject *parent = nullptr);
    ~OpenMeteoAirQualityProvider() override;

    // The host, for tests. A loopback stub is pointed at by overriding this;
    // production never calls it. It is a setter rather than a constructor
    // parameter so that the real URL is the default and a test has to say it is
    // doing something unusual.
    void setBaseUrl(const QUrl &url);

    // Where the last good payload is kept across launches. Not owned, must
    // outlive this, null turns the persistent half of §4.5 off.
    void setCache(CacheStore *cache);

    [[nodiscard]] QString     id() const override;
    [[nodiscard]] QString     displayName() const override;
    [[nodiscard]] Attribution attribution() const override;
    [[nodiscard]] bool        covers(Coordinate coord) const override;
    [[nodiscard]] Capabilities capabilitiesAt(Coordinate coord) const override;

    QFuture<Result<AirQuality>> fetchAirQuality(const ForecastRequest &request) override;

    // Parsing, without a network, a client or an event loop. Public because the
    // golden-file tests in docs/04-architecture.md §4.11 are tests of exactly
    // this function, and because tools/provider-probe reads a recorded fixture
    // through it to print what the app would show.
    //
    // `fetchedAt` is passed in rather than read from the clock so that a fixture
    // parsed twice produces two identical AirQuality values.
    static Result<AirQuality> parse(const QByteArray &body, const QDateTime &fetchedAt);

    // The number of remembered verdicts. For tests: proving that a second
    // capabilitiesAt() for the same CAMS cell did not need a second fetch is
    // proving this stayed at one.
    [[nodiscard]] int rememberedVerdictCount() const;

private:
    // See the header: one decimal place, one CAMS Europe cell.
    static QString verdictKey(Coordinate coord);

    void remember(const AirQuality &airQuality);

    HttpClient *m_http = nullptr;
    Clock      *m_clock = nullptr;
    CacheStore *m_cache = nullptr;
    QUrl        m_baseUrl;

    struct Verdict {
        bool pollen = false;
        bool ammonia = false;
    };

    // Written only from remember(), which runs when a payload has been parsed.
    // capabilitiesAt() is const and reads it; it never learns anything, which
    // is what keeps that function free of the network it must not touch.
    QHash<QString, Verdict> m_verdicts;

    // The last successfully parsed payload per RequestKey, so that a 304 has
    // something to answer with. See fetchAirQuality() for why this lives here
    // and not in CacheStore.
    QHash<QString, AirQuality> m_lastParsed;
};

} // namespace clima
