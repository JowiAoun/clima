// SPDX-License-Identifier: GPL-3.0-or-later
// The small round arrow beside a detail card's status line.
//
// Rising, falling and steady are the three things a reading can be doing, and
// the badge says which at a glance without spending a word on it.
import QtQuick
import QtQuick.Shapes
import "theme.js" as Theme

Item {
    id: root

    property string direction: "none"    // "up" | "down" | "steady" | "none"
    property real badgeSize: 16

    visible: direction !== "none"
    width: visible ? badgeSize : 0
    height: badgeSize

    readonly property color tint: direction === "up" ? Theme.color.trendUp
                                : direction === "down" ? Theme.color.trendDown
                                                       : Theme.color.trendSteady

    Rectangle {
        anchors.fill: parent
        radius: width / 2
        color: root.tint
    }

    Shape {
        anchors.fill: parent
        preferredRendererType: Shape.CurveRenderer

        ShapePath {
            fillColor: "transparent"
            strokeColor: Theme.color.onAccent
            strokeWidth: 1.6
            capStyle: ShapePath.RoundCap
            joinStyle: ShapePath.RoundJoin

            PathSvg {
                path: {
                    var s = root.badgeSize
                    var a = s * 0.3, b = s * 0.7      // arrow extents
                    if (root.direction === "steady")
                        return "M " + a + " " + (s / 2) + " L " + b + " " + (s / 2)
                    // Diagonal shaft with a two-stroke head at the tip.
                    var y0 = root.direction === "up" ? b : a
                    var y1 = root.direction === "up" ? a : b
                    return "M " + a + " " + y0 + " L " + b + " " + y1
                         + " M " + b + " " + y1 + " L " + (b - s * 0.22) + " " + y1
                         + " M " + b + " " + y1 + " L " + b + " "
                         + (root.direction === "up" ? y1 + s * 0.22 : y1 - s * 0.22)
                }
            }
        }
    }
}
