// SPDX-FileCopyrightText: 2026 Jowi Aoun
// SPDX-License-Identifier: GPL-3.0-or-later
//
// The gallery's entry point. Three things happen, and the interesting one is
// the thing that does not.
//
// app/main.cpp calls Settings::prepareStorage() before anything else, because
// the weather app remembers its window size and its units. This does not, and
// that is a rule rather than an omission: a developer tool has no business
// writing to the config directory the product reads. Open the gallery at
// 1500x950 and the app must still open where the reader last left it.

#include "galleryoptions.h"

#include "climaconfig.h"

#include <QGuiApplication>
#include <QQmlApplicationEngine>

int main(int argc, char *argv[])
{
    QGuiApplication app(argc, argv);

    // Its own name, and no organisation. The name is what QCommandLineParser
    // prints in front of an error and what --version prints; the organisation
    // is what QStandardPaths would key a config directory off, and there is no
    // config directory here to key.
    QGuiApplication::setApplicationName(QStringLiteral("clima-gallery"));
    QGuiApplication::setApplicationVersion(QStringLiteral(CLIMA_VERSION));

    // Before the engine, because Main.qml reads these at construction: the
    // window sizes itself from --size, and the stage frames itself from
    // --viewport.
    //
    // Does not return for --help, --version, an unknown flag or a malformed
    // value.
    GalleryOptions::parseCommandLine(app);

    QQmlApplicationEngine engine;

    // A QML file that fails to construct its root object leaves the engine
    // holding nothing and the event loop with no window to show — the process
    // comes up invisible and has to be killed. Exit instead, non-zero, so a
    // headless capture in CI fails as a failure rather than as a timeout.
    QObject::connect(
        &engine, &QQmlApplicationEngine::objectCreationFailed, &app,
        []() { QCoreApplication::exit(-1); },
        Qt::QueuedConnection);

    // By URI. `Clima.Gallery` is this executable's own module; the components it
    // stages come from `Clima`, which it links and which Main.qml imports.
    engine.loadFromModule("Clima.Gallery", "Main");

    return app.exec();
}
