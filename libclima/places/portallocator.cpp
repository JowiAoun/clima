// SPDX-FileCopyrightText: 2026 Jowi Aoun
// SPDX-License-Identifier: MPL-2.0

#include "portallocator.h"

#include <QDBusConnectionInterface>
#include <QDBusMessage>
#include <QDBusPendingCallWatcher>
#include <QDBusPendingReply>
#include <QFile>
#include <QVariant>

#include <cstdlib>

namespace clima {

namespace {

constexpr auto kService           = "org.freedesktop.portal.Desktop";
constexpr auto kObjectPath        = "/org/freedesktop/portal/desktop";
constexpr auto kLocationInterface = "org.freedesktop.portal.Location";
constexpr auto kRequestInterface  = "org.freedesktop.portal.Request";
constexpr auto kSessionInterface  = "org.freedesktop.portal.Session";

// org.freedesktop.portal.Location's accuracy enum. CITY: see the header.
constexpr uint kAccuracyCity = 2;

// How long the reader is given to answer the portal's permission dialog. Not
// timeout(), which bounds the arrival of a POSITION: a dialog can sit open for
// as long as somebody takes to read it, and a locator that gave up at fifteen
// seconds would cancel a request the user was about to grant. Generous, and
// still bounded, because a portal that never answers at all must not leave the
// button dead for the life of the process.
constexpr int kDialogTimeoutMs = 3 * 60 * 1000;

// The property an outstanding reply carries its request's serial in.
constexpr const char *kSerial = "clima_serial";

// Response codes on org.freedesktop.portal.Request.
constexpr uint kResponseSuccess   = 0;
constexpr uint kResponseCancelled = 1;

} // namespace

PortalLocator::PortalLocator(QObject *parent)
    : PortalLocator(QDBusConnection::sessionBus(), parent)
{
}

PortalLocator::PortalLocator(const QDBusConnection &bus, QObject *parent)
    : DeviceLocator(parent)
    , m_bus(bus)
{
    // Single-shot, and its slot is chosen per phase rather than once here: the
    // dialog and the fix are two different waits with two different bounds.
    // See requestPosition() and onResponse().
    m_timer.setSingleShot(true);
}

PortalLocator::~PortalLocator()
{
    cancel();
}

// ---- constants, for a fake to agree with -----------------------------------

QString PortalLocator::service()           { return QLatin1String(kService); }
QString PortalLocator::objectPath()        { return QLatin1String(kObjectPath); }
QString PortalLocator::locationInterface() { return QLatin1String(kLocationInterface); }
QString PortalLocator::requestInterface()  { return QLatin1String(kRequestInterface); }
QString PortalLocator::sessionInterface()  { return QLatin1String(kSessionInterface); }

bool PortalLocator::inSandbox()
{
    if (QFile::exists(QStringLiteral("/.flatpak-info")))
        return true;
    const char *id = std::getenv("FLATPAK_ID");
    return id != nullptr && *id != '\0';
}

// ---- availability ------------------------------------------------------------

bool PortalLocator::isAvailable() const
{
    // The bus, not the portal. Asking the bus whether the portal's name is
    // activatable is a round trip, and this is called on the way to the first
    // frame to decide whether a button exists. A missing portal is reported at
    // request time, once, as Unavailable — the same branch a refusal takes and
    // the same sentence the UI already has for it.
    return m_bus.isConnected();
}

// ---- paths -------------------------------------------------------------------

QString PortalLocator::senderSegment() const
{
    // ":1.42" -> "1_42". The portal spec's derivation, exactly.
    QString sender = m_bus.baseService();
    if (sender.startsWith(QLatin1Char(':')))
        sender.remove(0, 1);
    sender.replace(QLatin1Char('.'), QLatin1Char('_'));
    return sender;
}

QString PortalLocator::sessionPathFor(const QString &token) const
{
    return QStringLiteral("%1/session/%2/%3").arg(objectPath(), senderSegment(), token);
}

QString PortalLocator::requestPathFor(const QString &token) const
{
    return QStringLiteral("%1/request/%2/%3").arg(objectPath(), senderSegment(), token);
}

// ---- the request ---------------------------------------------------------------

void PortalLocator::requestPosition()
{
    // The in-flight check comes FIRST, and the order is load-bearing. A second
    // press while the first request is outstanding is the user being impatient,
    // not a second question — and here a second question would be a second
    // permission dialog. Asked the other way round, a bus that had gone away
    // between the two presses reported Unavailable for the second one, which
    // clears the in-flight flag without going through finish(): the first
    // request is then orphaned with its match rules still installed, every
    // later request early-returns out of subscribe(), and "use my location"
    // times out for the rest of the process.
    if (isRequestInFlight())
        return;

    if (!isAvailable()) {
        reportFailure(Failure::Unavailable,
                      QStringLiteral("there is no session bus, so there is no location portal "
                                     "to ask"));
        return;
    }

    setRequestInFlight(true);

    // One token for both objects. The spec allows different ones; using one
    // means one thing to keep and one thing to compare an incoming signal's
    // path against.
    m_token       = QStringLiteral("clima%1").arg(++m_serial);
    m_sessionPath = sessionPathFor(m_token);
    m_requestPath = requestPathFor(m_token);

    // Subscribed BEFORE anything is asked — see the header for the race this
    // closes. Both signals, because Response and LocationUpdated can arrive in
    // the same burst as the reply to Start().
    subscribe();

    QVariantMap options;
    options.insert(QStringLiteral("session_handle_token"), m_token);
    options.insert(QStringLiteral("accuracy"), kAccuracyCity);

    QDBusMessage call = QDBusMessage::createMethodCall(service(), objectPath(),
                                                       locationInterface(),
                                                       QStringLiteral("CreateSession"));
    call << options;

    auto *watcher = new QDBusPendingCallWatcher(m_bus.asyncCall(call), this);
    watcher->setProperty(kSerial, QVariant::fromValue(m_serial));
    connect(watcher, &QDBusPendingCallWatcher::finished, this, &PortalLocator::onSessionCreated);

    // The DIALOG's clock, not the fix's. timeout() is how long to wait for a
    // position, and until the reader has answered the portal's permission
    // prompt there is no position to wait for — a request timed out at fifteen
    // seconds while somebody was still reading the dialog, closed the session
    // out from under it, and reported "no position arrived" for a request they
    // were in the middle of granting. Restarted at timeout() the moment the
    // portal says yes; see onResponse().
    m_timer.disconnect(this);
    connect(&m_timer, &QTimer::timeout, this, &PortalLocator::onDialogTimeout);
    m_timer.start(kDialogTimeoutMs);
}

void PortalLocator::onSessionCreated(QDBusPendingCallWatcher *watcher)
{
    watcher->deleteLater();

    const QDBusPendingReply<QDBusObjectPath> reply = *watcher;

    // A reply for a request that was cancelled or timed out in the meantime —
    // or, worse, for one that was, while a NEW request is now outstanding. The
    // serial tells those apart; the in-flight flag alone cannot, and adopting
    // a stale reply would point this locator at one session while leaving
    // another open forever.
    //
    // The portal created that session before we lost interest, and nothing but
    // this process will ever close it: xdg-desktop-portal reaps a session only
    // when the owning bus name goes away, so an abandoned one keeps GeoClue
    // reporting to nobody for the life of the program. Close it here.
    if (watcher->property(kSerial).value<quint64>() != m_serial || !isRequestInFlight()) {
        if (!reply.isError())
            closePath(reply.value().path());
        return;
    }
    if (reply.isError()) {
        // The portal is not on this bus, or is too old to have a Location
        // portal. Both are "there is nothing to ask", which is Unavailable,
        // and neither is the user's doing.
        const QString reason = QStringLiteral("the location portal did not answer: %1")
                                   .arg(reply.error().message());
        finish();
        reportFailure(Failure::Unavailable, reason);
        return;
    }

    // The portal is entitled to hand back a different path from the one the
    // token predicts — the spec says it may — and the one it hands back wins.
    const QString handed = reply.value().path();
    if (!handed.isEmpty())
        m_sessionPath = handed;
    m_sessionOpen = true;

    QVariantMap options;
    options.insert(QStringLiteral("handle_token"), m_token);

    QDBusMessage call = QDBusMessage::createMethodCall(service(), objectPath(),
                                                       locationInterface(),
                                                       QStringLiteral("Start"));
    // An empty parent window: this process may not have one, and the portal
    // treats it as "no parent" rather than as an error.
    call << QVariant::fromValue(QDBusObjectPath(m_sessionPath)) << QString() << options;

    auto *start = new QDBusPendingCallWatcher(m_bus.asyncCall(call), this);
    start->setProperty(kSerial, QVariant::fromValue(m_serial));
    connect(start, &QDBusPendingCallWatcher::finished, this, &PortalLocator::onStarted);
}

void PortalLocator::onStarted(QDBusPendingCallWatcher *watcher)
{
    watcher->deleteLater();

    if (watcher->property(kSerial).value<quint64>() != m_serial || !isRequestInFlight())
        return;

    const QDBusPendingReply<QDBusObjectPath> reply = *watcher;
    if (reply.isError()) {
        const QString reason = QStringLiteral("the location portal refused to start: %1")
                                   .arg(reply.error().message());
        finish();
        reportFailure(Failure::Error, reason);
        return;
    }

    // Nothing to do with the request path it returns: we subscribed to the
    // predicted one before calling, and a portal that hands back a different
    // request path than its own token derivation is not one this protocol can
    // be used with at all. The answer arrives as Response, and then as
    // LocationUpdated.
}

void PortalLocator::onResponse(uint response, const QVariantMap &results)
{
    Q_UNUSED(results)

    if (!isRequestInFlight())
        return;

    switch (response) {
    case kResponseSuccess:
        // Granted, so the dialog is answered and the wait becomes a wait for a
        // POSITION. That is what timeout() bounds, and it starts here rather
        // than when the request did — see requestPosition().
        m_timer.disconnect(this);
        connect(&m_timer, &QTimer::timeout, this, &PortalLocator::onTimeout);
        m_timer.start(timeout());
        return;

    case kResponseCancelled:
        // The one that is not a malfunction. The user, through their own
        // desktop's dialog, said no — and the right response is to stop asking
        // and let them search for a place by name.
        finish();
        reportFailure(Failure::PermissionDenied,
                      QStringLiteral("the location portal's dialog was declined"));
        return;

    default:
        finish();
        reportFailure(Failure::Error,
                      QStringLiteral("the location portal answered with response code %1")
                          .arg(response));
        return;
    }
}

void PortalLocator::onLocationUpdated(const QDBusObjectPath &session, const QVariantMap &location)
{
    if (!isRequestInFlight())
        return;

    // The signal is broadcast on the portal object, so every client sees every
    // session's updates. Only ours is ours.
    if (session.path() != m_sessionPath)
        return;

    bool okLatitude = false, okLongitude = false;
    const double latitude  = location.value(QStringLiteral("Latitude")).toDouble(&okLatitude);
    const double longitude = location.value(QStringLiteral("Longitude")).toDouble(&okLongitude);

    if (!okLatitude || !okLongitude) {
        finish();
        reportFailure(Failure::Error,
                      QStringLiteral("the location portal sent a fix with no coordinate in it"));
        return;
    }

    // Accuracy is optional in the spec and -1 is DeviceLocator's word for
    // "the backend did not say". See devicelocator.h on why not zero.
    bool okAccuracy = false;
    const double accuracy = location.value(QStringLiteral("Accuracy")).toDouble(&okAccuracy);

    finish();
    reportPosition(Coordinate{ latitude, longitude }, okAccuracy ? accuracy : -1.0);
}

void PortalLocator::onDialogTimeout()
{
    if (!isRequestInFlight())
        return;
    finish();
    reportFailure(Failure::Timeout,
                  QStringLiteral("the location portal never answered its own permission "
                                 "request within %1 ms").arg(kDialogTimeoutMs));
}

void PortalLocator::onTimeout()
{
    if (!isRequestInFlight())
        return;
    finish();
    reportFailure(Failure::Timeout,
                  QStringLiteral("no position arrived from the location portal within %1 ms")
                      .arg(timeout()));
}

void PortalLocator::cancel()
{
    if (!isRequestInFlight())
        return;
    finish();
    setRequestInFlight(false);
}

// ---- bookkeeping ---------------------------------------------------------------

void PortalLocator::subscribe()
{
    if (m_subscribed)
        return;

    // Matched on the request PATH for Response, since every request has its
    // own object; and on the portal's own path for LocationUpdated, which is
    // emitted there for every session — filtered by path in the slot.
    m_bus.connect(service(), m_requestPath, requestInterface(), QStringLiteral("Response"),
                  this, SLOT(onResponse(uint, QVariantMap)));
    m_bus.connect(service(), objectPath(), locationInterface(), QStringLiteral("LocationUpdated"),
                  this, SLOT(onLocationUpdated(QDBusObjectPath, QVariantMap)));
    m_subscribed = true;
}

void PortalLocator::unsubscribe()
{
    if (!m_subscribed)
        return;
    m_bus.disconnect(service(), m_requestPath, requestInterface(), QStringLiteral("Response"),
                     this, SLOT(onResponse(uint, QVariantMap)));
    m_bus.disconnect(service(), objectPath(), locationInterface(),
                     QStringLiteral("LocationUpdated"), this,
                     SLOT(onLocationUpdated(QDBusObjectPath, QVariantMap)));
    m_subscribed = false;
}

void PortalLocator::closeSession()
{
    if (!m_sessionOpen)
        return;
    m_sessionOpen = false;
    closePath(m_sessionPath);
}

void PortalLocator::closePath(const QString &sessionPath)
{
    if (sessionPath.isEmpty())
        return;

    // Fire and forget. A session left open keeps GeoClue reporting to a
    // client that has stopped listening, which costs the machine a radio it
    // does not need on; the reply to Close() is of no use to anybody.
    QDBusMessage call = QDBusMessage::createMethodCall(service(), sessionPath,
                                                       sessionInterface(),
                                                       QStringLiteral("Close"));
    m_bus.asyncCall(call);
}

// Everything that ends a request, whichever way it ended. The in-flight flag
// itself is cleared by reportFailure()/reportPosition() — or by cancel(),
// which reports nothing — so that a slot arriving after this has a flag to
// test.
void PortalLocator::finish()
{
    m_timer.stop();
    unsubscribe();
    closeSession();
}

} // namespace clima
