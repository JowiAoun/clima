// SPDX-License-Identifier: GPL-3.0-or-later
// Feels-like detail card.
//
// The reading here is a *comparison*, not a number: 30° only means something
// next to the 27° it is being compared with. So the visualisation draws both
// twelve-hour curves and fills the gap between them — the shaded band is the
// part of the reading the thermometer does not show, and it is the only thing
// on the card that would change shape if the two series converged.
//
// It is a sparkline card like Temperature and Pressure, and it uses their
// marker: a 14 px disc with a 2.5 px ring, on the feels-like curve. It used to
// carry a 12 px dot on a 2 px rule down to a 9 px tick on the measured line,
// which composited into a map pin dangling below the curve — a third mark
// spec in a family of three cards, and the ribbon already draws that gap over
// all twelve hours rather than only at this one.
//
// The line colour is the rosy red measured off the reference (#dc626d), close
// enough to the Temperature card to read as "warmth" and far enough — with the
// second curve, the band and the paired readings — never to be mistaken for it.
import QtQuick
import QtQuick.Shapes
import "theme.js" as Theme
import "chartmath.js" as ChartMath
import "detaildata.js" as Detail

DetailCard {
    id: root

    readonly property var d: Detail.feelsLike

    // Local to this visualisation, so they stay out of theme.js.
    readonly property color feelsStroke:     "#dc626d"   // measured, reference
    readonly property color feelsStrokeDim:  "#59dc626d" // forecast: same line, less certainty
    readonly property color feelsInk:        "#eda2ab"   // the line's hue, at label contrast
    readonly property color actualStroke:    "#b3c8d2e6" // the temperature being compared against
    readonly property color actualStrokeDim: "#4dc8d2e6"
    readonly property color gapObserved:     "#80dc626d"
    readonly property color gapForecast:     "#2edc626d"

    title: qsTr("Feels like")
    status: d.status
    trend: d.trend
    body: d.body

    content: Item {
        id: viz

        readonly property var feels: root.d.series
        readonly property var actual: Detail.temperature.series
        readonly property int nowIndex: Detail.nowIndex

        // The comparison curve is the Temperature card's series. Both cover the
        // same twelve hours today and the card goes on assuming they do — but a
        // provider that returned a shorter one should cost the comparison, not
        // the card, so everything below is gated on this rather than indexing
        // off the end of the array and drawing NaN.
        readonly property bool hasActual: actual !== undefined && actual !== null
                                          && actual.length >= 2

        // One scale for both series, or the gap between them is a lie. Padded
        // so neither curve touches an edge and reads as clipped.
        readonly property var bounds: {
            var vals = feels.slice()
            if (hasActual)
                vals = vals.concat(actual)
            return { lo: Math.min.apply(null, vals) - 1,
                     hi: Math.max.apply(null, vals) + 1 }
        }
        readonly property real lo: bounds.lo
        readonly property real hi: bounds.hi

        Item {
            id: chart

            anchors.left: parent.left
            anchors.right: parent.right
            anchors.top: parent.top
            anchors.bottom: valueRow.top
            // The chart ends where the reading begins. It used to hang the
            // reading 6 px outside the content box to buy this room back; the
            // box is the box, so the chart is shorter instead — and it is
            // taller than it was anyway, because the row that used to sit here
            // is gone.
            anchors.bottomMargin: 10

            // Room for the marker's ring at either extreme of the range.
            readonly property real padTop: 8
            readonly property real padBottom: 6

            // Both series cover the same twelve hours, so each spans the full
            // width whatever its sample count.
            function xAt(i, count) {
                var n = (count === undefined ? viz.feels.length : count)
                return n > 1 ? i * (width - 14) / (n - 1) + 7 : width / 2
            }
            function yAt(v) {
                var span = height - padTop - padBottom
                return height - padBottom - (v - viz.lo) / (viz.hi - viz.lo) * span
            }

            function pointsFor(series) {
                var out = []
                for (var i = 0; i < series.length; ++i)
                    out.push({ x: xAt(i, series.length), y: yAt(series[i]) })
                return out
            }

            readonly property var feelsPts: pointsFor(viz.feels)
            readonly property var actualPts: viz.hasActual ? pointsFor(viz.actual) : []

            // "Now" in the comparison series, which is only the same index as
            // ours while the two are the same length.
            readonly property int actualSplit: viz.hasActual
                ? ChartMath.clamp(viz.nowIndex, 0, viz.actual.length - 1) : 0

            // Where "now" falls along the width, for the observed/forecast step
            // in the band's fill.
            readonly property real nowP: width > 0
                ? ChartMath.clamp(xAt(viz.nowIndex) / width, 0.02, 0.96) : 0.5

            Shape {
                anchors.fill: parent
                preferredRendererType: Shape.CurveRenderer

                // The gap. Warm where it has been measured, faint where it is
                // still a forecast.
                ShapePath {
                    strokeColor: "transparent"
                    fillGradient: LinearGradient {
                        x1: 0; y1: 0; x2: chart.width; y2: 0
                        GradientStop { position: 0.0; color: root.gapObserved }
                        GradientStop { position: chart.nowP; color: root.gapObserved }
                        GradientStop { position: chart.nowP + 0.003; color: root.gapForecast }
                        GradientStop { position: 1.0; color: root.gapForecast }
                    }
                    PathSvg {
                        path: viz.hasActual
                              ? ChartMath.smooth(chart.feelsPts, "M")
                                + " " + ChartMath.smooth(ChartMath.reverse(chart.actualPts), "L")
                                + " Z"
                              : ""
                    }
                }

                // The measured temperature: thin and quiet, it is the baseline
                // of the comparison rather than the subject of the card.
                ShapePath {
                    fillColor: "transparent"
                    strokeColor: root.actualStroke
                    strokeWidth: 2
                    capStyle: ShapePath.RoundCap
                    PathSvg {
                        path: viz.hasActual
                              ? ChartMath.smooth(chart.actualPts.slice(0, chart.actualSplit + 1), "M")
                              : ""
                    }
                }
                ShapePath {
                    fillColor: "transparent"
                    strokeColor: root.actualStrokeDim
                    strokeWidth: 2
                    capStyle: ShapePath.RoundCap
                    PathSvg {
                        path: viz.hasActual
                              ? ChartMath.smooth(chart.actualPts.slice(chart.actualSplit), "M")
                              : ""
                    }
                }

                // What it feels like: the observed stretch, then the forecast.
                ShapePath {
                    fillColor: "transparent"
                    strokeColor: root.feelsStroke
                    strokeWidth: 3
                    capStyle: ShapePath.RoundCap
                    PathSvg { path: ChartMath.smooth(chart.feelsPts.slice(0, viz.nowIndex + 1), "M") }
                }
                ShapePath {
                    fillColor: "transparent"
                    strokeColor: root.feelsStrokeDim
                    strokeWidth: 3
                    capStyle: ShapePath.RoundCap
                    PathSvg { path: ChartMath.smooth(chart.feelsPts.slice(viz.nowIndex), "M") }
                }
            }

            // "Now", on the feels-like curve: the grid's one mark, ringed so it
            // stays visible wherever the curve puts it. The measured line gets
            // none of its own — at this gap a second disc lands under this one.
            Rectangle {
                width: 14; height: 14; radius: 7
                color: root.feelsStroke
                border.width: 2.5
                border.color: Theme.color.textPrimary
                x: chart.xAt(viz.nowIndex) - width / 2
                y: chart.yAt(viz.feels[viz.nowIndex]) - height / 2
            }
        }

        // Two co-equal readings on one baseline, so the eye compares them
        // without being asked to. Numbers first: the big digits are what sets
        // the grid's left edge, and a label in front of them pushes this card's
        // reading 60 px right of every other card's.
        //
        // "Dominant factor: humidity" used to sit above this row and left the
        // chart 37 px to draw in. The body sentence underneath already says
        // "due to the humidity", so it was the same fact twice at the cost of
        // the only thing on the card that answers the question.
        Item {
            id: valueRow
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.bottom: parent.bottom
            height: feelsValue.height

            Text {
                id: feelsValue
                text: root.d.value + root.d.unit
                color: Theme.color.textPrimary
                font.pixelSize: Theme.type.readingPair
                font.bold: true
                anchors.left: parent.left
                anchors.bottom: parent.bottom
            }

            // Each label takes its curve's colour, which is what says which
            // number belongs to which line.
            Text {
                id: feelsLabel
                text: qsTr("Feels like")
                color: root.feelsInk
                font.pixelSize: Theme.type.label
                anchors.left: feelsValue.right
                anchors.leftMargin: 6
                anchors.baseline: feelsValue.baseline
            }

            Text {
                id: actualValue
                text: root.d.actual + root.d.unit
                color: Theme.color.textPrimary
                font.pixelSize: Theme.type.readingPair
                font.bold: true
                anchors.left: feelsLabel.right
                anchors.leftMargin: 18
                anchors.baseline: feelsValue.baseline
            }

            Text {
                id: actualLabel
                text: qsTr("Actual")
                color: Theme.color.textMuted
                font.pixelSize: Theme.type.label
                anchors.left: actualValue.right
                anchors.leftMargin: 6
                anchors.baseline: feelsValue.baseline
            }
        }
    }
}
