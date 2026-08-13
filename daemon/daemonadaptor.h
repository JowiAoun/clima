// SPDX-FileCopyrightText: 2026 Jowi Aoun
// SPDX-License-Identifier: GPL-3.0-or-later
//
// The daemon's session-bus face. Six methods and one signal, and nothing in
// here does any work — it exists so that the D-Bus surface is one small file a
// reader can hold in their head, separate from the fetching underneath it.
//
// ============================================================================
// THE SHAPE OF THE INTERFACE, AND WHY SUBSCRIBE RETURNS A TOKEN
//
// A D-Bus signal is a broadcast: every subscriber on the connection is woken
// for every emission. That is exactly wrong for a desktop with eight widgets
// on it, because a wind rose does not want to be woken — or to parse a
// snapshot — because a seven-day strip refreshed.
//
// So Subscribe() hands back a token and it is the *first argument* of
// SnapshotChanged. A reader adds a match rule with arg0='<its token>' and the
// bus daemon does the filtering before the message is ever delivered. One
// widget's refresh does not cost the other seven a wakeup, which is what makes
// the ~0% idle CPU line in docs/03-tech-stack.md §3.4 survive contact with a
// desktop full of tiles.
//
// ============================================================================
// EVERYTHING IS A STRING OF JSON
//
// GetSnapshot and ListWidgets both return `s`, not a typed structure. The
// reasoning is in libclima/wire/snapshot.h and it is about version skew: the
// GNOME extension ships from extensions.gnome.org and the app from Flathub,
// they will routinely disagree by a version, and an unknown key must be
// ignorable rather than an unmarshalling error.
//
// SchemaVersion() is how a reader finds out whether it can understand what it
// is about to be given, and it is the one call that must never change shape.

#pragma once

#include <QDBusAbstractAdaptor>
#include <QObject>
#include <QString>
#include <QStringList>

#include "daemonconfig.h"

class SnapshotService;

class DaemonAdaptor : public QDBusAbstractAdaptor
{
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", CLIMA_DAEMON_INTERFACE)

public:
    // `service` is also the parent: QDBusAbstractAdaptor has to be a child of
    // the object registered on the bus, and registering the service object is
    // what exports this.
    explicit DaemonAdaptor(SnapshotService *service);

public Q_SLOTS:
    // The version of the JSON, not of this interface. Check it first.
    [[nodiscard]] int SchemaVersion() const;

    // One snapshot, now, masked. `fields` empty means everything; `hours` and
    // `days` are how much of each series to send, 0 for none and -1 for all.
    [[nodiscard]] QString GetSnapshot(const QString    &placeId,
                                      const QStringList &fields,
                                      int                hours,
                                      int                days);

    // The same arguments, kept. Returns a token to match SnapshotChanged on,
    // or an empty string if the place could not be resolved. The first
    // snapshot arrives by signal without a further call.
    [[nodiscard]] QString Subscribe(const QString    &placeId,
                                    const QStringList &fields,
                                    int                hours,
                                    int                days);

    bool Unsubscribe(const QString &token);

    // Ask now rather than at the next poll. Whether a socket is opened is
    // still the cache policy's decision, so this is not a way to hammer a
    // provider — it is what a widget calls when a user clicks refresh.
    void RequestRefresh(const QString &placeId);

    // widgets/catalogue.json, verbatim. Served from here so that the widget
    // host has no copy of its own: one file, one reader, at run time as well
    // as in the repository.
    [[nodiscard]] QString ListWidgets();

    // Canonical place ids the daemon can answer for. "home" always works and
    // is not in the list, because it is an alias rather than a place.
    [[nodiscard]] QStringList ListPlaces();

Q_SIGNALS:
    void SnapshotChanged(const QString &token, const QString &json);

    // The saved places changed: one was added, removed, moved or made home.
    // Carries nothing — a reader that cares calls Subscribe again, which is
    // the only thing it could do with any argument this might have had.
    //
    // Additive, so it does not move the trailing 1 on the interface name. An
    // older reader never asks for it and is unaffected; existing subscriptions
    // are re-pointed at the new place before this goes out, so ignoring it
    // costs nothing except in the one case where there was no subscription to
    // re-point — a widget that came up before the user had chosen a place.
    void PlacesChanged();

private:
    SnapshotService *m_service;
};
