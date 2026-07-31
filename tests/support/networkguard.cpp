// SPDX-FileCopyrightText: 2026 Jowi Aoun
// SPDX-License-Identifier: GPL-3.0-or-later

#include "networkguard.h"

#include <QHostAddress>
#include <QMutex>
#include <QMutexLocker>
#include <QNetworkProxy>
#include <QNetworkProxyFactory>

namespace {

QMutex      g_mutex;
QStringList g_attempts;

bool isLocal(const QString &host)
{
    if (host.isEmpty())
        return true;
    if (host.compare(QLatin1String("localhost"), Qt::CaseInsensitive) == 0)
        return true;

    QHostAddress address(host);
    return !address.isNull() && address.isLoopback();
}

class GuardFactory : public QNetworkProxyFactory
{
public:
    QList<QNetworkProxy> queryProxy(const QNetworkProxyQuery &query) override
    {
        const QString host = query.peerHostName();

        if (isLocal(host))
            return { QNetworkProxy(QNetworkProxy::NoProxy) };

        {
            QMutexLocker locker(&g_mutex);
            g_attempts.append(host);
        }

        // Port 9 is DISCARD (RFC 863) on loopback, where nothing is listening.
        // A SOCKS5 proxy there fails to connect immediately rather than hanging
        // for a DNS timeout, so a test that violates the rule fails in
        // milliseconds and says why, instead of taking thirty seconds to say
        // "operation timed out".
        return { QNetworkProxy(QNetworkProxy::Socks5Proxy, QStringLiteral("127.0.0.1"), 9) };
    }
};

} // namespace

void NetworkGuard::install()
{
    // setApplicationProxyFactory takes ownership and deletes any previous one,
    // so repeated installs are safe and do not leak.
    QNetworkProxyFactory::setApplicationProxyFactory(new GuardFactory);
}

QStringList NetworkGuard::externalAttempts()
{
    QMutexLocker locker(&g_mutex);
    return g_attempts;
}

void NetworkGuard::clearAttempts()
{
    QMutexLocker locker(&g_mutex);
    g_attempts.clear();
}
