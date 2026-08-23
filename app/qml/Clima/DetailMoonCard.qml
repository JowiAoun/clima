// SPDX-FileCopyrightText: 2026 Jowi Aoun
// SPDX-License-Identifier: GPL-3.0-or-later
// Moon detail card — DetailSunCard.qml's twin, and deliberately the same card.
//
// The curve is a sinusoid in altitude: zero at moonrise and moonset, one at the
// moon's transit, negative outside. The horizon line is altitude zero, so the
// stretch above it *is* the time the moon is up and its width on the card *is*
// that duration, as a fraction of the twenty-four hours the box spans.
//
// Two things are the moon's own, and only two:
//
//   - the ramp is cool silver-blue rather than warm, so a glance at the grid
//     tells the pair apart before either title is read;
//   - the mark on the curve is the moon at its actual phase, drawn by
//     ChartMath.moonPath from `illumination`, so a first quarter and a waning
//     gibbous do not draw the same card.
//
// Everything else — band geometry, horizon, crossing dots, the annotation under
// the span, the two clock figures, and the arrival — is written the same way as
// in the sun card on purpose. A geometry change here belongs there too, and so
// does a change to the motion: the pair is read side by side and two twins that
// arrive differently stop being twins.
//
// Where the mark sits is the honest part. The moon set at 8:03 AM and does not
// rise again until 9:25 PM, and it is 12:28 PM: it is *below* the horizon, about
// a third of the way through that gap. So it is drawn low on the dim stretch
// past moonset, not parked on the arc. The window wraps, which is what puts a
// set-before-rise night on the same footing as the sun's rise-before-set day.
import QtQuick
import QtQuick.Shapes
import "chartmath.js" as ChartMath

DetailCard {
    id: root

    readonly property var d: Detail.moon

    title: qsTr("Moon")
    status: d.status
    trend: d.trend
    body: d.body

    content: Item {
        id: viz

        // The one thing this card owns that its twin does not: a cool ramp,
        // keyed on altitude with p = 0 at the transit and p = 1 at the horizon.
        // Silver overhead, cool blue where it meets the horizon.
        // Light mode is not a tint of this. Silver-to-cool-blue is a range that
        // exists above a dark card; over a pale one the whole ramp sits within a
        // few percent of the surface and the arc disappears, which is what the
        // first light render showed — the Sun card beside it kept its arc,
        // because saturated gold survives either ground, and the Moon's did not.
        // So the light ramp keeps the hue relationship and moves the range down.
        readonly property var skyRamp: Theme.isLight
            ? [
                { p: 0.00, c: "#8fa3c9" },
                { p: 0.45, c: "#5f7bb0" },
                { p: 1.00, c: "#3d5a94" }
              ]
            : [
                { p: 0.00, c: "#eaf0ff" },
                { p: 0.45, c: "#b6c8f0" },
                { p: 1.00, c: "#7a97d8" }
              ]

        // The unlit limb, and it inverts outright rather than shifting. In dark
        // the shadowed side is below the surface it sits on; in light it has to
        // be above the lit side, or the phase reads inside out.
        //
        // Darker than MoonGlyph's in either theme, which sits on the hourly
        // chart: at 14 px the shadowed side has to be well clear of the surface
        // or the phase stops being visible at all.
        readonly property color moonDark: Theme.isLight ? "#c4cde0" : "#222a4a"

        // ---- the cycle, in minutes -------------------------------------------
        // Minutes up, taken modulo a day, because the moon sets before it rises:
        // 9:25 PM to 8:03 AM is 10 h 38 m up, not minus 13 h. tSet is the set
        // expressed in the same continuous frame as the rise, so the arc can be
        // walked straight through without wrapping.
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
        // radius at both ends or the ring hangs outside the content box. The
        // horizon is placed from the inset rather than from the curve's own
        // extremes, so both cards in the pair put it at the same height whatever
        // their tails do.
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
        // The sun card's arrival, unchanged, because the pair has to move alike.
        // Two pens and a mark leave the rise crossing together on the card's
        // one-shot `reveal`: the curve draws itself outward from moonrise, and
        // the mark walks that curve from moonrise to now. Here the walk is the
        // whole answer — the moon climbs, transits, sets, and keeps going down
        // the dim tail, which is how it ends up parked below the horizon at
        // half past twelve in the afternoon.
        //
        // `now` is inside the drawn window by construction, so it is always
        // nearer the rise than the pen is and the mark can never stand on curve
        // that has not been drawn yet.
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
            // it is drawn because a bare hump gives no sense of where in the
            // whole twenty-four hours we are — which on this card is the whole
            // answer, since the moon is down.
            ShapePath {
                fillColor: "transparent"
                strokeColor: Theme.line.track
                strokeWidth: viz.strokeW
                capStyle: ShapePath.RoundCap
                PathSvg {
                    path: (viz.segment(viz.tPenL, viz.tRise, 12) + " "
                         + viz.segment(viz.tSet, viz.tPenR, 12)).trim()
                }
            }

            // Above the horizon, drawn as a ribbon because a ShapePath can
            // gradient-fill but not gradient-stroke. Silver at the transit, cool
            // blue where it meets the horizon at either end.
            ShapePath {
                strokeColor: "transparent"
                fillGradient: LinearGradient {
                    x1: 0; y1: viz.horizonY - viz.amp; x2: 0; y2: viz.horizonY
                    // The same three stops the arc and the mark are ramped off,
                    // read out of `skyRamp` rather than written again — see the
                    // twin in DetailSunCard.qml, which had the identical pair of
                    // copies a hundred lines apart.
                    GradientStop { position: viz.skyRamp[0].p; color: viz.skyRamp[0].c }
                    GradientStop { position: viz.skyRamp[1].p; color: viz.skyRamp[1].c }
                    GradientStop { position: viz.skyRamp[2].p; color: viz.skyRamp[2].c }
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
            color: Theme.line.grid
        }

        // The two crossings the two readings below name. Flat and small on
        // purpose: a ring at this size is the "now" mark's signature and must not
        // be spent on anything else.
        Repeater {
            model: [viz.tRise, viz.tSet]
            delegate: Rectangle {
                required property var modelData
                width: 8; height: 8; radius: 4
                color: Theme.ink.primary
                // Uncovered as the pen reaches it, so a crossing is never marked
                // on curve that has not been drawn. The rise is where the pen
                // starts, so that one is there from the first frame. Opacity
                // rather than `visible` — §10.8.
                opacity: viz.tPenR >= modelData ? 1 : 0
                x: viz.xAt(modelData) - width / 2
                y: viz.horizonY - height / 2
            }
        }

        // Now: 14 px, ringed, and — this being the moon — showing the phase. The
        // ring is drawn last so it sits over the glyph: without it the shadowed
        // limb merges straight into the dim stretch the mark is standing on.
        Item {
            id: nowMark

            width: 2 * viz.markR
            height: width
            x: viz.xAt(viz.tMark) - viz.markR
            y: viz.yAt(viz.tMark) - viz.markR

            Rectangle {
                anchors.fill: parent
                radius: width / 2
                color: viz.moonDark
            }

            Shape {
                anchors.fill: parent
                preferredRendererType: Shape.CurveRenderer

                ShapePath {
                    fillColor: Theme.glyph.moon
                    strokeColor: "transparent"
                    PathSvg {
                        path: ChartMath.moonPath(viz.markR, viz.markR, viz.markR,
                                                 root.d.illumination, root.d.waxing)
                    }
                }
            }

            Rectangle {
                anchors.fill: parent
                radius: width / 2
                color: "transparent"
                border.width: 2.5
                border.color: Theme.ink.primary
            }
        }

        // How long the moon is up, centred on the stretch of horizon it measures,
        // and under it the one fact the status line below does not already carry.
        // The status line says "Waning Gibbous"; printing that again here — which
        // this card used to do — spends a line saying nothing.
        Text {
            id: spanLabel
            text: root.d.upLength
            color: Theme.ink.muted
            font.pixelSize: Theme.type.axis
            x: Math.round((viz.xAt(viz.tRise) + viz.xAt(viz.tSet) - width) / 2)
            y: Math.round(viz.horizonY) + 3
        }

        Text {
            id: litLabel
            text: qsTr("%1% lit").arg(Math.round(root.d.illumination * 100))
            color: Theme.ink.muted
            font.pixelSize: Theme.type.axis
            x: Math.round((viz.xAt(viz.tRise) + viz.xAt(viz.tSet) - width) / 2)
            y: spanLabel.y + spanLabel.height
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
                      figName: qsTr("Moonrise") },
                    { figTime: root.d.setLabel, figSuffix: root.d.setSuffix,
                      figName: qsTr("Moonset") }
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
                        color: Theme.ink.primary
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
                            color: Theme.ink.muted
                            font.pixelSize: Theme.type.label
                        }
                        Text {
                            text: modelData.figName
                            color: Theme.ink.muted
                            font.pixelSize: Theme.type.label
                        }
                    }
                }
            }
        }
    }
}
