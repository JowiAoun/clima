// SPDX-FileCopyrightText: 2026 Jowi Aoun
// SPDX-License-Identifier: GPL-3.0-or-later
//
// clima-daemon: fetches the weather once, and serves it to everything else on
// the desktop over the session bus.
//
// ============================================================================
// WHY IT IS A SEPARATE EXECUTABLE AND NOT A MODE OF THE APP
//
// Because it has to be able to run when the app is not, and it must not carry
// a GUI when it does. A widget on a desktop is useful precisely on the days
// nobody opens the weather app, and `clima --daemon` would mean linking Qt
// Quick into a process that draws nothing and paying its startup cost.
//
// It links libclima and Qt DBus. No Gui, no Quick, no QML — and
// clima_forbid_gui() in the build file makes that a configure-time error
// rather than a rule in a document.
//
// ============================================================================
// LIFETIME
//
// Deliberately dumb: it starts, it registers, it serves, and it exits when
// told. There is no idle timeout and no auto-quit, because a daemon that
// disappears after five idle minutes is a daemon whose widgets go stale in a
// way that looks like a bug in the widget.
//
// It is also NOT D-Bus-activatable, and that is not an oversight. A
// bus-activated process is spawned by dbus-daemon, which means gnome-shell can
// never own its Wayland client and can never adopt its windows — see
// docs/widgets.md, where that was measured. The widget host is spawned by the
// shell extension; this daemon is started by whatever wants it and its
// autostart is an ordinary .desktop entry.

#include "daemonadaptor.h"
#include "daemonconfig.h"
#include "snapshotservice.h"

#include "libclima/providers/fixture/fixtureprovider.h"

#include <QCommandLineOption>
#include <QCommandLineParser>
#include <QCoreApplication>
#include <QDBusConnection>
#include <QDBusConnectionInterface>
#include <QDBusError>
#include <QDBusReply>

#include <cstdio>

int main(int argc, char *argv[])
{
    QCoreApplication app(argc, argv);
    QCoreApplication::setApplicationName(QStringLiteral("clima-daemon"));
    QCoreApplication::setApplicationVersion(QStringLiteral(CLIMA_VERSION));
    QCoreApplication::setOrganizationName(QStringLiteral(CLIMA_APP_NAME));

    QCommandLineParser parser;
    parser.setApplicationDescription(
        QStringLiteral("Fetches the weather once and serves it to Clima's widgets and tray."));
    parser.addHelpOption();
    parser.addVersionOption();

    const QCommandLineOption fixtureOption(
        QStringLiteral("fixture"),
        QStringLiteral("Serve a recorded fixture at a frozen clock instead of the network. "
                       "One of: %1")
            .arg(clima::fixtures::names().join(QStringLiteral(", "))),
        QStringLiteral("name"));
    parser.addOption(fixtureOption);

    const QCommandLineOption replaceOption(
        QStringLiteral("replace"),
        QStringLiteral("Take the bus name from an already-running daemon."));
    parser.addOption(replaceOption);

    const QCommandLineOption printOption(
        QStringLiteral("print-address"),
        QStringLiteral("Print the service name, object path and interface, then exit."));
    parser.addOption(printOption);

    parser.process(app);

    if (parser.isSet(printOption)) {
        std::printf("service   %s\npath      %s\ninterface %s\n", CLIMA_DAEMON_SERVICE,
                    CLIMA_DAEMON_PATH, CLIMA_DAEMON_INTERFACE);
        return 0;
    }

    const QString fixtureName = parser.value(fixtureOption);
    if (!fixtureName.isEmpty() && !clima::fixtures::exists(fixtureName)) {
        std::fprintf(stderr, "clima-daemon: no fixture called \"%s\". Known: %s\n",
                     qPrintable(fixtureName),
                     qPrintable(clima::fixtures::names().join(QStringLiteral(", "))));
        return 2;
    }

    QDBusConnection bus = QDBusConnection::sessionBus();
    if (!bus.isConnected()) {
        std::fprintf(stderr,
                     "clima-daemon: there is no session bus to serve on.\n"
                     "  %s\n",
                     qPrintable(bus.lastError().message()));
        return 3;
    }

    // The service is configured *before* the name is taken, so that a client
    // which connects the instant the name appears finds a daemon that can
    // already answer rather than one still opening its cache.
    auto *service = new SnapshotService(&app);
    service->configure(fixtureName);

    new DaemonAdaptor(service);

    if (!bus.registerObject(QStringLiteral(CLIMA_DAEMON_PATH), service)) {
        std::fprintf(stderr, "clima-daemon: could not export %s: %s\n", CLIMA_DAEMON_PATH,
                     qPrintable(bus.lastError().message()));
        return 4;
    }

    const auto queueOption = parser.isSet(replaceOption)
        ? QDBusConnectionInterface::ReplaceExistingService
        : QDBusConnectionInterface::DontQueueService;

    // Always allowed, regardless of how this one started. The alternative is a
    // daemon that can only be upgraded by finding and killing it, and the
    // person who needs to do that is the one least likely to know how.
    const auto replacementOption = QDBusConnectionInterface::AllowReplacement;

    const QDBusReply<QDBusConnectionInterface::RegisterServiceReply> reply =
        bus.interface()->registerService(QStringLiteral(CLIMA_DAEMON_SERVICE), queueOption,
                                         replacementOption);

    if (!reply.isValid() || reply.value() != QDBusConnectionInterface::ServiceRegistered) {
        // The overwhelmingly common cause is a daemon already running, which is
        // not an error worth a stack trace — it is the system working.
        std::fprintf(stderr,
                     "clima-daemon: %s is already owned. Another daemon is running;\n"
                     "              pass --replace to take over from it.\n",
                     CLIMA_DAEMON_SERVICE);
        return 5;
    }

    return QCoreApplication::exec();
}
