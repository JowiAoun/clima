// SPDX-FileCopyrightText: 2026 Jowi Aoun
// SPDX-License-Identifier: GPL-3.0-or-later
//
// The gallery's window, and the three ways of looking at a piece of the app
// without the rest of the app in the way: the component library, one detail
// card, or the details grid.
//
// This was DevPreviews.qml — an Item inside the weather app's window that
// switched itself on when one of three flags was passed. It is a window of its
// own now, which is why the second half of the file reads as the second half of
// app/qml/Clima/Main.qml: the backdrop, the sky rule, the geometry and the
// shutters are the same window logic, because a component previewed on the
// wrong background is a preview of the wrong thing.
//
// What deliberately did not come with it is the Settings restore. The app
// remembers where you left its window; this must not, in either direction. A
// golden image whose size depends on where a developer last dragged a tool
// window is not a golden image, and a tool that quietly rewrites the size the
// reader gets when they next open the product is worse than that.
//
// `import Clima` is where everything below that is not a Loader comes from:
// Theme, Viewports, Clock, PageBackdrop, WeatherDetails and ScreenshotController
// are the app's, resolved through the engine's import machinery rather than
// through any path — which is what makes them the app's and not a copy.
import QtQuick
import QtQuick.Window
import Clima

Window {
    id: win

    visible: true
    width: 1340
    height: 762

    // The app's floors, not a tighter pair of the gallery's own. The rail alone
    // is 232 px, so this window is useless at 360 — but `--card Uv --size
    // 390x844` is a perfectly reasonable thing to ask for, and a minimum that
    // silently clamps a --size is a screenshot at a size nobody chose.
    minimumWidth: 360
    minimumHeight: 480
    color: Theme.page.bg

    title: qsTr("Clima — components")

    // --card and --details each show one thing on its own; the catalogue is
    // what is left. The parser has already refused the two together, so this is
    // a switch and not a precedence rule.
    readonly property bool previewing: GalleryOptions.card !== "" || GalleryOptions.details

    // ---- the sky -------------------------------------------------------------
    // The same rule the app runs, off the same tables, because the whole premise
    // here is that a component is reviewed on the background it will be given. A
    // wide window is mostly rail and stage, so it stays at `dusk` like the
    // desktop page does; size this one to a phone and it gets the phone's sky.
    //
    // A *frame* on the stage decides its own sky separately, further down. That
    // one is a phone whatever this window is.
    readonly property bool mobile: Viewports.usesMobileShell(Viewports.classOf(win.width))

    readonly property string skyPhase:
        GalleryOptions.sky !== "" ? GalleryOptions.sky
                                  : (win.mobile ? Clock.skyPhase : "dusk")

    // Painted as an item rather than left to Window.color: grabToImage()
    // captures contentItem, which does not include the window's clear colour, so
    // a headless grab of a Window.color background comes out black.
    PageBackdrop {
        anchors.fill: parent
        phase: win.skyPhase
        stars: win.mobile || GalleryOptions.sky !== ""
    }

    // `--card Uv` renders one detail card on the page gradient and nothing else,
    // so a card can be built and checked without the rest of the screen in the
    // way.
    //
    // A Specimen and not the Loader this was, which is the same fix the
    // catalogue got: both instantiate a component from a name, and only one of
    // them draws a labelled red box when the name is wrong. `--card Ub` used to
    // answer with a console warning and an empty window, and an empty window is
    // exactly what a card that failed to reveal looks like.
    Specimen {
        anchors.centerIn: parent
        visible: GalleryOptions.card !== ""
        file: GalleryOptions.card !== "" ? "Detail" + GalleryOptions.card + "Card.qml" : ""
    }

    // The grid lays out in full and does not scroll itself — the page owns that.
    // Shown on its own it still needs somewhere to scroll and something to bound
    // its cards' Shapes, so the preview supplies both.
    Loader {
        active: GalleryOptions.details
        anchors.fill: parent
        anchors.margins: 22
        sourceComponent: Component {
            Flickable {
                clip: true
                layer.enabled: true
                contentWidth: width
                contentHeight: gridItem.height
                flickableDirection: Flickable.VerticalFlick
                boundsBehavior: Flickable.StopAtBounds

                WeatherDetails {
                    id: gridItem
                    width: parent.width
                }
            }
        }
    }

    // The gallery gets the whole window with no margin: it paints its own rail
    // to the edge, the way a tool window should.
    Loader {
        id: galleryLoader
        active: !win.previewing
        anchors.fill: parent
        sourceComponent: Component {
            Gallery {
                pick: GalleryOptions.pick

                // The device frame a specimen is staged in, not a window size.
                // Resizing this window to 390 px would leave 158 px of stage
                // beside a 232 px rail; the stage frames the specimen at the
                // preset instead and the window keeps its own size.
                viewport: GalleryOptions.viewport

                // Whatever the app would be showing at this hour, so a component
                // framed as a phone is reviewed on the sky the phone would
                // actually give it — stars included. Not win.skyPhase, which is
                // `dusk` on a window this wide.
                skyPhase: GalleryOptions.sky !== "" ? GalleryOptions.sky : Clock.skyPhase
            }
        }
    }

    // The shutters, out of the Clima module — the same class the app drives,
    // with its inputs bound to this parser instead of that one. `shell` is left
    // null because there is no shell here, which is a case every poke already
    // knows how to report.
    //
    // `as Item` because Loader.item is declared QObject and qmllint will not
    // narrow it for you. Without the assertion this is an incompatible-type
    // warning on a line that is perfectly correct at runtime, and a lint that
    // warns about correct code is a lint people stop reading.
    ScreenshotController {
        id: capture
        window: win
        gallery: galleryLoader.item as Item

        grab:   GalleryOptions.grab
        film:   GalleryOptions.film
        frames: GalleryOptions.frames
        every:  GalleryOptions.every

        pokes:  GalleryOptions.pokes
        walk:   GalleryOptions.walk
    }

    Component.onCompleted: {
        // Same rule as the app, and first for the same reason: the scheme
        // decides what everything below paints with, and a still capture holds
        // still so the palette page compares byte for byte between runs.
        if (GalleryOptions.scheme !== "")
            Theme.scheme = GalleryOptions.scheme
        if (GalleryOptions.grab !== "" && GalleryOptions.film === "")
            Theme.stillness = true

        if (GalleryOptions.hasSize) {
            win.width = GalleryOptions.sizeWidth
            win.height = GalleryOptions.sizeHeight
        } else if (!win.previewing) {
            // Room for the rail plus a stage, but only when nobody said
            // otherwise — hence Math.max and not an assignment. The two preview
            // modes stay at the default, because one card alone on a gradient
            // does not need 1500 px of it.
            win.width = Math.max(win.width, 1500)
            win.height = Math.max(win.height, 950)
        }

        // Last, after every line above that can change the window's width: the
        // width chooses the sky, and a shutter opened before the geometry has
        // settled photographs the moment before.
        capture.start()
    }
}
