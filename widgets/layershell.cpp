// SPDX-FileCopyrightText: 2026 Jowi Aoun
// SPDX-License-Identifier: GPL-3.0-or-later

#include "layershell.h"

#include <QElapsedTimer>
#include <QGuiApplication>
#include <QScreen>
#include <QTimer>
#include <QWindow>

#ifdef CLIMA_HAS_LAYER_SHELL
#include <LayerShellQt/Window>
#include <wayland-client.h>
#endif

namespace clima::widgets::layershell {

namespace {

// The nine places a tile can sit, and the four edges each one anchors to.
//
// Anchoring to two adjacent edges keeps the surface its own size and puts it
// in that corner; anchoring to none lets the compositor centre it. Anchoring
// to two *opposite* edges would stretch the surface across the screen, which
// is what a panel wants and never what a tile wants — so no name here produces
// that combination.
struct AnchorSpec
{
    const char *name;
    int         edges; // LayerShellQt::Window::Anchors, spelled as ints so this
                       // table compiles in a build without layer-shell-qt.
};

constexpr int kTop    = 1;
constexpr int kBottom = 2;
constexpr int kLeft   = 4;
constexpr int kRight  = 8;

constexpr AnchorSpec kAnchors[] = {
    {"top-left", kTop | kLeft},         {"top", kTop},
    {"top-right", kTop | kRight},       {"left", kLeft},
    {"center", 0},                      {"right", kRight},
    {"bottom-left", kBottom | kLeft},   {"bottom", kBottom},
    {"bottom-right", kBottom | kRight},
};

// The four layers of the protocol, lowest first.
//
// `bottom` is the default and it is the one that means "desktop widget": above
// the wallpaper, below every ordinary window. `background` is where the
// wallpaper itself lives — a tile there is stacked against swaybg by creation
// order, which is a race — and `top`/`overlay` sit above windows, which is a
// panel or a notification, not a tile that is meant to be got out of the way
// by opening anything.
constexpr const char *kLayers[] = {"background", "bottom", "top", "overlay"};

int anchorEdges(const QString &name)
{
    for (const AnchorSpec &spec : kAnchors) {
        if (name == QLatin1String(spec.name))
            return spec.edges;
    }
    return kTop | kRight;
}

int layerIndex(const QString &name)
{
    for (int i = 0; i < int(std::size(kLayers)); ++i) {
        if (name == QLatin1String(kLayers[i]))
            return i;
    }
    return 1; // bottom
}

#ifdef CLIMA_HAS_LAYER_SHELL

void registryGlobal(void *data, wl_registry *, uint32_t, const char *interface, uint32_t)
{
    if (qstrcmp(interface, "zwlr_layer_shell_v1") == 0)
        *static_cast<bool *>(data) = true;
}

void registryGlobalRemove(void *, wl_registry *, uint32_t)
{
}

// Does the compositor on the other end implement the protocol?
//
// One registry roundtrip on a second connection, thrown away immediately. Qt
// will not answer this: QtWaylandClient's globals are private API, and asking
// layer-shell-qt costs a *window* — LayerShellQt::Window::get() only discovers
// the protocol is missing at the moment it tries to swap the shell
// integration, by which point the window exists and the only report is a
// warning on a logging category.
//
// Which matters because of what `--pin on` has to be able to do: fail. An
// autostart entry that asks for a pinned tile and silently gets a floating
// window is the failure this project keeps refusing to ship.
bool compositorHasLayerShell()
{
    wl_display *display = wl_display_connect(nullptr);
    if (display == nullptr)
        return false;

    bool         found    = false;
    wl_registry *registry = wl_display_get_registry(display);

    static const wl_registry_listener listener = {registryGlobal, registryGlobalRemove};
    wl_registry_add_listener(registry, &listener, &found);
    wl_display_roundtrip(display);

    wl_registry_destroy(registry);
    wl_display_disconnect(display);
    return found;
}

// ---- surviving a monitor going away -----------------------------------------
//
// A layer surface belongs to an output. Unplug that monitor and the compositor
// sends `zwlr_layer_surface_v1.closed`; layer-shell-qt turns that into
// QWindow::close(), and since this is the host's only window the process would
// then quit — a desktop with two screens would lose its tiles for good the
// first time somebody undocked a laptop.
//
// A dismissed surface cannot be reused, so the recovery is to close and open
// again, which is what makes a new one. That is `show()` on a screen that still
// exists.
//
// Bounded, because a compositor that dismisses us the instant we appear would
// otherwise turn this into a spin: a second dismissal within two seconds of the
// last remap is taken as "there is nowhere to put this" and the process exits
// rather than fighting for it.
class Remap : public QObject
{
public:
    explicit Remap(QWindow *window)
        : QObject(window)
        , m_window(window)
    {
        // Otherwise the close that a dismissal produces takes the process down
        // before the remap below can run — QGuiApplication quits on the last
        // window closing, and that quit is posted from inside the close. The
        // host now decides for itself when it is finished, which is when it has
        // given up, immediately below.
        QGuiApplication::setQuitOnLastWindowClosed(false);

        m_since.start();
        connect(window, &QWindow::visibleChanged, this, &Remap::onVisibleChanged);
    }

private:
    void onVisibleChanged(bool visible)
    {
        if (visible || m_stopping)
            return;

        if (m_remapped && m_since.elapsed() < 2000) {
            qWarning("clima-widget: the compositor dismissed the tiles twice in "
                     "two seconds — giving up rather than spinning");
            m_stopping = true;
            QCoreApplication::quit();
            return;
        }

        if (QGuiApplication::primaryScreen() == nullptr) {
            qWarning("clima-widget: no screens left to put the tiles on");
            m_stopping = true;
            QCoreApplication::quit();
            return;
        }

        m_remapped = true;
        m_since.restart();

        // Deferred, for two reasons. This runs inside QWindow::close(), called
        // from layer-shell-qt's `closed` handler, and destroying the platform
        // window from inside that unwinds the stack it is standing on. And the
        // compositor is still taking the old output apart; the new surface
        // wants to be asked for after it has finished.
        QTimer::singleShot(0, this, &Remap::remap);
    }

    void remap()
    {
        if (m_stopping)
            return;

        QScreen *screen = QGuiApplication::primaryScreen();
        if (screen == nullptr) {
            m_stopping = true;
            QCoreApplication::quit();
            return;
        }

        // ---- destroy(), and this is the whole of the fix ---------------------
        //
        // QWindow::close() hides the window; it does not necessarily take the
        // platform window down with it. Qt then still holds a QWaylandWindow
        // whose wl_surface the compositor destroyed when it dismissed us, and
        // `show()` on that maps nothing — measured: visibleChanged went false
        // and then true again, the window reported itself visible, and the
        // compositor logged no second layer surface at all. Nothing failed;
        // there were simply no tiles.
        //
        // destroy() drops it, so show() has to build a new one — and building a
        // new one is what fires the QPlatformSurfaceEvent that layer-shell-qt's
        // event filter is waiting for to swap the shell integration back in.
        m_window->destroy();
        m_window->setScreen(screen);
        m_window->show();
    }

    QWindow      *m_window;
    QElapsedTimer m_since;
    bool          m_remapped = false;
    bool          m_stopping = false;
};

#endif // CLIMA_HAS_LAYER_SHELL

QString computeUnavailableReason()
{
#ifndef CLIMA_HAS_LAYER_SHELL
    return QStringLiteral(
        "this build was configured without layer-shell-qt, so it cannot ask a "
        "compositor for a desktop-layer surface");
#else
    const QString platform = QGuiApplication::platformName();
    if (platform != QLatin1String("wayland")) {
        return QStringLiteral(
                   "the Qt platform plugin is \"%1\", not \"wayland\" — "
                   "zwlr_layer_shell_v1 is a Wayland protocol and has no X11 equivalent")
            .arg(platform);
    }

    // ---- the footgun with our own name on it --------------------------------
    //
    // wl_display_connect(nullptr) reads WAYLAND_SOCKET first, and on success it
    // takes ownership of that file descriptor and *unsets the variable*. That
    // is exactly the fd a GNOME Shell extension hands us: extension identity on
    // Wayland is an inherited socket from a socketpair the shell made, and it
    // is the whole reason clima-widget is spawned rather than D-Bus activated
    // (docs/widgets.md, finding 2).
    //
    // So probing here would consume the handshake, Qt would find no socket to
    // connect to, and the tiles would never appear — a bug that reproduces only
    // under the shell that spawns us, which is the hardest place to see it.
    //
    // It is also the right answer semantically. A shell that spawned us to
    // adopt our window is going to place that window itself; asking the
    // compositor to place it as well is two authorities on one surface.
    if (qEnvironmentVariableIsSet("WAYLAND_SOCKET")) {
        return QStringLiteral(
            "this process was spawned with an inherited Wayland socket, so the "
            "shell that started it is placing the window itself");
    }

    if (!compositorHasLayerShell()) {
        return QStringLiteral(
            "this compositor does not implement zwlr_layer_shell_v1 — GNOME's "
            "mutter is the one that does not, and there the GNOME Shell "
            "extension in packaging/gnome-shell/ is what pins the tiles");
    }

    return {};
#endif
}

} // namespace

QStringList anchorNames()
{
    QStringList names;
    names.reserve(int(std::size(kAnchors)));
    for (const AnchorSpec &spec : kAnchors)
        names << QLatin1String(spec.name);
    return names;
}

QStringList layerNames()
{
    QStringList names;
    names.reserve(int(std::size(kLayers)));
    for (const char *name : kLayers)
        names << QLatin1String(name);
    return names;
}

QString unavailableReason()
{
    // Computed once. The probe opens a socket and does a roundtrip, and both
    // `--pin auto` and a `--pin on` failure message want the answer.
    static const QString reason = computeUnavailableReason();
    return reason;
}

bool pin([[maybe_unused]] QWindow *window, [[maybe_unused]] const Placement &placement)
{
    if (window == nullptr || !unavailableReason().isEmpty())
        return false;

#ifdef CLIMA_HAS_LAYER_SHELL
    using LayerShellQt::Window;

    Window *surface = Window::get(window);
    if (surface == nullptr)
        return false;

    surface->setLayer(Window::Layer(layerIndex(placement.layer)));
    surface->setAnchors(Window::Anchors(anchorEdges(placement.anchor)));

    const int m = placement.margin;
    surface->setMargins(QMargins(m, m, m, m));

    // Reserve nothing. A panel sets a positive zone and the compositor keeps
    // that strip clear of windows for it; a tile is decoration and must not
    // take a bite out of everybody's maximised geometry. Zero rather than -1
    // because -1 additionally means "ignore everyone else's exclusive zone",
    // which would put a top-anchored tile underneath the panel.
    surface->setExclusiveZone(0);

    // A tile has nothing to type into, and a desktop-layer surface that takes
    // focus on appearing would steal the first keystroke after login.
    surface->setKeyboardInteractivity(Window::KeyboardInteractivityNone);
    surface->setActivateOnShow(false);

    // What the compositor calls this surface. Sway prints it in the log line
    // scripts/check-layer-shell.sh asserts on, KWin matches window rules
    // against it, and a user writing their own rule needs a name that does not
    // change between releases.
    surface->setScope(QStringLiteral("clima-widgets"));

    // Parented to the window, so it lives exactly as long as the thing it is
    // watching. `new` without a delete is not a leak here for the same reason
    // it is not one anywhere else in Qt.
    new Remap(window);

    return true;
#else
    return false;
#endif
}

} // namespace clima::widgets::layershell
