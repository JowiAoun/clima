// SPDX-FileCopyrightText: 2026 Jowi Aoun
// SPDX-License-Identifier: MPL-2.0
//
// A severe-weather alert, and the four timestamps that decide whether it is on
// the screen.
//
// ============================================================================
// THE ONE THAT WOULD SHIP WRONG: `expires` IS NOT WHEN THE WEATHER STOPS
//
// CAP 1.2 gives an alert four instants, and the two in the middle are the ones
// everybody gets backwards:
//
//     sent        when the message was written
//     effective   when the message becomes the current word on the subject
//     onset       when the HAZARD begins
//     expires     when the MESSAGE goes stale and must be refreshed
//     ends        when the HAZARD is over
//
// `expires` is a property of the paperwork. It is the issuer promising to send
// another message by then, and it routinely falls *before* the weather it
// describes. Verified live on 2026-08-05, recorded in
// tests/fixtures/alerts/nws/siskiyou-heat-advisory.json:
//
//     onset    2026-08-05T14:15-07:00
//     expires  2026-08-06T05:00-07:00      <- message refresh deadline
//     ends     2026-08-06T23:00-07:00      <- heat stops
//
// Eighteen hours between them, and 19 of the 25 alerts in force in California
// that afternoon had the same shape. An app that hides an alert at `expires`
// takes the Heat Advisory off the screen at five in the morning and leaves it
// off through the hottest day of the year.
//
// So the rule, and it is the reason this file has a function rather than a
// comparison at each call site:
//
//     THE HAZARD ENDS AT `ends`. WHERE THERE IS NO `ends`, AND ONLY THERE,
//     `expires` STANDS IN FOR IT.
//
// `ends` really is absent sometimes — all three Air Quality Alerts in
// tests/fixtures/alerts/nws/seattle-four.json have `ends: null` — which is why
// the fallback exists at all and why it is not the primary rule.
//
// docs/06-roadmap.md §6.6 says "no *expired* alert is ever displayed". As
// written that mandates the bug above. It is corrected there to "no *ended*
// alert", and this file is what the corrected sentence means.
//
// ---- what `expires` IS for --------------------------------------------------
//
// Confidence, not visibility. Past `expires` the issuer said they would have
// spoken again; if our last poll succeeded and the alert was still in it, the
// alert is current whatever `expires` says. If our last poll FAILED, we are
// holding a message its author has already disowned, and the honest thing is to
// keep showing it and say when we last confirmed it. Never silently keep it,
// never silently drop it — AlertSet::confirmedAt is that sentence's data.
//
// ============================================================================
// SEVERITY IS CAP'S, AND THE ISSUER'S OWN WORDS TRAVEL BESIDE IT
//
// Five values, ordered, from CAP 1.2. Every provider maps into them so that one
// banner can rank a Canadian heat warning against an American air quality alert
// without knowing what either service calls things.
//
// That mapping is lossy in a way that matters to a reader. "Moderate" is what
// the app ranks with; "yellow warning · heat warning" is what weather.gc.ca
// shows and what a Nova Scotian recognises. Both are carried. `severity` is for
// sorting and for choosing a colour; `issuerLabel` is for the sentence under
// the headline, and a UI that shows only the first has translated a warning
// into a vocabulary its reader has never seen.
//
// ---- Unknown is a real value and it is not "probably fine" ------------------
//
// severity `Unknown` is what NWS sends for every Air Quality Alert — six of the
// nine alerts recorded in tests/fixtures/alerts/nws/. It means the issuer did
// not classify, not that the classification is low. It therefore sorts BELOW
// Minor for ranking, because something graded Minor was graded, and it must
// still be displayed, because an unclassified alert is still an alert.
//
// ============================================================================
// IDENTITY: WHY THIS IS A LIST OF KEYS AND NOT A STRING
//
// Acknowledgement — see the banner — is remembered per hazard, not per message,
// so the app has to be able to say "this is the alert you already dismissed"
// about a message it has never seen before. The two services make that a
// different problem each:
//
//   NWS re-sends the whole alert on every update, with a NEW id, and lists the
//   ids it supersedes in `references`. 24 of the 25 alerts in force in
//   California were messageType Update. So identity has to follow the chain.
//
//   ECCC does not have references. It has `alert_code` and `feature_id` —
//   "heat warning" and "Annapolis County" — and it issues one alert per type
//   per area, so that pair IS the hazard.
//
// And the shape that rules out the obvious shared answer: Seattle had TWO Air
// Quality Alerts with identical `event`, identical `senderName` and an
// identical SAME geocode list, differing only in `effective`. They are two
// distinct hazards. Any key built from (event, area) merges them into one and
// silently hides the second.
//
// So `identityKeys()` returns every string this alert may be recognised by, and
// two alerts are the same hazard when their key sets INTERSECT. NWS contributes
// its own id plus every id it references; ECCC contributes one key built from
// the pair that is stable by construction. A dismissal is stored against all of
// an alert's keys, so the next message in a chain matches on the reference it
// carries. Nothing has to be migrated and nothing has to be chased.

#pragma once

#include "libclima/domain/coordinate.h"

#include <QDateTime>
#include <QList>
#include <QString>
#include <QStringList>
#include <QUrl>

namespace clima {

// ---- CAP 1.2 enumerations ------------------------------------------------------
//
// Ordered lowest to highest so that `<` means what it reads as. `Unknown` is
// first for exactly that reason — see the header.

enum class AlertSeverity {
    Unknown = 0,
    Minor,
    Moderate,
    Severe,
    Extreme,
};

enum class AlertUrgency {
    Unknown = 0,
    Past,        // the responsive action is no longer relevant
    Future,      // take action in the near future
    Expected,    // within the next hour
    Immediate,   // now
};

enum class AlertCertainty {
    Unknown = 0,
    Unlikely,
    Possible,
    Likely,
    Observed,
};

// CAP's msgType. Carried rather than acted on by the primary providers — both
// of them serve an *active* collection, so a cancelled alert is one that is no
// longer in the answer — but the CAP feeds a fallback would read do send Cancel
// explicitly, and a field that arrives with the parser is a field that does not
// have to be retrofitted around one.
enum class AlertMessageType {
    Alert = 0,
    Update,
    Cancel,
    Ack,
    Error,
};

QString alertSeverityName(AlertSeverity severity);
QString alertUrgencyName(AlertUrgency urgency);
QString alertCertaintyName(AlertCertainty certainty);

// Lowercase, stable, and the string QML indexes a token group by:
// "extreme", "severe", "moderate", "minor", "unknown". Separate from
// alertSeverityName() — that one is for logs and may be capitalised or
// translated; this one is a key and must not move.
QString alertSeverityKey(AlertSeverity severity);

// ---- when an alert is on the screen ---------------------------------------------

enum class AlertPhase {
    // effective is in the future: the issuer has not made this the current word
    // yet. Not drawn.
    NotYet,

    // Effective, but the hazard has not started. Drawn, with "begins 12:00 PM" —
    // this is the Heat Advisory issued at breakfast for an afternoon that has
    // not arrived, and hiding it is the failure docs/06 §6.6 was corrected for.
    Pending,

    // The hazard is running.
    Active,

    // Past `ends`, or past `expires` where there is no `ends`. Not drawn, ever,
    // by anything.
    Ended,
};

QString alertPhaseName(AlertPhase phase);

// ---- the alert -------------------------------------------------------------------

struct Alert {
    // Provider-scoped and unique per MESSAGE. Not the hazard's identity — see
    // identityKeys(), and the header for why those are different things.
    QString id;
    QString providerId;

    // "Heat Advisory", "heat warning". The issuer's name for the event, in the
    // issuer's own capitalisation, because that is what it is called on their
    // site.
    QString event;

    // The issuer's one-line summary where they wrote one. NWS sends a `headline`
    // ("Heat Advisory issued August 5 at 2:15PM PDT until…"); ECCC does not, and
    // a provider that has none leaves this empty rather than manufacturing one
    // out of the other fields. An empty headline means the UI shows `event`.
    QString headline;

    // The body, and the "what you should do" paragraph, kept apart because the
    // sheet renders them differently and joining them is not reversible.
    QString description;
    QString instruction;

    // "Western Siskiyou County; Central Siskiyou County", "Annapolis County".
    QString areaDescription;

    // "NWS Medford OR", "Environment and Climate Change Canada".
    QString senderName;

    AlertSeverity  severity  = AlertSeverity::Unknown;
    AlertUrgency   urgency   = AlertUrgency::Unknown;
    AlertCertainty certainty = AlertCertainty::Unknown;

    AlertMessageType messageType = AlertMessageType::Alert;

    // The issuer's own grading, spelled the way they spell it: "yellow warning",
    // "Severe". Shown to the reader; never parsed. See the header.
    QString issuerLabel;

    // The four instants. Any of them may be invalid; the accessors below are
    // where that is handled, once.
    QDateTime sent;
    QDateTime effective;
    QDateTime onset;
    QDateTime expires;
    QDateTime ends;

    // Where the issuer's own page for this is, when they gave one.
    QUrl web;

    // Every string this alert may be recognised by. Populated by the provider —
    // see the header for what each one puts in it. Never empty: a provider with
    // nothing better contributes the message id, which at least makes the alert
    // identical to itself.
    QStringList identityKeys;

    // ---- the whole point of the file ---------------------------------------

    // `ends`, or `expires` where there is no `ends`. Invalid when there is
    // neither, which no observed payload has produced and which is treated as
    // "no end stated" — such an alert stays visible until it leaves the feed.
    [[nodiscard]] QDateTime hazardEnd() const;

    [[nodiscard]] AlertPhase phaseAt(const QDateTime &now) const;

    // Phase is NotYet or Ended. The single question a view asks.
    [[nodiscard]] bool isDisplayableAt(const QDateTime &now) const;

    // True once the issuer's own refresh deadline has passed. NOT a reason to
    // hide anything — see the header — only a reason to say when we last
    // confirmed it, and only when the poll that would have confirmed it failed.
    [[nodiscard]] bool isPastRefreshDeadline(const QDateTime &now) const;

    // Do these two messages describe the same hazard? Key-set intersection.
    [[nodiscard]] bool isSameHazard(const Alert &other) const;

    // Higher ranks first in the banner. Severity, then urgency, then certainty,
    // then the earlier onset — an Extreme that starts tomorrow still outranks a
    // Severe that started this morning, because the banner shows one alert and
    // the one it must not omit is the worst one.
    [[nodiscard]] bool outranks(const Alert &other) const;

    [[nodiscard]] bool isValid() const;
};

// ---- what one poll produced ------------------------------------------------------

struct AlertSet {
    QList<Alert> alerts;

    // The coordinate the question was asked about, not one a service snapped to:
    // neither ECCC nor NWS reports a coordinate back, and inventing one would
    // make this field the only lie in the struct.
    Coordinate coordinate;

    // When these bytes arrived. Drives "updated N minutes ago" exactly as
    // Forecast::fetchedAt does.
    QDateTime fetchedAt;

    // The last time a poll SUCCEEDED, which is not the same as when this set was
    // built: a set served from cache after a failed refresh keeps the older
    // instant, and that is what "last confirmed 14:05" is read from.
    QDateTime confirmedAt;

    // Comma-joined provider ids, in the order they answered. Plural because
    // alerts fan out rather than fall back — see registry.h.
    QString providerId;

    // False when at least one provider that covers this place did not answer.
    // The set is then everything we could get and not everything there is, and
    // the sheet says so. A UI that renders a partial set as complete is telling
    // the user there are no tornado warnings when what it means is that it could
    // not reach the service that would know.
    bool complete = true;

    // Sorted by outranks(), highest first, with anything not displayable at
    // `now` removed. This is what a view binds to; nothing outside this function
    // decides what is on the screen.
    [[nodiscard]] QList<Alert> displayableAt(const QDateTime &now) const;

    [[nodiscard]] bool isValid() const { return fetchedAt.isValid(); }
};

} // namespace clima
