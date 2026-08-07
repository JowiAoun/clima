// SPDX-FileCopyrightText: 2026 Jowi Aoun
// SPDX-License-Identifier: GPL-3.0-or-later
//
// clima-widget: the process that draws the tiles on the desktop.
//
// ============================================================================
// WHY THIS IS A SEPARATE EXECUTABLE FROM THE APP
//
// Because a GNOME Shell extension has to *spawn* it. Extension identity on
// Wayland is established by an inherited socket fd — the shell makes a
// socketpair, keeps one end and hands the child the other as WAYLAND_SOCKET —
// and only then can it own, re-type and pin the window that appears. That was
// measured before any of this was written; see docs/widgets.md.
//
// `clima --widgets` could not be spawned that way without the shell also
// starting a full weather app, and a desktop with six tiles on it would be six
// weather apps.
//
// ============================================================================
// ONE PROCESS, ALL THE TILES
//
// Six tiles in one process is about 95 MB; six processes is about 280 MB. So
// the host lays every requested tile out in one window and the shell adopts
// one window. This is also what makes the per-process RSS budget in
// docs/03-tech-stack.md §3.4 mean something: app < 120 MB, daemon < 25 MB
// headless, widget host < 60 MB plus about 6 MB a tile.
//
// ============================================================================
// WHAT THIS PROCESS MAY NOT DO
//
// Fetch. Open a socket. Write the cache. Every number on screen came from
// clima-daemon over the session bus, and widgets/CMakeLists.txt turns that
// into a check on the built binary rather than a promise in a comment — the
// symbol table is inspected for HttpClient and the providers, because libclima
// is a static archive and `ldd` would prove nothing.
//
// When the daemon is not there, the tiles draw their last snapshot and say how
// old it is. They never blank. That is non-negotiable 1 in docs/README.md, one
// process further out than it was written for.

#include "appfont.h"
#include "daemonlink.h"
#include "layershell.h"
#include "settings.h"
#include "widgetconfig.h"
#include "widgetoptions.h"

#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QWindow>

#include <cstdio>
#include <cstdlib>

int main(int argc, char *argv[])
{
    QGuiApplication app(argc, argv);

    // The same organisation and application names the app uses, because they
    // are what QSettings and QStandardPaths key on: the host has to read the
    // INI the app wrote or a widget shows °C to somebody who chose °F.
    QGuiApplication::setOrganizationName(QStringLiteral("Clima"));
    QGuiApplication::setOrganizationDomain(QStringLiteral("github.io"));
    QGuiApplication::setApplicationName(QStringLiteral(CLIMA_APP_NAME));
    QGuiApplication::setApplicationVersion(QStringLiteral(CLIMA_VERSION));

    // A distinct desktop file name, and it matters more here than it does for
    // the app. Wayland reads it to decide what this surface is called and what
    // icon it carries, and a widget host claiming to be the weather app puts a
    // second Clima in the dock the moment anything shows it in a window list.
    QGuiApplication::setDesktopFileName(QStringLiteral(CLIMA_WIDGET_DESKTOP_ID));

    Settings::prepareStorage();
    AppFont::install();

    WidgetOptions::parseCommandLine(app);

    // Decided here rather than in QML, because it changes whether this process
    // ever touches D-Bus. A recorded snapshot is how the tiles are reviewed in
    // the gallery and photographed in CI, and CI has no session bus.
    const QString snapshot = WidgetOptions::instance()->snapshotFile();
    if (snapshot.isEmpty())
        DaemonLink::instance()->connectToBus();
    else
        DaemonLink::instance()->useSnapshotFile(snapshot);

    QQmlApplicationEngine engine;
    QObject::connect(
        &engine, &QQmlApplicationEngine::objectCreationFailed, &app,
        []() { QCoreApplication::exit(-1); },
        Qt::QueuedConnection);
    engine.loadFromModule("Clima.Widgets", "WidgetWindow");

    // ---- shown from here, not from QML -------------------------------------
    //
    // WidgetWindow.qml is `visible: false` and this is why. A window that shows
    // itself during component completion already has a platform surface by the
    // time this line runs, and a surface has already committed to being an
    // ordinary xdg-shell window — `LayerShellQt::Window::get()` after that
    // point warns onto a logging category nobody has enabled and changes
    // nothing. The tiles would appear, floating, and the only evidence that
    // `--pin` had failed would be that they were in the wrong place.
    //
    // So: find the window, ask for a layer surface, then show it.
    const QList<QObject *> roots = engine.rootObjects();
    QWindow               *window =
        roots.isEmpty() ? nullptr : qobject_cast<QWindow *>(roots.constFirst());
    if (window == nullptr) {
        std::fprintf(stderr, "clima-widget: the QML root is not a window.\n");
        return 1;
    }

    const WidgetOptions *options = WidgetOptions::instance();
    if (options->pin() != WidgetOptions::Pin::Off) {
        const QString unavailable = clima::widgets::layershell::unavailableReason();

        // `--pin on` refuses rather than degrades, because the caller that
        // passes it is an autostart entry or a compositor config line and there
        // is nobody at the keyboard to notice that the tiles came up floating in
        // the middle of the screen. `--pin auto` is the one a person types.
        if (!unavailable.isEmpty() && options->pin() == WidgetOptions::Pin::On) {
            std::fprintf(stderr, "clima-widget: --pin on, but %s.\n",
                         qUtf8Printable(unavailable));
            return 3;
        }

        clima::widgets::layershell::pin(window, options->placement());
    }

    window->setVisible(true);

    return QGuiApplication::exec();
}
