// SPDX-FileCopyrightText: 2026 Jowi Aoun
// SPDX-License-Identifier: GPL-3.0-or-later
//
// The capture harness: --grab, --film, --poke, --walk and --scroll, and the
// four timers that make them land at a moment worth photographing.
//
// All of this used to be Timer elements and a hundred lines of
// Component.onCompleted in Main.qml. It is here because none of it is the app.
// Main.qml is a window with a backdrop and a shell in it; everything below is
// scaffolding held up beside that window so a person or a CI job can take a
// picture of it, and mixing the two made the shortest file in the module the
// longest one.
//
// ---- why this compiles at all in a packaged build ---------------------------
// Only just. `--grab` ships — the issue template says "attach `clima --grab
// bug.png`", and a flag that only exists in a developer build cannot be in an
// issue template — so the code that implements --grab has to ship with it, and
// that code is here. Everything past it is behind CLIMA_DEV_TOOLS: the film
// timer, the poke table, the walk and the scroll are compiled out, and what a
// packaged binary carries is one QTimer and one grabToImage.
//
// The alternative was two files, one shipped and one not, differing by four
// members. That is a worse trade: the shipped half would still be a screenshot
// controller, and nobody would remember which of the two to put the next fix
// in.
//
// ---- why the timers are still timers ----------------------------------------
// A grab has to happen after the scene has settled, and "settled" is not a
// state Qt Quick will tell you about — there is no signal for "every animation
// this frame started has finished". The details grid staggers twelve cards into
// a reveal, each with its own delay; the numbers in the .cpp are longer than all
// of them put together, measured, and they are the reason two --grab runs of the
// same scene are byte-identical. Shortening them is how golden images start
// disagreeing about which card is mid-sweep.
//
// ---- why they are QPauseAnimation and not QTimer ----------------------------
// Because QML's Timer is. QQmlTimer is a QPauseAnimationJob underneath, so it
// runs on the animation driver: it fires on the first *frame* at or after its
// interval, and every animation a QML Timer starts therefore begins on a frame
// boundary. QTimer runs on the event loop and fires whenever the interval
// elapses, which is usually between two frames.
//
// That difference is invisible until you measure it. This port was written with
// QTimer first, and eight runs of `--grab --poke metric=uv --poke day=3 --poke
// list=true` produced three different PNGs — one or two levels apart across a
// fifty-pixel square, an animation caught a fraction of a frame further along
// because the gap between the poke and the shutter was no longer a whole number
// of frames.
//
// QPauseAnimation is the public spelling of what QQmlTimer uses, driven by the
// same QUnifiedTimer, and with it the two implementations agree. Not perfectly:
// that poke scene flakes about one run in twelve *in the QML prototype too*, to
// the same alternate image, so there is a real race in the poke path that
// predates all of this and belongs to whatever is still moving at 1600 ms. The
// point of matching clocks is that this file does not add a second one on top
// of it. The ten golden scenes are stable on both sides.
#pragma once

// QQuickItem and QQuickWindow are included rather than forward-declared: moc
// registers a pointer property's type as a metatype, and a metatype for a
// pointer to an incomplete type is a static_assert with three screens of
// template instantiation above it.
#include <QObject>
#include <QQmlEngine>
#include <QQuickItem>
#include <QQuickWindow>
#include <QString>
#include <QVariant>

class QPauseAnimation;

class ScreenshotController : public QObject
{
    Q_OBJECT
    QML_ELEMENT

    // What to photograph. The window rather than a bare item, because a grab
    // has to come off contentItem: grabToImage() renders an item tree and does
    // not include the window's clear colour, which is why PageBackdrop is an
    // item and not a Window.color in the first place.
    Q_PROPERTY(QQuickWindow *window READ window WRITE setWindow NOTIFY windowChanged)

    // Whichever shell is live, or null. Null is normal — under --gallery there
    // is no shell at all — and every use of it below says so rather than
    // throwing, because a poke that cannot land should report that and let the
    // rest of the list through.
    Q_PROPERTY(QQuickItem *shell READ shell WRITE setShell NOTIFY shellChanged)

    // The gallery, or null. Only --walk and `--poke remount` reach for it.
    Q_PROPERTY(QQuickItem *gallery READ gallery WRITE setGallery NOTIFY galleryChanged)

    // Whether the live shell is the phone's. `--poke tab` is the one poke that
    // is meaningless rather than merely ignored on the desktop, and this is how
    // it knows to say so.
    Q_PROPERTY(bool mobile READ mobile WRITE setMobile NOTIFY mobileChanged)

public:
    explicit ScreenshotController(QObject *parent = nullptr);

    // Called from Main.qml's Component.onCompleted, *after* the window geometry
    // has been settled — deliberately, and not from componentComplete() here.
    //
    // The ordering is load-bearing. `--viewport mobile` resizes the window,
    // which swaps the desktop shell for the phone's, which destroys the item
    // `shell` pointed at. Anything applied before that resize is applied to an
    // object that is about to be deleted, and the flag then looks like it does
    // nothing. So: geometry first, in QML where the viewport table lives, then
    // this.
    Q_INVOKABLE void start();

    QQuickWindow *window() const { return m_window; }
    void setWindow(QQuickWindow *window);
    QQuickItem *shell() const { return m_shell; }
    void setShell(QQuickItem *shell);
    QQuickItem *gallery() const { return m_gallery; }
    void setGallery(QQuickItem *gallery);
    bool mobile() const { return m_mobile; }
    void setMobile(bool mobile);

Q_SIGNALS:
    void windowChanged();
    void shellChanged();
    void galleryChanged();
    void mobileChanged();

private:
    // Grabs contentItem, writes it, quits. Quitting is the point as much as the
    // writing is: --grab is what a headless CI job runs, and a job that renders
    // a perfect PNG and then waits forever fails as a timeout rather than as a
    // failure.
    void grabTo(const QString &file, bool quitWhenSaved);

    // `page.foo = value` if the page has a `foo`, otherwise nothing. QML's rule
    // for assigning a property an object may not have is to throw, and a throw
    // in the middle of applying flags takes every later flag with it — which is
    // how three of the four capture paths once came to print one error and then
    // hang, never having started the timer that both writes the file and quits.
    bool offer(QQuickItem *item, const char *property, const QVariant &value);

    // "Call this once, `milliseconds` from now, on the animation clock." The
    // one-shot half of what a QML Timer does, and the only half anything here
    // needs except the film ticker.
    void after(int milliseconds, void (ScreenshotController::*slot)());

    void onGrab();

    QQuickWindow *m_window  = nullptr;
    QQuickItem   *m_shell   = nullptr;
    QQuickItem   *m_gallery = nullptr;
    bool          m_mobile  = false;
    QString       m_grabFile;

#ifdef CLIMA_DEV_TOOLS
    // The state flags — --metric, --day, --list — applied as soon as there is a
    // shell to apply them to.
    void applyOpeningState();

    // --poke target=value, applied either at 500 ms (a settled poked frame for
    // --grab) or on the second filmed frame (so frame 00 is a "before" and
    // everything after it is the change).
    void applyPokes();

    void onWalk();
    void onScroll();
    void startFilming();
    void onFilmTick();

    QPauseAnimation *m_filmTicker = nullptr;
    int m_walkSteps  = 0;
    int m_filmShot   = 0;
    int m_filmSaved  = 0;
#endif
};
