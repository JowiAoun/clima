// SPDX-FileCopyrightText: 2026 Jowi Aoun
// SPDX-License-Identifier: MPL-2.0
//
// "My location" — asked for, never assumed, and never on the critical path.
//
// docs/04-architecture.md §4.9 puts Qt Positioning against GeoClue2 on Linux,
// Windows Location on Windows and CoreLocation on macOS. Qt Positioning wraps
// all three, so this is one class with three backends we do not have to write.
//
// ============================================================================
// THREE RULES, AND THEY ARE ALL ABOUT NOT BLOCKING
//
//   1. NOTHING WAITS FOR IT. The app opens on the home place, from SQLite,
//      inside a 400 ms cold-start budget. A GeoClue2 fix takes anywhere from
//      tens of milliseconds to never — it is a D-Bus round trip to a service
//      that may itself be waiting on a portal dialog, a Wi-Fi scan or a GPS
//      lock. `requestPosition()` returns immediately and answers by signal, and
//      the UI it feeds must already be showing something by then.
//
//   2. A REFUSAL IS AN ORDINARY OUTCOME. Location permission is a thing users
//      say no to, and they are right to. `failed(PermissionDenied)` is not an
//      error state to recover from; it is the state where the app uses manual
//      search instead and says so once, in a non-modal line the user can
//      dismiss. Never a dialog, never a retry loop, never a second prompt.
//
//   3. IT IS OPTIONAL AT BUILD TIME. Qt Positioning is a separate Qt module
//      and a packager may not have it. `DeviceLocator::create()` returns a
//      locator that reports Unavailable when it was not compiled in, so every
//      caller has exactly one code path for "there is no location service" and
//      it is the same one a user who said no takes.
//
// ============================================================================
//
// ---- Flatpak needs the Location portal, and that is packaging work ----------
//
// Inside a Flatpak sandbox, GeoClue2 is not reachable on the session bus. The
// route is `org.freedesktop.portal.Location`, which needs
// `--talk-name=org.freedesktop.portal.Desktop` in the manifest and shows the
// user a portal prompt on first use. Qt Positioning's GeoClue2 backend does not
// use the portal, so under Flatpak this class will report Unavailable until
// docs/07-packaging.md's manifest work either adds the permission GeoClue2
// needs or a portal-backed source is written.
//
// That is noted rather than solved here: it is a manifest and a plugin, not an
// interface change, and getting the interface right first is what makes it a
// small change later.
//
// ---- what it does not do ----------------------------------------------------
//
// It does not name the place. A coordinate is not a location as far as a user
// is concerned, and turning one into "Toronto, Ontario" is
// libclima/providers/geocoding/offlinereversegeocoder.h — offline, because
// Nominatim answers 403 to the first request. The two are separate on purpose:
// this class knows about permissions and D-Bus, that one knows about places,
// and neither wants the other's failure modes.

#pragma once

#include "libclima/domain/coordinate.h"

#include <QObject>
#include <QString>

namespace clima {

class DeviceLocator : public QObject
{
    Q_OBJECT

public:
    explicit DeviceLocator(QObject *parent = nullptr);
    ~DeviceLocator() override;

    enum class Failure {
        // No positioning backend: the module was not compiled in, or Qt found
        // no source on this machine. Not the user's doing, and the UI should
        // simply not offer the button.
        Unavailable,

        // The user, or the portal, said no. The UI explains once and falls
        // back to manual search. It does not ask again.
        PermissionDenied,

        // The backend accepted the request and produced nothing in time. Worth
        // a retry, at the user's request rather than automatically — a laptop
        // indoors can take a long time and retrying costs battery.
        Timeout,

        // Anything else the backend reported.
        Error,
    };
    Q_ENUM(Failure)

    // The one a build actually has: the Qt Positioning implementation when
    // Qt6::Positioning was found at configure time, and a locator that reports
    // Unavailable when it was not. Ownership passes to `parent`.
    //
    // A factory rather than a constructor, so that the choice is made once, in
    // one place, at build time — and not by an `#ifdef` at every call site.
    [[nodiscard]] static DeviceLocator *create(QObject *parent = nullptr);

    // Whether this build has a positioning backend at all. Cheap, synchronous
    // and safe to call before anything else: it is what decides whether the
    // "use my location" control is shown.
    [[nodiscard]] virtual bool isAvailable() const;

    // Asks for one fix. Returns immediately; the answer arrives as `located`
    // or `failed`, once. Calling it again while a request is outstanding is a
    // no-op rather than a second request.
    virtual void requestPosition();

    // Abandons an outstanding request. Emits nothing: a cancelled request has
    // no outcome, and a caller that cancelled does not need to be told.
    virtual void cancel();

    [[nodiscard]] bool isRequestInFlight() const { return m_inFlight; }

    // How long to wait before giving up. Applies per request.
    void              setTimeout(int milliseconds);
    [[nodiscard]] int timeout() const { return m_timeoutMs; }

Q_SIGNALS:
    // `accuracyMetres` is negative when the backend did not say. A caller that
    // cares — deciding whether a fix is precise enough to pick a city rather
    // than a region — has to handle that, so it is a documented value rather
    // than a zero that looks like perfect accuracy.
    void located(const clima::Coordinate &coordinate, double accuracyMetres);

    // `reason` is developer-facing English for a log. The user-facing string
    // belongs to the app and goes through Qt Linguist; the enum is what the UI
    // branches on.
    void failed(clima::DeviceLocator::Failure failure, const QString &reason);

protected:
    void setRequestInFlight(bool inFlight) { m_inFlight = inFlight; }
    void reportFailure(Failure failure, const QString &reason);
    void reportPosition(const Coordinate &coordinate, double accuracyMetres);

private:
    bool m_inFlight = false;
    int  m_timeoutMs = 15000;
};

} // namespace clima
