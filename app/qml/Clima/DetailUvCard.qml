// SPDX-FileCopyrightText: 2026 Jowi Aoun
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
// A card is a `DetailCard { content: Item { id: viz } }`, so everything drawn
// here lives inside a Component and reaches the two ids around it — `root` for
// the card and `viz` for the visualisation — across that boundary. Without this
// pragma neither is resolvable at compile time: qmllint reports every one of
// them as an unqualified access, and qmlcachegen, which is the half that costs
// something, cannot ahead-of-time compile the binding and leaves it to be
// interpreted on every evaluation. That is the first-paint budget in
// docs/03-tech-stack.md §3.4 being spent on lookups the compiler could have
// done.
//
// Bound makes the enclosing scope's ids lexical, which is what they already
// read as. It is safe here because every delegate in this file declares its
// `required property` — that is the one thing Bound takes away, and none of
// these were relying on it.
pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Shapes
import "chartmath.js" as ChartMath

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
        readonly property real reading: ChartMath.clamp(root.d.value / root.d.max, 0, 1)

        // ---- the arrival ---------------------------------------------------
        // `t` is the head of the paint, not the reading: it runs 0 → `reading`
        // once on mount, over `Theme.motion.reveal`, off the shell's `reveal`
        // hook. The scale is what a UV index means, and a band sweeping up it
        // from 0 shows *where on the scale* 7 lands instead of asserting a
        // colour and leaving the reader to place it.
        //
        // Everything downstream of the paint is derived from `t` and so travels
        // with it for free: the untouched remainder of the track starts where
        // the paint stops, and the mark sits on the head, taking the colour of
        // the band it is currently crossing. One property, one gesture — and
        // the same pair of properties in the air-quality and cloud-cover dials,
        // because three rings sharing one geometry should share one arrival.
        //
        // What does *not* move is the number in the middle. §10.6 permits a
        // reading to count up; it is wrong here. The digit is the answer, and a
        // dial that spends half a second saying 0, 2, 4, 6 is a dial that is
        // briefly lying about the UV index — while the arc beside it is already
        // telling the true story. A counted reading also re-centres itself the
        // frame it grows a second digit, which is the air-quality dial's 25
        // sliding sideways under its own ring, and §10.6 does not allow text to
        // slide. The arc carries the journey; the number carries the value.
        // ---- the hover -------------------------------------------------------
        // The badge on this card is worked out against today's peak, and a badge
        // is a direction with no magnitude: it says the index is climbing and
        // never says how far. On hover the head of the paint carries on up to
        // the peak and comes back — and because the mark takes the colour of the
        // band it is crossing, "3 now, 8 at two" is told in the scale's own
        // language instead of only in the sentence underneath.
        //
        // Never below the reading, because a peak is a maximum over the whole
        // day. By late afternoon the two coincide and the dial holds still,
        // which is the correct thing for it to say then.
        readonly property real peakFrac:
            Math.max(reading, ChartMath.clamp(root.d.peak / root.d.max, 0, 1))

        // The value being drawn, before the arrival scales it. Splitting the two
        // is what keeps the hover out of the reveal: at rest `shown` is the
        // reading exactly, so the resting dial is the dial this card has always
        // drawn.
        readonly property real shown: reading + (peakFrac - reading) * root.hoverWalk

        readonly property real t: shown * root.reveal

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
        //
        // Counted off the reading rather than off the sweeping head, so the
        // model is a fixed-length array. A JS-array model that changes length
        // resets the whole Repeater, which during a sweep would tear down and
        // rebuild every Shape on the ring thirty times over. The segments exist
        // from the first frame; each one is *clipped* to the head instead.
        readonly property int splitSeg: Math.ceil(reading * segments)
        readonly property var reachedSegs: {
            var a = []
            for (var i = 0; i < splitSeg; ++i) a.push(i)
            return a
        }

        // ---- the stretch the hover reaches on to -----------------------------
        // A run of its own rather than more of the one above, and that is not
        // tidiness. Extending the run above means moving where its last segment
        // ends, which moves the point that segment's colour is sampled at, which
        // repaints a resting card that nothing asked to change. This run tiles
        // the stretch between the reading and where the hover walks to, exists
        // only when there is one, and is clipped to the head exactly as its
        // neighbour is. At rest the head is the reading, every segment here is
        // empty, and the card is the card it has always been — which is what
        // lets the golden images go on asserting it.
        readonly property var aheadSegs: {
            var span = peakFrac - reading
            if (span <= 0.005)
                return []
            var out = []
            var n = Math.max(2, Math.round(span * segments))
            for (var i = 0; i < n; ++i)
                out.push({ from: reading + span * i / n,
                           to:   reading + span * (i + 1) / n,
                           mid:  reading + span * (i + 0.5) / n })
            return out
        }

        Component {
            id: bandSegment

            Shape {
                id: seg
                anchors.fill: parent
                preferredRendererType: Shape.CurveRenderer

                required property int modelData
                readonly property real segFrom: modelData / viz.segments
                readonly property real segEnd: Math.min((modelData + 1) / viz.segments, viz.reading)
                // The band this segment stands for is a fixed position on the
                // scale, so it is sampled off `segEnd` and not off the head:
                // the run grows, the colours under it do not shift.
                readonly property real segMid: (segFrom + segEnd) / 2

                // Clipped to the sweeping head. Clamped at `segFrom` because an
                // arc that ends before it starts is not an empty arc — SVG
                // takes the long way round and paints 300° of it.
                readonly property real segTo: Math.max(segFrom, Math.min(segEnd, viz.t))
                // Nothing to draw yet. `opacity`, not `visible`: hiding part of
                // a chart subtree corrupts clip state for its neighbours
                // (§10.8), and this is a binding rather than a fade — there is
                // no Behavior on it and it never lands between 0 and 1.
                opacity: segTo > segFrom ? 1 : 0

                ShapePath {
                    fillColor: "transparent"
                    strokeColor: ChartMath.sampleRamp(Detail.bands.uv, seg.segMid)
                    strokeWidth: viz.ringWidth
                    capStyle: ShapePath.RoundCap
                    PathSvg { path: viz.arcSeg(seg.segFrom, seg.segTo) }
                }
            }
        }

        // The unfilled remainder, in the gauge track colour. Anchored at the
        // head, so during the arrival it gives ground to the paint rather than
        // being overdrawn by it: the ring is a complete scale in every frame,
        // including the first one, and the two ends always meet exactly.
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
                strokeColor: Theme.line.track
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

        // The stretch beyond the reading, painted only as far as the hover has
        // walked. Empty at rest.
        Repeater {
            model: viz.aheadSegs

            Shape {
                id: ahead
                anchors.fill: parent
                preferredRendererType: Shape.CurveRenderer

                required property var modelData

                readonly property real segTo: Math.max(modelData.from,
                                                       Math.min(modelData.to, viz.t))
                opacity: segTo > modelData.from ? 1 : 0

                ShapePath {
                    fillColor: "transparent"
                    strokeColor: ChartMath.sampleRamp(Detail.bands.uv, ahead.modelData.mid)
                    strokeWidth: viz.ringWidth
                    capStyle: ShapePath.RoundCap
                    PathSvg { path: viz.arcSeg(ahead.modelData.from, ahead.segTo) }
                }
            }
        }

        // The reading, ringed in white so it stays legible over any band.
        Rectangle {
            width: viz.markSize
            height: viz.markSize
            radius: width / 2
            color: viz.markColor
            border.width: 2.5
            border.color: Theme.ink.primary
            x: viz.dialX(viz.markAngle, viz.radius) - width / 2
            y: viz.dialY(viz.markAngle, viz.radius) - height / 2
        }

        Text {
            text: root.d.value
            color: Theme.ink.primary
            font.pixelSize: Theme.type.reading
            font.bold: true
            x: viz.cx - width / 2
            y: viz.cy - height / 2
        }
    }
}
