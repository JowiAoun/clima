// SPDX-FileCopyrightText: 2026 Jowi Aoun
// SPDX-License-Identifier: GPL-3.0-or-later
//
// The command line, parsed once, offered to QML as a singleton.
//
// This replaces a hand-rolled scraper that lived in Main.qml's
// Component.onCompleted and walked Qt.application.arguments with indexOf().
// That scraper worked, and everything it could not do was invisible until you
// wanted it:
//
//   * `--help` printed nothing, because there was nothing to print it from.
//   * `--version` printed nothing either.
//   * A misspelled flag was silently ignored — `--vieport mobile` opened the
//     desktop shell and looked like the flag did nothing.
//   * `--size 30x` warned and carried on, so a headless grab produced a
//     perfectly good screenshot at the wrong size and a warning nobody read.
//
// QCommandLineParser gives us the first two for free and turns the second two
// into what they always were: errors. That is the deal this file makes. A flag
// whose *value* is malformed now stops the process with a message naming the
// flag, rather than warning into a log that a CI job throws away.
//
// ---- what it does not decide ------------------------------------------------
// Nothing here knows what a viewport preset is worth in pixels, or which sky
// phase paints which gradient. Viewports.qml and Theme.qml own those tables and
// stay the only place they are written down. What this file owns is the CLI's
// *vocabulary* — the set of words the parser accepts and lists in --help — and
// the two lists below are exactly that and nothing more. If a preset is ever
// added to Viewports.qml without being added here, the flag is rejected with a
// message listing what is accepted, which is a loud failure rather than a
// quiet one.
//
// gallery/galleryoptions.cpp calls viewportIds() and skyPhases() rather than
// writing the words out again. Two binaries in one repository that disagree
// about whether `tablet` is a viewport is a bug report nobody can reproduce,
// and the whole argument for a vocabulary living in one place applies twice as
// hard once there are two parsers reading from it.
//
// ---- what this file is *not* any more ---------------------------------------
// `--gallery`, `--card` and `--details` used to be here. They are the component
// gallery's flags and the component gallery is `clima-gallery` now, so they
// went with it: the weather app rejects all three, and rejects them by name
// rather than ignoring them, which is what QCommandLineParser does with a flag
// it was never told about.
//
// ---- every property is declared unconditionally ------------------------------
// Half these flags are CLIMA_DEV_TOOLS-only (see the FLAG DISPOSITION comment
// in the .cpp). Only their *registration with the parser* is conditional: the
// properties themselves always exist, so QML can read AppOptions.metric in
// every build and get "" in the ones where the flag was never offered. The
// alternative — #ifdef'ing the Q_PROPERTY list — would mean Main.qml failing to
// load in a packaged build, and failing at the point of use, which is the same
// class of bug the QML_FILES list in app/CMakeLists.txt exists to prevent.
#pragma once

#include <QObject>
#include <QQmlEngine>
#include <QString>
#include <QStringList>

class QCoreApplication;

class AppOptions : public QObject
{
    Q_OBJECT
    QML_ELEMENT
    QML_SINGLETON

    // ---- capture (ships) ---------------------------------------------------
    Q_PROPERTY(QString grab      READ grab      CONSTANT)
    Q_PROPERTY(bool    capturing READ capturing CONSTANT)

    // ---- data source (ships) -----------------------------------------------
    //
    // Ships, and it has to. --grab ships, --grab defaults to a fixture, and a
    // flag whose default a user cannot see or override is not a default, it is
    // a secret.
    Q_PROPERTY(QString fixture READ fixture CONSTANT)

    // ---- geometry (ships) --------------------------------------------------
    Q_PROPERTY(bool    hasSize    READ hasSize    CONSTANT)
    Q_PROPERTY(int     sizeWidth  READ sizeWidth  CONSTANT)
    Q_PROPERTY(int     sizeHeight READ sizeHeight CONSTANT)
    Q_PROPERTY(QString viewport   READ viewport   CONSTANT)

    // ---- filming (dev tools) -----------------------------------------------
    Q_PROPERTY(QString     film   READ film   CONSTANT)
    Q_PROPERTY(int         frames READ frames CONSTANT)
    Q_PROPERTY(int         every  READ every  CONSTANT)
    Q_PROPERTY(QStringList pokes  READ pokes  CONSTANT)

    // ---- opening state (dev tools) -----------------------------------------
    Q_PROPERTY(QString place  READ place  CONSTANT)
    Q_PROPERTY(QString tab    READ tab    CONSTANT)
    Q_PROPERTY(QString sky    READ sky    CONSTANT)
    Q_PROPERTY(QString scheme READ scheme CONSTANT)
    Q_PROPERTY(QString metric READ metric CONSTANT)
    Q_PROPERTY(int     day    READ day    CONSTANT)
    Q_PROPERTY(bool    list   READ list   CONSTANT)
    Q_PROPERTY(qreal   scroll READ scroll CONSTANT)

public:
    // Parses argv into the process-wide instance. Call from main() *before*
    // the QML engine loads anything, because Main.qml reads these at
    // construction and a window sized after its shell has been chosen is a
    // window whose shell was chosen from the wrong width.
    //
    // Does not return on --help, --version or a malformed value: the parser
    // prints and exits, which is what a command-line program is supposed to do.
    static void parseCommandLine(const QCoreApplication &app);

    // The instance parseCommandLine() filled. Never null — before the parse it
    // is simply an AppOptions holding every default, so a unit test or a tool
    // that never calls parseCommandLine() still gets a usable object.
    static AppOptions *instance();

    // QML_SINGLETON's factory. Hands QML the same instance C++ already has,
    // under C++ ownership so the engine does not delete something main() still
    // holds.
    static AppOptions *create(QQmlEngine *qmlEngine, QJSEngine *jsEngine);

    // The vocabulary. Public because --help is generated from it and because a
    // test can then assert it against Viewports.presets rather than trusting
    // the comment at the top of this file.
    static QStringList viewportIds();
    static QStringList skyPhases();
    static QStringList schemes();

    // Which recorded fixture this run replays, or empty for the live network.
    //
    // Resolved rather than stored, because three things can decide it and the
    // order between them is the interesting part:
    //
    //   1. --fixture <name>, or --fixture off, which always wins
    //   2. CLIMA_FIXTURE in the environment, which is how CI says it once for
    //      a whole job rather than on every command
    //   3. a capture — --grab or --film — which defaults to the fixture,
    //      because a screenshot taken from the live network is a screenshot of
    //      a different afternoon every time it is taken
    //
    // and nothing else. An ordinary launch with no flags is live, which is the
    // one case where a user is asking about the weather rather than about the
    // app.
    //
    // ---- except that --place names a place, and a fixture cannot answer -----
    //
    // A fixture is one recorded place. It answers with that place's weather
    // whatever coordinate it is handed — that is what makes it reproducible.
    // So `--place Reykjavik --grab out.png` used to take rule 3, fetch nothing,
    // and write a PNG with "Reykjavik, Capital Region" in the location bar over
    // Toronto's recorded afternoon: 26 °C, high 28, low 16, on a day Reykjavik
    // reached 16.
    //
    // That is not a silent default, it is a mislabelled forecast, and it is
    // worse than it sounds because the output is a file — the warning would
    // scroll past in a terminal while the picture goes into a bug report or a
    // README.
    //
    // So an implied fixture yields to a named place: rule 3 and rule 2 both
    // check `--place` and step aside, saying so on stderr, and the run goes
    // live. Rule 1 does not, because `--fixture <name> --place <query>` is two
    // explicit and incompatible instructions — that combination is rejected in
    // parseCommandLine rather than silently resolved either way.
    //
    // `--grab` with no `--place` is untouched, which is the property every
    // committed screenshot depends on.
    QString fixture() const;

    QString     grab()        const { return m_grab; }
    bool        capturing()   const { return !m_grab.isEmpty() || !m_film.isEmpty(); }
    bool        hasSize()     const { return m_sizeWidth > 0 && m_sizeHeight > 0; }
    int         sizeWidth()   const { return m_sizeWidth; }
    int         sizeHeight()  const { return m_sizeHeight; }
    QString     viewport()    const { return m_viewport; }
    QString     film()        const { return m_film; }
    int         frames()      const { return m_frames; }
    int         every()       const { return m_every; }
    QStringList pokes()       const { return m_pokes; }
    QString     place()       const { return m_place; }
    QString     tab()         const { return m_tab; }
    QString     sky()         const { return m_sky; }
    QString     scheme()      const { return m_scheme; }
    QString     metric()      const { return m_metric; }
    int         day()         const { return m_day; }
    bool        list()        const { return m_list; }
    qreal       scroll()      const { return m_scroll; }

private:
    // Private, and that is not tidiness — it is what makes create() run.
    //
    // QQmlPrivate::singletonConstructionMode() tests is_default_constructible
    // *before* it looks for a factory, so a QML_SINGLETON that can be
    // default-constructed is default-constructed and create() is never called.
    // The symptom is perfect: the type registers, the properties resolve, every
    // binding evaluates, and QML reads a second instance holding nothing but
    // defaults while C++ reads the parsed one. Nothing warns. Taking the
    // default constructor away is what makes the two the same object.
    AppOptions();

    QString     m_grab;
    int         m_sizeWidth   = 0;
    int         m_sizeHeight  = 0;
    QString     m_viewport;
    QString     m_film;
    // The two filming defaults are the ones the QML Timer carried: eight
    // frames, one every 60 ms. They live here now because --frames and --every
    // are only ever a way of overriding them.
    int         m_frames      = 8;
    int         m_every       = 60;
    QStringList m_pokes;
    QString     m_fixture;
    QString     m_place;
    QString     m_tab;
    QString     m_sky;
    QString     m_scheme;
    QString     m_metric;
    // -1 rather than 0, because 0 is a day. Every "unset" below is a value the
    // flag could not legally produce, so QML can test for it without a second
    // has-it boolean.
    int         m_day         = -1;
    bool        m_list        = false;
    qreal       m_scroll      = -1;
};
