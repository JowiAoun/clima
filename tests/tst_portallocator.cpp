// SPDX-FileCopyrightText: 2026 Jowi Aoun
// SPDX-License-Identifier: GPL-3.0-or-later
//
// "My location" through the desktop portal, against a portal of our own.
//
// ============================================================================
// WHY A FAKE, AND WHY A PRIVATE BUS
//
// The real portal is a permission dialog. A test cannot click it, and a test
// that reached the real GeoClue would be testing where the runner is. So this
// binary IS the portal: it owns org.freedesktop.portal.Desktop on the bus it
// is given and implements the three objects the protocol names — the portal,
// a request and a session — with the same paths, the same signals and the same
// response codes the spec gives them. The locator under test talks to it over
// D-Bus exactly as it would talk to xdg-desktop-portal; nothing is stubbed
// on the client side.
//
// It runs under `dbus-run-session`, which starts a bus that lives exactly as
// long as this process. On the real session bus the well-known name is taken,
// and a test that failed for want of the name would be a test about the
// runner. tests/CMakeLists.txt registers it only where dbus-run-session is
// found and says so when it is not.
//
// ============================================================================
// THE ONE THING THIS CANNOT PROVE
//
// That the real portal's dialog is worded the way the accuracy asked for
// implies. CITY accuracy is a number in a map; what the desktop shows the
// reader for it is the desktop's. Everything else — the two paths, the race
// the subscription order closes, refusal against error against timeout, a
// stranger's session ignored, the session closed after the fix — is here.

#include "libclima/places/portallocator.h"

#include "support/networkguard.h"

#include <QDBusAbstractAdaptor>
#include <QDBusConnection>
#include <QDBusMessage>
#include <QDBusObjectPath>
#include <QSignalSpy>
#include <QTimer>
#include <QVariantMap>
#include <QtTest>

#include <functional>

using namespace clima;

// ---- the fake portal, in three objects ---------------------------------------

// A request: one object per Start(), whose only job is to say yes or no.
class FakeRequest : public QObject
{
    Q_OBJECT
public:
    using QObject::QObject;
Q_SIGNALS:
    void Response(uint response, const QVariantMap &results);
};

class FakeRequestAdaptor : public QDBusAbstractAdaptor
{
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "org.freedesktop.portal.Request")
public:
    explicit FakeRequestAdaptor(FakeRequest *request)
        : QDBusAbstractAdaptor(request)
    {
        setAutoRelaySignals(true);
    }
Q_SIGNALS:
    void Response(uint response, const QVariantMap &results);
};

// A session: it exists to be closed.
class FakeSession : public QObject
{
    Q_OBJECT
public:
    using QObject::QObject;
    int closes = 0;
};

class FakeSessionAdaptor : public QDBusAbstractAdaptor
{
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "org.freedesktop.portal.Session")
public:
    explicit FakeSessionAdaptor(FakeSession *session)
        : QDBusAbstractAdaptor(session)
        , m_session(session)
    {
    }
public Q_SLOTS:
    void Close() { ++m_session->closes; }
private:
    FakeSession *m_session;
};

// The portal itself. What it does on Start() is the test's decision, handed in
// as a script: grant and send a fix, refuse, grant and go quiet.
class FakePortal : public QObject
{
    Q_OBJECT
public:
    explicit FakePortal(const QDBusConnection &bus, QObject *parent = nullptr)
        : QObject(parent)
        , m_bus(bus)
    {
    }

    // The locator derives its request path from ITS unique name, and this
    // process is a different connection. So the test tells the portal how the
    // locator spells its paths, which is the public half of that derivation.
    std::function<QString(const QString &)> requestPathFor;

    // What to do after Start(). Called on the next turn of the loop, as the
    // real portal answers after its reply.
    std::function<void(FakePortal &, const QString &requestPath, const QString &sessionPath)>
        onStart;

    int         sessionsCreated = 0;
    QVariantMap lastSessionOptions;

    // The path handed back by the most recent CreateSession, so a test can name
    // a session it never got to hear about.
    QString lastSessionPath;

    QDBusObjectPath createSession(const QVariantMap &options)
    {
        ++sessionsCreated;
        lastSessionOptions = options;

        // A path of the portal's choosing, deliberately NOT the one the
        // token predicts: the spec allows the portal to choose, and the
        // locator has to take what it is handed.
        const QString token = options.value(QStringLiteral("session_handle_token")).toString();
        const QString path  = QStringLiteral("/org/freedesktop/portal/desktop/session/fake/%1")
                                 .arg(token);

        auto *session = new FakeSession(this);
        new FakeSessionAdaptor(session);
        m_bus.registerObject(path, session);
        m_sessions.insert(path, session);
        lastSessionPath = path;
        return QDBusObjectPath(path);
    }

    QDBusObjectPath start(const QDBusObjectPath &session, const QVariantMap &options)
    {
        const QString token = options.value(QStringLiteral("handle_token")).toString();
        const QString path  = requestPathFor(token);

        auto *request = new FakeRequest(this);
        new FakeRequestAdaptor(request);
        m_bus.registerObject(path, request);
        m_requests.insert(path, request);

        const QString sessionPath = session.path();
        QTimer::singleShot(0, this, [this, path, sessionPath]() {
            if (onStart)
                onStart(*this, path, sessionPath);
        });
        return QDBusObjectPath(path);
    }

    void respond(const QString &requestPath, uint code)
    {
        if (FakeRequest *request = m_requests.value(requestPath))
            Q_EMIT request->Response(code, {});
    }

    void sendFix(const QString &sessionPath, double latitude, double longitude,
                 double accuracy = -1)
    {
        QVariantMap location;
        location.insert(QStringLiteral("Latitude"), latitude);
        location.insert(QStringLiteral("Longitude"), longitude);
        if (accuracy >= 0)
            location.insert(QStringLiteral("Accuracy"), accuracy);
        Q_EMIT LocationUpdated(QDBusObjectPath(sessionPath), location);
    }

    int closesOf(const QString &sessionPath) const
    {
        const FakeSession *session = m_sessions.value(sessionPath);
        return session != nullptr ? session->closes : -1;
    }

Q_SIGNALS:
    void LocationUpdated(const QDBusObjectPath &session, const QVariantMap &location);

private:
    QDBusConnection               m_bus;
    QHash<QString, FakeRequest *> m_requests;
    QHash<QString, FakeSession *> m_sessions;
};

class FakePortalAdaptor : public QDBusAbstractAdaptor
{
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "org.freedesktop.portal.Location")
public:
    explicit FakePortalAdaptor(FakePortal *portal)
        : QDBusAbstractAdaptor(portal)
        , m_portal(portal)
    {
        setAutoRelaySignals(true);
    }

public Q_SLOTS:
    QDBusObjectPath CreateSession(const QVariantMap &options)
    {
        return m_portal->createSession(options);
    }

    QDBusObjectPath Start(const QDBusObjectPath &session, const QString &parentWindow,
                          const QVariantMap &options)
    {
        Q_UNUSED(parentWindow)
        return m_portal->start(session, options);
    }

Q_SIGNALS:
    void LocationUpdated(const QDBusObjectPath &session, const QVariantMap &location);

private:
    FakePortal *m_portal;
};

// ---- the test ------------------------------------------------------------------

class TestPortalLocator : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void initTestCase();
    void init();
    void cleanup();

    void theTwoPathsAreDerivedTheWayTheSpecSays();
    void noPortalOnTheBusIsUnavailableAndDoesNotBlock();
    void aFixArrivesAndTheSessionIsClosed();
    void aFixWithoutAccuracySaysSo();
    void aRefusalIsPermissionDenied();
    void anErrorResponseIsAnError();
    void nothingArrivingIsATimeout();
    void aSecondPressWhileWaitingOpensNoSecondSession();
    void aStrangersSessionIsIgnored();
    void cancelReportsNothingAndClosesTheSession();
    void theAccuracyAskedForIsCity();
    void aSessionCreatedForACancelledRequestIsClosed();
    void aPermissionDialogNobodyAnswersIsBounded();

private:
    void putThePortalOnTheBus();
    void takeThePortalOffTheBus();

    // Two connections, so that the portal is another party on the bus rather
    // than the locator talking to itself: a signal a connection emits is not
    // delivered back to that same connection's own match rules the way it is
    // to everybody else's, and a test that passed on that quirk would not be
    // describing the product.
    QDBusConnection m_portalBus = QDBusConnection::connectToBus(
        QDBusConnection::SessionBus, QStringLiteral("portal-side"));
    FakePortal *m_portal = nullptr;
};

void TestPortalLocator::initTestCase()
{
    NetworkGuard::install();

    QVERIFY2(QDBusConnection::sessionBus().isConnected(),
             "no session bus — this test is meant to run under dbus-run-session");
    QVERIFY2(m_portalBus.isConnected(), "the portal-side connection did not connect");
}

void TestPortalLocator::init()
{
    putThePortalOnTheBus();
}

void TestPortalLocator::cleanup()
{
    takeThePortalOffTheBus();
}

void TestPortalLocator::putThePortalOnTheBus()
{
    m_portal = new FakePortal(m_portalBus, this);
    new FakePortalAdaptor(m_portal);

    QVERIFY(m_portalBus.registerObject(PortalLocator::objectPath(), m_portal));
    QVERIFY2(m_portalBus.registerService(PortalLocator::service()),
             "could not own org.freedesktop.portal.Desktop — is the real portal on this bus?");
}

void TestPortalLocator::takeThePortalOffTheBus()
{
    if (m_portal == nullptr)
        return;
    m_portalBus.unregisterService(PortalLocator::service());
    m_portalBus.unregisterObject(PortalLocator::objectPath(), QDBusConnection::UnregisterTree);
    delete m_portal;
    m_portal = nullptr;
}

// ----------------------------------------------------------------------------

void TestPortalLocator::theTwoPathsAreDerivedTheWayTheSpecSays()
{
    PortalLocator locator(QDBusConnection::sessionBus());

    // ":1.7" -> "1_7". The spec's derivation, asserted rather than trusted.
    QString sender = QDBusConnection::sessionBus().baseService();
    QVERIFY(sender.startsWith(QLatin1Char(':')));
    sender.remove(0, 1);
    sender.replace(QLatin1Char('.'), QLatin1Char('_'));

    QCOMPARE(locator.sessionPathFor(QStringLiteral("t1")),
             QStringLiteral("/org/freedesktop/portal/desktop/session/%1/t1").arg(sender));
    QCOMPARE(locator.requestPathFor(QStringLiteral("t1")),
             QStringLiteral("/org/freedesktop/portal/desktop/request/%1/t1").arg(sender));
}

void TestPortalLocator::noPortalOnTheBusIsUnavailableAndDoesNotBlock()
{
    takeThePortalOffTheBus();

    PortalLocator locator(QDBusConnection::sessionBus());
    QVERIFY(locator.isAvailable()); // the bus is there; the portal is not

    QSignalSpy failed(&locator, &DeviceLocator::failed);
    locator.requestPosition();

    // Nothing waits for it. The answer — whatever it is — is not on the stack.
    QCOMPARE(failed.count(), 0);
    QVERIFY(locator.isRequestInFlight());

    QVERIFY(failed.wait(3000));
    QCOMPARE(failed.constFirst().at(0).value<DeviceLocator::Failure>(),
             DeviceLocator::Failure::Unavailable);
    QVERIFY(!locator.isRequestInFlight());
}

void TestPortalLocator::aFixArrivesAndTheSessionIsClosed()
{
    PortalLocator locator(QDBusConnection::sessionBus());
    m_portal->requestPathFor = [&locator](const QString &token) {
        return locator.requestPathFor(token);
    };

    QString sessionUsed;
    m_portal->onStart = [&sessionUsed](FakePortal &portal, const QString &request,
                                       const QString &session) {
        sessionUsed = session;
        portal.respond(request, 0);
        portal.sendFix(session, 43.6535, -79.3839, 1200.0);
    };

    QSignalSpy located(&locator, &DeviceLocator::located);
    QSignalSpy failed(&locator, &DeviceLocator::failed);

    locator.requestPosition();
    QVERIFY(located.wait(3000));

    QCOMPARE(failed.count(), 0);
    QCOMPARE(located.count(), 1);
    const auto coordinate = located.constFirst().at(0).value<Coordinate>();
    QCOMPARE(coordinate.latitude, 43.6535);
    QCOMPARE(coordinate.longitude, -79.3839);
    QCOMPARE(located.constFirst().at(1).toDouble(), 1200.0);
    QVERIFY(!locator.isRequestInFlight());

    // The session is closed once the fix is in, and it is the portal's own
    // path that was closed — the one it handed back, not the one the token
    // predicted.
    QVERIFY(sessionUsed.startsWith(QStringLiteral("/org/freedesktop/portal/desktop/session/fake/")));
    QTRY_COMPARE_WITH_TIMEOUT(m_portal->closesOf(sessionUsed), 1, 3000);
}

void TestPortalLocator::aFixWithoutAccuracySaysSo()
{
    PortalLocator locator(QDBusConnection::sessionBus());
    m_portal->requestPathFor = [&locator](const QString &token) {
        return locator.requestPathFor(token);
    };
    m_portal->onStart = [](FakePortal &portal, const QString &request, const QString &session) {
        portal.respond(request, 0);
        portal.sendFix(session, 51.5, -0.12);
    };

    QSignalSpy located(&locator, &DeviceLocator::located);
    locator.requestPosition();
    QVERIFY(located.wait(3000));

    // -1, and not 0: zero metres of error is a claim no positioning system
    // makes. devicelocator.h documents the value.
    QCOMPARE(located.constFirst().at(1).toDouble(), -1.0);
}

void TestPortalLocator::aRefusalIsPermissionDenied()
{
    PortalLocator locator(QDBusConnection::sessionBus());
    m_portal->requestPathFor = [&locator](const QString &token) {
        return locator.requestPathFor(token);
    };
    m_portal->onStart = [](FakePortal &portal, const QString &request, const QString &) {
        portal.respond(request, 1);
    };

    QSignalSpy failed(&locator, &DeviceLocator::failed);
    locator.requestPosition();
    QVERIFY(failed.wait(3000));

    QCOMPARE(failed.constFirst().at(0).value<DeviceLocator::Failure>(),
             DeviceLocator::Failure::PermissionDenied);
}

void TestPortalLocator::anErrorResponseIsAnError()
{
    PortalLocator locator(QDBusConnection::sessionBus());
    m_portal->requestPathFor = [&locator](const QString &token) {
        return locator.requestPathFor(token);
    };
    m_portal->onStart = [](FakePortal &portal, const QString &request, const QString &) {
        portal.respond(request, 2);
    };

    QSignalSpy failed(&locator, &DeviceLocator::failed);
    locator.requestPosition();
    QVERIFY(failed.wait(3000));

    QCOMPARE(failed.constFirst().at(0).value<DeviceLocator::Failure>(),
             DeviceLocator::Failure::Error);
}

void TestPortalLocator::nothingArrivingIsATimeout()
{
    PortalLocator locator(QDBusConnection::sessionBus());
    locator.setTimeout(300);
    m_portal->requestPathFor = [&locator](const QString &token) {
        return locator.requestPathFor(token);
    };

    // Granted, and then silence — GeoClue indoors, with no fix to give.
    QString sessionUsed;
    m_portal->onStart = [&sessionUsed](FakePortal &portal, const QString &request,
                                       const QString &session) {
        sessionUsed = session;
        portal.respond(request, 0);
    };

    QSignalSpy failed(&locator, &DeviceLocator::failed);
    locator.requestPosition();
    QVERIFY(failed.wait(3000));

    QCOMPARE(failed.constFirst().at(0).value<DeviceLocator::Failure>(),
             DeviceLocator::Failure::Timeout);

    // A session that produced nothing is still a session, and it is closed.
    QTRY_COMPARE_WITH_TIMEOUT(m_portal->closesOf(sessionUsed), 1, 3000);
}

void TestPortalLocator::aSecondPressWhileWaitingOpensNoSecondSession()
{
    PortalLocator locator(QDBusConnection::sessionBus());
    m_portal->requestPathFor = [&locator](const QString &token) {
        return locator.requestPathFor(token);
    };
    m_portal->onStart = [](FakePortal &portal, const QString &request, const QString &session) {
        portal.respond(request, 0);
        portal.sendFix(session, 1.0, 2.0);
    };

    QSignalSpy located(&locator, &DeviceLocator::located);
    locator.requestPosition();
    locator.requestPosition();
    locator.requestPosition();
    QVERIFY(located.wait(3000));

    // One dialog, one session, one answer. Three presses are impatience.
    QCOMPARE(m_portal->sessionsCreated, 1);
    QCOMPARE(located.count(), 1);
}

void TestPortalLocator::aStrangersSessionIsIgnored()
{
    PortalLocator locator(QDBusConnection::sessionBus());
    m_portal->requestPathFor = [&locator](const QString &token) {
        return locator.requestPathFor(token);
    };

    // LocationUpdated is broadcast on the portal object for every session on
    // the bus. Somebody else's fix first, then ours.
    m_portal->onStart = [](FakePortal &portal, const QString &request, const QString &session) {
        portal.respond(request, 0);
        portal.sendFix(QStringLiteral("/org/freedesktop/portal/desktop/session/fake/somebody"),
                       0.0, 0.0);
        portal.sendFix(session, 48.85, 2.35);
    };

    QSignalSpy located(&locator, &DeviceLocator::located);
    locator.requestPosition();
    QVERIFY(located.wait(3000));

    QCOMPARE(located.count(), 1);
    QCOMPARE(located.constFirst().at(0).value<Coordinate>().latitude, 48.85);
}

void TestPortalLocator::cancelReportsNothingAndClosesTheSession()
{
    PortalLocator locator(QDBusConnection::sessionBus());
    m_portal->requestPathFor = [&locator](const QString &token) {
        return locator.requestPathFor(token);
    };

    // Granted, then a fix — but the caller has changed its mind in between.
    QString sessionUsed;
    m_portal->onStart = [&sessionUsed](FakePortal &portal, const QString &request,
                                       const QString &session) {
        sessionUsed = session;
        portal.respond(request, 0);
    };

    QSignalSpy located(&locator, &DeviceLocator::located);
    QSignalSpy failed(&locator, &DeviceLocator::failed);

    locator.requestPosition();
    // Let CreateSession and Start go round, so there is a session to close.
    QTRY_VERIFY_WITH_TIMEOUT(!sessionUsed.isEmpty(), 3000);

    locator.cancel();
    QVERIFY(!locator.isRequestInFlight());

    // A late fix for the cancelled request is nobody's answer.
    m_portal->sendFix(sessionUsed, 1.0, 1.0);
    QTest::qWait(200);

    QCOMPARE(located.count(), 0);
    QCOMPARE(failed.count(), 0);
    QTRY_COMPARE_WITH_TIMEOUT(m_portal->closesOf(sessionUsed), 1, 3000);
}

void TestPortalLocator::theAccuracyAskedForIsCity()
{
    PortalLocator locator(QDBusConnection::sessionBus());
    m_portal->requestPathFor = [&locator](const QString &token) {
        return locator.requestPathFor(token);
    };
    m_portal->onStart = [](FakePortal &portal, const QString &request, const QString &session) {
        portal.respond(request, 0);
        portal.sendFix(session, 1.0, 2.0);
    };

    QSignalSpy located(&locator, &DeviceLocator::located);
    locator.requestPosition();
    QVERIFY(located.wait(3000));

    // 2 is CITY in the portal's enum. A weather forecast is answered on a grid
    // kilometres across; asking for more would ask the reader to grant more
    // than the feature uses, and the dialog says what was asked for.
    QCOMPARE(m_portal->lastSessionOptions.value(QStringLiteral("accuracy")).toUInt(), 2u);
    QVERIFY(m_portal->lastSessionOptions.contains(QStringLiteral("session_handle_token")));
}

void TestPortalLocator::aSessionCreatedForACancelledRequestIsClosed()
{
    // The window the serial exists for. A CreateSession reply that lands after
    // its request was cancelled used to be dropped on the floor — but the
    // portal had already created the session, and xdg-desktop-portal reaps one
    // only when the owning bus name goes away, so it kept GeoClue reporting to
    // nobody for the life of the process.
    //
    // Every other case in this file answers CreateSession before it can
    // cancel, which is why this needs the reply held open.
    PortalLocator locator(QDBusConnection::sessionBus());
    m_portal->requestPathFor = [&locator](const QString &token) {
        return locator.requestPathFor(token);
    };

    // Cancelled in the SAME turn of the event loop, before anything can have
    // answered: an asyncCall cannot complete without the loop running, so the
    // portal has not been asked yet and certainly has not replied. That is the
    // window, and it needs no delayed reply to reach — it is simply the one
    // sequence cancelReportsNothingAndClosesTheSession deliberately avoids by
    // waiting for the session to exist first.
    locator.requestPosition();
    locator.cancel();
    QVERIFY(!locator.isRequestInFlight());

    // The portal now hears the call and creates a session for a request
    // nobody wants any more...
    QTRY_VERIFY_WITH_TIMEOUT(m_portal->sessionsCreated == 1, 3000);
    QVERIFY(!m_portal->lastSessionPath.isEmpty());

    // ...and it is closed rather than left running for the life of the
    // process, which is what xdg-desktop-portal would otherwise do with it.
    QTRY_COMPARE_WITH_TIMEOUT(m_portal->closesOf(m_portal->lastSessionPath), 1, 3000);
}

void TestPortalLocator::aPermissionDialogNobodyAnswersIsBounded()
{
    // The other new clock. timeout() bounds the arrival of a POSITION and does
    // not start until the portal has said yes, so a dialog left open is
    // covered by its own generous bound instead — which is three minutes in
    // the product and has to be settable to be reachable here.
    //
    // Granted never, and the session created: the portal shows the prompt and
    // nobody touches it.
    PortalLocator locator(QDBusConnection::sessionBus());
    locator.setDialogTimeout(300);
    m_portal->requestPathFor = [&locator](const QString &token) {
        return locator.requestPathFor(token);
    };

    QString sessionUsed;
    m_portal->onStart = [&sessionUsed](FakePortal &, const QString &, const QString &session) {
        sessionUsed = session;   // Start returns, and then silence.
    };

    QSignalSpy failed(&locator, &DeviceLocator::failed);
    locator.requestPosition();

    QVERIFY(failed.wait(3000));
    QCOMPARE(failed.constFirst().at(0).value<DeviceLocator::Failure>(),
             DeviceLocator::Failure::Timeout);

    // And the session goes with it, rather than outliving a prompt nobody
    // answered.
    QTRY_COMPARE_WITH_TIMEOUT(m_portal->closesOf(sessionUsed), 1, 3000);
}

QTEST_GUILESS_MAIN(TestPortalLocator)
#include "tst_portallocator.moc"
