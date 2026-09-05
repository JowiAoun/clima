// SPDX-FileCopyrightText: 2026 Jowi Aoun
// SPDX-License-Identifier: GPL-3.0-or-later

#include "notifier.h"

#include "climaconfig.h"

#ifdef CLIMA_HAVE_DBUS
#include <QDBusArgument>
#include <QDBusConnection>
#include <QDBusError>
#include <QDBusMessage>
#include <QDBusPendingCallWatcher>
#include <QDBusPendingReply>
#include <QVariantMap>
#endif

namespace {

#ifdef CLIMA_HAVE_DBUS
constexpr auto portalService   = "org.freedesktop.portal.Desktop";
constexpr auto portalPath      = "/org/freedesktop/portal/desktop";
constexpr auto portalInterface = "org.freedesktop.portal.Notification";

constexpr auto serviceName      = "org.freedesktop.Notifications";
constexpr auto servicePath      = "/org/freedesktop/Notifications";
constexpr auto serviceInterface = "org.freedesktop.Notifications";

// The portal's spelling of the four grades.
QString portalPriority(Notifier::Priority priority)
{
    switch (priority) {
    case Notifier::Priority::Low:    return QStringLiteral("low");
    case Notifier::Priority::Normal: return QStringLiteral("normal");
    case Notifier::Priority::High:   return QStringLiteral("high");
    case Notifier::Priority::Urgent: return QStringLiteral("urgent");
    }
    return QStringLiteral("normal");
}

// The service's: a byte, 0 low, 1 normal, 2 critical. "critical" is what a
// desktop keeps on the screen until it is dismissed, which is what an
// extreme-grade warning has earned and a high one has not.
uchar serviceUrgency(Notifier::Priority priority)
{
    switch (priority) {
    case Notifier::Priority::Low:    return 0;
    case Notifier::Priority::Normal:
    case Notifier::Priority::High:   return 1;
    case Notifier::Priority::Urgent: return 2;
    }
    return 1;
}

// "The portal is not here" as distinct from "the portal said no". Only the
// first is a reason to try the service instead.
bool portalIsAbsent(const QDBusError &error)
{
    switch (error.type()) {
    case QDBusError::ServiceUnknown:
    case QDBusError::UnknownInterface:
    case QDBusError::UnknownMethod:
    case QDBusError::UnknownObject:
    case QDBusError::NoServer:
    case QDBusError::Disconnected:
        return true;
    default:
        return false;
    }
}
#endif

} // namespace

Notifier::Notifier(QObject *parent)
    : QObject(parent)
{
}

Notifier::~Notifier() = default;

bool Notifier::available()
{
#ifdef CLIMA_HAVE_DBUS
    return true;
#else
    return false;
#endif
}

void Notifier::notify(const QString &id, const QString &title, const QString &body,
                      Priority priority)
{
#ifdef CLIMA_HAVE_DBUS
    if (!QDBusConnection::sessionBus().isConnected()) {
        Q_EMIT failed(id, QStringLiteral("there is no session bus"));
        return;
    }
    viaPortal(id, title, body, priority);
#else
    Q_EMIT failed(id, QStringLiteral("this build has no D-Bus and so no way to notify"));
#endif
}

void Notifier::withdraw(const QString &id)
{
#ifdef CLIMA_HAVE_DBUS
    if (!QDBusConnection::sessionBus().isConnected())
        return;

    // Both routes, because this object does not remember which one carried
    // the id: the portal's RemoveNotification is a no-op for an id it never
    // saw, and the service's CloseNotification is only sent for one it did.
    QDBusMessage portal = QDBusMessage::createMethodCall(
        QLatin1String(portalService), QLatin1String(portalPath), QLatin1String(portalInterface),
        QStringLiteral("RemoveNotification"));
    portal << id;
    QDBusConnection::sessionBus().asyncCall(portal);

    const auto it = m_serviceIds.constFind(id);
    if (it != m_serviceIds.cend()) {
        QDBusMessage service = QDBusMessage::createMethodCall(
            QLatin1String(serviceName), QLatin1String(servicePath), QLatin1String(serviceInterface),
            QStringLiteral("CloseNotification"));
        service << it.value();
        QDBusConnection::sessionBus().asyncCall(service);
        m_serviceIds.erase(it);
    }
#else
    Q_UNUSED(id)
#endif
}

void Notifier::viaPortal(const QString &id, const QString &title, const QString &body,
                         Priority priority)
{
#ifdef CLIMA_HAVE_DBUS
    QVariantMap notification;
    notification.insert(QStringLiteral("title"), title);
    notification.insert(QStringLiteral("body"), body);
    notification.insert(QStringLiteral("priority"), portalPriority(priority));

    QDBusMessage call = QDBusMessage::createMethodCall(
        QLatin1String(portalService), QLatin1String(portalPath), QLatin1String(portalInterface),
        QStringLiteral("AddNotification"));
    call << id << notification;

    auto *watcher = new QDBusPendingCallWatcher(QDBusConnection::sessionBus().asyncCall(call), this);
    connect(watcher, &QDBusPendingCallWatcher::finished, this,
            [this, watcher, id, title, body, priority]() {
                watcher->deleteLater();
                const QDBusPendingReply<> reply = *watcher;
                if (!reply.isError()) {
                    Q_EMIT posted(id, QStringLiteral("portal"));
                    return;
                }
                if (portalIsAbsent(reply.error())) {
                    viaService(id, title, body, priority);
                    return;
                }
                Q_EMIT failed(id, QStringLiteral("the notification portal refused: %1")
                                      .arg(reply.error().message()));
            });
#else
    Q_UNUSED(id) Q_UNUSED(title) Q_UNUSED(body) Q_UNUSED(priority)
#endif
}

void Notifier::viaService(const QString &id, const QString &title, const QString &body,
                          Priority priority)
{
#ifdef CLIMA_HAVE_DBUS
    QVariantMap hints;
    hints.insert(QStringLiteral("urgency"), QVariant::fromValue(serviceUrgency(priority)));
    // The desktop file, so a shell that groups notifications by application
    // files this under Clima and draws its icon.
    hints.insert(QStringLiteral("desktop-entry"), QStringLiteral(CLIMA_APP_ID));

    // Replacing rather than stacking: an update to a hazard this object has
    // already announced replaces the earlier notification, which is what the
    // portal route does by id and the service route does by number.
    const uint replaces = m_serviceIds.value(id, 0);

    QDBusMessage call = QDBusMessage::createMethodCall(
        QLatin1String(serviceName), QLatin1String(servicePath), QLatin1String(serviceInterface),
        QStringLiteral("Notify"));
    call << QStringLiteral("Clima")               // app_name
         << replaces                              // replaces_id
         << QStringLiteral(CLIMA_APP_ID)          // app_icon — the icon theme name
         << title                                 // summary
         << body                                  // body
         << QStringList()                         // actions
         << hints                                 // hints
         << -1;                                   // expire_timeout: the server's default

    auto *watcher = new QDBusPendingCallWatcher(QDBusConnection::sessionBus().asyncCall(call), this);
    connect(watcher, &QDBusPendingCallWatcher::finished, this, [this, watcher, id]() {
        watcher->deleteLater();
        const QDBusPendingReply<uint> reply = *watcher;
        if (reply.isError()) {
            Q_EMIT failed(id, QStringLiteral("no notification portal, and the notification "
                                             "service answered: %1")
                                  .arg(reply.error().message()));
            return;
        }
        m_serviceIds.insert(id, reply.value());
        Q_EMIT posted(id, QStringLiteral("service"));
    });
#else
    Q_UNUSED(id) Q_UNUSED(title) Q_UNUSED(body) Q_UNUSED(priority)
#endif
}
