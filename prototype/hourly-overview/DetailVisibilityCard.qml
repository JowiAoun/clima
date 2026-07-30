// SPDX-License-Identifier: GPL-3.0-or-later
// Visibility detail card — how far you can see, drawn as the distance itself.
//
// One continuous sight line, near end at the left where you are standing,
// tapering away to the ceiling of the scale at the right. It is lit from the
// near end out to the reading and dissolves into the track beyond it, so the
// picture of limited visibility is itself limited visibility.
//
// Why a continuous form rather than the five bars this card used to draw. Those
// bars were a 5-step meter: their widths were five hard-coded numbers, so the
// only thing a new reading could change was which of five colours each bar took.
// 16 km and 19 km drew exactly the same picture, and every value between 12 and
// 16 drew the same picture as 16. Next to Temperature's twelve-point series that
// is a coarse read of a quantity that is not stepped in the first place —
// visibility is a distance, continuous by nature, and the honest drawing of a
// continuous quantity is a continuous one. The taper survives because it is what
// makes the shape read as distance rather than as a bar; what has changed is
// that the taper is now the *scale*, and the lit length is the *reading*, where
// before the taper was fixed decoration and the reading was a colour lookup.
//
// Three other corrections. The reading was scaled against `d.max`, which no
// longer exists and which was today's *peak* distance when it did — 16 km of
// "Excellent" lit two bars of five while the status line said Excellent. The
// ceiling is `d.scaleMax`, which is data and means a ceiling: past 20 km a
// public forecast stops distinguishing. The colour comes from
// `Detail.bands.visibility`, which exists for this card, rather than from a
// five-colour ramp invented here. And the reading is bottom-left, where every
// card puts it except the three dials, which centre theirs inside the ring.
import QtQuick
import QtQuick.Shapes
import "theme.js" as Theme
import "chartmath.js" as ChartMath
import "detaildata.js" as Detail

DetailCard {
    id: root

    readonly property var d: Detail.visibility

    title: qsTr("Visibility")
    status: d.status
    trend: d.trend
    body: d.body

    content: Item {
        id: viz

        // How far the reading gets along the scale the data declares. Clamped,
        // so a distance past the ceiling fills the sight line rather than
        // running off the end of it.
        readonly property real frac: ChartMath.clamp(root.d.value / root.d.scaleMax, 0, 1)

        // One colour for the whole lit stretch, sampled from the published band
        // ramp at the reading: murky green when you can barely see, bright mint
        // when you can see to the ceiling. Colour and length move together.
        readonly property color litColor: ChartMath.sampleRamp(Detail.bands.visibility, frac)

        // ---- the arrival ---------------------------------------------------
        // The sight line extends from where you are standing out to the
        // reading. A distance is read along its length, so pushing it out
        // lengthwise is the same movement the eye makes over it — where a bar
        // "growing" off a baseline would be the wrong axis entirely.
        //
        // The taper behind it does not extend: it is the whole 0–20 km scale,
        // the thing the lit stretch is a fraction of (§10.7), and revealing it
        // alongside the reading would leave the first frames with a lit length
        // and nothing to judge it against.
        //
        // The colour does not sweep either. It is sampled once, at the reading;
        // a colour climbing the band ramp during the arrival would be painting
        // the murky greens of readings nobody took. Length is the reveal;
        // colour is the reading.
        //
        // No stagger and no second clock: there is one shape, so the card's own
        // reveal is the whole animation, and it is eased by `DetailCard`.
        readonly property real litFrac: frac * root.reveal

        // The sight line lives above the reading and uses the width it is given.
        // Thickness is perspective, not data: near ground fills more of the view
        // than far ground does, which is what makes a horizontal bar read as a
        // distance receding rather than as a quantity stacked up.
        readonly property real bandH: Math.max(12, reading.y - 12)
        readonly property real axisY: bandH / 2
        readonly property real hNear: Math.min(15, bandH / 2 - 1)
        readonly property real hFar: Math.max(2, hNear * 0.3)

        // The spine runs between the two cap centres, so the round ends land
        // exactly on the edges of the content box instead of hanging outside it.
        readonly property real x0: hNear
        readonly property real x1: Math.max(x0 + 1, width - hFar)
        readonly property real xVal: x0 + litFrac * (x1 - x0)

        function halfAt(x) {
            return hNear + (hFar - hNear) * (x - x0) / (x1 - x0)
        }

        // A tapering pill between two points on the spine, rounded at both ends.
        // Both arcs sweep the same way because the outline is walked clockwise.
        function sightPath(xa, xb) {
            var ha = halfAt(xa), hb = halfAt(xb)
            var n = ChartMath.n
            return "M " + n(xa) + " " + n(axisY - ha)
                 + " L " + n(xb) + " " + n(axisY - hb)
                 + " A " + n(hb) + " " + n(hb) + " 0 0 1 " + n(xb) + " " + n(axisY + hb)
                 + " L " + n(xa) + " " + n(axisY + ha)
                 + " A " + n(ha) + " " + n(ha) + " 0 0 1 " + n(xa) + " " + n(axisY - ha)
                 + " Z"
        }

        // Where the lit stretch starts giving way. A fixed run, so the boundary
        // stays findable at any value — but never more than half the stretch, or
        // a short reading is nothing but fade and its length stops being legible.
        readonly property real litLength: Math.max(1, xVal - x0)
        readonly property real fadeRun: Math.min(26, litLength * 0.45)
        readonly property real fadeStop: Math.max(0, 1 - fadeRun / litLength)

        Shape {
            anchors.fill: parent
            preferredRendererType: Shape.CurveRenderer

            // The whole scale, under everything: the lit stretch only means
            // something as a fraction of the distance it could have run.
            ShapePath {
                strokeColor: "transparent"
                fillColor: Theme.color.trackLine
                PathSvg { path: viz.sightPath(viz.x0, viz.x1) }
            }

            // As far as you can see, fading out at its own limit rather than
            // stopping at an edge — which is what a visibility limit does. The
            // fade goes to zero alpha, so it is a dissolve and not a second
            // shape laid over the first.
            ShapePath {
                strokeColor: "transparent"
                fillGradient: LinearGradient {
                    x1: viz.x0; y1: 0; x2: viz.xVal; y2: 0
                    GradientStop { position: 0.0; color: viz.litColor }
                    GradientStop { position: viz.fadeStop; color: viz.litColor }
                    GradientStop {
                        position: 1.0
                        color: Qt.rgba(viz.litColor.r, viz.litColor.g, viz.litColor.b, 0)
                    }
                }
                PathSvg { path: viz.litFrac > 0.005 ? viz.sightPath(viz.x0, viz.xVal) : "" }
            }
        }

        // The far end of the scale, so the lit stretch reads as "16 of 20" and
        // not merely as "a bar that is nearly full". An axis label, at axis size
        // and dim: it is the ceiling, not a second reading.
        Text {
            text: root.d.scaleMax + " " + root.d.unit
            color: Theme.color.textDim
            font.pixelSize: Theme.type.axis
            anchors.right: parent.right
            y: viz.axisY + viz.hFar + 8
        }

        // Bottom-left, at the reading size, like the other eleven cards.
        Row {
            id: reading
            spacing: 6
            anchors.left: parent.left
            anchors.bottom: parent.bottom

            Text {
                id: readingValue
                text: root.d.value
                color: Theme.color.textPrimary
                font.pixelSize: Theme.type.reading
                font.bold: true
            }

            Text {
                text: root.d.unit
                color: Theme.color.textMuted
                font.pixelSize: Theme.type.label
                anchors.baseline: readingValue.baseline
            }
        }
    }
}
