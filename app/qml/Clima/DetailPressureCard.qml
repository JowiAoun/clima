// SPDX-FileCopyrightText: 2026 Jowi Aoun
// SPDX-License-Identifier: GPL-3.0-or-later
// Pressure detail card.
//
// Twelve hours of barometric pressure as a sparkline with the reading beneath
// it — the same anatomy as Temperature and Feels like, deliberately. Three
// sparkline cards that disagree about their marker size and their fill read as
// three chart libraries sharing a grid, which is what the last review found
// here: this card's "now" dot was 20 px against the exemplar's 14, the single
// heaviest mark in the twelve. One marker, one fill, one reading size.
//
// The wash under the curve is not a claim that the area means anything —
// nobody adds up an afternoon of millibars. Its baseline is the floor of the
// chart box rather than zero pressure, so there is no quantity there to
// misread; it is weight beneath the line, and it is what stops this card
// reading a stop lighter than the two beside it.
//
// The curve is scaled against the day's published range (`d.min`..`d.max`)
// rather than against the series' own extremes. A curve auto-fitted to a 5 mb
// wobble would fill the box and claim a swing the weather did not have; held
// against the real range, a slow rise looks like a slow rise.
//
// On arrival it draws itself in from the left, the same gesture and the same
// token as the other two sparkline cards — three cards that agree about their
// marker and their fill and then arrive three different ways are still three
// chart libraries sharing a grid.
import QtQuick
import QtQuick.Shapes
import "theme.js" as Theme
import "chartmath.js" as ChartMath
import "detaildata.js" as Detail

DetailCard {
    id: root

    readonly property var d: Detail.pressure

    // Measured off the reference's PressureCardGradient: the observed stretch
    // runs pale blue at the oldest hour into violet at "now". These belong to
    // this one visualisation, so they live here and not in theme.js.
    readonly property color lineStart: "#96c6fa"
    readonly property color lineEnd:   "#a375ff"
    // The forecast stretch — the same line with the certainty taken out of it.
    readonly property color lineAhead: "#59c3b4f2"
    // The wash, mid-way between the two ends of the line so it belongs to the
    // whole curve rather than to either half of it. Carried at a higher alpha
    // than the exemplar's because it is a cool wash on a cool page: at the
    // exemplar's 0.30 the same fill measured the same but read half as present
    // as its warm orange does against this gradient.
    readonly property color washTop:   "#669d9dfd"
    readonly property color washFoot:  "#009d9dfd"

    title: qsTr("Pressure")
    status: d.status
    trend: d.trend
    body: d.body

    content: Item {
        id: viz

        readonly property var series: root.d.series
        readonly property int nowIndex: Detail.nowIndex

        // The line occupies the upper half; the reading sits in the lower.
        // Same split as the exemplar, so the two sparklines sit at the same
        // height when the grid puts them side by side.
        readonly property real lineTop: 6
        readonly property real lineBottom: height * 0.52

        readonly property real lo: root.d.min
        readonly property real hi: root.d.max

        // Half the stroke, so the ends and the round caps stay inside the box.
        readonly property real halfW: 3.5

        function xAt(i) { return i * (width - 16) / (series.length - 1) + 8 }
        function yAt(v) {
            return lineBottom - (v - lo) / (hi - lo) * (lineBottom - lineTop)
        }

        readonly property var pts: {
            var out = []
            for (var i = 0; i < series.length; ++i)
                out.push({ x: xAt(i), y: yAt(series[i]) })
            return out
        }

        readonly property real nowX: xAt(nowIndex)
        readonly property real nowY: yAt(series[nowIndex])

        // Where "now" falls along the sweep. The drawing edge is at
        // `width * reveal`, so comparing `reveal` against this is the same
        // instant the line reaches the mark.
        readonly property real nowP: width > 0 ? nowX / width : 1

        // The sweep: a window over the chart whose right edge travels left to
        // right on `reveal`, so the ribbon, its wash and the dimmed forecast
        // all appear in the order the hours did.
        //
        // A clip rather than a regenerated path — re-splining a growing subset
        // of the points would shift the control points of the stretch already
        // drawn, and the ribbon (two offset copies of the curve) would wriggle
        // twice over. `layer.enabled` is what makes the clip bite: Shapes
        // escape ancestor clipping (docs/10-design-system.md §10.8), and a
        // child outside the layer's texture is never drawn into it.
        Item {
            id: sweep

            // Never zero: a layer with no area is a texture Qt has to complain
            // about, and at 1 px nothing of the curve is inside it anyway.
            width: Math.max(1, viz.width * root.reveal)
            height: viz.height
            clip: true
            layer.enabled: true

            Shape {
                width: viz.width
                height: viz.height
                preferredRendererType: Shape.CurveRenderer

                // Area under the curve, fading out downward so it reads as weight
                // beneath the line rather than as a second shape with an edge.
                ShapePath {
                    strokeColor: "transparent"
                    fillGradient: LinearGradient {
                        x1: 0; y1: viz.lineTop; x2: 0; y2: viz.lineBottom
                        GradientStop { position: 0.0; color: root.washTop }
                        GradientStop { position: 1.0; color: root.washFoot }
                    }
                    PathSvg { path: ChartMath.areaPath(viz.pts, viz.lineBottom) }
                }

                // Forecast first, so the "now" dot lands on top of its blunt start.
                ShapePath {
                    fillColor: "transparent"
                    strokeColor: root.lineAhead
                    strokeWidth: viz.halfW * 2
                    capStyle: ShapePath.RoundCap
                    joinStyle: ShapePath.RoundJoin
                    PathSvg { path: ChartMath.smooth(viz.pts.slice(viz.nowIndex), "M") }
                }

                // The observed stretch. ShapePath cannot gradient-*stroke*, so the
                // line is a thin closed ribbon around the curve, filled with the
                // blue-to-violet ramp along its length.
                ShapePath {
                    strokeColor: "transparent"
                    fillGradient: LinearGradient {
                        x1: viz.xAt(0); y1: 0; x2: viz.nowX; y2: 0
                        GradientStop { position: 0.0; color: root.lineStart }
                        GradientStop { position: 1.0; color: root.lineEnd }
                    }
                    PathSvg {
                        path: ChartMath.ribbonPath(viz.pts.slice(0, viz.nowIndex + 1),
                                                   viz.halfW)
                    }
                }
            }

            // The ribbon has square ends; this is the round cap the oldest hour
            // would have had if the stroke could carry the gradient itself. It
            // belongs to the line, so it is inside the sweep and arrives with it
            // rather than sitting on an empty chart waiting to be joined.
            Rectangle {
                width: viz.halfW * 2
                height: width
                radius: width / 2
                color: root.lineStart
                x: viz.xAt(0) - width / 2
                y: viz.yAt(viz.series[0]) - height / 2
            }
        }

        // "Now", ringed so it stays visible wherever the line puts it. 14 with
        // a 2.5 ring — the one mark every card in the grid uses.
        //
        // Outside the sweep, and grown in when the drawing edge passes it: a
        // mark sitting there before the line arrives is pointing at nothing,
        // and one inside the window gets sliced in half on the way past.
        Rectangle {
            width: 14; height: 14; radius: 7
            color: root.lineEnd
            border.width: 2.5
            border.color: Theme.color.textPrimary
            x: viz.nowX - width / 2
            y: viz.nowY - height / 2

            scale: root.reveal >= viz.nowP ? 1 : 0
            Behavior on scale {
                NumberAnimation {
                    duration: Theme.motion.move
                    easing.type: Easing.OutCubic
                }
            }
        }

        Text {
            id: valueText
            text: root.d.value
            color: Theme.color.textPrimary
            font.pixelSize: Theme.type.reading
            font.bold: true
            // Bottom-left, where the other eleven put their reading.
            anchors.left: parent.left
            anchors.bottom: parent.bottom
        }

        // The unit, as the one label beside the reading. `d.at` — "12:28 PM
        // (Now)" — used to be stacked under it, and it is two facts the card
        // already carries: the observation time is the page's, identical on all
        // twelve cards, and "now" is what the ringed mark on the curve says.
        Text {
            text: root.d.unit
            color: Theme.color.textMuted
            font.pixelSize: Theme.type.label
            anchors.left: valueText.right
            anchors.leftMargin: 6
            anchors.baseline: valueText.baseline
        }
    }
}
