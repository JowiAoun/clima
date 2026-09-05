// SPDX-FileCopyrightText: 2026 Jowi Aoun
// SPDX-License-Identifier: GPL-3.0-or-later
//
// A desktop notification, for the reader who is not looking at the window.
//
// ============================================================================
// TWO ROUTES, IN ORDER
//
//   1. org.freedesktop.portal.Notification   the portal. Works from inside a
//                                            Flatpak with nothing declared,
//                                            and the desktop draws it in its
//                                            own style with the app's own
//                                            icon. Tried first.
//   2. org.freedesktop.Notifications         the service every Linux desktop
//                                            has implemented since 2005. For
//                                            a session with no portal — a
//                                            bare compositor, an old
//                                            distribution.
//
// The fallback is taken only when the portal *answers* that it cannot — the
// name is not on the bus, or the interface is not on the object — and not on
// any other error. A portal that exists and refuses has made a decision, and
// going around it to the service underneath would be exactly the thing a
// portal is there to prevent.
//
// ============================================================================
// WHAT IS NOT HERE
//
// Actions. A notification here says what the desktop's own alert would say —
// the event, the grade, until when — and clicking it is the desktop's
// business. Wiring "open the app on the sheet" needs a route back into a
// process that may have been closed, which is the daemon's job and a later
// one.
//
// Windows toasts and macOS Notification Center. Neither has a session bus,
// and this class is compiled to a stub without one: available() is false, the
// preference row does not appear, and nothing is promised. docs/04 §4.9 lists
// both; the shape here — an id, a title, a body, a priority — is the shape
// each of them wants, so the port is a second file and not a redesign.

#pragma once

#include <QHash>
#include <QObject>
#include <QString>

class QDBusPendingCallWatcher;

class Notifier : public QObject
{
    Q_OBJECT

public:
    enum class Priority { Low, Normal, High, Urgent };
    Q_ENUM(Priority)

    explicit Notifier(QObject *parent = nullptr);
    ~Notifier() override;

    // Whether this build can post one at all — that is, whether it was
    // compiled with Qt D-Bus. What decides whether the preference row is
    // shown, and deliberately a question about the BUILD rather than about
    // the session.
    //
    // Asking whether a session bus is connected would be the more precise
    // question and it is the wrong one here, for a reason that is about
    // pictures. Every golden image is rendered headless, where there is no
    // session bus, while a developer's `--grab` has one — so a row whose
    // visibility turned on that would be in the screenshot on one machine and
    // absent on the next, and the failure would appear in CI on an unrelated
    // change with nothing in the diff to explain it. docs/screenshots.md and
    // Main.qml's scheme pin are the same argument about other properties.
    //
    // The cost is a build with D-Bus running with no session bus, which is a
    // headless capture and not a reader: the switch appears, and notify()
    // reports `there is no session bus` rather than posting. A packager who
    // left Qt D-Bus out gets no row at all, which is the honest answer.
    [[nodiscard]] static bool available();

    // Posts, or replaces the one with the same id. Asynchronous; nothing here
    // waits for the desktop, which may be slow to answer or absent.
    void notify(const QString &id, const QString &title, const QString &body, Priority priority);

    // Takes one down. A no-op for an id this object never posted.
    void withdraw(const QString &id);

Q_SIGNALS:
    // Which route carried it: "portal" or "service". For a test and for a
    // diagnostics line; nothing in the UI branches on it.
    void posted(const QString &id, const QString &route);

    // Neither route would take it. Reported rather than swallowed so that a
    // preference which appears to do nothing can be explained.
    void failed(const QString &id, const QString &reason);

private:
    void viaPortal(const QString &id, const QString &title, const QString &body,
                   Priority priority);
    void viaService(const QString &id, const QString &title, const QString &body,
                    Priority priority);

    // Ids the service route handed back, so that withdraw() can name them.
    // The portal route keys on our own id and needs nothing kept.
    QHash<QString, uint> m_serviceIds;
};
