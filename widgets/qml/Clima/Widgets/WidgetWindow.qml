// SPDX-FileCopyrightText: 2026 Jowi Aoun
// SPDX-License-Identifier: GPL-3.0-or-later
//
// One window, every tile the reader asked for.
//
// ============================================================================
// WHY ONE WINDOW
//
// Because a GNOME Shell extension adopts *a window*, and because six tiles in
// one process is about 95 MB against six processes at about 280 MB. The
// extension spawns this once, adopts what appears, types it as a dock and pins
// it to the bottom of the stack — the DING pattern, measured on a real shell in
// docs/widgets.md.
//
// ============================================================================
// THE BACKGROUND IS NOT PAINTED
//
// `color: "transparent"` and nothing behind the tiles, because what is behind
// them is somebody's wallpaper. This is the one surface in the product that
// does not own its own background, which is also why WidgetSurface keeps a
// hairline border in both schemes rather than only in light mode: contrast
// against the page defines a card in the app, and here there is no page.
//
// ============================================================================
// SIZING
//
// The window is exactly as large as its tiles. The extension reads that and
// places it; a user who resizes gets tiles that stretch, because every tile
// anchors rather than assuming its catalogue size. `--windowed` is for
// developing one, and puts an ordinary decorated frame around it.

import QtQuick

Window {
    id: win

    // Shown by widgets/main.cpp, and this is the one property in the file that
    // is load-bearing rather than cosmetic.
    //
    // `--pin` asks the compositor for a desktop-layer surface, and that request
    // has to be made before the window has a platform surface at all. A
    // `visible: true` here creates one during component completion, which is
    // before main() gets the chance — and the failure is silent: the tiles
    // appear as an ordinary window, in the middle of the screen, above
    // everything, and nothing says why. See widgets/layershell.h.
    visible: false
    title: qsTr("Clima widgets")

    color: "transparent"

    flags: WidgetOptions.windowed ? Qt.Window
                                  : (Qt.Window | Qt.FramelessWindowHint | Qt.NoDropShadowWindowHint)

    width: tiles.implicitWidth + tiles.padding * 2
    height: tiles.implicitHeight + tiles.padding * 2

    // ---- the theme ---------------------------------------------------------
    //
    // The same three Bindings app/qml/Clima/Main.qml carries, and they are
    // Bindings rather than assignments for the same reason: the desktop can
    // change its mind while this is running, and a widget that stayed dark
    // through sunrise would be the only thing on the screen that did.

    Binding {
        target: Theme
        property: "scheme"
        value: WidgetOptions.scheme !== "" ? WidgetOptions.scheme
                                           : SystemAppearance.colorScheme
    }

    // A tile draws no sky, so there is no star field to thin out — but the
    // token is read by the components this module shares with the app, and
    // leaving it at "full" would be a claim rather than a default.
    Binding {
        target: Theme
        property: "perfTier"
        value: "reduced"
    }

    // Stillness, and here it is closer to a rule than a preference. A tile
    // sits on a wallpaper for eight hours; the only motion it is allowed is a
    // transition that ends. `--still` and `--grab` pin it, and so does a reader
    // who asked their desktop for less movement.
    Binding {
        target: Theme
        property: "stillness"
        value: WidgetOptions.still || SystemAppearance.reduceMotion
    }

    // ---- the tiles ---------------------------------------------------------

    Grid {
        id: tiles

        readonly property int padding: 8

        x: tiles.padding
        y: tiles.padding
        columns: WidgetOptions.columns
        spacing: 8

        Repeater {
            model: WidgetOptions.ids

            WidgetTile {
                required property string modelData
                widgetId: modelData
                place: WidgetOptions.place

                // The Loader is sized by what it loaded, which is the tile's
                // catalogue size. Without these the Loader is 0x0 and the Grid
                // stacks ten invisible tiles at the origin.
                width: item ? item.implicitWidth : 0
                height: item ? item.implicitHeight : 0
            }
        }
    }

    // ---- the shutter -------------------------------------------------------
    //
    // The app's own ScreenshotController, compiled into this module as well.
    // It is what makes a tile reviewable in CI: `--snapshot` feeds it recorded
    // data, `--grab` photographs it, and neither needs a session bus or a
    // network.

    ScreenshotController {
        id: shutter
        window: win
        grab: WidgetOptions.grab
        film: WidgetOptions.film
        frames: WidgetOptions.frames
        every: WidgetOptions.every
    }

    Component.onCompleted: shutter.start()
}
