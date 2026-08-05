// SPDX-FileCopyrightText: 2026 Jowi Aoun
// SPDX-License-Identifier: MPL-2.0
//
// United States alerts, from the National Weather Service.
//
//     https://api.weather.gov/alerts/active?point=<lat>,<lon>
//
// No key, no account. The service resolves the point to its own UGC zones
// server-side, which is the finding that deleted a subsystem: the alternative
// design ships a zone shapefile, 25–100 MB of it, and does the point-in-polygon
// locally. `?point=` makes that unnecessary and it is not an approximation —
// the zones are the authority and they are being asked directly.
//
// ============================================================================
// A COORDINATE OUTSIDE THE UNITED STATES IS HTTP 400, NOT AN EMPTY LIST
//
// Verified live and recorded in tests/fixtures/alerts/nws/out-of-bounds.json:
//
//     GET /alerts/active?point=44.7,-65.3        (Nova Scotia)
//     400  {"title":"Invalid Parameter",
//           "detail":"Parameter \"point\" is invalid: out of bounds"}
//
// That is not a failure and it must not reach the user as one. It is the
// service saying the question was not for them, which is exactly what
// ErrorKind::Unsupported means — and Unsupported is the one kind the alert
// fan-out in registry.cpp does not count against AlertSet::complete. Reported
// as anything else, a user in Halifax gets "alerts unavailable" forever,
// because Nova Scotia is inside the loose Canadian box and the loose Canadian
// box overlaps Maine.
//
// The cost of mapping 400 to Unsupported is that a 400 caused by OUR bug — a
// malformed parameter — would be swallowed the same way. Two things bound it:
// the request has exactly one parameter and it is built by composeUrl() from a
// validated coordinate, and the message from HttpClient now carries the
// service's own `detail` string, so the log says which of the two happened.
//
// ============================================================================
// IDENTITY FOLLOWS `references`, AND THE REASON IS IN THE FIXTURES
//
// NWS re-sends an alert in full on every update, under a NEW id, listing the
// ids it supersedes in `references`. Twenty-four of the twenty-five alerts in
// force in California on 2026-08-05 were messageType Update. Keying identity on
// the message id therefore means a dismissed alert un-dismisses itself the next
// time the office touches it, which is often.
//
// The obvious alternative — key on (event, sender, area) — is disproved by
// tests/fixtures/alerts/nws/seattle-four.json, which contains TWO Air Quality
// Alerts with:
//
//     the same event          "Air Quality Alert"
//     the same senderName     "NWS Seattle WA"
//     the same SAME geocodes  053033, 053035, 053053, 053061, 053067
//
// differing only in `effective` and in id, referencing nothing and each other
// not at all. They are two separate hazards. A key built from what they share
// merges them, and merging them hides one — which is the failure mode this
// whole workflow exists to prevent, arrived at through tidiness.
//
// So identity is the set {own id} ∪ {referenced ids}, and two messages are the
// same hazard when those sets intersect — libclima/domain/alert.h. An update
// matches its predecessor through the reference it carries; two unrelated
// alerts share nothing and stay two.
//
// ============================================================================
// `ends` IS NULL MORE OFTEN THAN NOT, AND `severity` IS OFTEN "Unknown"
//
// Both are real, both are in the fixtures, and both have a wrong obvious
// handling:
//
//   All three Seattle Air Quality Alerts have `ends: null`. Alert::hazardEnd()
//   falls back to `expires` for exactly this, and that fallback is why it is a
//   function rather than a field.
//
//   Six of the nine recorded alerts have severity "Unknown" — every Air Quality
//   Alert does. Mapped to AlertSeverity::Unknown, which sorts BELOW Minor and
//   still displays. Mapping it to Minor would be inventing a grade the issuer
//   declined to give; dropping it would hide a real alert.

#pragma once

#include "libclima/providers/ialertprovider.h"

#include <QObject>
#include <QString>
#include <QUrl>

namespace clima {

class CacheStore;
class Clock;
class HttpClient;

class NwsAlertProvider : public QObject, public IAlertProvider
{
    Q_OBJECT

public:
    NwsAlertProvider(HttpClient *http, Clock *clock, QObject *parent = nullptr);
    ~NwsAlertProvider() override;

    void setBaseUrl(const QUrl &url);
    void setCache(CacheStore *cache);

    [[nodiscard]] QString      id() const override;
    [[nodiscard]] QString      displayName() const override;
    [[nodiscard]] Attribution  attribution() const override;
    [[nodiscard]] bool         covers(Coordinate coord) const override;
    [[nodiscard]] Capabilities capabilitiesAt(Coordinate coord) const override;

    QFuture<Result<AlertSet>> fetchAlerts(const AlertRequest &request) override;

    // English only — api.weather.gov serves `language: en-US` and has no
    // bilingual field pairs, so unlike ECCC's there is no language parameter to
    // take. `fetchedAt` is passed in so a fixture parsed twice is identical.
    static Result<AlertSet> parse(const QByteArray &body, const QDateTime &fetchedAt);

private:
    HttpClient *m_http = nullptr;
    Clock      *m_clock = nullptr;
    CacheStore *m_cache = nullptr;
    QUrl        m_baseUrl;
};

} // namespace clima
