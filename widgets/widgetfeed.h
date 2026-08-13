// SPDX-FileCopyrightText: 2026 Jowi Aoun
// SPDX-License-Identifier: GPL-3.0-or-later
//
// One tile's subscription, declared where the tile is.
//
//     WidgetFeed {
//         place:  host.place
//         fields: ["place", "current.temperature", "current.weatherCode"]
//         hours:  0
//         days:   0
//     }
//
// A widget declares what it needs and reads `snapshot` — it never sees a bus,
// a token or a match rule. The mask is not a suggestion: the daemon sends only
// what was asked for, so a wind rose is not sent 408 hourly points.
//
// In practice `fields`, `hours` and `days` are read out of the catalogue entry
// rather than written here, so a widget's contract with the daemon lives in
// widgets/catalogue.json and nowhere else.
//
// ============================================================================
// WHY THE SNAPSHOT IS A QVariantMap AND NOT A TYPED OBJECT
//
// Because the daemon's JSON keeps *absent* distinguishable from *zero* (rule 2
// in libclima/wire/snapshot.h), and QVariantMap is the shape that survives the
// trip: a JSON null becomes a null QVariant, which QML reads as `null`, and
// `wire.js` turns that into an em dash rather than into 0.
//
// A typed C++ object would mean a property per field, which is 40-odd
// properties that all have to answer "I do not know" somehow, and the somehow
// would be a sentinel. We have been here before — see the header of
// libclima/domain/forecast.h, where a plain double for a missing gust made the
// hero read "Feels like 0°" on a 28 °C afternoon.
//
// ============================================================================
// WHAT NEVER HAPPENS HERE
//
// The snapshot is never cleared. Not when the daemon exits, not when a
// re-subscribe is in flight, not when the place changes. A tile that has ever
// had a reading goes on drawing it, with `state` and `age` saying how old it
// is. Blanking is the failure mode this whole design exists to avoid.

#pragma once

#include <QDateTime>
#include <QObject>
#include <QQmlEngine>
#include <QQmlParserStatus>
#include <QString>
#include <QStringList>
#include <QVariantMap>

class WidgetFeed : public QObject, public QQmlParserStatus
{
    Q_OBJECT
    QML_ELEMENT
    Q_INTERFACES(QQmlParserStatus)

    // ---- what the tile asks for -------------------------------------------

    // "" and "home" both mean the home place. The daemon reports back the
    // canonical id it resolved to, in `snapshot.placeId`.
    Q_PROPERTY(QString place READ place WRITE setPlace NOTIFY requestChanged)
    Q_PROPERTY(QStringList fields READ fields WRITE setFields NOTIFY requestChanged)
    Q_PROPERTY(int hours READ hours WRITE setHours NOTIFY requestChanged)
    Q_PROPERTY(int days READ days WRITE setDays NOTIFY requestChanged)

    // ---- what the tile draws ----------------------------------------------

    Q_PROPERTY(QVariantMap snapshot READ snapshot NOTIFY snapshotChanged)

    // True once anything at all has arrived. A tile shows its skeleton until
    // this is true and never afterwards — see the class comment.
    Q_PROPERTY(bool hasData READ hasData NOTIFY snapshotChanged)

    // "live" | "cached" | "unknown", straight from the wire. Three values and
    // not two, because "a reading that was true forty minutes ago" and "no
    // reading at all" render identically to a reader that conflates them.
    Q_PROPERTY(QString state READ state NOTIFY snapshotChanged)

    // When the data was fetched, not when the snapshot was built. This is what
    // "updated 40 minutes ago" is computed from.
    Q_PROPERTY(QDateTime fetchedAt READ fetchedAt NOTIFY snapshotChanged)

    // Whole minutes since `fetchedAt`, or -1 when there is nothing to age.
    // Recomputed on a timer as well as on delivery, so a tile whose daemon
    // died keeps counting up instead of freezing at the last number it saw.
    Q_PROPERTY(int ageMinutes READ ageMinutes NOTIFY ageChanged)

    // ---- why there is still nothing ----------------------------------------
    //
    // A sentence to draw in place of the skeleton, or empty while a snapshot is
    // genuinely on its way.
    //
    // The skeleton in WidgetSurface was designed for the moment before the
    // first snapshot arrives, and for that moment it is exactly right. What it
    // was never honest about is the state it is actually in most of the time:
    // there is no daemon, nothing is coming, and three grey bars go on claiming
    // to be loading for as long as the desktop is up. That is the one failure
    // in this process that a user cannot tell apart from a hang — and the file
    // that draws it says so in its own comment ("most of the time this tile is
    // waiting for a daemon that has not been started").
    //
    // So DaemonLink sets this the moment it knows nothing is coming, and it is
    // the whole of what separates "loading" from "there is nothing to load
    // from". The sentence is built in C++, beside the code that establishes the
    // fact, rather than mapped from a code in QML — the same shape as
    // DaemonLink::incompatibility, which is the other state that replaces a
    // tile's body with words.
    //
    // Cleared the instant anything arrives. A tile with data draws the data and
    // its age, never a message; see the state order in WidgetSurface.qml.
    Q_PROPERTY(QString waitingReason READ waitingReason NOTIFY waitingReasonChanged)

public:
    explicit WidgetFeed(QObject *parent = nullptr);
    ~WidgetFeed() override;

    void classBegin() override;
    void componentComplete() override;

    [[nodiscard]] QString     place() const { return m_place; }
    [[nodiscard]] QStringList fields() const { return m_fields; }
    [[nodiscard]] int         hours() const { return m_hours; }
    [[nodiscard]] int         days() const { return m_days; }

    void setPlace(const QString &place);
    void setFields(const QStringList &fields);
    void setHours(int hours);
    void setDays(int days);

    [[nodiscard]] QVariantMap snapshot() const { return m_snapshot; }
    [[nodiscard]] bool        hasData() const { return m_hasData; }
    [[nodiscard]] QString     state() const;
    [[nodiscard]] QDateTime   fetchedAt() const { return m_fetchedAt; }
    [[nodiscard]] int         ageMinutes() const;
    [[nodiscard]] QString     waitingReason() const { return m_waitingReason; }

    // Called by DaemonLink when a snapshot for this feed's token arrives.
    void deliver(const QVariantMap &snapshot);

    // Also DaemonLink's, and not QML's — which is why it is a plain setter and
    // not a WRITE on the property. A tile declares what it needs; why it has
    // not been given it is a fact about the bus, and the bus is the one thing a
    // widget file is not allowed to know about.
    void setWaitingReason(const QString &reason);

    // Ask the daemon to refresh this feed's place now.
    Q_INVOKABLE void refresh();

Q_SIGNALS:
    void requestChanged();
    void snapshotChanged();
    void ageChanged();
    void waitingReasonChanged();

private:
    void requestResubscribe();

    QString     m_place = QStringLiteral("home");
    QStringList m_fields;
    int         m_hours = 0;
    int         m_days  = 0;

    QVariantMap m_snapshot;
    bool        m_hasData  = false;
    bool        m_complete = false;
    QDateTime   m_fetchedAt;
    QString     m_waitingReason;
};
