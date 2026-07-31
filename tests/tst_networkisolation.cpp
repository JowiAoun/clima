// SPDX-FileCopyrightText: 2026 Jowi Aoun
// SPDX-License-Identifier: GPL-3.0-or-later
//
// The test that fails if a test opens an external socket.
//
// Two halves, and both are necessary:
//
//   1. The guard trips. A deliberate attempt to reach a public host is
//      recorded and blocked. Without this, "no external attempts were
//      recorded" would be equally true of a guard that was never installed —
//      which is the failure mode that would let the whole rule quietly stop
//      working.
//
//   2. The guard does not trip on loopback. HttpStub has to keep working, so
//      the guard has to distinguish "the internet" from "this machine". A
//      guard that blocked everything would be caught immediately; a guard that
//      blocked loopback *slowly* would just make the suite flaky.
//
// Every other test class in this suite installs the same guard in
// initTestCase and asserts an empty attempt list in cleanup, so the rule is
// enforced per test rather than once per binary.

#include "libclima/core/clock.h"
#include "libclima/net/httpclient.h"
#include "support/httpstub.h"
#include "support/networkguard.h"

#include <QTcpSocket>
#include <QTest>

using namespace clima;

class TestNetworkIsolation : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void initTestCase();
    void init();

    void theGuardRecordsAndBlocksAnExternalConnection();
    void theGuardRecordsAnExternalHttpRequest();
    void theGuardLeavesLoopbackAlone();
    void aClientRequestToLoopbackRecordsNothing();
};

void TestNetworkIsolation::initTestCase()
{
    NetworkGuard::install();
}

void TestNetworkIsolation::init()
{
    NetworkGuard::clearAttempts();
}

void TestNetworkIsolation::theGuardRecordsAndBlocksAnExternalConnection()
{
    // A raw socket, which is the lowest level a test could reach for. Qt routes
    // QAbstractSocket through the application proxy factory for any host it
    // does not consider local, which is what makes this catchable at all.
    QTcpSocket socket;
    socket.connectToHost(QStringLiteral("api.open-meteo.com"), 443);

    // It must not connect. The proxy points at the discard port on loopback,
    // so this fails fast rather than waiting out a DNS timeout.
    QVERIFY2(!socket.waitForConnected(2000),
             "a test reached api.open-meteo.com — the network guard is not installed");

    const QStringList attempts = NetworkGuard::externalAttempts();
    QCOMPARE(attempts.size(), 1);
    QCOMPARE(attempts.at(0), QStringLiteral("api.open-meteo.com"));
}

void TestNetworkIsolation::theGuardRecordsAnExternalHttpRequest()
{
    // The same thing one layer up: a provider that hardcoded a real URL.
    FrozenClock clock;
    HttpClient  client(&clock);
    client.setBackoffPolicy(BackoffPolicy{ std::chrono::milliseconds(1), 2.0,
                                           std::chrono::milliseconds(5), 0 },
                            1);

    HttpRequest request;
    request.providerId = QStringLiteral("met-no");
    request.endpoint = QStringLiteral("locationforecast");
    request.url = QUrl(QStringLiteral("https://api.met.no/weatherapi/locationforecast/2.0/compact"));
    request.coordinate = Coordinate{ 59.9139, 10.7522 };

    auto future = client.send(request);
    QVERIFY(QTest::qWaitFor([&future]() { return future.isFinished(); }, 5000));

    QVERIFY(!future.result().hasValue());
    QVERIFY2(!NetworkGuard::externalAttempts().isEmpty(),
             "an outbound request to api.met.no was not seen by the guard");
    QVERIFY(NetworkGuard::externalAttempts().contains(QStringLiteral("api.met.no")));
}

void TestNetworkIsolation::theGuardLeavesLoopbackAlone()
{
    HttpStub stub;
    QVERIFY(stub.listen());

    QTcpSocket socket;
    socket.connectToHost(QHostAddress::LocalHost, QUrl(stub.baseUrl()).port());
    QVERIFY2(socket.waitForConnected(2000), "the guard blocked loopback");

    QCOMPARE(NetworkGuard::externalAttempts(), QStringList());
}

void TestNetworkIsolation::aClientRequestToLoopbackRecordsNothing()
{
    HttpStub stub;
    QVERIFY(stub.listen());
    stub.enqueue(StubResponse::ok(QByteArrayLiteral("{}")));

    FrozenClock clock;
    HttpClient  client(&clock);

    HttpRequest request;
    request.providerId = QStringLiteral("open-meteo");
    request.endpoint = QStringLiteral("forecast");
    request.url = QUrl(stub.baseUrl() + QStringLiteral("/v1/forecast"));
    request.coordinate = Coordinate{ 52.52, 13.405 };

    auto future = client.send(request);
    QVERIFY(QTest::qWaitFor([&future]() { return future.isFinished(); }, 5000));
    QVERIFY(future.result().hasValue());

    QCOMPARE(stub.requestCount(), 1);
    QCOMPARE(NetworkGuard::externalAttempts(), QStringList());
}

QTEST_MAIN(TestNetworkIsolation)
#include "tst_networkisolation.moc"
