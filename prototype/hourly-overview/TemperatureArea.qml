// SPDX-License-Identifier: GPL-3.0-or-later
// The temperature curve: gradient-filled area plus a gradient "stroke".
//
// The gradient runs down the *value* axis, not along time, so colour encodes
// absolute temperature. That is what makes a 19° hour read green and a 27° hour
// read tan at the same place on the same chart.
import QtQuick
import QtQuick.Shapes
import "chartmath.js" as ChartMath
import "theme.js" as Theme

Item {
    id: root

    property var points: []
    property real baselineY: height
    property real gradientTop: 0        // y of tempMax
    property real gradientBottom: height // y of tempMin
    property real tempMin: Theme.scale.tempMin
    property real tempMax: Theme.scale.tempMax
    property real lineWidth: 2.4

    function stopPos(t) {
        var span = tempMax - tempMin
        return span <= 0 ? 0 : ChartMath.clamp((tempMax - t) / span, 0, 1)
    }

    Shape {
        anchors.fill: parent
        preferredRendererType: Shape.CurveRenderer

        // ---- filled area -------------------------------------------------
        ShapePath {
            strokeColor: "transparent"
            fillGradient: LinearGradient {
                x1: 0; y1: root.gradientTop
                x2: 0; y2: root.gradientBottom
                GradientStop { position: root.stopPos(40); color: Theme.fillRamp[0].c }
                GradientStop { position: root.stopPos(32); color: Theme.fillRamp[1].c }
                GradientStop { position: root.stopPos(26); color: Theme.fillRamp[2].c }
                GradientStop { position: root.stopPos(22); color: Theme.fillRamp[3].c }
                GradientStop { position: root.stopPos(18); color: Theme.fillRamp[4].c }
                GradientStop { position: root.stopPos(12); color: Theme.fillRamp[5].c }
                GradientStop { position: root.stopPos(6);  color: Theme.fillRamp[6].c }
                GradientStop { position: root.stopPos(0);  color: Theme.fillRamp[7].c }
            }
            PathSvg { path: ChartMath.areaPath(root.points, root.baselineY) }
        }

        // ---- curve, as a gradient-fillable ribbon ------------------------
        ShapePath {
            strokeColor: "transparent"
            fillGradient: LinearGradient {
                x1: 0; y1: root.gradientTop
                x2: 0; y2: root.gradientBottom
                GradientStop { position: root.stopPos(40); color: Theme.lineRamp[0].c }
                GradientStop { position: root.stopPos(32); color: Theme.lineRamp[1].c }
                GradientStop { position: root.stopPos(26); color: Theme.lineRamp[2].c }
                GradientStop { position: root.stopPos(22); color: Theme.lineRamp[3].c }
                GradientStop { position: root.stopPos(18); color: Theme.lineRamp[4].c }
                GradientStop { position: root.stopPos(12); color: Theme.lineRamp[5].c }
                GradientStop { position: root.stopPos(6);  color: Theme.lineRamp[6].c }
                GradientStop { position: root.stopPos(0);  color: Theme.lineRamp[7].c }
            }
            PathSvg { path: ChartMath.ribbonPath(root.points, root.lineWidth / 2) }
        }
    }
}
