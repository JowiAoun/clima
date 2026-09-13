// SPDX-FileCopyrightText: 2026 Jowi Aoun
// SPDX-License-Identifier: GPL-3.0-or-later
// A gear: the one mark in this app whose meaning is learned rather than read.
//
// Which is the argument for using it rather than the word "Preferences" beside
// the location bar. That row is one line tall and holds a place name, a
// disclosure chevron and a home marker; a fourth item spelling out a word would
// be the widest thing on it and would compete with the name of the place the
// page is about. A gear is the one pictogram every desktop has already taught
// everybody, which is a claim that can be made about nothing else here — see
// WeatherGlyph, whose sun and cloud are drawn rather than borrowed precisely
// because weather symbols are not standard.
//
// ---- eight teeth, drawn as one path -------------------------------------------
//
// A ring with eight trapezoids around it, generated rather than written out: the
// arithmetic is four lines and eight hand-placed quadrilaterals is eight chances
// for one to sit a fraction off its neighbours. The hole is a second circle in
// the same path, wound the other way, so the even-odd fill rule cuts it out —
// which is what keeps this one ShapePath instead of a filled gear with a
// page-coloured disc on top of it. A disc would be opaque, and every surface in
// this app is a wash over a gradient.
import QtQuick
import QtQuick.Shapes

Item {
    id: root

    property real glyphSize: 17
    property color tint: Theme.ink.muted

    implicitWidth: glyphSize
    implicitHeight: glyphSize
    width: glyphSize
    height: glyphSize

    // The four dimensions of the drawing, as fractions of the box. Typed
    // properties rather than `var` locals inside the path expression, which is
    // where they started: qmllint types a local initialised from arithmetic as
    // QJSPrimitiveValue, which has no `toFixed`, so every number that reached
    // the path string was an unresolvable member lookup and a warning the
    // ratchet counts.
    readonly property real ringRadius: glyphSize * 0.30   // the body's outer edge
    readonly property real holeRadius: glyphSize * 0.15
    readonly property real toothTip:   glyphSize * 0.46   // how far a tooth reaches
    readonly property real toothHalf:  0.20               // half-width, radians

    // One point on the circle of the given radius, as an SVG coordinate pair.
    // Typed parameters and a typed return, so the whole expression stays a
    // `double` all the way to the string.
    function point(radius: real, angle: real): string {
        return (root.glyphSize / 2 + radius * Math.cos(angle)) + " "
             + (root.glyphSize / 2 + radius * Math.sin(angle))
    }

    Shape {
        anchors.fill: parent
        preferredRendererType: Shape.CurveRenderer

        ShapePath {
            fillColor: root.tint
            strokeColor: "transparent"
            fillRule: ShapePath.OddEvenFill

            Behavior on fillColor {
                ColorAnimation { duration: Theme.motion.tint; easing.type: Easing.OutCubic }
            }

            PathSvg {
                // No toFixed anywhere, which is not an oversight. Rounding the
                // coordinates was cosmetic — an SVG path is not read by anybody
                // — and every call was an unresolvable member lookup: qmllint
                // types the result of arithmetic on untyped locals as
                // QJSPrimitiveValue, which has no `toFixed`, and an unresolvable
                // lookup is what stops qmlcachegen compiling the binding ahead
                // of time. Concatenating the numbers is shorter and typed.
                path: {
                    var half = root.toothHalf
                    var d = ""

                    for (var i = 0; i < 8; ++i) {
                        var mid = i * Math.PI / 4
                        d += (i === 0 ? "M " : "L ") + root.point(root.ringRadius,
                                                                  mid - half * 1.7)
                        d += " L " + root.point(root.toothTip, mid - half)
                        d += " L " + root.point(root.toothTip, mid + half)
                        d += " L " + root.point(root.ringRadius, mid + half * 1.7)
                        // Round the gap between one tooth and the next along the
                        // ring, so the body reads as a circle and not an octagon.
                        d += " A " + root.ringRadius + " " + root.ringRadius + " 0 0 1 "
                           + root.point(root.ringRadius, mid + Math.PI / 4 - half * 1.7)
                    }
                    d += " Z"

                    // The hole, wound the other way so OddEvenFill takes it out.
                    d += " M " + root.point(root.holeRadius, 0)
                    d += " A " + root.holeRadius + " " + root.holeRadius + " 0 1 0 "
                       + root.point(root.holeRadius, Math.PI)
                    d += " A " + root.holeRadius + " " + root.holeRadius + " 0 1 0 "
                       + root.point(root.holeRadius, 0)
                    d += " Z"

                    return d
                }
            }
        }
    }
}
