// SPDX-FileCopyrightText: 2026 Jowi Aoun
// SPDX-License-Identifier: GPL-3.0-or-later
//
// What the host was asked to put on the desktop.
//
//     clima-widget --widget current-conditions --widget uv-dial
//     clima-widget --list
//     clima-widget --snapshot tests/fixtures/wire/toronto.json --grab tiles.png
//
// The same shape as app/appoptions.h and for the same reason: the command line
// is parsed once, in C++, into a singleton QML binds to — rather than scraped
// out of `Qt.application.arguments` inside a .qml file, which is what
// Main.qml used to do in 180 lines.
//
// ============================================================================
// WHO CHOOSES THE TILES
//
// In order: `--widget`, then the saved layout, then a default set. The saved
// layout is `widgets/enabled` in the same INI the app writes (app/settings.h),
// so the GNOME extension's menu, the Plasma applet's config page and this
// process are all editing one list rather than three.
//
// An id that is not in the catalogue is a hard error with the known ids
// printed, not a silently missing tile. A desktop that renders five of the six
// widgets someone asked for, with no message anywhere, is a bug report that
// takes an afternoon.

#pragma once

#include "layershell.h"

#include <QObject>
#include <QQmlEngine>
#include <QString>
#include <QStringList>

class QCoreApplication;

class WidgetOptions : public QObject
{
    Q_OBJECT
    QML_ELEMENT
    QML_SINGLETON

    Q_PROPERTY(QStringList ids READ ids CONSTANT)
    Q_PROPERTY(QString place READ place CONSTANT)
    Q_PROPERTY(QString scheme READ scheme CONSTANT)
    Q_PROPERTY(int columns READ columns CONSTANT)
    Q_PROPERTY(bool still READ still CONSTANT)
    Q_PROPERTY(QString grab READ grab CONSTANT)

    // A contact sheet rather than one frame. The app has the same three flags
    // and they are here for a reason a tile makes sharper: the states worth
    // looking at are the ones that arrive over TIME — a first snapshot landing,
    // and a daemon going away under a tile that has to keep drawing. Neither is
    // visible in a single shutter.
    Q_PROPERTY(QString film READ film CONSTANT)
    Q_PROPERTY(int frames READ frames CONSTANT)
    Q_PROPERTY(int every READ every CONSTANT)

    // True when the window should be an ordinary decorated window rather than a
    // frameless surface for the shell to adopt. The GNOME extension spawns us
    // without it; a developer looking at a tile wants it.
    Q_PROPERTY(bool windowed READ windowed CONSTANT)

public:

    // What `--pin` was asked for. Not a bool, because the three answers are
    // genuinely different: `Off` never asks the compositor, `Auto` asks and
    // accepts an ordinary window if it cannot have one, and `On` refuses to
    // start rather than put an unpinned window on somebody's desktop — which is
    // what an autostart entry needs, since nobody is watching it start.
    enum class Pin { Auto, On, Off };

    static WidgetOptions *instance();
    static WidgetOptions *create(QQmlEngine *qmlEngine, QJSEngine *jsEngine);

    // Parses, and exits the process on --help, --version, --list or a bad
    // argument. Call before the QML engine exists.
    static void parseCommandLine(QCoreApplication &app);

    [[nodiscard]] QStringList ids() const { return m_ids; }
    [[nodiscard]] QString     place() const { return m_place; }
    [[nodiscard]] QString     scheme() const { return m_scheme; }
    [[nodiscard]] int         columns() const { return m_columns; }
    [[nodiscard]] bool        still() const { return m_still; }
    [[nodiscard]] QString     grab() const { return m_grab; }
    [[nodiscard]] QString     film() const { return m_film; }
    [[nodiscard]] int         frames() const { return m_frames; }
    [[nodiscard]] int         every() const { return m_every; }
    [[nodiscard]] bool        windowed() const { return m_windowed; }
    [[nodiscard]] QString     snapshotFile() const { return m_snapshotFile; }

    [[nodiscard]] Pin pin() const { return m_pin; }

    // Whether `--pin` was typed, as against defaulted to `auto`. The difference
    // is who gets told when the compositor cannot pin: somebody who asked, and
    // not every X11 and GNOME start of a flag nobody passed.
    [[nodiscard]] bool pinWasRequested() const { return m_pinRequested; }

    [[nodiscard]] clima::widgets::layershell::Placement placement() const
    {
        return m_placement;
    }

private:
    // Private, and that is load-bearing rather than tidy: a public
    // `WidgetOptions(QObject *parent = nullptr)` makes this type default-constructible,
    // and Qt's singletonConstructionMode() checks default-constructible BEFORE
    // it looks for create() — so QML would build a second instance and never
    // call the factory. widgets/daemonlink.h has the full argument and what it
    // looked like when it happened.
    explicit WidgetOptions(QObject *parent = nullptr);

    QStringList m_ids;
    QString     m_place = QStringLiteral("home");
    QString     m_scheme;
    QString     m_snapshotFile;
    QString     m_grab;
    QString     m_film;
    int         m_frames = 8;
    int         m_every  = 500;
    int         m_columns  = 1;
    bool        m_still    = false;
    bool        m_windowed = false;

    Pin                                  m_pin          = Pin::Auto;
    bool                                 m_pinRequested = false;
    clima::widgets::layershell::Placement m_placement;
};
