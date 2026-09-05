// SPDX-FileCopyrightText: 2026 Jowi Aoun
// SPDX-License-Identifier: GPL-3.0-or-later
//
// The notifier, against a portal and a service of our own.
//
// Same arrangement as tst_portallocator: this binary owns the well-known
// names on a bus dbus-run-session started for it, and the class under test
// talks to them over D-Bus exactly as it would to the desktop. What is
// asserted is the ORDER — portal first, the service only when the portal is
// not there — and that a portal which refuses is not gone around.

#include "app/platform/notifier.h"

#include "support/networkguard.h"

#include <QDBusAbstractAdaptor>
#include <QDBusConnection>
#include <QDBusContext>
#include <QDBusMessage>
#include <QSignalSpy>
#include <QVariantMap>
#include <QtTest>

// ---- a notification portal ---------------------------------------------------

class FakePortal : public QObject, protected QDBusContext
{
    Q_OBJECT
public:
    using QObject::QObject;

    QStringList added;
    QStringList removed;
    bool        refuse = false;

    void add(const QString &id, const QVariantMap &notification)
    {
        if (refuse) {
            sendErrorReply(QDBusError::AccessDenied, QStringLiteral("not allowed"));
            return;
        }
        added.append(id);
        lastNotification = notification;
    }
    QVariantMap lastNotification;
};

class FakePortalAdaptor : public QDBusAbstractAdaptor
{
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "org.freedesktop.portal.Notification")
public:
    explicit FakePortalAdaptor(FakePortal *portal)
        : QDBusAbstractAdaptor(portal)
        , m_portal(portal)
    {
    }
public Q_SLOTS:
    void AddNotification(const QString &id, const QVariantMap &notification)
    {
        m_portal->add(id, notification);
    }
    void RemoveNotification(const QString &id) { m_portal->removed.append(id); }

private:
    FakePortal *m_portal;
};

// ---- the freedesktop service --------------------------------------------------

class FakeService : public QObject
{
    Q_OBJECT
public:
    using QObject::QObject;
    QStringList summaries;
    QList<uint> replaced;
    QList<uint> closed;
    uint        next = 41;
};

class FakeServiceAdaptor : public QDBusAbstractAdaptor
{
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "org.freedesktop.Notifications")
public:
    explicit FakeServiceAdaptor(FakeService *service)
        : QDBusAbstractAdaptor(service)
        , m_service(service)
    {
    }
public Q_SLOTS:
    uint Notify(const QString &appName, uint replacesId, const QString &appIcon,
                const QString &summary, const QString &body, const QStringList &actions,
                const QVariantMap &hints, int expireTimeout)
    {
        Q_UNUSED(appName) Q_UNUSED(appIcon) Q_UNUSED(body) Q_UNUSED(actions) Q_UNUSED(hints)
        Q_UNUSED(expireTimeout)
        m_service->summaries.append(summary);
        m_service->replaced.append(replacesId);
        return ++m_service->next;
    }
    void CloseNotification(uint id) { m_service->closed.append(id); }

private:
    FakeService *m_service;
};

// ---- the test ------------------------------------------------------------------

class TestNotifier : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void initTestCase();
    void cleanup();

    void thePortalCarriesItWhenThereIsOne();
    void theServiceCarriesItWhenThereIsNoPortal();
    void aPortalThatRefusesIsNotGoneAround();
    void nothingOnTheBusIsReportedNotSwallowed();
    void withdrawingReachesWhicheverRouteCarriedIt();

private:
    void putThePortalUp();
    void putTheServiceUp();
    void takeEverythingDown();

    QDBusConnection m_desktopBus = QDBusConnection::connectToBus(
        QDBusConnection::SessionBus, QStringLiteral("desktop-side"));
    FakePortal  *m_portal  = nullptr;
    FakeService *m_service = nullptr;
};

void TestNotifier::initTestCase()
{
    NetworkGuard::install();
    QVERIFY2(QDBusConnection::sessionBus().isConnected(),
             "no session bus — this test is meant to run under dbus-run-session");
    QVERIFY(m_desktopBus.isConnected());
    QVERIFY(Notifier::available());
}

void TestNotifier::cleanup()
{
    takeEverythingDown();
}

void TestNotifier::putThePortalUp()
{
    m_portal = new FakePortal(this);
    new FakePortalAdaptor(m_portal);
    QVERIFY(m_desktopBus.registerObject(QStringLiteral("/org/freedesktop/portal/desktop"), m_portal));
    QVERIFY(m_desktopBus.registerService(QStringLiteral("org.freedesktop.portal.Desktop")));
}

void TestNotifier::putTheServiceUp()
{
    m_service = new FakeService(this);
    new FakeServiceAdaptor(m_service);
    QVERIFY(m_desktopBus.registerObject(QStringLiteral("/org/freedesktop/Notifications"), m_service));
    QVERIFY(m_desktopBus.registerService(QStringLiteral("org.freedesktop.Notifications")));
}

void TestNotifier::takeEverythingDown()
{
    if (m_portal != nullptr) {
        m_desktopBus.unregisterService(QStringLiteral("org.freedesktop.portal.Desktop"));
        m_desktopBus.unregisterObject(QStringLiteral("/org/freedesktop/portal/desktop"));
        delete m_portal;
        m_portal = nullptr;
    }
    if (m_service != nullptr) {
        m_desktopBus.unregisterService(QStringLiteral("org.freedesktop.Notifications"));
        m_desktopBus.unregisterObject(QStringLiteral("/org/freedesktop/Notifications"));
        delete m_service;
        m_service = nullptr;
    }
}

void TestNotifier::thePortalCarriesItWhenThereIsOne()
{
    putThePortalUp();
    putTheServiceUp();

    Notifier   notifier;
    QSignalSpy posted(&notifier, &Notifier::posted);

    notifier.notify(QStringLiteral("nws:x"), QStringLiteral("Tornado Warning"),
                    QStringLiteral("Until 5:00 PM"), Notifier::Priority::Urgent);

    // Nothing waits: the call returns before the desktop has answered.
    QCOMPARE(posted.count(), 0);
    QVERIFY(posted.wait(3000));

    QCOMPARE(posted.constFirst().at(1).toString(), QStringLiteral("portal"));
    QCOMPARE(m_portal->added, QStringList{ QStringLiteral("nws:x") });
    QCOMPARE(m_portal->lastNotification.value(QStringLiteral("title")).toString(),
             QStringLiteral("Tornado Warning"));
    QCOMPARE(m_portal->lastNotification.value(QStringLiteral("priority")).toString(),
             QStringLiteral("urgent"));

    // And the service, which was there too, was not asked.
    QVERIFY(m_service->summaries.isEmpty());
}

void TestNotifier::theServiceCarriesItWhenThereIsNoPortal()
{
    putTheServiceUp();

    Notifier   notifier;
    QSignalSpy posted(&notifier, &Notifier::posted);

    notifier.notify(QStringLiteral("eccc:y"), QStringLiteral("Heat Warning"),
                    QStringLiteral("Until Fri 6:00 AM"), Notifier::Priority::High);
    QVERIFY(posted.wait(3000));

    QCOMPARE(posted.constFirst().at(1).toString(), QStringLiteral("service"));
    QCOMPARE(m_service->summaries, QStringList{ QStringLiteral("Heat Warning") });
    QCOMPARE(m_service->replaced.constFirst(), 0u);

    // A second post under the same id replaces rather than stacks.
    notifier.notify(QStringLiteral("eccc:y"), QStringLiteral("Heat Warning"),
                    QStringLiteral("Until Sat 6:00 AM"), Notifier::Priority::High);
    QVERIFY(posted.wait(3000));
    QCOMPARE(m_service->replaced.size(), 2);
    QCOMPARE(m_service->replaced.at(1), 42u);
}

void TestNotifier::aPortalThatRefusesIsNotGoneAround()
{
    putThePortalUp();
    putTheServiceUp();
    m_portal->refuse = true;

    Notifier   notifier;
    QSignalSpy posted(&notifier, &Notifier::posted);
    QSignalSpy failed(&notifier, &Notifier::failed);

    notifier.notify(QStringLiteral("nws:z"), QStringLiteral("Flood Watch"), QString(),
                    Notifier::Priority::Normal);
    QVERIFY(failed.wait(3000));

    // A refusal is a decision. The service underneath the portal is exactly
    // what the portal exists to stand in front of.
    QCOMPARE(posted.count(), 0);
    QVERIFY(m_service->summaries.isEmpty());
}

void TestNotifier::nothingOnTheBusIsReportedNotSwallowed()
{
    Notifier   notifier;
    QSignalSpy failed(&notifier, &Notifier::failed);

    notifier.notify(QStringLiteral("nws:q"), QStringLiteral("Wind Advisory"), QString(),
                    Notifier::Priority::Normal);
    QVERIFY(failed.wait(3000));
    QVERIFY(failed.constFirst().at(1).toString().contains(QStringLiteral("no notification portal")));
}

void TestNotifier::withdrawingReachesWhicheverRouteCarriedIt()
{
    putThePortalUp();
    putTheServiceUp();

    Notifier   notifier;
    QSignalSpy posted(&notifier, &Notifier::posted);

    notifier.notify(QStringLiteral("a"), QStringLiteral("A"), QString(), Notifier::Priority::Normal);
    QVERIFY(posted.wait(3000));

    notifier.withdraw(QStringLiteral("a"));
    QTRY_COMPARE_WITH_TIMEOUT(m_portal->removed, QStringList{ QStringLiteral("a") }, 3000);
    // Never posted through the service, so never closed there either.
    QTest::qWait(100);
    QVERIFY(m_service->closed.isEmpty());
}

QTEST_GUILESS_MAIN(TestNotifier)
#include "tst_notifier.moc"
