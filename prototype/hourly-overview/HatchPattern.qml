// SPDX-License-Identifier: GPL-3.0-or-later
// Diagonal hatch. Used wherever the chart means "the past — there is no forecast
// here", so the absence of data reads as intentional rather than as a rendering bug.
import QtQuick
import QtQuick.Shapes
import "chartmath.js" as ChartMath

Item {
    id: root

    property color lineColor: "#16ffffff"
    property real lineWidth: 1
    property real spacing: 8
    property real slope: 0.7        // dx per dy

    clip: true

    Shape {
        anchors.fill: parent
        preferredRendererType: Shape.CurveRenderer

        ShapePath {
            strokeColor: root.lineColor
            strokeWidth: root.lineWidth
            fillColor: "transparent"
            capStyle: ShapePath.FlatCap
            PathSvg {
                path: ChartMath.hatchPath(root.width, root.height, root.spacing, root.slope)
            }
        }
    }
}
