// SPDX-FileCopyrightText: 2026 Jowi Aoun
// SPDX-License-Identifier: GPL-3.0-or-later
// Sun detail card — the day drawn as the sun's own path across the sky.
//
// The curve is a sinusoid in altitude: zero at sunrise and sunset, one at solar
// noon, negative outside. The horizon line is altitude zero, so the stretch
// above it *is* the daylight and its width on the card *is* the day length.
// Nothing here is a decorative bell — move riseMin or setMin and the crossings,
// the sun mark and the warm stretch all move with them.
//
// The horizontal span is a full 24 hours centred on solar noon, which is why the
// two crossings sit symmetrically about the middle of the box.
//
// The arrival is a single gesture with a single origin: sunrise. The curve grows
// outward from that crossing in both directions while the sun leaves it and
// walks the arc to where it actually is now, so the distance the mark covers is
// how far through the day we are. Nothing moves afterwards — see the `reveal`
// block below.
//
// DetailMoonCard.qml is this card's twin and is deliberately the same card with
// two things changed: a cool ramp instead of a warm one, and the moon at its
// phase instead of a sun disc on the mark. Everything else — the band geometry,
// the horizon, the crossing dots, the annotation under the span, the two clock
// figures — is written the same way in both files on purpose. A geometry change
// here belongs there too; the grid puts them side by side and they are read as a
// pair.
import QtQuick
import QtQuick.Shapes
import "chartmath.js" as ChartMath

DetailCard {
    id: root

    readonly property var d: Detail.sun

    title: qsTr("Sun")
    status: d.status
    trend: d.trend
    body: d.body

    content: Item {
        id: viz

        // The one thing this card owns that its twin does not: a warm ramp,
        // keyed on altitude with p = 0 at the zenith and p = 1 at the horizon.
        // The same three stops fill the arc and tint the mark, so the mark's
        // colour reads as "how high the sun is" rather than as branding.
        readonly property var skyRamp: [
            { p: 0.00, c: "#ffe488" },
            { p: 0.45, c: "#ffc63f" },
            { p: 1.00, c: "#ef7526" }
        ]

        // ---- the cycle, in minutes -------------------------------------------
        // Minutes up, taken modulo a day so a set-before-rise pair (which is what
        // the moon hands its twin) measures the up-window rather than a negative
        // number. tSet is the set expressed in the same continuous frame as the
        // rise, so the arc can be walked straight through without wrapping.
        readonly property real upMin:
            ((root.d.setMin - root.d.riseMin) % 1440 + 1440) % 1440
        readonly property real tRise: root.d.riseMin
        readonly property real tSet: tRise + upMin
        readonly property real tTransit: tRise + upMin / 2
        readonly property real tMin: tTransit - 720
        readonly property real tMax: tTransit + 720
        // Now, wrapped into the drawn window. The window is exactly a day, so
        // there is one place it can land.
        readonly property real tNow:
            tMin + (((root.d.nowMin - tMin) % 1440) + 1440) % 1440

        // ---- the band the arc is drawn in ------------------------------------
        // The mark rides the curve, so the band has to be inset by the mark's own
        // radius at both ends or the ring hangs outside the content box — which
        // is exactly what this card used to do at the top. The horizon is placed
        // from the inset rather than from the curve's own extremes, so both cards
        // in the pair put it at the same height whatever their tails do.
        readonly property real markR: 7
        readonly property real padX: 5
        readonly property real bandH: figures.y
        readonly property real amp: Math.max(10, (bandH - 2 * markR - 14) / 2)
        readonly property real horizonY: markR + 8 + amp
        readonly property real strokeW: 5

        function altAt(t) { return Math.sin(Math.PI * (t - tRise) / upMin) }
        function xAt(t) { return padX + (t - tMin) / 1440 * (width - padX * 2) }
        function yAt(t) { return horizonY - altAt(t) * amp }

        function arcPoints(t0, t1, n) {
            var out = []
            for (var i = 0; i <= n; ++i) {
                var t = t0 + (t1 - t0) * i / n
                out.push({ x: xAt(t), y: yAt(t) })
            }
            return out
        }

        // ---- the arrival -----------------------------------------------------
        // Two pens and a mark, all three leaving the rise crossing together and
        // all three driven by the card's one-shot `reveal`. The curve draws
        // itself outward from the rise — right toward the set and on down the
        // far tail, left back into the night before — and the mark walks the
        // same curve from the rise to now.
        //
        // Sunrise is the origin because it is the only instant on this card that
        // means anything on its own: the mark's journey is then literally how far
        // through the day we are, which is the reading the card exists to give.
        // It also settles the ordering question for free. `now` is inside the
        // drawn window by construction, so it is always nearer the rise than the
        // pen is, and the mark can never end up standing on curve that has not
        // been drawn yet. Both cards in the pair move exactly this way.
        readonly property real tPenL: tRise - (tRise - tMin) * root.reveal
        readonly property real tPenR: tRise + (tMax - tRise) * root.reveal
        readonly property real tMark: tRise + (tNow - tRise) * root.reveal

        // Sub-minute spans are dropped rather than drawn: a zero-length subpath
        // under a round cap paints a bead the width of the stroke, which at
        // reveal 0 would put three of them on the horizon before the curve
        // exists at all.
        function segment(t0, t1, n) {
            return (t1 - t0) > 0.5 ? ChartMath.smooth(arcPoints(t0, t1, n), "M") : ""
        }

        function ribbonTo(t1) {
            return (t1 - tRise) > 0.5
                ? ChartMath.ribbonPath(arcPoints(tRise, t1, 28), strokeW / 2)
                : ""
        }

        Shape {
            anchors.fill: parent
            preferredRendererType: Shape.CurveRenderer

            // Below the horizon: the same curve, at track weight. It is the
            // unfilled remainder of the day, which is what trackLine is for, and
            // it is drawn because a bare daylight hump gives no sense of where in
            // the whole twenty-four hours we are.
            ShapePath {
                fillColor: "transparent"
                strokeColor: Theme.color.trackLine
                strokeWidth: viz.strokeW
                capStyle: ShapePath.RoundCap
                PathSvg {
                    path: (viz.segment(viz.tPenL, viz.tRise, 12) + " "
                         + viz.segment(viz.tSet, viz.tPenR, 12)).trim()
                }
            }

            // Daylight, drawn as a ribbon because a ShapePath can gradient-fill
            // but not gradient-stroke. Bright at the zenith, sunset-orange where
            // it meets the horizon at either end.
            ShapePath {
                strokeColor: "transparent"
                fillGradient: LinearGradient {
                    x1: 0; y1: viz.horizonY - viz.amp; x2: 0; y2: viz.horizonY
                    GradientStop { position: 0.00; color: "#ffe488" }
                    GradientStop { position: 0.45; color: "#ffc63f" }
                    GradientStop { position: 1.00; color: "#ef7526" }
                }
                PathSvg {
                    path: viz.ribbonTo(Math.min(viz.tPenR, viz.tSet))
                }
            }
        }

        // The horizon. Altitude zero, and the only reference the arc needs.
        Rectangle {
            x: 0
            y: Math.round(viz.horizonY) - height / 2
            width: viz.width
            height: 1
            color: Theme.color.gridLine
        }

        // The two crossings the two readings below name. Flat and small on
        // purpose: a ring at this size is the "now" mark's signature and must not
        // be spent on anything else.
        Repeater {
            model: [viz.tRise, viz.tSet]
            delegate: Rectangle {
                required property var modelData
                width: 8; height: 8; radius: 4
                color: Theme.color.textPrimary
                // Uncovered as the pen reaches it, so a crossing is never marked
                // on curve that has not been drawn. The rise is where the pen
                // starts, so that one is there from the first frame. Opacity
                // rather than `visible` — §10.8.
                opacity: viz.tPenR >= modelData ? 1 : 0
                x: viz.xAt(modelData) - width / 2
                y: viz.horizonY - height / 2
            }
        }

        // Now: 14 px, ringed, tinted by the altitude it sits at. On its way it
        // rides `tMark`, so the tint tracks the altitude it is passing through
        // and reads as "how high the sun is" for the whole journey, not only at
        // the end.
        Rectangle {
            width: 2 * viz.markR; height: width; radius: viz.markR
            color: ChartMath.sampleRamp(viz.skyRamp,
                                        1 - Math.max(0, viz.altAt(viz.tMark)))
            border.width: 2.5
            border.color: Theme.color.textPrimary
            x: viz.xAt(viz.tMark) - width / 2
            y: viz.yAt(viz.tMark) - height / 2
        }

        // How long the sun is up, centred on the stretch of horizon it measures.
        // It sits just under the horizon because that band is empty by
        // construction — the arc is above it between the crossings — and the card
        // is too short to spend a row of its own on the label.
        Text {
            id: spanLabel
            text: root.d.dayLength
            color: Theme.color.textMuted
            font.pixelSize: Theme.type.axis
            x: Math.round((viz.xAt(viz.tRise) + viz.xAt(viz.tSet) - width) / 2)
            y: Math.round(viz.horizonY) + 3
        }

        // The two readings, co-equal, so both take the pair size. Each figure is
        // flush left in its half of the box: the grid has a left rhythm and a
        // pair that centres itself in halves breaks it just as surely as a single
        // centred reading would.
        Item {
            id: figures

            anchors.left: parent.left
            anchors.right: parent.right
            anchors.bottom: parent.bottom
            height: figProbe.height

            Text {
                id: figProbe
                visible: false
                text: "0"
                font.pixelSize: Theme.type.readingPair
                font.bold: true
            }

            Repeater {
                model: [
                    { figTime: root.d.riseLabel, figSuffix: root.d.riseSuffix,
                      figName: qsTr("Sunrise") },
                    { figTime: root.d.setLabel, figSuffix: root.d.setSuffix,
                      figName: qsTr("Sunset") }
                ]

                delegate: Item {
                    required property int index
                    required property var modelData

                    width: figures.width / 2
                    height: figures.height
                    x: index * width

                    Text {
                        id: figClock
                        text: modelData.figTime
                        color: Theme.color.textPrimary
                        font.pixelSize: Theme.type.readingPair
                        font.bold: true
                        anchors.left: parent.left
                        anchors.bottom: parent.bottom
                    }

                    Column {
                        anchors.left: figClock.right
                        anchors.leftMargin: 6
                        anchors.verticalCenter: figClock.verticalCenter

                        Text {
                            text: modelData.figSuffix
                            color: Theme.color.textMuted
                            font.pixelSize: Theme.type.label
                        }
                        Text {
                            text: modelData.figName
                            color: Theme.color.textMuted
                            font.pixelSize: Theme.type.label
                        }
                    }
                }
            }
        }
    }
}
