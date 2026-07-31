// SPDX-FileCopyrightText: 2026 Jowi Aoun
// SPDX-License-Identifier: GPL-3.0-or-later
//
// The entry point, and deliberately almost nothing else.
//
// This file's whole job right now is to put a QML engine in front of the Clima
// module and get out of the way. It is short because the thing it replaced —
// `qml Main.qml` — was also short, and the port that introduced it is verified
// by pixel equality against that runtime. Every line here is a line that could
// have made a screenshot differ, so there are as few of them as the job allows.
//
// In particular, the command-line flags (`--grab`, `--viewport`, `--size`, …)
// are still parsed in Main.qml, off `Qt.application.arguments`. Moving that
// parse into a QCommandLineParser here is the obvious next step and it is a
// separate one: a C++ parser has to reproduce the QML parser's behaviour
// exactly, and the only way to know that it has is to hold everything else
// still while it changes.

#include "climaconfig.h"

#include <QGuiApplication>
#include <QQmlApplicationEngine>

int main(int argc, char *argv[])
{
    QGuiApplication app(argc, argv);

    // QStandardPaths derives the cache and config directories from the
    // organisation and application names, so these two decide where the
    // forecast cache will live long before there is a cache. The app ID is the
    // reverse-DNS name everything else in the desktop stack keys off — the
    // desktop entry's basename, the icon name, the D-Bus name — and
    // setDesktopFileName is what lets a Wayland compositor match this window to
    // that entry. Without it the window gets a generic icon and no app-menu
    // association, which is the sort of thing nobody notices until packaging.
    QGuiApplication::setOrganizationName(QStringLiteral("Clima"));
    QGuiApplication::setOrganizationDomain(QStringLiteral("github.io"));
    QGuiApplication::setApplicationName(QStringLiteral(CLIMA_APP_NAME));
    QGuiApplication::setApplicationVersion(QStringLiteral(CLIMA_VERSION));
    QGuiApplication::setDesktopFileName(QStringLiteral(CLIMA_APP_ID));

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
