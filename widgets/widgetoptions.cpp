// SPDX-FileCopyrightText: 2026 Jowi Aoun
// SPDX-License-Identifier: GPL-3.0-or-later

#include "widgetoptions.h"

#include "daemonlink.h"
#include "settings.h"

#include <QCommandLineOption>
#include <QCommandLineParser>
#include <QCoreApplication>
#include <QSettings>
#include <QVariantMap>

#include <cstdio>

namespace {

// What a first run puts on the desktop when nobody has chosen yet.
//
// Four tiles and not ten: a desktop that fills itself with everything the
// moment the extension is enabled is a desktop the user has to tidy, and the
// first impression of a widget set is how much of the wallpaper it left alone.
// Current conditions because it is the reason anyone adds a weather widget,
// the hourly strip because it is the thing the app is actually about, and
// alerts because a warning nobody asked to see is the one that matters.
const QStringList kDefaultIds{
    QStringLiteral("current-conditions"),
    QStringLiteral("hourly-strip"),
    QStringLiteral("daily-strip"),
    QStringLiteral("alerts"),
};

QStringList savedIds()
{
    // Read straight from QSettings rather than through the Settings singleton,
    // because this runs before there is a QML engine and Settings publishes no
    // widget list of its own — the layout belongs to the desktop shell, and the
    // app has no opinion about it.
    QSettings store;
    const QString saved = store.value(QStringLiteral("widgets/enabled")).toString();
    if (saved.trimmed().isEmpty())
        return {};
    return saved.split(u',', Qt::SkipEmptyParts);
}

} // namespace

WidgetOptions::WidgetOptions(QObject *parent)
    : QObject(parent)
{
}

WidgetOptions *WidgetOptions::instance()
{
    static WidgetOptions *options = new WidgetOptions;
    return options;
}

WidgetOptions *WidgetOptions::create(QQmlEngine *, QJSEngine *)
{
    WidgetOptions *options = instance();
    QQmlEngine::setObjectOwnership(options, QQmlEngine::CppOwnership);
    return options;
}

void WidgetOptions::parseCommandLine(QCoreApplication &app)
{
    WidgetOptions *self = instance();

    QCommandLineParser parser;
    parser.setApplicationDescription(
        QStringLiteral("Draws Clima's weather tiles. Reads from clima-daemon; fetches nothing."));
    parser.addHelpOption();
    parser.addVersionOption();

    const QCommandLineOption widgetOption(
        QStringLiteral("widget"),
        QStringLiteral("A widget to show, by catalogue id. Repeat for more than one."),
        QStringLiteral("id"));
    const QCommandLineOption placeOption(
        QStringLiteral("place"),
        QStringLiteral("Which place to show. Defaults to the home place."),
        QStringLiteral("id"), QStringLiteral("home"));
    const QCommandLineOption columnsOption(
        QStringLiteral("columns"), QStringLiteral("How many tiles per row. Default 1."),
        QStringLiteral("n"), QStringLiteral("1"));
    const QCommandLineOption schemeOption(
        QStringLiteral("scheme"), QStringLiteral("dark | light. Default: follow the desktop."),
        QStringLiteral("name"));
    const QCommandLineOption snapshotOption(
        QStringLiteral("snapshot"),
        QStringLiteral("Draw one recorded snapshot from a file instead of reading the bus."),
        QStringLiteral("file"));
    const QCommandLineOption grabOption(
        QStringLiteral("grab"), QStringLiteral("Render once, write a PNG, and exit."),
        QStringLiteral("file"));
    const QCommandLineOption stillOption(
        QStringLiteral("still"), QStringLiteral("Hold every animation at its end state."));
    const QCommandLineOption windowedOption(
        QStringLiteral("windowed"),
        QStringLiteral("Show an ordinary decorated window rather than a bare surface."));
    const QCommandLineOption listOption(
        QStringLiteral("list"), QStringLiteral("Print the widget catalogue and exit."));

    parser.addOption(widgetOption);
    parser.addOption(placeOption);
    parser.addOption(columnsOption);
    parser.addOption(schemeOption);
    parser.addOption(snapshotOption);
    parser.addOption(grabOption);
    parser.addOption(stillOption);
    parser.addOption(windowedOption);
    parser.addOption(listOption);

    parser.process(app);

    const QVariantList catalogue = DaemonLink::instance()->catalogue();

    if (parser.isSet(listOption)) {
        for (const QVariant &entry : catalogue) {
            const QVariantMap widget = entry.toMap();
            std::printf("%-24s %s\n",
                        qPrintable(widget.value(QStringLiteral("id")).toString()),
                        qPrintable(widget.value(QStringLiteral("summary")).toString()));
        }
        std::exit(0);
    }

    QStringList requested = parser.values(widgetOption);
    if (requested.isEmpty())
        requested = savedIds();
    if (requested.isEmpty())
        requested = kDefaultIds;

    QStringList known;
    known.reserve(catalogue.size());
    for (const QVariant &entry : catalogue)
        known.append(entry.toMap().value(QStringLiteral("id")).toString());

    for (const QString &id : std::as_const(requested)) {
        const QString trimmed = id.trimmed();
        if (trimmed.isEmpty())
            continue;
        if (!known.contains(trimmed)) {
            // Loud, and with the list, because the alternative is a desktop
            // quietly one tile short of what somebody configured.
            std::fprintf(stderr,
                         "clima-widget: there is no widget called \"%s\".\n"
                         "              Known: %s\n",
                         qPrintable(trimmed), qPrintable(known.join(QStringLiteral(", "))));
            std::exit(2);
        }
        if (!self->m_ids.contains(trimmed))
            self->m_ids.append(trimmed);
    }

    self->m_place = parser.value(placeOption);

    const QString scheme = parser.value(schemeOption);
    if (!scheme.isEmpty() && scheme != QStringLiteral("dark")
        && scheme != QStringLiteral("light")) {
        std::fprintf(stderr, "clima-widget: --scheme takes \"dark\" or \"light\".\n");
        std::exit(2);
    }
    self->m_scheme = scheme;

    bool      ok      = false;
    const int columns = parser.value(columnsOption).toInt(&ok);
    if (!ok || columns < 1) {
        std::fprintf(stderr, "clima-widget: --columns takes a positive whole number.\n");
        std::exit(2);
    }
    self->m_columns = columns;

    self->m_snapshotFile = parser.value(snapshotOption);
    self->m_grab         = parser.value(grabOption);

    // A capture holds still, for the same reason app/qml/Clima/Main.qml gives:
    // otherwise the shutter catches whichever frame of a reveal it happened to
    // land on, and two runs of the same command produce two different files.
    self->m_still    = parser.isSet(stillOption) || !self->m_grab.isEmpty();
    self->m_windowed = parser.isSet(windowedOption) || !self->m_grab.isEmpty();
}
