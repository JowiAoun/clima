// SPDX-FileCopyrightText: 2026 Jowi Aoun
// SPDX-License-Identifier: GPL-3.0-or-later
// What every surface in this prototype is composited over.
//
// It is a gradient rather than a flat fill because every surface above it is
// translucent: the cards have no colour of their own and take whatever this is
// doing behind them at that height. Flatten it and the whole page flattens.
//
// Extracted from Main.qml when the gallery grew device frames. A specimen
// framed at 390x844 inside a 950 px window was being drawn over whatever slice
// of the *window's* gradient happened to be behind it, which is not the slice
// it gets in the app — and the gallery's entire premise is that a component is
// reviewed on the background it is actually composited over.
//
// ---- the sky -----------------------------------------------------------------
// `phase` picks one of theme.js's four skies. It defaults to `dusk`, which is
// the palette this prototype has always had, so a caller that does not care
// gets exactly what it got before this file existed. The mobile shell is the
// one that cares: it follows the clock.
//
// `stars` adds the field and three constellations. On by default at night and
// dusk only through `Theme.sky[phase].stars` — a starfield over a midday sky
// is not a stylistic choice, it is wrong.
//
// ---- why nothing here twinkles -----------------------------------------------
// §10.6: "Nothing animates on a timer. No pulsing, breathing, shimmering,
// drifting or looping. If it moves while the user is doing nothing, it is
// wrong." A twinkling star field is the single most tempting thing to build in
// this file and it breaks that rule harder than anything else in the
// prototype: it would run forever, behind every screen, while the reader is
// trying to read a number off a chart. It would also make every golden image
// of every mobile screen a coin toss.
//
// The field is static and deterministic — see sky.js, which has no
// Math.random in it for the same reason.
//
// The stars do not scroll with the page either. They are the sky, and the sky
// does not move when you scroll a page: a parallax layer here would be motion
// tied to a scroll rather than to a timer, which is a different rule, but it
// would still put drifting pinpoints behind a chart being read.
import QtQuick
import QtQuick.Shapes
import "sky.js" as Sky

Rectangle {
    id: root

    property string phase: "dusk"
    property bool stars: false

    // `skyPalette` rather than the obvious `palette`: Item already declares
    // that one, and shadowing it is not an error — it is a runtime warning and
    // a property that quietly means two things. Same family of trap as the
    // FINAL `top` the README records.
    readonly property var skyPalette: Theme.sky[phase] !== undefined ? Theme.sky[phase]
                                                                    : Theme.sky.dusk
    readonly property real starOpacity: stars ? skyPalette.stars : 0

    // Positions are here and colours are in theme.js: QML cannot generate
    // GradientStop elements from a Repeater, so the five have to be written
    // out somewhere, and the somewhere should be the file that draws them.
    gradient: Gradient {
        GradientStop { position: 0.00; color: root.skyPalette.stops[0] }
        GradientStop { position: 0.06; color: root.skyPalette.stops[1] }
        GradientStop { position: 0.30; color: root.skyPalette.stops[2] }
        GradientStop { position: 0.60; color: root.skyPalette.stops[3] }
        GradientStop { position: 1.00; color: root.skyPalette.stops[4] }
    }

    // One item for the whole sky, so a phase change is one opacity rather than
    // a hundred and thirty. It is also what keeps the field out of the scene
    // graph's way entirely during the day.
    Item {
        id: night
        anchors.fill: parent
        visible: root.starOpacity > 0
        opacity: root.starOpacity

        // ---- the field ------------------------------------------------------
        // Plain Rectangles, not Shapes. At 0.55–2 px these are points, and a
        // radial gradient per point would be 120 offscreen passes to draw
        // something two pixels across.
        Repeater {
            model: Sky.field(130)

            delegate: Rectangle {
                required property var modelData

                width: modelData.r * 2
                height: width
                radius: modelData.r
                color: Theme.star.ink
                opacity: modelData.a
                x: modelData.x * night.width - modelData.r
                y: modelData.y * night.height - modelData.r
            }
        }

        // ---- the brighter few ----------------------------------------------
        // A glow, not a disc — the distinction §10.1 draws, and the same one
        // WeatherGlyph's sun halo had to learn: a flat circle has an edge you
        // can trace, and an edge makes it a stacked wash.
        Repeater {
            model: Sky.beacons(9)

            delegate: Item {
                id: beacon
                required property var modelData

                // Every dimension below is `beacon.something`, never `parent`.
                // Inside a RadialGradient, `parent` is the ShapePath — which
                // has no width — so `parent.width / 2` is NaN and the gradient
                // renders as a flat white disc the size of the item. That is
                // exactly what it did: nine hard white blobs sitting on top of
                // the ten-day card, and the mistake is invisible in the code.
                width: beacon.modelData.r * 7
                height: width
                x: beacon.modelData.x * night.width - width / 2
                y: beacon.modelData.y * night.height - height / 2

                Shape {
                    anchors.fill: parent
                    preferredRendererType: Shape.CurveRenderer

                    ShapePath {
                        strokeColor: "transparent"
                        fillGradient: RadialGradient {
                            centerX: beacon.width / 2
                            centerY: beacon.height / 2
                            centerRadius: beacon.width / 2
                            focalX: centerX
                            focalY: centerY
                            // Never fully white. These sit behind cards, and a
                            // star that reaches full opacity behind a 0.07
                            // wash reads as a rendering fault in the card.
                            GradientStop { position: 0.00; color: "#d9ffffff" }
                            GradientStop { position: 0.18; color: "#66ffffff" }
                            GradientStop { position: 0.50; color: "#1affffff" }
                            GradientStop { position: 1.00; color: "#00ffffff" }
                        }
                        PathAngleArc {
                            centerX: beacon.width / 2
                            centerY: beacon.height / 2
                            radiusX: beacon.width / 2
                            radiusY: beacon.height / 2
                            startAngle: 0
                            sweepAngle: 360
                        }
                    }
                }
            }
        }

        // ---- the figures ----------------------------------------------------
        Repeater {
            model: Sky.constellations

            delegate: Item {
                id: figure
                required property var modelData

                anchors.fill: parent

                Shape {
                    anchors.fill: parent
                    preferredRendererType: Shape.CurveRenderer

                    ShapePath {
                        fillColor: "transparent"
                        strokeColor: Theme.star.line
                        strokeWidth: 1
                        capStyle: ShapePath.RoundCap
                        joinStyle: ShapePath.RoundJoin
                        PathSvg {
                            path: Sky.figurePath(figure.modelData, night.width, night.height)
                        }
                    }
                }

                // The figure's own stars, brighter than the field they sit in.
                // Without these a constellation is a bare polygon drawn on the
                // sky, which reads as a UI overlay rather than as a shape the
                // stars happen to make.
                Repeater {
                    model: Sky.figureStars(figure.modelData, night.width, night.height)

                    delegate: Rectangle {
                        required property var modelData

                        width: 2.6
                        height: 2.6
                        radius: 1.3
                        color: Theme.star.ink
                        // Brighter than the field, dimmer than a beacon. The
                        // figure has to be findable without becoming the first
                        // thing read on a screen whose subject is a number.
                        opacity: 0.7
                        x: modelData.x - 1.3
                        y: modelData.y - 1.3
                    }
                }
            }
        }
    }
}
