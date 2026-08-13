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
// It IS D-Bus-activatable — packaging/linux/clima-daemon.service.in — and this
// comment used to say the opposite, at length, so it is worth being clear about
// what changed and what did not.
//
// The argument was: a bus-activated process is spawned by dbus-daemon, so
// gnome-shell can never own its Wayland client and can never adopt its window.
// That is true, it was measured (docs/widgets.md, finding 1), and it rules
// activation out — for the WIDGET HOST, which is the process with a window to
// adopt. This one has no window, no Wayland connection and nothing for a shell
// to own. The constraint was carried one process too far, and the cost of
// carrying it was a desktop full of tiles that had nothing to read: the GNOME
// extension starts this daemon, and on KDE, Sway, Hyprland, Wayfire and river —
// where `clima-widget --pin` needs no extension at all — nothing did.
//
// So there are now three ways it starts, in order of how little they ask of the
// user: the bus activates it when a widget host or the extension looks for it,
// an /etc/xdg/autostart entry starts it at login where one can be installed, and
// anybody can run it by hand. All three are idempotent — see the name
// registration below, which exits 5 rather than fighting over the name.

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
#include <QTimer>

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

    const QCommandLineOption dumpOption(
        QStringLiteral("dump-snapshot"),
        QStringLiteral("Print one full snapshot as JSON and exit. Needs no session bus."));
    parser.addOption(dumpOption);

    const QCommandLineOption placeOption(
        QStringLiteral("place"),
        QStringLiteral("Which place --dump-snapshot is for. Defaults to the home place."),
        QStringLiteral("id"), QStringLiteral("home"));
    parser.addOption(placeOption);

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

    // ---- one snapshot, no bus ------------------------------------------------
    //
    // This is how tests/fixtures/wire/*.json are recorded (scripts/record-wire.sh),
    // and it is deliberately the same code path the bus serves rather than a
    // second encoder written for the purpose: a recorded fixture that came from
    // somewhere other than the daemon would drift from what the daemon actually
    // sends, and the whole value of the recording is that it does not.
    //
    // It waits for the snapshot to SETTLE, which is not the same as waiting for
    // the first one. Three fetches are in flight — the forecast, the air
    // quality and the alerts — and each publishes as it lands, so a recorder
    // that took the first non-empty answer would write a file with no
    // air-quality index and `alertsKnown: false` in it. That file would then be
    // the fixture every widget was reviewed against, and the AQI dial would
    // have been developed against a permanent dash.
    //
    // So: keep the most recent snapshot, and print it once nothing new has
    // arrived for a moment.
    if (parser.isSet(dumpOption)) {
        auto *service = new SnapshotService(&app);
        service->configure(fixtureName);

        const QString place = parser.value(placeOption);
        const QString token = service->subscribe(place, {}, -1, -1);
        if (token.isEmpty()) {
            std::fprintf(stderr, "clima-daemon: could not resolve a place called \"%s\".\n",
                         qPrintable(place));
            return 6;
        }

        auto *settled = new QTimer(&app);
        settled->setSingleShot(true);
        settled->setInterval(750);

        auto *latest = new QString;

        QObject::connect(service, &SnapshotService::snapshotChanged, &app,
                         [token, settled, latest](const QString &delivered, const QString &json) {
                             if (delivered != token)
                                 return;
                             // The cold-start publish carries no reading at all.
                             // subscribe() delivers it immediately so a widget
                             // has a shape to bind to before the first fetch
                             // lands; it is not something to record.
                             if (json.contains(QLatin1String("\"state\":\"unknown\"")))
                                 return;
                             *latest = json;
                             settled->start();
                         });

        QObject::connect(settled, &QTimer::timeout, &app, [latest]() {
            std::printf("%s\n", qPrintable(*latest));
            QCoreApplication::quit();
        });

        // Long enough for a live fetch over a slow link, and finite so that a
        // recording script cannot hang a CI job. Exiting non-zero rather than
        // writing a half-empty file is the point.
        QTimer::singleShot(30000, &app, []() {
            std::fprintf(stderr, "clima-daemon: no snapshot arrived within 30 s.\n");
            QCoreApplication::exit(7);
        });

        return QCoreApplication::exec();
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
