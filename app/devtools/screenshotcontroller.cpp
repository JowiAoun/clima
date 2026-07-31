// SPDX-FileCopyrightText: 2026 Jowi Aoun
// SPDX-License-Identifier: GPL-3.0-or-later

#include "devtools/screenshotcontroller.h"

#include "appoptions.h"

#include <QCoreApplication>
#include <QPauseAnimation>
#include <QQuickItem>
#include <QQuickItemGrabResult>
#include <QQuickWindow>

namespace {

// Every delay in this file, with the reason it is what it is. They were tuned
// against the real scenes and every one of them is the difference between a
// golden image and a lottery ticket.
namespace when {

// Long enough for the details grid's staggered reveal to finish: the last card
// waits a stagger-per-card into the wave and then takes a full reveal of its
// own. Grab before that lands and every golden image catches a different card
// mid-sweep.
constexpr int grab = 1600;

// After first paint, before anything a person would call "settled". Both the
// walk and the scroll need a laid-out scene rather than a settled one —
// contentHeight is still 0 during componentComplete, and a contentY assigned
// against it is clamped straight back to zero.
constexpr int placement = 400;

// A poke without a film is a poke whose result gets grabbed at rest, so it has
// to land early enough for the motion it causes to finish before `grab` above.
constexpr int poke = 500;

// Filming starts later than everything else on purpose: frame 00 has to be a
// genuinely quiet "before", so the mount motion of whatever is on screen must
// be over before the first shutter.
constexpr int film = 900;

} // namespace when

} // namespace

ScreenshotController::ScreenshotController(QObject *parent)
    : QObject(parent)
{
}

void ScreenshotController::setWindow(QQuickWindow *window)
{
    if (m_window == window)
        return;
    m_window = window;
    Q_EMIT windowChanged();
}

void ScreenshotController::setShell(QQuickItem *shell)
{
    if (m_shell == shell)
        return;
    m_shell = shell;
    Q_EMIT shellChanged();
}

void ScreenshotController::setGallery(QQuickItem *gallery)
{
    if (m_gallery == gallery)
        return;
    m_gallery = gallery;
    Q_EMIT galleryChanged();
}

void ScreenshotController::setMobile(bool mobile)
{
    if (m_mobile == mobile)
        return;
    m_mobile = mobile;
    Q_EMIT mobileChanged();
}

void ScreenshotController::after(int milliseconds, void (ScreenshotController::*slot)())
{
    // DeleteWhenStopped, so each of these cleans itself up after it fires. The
    // connection to `slot` is made first and therefore runs first: a queued
    // deletion cannot outrun a direct signal emission.
    QPauseAnimation *pause = new QPauseAnimation(milliseconds, this);
    connect(pause, &QAbstractAnimation::finished, this, slot);
    pause->start(QAbstractAnimation::DeleteWhenStopped);
}

bool ScreenshotController::offer(QQuickItem *item, const char *property, const QVariant &value)
{
    if (item == nullptr)
        return false;
    // indexOfProperty rather than property().isValid(): a QML property that
    // currently holds undefined reads back as an invalid QVariant, and "the
    // property is empty" is not "the property does not exist".
    if (item->metaObject()->indexOfProperty(property) < 0)
        return false;
    return item->setProperty(property, value);
}

void ScreenshotController::start()
{
    const AppOptions *options = AppOptions::instance();

#ifdef CLIMA_DEV_TOOLS
    // Order matters here in exactly one way and it is worth naming: the state
    // flags go on before the capture timers start, so a --grab of a --metric is
    // a grab of the metric and not of the moment before it.
    applyOpeningState();

    if (options->gallery() && options->walk() > 0) {
        m_walkSteps = options->walk();
        after(when::placement, &ScreenshotController::onWalk);
    }

    if (options->scroll() >= 0)
        after(when::placement, &ScreenshotController::onScroll);

    if (!options->film().isEmpty())
        after(when::film, &ScreenshotController::startFilming);
    else if (!options->pokes().isEmpty())
        // --poke without --film: apply once the scene has settled, so a plain
        // --grab captures a poked *resting* state rather than a transition.
        after(when::poke, &ScreenshotController::applyPokes);
#endif

    if (options->grab().isEmpty())
        return;

    // The precipitation field is the one thing on this page that moves without
    // being asked, so a grab of it would otherwise catch a different frame
    // every run and no two golden images would agree. Frozen, it still draws
    // rain — precip.js seeds every drop from its hour, so the frozen frame is a
    // deterministic one rather than an empty one.
    //
    // Offered rather than assigned, which is MobileShell.push()'s rule and is
    // here for the same reason: under --gallery there is no shell at all.
    offer(m_shell, "animated", false);

    m_grabFile = options->grab();
    after(when::grab, &ScreenshotController::onGrab);
}

void ScreenshotController::grabTo(const QString &file, bool quitWhenSaved)
{
    if (m_window == nullptr) {
        qWarning("grab: no window to photograph");
        QCoreApplication::quit();
        return;
    }

    // contentItem, not the window: grabToImage() renders an item tree into an
    // FBO and never sees QQuickWindow::color, so grabbing the window instead
    // would put whatever the clear colour happens to be behind the page.
    const QSharedPointer<QQuickItemGrabResult> result = m_window->contentItem()->grabToImage();
    if (result.isNull()) {
        qWarning("grab: grabToImage refused");
        QCoreApplication::quit();
        return;
    }

    // The result is kept alive by this lambda and by nothing else — it is a
    // shared pointer whose only other owner was the local above. Capture it by
    // value or the render thread finishes into a deleted object.
    connect(result.data(), &QQuickItemGrabResult::ready, this,
            [this, result, file, quitWhenSaved] {
                if (!result->saveToFile(file))
                    qWarning("grab: could not write %s", qPrintable(file));
                else if (quitWhenSaved)
                    qInfo("grab: wrote %s", qPrintable(file));

#ifdef CLIMA_DEV_TOOLS
                if (!quitWhenSaved) {
                    // Filming: the run ends when the last frame is on disk, not
                    // when the last shutter fires. Saves are asynchronous and
                    // quitting on the shutter loses the tail of the reel.
                    ++m_filmSaved;
                    if (m_filmSaved >= AppOptions::instance()->frames())
                        QCoreApplication::quit();
                    return;
                }
#endif
                QCoreApplication::quit();
            });
}

void ScreenshotController::onGrab()
{
    grabTo(m_grabFile, true);
}

#ifdef CLIMA_DEV_TOOLS

void ScreenshotController::applyOpeningState()
{
    const AppOptions *options = AppOptions::instance();

    // --tab is not here: it is the shell's own opening state rather than
    // something done to a running shell, so Main.qml hands it to MobileShell at
    // construction. Assigning it from out here would rebuild the page that has
    // just finished laying itself out.
    if (!options->metric().isEmpty())
        offer(m_shell, "metricId", options->metric());
    if (options->day() >= 0)
        offer(m_shell, "dayIndex", options->day());
    if (options->list())
        offer(m_shell, "listView", true);
}

void ScreenshotController::onWalk()
{
    if (m_gallery == nullptr)
        return;
    for (int i = 0; i < m_walkSteps; ++i)
        QMetaObject::invokeMethod(m_gallery, "step", Q_ARG(QVariant, QVariant(1)));
}

void ScreenshotController::onScroll()
{
    if (m_shell == nullptr)
        return;
    const qreal target = AppOptions::instance()->scroll();
    const qreal limit  = m_shell->property("maxContentY").toReal();
    offer(m_shell, "contentY", qMin(target, limit));
}

void ScreenshotController::applyPokes()
{
    const QStringList pokes = AppOptions::instance()->pokes();
    for (const QString &poke : pokes) {
        const qsizetype split = poke.indexOf(QLatin1Char('='));
        const QString target  = poke.left(split);
        const QString value   = poke.mid(split + 1);
        const bool on         = value == QLatin1String("true") || value == QLatin1String("1");

        // Every target below except `remount` lives on the shell, and under
        // --gallery there is no shell. Warning beats throwing: a poke that
        // cannot land should say so, not abort the rest of the list.
        if (target != QLatin1String("remount") && m_shell == nullptr) {
            qWarning("--poke %s: no shell is running", qPrintable(target));
            continue;
        }

        if (target == QLatin1String("metric")) {
            offer(m_shell, "metricId", value);
        } else if (target == QLatin1String("day")) {
            offer(m_shell, "dayIndex", value.toInt());
        } else if (target == QLatin1String("list")) {
            offer(m_shell, "listView", on);
        } else if (target == QLatin1String("feels")) {
            offer(m_shell, "feelsLike", on);
        } else if (target == QLatin1String("scroll")) {
            offer(m_shell, "contentY", value.toDouble());
        } else if (target == QLatin1String("tab")) {
            // Only the mobile shell has tabs. On the desktop this is the one
            // poke that is genuinely meaningless rather than merely ignored, so
            // it says so.
            if (m_mobile)
                offer(m_shell, "tab", value);
            else
                qWarning("--poke tab: only the mobile shell has tabs");
        } else if (target == QLatin1String("flick")) {
            // Negative velocity carries the content upward, i.e. scrolls down.
            bool ok = false;
            const double velocity = value.toDouble(&ok);
            const double amount   = (!ok || velocity == 0) ? 1400.0 : velocity;
            QMetaObject::invokeMethod(m_shell, "flickBy",
                                      Q_ARG(QVariant, QVariant(-qAbs(amount))));
        } else if (target == QLatin1String("remount")) {
            // Rebuilding the specimen replays whatever the component does on
            // mount, which for a detail card is the only animation it has — the
            // data behind these cards never changes while the app runs.
            if (m_gallery != nullptr)
                QMetaObject::invokeMethod(m_gallery, "remount");
            else
                qWarning("--poke remount: only meaningful with --gallery");
        } else {
            qWarning("--poke: unknown target %s", qPrintable(target));
        }
    }
}

void ScreenshotController::startFilming()
{
    // Looping rather than restarted, which is how a QML Timer with repeat:true
    // behaves: the loop boundary is the tick, so the interval cannot drift by
    // however long the previous tick's work took.
    m_filmTicker = new QPauseAnimation(AppOptions::instance()->every(), this);
    m_filmTicker->setLoopCount(-1);
    connect(m_filmTicker, &QAbstractAnimation::currentLoopChanged, this,
            &ScreenshotController::onFilmTick);
    m_filmTicker->start();
}

void ScreenshotController::onFilmTick()
{
    const AppOptions *options = AppOptions::instance();

    // On the second tick, so frame 00 is a settled "before" that has definitely
    // rendered and frame 01 is the first moment of change.
    if (m_filmShot == 1)
        applyPokes();

    if (m_filmShot >= options->frames()) {
        // Stop the ticker but not the process: the saves are still in flight,
        // and the last one to land is what quits.
        m_filmTicker->stop();
        return;
    }

    const int index = m_filmShot++;
    const QString name = QStringLiteral("%1-%2.png")
                             .arg(options->film(), QString::number(index).rightJustified(2, u'0'));
    grabTo(name, false);
}

#endif // CLIMA_DEV_TOOLS
