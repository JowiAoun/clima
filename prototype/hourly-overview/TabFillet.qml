// SPDX-License-Identifier: GPL-3.0-or-later
// The concave corner where a raised tab meets the surface below it.
//
// A tab joined to a panel makes a *reflex* corner: the fill wraps around the
// outside of the angle. Rounding that corner is the opposite of rounding a normal
// one — instead of cutting the corner off, you subtract a quarter-disc from the
// gap beside it, so the fill bulges outward and the tab's side edge flows into the
// panel's top edge. Squaring it off instead is what makes a tab look pasted on.
//
// This is one such corner, filled with the tab colour and sitting in the gap
// *beside* the tab, not inside it.
import QtQuick
import QtQuick.Shapes

Item {
    id: root

    property real filletRadius: 14
    property real extendBelow: 0      // straight skirt under the arc
    property bool mirrored: false     // false = left of the tab, true = right
    property color fillColor: "#1a2440"

    width: filletRadius
    height: filletRadius + extendBelow

    Shape {
        anchors.fill: parent
        preferredRendererType: Shape.CurveRenderer

        ShapePath {
            fillColor: root.fillColor
            strokeColor: "transparent"
            PathSvg {
                path: {
                    var r = root.filletRadius
                    var h = r + root.extendBelow
                    // Box minus a quarter-disc centred on the corner furthest from
                    // the tab; what remains is the outward curve.
                    return root.mirrored
                        ? "M 0 0 L 0 " + h + " L " + r + " " + h + " L " + r + " " + r
                          + " A " + r + " " + r + " 0 0 1 0 0 Z"
                        : "M " + r + " 0 L " + r + " " + h + " L 0 " + h + " L 0 " + r
                          + " A " + r + " " + r + " 0 0 0 " + r + " 0 Z"
                }
            }
        }
    }
}
