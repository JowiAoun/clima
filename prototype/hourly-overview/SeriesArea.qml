// SPDX-License-Identifier: GPL-3.0-or-later
// Filled series: gradient area plus a gradient "curve".
//
// Generalised from the temperature chart — the gradient runs down the *value*
// axis and its stops are keyed by normalised position, so the same component
// serves °C, %, km/h and hPa without knowing what it is drawing.
import QtQuick
import QtQuick.Shapes
import "chartmath.js" as ChartMath

Item {
    id: root

    property var points: []
    property var overlayPoints: []      // optional second line, e.g. wind gusts
    property real baselineY: height
    property real gradientTop: 0
    property real gradientBottom: height
    property var fillRamp: []
    property var lineRamp: []
    property real lineWidth: 2.4

    function stopColor(which, i) {
        var r = which === "fill" ? fillRamp : lineRamp
        return (r && r[i]) ? r[i].c : "#00000000"
    }

    function stopPos(which, i) {
        var r = which === "fill" ? fillRamp : lineRamp
        return (r && r[i]) ? r[i].p : (i / 7)
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
                GradientStop { position: root.stopPos("fill", 0); color: root.stopColor("fill", 0) }
                GradientStop { position: root.stopPos("fill", 1); color: root.stopColor("fill", 1) }
                GradientStop { position: root.stopPos("fill", 2); color: root.stopColor("fill", 2) }
                GradientStop { position: root.stopPos("fill", 3); color: root.stopColor("fill", 3) }
                GradientStop { position: root.stopPos("fill", 4); color: root.stopColor("fill", 4) }
                GradientStop { position: root.stopPos("fill", 5); color: root.stopColor("fill", 5) }
                GradientStop { position: root.stopPos("fill", 6); color: root.stopColor("fill", 6) }
                GradientStop { position: root.stopPos("fill", 7); color: root.stopColor("fill", 7) }
            }
            PathSvg { path: ChartMath.areaPath(root.points, root.baselineY) }
        }

        // ---- curve, as a gradient-fillable ribbon ------------------------
        ShapePath {
            strokeColor: "transparent"
            fillGradient: LinearGradient {
                x1: 0; y1: root.gradientTop
                x2: 0; y2: root.gradientBottom
                GradientStop { position: root.stopPos("line", 0); color: root.stopColor("line", 0) }
                GradientStop { position: root.stopPos("line", 1); color: root.stopColor("line", 1) }
                GradientStop { position: root.stopPos("line", 2); color: root.stopColor("line", 2) }
                GradientStop { position: root.stopPos("line", 3); color: root.stopColor("line", 3) }
                GradientStop { position: root.stopPos("line", 4); color: root.stopColor("line", 4) }
                GradientStop { position: root.stopPos("line", 5); color: root.stopColor("line", 5) }
                GradientStop { position: root.stopPos("line", 6); color: root.stopColor("line", 6) }
                GradientStop { position: root.stopPos("line", 7); color: root.stopColor("line", 7) }
            }
            PathSvg { path: ChartMath.ribbonPath(root.points, root.lineWidth / 2) }
        }

        // ---- optional overlay line (dashed, no fill) ---------------------
        ShapePath {
            strokeColor: "#8cffffff"
            strokeWidth: root.overlayPoints.length > 1 ? 1.4 : -1
            strokeStyle: ShapePath.DashLine
            dashPattern: [4, 3]
            fillColor: "transparent"
            capStyle: ShapePath.RoundCap
            PathSvg {
                path: root.overlayPoints.length > 1
                      ? ChartMath.smooth(root.overlayPoints, "M") : ""
            }
        }
    }
}
