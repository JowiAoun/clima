<!-- SPDX-FileCopyrightText: 2026 Jowi Aoun -->
<!-- SPDX-License-Identifier: CC-BY-SA-4.0 -->

# Plasma, and why there is no applet in this directory

The plan for this work said a Plasma 6 applet would be **the cheapest of the three desktop
integrations**, on the grounds that "a Plasma applet *is* QML and the widget files go into
`contents/ui/` nearly verbatim". That turns out to be wrong, and the reason is worth writing down
because it is not visible until you try it.

## What a plasmoid can and cannot import

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
`clima-widget`, exactly like `Clima` is compiled into `clima`.

So a plasmoid would need either a second implementation of the data path in pure QML — and Plasma
6 ships no generic D-Bus binding for QML, so that path does not exist either — or the module has
to become a shared, installed QML plugin.

## The two real options, and their costs

**Install `Clima.Widgets` as a shared QML module.** `qt_add_qml_module(… SHARED)`, installed to
`$libdir/qt6/qml/Clima/Widgets`, with `clima-widget` linking it dynamically. Then a plasmoid is
about forty lines: `import Clima.Widgets` and one `WidgetTile`. This is the architecturally right
answer and it also gives every other QML consumer the same thing. It costs a shared library, an
RPATH, and three packaging formats that have to place two more files correctly.

**Use `layer-shell-qt`.** KWin implements `wlr-layer-shell`, and so does every wlroots compositor.
`LayerShellQt::Shell::useLayerShell()` plus a per-window layer, anchor and margin turns
`clima-widget` — *the same binary GNOME's extension spawns* — into a desktop-layer surface with no
applet, no plasmoid and no plugin at all. This is the better outcome: one process, one
implementation, and Sway and Hyprland get it for free.

The second is the one to build. It is not built here for a reason this repository takes seriously:
**mutter does not implement `wlr-layer-shell`**, so on the machine this work was done on the code
could be compiled and never once run. `docs/widgets.md` exists because the DING pattern was
measured before anything was built on it; shipping a layer-shell path that has never created a
layer surface would be the opposite of that.

The ordering constraints are the part that only runtime settles, and they are why "it compiles" is
not evidence here: `useLayerShell()` has to run before any window is created, and the layer, anchor
and margin have to be set before the surface is committed — which for a `QQmlApplicationEngine`
loading a `Window { visible: true }` means the QML cannot show itself.

## What a KDE user gets today

`clima-widget` runs. The tiles work, they read from `clima-daemon` over the session bus, they
follow the desktop's dark/light preference through the portal, and they keep their last reading
when the daemon goes away. What they do not do is pin themselves below the windows — they are an
ordinary window that the user places, and KWin's own window rules can make that behave (Keep
Below, No titlebar, all desktops) until the layer-shell path lands.

`packaging/linux/clima-widget.desktop.in` is the launcher entry that makes that one click, and
`packaging/linux/clima-daemon.desktop.in` autostarts the service it reads from.

```sh
clima-widget --widget current-conditions --widget hourly-strip --windowed
```
