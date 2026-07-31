// SPDX-FileCopyrightText: 2026 Jowi Aoun
// SPDX-License-Identifier: GPL-3.0-or-later
// Procedural weather icons.
//
// Deliberately drawn rather than shipped as assets: it keeps the prototype a
// single `qml` invocation with no asset pipeline. Production swaps these for
// Meteocons (MIT) converted to Qt Quick Shapes via svgtoqml — decision D10.
import QtQuick
import QtQuick.Shapes
import "theme.js" as Theme
import "chartmath.js" as ChartMath

Item {
    id: root

    property string kind: "clear-day"
    property real glyphSize: 26
    property bool onLightBackground: false

    implicitWidth: glyphSize
    implicitHeight: glyphSize
    width: glyphSize
    height: glyphSize

    readonly property bool hasSun:   kind === "clear-day" || kind === "partly-day"
    readonly property bool hasMoon:  kind === "clear-night" || kind === "partly-night"
    readonly property bool hasCloud: kind === "partly-day" || kind === "partly-night"
                                     || kind === "cloudy" || kind === "rain" || kind === "rain-night"
    readonly property bool soloCloud: kind === "cloudy" || kind === "rain" || kind === "rain-night"
    readonly property bool hasRain:  kind === "rain" || kind === "rain-night"

    // ---- sun -------------------------------------------------------------
    Item {
        visible: root.hasSun
        width: root.soloCloud ? 0 : (root.kind === "clear-day" ? root.width * 0.72 : root.width * 0.56)
        height: width
        x: root.kind === "clear-day" ? (root.width - width) / 2 : root.width * 0.04
        y: root.kind === "clear-day" ? (root.height - height) / 2 : root.height * 0.06

        // A glow, not a disc.
        //
        // This was a flat circle of `sunGlyphWarm` at 16 %, 1.28x the sun's
        // diameter. A flat circle has an edge you can trace, which is §10.1's
        // test for a stacked wash rather than a glow — and at the 26 px this
        // glyph is normally drawn at, the edge is a couple of pixels and nobody
        // ever saw it. Staged at 72 px on the current-conditions card it is an
        // unmistakable hard-rimmed ring around the sun.
        //
        // Fading to zero alpha at the rim is what it was always meant to be.
        Shape {
            id: halo
            anchors.centerIn: parent
            width: parent.width * 1.9
            height: width
            preferredRendererType: Shape.CurveRenderer

            readonly property color tint: Theme.color.sunGlyphWarm

            ShapePath {
                strokeColor: "transparent"
                fillGradient: RadialGradient {
                    centerX: halo.width / 2
                    centerY: halo.height / 2
                    centerRadius: halo.width / 2
                    focalX: centerX
                    focalY: centerY
                    // Flat out to the sun's own rim, then away to nothing.
                    GradientStop {
                        position: 0.0
                        color: Qt.rgba(halo.tint.r, halo.tint.g, halo.tint.b, 0.20)
                    }
                    GradientStop {
                        position: 0.52
                        color: Qt.rgba(halo.tint.r, halo.tint.g, halo.tint.b, 0.18)
                    }
                    GradientStop {
                        position: 1.0
                        color: Qt.rgba(halo.tint.r, halo.tint.g, halo.tint.b, 0.0)
                    }
                }
                PathAngleArc {
                    centerX: halo.width / 2
                    centerY: halo.height / 2
                    radiusX: halo.width / 2
                    radiusY: halo.height / 2
                    startAngle: 0
                    sweepAngle: 360
                }
            }
        }
        Rectangle {
            anchors.fill: parent
            radius: width / 2
            gradient: Gradient {
                GradientStop { position: 0.0; color: Theme.color.sunGlyphWarm }
                GradientStop { position: 1.0; color: Theme.color.sunGlyphCool }
            }
        }
    }

    // ---- moon ------------------------------------------------------------
    Shape {
        visible: root.hasMoon
        width: root.width
        height: root.height
        preferredRendererType: Shape.CurveRenderer

        ShapePath {
            fillColor: Theme.color.moonGlyph
            strokeColor: "transparent"
            // Crescent sits centred when alone, upper-left when a cloud is in front.
            PathSvg {
                path: ChartMath.moonPath(
                          root.width  * (root.kind === "clear-night" ? 0.50 : 0.34),
                          root.height * (root.kind === "clear-night" ? 0.47 : 0.34),
                          root.width  * (root.kind === "clear-night" ? 0.34 : 0.26),
                          0.42)
            }
        }
    }

    // ---- cloud -----------------------------------------------------------
    Shape {
        visible: root.hasCloud
        width: root.width
        height: root.height
        preferredRendererType: Shape.CurveRenderer

        ShapePath {
            strokeColor: "transparent"
            fillGradient: LinearGradient {
                x1: 0; y1: root.height * (root.soloCloud ? 0.24 : 0.42)
                x2: 0; y2: root.height * (root.soloCloud ? 0.80 : 0.92)
                GradientStop {
                    position: 0.0
                    color: root.onLightBackground ? Theme.color.cloudTopOnLight
                                                  : Theme.color.cloudTop
                }
                GradientStop {
                    position: 1.0
                    color: root.onLightBackground ? Theme.color.cloudBottomOnLight
                                                  : Theme.color.cloudBottom
                }
            }
            PathSvg { path: root.cloudPath(root.width, root.height, root.soloCloud) }
        }
    }

    // ---- rain ------------------------------------------------------------
    Repeater {
        model: root.hasRain ? 3 : 0
        delegate: Rectangle {
            required property int index
            width: Math.max(1.8, root.width * 0.07)
            height: root.height * 0.22
            radius: width / 2
            color: Theme.color.rainDrop
            x: root.width * (0.25 + index * 0.21)
            y: root.height * (0.74 + (index === 1 ? 0.07 : 0))
            transform: Rotation { angle: 12 }
        }
    }

    // A cloud from four cubic segments: flat base, tall bump left of centre,
    // shorter bump to the right. Coordinates are fractions of the icon box.
    function cloudPath(w, h, solo) {
        var s  = solo ? 1.0 : 0.74;                 // scale
        var ox = solo ? 0.04 * w : 0.26 * w;        // origin
        var oy = solo ? 0.24 * h : 0.40 * h;
        var uw = 0.92 * w * s;
        var uh = 0.56 * h * s;

        function px(f) { return (ox + f * uw).toFixed(2) }
        function py(f) { return (oy + f * uh).toFixed(2) }

        return "M " + px(0.13) + " " + py(1.00)
             + " C " + px(0.04) + " " + py(1.00) + " " + px(0.00) + " " + py(0.79) + " " + px(0.00) + " " + py(0.66)
             + " C " + px(0.00) + " " + py(0.48) + " " + px(0.07) + " " + py(0.36) + " " + px(0.16) + " " + py(0.34)
             + " C " + px(0.19) + " " + py(0.12) + " " + px(0.32) + " " + py(0.00) + " " + px(0.46) + " " + py(0.00)
             + " C " + px(0.58) + " " + py(0.00) + " " + px(0.68) + " " + py(0.09) + " " + px(0.73) + " " + py(0.24)
             + " C " + px(0.77) + " " + py(0.20) + " " + px(0.82) + " " + py(0.18) + " " + px(0.87) + " " + py(0.18)
             + " C " + px(0.96) + " " + py(0.18) + " " + px(1.00) + " " + py(0.38) + " " + px(1.00) + " " + py(0.62)
             + " C " + px(1.00) + " " + py(0.83) + " " + px(0.94) + " " + py(1.00) + " " + px(0.84) + " " + py(1.00)
             + " Z";
    }
}
