// SPDX-FileCopyrightText: 2026 Jowi Aoun
// SPDX-License-Identifier: GPL-3.0-or-later
//
// The window, and deliberately almost nothing else.
//
// This file used to be five hundred lines, and four hundred of them were an
// argv scraper and five capture timers. Those are C++ now — AppOptions parses
// the command line, Settings remembers what should outlive the process, and
// ScreenshotController owns the shutters. What is left is the product: a
// backdrop, a shell, and the two rules that choose the shell.
import QtQuick
import QtQuick.Window
import "sky.js" as Sky
import "detaildata.js" as Detail

Window {
    id: win

    visible: true
    width: 1340
    height: 762

    // A phone is 390 px wide, so the old 680 floor would have made the layout
    // this window exists to show unreachable by dragging. The floor now only
    // has to keep the narrowest screen readable.
    minimumWidth: 360
    minimumHeight: 480
    color: Theme.color.pageBg
    title: qsTr("Clima")

    // ---- which shell -------------------------------------------------------
    // The window's width decides, and Viewports owns the thresholds so the
    // gallery frames a specimen at exactly the width the app would switch at.
    // `--viewport mobile` pins it and resizes to the preset, which is what a
    // headless grab needs: a shell chosen from a width that a screenshot flag
    // then changes is a shell chosen from the wrong width.
    property string forcedViewport: ""
    readonly property string viewportClass:
        forcedViewport !== "" ? forcedViewport : Viewports.classOf(win.width)
    readonly property bool mobile: Viewports.usesMobileShell(viewportClass)

    // ---- the sky -----------------------------------------------------------
    // The phone's background follows the clock: a deep blue by day, a starred
    // indigo at night, warmed at the two crossings. The desktop stays at
    // `dusk`, because a 1340 px window is mostly cards — the background is a
    // rim around them, and the reader never sees enough of it for a
    // constellation to be anything but noise behind a chart. A phone is the
    // opposite: the hero sits directly on the sky with no card at all.
    //
    // detaildata.sun is the clock, because it is the one that carries minutes,
    // and dawn and dusk are the seventy minutes either side of a crossing.
    readonly property string skyPhase:
        AppOptions.sky !== "" ? AppOptions.sky
      : (win.mobile ? Sky.phaseAt(Detail.sun.nowMin, Detail.sun.riseMin, Detail.sun.setMin)
                    : "dusk")

    // The page background is painted as an item, not left to Window.color.
    // grabToImage() captures contentItem, which does not include the window's
    // clear colour — so every headless screenshot came out with a black page
    // behind the cards, which is not what is on screen.
    PageBackdrop {
        anchors.fill: parent
        phase: win.skyPhase
        stars: win.mobile || AppOptions.sky !== ""
    }

    // --card, --details and --gallery. Inactive unless one of them was passed.
    DevPreviews {
        id: previews
        anchors.fill: parent
    }

    // The app itself. Two shells, one product: `WeatherPage` is the desktop's
    // single scrolling column, `MobileShell` is the phone's five tabs under a
    // nav bar, and which one runs is a function of the window width and nothing
    // else — there is no "mobile build".
    //
    // A Loader rather than both in the tree with one hidden. Keeping both would
    // build five phone screens on every desktop launch to show none of them.
    Loader {
        id: shellLoader
        anchors.fill: parent
        visible: !previews.active
        active: visible
        sourceComponent: win.mobile ? mobileShell : desktopShell
    }

    Component {
        id: desktopShell
        WeatherPage { }
    }

    Component {
        id: mobileShell
        // Assigned on completion rather than bound through the constructor:
        // MobileShell's default tab is mobiletabs.js's first entry, and naming
        // that entry a second time out here is how the two come to disagree.
        MobileShell {
            Component.onCompleted: if (AppOptions.tab !== "") tab = AppOptions.tab
        }
    }

    // The shutters. Everything it needs is bound, so it still points at the
    // right shell after `--viewport mobile` has swapped one for the other.
    ScreenshotController {
        id: capture
        window: win
        shell: shellLoader.item as Item
        gallery: previews.gallery
        mobile: win.mobile
    }

    // ---- window geometry ---------------------------------------------------
    // Four sources, and the order between them is the whole design:
    //
    //   1. a remembered size, weakest, and only when nothing else spoke
    //   2. --viewport's preset
    //   3. --size, which overrides the preset so `--viewport mobile --size
    //      390x1600` grabs a tall phone rather than fighting over the window
    //   4. the gallery's floor, which asks rather than insists
    //
    // A capture restores nothing at all. A golden image whose size depends on
    // where the developer last left the window is not a golden image, and the
    // symmetry matters as much as the rule: a run that did not restore does not
    // save either, so `--grab` and `--gallery` cannot quietly rewrite the size
    // the reader will get next time they open the app.
    readonly property bool geometryFromFlags:
        AppOptions.hasSize || AppOptions.viewport !== "" || AppOptions.gallery
    readonly property bool geometryRemembered:
        !AppOptions.capturing && !win.geometryFromFlags

    Component.onCompleted: {
        if (win.geometryRemembered && Settings.hasWindowSize) {
            // Size only. Position is stored, and restoring it is a lie on
            // Wayland — the compositor places windows, and a client that
            // assigns x/y there is assigning to nothing.
            win.width = Settings.windowWidth
            win.height = Settings.windowHeight
        }

        var preset = AppOptions.viewport !== "" ? Viewports.byId(AppOptions.viewport) : null
        if (preset !== null && !AppOptions.gallery) {
            win.forcedViewport = preset.id
            win.width = preset.w
            win.height = preset.h
        } else if (preset === null && AppOptions.viewport !== "") {
            // The parser accepted an id Viewports has never heard of, which
            // means the two lists have drifted. Say so here rather than opening
            // a desktop window and letting the flag look inert.
            console.warn("viewport:", AppOptions.viewport, "is not in Viewports.presets")
        }

        if (AppOptions.hasSize) {
            win.width = AppOptions.sizeWidth
            win.height = AppOptions.sizeHeight
        }

        if (AppOptions.gallery && !AppOptions.hasSize) {
            // Room for the rail plus a stage, but only when nobody said
            // otherwise — hence Math.max and not an assignment.
            win.width = Math.max(win.width, 1500)
            win.height = Math.max(win.height, 950)
        }

        // Last, and that is the point: every flag above can change which shell
        // is loaded, and a state flag applied to a shell that is about to be
        // destroyed looks exactly like a flag that does nothing.
        capture.start()
    }

    Component.onDestruction:
        if (win.geometryRemembered)
            Settings.saveWindowGeometry(win.x, win.y, win.width, win.height)
}
