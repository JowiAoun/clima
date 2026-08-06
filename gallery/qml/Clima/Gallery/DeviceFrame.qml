// SPDX-FileCopyrightText: 2026 Jowi Aoun
// SPDX-License-Identifier: GPL-3.0-or-later
//
// One device, drawn around a real screen.
//
// The screen inside is not a picture of the app — it is the app: a MobileShell
// or a WeatherPage built from the same module the product ships, laid out at
// the exact width `Viewports` says that device is. So a README image cannot
// show a layout the app does not produce, which is the failure mode every
// hand-composited screenshot eventually has.
//
// The bezel is deliberately plain. A photographic handset mock-up with a
// reflection and a notch dates in about a year and tells the reader something
// about a phone rather than about this app; a flat rounded rim reads as "a
// screen" at any size and never looks like last year's device.
pragma ComponentBehavior: Bound

import QtQuick
import Clima

Item {
    id: root

    // A preset id out of Viewports — "mobile", "tablet", "tablet-landscape",
    // "desktop". The screen takes its size from there rather than from
    // arguments, so a frame and the app agree about what a tablet is by
    // construction.
    property string viewport: "mobile"

    // The sky behind the content. A frame paints its own, because the whole
    // point is that the device looks like the device: a phone at night is a
    // phone at night regardless of what the sheet around it is doing.
    property string skyPhase: "dusk"
    property bool stars: true

    // What goes on the screen.
    default property alias screen: screenArea.data

    readonly property var preset: Viewports.byId(root.viewport)
    readonly property int screenWidth: root.preset !== null ? root.preset.w : 390
    readonly property int screenHeight: root.preset !== null ? root.preset.h : 844

    // The rim, in screen pixels before zoom.
    property real bezel: 14

    // ---- zoom, and why it is not `scale` ------------------------------------
    //
    // `Item.scale` is a render-time transform: it changes what is drawn and
    // leaves width and height alone. Put three scaled frames in a Row and the
    // Row spaces them by their unscaled widths, so a half-size hero comes out
    // with the devices overlapping and a hole on the right.
    //
    // So the zoom lives in the implicit size — the frame really is smaller —
    // and an inner item of natural size carries the transform. Layout and
    // drawing then agree, and the content inside still lays out at the device's
    // true pixel width, which is the entire point of framing it at all.
    property real zoom: 1.0

    readonly property real naturalWidth: root.screenWidth + root.bezel * 2
    readonly property real naturalHeight: root.screenHeight + root.bezel * 2

    implicitWidth: root.naturalWidth * root.zoom
    implicitHeight: root.naturalHeight * root.zoom
    width: implicitWidth
    height: implicitHeight

    // A phone is rounder than a monitor, and getting that backwards is most of
    // what makes a mock-up look wrong. Keyed off the shell the preset uses
    // rather than off its id, so a new preset inherits the right answer.
    readonly property real screenRadius:
        Viewports.usesMobileShell(root.preset !== null ? root.preset.cls : "") ? 26 : 10

    readonly property color rim: "#0a0d18"

    Item {
        width: root.naturalWidth
        height: root.naturalHeight
        transformOrigin: Item.TopLeft
        transform: Scale { xScale: root.zoom; yScale: root.zoom }

        // The body. Darker than any surface in the palette on purpose — a bezel
        // that lands inside the app's own surface ladder reads as another card.
        Rectangle {
            anchors.fill: parent
            radius: root.screenRadius + root.bezel * 0.7
            color: root.rim

            // The one border in this repository that docs/10-design-system.md
            // §10.1 does not ban. That rule is about junctions inside a page;
            // this is the outline of a physical object against whatever the
            // sheet behind it happens to be, and without it a dark frame on a
            // dark sheet has no edge at all.
            border.width: 1
            border.color: "#26304d"
        }

        // The screen. `clip` rather than an opacity mask: the content is a live
        // shell with a Flickable in it, and a mask would force the whole thing
        // through an offscreen texture every frame.
        Item {
            id: screenArea
            x: root.bezel
            y: root.bezel
            width: root.screenWidth
            height: root.screenHeight
            clip: true

            // Behind the shell, because neither WeatherPage nor MobileShell
            // paints its own sky — app/qml/Clima/Main.qml puts a PageBackdrop
            // under the Loader for exactly this reason, and a frame is that
            // window.
            PageBackdrop {
                anchors.fill: parent
                phase: root.skyPhase
                stars: root.stars
            }
        }

        // The rounded corners of the screen, painted back over the content.
        //
        // `clip` above is rectangular — Qt Quick clips to a bounding box, not
        // to a Rectangle's radius — so the shell's square corners otherwise
        // poke into the rim.
        //
        // One rectangle the size of the whole frame, filled with nothing and
        // outlined with a border exactly as thick as the bezel. A border
        // follows its rectangle's radius, so its *inner* edge is a rounded rect
        // inset by `bezel` with radius `screenRadius` — the screen's outline,
        // to the pixel. The rest of the border lands on bezel that is already
        // this colour, so the only thing it changes is the four corners.
        Rectangle {
            anchors.fill: parent
            color: "transparent"
            radius: root.screenRadius + root.bezel
            border.width: root.bezel
            border.color: root.rim
        }

        // And the rim's own hairline again, because the border above ran over
        // it at the corners.
        Rectangle {
            anchors.fill: parent
            color: "transparent"
            radius: root.screenRadius + root.bezel * 0.7
            border.width: 1
            border.color: "#26304d"
        }
    }
}
