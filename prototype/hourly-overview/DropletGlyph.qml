// SPDX-License-Identifier: GPL-3.0-or-later
import QtQuick
import QtQuick.Shapes
import "theme.js" as Theme

Item {
    id: root

    property real glyphSize: 11
    property color fillColor: Theme.color.droplet

    implicitWidth: glyphSize * 0.75
    implicitHeight: glyphSize
    width: implicitWidth
    height: implicitHeight

    Shape {
        anchors.fill: parent
        preferredRendererType: Shape.CurveRenderer

        ShapePath {
            fillColor: root.fillColor
            strokeColor: "transparent"
            PathSvg {
                path: {
                    var w = root.width, h = root.height
                    return "M " + (w * 0.5).toFixed(2) + " " + (h * 0.04).toFixed(2)
                         + " C " + (w * 0.92).toFixed(2) + " " + (h * 0.40).toFixed(2)
                         + " "   + (w * 1.00).toFixed(2) + " " + (h * 0.56).toFixed(2)
                         + " "   + (w * 1.00).toFixed(2) + " " + (h * 0.68).toFixed(2)
                         + " C " + (w * 1.00).toFixed(2) + " " + (h * 0.89).toFixed(2)
                         + " "   + (w * 0.78).toFixed(2) + " " + (h * 1.00).toFixed(2)
                         + " "   + (w * 0.50).toFixed(2) + " " + (h * 1.00).toFixed(2)
                         + " C " + (w * 0.22).toFixed(2) + " " + (h * 1.00).toFixed(2)
                         + " "   + (w * 0.00).toFixed(2) + " " + (h * 0.89).toFixed(2)
                         + " "   + (w * 0.00).toFixed(2) + " " + (h * 0.68).toFixed(2)
                         + " C " + (w * 0.00).toFixed(2) + " " + (h * 0.56).toFixed(2)
                         + " "   + (w * 0.08).toFixed(2) + " " + (h * 0.40).toFixed(2)
                         + " "   + (w * 0.50).toFixed(2) + " " + (h * 0.04).toFixed(2)
                         + " Z"
                }
            }
        }
    }
}
