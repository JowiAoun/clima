// SPDX-FileCopyrightText: 2026 Jowi Aoun
// SPDX-License-Identifier: GPL-3.0-or-later
//
// Makes it impossible for a test to reach the internet, and noisy when one
// tries.
//
// docs/04-architecture.md §4.11 says it plainly: golden-file provider tests run
// "against recorded API responses committed to `tests/fixtures/` — no network
// in CI". That is a rule with two failure modes, and only one of them is
// obvious.
//
// The obvious one is a red build on a machine with no route out. The expensive
// one is a *green* build on a developer's laptop, where a test that quietly
// reached api.open-meteo.com passed because the real service happened to be up
// and answered the way the fixture said it would. That test is not testing our
// parser; it is testing the weather. It goes red in CI, six commits later,
// naming a file nobody touched.
//
// So this is a guard rather than a convention, and it does two things:
//
//   1. BLOCKS.  It installs a QNetworkProxyFactory that hands back a proxy
//      pointing at the discard port on loopback for every non-local host. Qt
//      consults the application proxy factory from both QNetworkAccessManager
//      and plain QTcpSocket, and QAbstractSocket bypasses the proxy for
//      loopback addresses on its own — which is exactly the split needed here:
//      HttpStub keeps working, and anything else fails to connect.
//
//   2. COUNTS.  Every query the factory is asked is recorded with the host it
//      was asked about. A test can then assert that the number of external
//      queries is zero, which turns "did not reach the network" from a claim
//      into an assertion.
//
// Install it once per test binary, from initTestCase. The factory is
// process-global — that is Qt's design, not a shortcut — so installing it twice
// simply replaces it.

#pragma once

#include <QStringList>

class NetworkGuard
{
public:
    // Installs the factory. Idempotent.
    static void install();

    // Hosts the process tried to resolve a proxy for that were not loopback,
    // in order, with duplicates. Empty is the expected state at the end of
    // every test in this suite.
    static QStringList externalAttempts();

    static void clearAttempts();
};
