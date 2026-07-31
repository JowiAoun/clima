// SPDX-FileCopyrightText: 2026 Jowi Aoun
// SPDX-License-Identifier: GPL-3.0-or-later
//
// The entry point.
//
// Four things happen here and their order is the only interesting thing about
// the file: identity, then storage, then the command line, then the engine.
// Each one is a precondition of the next, and each of the three ways to get it
// wrong is silent.

#include "appoptions.h"
#include "climaconfig.h"
#include "settings.h"

#include <QGuiApplication>
#include <QQmlApplicationEngine>

int main(int argc, char *argv[])
{
    QGuiApplication app(argc, argv);

    // ---- 1. identity -------------------------------------------------------
    // QStandardPaths derives the cache and config directories from the
    // organisation and application names, so these two decide where the
    // forecast cache and the preferences file will live long before there is
    // either. The app ID is the reverse-DNS name the rest of the desktop stack
    // keys off — the desktop entry's basename, the icon name, the D-Bus name —
    // and setDesktopFileName is what lets a Wayland compositor match this
    // window to that entry. Without it the window gets a generic icon and no
    // app-menu association, which is the sort of thing nobody notices until
    // packaging.
    QGuiApplication::setOrganizationName(QStringLiteral("Clima"));
    QGuiApplication::setOrganizationDomain(QStringLiteral("github.io"));
    QGuiApplication::setApplicationName(QStringLiteral(CLIMA_APP_NAME));
    QGuiApplication::setApplicationVersion(QStringLiteral(CLIMA_VERSION));
    QGuiApplication::setDesktopFileName(QStringLiteral(CLIMA_APP_ID));

    // ---- 2. storage --------------------------------------------------------
    // Before anything constructs a QSettings, because the format is decided at
    // construction and never revisited: a QSettings built before this line
    // reads the Windows registry while every later one reads an INI file, and
    // nothing complains — the preferences simply do not stick. Also the moment
    // a superseded config directory gets copied forward. See settings.h.
    Settings::prepareStorage();

    // ---- 3. the command line -----------------------------------------------
    // Before the engine loads anything, because Main.qml reads AppOptions at
    // construction: --viewport and --size choose the window's width, and the
    // window's width chooses which shell is built.
    //
    // Does not return for --help, --version, an unknown flag or a malformed
    // value.
    AppOptions::parseCommandLine(app);

    // ---- 4. the engine -----------------------------------------------------
    QQmlApplicationEngine engine;

    // A QML file that fails to construct its root object leaves the engine
    // holding nothing and the event loop with no window to show — the app comes
    // up as an invisible process that has to be killed. Exit instead, non-zero,
    // so a headless capture in CI fails as a failure rather than as a timeout.
    //
    // Queued, because this signal is emitted from inside load() and calling
    // exit() before exec() has started does nothing at all.
    QObject::connect(
        &engine, &QQmlApplicationEngine::objectCreationFailed, &app,
        []() { QCoreApplication::exit(-1); },
        Qt::QueuedConnection);

    // By URI, not by file path or qrc URL. loadFromModule asks the engine's
    // import machinery for `Clima.Main`, which means the same line works
    // whether the module is compiled into this binary (it is) or found on
    // QML_IMPORT_PATH (which is how qmllint and qmlls see it).
    engine.loadFromModule("Clima", "Main");

    return app.exec();
}
