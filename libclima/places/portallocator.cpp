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
    m_timer.setSingleShot(true);
    connect(&m_timer, &QTimer::timeout, this, &PortalLocator::onTimeout);
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
    if (!isAvailable()) {
        reportFailure(Failure::Unavailable,
                      QStringLiteral("there is no session bus, so there is no location portal "
                                     "to ask"));
        return;
    }

    // A second press while the first is outstanding is the user being
    // impatient, not a second question — and here a second question would be
    // a second permission dialog.
    if (isRequestInFlight())
        return;

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
    connect(watcher, &QDBusPendingCallWatcher::finished, this, &PortalLocator::onSessionCreated);

    m_timer.start(timeout());
}

void PortalLocator::onSessionCreated(QDBusPendingCallWatcher *watcher)
{
    watcher->deleteLater();

    // A reply for a request that was cancelled or timed out in the meantime.
    if (!isRequestInFlight())
        return;

    const QDBusPendingReply<QDBusObjectPath> reply = *watcher;
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
    connect(start, &QDBusPendingCallWatcher::finished, this, &PortalLocator::onStarted);
}

void PortalLocator::onStarted(QDBusPendingCallWatcher *watcher)
{
    watcher->deleteLater();

    if (!isRequestInFlight())
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
        // Granted. The fix follows on LocationUpdated; the timer is still
        // running for it.
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

    // Fire and forget. A session left open keeps GeoClue reporting to a
    // client that has stopped listening, which costs the machine a radio it
    // does not need on; the reply to Close() is of no use to anybody.
    QDBusMessage call = QDBusMessage::createMethodCall(service(), m_sessionPath,
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
