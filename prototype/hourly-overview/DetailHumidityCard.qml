// SPDX-License-Identifier: GPL-3.0-or-later
// Humidity detail card.
//
// Eight hours as a row of filling gauges: each column is drawn full height in
// the track colour — the whole 0–100% range — with the lower part filled in
// blue to the hour's relative humidity. Reading the array against a fixed 0–100
// scale is what makes it a visualisation rather than a decoration: 45% is a bar
// filled just under halfway, and it would be visibly a different picture at 80%.
//
// The two readings sit to the right at the pair size: relative humidity, and
// the dew point that explains how it feels. Neither is subordinate to the
// other, so neither gets shrunk to make the point.
import QtQuick
import "theme.js" as Theme
import "detaildata.js" as Detail

DetailCard {
    id: root

    readonly property var d: Detail.humidity

    // The fill belongs to this visualisation alone, so it lives here rather
    // than in theme.js (design system §10.1). The empty part of each gauge does
    // not: `trackLine` is the token for exactly that, and a locally tinted
    // track was one more thing for twelve cards to disagree about. The blue
    // cast it used to carry was family resemblance, not meaning.
    readonly property color barFill: "#6e9cff"

    title: qsTr("Humidity")
    status: d.status
    trend: d.trend
    body: d.body

    content: Item {
        id: viz

        readonly property var series: root.d.series
        readonly property int count: series.length

        // Measured off the reference: bars about half as wide as their pitch,
        // so the array reads as a comb rather than as a solid block.
        readonly property real barW: 9
        readonly property real barGap: 7
        readonly property real arrayW: count * barW + (count - 1) * barGap

        // Gauges are scaled over the full 0–100% range, not over the series'
        // own span. Relative humidity has a meaningful zero and a meaningful
        // ceiling; stretching eight readings across the box would make a calm
        // afternoon look like a storm.
        readonly property real trackH: Math.min(112, height)

        Row {
            spacing: viz.barGap
            anchors.verticalCenter: parent.verticalCenter

            Repeater {
                model: viz.series

                Item {
                    id: bar

                    width: viz.barW
                    height: viz.trackH

                    readonly property real barValue: modelData

                    Rectangle {
                        anchors.fill: parent
                        radius: viz.barW / 2
                        color: Theme.color.trackLine
                    }

                    Rectangle {
                        // An hour at 0% has nothing to draw. Clamping the height
                        // up to the bar width instead would print a 9px stub for
                        // a reading that is not there — the same guard
                        // DetailPrecipitationCard puts on its chance strip.
                        visible: bar.barValue > 0
                        width: viz.barW
                        // A hairline floor below that, so a reading small enough
                        // to round to nothing is still drawn as the something it
                        // is. Nothing in the current series comes near it.
                        height: Math.max(2, parent.height * bar.barValue / 100)
                        y: parent.height - height
                        radius: viz.barW / 2
                        color: root.barFill
                    }
                }
            }
        }

        Column {
            spacing: 10
            x: viz.arrayW + 18
            width: viz.width - x
            // Bottom-anchored, not centred: nine of the twelve cards put their
            // reading on the floor of the content box, and a side-by-side layout
            // that centres its readout column instead is what made row 2 the row
            // with no baseline.
            anchors.bottom: parent.bottom

            Column {
                spacing: 0
                width: parent.width

                Text {
                    text: root.d.value + root.d.unit
                    color: Theme.color.textPrimary
                    font.pixelSize: Theme.type.readingPair
                    font.bold: true
                }

                Text {
                    text: qsTr("Relative Humidity")
                    color: Theme.color.textMuted
                    font.pixelSize: Theme.type.label
                    width: parent.width
                    elide: Text.ElideRight
                }
            }

            Column {
                spacing: 0
                width: parent.width

                Text {
                    text: root.d.dewPoint + root.d.dewUnit
                    color: Theme.color.textPrimary
                    font.pixelSize: Theme.type.readingPair
                    font.bold: true
                }

                Text {
                    text: qsTr("Dew point")
                    color: Theme.color.textMuted
                    font.pixelSize: Theme.type.label
                    width: parent.width
                    elide: Text.ElideRight
                }
            }
        }
    }
}
