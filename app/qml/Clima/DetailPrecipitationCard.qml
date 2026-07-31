// SPDX-FileCopyrightText: 2026 Jowi Aoun
// SPDX-License-Identifier: GPL-3.0-or-later
// Precipitation detail card.
//
// One visualisation: twelve hours of the chance of rain, as columns against a
// 0–100% frame. The card previously carried two — a vessel gauge for the
// amount and this strip — and neither had the room to be drawn properly.
//
// The strip is the one worth keeping. The amount is 0 mm, and a gauge of zero
// draws a scale with nothing in it: true, but it is the reading restated as a
// picture rather than anything the reading does not already say. The chance of
// rain is the fact the reading cannot carry — nothing through the morning,
// then climbing to a third of a chance by late afternoon, which is what the
// body sentence is about. The number says how much; the chart says when.
//
// Columns rather than a curve, because a chance of rain is quoted per hour and
// a smooth line between 0% and 35% claims intermediate values nobody forecast.
// There are no per-column tracks: eight of the twelve hours are 0%, and twelve
// empty grey capsules read as a skeleton waiting for data rather than as a dry
// morning. The frame is horizontal instead — the 0% baseline the columns stand
// on, and the labelled 100% ceiling that makes 35% a third rather than merely
// the tall one.
//
// On arrival the columns grow off that baseline in hour order, so the wave runs
// along the axis the chart is drawn on and the rain reads as arriving. The
// frame does not grow with them: the baseline and the now-rule are what the
// columns are measured against and are there from the first frame. The eight
// dry hours have nothing to grow and so grow nothing — the wave crosses them
// silently and the strip fills in from the far end, which is where the day's
// only chance of rain is.
import QtQuick
import "detaildata.js" as Detail

DetailCard {
    id: root

    readonly property var d: Detail.precipitation

    title: qsTr("Precipitation")
    status: d.status
    trend: d.trend
    body: d.body

    content: Item {
        id: viz

        readonly property var series: root.d.series
        readonly property int count: series.length

        // The forecast stretch of the rain blue: same colour, less certainty
        // (design system §10.7). It belongs to this card alone, so it lives
        // here rather than in theme.js.
        readonly property color forecastInk: "#b37fb6e8"

        readonly property real barW: 14
        readonly property real barGap: count > 1
            ? (width - count * barW) / (count - 1) : 0

        function xAt(i) { return i * (barW + barGap) }

        // The plot runs from the ceiling line down to the baseline. The
        // ceiling sits half a label below the top of the box so its number can
        // be centred on it rather than hang over the edge; the baseline sits
        // just above the reading's line box, which puts it a comfortable
        // sixteen pixels clear of the digits — a 34px line box carries that
        // much leading above the caps — without any of it spent on air.
        readonly property real ceilY: caption.height / 2
        readonly property real baseY: Math.max(ceilY + 10, reading.y - 2)
        readonly property real plotH: baseY - ceilY

        // ---- the arrival wave ----------------------------------------------
        // A column growing is a change of size, so it takes `move`; the gap
        // between one column and the next is `stagger`, the token the grid
        // staggers whole cards by.
        //
        // The wave steps once per column that has something to draw, not once
        // per hour. Stepping over the eight dry hours as well left the strip
        // empty for a quarter of a second after the rest of the card had
        // arrived — filmed, it reads as a chart that failed to load — and it
        // bought nothing, because the four hours that do draw are consecutive
        // and would have rippled either way. Skipping the gaps keeps the wave
        // running along the time axis; the hours with nothing to say simply do
        // not take a turn.
        function drawnRank(i) {
            var r = 0
            for (var k = 0; k < i && k < count; ++k)
                if (series[k] > 0) r++
            return r
        }
        readonly property int drawnCount: drawnRank(count)

        // Tightened when a long enough run of wet hours would push the last
        // column past the card's own reveal, so however the forecast falls the
        // last column has landed within `Theme.motion.reveal` of the first
        // starting — a card that ripples still arrives on the grid's wave
        // rather than trailing a third of a second behind it.
        readonly property int growSpan: Theme.motion.move
        readonly property int growStep: Math.round(Math.min(
            Theme.motion.stagger,
            (Theme.motion.reveal - growSpan) / Math.max(1, drawnCount - 1)))

        // What the columns are, and what they are a fraction of: one at each
        // end of the line they are measured against.
        Text {
            id: caption
            text: qsTr("Chance of rain")
            color: Theme.color.textDim
            font.pixelSize: Theme.type.axis
            anchors.left: parent.left
            y: viz.ceilY - height / 2
        }

        // There is no ceiling rule. The scale is a probability, so 100% is
        // where it ends by definition and needs no announcing — and drawing it
        // cost more than it said: a rule across the top, the baseline across
        // the bottom and the now-line between them closed into three sides of a
        // box, which read as a broken table rather than as a chart. A dry
        // morning is allowed to look empty.
        //
        // `precipitation.scaleMax` is the ceiling for the *amount*, and went out
        // of this card with the amount gauge; a probability's ceiling is not an
        // editorial choice §10.7 would want taken away from the card.

        Repeater {
            model: viz.series

            delegate: Rectangle {
                id: bar

                required property int index
                required property var modelData

                readonly property real barValue: modelData        // 0–100 %
                readonly property bool barIsForecast: index > Detail.nowIndex

                // 0 → 1 once, this column's turn into the card's reveal. A value
                // source rather than a binding on `root.reveal`: the card's
                // reveal is one eased ramp, and slicing an eased ramp into
                // twelve windows gives twelve differently-eased columns crushed
                // into its first third. Nothing restarts it — the card's reveal
                // only ever goes 0 → 1, so the guard latches.
                property real barGrow: 0

                SequentialAnimation on barGrow {
                    running: root.reveal > 0
                    PauseAnimation { duration: viz.drawnRank(bar.index) * viz.growStep }
                    NumberAnimation {
                        to: 1
                        duration: viz.growSpan
                        easing.type: Easing.OutCubic
                    }
                }

                // A floor of 2px so a 5% hour is a mark rather than a rounding
                // error, and still visibly half the 10% beside it — but only
                // for hours that have a chance at all. A stub on a 0% hour
                // would print a zero as the something it is not, and would give
                // the arrival eight columns of nothing to grow.
                visible: barValue > 0
                x: viz.xAt(index)
                width: viz.barW
                height: Math.max(2, viz.plotH * barValue / 100) * bar.barGrow
                y: viz.baseY - height
                radius: 3
                color: barIsForecast ? viz.forecastInk : Theme.color.rainDrop
            }
        }

        // 0%: the line the columns stand on, and the last thing between them
        // and the reading.
        Rectangle {
            x: 0
            y: viz.baseY
            width: viz.width
            height: 1
            color: Theme.color.gridLine
        }

        // Now, as the vertical rule the rest of the prototype uses. The hours
        // to its left are observed and the ones to its right forecast; with
        // every observed hour at nothing, the strip has no past and no future
        // without it. It was a 14px disc on the baseline, which — with a rule
        // running out either side of it — read as a slider handle rather than
        // as the present moment.
        Rectangle {
            x: viz.xAt(Detail.nowIndex) + viz.barW / 2
            y: viz.ceilY
            width: 1
            height: viz.plotH
            color: Theme.color.gridLine
        }

        Item {
            id: reading
            anchors.left: parent.left
            anchors.bottom: parent.bottom
            width: valueText.width + 10 + windowText.width
            height: valueText.height

            Text {
                id: valueText
                text: root.d.value + " " + root.d.unit
                color: Theme.color.textPrimary
                font.pixelSize: Theme.type.reading
                font.bold: true
            }

            // What the zero is a zero *of*. Said nowhere else on the card.
            Text {
                id: windowText
                text: root.d.window
                color: Theme.color.textMuted
                font.pixelSize: Theme.type.label
                anchors.left: valueText.right
                anchors.leftMargin: 10
                anchors.baseline: valueText.baseline
            }
        }
    }
}
