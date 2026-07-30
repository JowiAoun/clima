// SPDX-License-Identifier: GPL-3.0-or-later
// Sunrise / sunset marker: half-disc over a horizon line, with the direction of
// travel implied by a small chevron.
import QtQuick
import QtQuick.Shapes
import "theme.js" as Theme

Item {
    id: root

    property string kind: "sunrise"     // "sunrise" | "sunset"
    property real glyphSize: 15

    implicitWidth: glyphSize
    implicitHeight: glyphSize
    width: glyphSize
    height: glyphSize

    Shape {
        anchors.fill: parent
        preferredRendererType: Shape.CurveRenderer

        // sun, clipped by the horizon: risen above it, or setting below it
        ShapePath {
            fillColor: Theme.color.sunGlyphWarm
            strokeColor: "transparent"
            PathSvg {
                path: {
                    var w = root.width, h = root.height
                    var r = w * 0.34
                    var cx = w * 0.5
                    var cy = h * 0.70
                    var sweep = root.kind === "sunrise" ? 1 : 0
                    var yc = root.kind === "sunrise" ? cy : h * 0.56
                    // half disc above the line for sunrise, below it for sunset
                    return "M " + (cx - r).toFixed(2) + " " + yc.toFixed(2)
                         + " A " + r.toFixed(2) + " " + r.toFixed(2) + " 0 0 " + sweep + " "
                         + (cx + r).toFixed(2) + " " + yc.toFixed(2) + " Z"
                }
            }
        }

        // horizon
        ShapePath {
            strokeColor: Theme.color.sunGlyphCool
            strokeWidth: 1.5
            fillColor: "transparent"
            capStyle: ShapePath.RoundCap
            PathSvg {
                path: {
                    var y = (root.kind === "sunrise" ? root.height * 0.70 : root.height * 0.56)
                    return "M " + (root.width * 0.06).toFixed(2) + " " + y.toFixed(2)
                         + " L " + (root.width * 0.94).toFixed(2) + " " + y.toFixed(2)
                }
            }
        }
    }
}
