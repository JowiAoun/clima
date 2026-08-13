// SPDX-FileCopyrightText: 2026 Jowi Aoun
// SPDX-License-Identifier: GPL-3.0-or-later

#include "widgetfeed.h"

#include "daemonlink.h"
#include "widgetclock.h"

#include <QDateTime>

WidgetFeed::WidgetFeed(QObject *parent)
    : QObject(parent)
{
    // One process-wide timer drives every tile's age. Connected in the
    // constructor rather than on first delivery, because a tile that never
    // receives anything still has to be able to say so.
    connect(DaemonLink::instance(), &DaemonLink::minutePassed, this, &WidgetFeed::ageChanged);
}

WidgetFeed::~WidgetFeed()
{
    if (m_complete)
        DaemonLink::instance()->detach(this);
}

void WidgetFeed::classBegin() { }

void WidgetFeed::componentComplete()
{
    // Attach here and not in the constructor. QML assigns properties between
    // the two, so subscribing earlier would ask the daemon for the default
    // place with an empty field mask — which the wire format reads as "send
    // everything" (libclima/wire/snapshot.h), so the mistake would be a working
    // widget receiving eight times the payload it needs. Silent, and the sort
    // of thing that is only ever found by looking at bus traffic.
    m_complete = true;
    DaemonLink::instance()->attach(this);
}

// ---- the request ------------------------------------------------------------

void WidgetFeed::requestResubscribe()
{
    if (!m_complete)
        return;
    Q_EMIT requestChanged();
    DaemonLink::instance()->resubscribe(this);
}

void WidgetFeed::setPlace(const QString &place)
{
    if (m_place == place)
        return;
    m_place = place;
    requestResubscribe();
}

void WidgetFeed::setFields(const QStringList &fields)
{
    if (m_fields == fields)
        return;
    m_fields = fields;
    requestResubscribe();
}

void WidgetFeed::setHours(int hours)
{
    if (m_hours == hours)
        return;
    m_hours = hours;
    requestResubscribe();
}

void WidgetFeed::setDays(int days)
{
    if (m_days == days)
        return;
    m_days = days;
    requestResubscribe();
}

// ---- what arrives -----------------------------------------------------------

void WidgetFeed::deliver(const QVariantMap &snapshot)
{
    m_snapshot = snapshot;
    m_hasData  = true;

    m_fetchedAt = QDateTime::fromString(snapshot.value(QStringLiteral("fetchedAt")).toString(),
                                        Qt::ISODate);

    // Data outranks any explanation of its absence, and it keeps outranking it:
    // when the daemon goes away later, this tile is `stale` and says how old its
    // reading is. It must not go back to being a tile with a sentence on it.
    setWaitingReason({});

    Q_EMIT snapshotChanged();
    Q_EMIT ageChanged();
}

void WidgetFeed::setWaitingReason(const QString &reason)
{
    if (m_waitingReason == reason)
        return;
    m_waitingReason = reason;
    Q_EMIT waitingReasonChanged();
}

QString WidgetFeed::state() const
{
    const QString reported = m_snapshot.value(QStringLiteral("state")).toString();
    return reported.isEmpty() ? QStringLiteral("unknown") : reported;
}

int WidgetFeed::ageMinutes() const
{
    if (!m_fetchedAt.isValid())
        return -1;

    // Floored rather than rounded, so a reading taken 119 seconds ago is "1
    // minute" and never "2". A widget's age is a claim about the past and it
    // must not overstate it.
    const qint64 seconds = m_fetchedAt.secsTo(clima::widgets::now());
    return seconds < 0 ? 0 : int(seconds / 60);
}

void WidgetFeed::refresh()
{
    DaemonLink::instance()->requestRefresh(m_place);
}
