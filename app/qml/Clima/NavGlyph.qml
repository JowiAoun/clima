// SPDX-FileCopyrightText: 2026 Jowi Aoun
// SPDX-License-Identifier: GPL-3.0-or-later
// The five bottom-nav icons, drawn rather than shipped.
//
// Same decision as WeatherGlyph and for the same reason: the prototype stays a
// single `qml` invocation with no asset pipeline. Production swaps these for
// the shipped icon set (decision D10) — these exist to get the shell's
// proportions right, not to be the final art.
//
// They are outlines, not fills, and that is the one thing to preserve if they
// are redrawn. A nav bar is five icons in a row at 22 px: at that size a solid
// glyph reads as a blob and the only thing distinguishing the five is their
// silhouette, so the interior detail — a clock's hands, a calendar's grid —
// has to be drawn rather than implied.
//
// `today` and `monthly` are deliberately the same calendar body with different
// interiors: one day marked, or the whole month dotted. That is the actual
// difference between the two screens, and two unrelated pictures would hide it.
import QtQuick
import QtQuick.Shapes

Item {
    id: root

    property string kind: "today"   // today | hourly | monthly | maps | me
    property real glyphSize: 22
    property color tint: Theme.color.navGlyph

    implicitWidth: glyphSize
    implicitHeight: glyphSize
    width: glyphSize
    height: glyphSize

    // Everything below is written in a 24-unit box and scaled here, so the
    // five stay in proportion to each other at any size.
    readonly property real u: glyphSize / 24
    function p(x, y) { return (x * u).toFixed(2) + " " + (y * u).toFixed(2) }

    readonly property bool isCalendar: kind === "today" || kind === "monthly"

    // The calendar body both date screens share: a rounded box, the header
    // rule under the month name, and two hangers over the top edge.
    readonly property string calendarPath:
          "M " + p(5.6, 5.2) + " L " + p(18.4, 5.2)
        + " A " + p(2.4, 2.4) + " 0 0 1 " + p(20.8, 7.6)
        + " L " + p(20.8, 18.6)
        + " A " + p(2.4, 2.4) + " 0 0 1 " + p(18.4, 21)
        + " L " + p(5.6, 21)
        + " A " + p(2.4, 2.4) + " 0 0 1 " + p(3.2, 18.6)
        + " L " + p(3.2, 7.6)
        + " A " + p(2.4, 2.4) + " 0 0 1 " + p(5.6, 5.2) + " Z"
        + " M " + p(3.2, 9.6) + " L " + p(20.8, 9.6)
        + " M " + p(8.2, 3) + " L " + p(8.2, 6.6)
        + " M " + p(15.8, 3) + " L " + p(15.8, 6.6)

    readonly property string clockPath:
          "M " + p(12, 3.6)
        + " A " + p(8.4, 8.4) + " 0 1 1 " + p(11.99, 3.6) + " Z"
        + " M " + p(12, 7.4) + " L " + p(12, 12.4) + " L " + p(15.8, 14.6)

    // A folded paper map: three panels, alternating up and down, with the two
    // folds drawn. The zigzag is what makes it a map rather than a page.
    readonly property string mapsPath:
          "M " + p(3.2, 6.6) + " L " + p(9, 4.2) + " L " + p(15, 7.2)
        + " L " + p(20.8, 4.8) + " L " + p(20.8, 17.4) + " L " + p(15, 19.8)
        + " L " + p(9, 16.8) + " L " + p(3.2, 19.2) + " Z"
        + " M " + p(9, 4.2) + " L " + p(9, 16.8)
        + " M " + p(15, 7.2) + " L " + p(15, 19.8)

    readonly property string mePath:
          "M " + p(12, 4.6)
        + " A " + p(3.8, 3.8) + " 0 1 1 " + p(11.99, 4.6) + " Z"
        + " M " + p(4.6, 20.4)
        + " C " + p(5.2, 15.8) + " " + p(8.3, 13.8) + " " + p(12, 13.8)
        + " C " + p(15.7, 13.8) + " " + p(18.8, 15.8) + " " + p(19.4, 20.4)

    readonly property string outline:
        kind === "hourly" ? clockPath
      : kind === "maps"   ? mapsPath
      : kind === "me"     ? mePath
      : calendarPath

    Shape {
        anchors.fill: parent
        preferredRendererType: Shape.CurveRenderer

        ShapePath {
            strokeColor: root.tint
            strokeWidth: Math.max(1.2, 1.7 * root.u)
            fillColor: "transparent"
            capStyle: ShapePath.RoundCap
            joinStyle: ShapePath.RoundJoin

            // The nav tints its glyphs when the tab changes, and a stroke that
            // snapped while the pill behind it slid was the first thing that
            // looked wrong about the bar.
            Behavior on strokeColor {
                ColorAnimation { duration: Theme.motion.tint; easing.type: Easing.OutCubic }
            }

            PathSvg { path: root.outline }
        }
    }

    // ---- calendar interiors ------------------------------------------------
    // One filled day for Today; the month dotted out for Monthly. Filled, not
    // stroked: at 22 px a 3-unit stroked square is a smudge.
    Shape {
        anchors.fill: parent
        visible: root.isCalendar
        preferredRendererType: Shape.CurveRenderer

        ShapePath {
            fillColor: root.tint
            strokeColor: "transparent"

            Behavior on fillColor {
                ColorAnimation { duration: Theme.motion.tint; easing.type: Easing.OutCubic }
            }

            PathSvg {
                path: {
                    if (root.kind === "today")
                        return "M " + root.p(7.4, 12.4) + " L " + root.p(12.2, 12.4)
                             + " L " + root.p(12.2, 17.2) + " L " + root.p(7.4, 17.2) + " Z"

                    // Six cells, 3 x 2, on the same rhythm the marked day sits on.
                    var d = ""
                    for (var c = 0; c < 3; ++c) {
                        for (var r = 0; r < 2; ++r) {
                            var x = 6.6 + c * 5.4
                            var y = 12.4 + r * 4.4
                            d += " M " + root.p(x, y) + " L " + root.p(x + 2.4, y)
                               + " L " + root.p(x + 2.4, y + 2.4) + " L " + root.p(x, y + 2.4) + " Z"
                        }
                    }
                    return d.trim()
                }
            }
        }
    }
}
