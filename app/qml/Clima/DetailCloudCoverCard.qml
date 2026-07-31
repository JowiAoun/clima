// SPDX-FileCopyrightText: 2026 Jowi Aoun
// SPDX-License-Identifier: GPL-3.0-or-later
// Cloud-cover detail card.
//
// A dial filled for exactly as much of its circumference as the sky is
// covered: 8% cover is 8% of the ring — a stub just past 12 o'clock — and an
// overcast sky would close the ring. The ring is the reading, so the number
// belongs to it and the word belongs to the status line. "Sunny" is said once,
// down there, and is not repeated inside the dial.
//
// There is no face behind the ring. A tinted disc sitting on the card's own
// 0.07 surface composites to a patch whose edge you can trace, which is a
// stacked wash (design system §10.1); the dial reads perfectly well drawn
// straight on to the card, and the review that found it was right.
import QtQuick
import QtQuick.Shapes
import "chartmath.js" as ChartMath
import "detaildata.js" as Detail

DetailCard {
    id: root

    readonly property var d: Detail.cloudCover

    title: qsTr("Cloud cover")
    status: d.status
    trend: d.trend
    body: d.body

    content: Item {
        id: viz

        // The same dial as UV and air quality: 310° from 115°, so the two ends
        // of the scale sit either side of a 50° mouth at the bottom. Three
        // dials in one grid drawn three different ways read as three separate
        // takes on the idea, so this card gave up its own 360°-from-12-o'clock
        // ring. Matching numbers rather than merely similar ones is what makes
        // the three rings the same size on screen.
        readonly property real startAngle: 115
        readonly property real sweepAngle: 310
        readonly property real ringWidth: 7
        readonly property real markSize: 14
        readonly property real rim: markSize / 2 + 1

        readonly property real reading: ChartMath.clamp(root.d.value / 100, 0, 1)

        // The head of the paint, running 0 → `reading` once on mount off the
        // shell's `reveal` hook, over `Theme.motion.reveal` — the same two
        // lines as the UV and air-quality dials, which is the point: three
        // rings with one geometry that arrived three different ways would read
        // as three authors. The long version of why is in DetailUvCard.qml.
        //
        // This replaces a `Behavior on t` at a literal 190 ms. That was not the
        // arrival and it was not anything else either: `reading` is computed
        // from provider data that is fixed for the life of the process, so the
        // Behavior could never once fire. It was a transition written for a
        // state change this card does not have, at a duration that was not a
        // token. An 8% ring drawing itself in is the motion that was wanted.
        readonly property real t: reading * root.reveal

        // Ring colour comes off the cloud ramp, so it is the reading and not a
        // decoration: p = 0 is the top of the value axis, so a clear sky picks
        // up the deep sky-blue end and an overcast one the near-white end.
        //
        // Sampled at the reading, not at the sweeping head. The band dials
        // change colour along their arc because their colour *is* the scale;
        // here it is a property of the one value being drawn, and a ring that
        // shifted hue while it grew would be animating a number that is not
        // changing.
        readonly property color coverInk:
            ChartMath.sampleRamp(Theme.ramp.cloud.line, 1 - reading)

        // The dial reaches a full radius above its centre but only sin(115°)
        // below, because of the mouth. Fit the shape it actually is.
        readonly property real reachDown: Math.sin(startAngle * Math.PI / 180)
        readonly property real radius: Math.max(0, Math.min((height - rim * 2) / (1 + reachDown),
                                                            (width - rim * 2) / 2))
        readonly property real cx: width / 2
        readonly property real cy: rim + radius

        readonly property real markAngle: startAngle + t * sweepAngle

        // Angles run clockwise from 3 o'clock, y down, as in SVG.
        function dialX(deg, r) { return cx + r * Math.cos(deg * Math.PI / 180) }
        function dialY(deg, r) { return cy + r * Math.sin(deg * Math.PI / 180) }

        Shape {
            anchors.fill: parent
            preferredRendererType: Shape.CurveRenderer

            // The unfilled remainder — the sky that is still clear. A gauge
            // only means something as a fraction of something, and this is the
            // something.
            ShapePath {
                fillColor: "transparent"
                strokeColor: Theme.color.trackLine
                strokeWidth: viz.ringWidth
                capStyle: ShapePath.RoundCap
                PathAngleArc {
                    centerX: viz.cx; centerY: viz.cy
                    radiusX: viz.radius; radiusY: viz.radius
                    startAngle: viz.startAngle; sweepAngle: viz.sweepAngle
                    moveToStart: true
                }
            }

            // The covered stretch. Flat caps, not round: a round cap adds half
            // a stroke width at each end, which on an 8% arc is another two and
            // a half points of cloud the sky does not have.
            ShapePath {
                fillColor: "transparent"
                strokeColor: viz.coverInk
                strokeWidth: viz.ringWidth
                capStyle: ShapePath.FlatCap
                PathAngleArc {
                    centerX: viz.cx; centerY: viz.cy
                    radiusX: viz.radius; radiusY: viz.radius
                    startAngle: viz.startAngle
                    sweepAngle: viz.t * viz.sweepAngle
                    moveToStart: true
                }
            }
        }

        // Where the paint stops.
        Rectangle {
            width: viz.markSize; height: viz.markSize; radius: width / 2
            color: viz.coverInk
            border.width: 2.5
            border.color: Theme.color.textPrimary
            x: viz.dialX(viz.markAngle, viz.radius) - width / 2
            y: viz.dialY(viz.markAngle, viz.radius) - height / 2
        }

        // In the middle of the dial — see the note in the air-quality card.
        Text {
            text: root.d.value + root.d.unit
            color: Theme.color.textPrimary
            font.pixelSize: Theme.type.reading
            font.bold: true
            x: viz.cx - width / 2
            y: viz.cy - height / 2
        }
    }
}
