// SPDX-FileCopyrightText: 2026 Jowi Aoun
// SPDX-License-Identifier: GPL-3.0-or-later
// Moon phase disc for the legend: unlit body plus the illuminated limb.
import QtQuick
import QtQuick.Shapes
import "chartmath.js" as ChartMath

Item {
    id: root

    property real glyphSize: 15
    property real illuminated: 0.74

    implicitWidth: glyphSize
    implicitHeight: glyphSize
    width: glyphSize
    height: glyphSize

    Rectangle {
        anchors.fill: parent
        radius: width / 2
        color: "#38425e"
    }

    Shape {
        anchors.fill: parent
        preferredRendererType: Shape.CurveRenderer

        ShapePath {
            fillColor: Theme.color.accent
            strokeColor: "transparent"
            PathSvg {
                path: ChartMath.moonPath(root.width / 2, root.height / 2,
                                         root.width / 2, root.illuminated)
            }
        }
    }
}
