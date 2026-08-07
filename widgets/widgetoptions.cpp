// SPDX-FileCopyrightText: 2026 Jowi Aoun
// SPDX-License-Identifier: GPL-3.0-or-later

#include "widgetoptions.h"

#include "daemonlink.h"
#include "settings.h"
#include "widgetclock.h"

#include <QCommandLineOption>
#include <QCommandLineParser>
#include <QCoreApplication>
#include <QDateTime>
#include <QSettings>
#include <QVariantMap>

#include <cstdio>

namespace layershell = clima::widgets::layershell;

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
    const QCommandLineOption filmOption(
        QStringLiteral("film"),
        QStringLiteral("Write a numbered sequence of frames instead of one."),
        QStringLiteral("prefix"));
    const QCommandLineOption framesOption(
        QStringLiteral("frames"), QStringLiteral("How many frames --film writes. Default 8."),
        QStringLiteral("n"), QStringLiteral("8"));
    const QCommandLineOption everyOption(
        QStringLiteral("every"), QStringLiteral("Milliseconds between frames. Default 500."),
        QStringLiteral("ms"), QStringLiteral("500"));
    const QCommandLineOption stillOption(
        QStringLiteral("still"), QStringLiteral("Hold every animation at its end state."));
    const QCommandLineOption windowedOption(
        QStringLiteral("windowed"),
        QStringLiteral("Show an ordinary decorated window rather than a bare surface."));
    const QCommandLineOption listOption(
        QStringLiteral("list"), QStringLiteral("Print the widget catalogue and exit."));
    const QCommandLineOption nowOption(
        QStringLiteral("now"),
        QStringLiteral("Draw as though it were this instant (ISO 8601). For captures."),
        QStringLiteral("iso"));

    // ---- pinning to the desktop --------------------------------------------
    //
    // The help text names the compositors rather than the protocol, because
    // "zwlr_layer_shell_v1" tells somebody running KDE nothing about whether it
    // will work for them.
    const QCommandLineOption pinOption(
        QStringLiteral("pin"),
        QStringLiteral("Pin the tiles to the desktop, under every window: "
                       "auto | on | off. Works on KDE Plasma, Sway, Hyprland and "
                       "other wlroots compositors. On GNOME the shell extension "
                       "does this instead. Default: auto."),
        QStringLiteral("mode"), QStringLiteral("auto"));
    const QCommandLineOption anchorOption(
        QStringLiteral("anchor"),
        QStringLiteral("Where a pinned window sits: %1. Default top-right.")
            .arg(layershell::anchorNames().join(QStringLiteral(" | "))),
        QStringLiteral("where"), QStringLiteral("top-right"));
    const QCommandLineOption marginOption(
        QStringLiteral("margin"),
        QStringLiteral("How far a pinned window sits from the screen edge. Default 24."),
        QStringLiteral("px"), QStringLiteral("24"));
    const QCommandLineOption layerOption(
        QStringLiteral("layer"),
        QStringLiteral("Which layer a pinned window is on: %1. Default bottom, "
                       "which is above the wallpaper and below every window.")
            .arg(layershell::layerNames().join(QStringLiteral(" | "))),
        QStringLiteral("name"), QStringLiteral("bottom"));

    parser.addOption(widgetOption);
    parser.addOption(placeOption);
    parser.addOption(columnsOption);
    parser.addOption(schemeOption);
    parser.addOption(snapshotOption);
    parser.addOption(grabOption);
    parser.addOption(filmOption);
    parser.addOption(framesOption);
    parser.addOption(everyOption);
    parser.addOption(stillOption);
    parser.addOption(windowedOption);
    parser.addOption(listOption);
    parser.addOption(nowOption);
    parser.addOption(pinOption);
    parser.addOption(anchorOption);
    parser.addOption(marginOption);
    parser.addOption(layerOption);

    parser.process(app);

    // Before anything else reads a clock. Two things on a tile move without new
    // data — the sun mark and the age footer — and both are correct behaviour
    // that makes a screenshot different every time it is taken. See
    // widgets/widgetclock.h.
    const QString frozen = parser.value(nowOption);
    if (!frozen.isEmpty()) {
        const QDateTime instant = QDateTime::fromString(frozen, Qt::ISODate);
        if (!instant.isValid()) {
            std::fprintf(stderr, "clima-widget: --now takes an ISO 8601 instant, "
                                 "for example 2026-08-06T15:00:00-07:00.\n");
            std::exit(2);
        }
        clima::widgets::freezeClock(instant);
    }

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
    self->m_film         = parser.value(filmOption);
    self->m_frames       = parser.value(framesOption).toInt();
    self->m_every        = parser.value(everyOption).toInt();

    if (self->m_frames < 1 || self->m_every < 1) {
        std::fprintf(stderr, "clima-widget: --frames and --every take positive numbers.\n");
        std::exit(2);
    }

    // A capture holds still, for the same reason app/qml/Clima/Main.qml gives:
    // otherwise the shutter catches whichever frame of a reveal it happened to
    // land on, and two runs of the same command produce two different files.
    // A single capture holds still so two runs of the same command produce the
    // same file. A film does not, and must not: a contact sheet of eight
    // identical frames is not a review of anything.
    self->m_still    = parser.isSet(stillOption)
        || (!self->m_grab.isEmpty() && self->m_film.isEmpty());
    self->m_windowed = parser.isSet(windowedOption) || !self->m_grab.isEmpty()
        || !self->m_film.isEmpty();

    // ---- pinning ------------------------------------------------------------

    const QString pin = parser.value(pinOption);
    if (pin == QLatin1String("auto")) {
        self->m_pin = Pin::Auto;
    } else if (pin == QLatin1String("on")) {
        self->m_pin = Pin::On;
    } else if (pin == QLatin1String("off")) {
        self->m_pin = Pin::Off;
    } else {
        std::fprintf(stderr, "clima-widget: --pin takes auto, on or off.\n");
        std::exit(2);
    }

    // A window somebody asked to be ordinary, or one that exists only to be
    // photographed, is not pinned to anything. Both already force --windowed
    // above; this says the same thing about the surface type, because the two
    // are separate decisions on Wayland — a frameless window is still an
    // xdg-shell window.
    if (self->m_windowed)
        self->m_pin = Pin::Off;

    self->m_placement.anchor = parser.value(anchorOption);
    if (!layershell::anchorNames().contains(self->m_placement.anchor)) {
        std::fprintf(stderr, "clima-widget: unknown --anchor \"%s\". Known: %s\n",
                     qUtf8Printable(self->m_placement.anchor),
                     qUtf8Printable(layershell::anchorNames().join(u' ')));
        std::exit(2);
    }

    self->m_placement.layer = parser.value(layerOption);
    if (!layershell::layerNames().contains(self->m_placement.layer)) {
        std::fprintf(stderr, "clima-widget: unknown --layer \"%s\". Known: %s\n",
                     qUtf8Printable(self->m_placement.layer),
                     qUtf8Printable(layershell::layerNames().join(u' ')));
        std::exit(2);
    }

    bool      marginOk = false;
    const int margin   = parser.value(marginOption).toInt(&marginOk);
    if (!marginOk || margin < 0) {
        std::fprintf(stderr, "clima-widget: --margin takes a number of pixels, zero or more.\n");
        std::exit(2);
    }
    self->m_placement.margin = margin;
}
