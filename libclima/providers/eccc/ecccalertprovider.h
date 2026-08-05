// SPDX-FileCopyrightText: 2026 Jowi Aoun
// SPDX-License-Identifier: MPL-2.0
//
// Canadian alerts, from Environment and Climate Change Canada's GeoMet-Weather.
//
//     https://api.weather.gc.ca/collections/weather-alerts/items
//
// No key, no account, no registration. One request per location returns every
// alert whose polygon contains the point, bilingual, with the issuer's own risk
// colour on it.
//
// ============================================================================
// IT IS A BOUNDING BOX, NOT THE CQL2 FILTER — AND THAT IS A CORRECTION
//
// The plan this was built from recorded, from research:
//
//     "Note CQL2 needs the `properties.` prefix; a bare attribute name returns
//      200 matched:0, not an error."
//
// Measured against the live service on 2026-08-05, that is exactly backwards:
//
//     filter=INTERSECTS(properties.geometry,POINT(-65.3 44.7))   HTTP 500
//         {"code":"NoApplicableCode","description":"query error (check logs)"}
//     filter=INTERSECTS(geometry,POINT(-65.3 44.7))              HTTP 200, 1 match
//
// The prefixed form — the one the plan called required — is the one that fails,
// and it fails with a 500 that says nothing. Whichever way round it is today,
// the interesting fact is that the convention MOVED, silently, and that a
// deployment which changes it again turns every Canadian alert off with an
// error no user could report usefully.
//
// So this provider does not use CQL2 at all. It sends:
//
//     ?f=json&bbox=lon,lat,lon,lat
//
// A zero-area bounding box, which is OGC API — Features **Part 1: Core**. CQL2
// filtering is Part 3, an extension, and Part 3 is the part that just moved
// underneath us. Verified equivalent: the same point, same single alert, 10,459
// bytes against the filter's 10,546.
//
// What that costs is the server-side `status_en <> 'ended'` clause, which is
// now applied here after parsing. That is one string comparison over a handful
// of features and it buys independence from an extension whose spelling is not
// stable.
//
// ============================================================================
// GEOMET SENDS NO ETag, NO Last-Modified AND NO Cache-Control
//
// Verified: the response carries `Vary: Accept-Encoding` and nothing else worth
// revalidating against. So the conditional GET that HttpClient sends everywhere
// else is a no-op here, and every poll is a full transfer of about 10 kB.
//
// That is the real polling budget, and it is worth writing down because the
// plan's estimate — "~264 KB/day" — assumed revalidation. At the three-minute
// foreground interval in docs/04-architecture.md §4.5 a full day of foreground
// polling would be nearer 5 MB. Nothing here is foreground for a day; the
// interval backs off when the window is hidden and stops when it is closed,
// which is where that number actually comes down. See
// app/viewmodels/alertsdata.h, which owns the schedule.
//
// ============================================================================
// SEVERITY FROM THE RISK COLOUR, AND WHY NOT FROM `impact`
//
// ECCC grades each alert on a risk matrix and publishes the result as a colour:
// red, orange, yellow. That colour is what weather.gc.ca paints, and it already
// combines the two axes the payload also reports separately (`impact_en`
// Moderate/High, `confidence_en` High/Medium/Low). Mapping severity from the
// colour therefore agrees with what a Canadian reader has already seen, which
// mapping from `impact` alone would not.
//
// The three CAP axes are filled from three different fields, which is the point
// of having three:
//
//     severity   <- risk_colour_en    red Extreme · orange Severe · yellow Moderate
//     urgency    <- alert_type        warning Expected · watch/statement Future
//     certainty  <- confidence_en     High Likely · Medium Possible · Low Unlikely
//
// `alert_type` deliberately does NOT touch severity. A watch is a statement
// about confidence and time, not about magnitude — that is what CAP separates
// urgency and certainty from severity for — and folding it into severity is how
// a tornado watch ends up ranked below a heat warning.
//
// The issuer's own words survive all of this: `issuerLabel` is built as
// "yellow warning", and the sheet shows it next to `event`. See
// libclima/domain/alert.h.
//
// ============================================================================
// IDENTITY IS (alert_code, feature_id)
//
// ECCC has no CAP `references` chain. What it has is `alert_code` ("EHW") and
// `feature_id` ("fea1-786", the county), and it issues one alert of a given
// type per area — a second heat warning for Annapolis County is not a thing
// that exists, it is the same warning `status_en: continued`. So that pair is
// the hazard's identity, and it is stable across re-issue by construction,
// which the message id is not: the id embeds an issue timestamp.
//
// This differs from the NWS provider on purpose, and the difference is real
// rather than stylistic — see libclima/providers/nws/nwsalertprovider.h, where
// two Air Quality Alerts share every field this provider would key on.

#pragma once

#include "libclima/providers/ialertprovider.h"

#include <QObject>
#include <QString>
#include <QUrl>

namespace clima {

class CacheStore;
class Clock;
class HttpClient;

class EcccAlertProvider : public QObject, public IAlertProvider
{
    Q_OBJECT

public:
    // Neither is owned and both must outlive this. Same rule as every other
    // provider, same reason: a provider that built its own network client would
    // be a provider outside the User-Agent and 403 policy HttpClient exists to
    // enforce.
    EcccAlertProvider(HttpClient *http, Clock *clock, QObject *parent = nullptr);
    ~EcccAlertProvider() override;

    void setBaseUrl(const QUrl &url);
    void setCache(CacheStore *cache);

    [[nodiscard]] QString      id() const override;
    [[nodiscard]] QString      displayName() const override;
    [[nodiscard]] Attribution  attribution() const override;
    [[nodiscard]] bool         covers(Coordinate coord) const override;
    [[nodiscard]] Capabilities capabilitiesAt(Coordinate coord) const override;

    QFuture<Result<AlertSet>> fetchAlerts(const AlertRequest &request) override;

    // Parsing, without a network, a client or an event loop — the shape every
    // provider here exposes, and what tests/tst_ecccalerts.cpp actually tests.
    //
    // `language` picks between the `_en` and `_fr` field pairs. `fetchedAt` is
    // passed in rather than read from a clock so that one fixture parsed twice
    // produces two identical AlertSets.
    static Result<AlertSet> parse(const QByteArray &body, const QDateTime &fetchedAt,
                                  const QString &language);

private:
    HttpClient *m_http = nullptr;
    Clock      *m_clock = nullptr;
    CacheStore *m_cache = nullptr;
    QUrl        m_baseUrl;
};

} // namespace clima
