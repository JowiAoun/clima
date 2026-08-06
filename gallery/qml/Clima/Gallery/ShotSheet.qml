// SPDX-FileCopyrightText: 2026 Jowi Aoun
// SPDX-License-Identifier: GPL-3.0-or-later
//
// A composed image: one or more devices, side by side, on the app's own page
// gradient — the thing `--grab` writes for the README.
//
// The composition happens here rather than in an image tool, and that is the
// point of the file. A hero image assembled from three PNGs in Node or ffmpeg
// is a fourth description of what a phone and a tablet look like, maintained
// by hand, and it goes stale the first time a breakpoint moves. This one is
// built out of Viewports and the real shells, so `scripts/shots.sh` regenerates
// it from the current code and the golden suite photographs it — which means a
// README image that no longer matches the app fails CI instead of appearing on
// the front page.
pragma ComponentBehavior: Bound

import QtQuick
import Clima
import "shots.js" as Shots

Item {
    id: root

    property string shot: "hero"

    readonly property var sheet: Shots.byId(root.shot)
    readonly property var devices: root.sheet !== null ? root.sheet.devices : []
    readonly property real zoom: root.sheet !== null ? root.sheet.zoom : 1.0
    readonly property string skyPhase: root.sheet !== null ? root.sheet.sky : "dusk"

    // Room around the devices, and between them. Generous rather than tight:
    // these are cropped by nothing and viewed at whatever width a README column
    // happens to be, so the frames need air to read as separate objects.
    readonly property real pad: 48
    readonly property real gap: 44

    property real bezel: 14

    // ---- the sheet sizes itself ---------------------------------------------
    //
    // Not circular, though it looks it: the window binds its size to these, and
    // these are computed from Viewports and the shot definition — neither of
    // which knows anything about the window. That is what lets shots.js carry
    // no pixel dimensions at all.
    function frameWidth(id) {
        var p = Viewports.byId(id);
        return ((p !== null ? p.w : 390) + root.bezel * 2) * root.zoom;
    }

    function frameHeight(id) {
        var p = Viewports.byId(id);
        return ((p !== null ? p.h : 844) + root.bezel * 2) * root.zoom;
    }

    readonly property real rowHeight: {
        var tallest = 0;
        for (var i = 0; i < root.devices.length; ++i)
            tallest = Math.max(tallest, root.frameHeight(root.devices[i]));
        return tallest;
    }

    implicitWidth: {
        var total = root.pad * 2;
        for (var i = 0; i < root.devices.length; ++i)
            total += root.frameWidth(root.devices[i]);
        return total + root.gap * Math.max(0, root.devices.length - 1);
    }

    implicitHeight: root.rowHeight + root.pad * 2

    // An Item does not take its implicit size, so say so. The window binds to
    // these, and a sheet left at 0x0 would produce a correctly sized window
    // containing nothing.
    width: implicitWidth
    height: implicitHeight

    // The sheet's own background, which is the same page gradient the app uses
    // and not a neutral grey. A product shot on a colour the product never
    // shows is a shot of somebody's slide deck.
    PageBackdrop {
        anchors.fill: parent
        phase: root.skyPhase

        // No stars behind the devices even at night. The frames are the
        // subject; a star field between them competes with the one inside them
        // and the eye reads two skies rather than three screens.
        stars: false
    }

    Row {
        anchors.centerIn: parent
        spacing: root.gap

        Repeater {
            model: root.devices

            // A wrapper of uniform height so the devices sit on one baseline.
            // Row manages x and leaves y alone, so bottom alignment has to come
            // from an anchor inside a cell rather than from the positioner —
            // and a tablet is 268 px taller than a desktop window, so without
            // this they float at three different heights.
            delegate: Item {
                id: cell

                required property string modelData

                // The preset's declared class, not a measurement of its width.
                // That distinction is the whole reason `tablet-landscape`
                // exists: at 1112 px a width test answers "desktop" and the
                // nav rail never appears. Same pin the app applies on a
                // handheld.
                readonly property string cls: Viewports.classFor(cell.modelData)

                width: root.frameWidth(cell.modelData)
                height: root.rowHeight

                DeviceFrame {
                    anchors.bottom: parent.bottom
                    viewport: cell.modelData
                    zoom: root.zoom
                    bezel: root.bezel
                    skyPhase: root.skyPhase

                    // The real thing, chosen the way the app chooses it — by
                    // asking Viewports which shell the class uses, not by
                    // matching on the preset's name. A new preset gets the
                    // right shell for free; a name match would need editing.
                    Loader {
                        anchors.fill: parent
                        sourceComponent:
                            Viewports.usesMobileShell(cell.cls) ? mobileShell : desktopShell
                    }
                }

                // Declared inside the delegate rather than beside the Row, and
                // that is not a style choice. `pragma ComponentBehavior: Bound`
                // makes a Component capture the context it is *declared* in, so
                // one shared at the root cannot see which cell asked for it —
                // and the hero has a phone and a tablet that both want
                // MobileShell with two different classes.
                Component {
                    id: desktopShell
                    WeatherPage { }
                }

                Component {
                    id: mobileShell
                    MobileShell { viewportClass: cell.cls }
                }
            }
        }
    }
}
