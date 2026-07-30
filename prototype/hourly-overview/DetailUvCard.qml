// SPDX-License-Identifier: GPL-3.0-or-later
// UV detail card.
//
// The visualisation is the WHO scale itself, bent into a dial: 0 at the
// bottom-left, 11 at the bottom-right, and the index marked on it. Colour is
// the reading here — a UV of 7 is not "a number out of eleven", it is *orange*,
// and the dial says so in the same language every UV forecast uses.
//
// The traversed stretch is drawn as a run of short stroked arcs rather than one
// path, because a ShapePath can gradient-*fill* but not gradient-*stroke*: each
// segment samples `Detail.bands.uv` at its own midpoint, which is the same
// trick SeriesBars uses per bar. Beyond the reading the dial is the plain gauge
// track, so it reads as "reached this far out of eleven" before the eye finds
// the dot — the coloured arc is carrying the value, not decorating it.
import QtQuick
import QtQuick.Shapes
import "theme.js" as Theme
import "chartmath.js" as ChartMath
import "detaildata.js" as Detail

DetailCard {
    id: root

    readonly property var d: Detail.uv

    title: qsTr("UV")
    status: d.status
    trend: d.trend
    body: d.body

    content: Item {
        id: viz

        // Measured off the reference: 310° of dial with the ~50° gap centred on
        // the bottom, so the two ends of the scale sit either side of it.
        readonly property real startAngle: 115
        readonly property real sweepAngle: 310
        readonly property int segments: 44
        readonly property real ringWidth: 7

        // The marker is the widest thing on the dial, so it — not the stroke —
        // sets how much room the ring has to leave around itself. 14/2.5 is the
        // one "now" mark the whole grid uses; this card was the last at 20/3,
        // which also made its ring 3px smaller in radius than the air-quality
        // dial beside it despite identical angles and stroke.
        readonly property real markSize: 14

        // The dial is not vertically symmetric: the gap at the bottom means it
        // reaches a full radius above its centre but only sin(startAngle) below.
        // Fitting it as if it were a circle throws that difference away, so fit
        // the shape it actually is and hang it from the top of the box.
        readonly property real reachDown: Math.sin(startAngle * Math.PI / 180)
        readonly property real rim: markSize / 2 + 1

        readonly property real radius: Math.max(0, Math.min((height - rim * 2) / (1 + reachDown),
                                                            (width - rim * 2) / 2))
        readonly property real cx: width / 2
        readonly property real cy: rim + radius

        // The reading, normalised over the card's own range (7 of 11).
        readonly property real t: ChartMath.clamp(root.d.value / root.d.max, 0, 1)
        readonly property real markAngle: startAngle + t * sweepAngle
        readonly property color markColor: ChartMath.sampleRamp(Detail.bands.uv, t)

        // Angles run clockwise from 3 o'clock, y down, as in SVG.
        function dialX(deg, r) { return cx + r * Math.cos(deg * Math.PI / 180) }
        function dialY(deg, r) { return cy + r * Math.sin(deg * Math.PI / 180) }

        function arcSeg(t0, t1) {
            var a0 = startAngle + t0 * sweepAngle
            var a1 = startAngle + t1 * sweepAngle
            return "M " + dialX(a0, radius).toFixed(2) + " " + dialY(a0, radius).toFixed(2)
                 + " A " + radius.toFixed(2) + " " + radius.toFixed(2)
                 + " 0 0 1 " + dialX(a1, radius).toFixed(2) + " " + dialY(a1, radius).toFixed(2)
        }

        // Only the traversed stretch is banded, so the segment list stops at
        // the reading — and the last segment is cut short to end exactly on it,
        // rather than at the nearest segment boundary. Rounding to a boundary
        // left the paint up to half a segment past the mark, which a 20px mark
        // covered and a 14px one does not.
        readonly property int splitSeg: Math.ceil(t * segments)
        readonly property var reachedSegs: {
            var a = []
            for (var i = 0; i < splitSeg; ++i) a.push(i)
            return a
        }

        Component {
            id: bandSegment

            Shape {
                id: seg
                anchors.fill: parent
                preferredRendererType: Shape.CurveRenderer

                required property int modelData
                readonly property real segFrom: modelData / viz.segments
                readonly property real segTo: Math.min((modelData + 1) / viz.segments, viz.t)
                readonly property real segMid: (segFrom + segTo) / 2

                ShapePath {
                    fillColor: "transparent"
                    strokeColor: ChartMath.sampleRamp(Detail.bands.uv, seg.segMid)
                    strokeWidth: viz.ringWidth
                    capStyle: ShapePath.RoundCap
                    PathSvg { path: viz.arcSeg(seg.segFrom, seg.segTo) }
                }
            }
        }

        // The unfilled remainder, in the gauge track colour.
        //
        // One arc rather than a run of segments, which is what a flat colour
        // buys: there is no ramp left to sample per segment, so the stretch can
        // be a single stroke. That matters beyond tidiness. It was previously
        // the same coloured segments dimmed as a group — dimmed as a group
        // because consecutive round caps overlap by more than half a segment
        // and two 30% caps composite to 51%, so fading them individually made
        // the ring lumpy. Fixing that needed `layer.enabled` on a wrapper Item,
        // which put half the ring through an FBO and left the two halves able
        // to disagree about edge quality. A flat track needs no fade, no layer
        // and no FBO.
        Shape {
            anchors.fill: parent
            preferredRendererType: Shape.CurveRenderer

            ShapePath {
                fillColor: "transparent"
                strokeColor: Theme.color.trackLine
                strokeWidth: viz.ringWidth
                capStyle: ShapePath.RoundCap
                PathAngleArc {
                    centerX: viz.cx; centerY: viz.cy
                    radiusX: viz.radius; radiusY: viz.radius
                    startAngle: viz.markAngle
                    sweepAngle: (1 - viz.t) * viz.sweepAngle
                    moveToStart: true
                }
            }
        }

        // The scale up to the reading, at full strength, over the track.
        Repeater {
            model: viz.reachedSegs
            delegate: bandSegment
        }

        // The reading, ringed in white so it stays legible over any band.
        Rectangle {
            width: viz.markSize
            height: viz.markSize
            radius: width / 2
            color: viz.markColor
            border.width: 2.5
            border.color: Theme.color.textPrimary
            x: viz.dialX(viz.markAngle, viz.radius) - width / 2
            y: viz.dialY(viz.markAngle, viz.radius) - height / 2
        }

        Text {
            text: root.d.value
            color: Theme.color.textPrimary
            font.pixelSize: Theme.type.reading
            font.bold: true
            x: viz.cx - width / 2
            y: viz.cy - height / 2
        }
    }
}
