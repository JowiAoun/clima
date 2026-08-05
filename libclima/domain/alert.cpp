// SPDX-FileCopyrightText: 2026 Jowi Aoun
// SPDX-License-Identifier: MPL-2.0

#include "libclima/domain/alert.h"

#include <QSet>

#include <algorithm>

namespace clima {

QString alertSeverityName(AlertSeverity severity)
{
    switch (severity) {
    case AlertSeverity::Unknown:  return QStringLiteral("Unknown");
    case AlertSeverity::Minor:    return QStringLiteral("Minor");
    case AlertSeverity::Moderate: return QStringLiteral("Moderate");
    case AlertSeverity::Severe:   return QStringLiteral("Severe");
    case AlertSeverity::Extreme:  return QStringLiteral("Extreme");
    }
    return QStringLiteral("Unknown");
}

QString alertSeverityKey(AlertSeverity severity)
{
    switch (severity) {
    case AlertSeverity::Unknown:  return QStringLiteral("unknown");
    case AlertSeverity::Minor:    return QStringLiteral("minor");
    case AlertSeverity::Moderate: return QStringLiteral("moderate");
    case AlertSeverity::Severe:   return QStringLiteral("severe");
    case AlertSeverity::Extreme:  return QStringLiteral("extreme");
    }
    return QStringLiteral("unknown");
}

QString alertUrgencyName(AlertUrgency urgency)
{
    switch (urgency) {
    case AlertUrgency::Unknown:   return QStringLiteral("Unknown");
    case AlertUrgency::Past:      return QStringLiteral("Past");
    case AlertUrgency::Future:    return QStringLiteral("Future");
    case AlertUrgency::Expected:  return QStringLiteral("Expected");
    case AlertUrgency::Immediate: return QStringLiteral("Immediate");
    }
    return QStringLiteral("Unknown");
}

QString alertCertaintyName(AlertCertainty certainty)
{
    switch (certainty) {
    case AlertCertainty::Unknown:  return QStringLiteral("Unknown");
    case AlertCertainty::Unlikely: return QStringLiteral("Unlikely");
    case AlertCertainty::Possible: return QStringLiteral("Possible");
    case AlertCertainty::Likely:   return QStringLiteral("Likely");
    case AlertCertainty::Observed: return QStringLiteral("Observed");
    }
    return QStringLiteral("Unknown");
}

QString alertPhaseName(AlertPhase phase)
{
    switch (phase) {
    case AlertPhase::NotYet:  return QStringLiteral("NotYet");
    case AlertPhase::Pending: return QStringLiteral("Pending");
    case AlertPhase::Active:  return QStringLiteral("Active");
    case AlertPhase::Ended:   return QStringLiteral("Ended");
    }
    return QStringLiteral("Ended");
}

// ---- Alert -----------------------------------------------------------------------

QDateTime Alert::hazardEnd() const
{
    // The whole argument is in the header. `ends` first, always; `expires` only
    // where there is no `ends`, which is the shape all three Seattle Air Quality
    // Alerts arrive in.
    if (ends.isValid())
        return ends;
    return expires;
}

AlertPhase Alert::phaseAt(const QDateTime &now) const
{
    if (!now.isValid())
        return AlertPhase::Ended;

    if (effective.isValid() && now < effective)
        return AlertPhase::NotYet;

    const QDateTime end = hazardEnd();
    if (end.isValid() && now >= end)
        return AlertPhase::Ended;

    // Effective, not over, and the weather has not started. The banner says
    // "begins 12:00 PM" rather than pretending it is happening — and rather than
    // hiding it, which is what an app that keys visibility off `onset` alone
    // would do to every advisory issued in advance.
    if (onset.isValid() && now < onset)
        return AlertPhase::Pending;

    return AlertPhase::Active;
}

bool Alert::isDisplayableAt(const QDateTime &now) const
{
    const AlertPhase phase = phaseAt(now);
    return phase == AlertPhase::Pending || phase == AlertPhase::Active;
}

bool Alert::isPastRefreshDeadline(const QDateTime &now) const
{
    return expires.isValid() && now.isValid() && now >= expires;
}

bool Alert::isSameHazard(const Alert &other) const
{
    // Provider-scoped by construction: every provider prefixes its keys with its
    // own id, so an ECCC key and an NWS key cannot collide however similar the
    // hazards are. Asserting that here would be asserting it in the wrong place;
    // the parsers are where the prefix is applied and where it is tested.
    for (const QString &key : identityKeys) {
        if (other.identityKeys.contains(key))
            return true;
    }
    return false;
}

bool Alert::outranks(const Alert &other) const
{
    if (severity != other.severity)
        return severity > other.severity;
    if (urgency != other.urgency)
        return urgency > other.urgency;
    if (certainty != other.certainty)
        return certainty > other.certainty;

    // Same grade on all three axes: the one that starts sooner is the one to
    // show. An invalid onset sorts last rather than first — a missing timestamp
    // is not evidence of imminence.
    const bool mine  = onset.isValid();
    const bool yours = other.onset.isValid();
    if (mine != yours)
        return mine;
    if (mine && onset != other.onset)
        return onset < other.onset;

    // Total, so that a sort is stable across runs and a golden image of a
    // two-alert banner does not alternate. Nothing below here is meaningful;
    // it only has to be deterministic.
    return id < other.id;
}

bool Alert::isValid() const
{
    return !id.isEmpty() && !event.isEmpty();
}

// ---- AlertSet ---------------------------------------------------------------------

QList<Alert> AlertSet::displayableAt(const QDateTime &now) const
{
    QList<Alert> shown;
    shown.reserve(alerts.size());
    for (const Alert &alert : alerts) {
        if (alert.isDisplayableAt(now))
            shown.append(alert);
    }

    std::stable_sort(shown.begin(), shown.end(),
                     [](const Alert &a, const Alert &b) { return a.outranks(b); });
    return shown;
}

} // namespace clima
