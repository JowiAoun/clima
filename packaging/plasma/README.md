<!-- SPDX-FileCopyrightText: 2026 Jowi Aoun -->
<!-- SPDX-License-Identifier: CC-BY-SA-4.0 -->

# KDE, wlroots, and why there is no applet in this directory

There is no plasmoid here and there is not going to be one. `clima-widget` pins itself to the
desktop on Plasma 6 without an applet, without a plugin, and without a single file installed into
`~/.local/share/plasma/`. The same binary GNOME's shell extension spawns asks KWin for a
desktop-layer surface and gets one.

```sh
clima-widget --widget current-conditions --widget hourly-strip --pin on
```

`scripts/check-layer-shell.sh` is the proof, and it runs in CI.

## Why not an applet

The plan for this work called a Plasma 6 applet **the cheapest of the three desktop
integrations**, on the grounds that "a Plasma applet *is* QML and the widget files go into
`contents/ui/` nearly verbatim". That is wrong, and the reason is not visible until you try it.

A plasmoid is a package of loose `.qml` files that plasmashell loads from disk. Files in
`contents/ui/` resolve each other as types, and a `qmldir` there can even declare a singleton — so
`Theme.qml`, `theme.js`, `chartmath.js`, `WeatherGlyph.qml` and every other **pure-QML** file this
project has really could be copied in and would really work.

The tiles are not pure QML. Every one of them reads:

| Type | What it is | Where it comes from |
|---|---|---|
| `WidgetFeed` | the subscription — place, field mask, snapshot, staleness | `widgets/widgetfeed.h` |
| `DaemonLink` | the session-bus connection, one per process | `widgets/daemonlink.h` |
| `Wx` | the WMO tables and the published bands, null-safe | `widgets/wx.h` |
| `Units` | the reader's unit preference, out of the app's own INI | `app/viewmodels/units.h` |

Those are C++. A plasmoid cannot import a C++ type that is not installed as a QML plugin on the
system import path, and none of ours is: `Clima.Widgets` is a **static** module compiled into
`clima-widget`, exactly like `Clima` is compiled into `clima`. So a plasmoid would need either a
second implementation of the data path in pure QML — and Plasma 6 ships no generic D-Bus binding
for QML, so that path does not exist either — or `Clima.Widgets` has to become a shared, installed
QML plugin, which costs a shared library, an RPATH and three packaging formats that have to place
two more files correctly.

`zwlr_layer_shell_v1` costs none of that. It is a Wayland protocol for exactly this: a client says
"put my surface on the desktop layer, anchored top-right, 24 px in, and give it no keyboard focus"
and the compositor does it. KWin implements it. So does every wlroots compositor — Sway, Hyprland,
Wayfire, river, labwc. One protocol, one binary, one implementation of every tile.

## What that looks like

```
--pin auto     ask, and accept an ordinary window if the compositor cannot (the default)
--pin on       refuse to start rather than put an unpinned window on somebody's desktop
--pin off      never ask

--anchor       top-left | top | top-right | left | center | right
               | bottom-left | bottom | bottom-right           (default top-right)
--margin       pixels from the screen edge                     (default 24)
--layer        background | bottom | top | overlay             (default bottom)
```

`bottom` is what a desktop widget is: above the wallpaper, below every ordinary window. It reserves
no space, so nothing anybody maximises changes shape, and it takes no keyboard focus, so the first
keystroke after login still goes where it was going.

`--pin on` exists for the case where nobody is watching. An autostart entry or a compositor
`exec` line that asks for a pinned tile and silently gets a floating window in the middle of the
screen is the failure this project keeps refusing to ship, so that spelling exits non-zero and
says which of the four reasons applied.

On GNOME, `--pin auto` finds no `zwlr_layer_shell_v1`, says so, and the tiles come up as an
ordinary window — the shell extension in `packaging/gnome-shell/` is what pins them there, and it
has to be, because mutter implements no such protocol and extensions.gnome.org forbids shipping
binaries. Two mechanisms, one set of tiles.

## How it was measured

`packaging/plasma/README.md` used to end by saying layer-shell was the better answer and was not
built, for one reason: mutter does not implement it, so on the machine this project is developed on
the code could have been compiled and never once run. `docs/widgets.md` exists because the GNOME
adoption mechanism was measured before anything was built on it, and shipping an unmeasured second
mechanism would have been the opposite of that.

The measurement is `WLR_BACKENDS=headless sway`, which stands up a wlroots compositor with a
virtual output, a software renderer, no GPU and no seat. `scripts/check-layer-shell.sh` drives it
and asserts six things:

| | |
|---|---|
| **pinned** | sway logs a layer surface with namespace `clima-widgets` on layer 1 |
| **placed** | the tiles are in the anchored corner and the opposite corner is empty — measured off a `grim` photograph, because the anchor and margins are sent *after* the surface exists and appear in no log line |
| **not a window** | `swaymsg -t get_tree` does not know about it: no alt-tab, no tiling, nothing reflows when it appears |
| **falsifiable** | the same binary with `--pin off` **is** in that tree and logs no layer surface |
| **refuses** | `--pin on` where a layer surface is impossible exits 3 and says why |
| **survives a monitor going away** | unplug the output the surface lives on and the tiles come back on another one |

The last one is the reason `widgets/layershell.cpp` has a `Remap` in it, and it is the one that
only a runtime could have found. A layer surface belongs to an output; destroy that output and the
compositor dismisses the surface and layer-shell-qt closes the window, which for a host whose only
window that is means the process exits — a two-screen desktop would lose its tiles the first time
somebody undocked a laptop. What makes the recovery subtle is that the obvious fix does not work:
`QWindow::close()` hides the window without necessarily destroying the platform window underneath,
so `show()` maps nothing, reports success, and produces a process that believes it is displaying
tiles nobody can see. It has to be `destroy()` and then `show()`.

## What has not been done

**None of this has run on KWin.** It has run on wlroots, which is the reference implementation of
the protocol and is what KWin was written against, and the surface this creates uses nothing
outside `zwlr_layer_shell_v1` version 1. That is a strong argument and it is not a measurement, and
the difference is the point of this section. What would close it: `clima-widget --pin on` on a
Plasma 6 session, and a line here saying so.

**There is no autostart entry that pins.** `packaging/linux/clima-widget.desktop.in` is a launcher:
it appears in the application menu and starting it from there gets pinned tiles, because `--pin
auto` is the default. Making the tiles appear at every login is a decision for the person whose
desktop it is, so it is a copy into `~/.config/autostart/` and not something a package does on
their behalf.
