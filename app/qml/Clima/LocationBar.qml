// SPDX-FileCopyrightText: 2026 Jowi Aoun
// SPDX-License-Identifier: GPL-3.0-or-later
// The place the page is about: name, a disclosure chevron, a home marker.
//
// It sits directly on the page gradient, not on a surface. Wrapping it in its
// own wash would put a fourth surface immediately above the hero card with
// nothing between them, and two washes that close together read as one badly
// aligned panel rather than as two things.
import QtQuick
import QtQuick.Shapes

Item {
    id: root

    property string label: Detail.location.label
    property bool isHome: Detail.location.isHome

    // Whether the picker this bar opens is currently up. Bound from outside —
    // the picker is a sheet over the whole page and the bar is inside it, so
    // the bar cannot own it — and read here for one thing only: which way the
    // chevron points.
    property bool disclosed: false

    signal changeRequested()
    signal homeToggled()

    implicitWidth: bar.width
    implicitHeight: 26
    height: implicitHeight

    Row {
        id: bar
        spacing: 10
        anchors.verticalCenter: parent.verticalCenter

        Text {
            id: placeName
            text: root.label
            color: Theme.ink.primary
            font.pixelSize: Theme.type.sectionTitle
            anchors.verticalCenter: parent.verticalCenter
        }

        // ---- disclosure chevron ------------------------------------------
        // It rotates now, and for most of this prototype's life it deliberately
        // did not: `changeRequested()` went nowhere, and a chevron that turns
        // over and reveals nothing is an animation making a promise the app
        // cannot keep.
        //
        // The promise is kept. The signal opens PlacePicker — search, saved
        // places, use my location — so the chevron is doing what a disclosure
        // chevron means: pointing at the thing it opened, and pointing back
        // when it closes. `disclosed` is bound from whichever page owns the
        // sheet, so the arrow is a readout of the picker's state rather than a
        // toggle of its own, and it cannot end up upside down over a closed
        // panel.
        //
        // `Theme.motion.move` and not `view`: the chevron is one small object
        // travelling, not a screenful arriving.
        Item {
            width: 14
            height: 14
            anchors.verticalCenter: parent.verticalCenter

            Shape {
                anchors.fill: parent
                rotation: root.disclosed ? 180 : 0
                Behavior on rotation {
                    NumberAnimation { duration: Theme.motion.move; easing.type: Easing.OutCubic }
                }
                preferredRendererType: Shape.CurveRenderer
                ShapePath {
                    strokeColor: chevronHover.hovered ? Theme.ink.primary
                                                      : Theme.ink.muted
                    // Hover is colour only (§10.6), and the stroke was the one
                    // hover on this bar that snapped while the ring beside it
                    // faded.
                    Behavior on strokeColor {
                        ColorAnimation { duration: Theme.motion.tint; easing.type: Easing.OutCubic }
                    }
                    strokeWidth: 1.6
                    fillColor: "transparent"
                    capStyle: ShapePath.RoundCap
                    joinStyle: ShapePath.RoundJoin
                    PathSvg { path: "M 2.5 5.5 L 7 10 L 11.5 5.5" }
                }
            }

            HoverHandler { id: chevronHover; cursorShape: Qt.PointingHandCursor }
            TapHandler { onTapped: root.changeRequested() }
        }

        // ---- home marker --------------------------------------------------
        // A ring rather than a filled plate: a filled circle here would be the
        // only opaque thing on the page.
        Item {
            width: 24
            height: 24
            anchors.verticalCenter: parent.verticalCenter

            Rectangle {
                anchors.fill: parent
                radius: width / 2
                color: homeHover.hovered ? Theme.surface.raised : "transparent"
                border.width: 1
                border.color: root.isHome ? Theme.line.control : Theme.line.grid
                Behavior on color {
                    ColorAnimation { duration: Theme.motion.tint; easing.type: Easing.OutCubic }
                }
                // The marker is a toggle — `homeToggled()` is its whole reason
                // for having a tap target — so the ring must not snap between
                // states while the wash behind it fades.
                Behavior on border.color {
                    ColorAnimation { duration: Theme.motion.tint; easing.type: Easing.OutCubic }
                }
            }

            Shape {
                anchors.centerIn: parent
                width: 14
                height: 14
                preferredRendererType: Shape.CurveRenderer
                ShapePath {
                    fillColor: root.isHome ? Theme.ink.primary : Theme.ink.muted
                    Behavior on fillColor {
                        ColorAnimation { duration: Theme.motion.tint; easing.type: Easing.OutCubic }
                    }
                    strokeColor: "transparent"
                    PathSvg {
                        path: "M 7 2.4 L 12.8 7.6 L 11.3 7.6 L 11.3 12.4 "
                            + "L 8.4 12.4 L 8.4 9.3 L 5.6 9.3 L 5.6 12.4 "
                            + "L 2.7 12.4 L 2.7 7.6 L 1.2 7.6 Z"
                    }
                }
            }

            HoverHandler { id: homeHover; cursorShape: Qt.PointingHandCursor }
            TapHandler { onTapped: root.homeToggled() }
        }
    }
}
