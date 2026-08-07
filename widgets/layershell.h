// SPDX-FileCopyrightText: 2026 Jowi Aoun
// SPDX-License-Identifier: GPL-3.0-or-later
//
// Pinning the tiles to the desktop without a shell extension.
//
// ============================================================================
// WHAT THIS IS INSTEAD OF
//
// GNOME needs 600 lines of GJS to put a window on the desktop, because mutter
// exposes no protocol for it: an extension has to spawn our process, adopt the
// window it makes, re-type it as a dock and lower it. That is the DING pattern,
// it is measured in docs/widgets.md, and it is the only way in on that shell.
//
// Everywhere else there is a protocol for exactly this. `zwlr_layer_shell_v1`
// lets a client say "put my surface on the desktop layer, anchored top-right,
// 24 px in, and do not give it keyboard focus" — and the compositor does it.
// KWin implements it, and so does every wlroots compositor: Sway, Hyprland,
// Wayfire, river, labwc. One protocol, one binary, no applet and no plugin.
//
// So packaging/plasma/README.md has no plasmoid in it. A plasmoid would be a
// second implementation of every tile, because a plasmoid cannot import the
// C++ types the tiles are built on; this is the same tiles, in the same
// process the GNOME extension spawns, asking the compositor directly.
//
// ============================================================================
// THE THREE ORDERING RULES THAT ARE NOT OPTIONAL
//
//   1. `LayerShellQt::Window::get(w)` must run BEFORE the window is shown.
//      It installs an event filter and swaps the shell integration when the
//      platform surface appears; a window that already has a surface has
//      already committed to xdg-shell. layer-shell-qt warns and carries on, so
//      getting this wrong produces an ordinary window and no error — which is
//      why widgets/qml/Clima/Widgets/WidgetWindow.qml is `visible: false` and
//      widgets/main.cpp does the showing.
//
//   2. The layer, anchors and margins must be set on that object before the
//      surface is committed, because `get_layer_surface` takes the layer and
//      the scope as arguments and the rest is sent in the same commit.
//
//   3. The availability probe must not run when WAYLAND_SOCKET is set. See
//      layershell.cpp — it is a footgun with our own name on it.
//
// ============================================================================
// OPTIONAL AT CONFIGURE TIME
//
// widgets/CMakeLists.txt compiles this whole file to stubs when
// layer-shell-qt is not installed, and `--pin` then reports that as its
// reason. A packager on a GNOME-only distribution is entitled to leave it out;
// what they are not entitled to is a widget host that silently does nothing.

#pragma once

#include <QString>
#include <QStringList>

class QWindow;

namespace clima::widgets::layershell {

// Where on the screen, and how far in. Both are strings as typed on the
// command line: parsed here so that `--anchor` validates identically in a
// build that has layer-shell-qt and one that does not. An option that only
// rejects a typo on some machines is worse than one that never does.
struct Placement
{
    QString anchor = QStringLiteral("top-right");
    QString layer  = QStringLiteral("bottom");
    int     margin = 24;
};

// The accepted values, in the order `--help` should list them.
[[nodiscard]] QStringList anchorNames();
[[nodiscard]] QStringList layerNames();

// Empty when this process can create a desktop-layer surface. Otherwise one
// sentence saying why it cannot, written to be printed at a user.
//
// Answered once and remembered: it opens a second Wayland connection to ask
// the compositor what it implements.
[[nodiscard]] QString unavailableReason();

// Turn `window` into a layer surface. Must be called before the window is
// shown; returns false, and leaves the window an ordinary one, when
// unavailableReason() is not empty.
bool pin(QWindow *window, const Placement &placement);

} // namespace clima::widgets::layershell
