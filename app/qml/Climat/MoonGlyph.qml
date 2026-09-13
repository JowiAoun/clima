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

    // Which limb is lit — see chartmath.js. Waxing by default so a caller that
    // has not got the answer draws what this glyph always drew.
    property bool waxing: true

    implicitWidth: glyphSize
    implicitHeight: glyphSize
    width: glyphSize
    height: glyphSize

    Rectangle {
        anchors.fill: parent
        radius: width / 2
        color: Theme.glyph.moonShade
    }

    Shape {
        anchors.fill: parent
        preferredRendererType: Shape.CurveRenderer

        ShapePath {
            fillColor: Theme.accent.fill
            strokeColor: "transparent"
            PathSvg {
                path: ChartMath.moonPath(root.width / 2, root.height / 2,
                                         root.width / 2, root.illuminated,
                                         root.waxing)
            }
        }
    }
}
