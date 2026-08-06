// SPDX-FileCopyrightText: 2026 Jowi Aoun
// SPDX-License-Identifier: GPL-3.0-or-later
//
// One index on a 270° arc, for the two tiles that show a published index.
//
// ============================================================================
// WHY A SHARED COMPONENT RATHER THAN TWO DIALS
//
// UV and air quality are the same drawing with different numbers on it, and
// they are next to each other on a desktop. Two implementations would drift in
// exactly the way that is most visible: one arc a pixel thicker than the other,
// one gap at the bottom a few degrees wider. The *scales* stay separate,
// because those are two different authorities' tables and must never be
// unified — see libclima/domain/scales.h.
//
// ============================================================================
// AN UNKNOWN READING DRAWS THE TRACK AND NOTHING ELSE
//
// `fraction` is NaN when there is no reading, and then the coloured arc is not
// drawn at all — not drawn at zero. A UV dial pinned at the start of its arc
// says "the sun is not out"; an empty track says "we have no UV product here",
// which is the true statement for most of the world's air-quality coverage and
// for every place before the first fetch lands.

import QtQuick
import QtQuick.Shapes

Item {
    id: root

    // 0..1, or NaN for "no reading".
    property real fraction: NaN

    property color tint: Theme.accent.fill

    // The number, already formatted. A dash is a perfectly good value.
    property string reading: "–"

    // The band name, or whatever one line of context the tile wants.
    property string caption: ""

    // A second line, dimmer: today's maximum, the dominant pollutant.
    property string footnote: ""

    property real thickness: Math.max(5, Math.round(Math.min(width, height) * 0.07))

    // 270°, opening at the bottom. A full ring has no start and no end, so a
    // reading near either extreme is unreadable; the gap is what makes "nearly
    // empty" and "nearly full" different pictures.
    readonly property real startAngle: 135
    readonly property real sweep: 270

    readonly property real radius:
        Math.min(width, height) / 2 - root.thickness / 2 - 1

    Shape {
        anchors.fill: parent
        preferredRendererType: Shape.CurveRenderer

        ShapePath {
            strokeWidth: root.thickness
            strokeColor: Theme.line.track
            fillColor: "transparent"
            capStyle: ShapePath.RoundCap

            PathAngleArc {
                centerX: root.width / 2
                centerY: root.height / 2
                radiusX: root.radius
                radiusY: root.radius
                startAngle: root.startAngle
                sweepAngle: root.sweep
            }
        }

        ShapePath {
            strokeWidth: root.thickness
            strokeColor: root.tint
            fillColor: "transparent"
            capStyle: ShapePath.RoundCap

            PathAngleArc {
                centerX: root.width / 2
                centerY: root.height / 2
                radiusX: root.radius
                radiusY: root.radius
                startAngle: root.startAngle

                // Zero sweep for an unknown reading, which draws nothing. Not a
                // hidden ShapePath, because toggling `visible` on a Shape's
                // path rebuilds the geometry and the two dials are on screen
                // together.
                sweepAngle: isNaN(root.fraction)
                            ? 0
                            : root.sweep * Math.max(0, Math.min(1, root.fraction))

                Behavior on sweepAngle {
                    NumberAnimation {
                        duration: Theme.motion.reveal
                        easing.type: Theme.motion.easing
                    }
                }
            }
        }
    }

    Column {
        anchors.centerIn: parent
        spacing: -1

        Text {
            anchors.horizontalCenter: parent.horizontalCenter
            text: root.reading
            color: Theme.ink.primary
            font.pixelSize: Math.max(16, Math.round(root.height * 0.26))
            font.bold: true
        }

        Text {
            anchors.horizontalCenter: parent.horizontalCenter
            text: root.caption
            color: Theme.ink.muted
            font.pixelSize: Theme.type.axis
            visible: text !== ""
        }

        Text {
            anchors.horizontalCenter: parent.horizontalCenter
            text: root.footnote
            color: Theme.ink.dim
            font.pixelSize: Theme.type.axis
            visible: text !== ""
        }
    }
}
