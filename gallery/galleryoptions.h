// SPDX-FileCopyrightText: 2026 Jowi Aoun
// SPDX-License-Identifier: GPL-3.0-or-later
//
// The gallery's command line, parsed once, offered to QML as a singleton.
//
// The weather app's AppOptions and this are deliberately two classes rather
// than one with a mode flag. They overlap — both take --grab, --size, --film —
// but they disagree about more than they share: `--viewport mobile` resizes the
// app's window and does *not* resize this one, `--tab hourly` is meaningless
// here, and `--card Uv` is meaningless there. A single parser covering both
// would have to say "ignored unless" in half its --help entries, which is how a
// --help stops being read.
//
// What is shared is shared properly: AppOptions::viewportIds() and
// AppOptions::skyPhases() are called from here rather than transcribed. Two
// binaries in one repository that disagree about whether `tablet` is a viewport
// is a bug nobody can reproduce.
//
// ---- what it does not decide ------------------------------------------------
// Nothing here knows what a viewport preset is worth in pixels, which sky phase
// paints which gradient, or what components exist. Viewports.qml, Theme.qml and
// gallery.js own those, and a component name is passed through unexamined so
// that `--gallery weather glyph` can be a substring match against the catalogue
// at the moment the catalogue is loaded — which is the only moment anything
// knows what is in it.
#pragma once

#include <QObject>
#include <QQmlEngine>
#include <QString>
#include <QStringList>

class QCoreApplication;

class GalleryOptions : public QObject
{
    Q_OBJECT
    QML_ELEMENT
    QML_SINGLETON

    // ---- capture -----------------------------------------------------------
    Q_PROPERTY(QString     grab   READ grab   CONSTANT)
    Q_PROPERTY(QString     film   READ film   CONSTANT)
    Q_PROPERTY(int         frames READ frames CONSTANT)
    Q_PROPERTY(int         every  READ every  CONSTANT)
    Q_PROPERTY(QStringList pokes  READ pokes  CONSTANT)
    Q_PROPERTY(int         walk   READ walk   CONSTANT)

    // ---- the window --------------------------------------------------------
    Q_PROPERTY(bool hasSize    READ hasSize    CONSTANT)
    Q_PROPERTY(int  sizeWidth  READ sizeWidth  CONSTANT)
    Q_PROPERTY(int  sizeHeight READ sizeHeight CONSTANT)

    // ---- what is on the stage ----------------------------------------------
    // `viewport` is the device frame a specimen is staged in, not a window size.
    // That is the one place this parser and the app's mean genuinely different
    // things by the same word, and it is not an accident: pinning the *window*
    // to 390 px would leave 158 px of stage beside a 232 px rail.
    Q_PROPERTY(QString viewport READ viewport CONSTANT)
    Q_PROPERTY(QString sky      READ sky      CONSTANT)
    Q_PROPERTY(QString pick     READ pick     CONSTANT)

    // ---- the two other ways of looking at one component --------------------
    Q_PROPERTY(QString card    READ card    CONSTANT)
    Q_PROPERTY(bool    details READ details CONSTANT)

public:
    // Parses argv into the process-wide instance. Call from main() before the
    // QML engine loads anything: the window reads these at construction to size
    // itself, and a window sized after its content has laid out is a window
    // whose content laid out at the wrong width.
    //
    // Does not return on --help, --version or a malformed value.
    static void parseCommandLine(const QCoreApplication &app);

    static GalleryOptions *instance();

    // QML_SINGLETON's factory. See the long note in appoptions.h for why the
    // constructor below has to be private for this to be called at all.
    static GalleryOptions *create(QQmlEngine *qmlEngine, QJSEngine *jsEngine);

    QString     grab()       const { return m_grab; }
    QString     film()       const { return m_film; }
    int         frames()     const { return m_frames; }
    int         every()      const { return m_every; }
    QStringList pokes()      const { return m_pokes; }
    int         walk()       const { return m_walk; }
    bool        hasSize()    const { return m_sizeWidth > 0 && m_sizeHeight > 0; }
    int         sizeWidth()  const { return m_sizeWidth; }
    int         sizeHeight() const { return m_sizeHeight; }
    QString     viewport()   const { return m_viewport; }
    QString     sky()        const { return m_sky; }
    QString     pick()       const { return m_pick; }
    QString     card()       const { return m_card; }
    bool        details()    const { return m_details; }

private:
    GalleryOptions();

    QString     m_grab;
    QString     m_film;
    int         m_frames     = 8;
    int         m_every      = 60;
    QStringList m_pokes;
    int         m_walk       = 0;
    int         m_sizeWidth  = 0;
    int         m_sizeHeight = 0;
    QString     m_viewport;
    QString     m_sky;
    QString     m_pick;
    QString     m_card;
    bool        m_details    = false;
};
