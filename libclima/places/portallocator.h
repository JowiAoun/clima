// SPDX-FileCopyrightText: 2026 Jowi Aoun
// SPDX-License-Identifier: MPL-2.0
//
// "My location" through the XDG desktop portal — the route that works from
// inside a Flatpak, and the only one that does.
//
// ============================================================================
// WHY A SECOND LOCATOR
//
// Qt Positioning's Linux backend talks to GeoClue2 on the session bus, and a
// sandbox does not let it. Flatpak's default policy permits `org.freedesktop.
// portal.*` and nothing else on that bus, so inside the primary channel of this
// application QtPositioningLocator found no source, reported Unavailable, and
// "use my location" was a button that explained itself away. docs/known-gaps.md
// and docs/07-packaging.md §7.2 both recorded it as a stopgap.
//
// `org.freedesktop.portal.Location` is the same GeoClue behind a permission
// dialog the desktop owns. The user is asked once, by their own desktop, in its
// own words; the answer is remembered per application; and the app never sees
// the service, only the fix. That is a better arrangement than direct access
// and it is why this locator is tried FIRST in a sandbox rather than only when
// Qt Positioning has failed.
//
// ============================================================================
// THE PROTOCOL, AND THE ONE RACE IN IT
//
//   CreateSession(options) -> session handle      a session, and a path for it
//   Start(session, parent, options) -> request    asks; the dialog is here
//   Request.Response(code, results)               0 granted, 1 refused, 2 error
//   LocationUpdated(session, location)            the fix, on the portal itself
//   Session.Close()                               we are done
//
// Both object paths are derived from OUR unique bus name and a token WE choose:
//
//   /org/freedesktop/portal/desktop/session/<sender>/<token>
//   /org/freedesktop/portal/desktop/request/<sender>/<token>
//
// where <sender> is the unique name with the leading ':' dropped and every '.'
// made '_'. That is not a convenience; it is what makes the protocol usable at
// all. The portal may emit Response before the reply to Start() reaches us,
// so a client that waited for the reply to learn the request path would
// subscribe to a signal that has already gone. Knowing the path in advance,
// this class subscribes first and calls second. The same holds for
// LocationUpdated, which can arrive in the same burst as Response.
//
// ============================================================================
// WHAT IT DOES NOT DO
//
// Nothing here blocks. Every call is a QDBusPendingCall and every answer
// arrives by signal, which is DeviceLocator's first rule and the one a portal
// makes hardest to keep — a permission dialog can sit open for a minute, and
// a locator that waited for it would hold the UI thread for that minute.
//
// It asks for CITY accuracy. A forecast is answered on a grid a few kilometres
// across, so a finer fix buys nothing and asks the reader to grant more than
// the feature uses. The dialog says what was asked for.
//
// It is available when the session bus is, and says Unavailable at request
// time when the portal is not on it. Deciding at construction would mean a
// round trip on the critical path of every launch, for a button most readers
// never press.

#pragma once

#include "libclima/places/devicelocator.h"

#include <QDBusConnection>
#include <QDBusObjectPath>
#include <QString>
#include <QTimer>
#include <QVariantMap>

class QDBusPendingCallWatcher;

namespace clima {

class PortalLocator final : public DeviceLocator
{
    Q_OBJECT

public:
    // The session bus.
    explicit PortalLocator(QObject *parent = nullptr);

    // A bus of the caller's choosing — a private one, for a test that puts a
    // portal of its own on it.
    PortalLocator(const QDBusConnection &bus, QObject *parent = nullptr);
    ~PortalLocator() override;

    [[nodiscard]] bool isAvailable() const override;
    void requestPosition() override;
    void cancel() override;

    // Whether this process is inside a Flatpak sandbox, which is the case
    // where the direct GeoClue2 route cannot work. `/.flatpak-info` is the
    // documented marker; FLATPAK_ID is checked too because the file is the
    // sandbox's and a test cannot create it.
    [[nodiscard]] static bool inSandbox();

    // Where the portal's own well-known name and paths are, for a fake to
    // register itself under.
    [[nodiscard]] static QString service();
    [[nodiscard]] static QString objectPath();
    [[nodiscard]] static QString locationInterface();
    [[nodiscard]] static QString requestInterface();
    [[nodiscard]] static QString sessionInterface();

    // The paths this locator will use for its next session and request, given
    // its connection. Public so that a test can assert the derivation rather
    // than reproduce it.
    [[nodiscard]] QString sessionPathFor(const QString &token) const;
    [[nodiscard]] QString requestPathFor(const QString &token) const;

private Q_SLOTS:
    void onSessionCreated(QDBusPendingCallWatcher *watcher);
    void onStarted(QDBusPendingCallWatcher *watcher);
    void onResponse(uint response, const QVariantMap &results);
    void onLocationUpdated(const QDBusObjectPath &session, const QVariantMap &location);
    void onTimeout();

private:
    [[nodiscard]] QString senderSegment() const;
    void subscribe();
    void unsubscribe();
    void closeSession();
    void finish();

    QDBusConnection m_bus;
    QTimer          m_timer;

    QString m_token;
    QString m_sessionPath;
    QString m_requestPath;
    bool    m_sessionOpen = false;
    bool    m_subscribed  = false;
    quint64 m_serial      = 0;
};

} // namespace clima
