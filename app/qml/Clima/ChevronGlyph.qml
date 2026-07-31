// SPDX-FileCopyrightText: 2026 Jowi Aoun
// SPDX-License-Identifier: GPL-3.0-or-later
// A chevron, pointing one of four ways.
//
// Extracted because the mobile shell grew five of them in an afternoon — the
// location bar's disclosure, every card header's "more" affordance, the metric
// dropdown, the month picker — and they had already drifted to three stroke
// widths. It is one stroked path; the point is that there is one of it.
//
// It does not rotate between directions. `direction` is a property rather than
// a state for that reason: a chevron that turns over is a disclosure animation,
// and disclosure is the caller's business, not the glyph's.
import QtQuick
import QtQuick.Shapes

Item {
    id: root

    property string direction: "right"       // "right" | "left" | "down" | "up"
    property real glyphSize: 14
    property color tint: Theme.color.textMuted
    property real weight: 1.6

    implicitWidth: glyphSize
    implicitHeight: glyphSize
    width: glyphSize
    height: glyphSize

    // Drawn once, pointing down, and rotated into place. Four hand-written
    // paths is four chances for one of them to be a pixel off its siblings,
    // and the rotation is a constant per direction rather than an animation.
    readonly property real turn: direction === "down" ? 0
                              : direction === "up" ? 180
                              : direction === "right" ? -90
                              : 90

    Shape {
        anchors.fill: parent
        rotation: root.turn
        preferredRendererType: Shape.CurveRenderer

        ShapePath {
            strokeColor: root.tint
            strokeWidth: root.weight
            fillColor: "transparent"
            capStyle: ShapePath.RoundCap
            joinStyle: ShapePath.RoundJoin

            Behavior on strokeColor {
                ColorAnimation { duration: Theme.motion.tint; easing.type: Easing.OutCubic }
            }

            // In a 14-unit box, scaled by the Shape to whatever size it is
            // given. The apex sits below centre so the glyph optically centres
            // against text rather than measuring centred and looking high.
            PathSvg {
                path: {
                    var u = root.glyphSize / 14
                    function p(x, y) { return (x * u).toFixed(2) + " " + (y * u).toFixed(2) }
                    return "M " + p(3.2, 5.4) + " L " + p(7, 9.4) + " L " + p(10.8, 5.4)
                }
            }
        }
    }
}
