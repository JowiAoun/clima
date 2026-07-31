// SPDX-License-Identifier: GPL-3.0-or-later
// Hourly: the chart, and the controls a phone has room for.
//
//   week strip     which day
//   reading        what it is doing now, and which metric the chart draws
//   the chart      HourlyOverview, unchanged
//   daily summary  the same day in a sentence
//
// The chart is the desktop's card, not a phone-sized rewrite of it. Everything
// that made it worth building — the metric-driven axis, the gradient keyed to
// the value, the past veiled and hatched rather than hidden, the feels-like
// morph — is width-independent, and the two things that are not are the column
// width and the plot height, which is why both are now properties on it.
//
// What the phone changes is the *control*: ten pills become one button and a
// list. See MobileMetricPicker for why.
//
// ---- what this does not do ---------------------------------------------------
// Selecting a different day moves the strip and leaves the chart alone. That
// is the same gap the desktop has and it is honest here for the same reason —
// there is one day of hourly data behind it. The strip is wired, the data is
// not; when a provider arrives, this is one binding.
import QtQuick
import "theme.js" as Theme
import "detaildata.js" as Detail
import "mockdata.js" as Data

MobilePage {
    id: root

    property string metricId: "overview"
    property bool listView: false
    property alias dayIndex: week.currentIndex
    property alias feelsLike: chart.feelsLike

    // The shell owns both, so a metric picked here survives a trip to the map
    // and back. See MobileShell's note on why these travel as requests rather
    // than as bindings.
    signal metricRequested(string id)
    signal dayRequested(int index)

    onDayIndexChanged: root.dayRequested(dayIndex)

    MobileWeekStrip {
        id: week
        width: parent.width
    }

    // The reading and the metric button share a row, and the row is lifted
    // above its siblings so the picker's list paints over the chart rather
    // than under it. Later children in a Column paint on top by default, and
    // a dropdown that opens *behind* the thing it configures is the whole
    // failure mode of building a menu without a popup layer.
    Item {
        id: readingRow
        width: parent.width
        height: 44
        z: 10

        WeatherGlyph {
            id: nowGlyph
            kind: Data.conditionFor(Data.nowIndex)
            glyphSize: 34
            anchors.left: parent.left
            anchors.verticalCenter: parent.verticalCenter
        }

        Text {
            id: nowTemp
            text: Math.round(Data.temperature[Data.nowIndex]) + "°"
            color: Theme.color.textPrimary
            font.pixelSize: Theme.type.heroCaption
            anchors.left: nowGlyph.right
            anchors.leftMargin: 8
            anchors.verticalCenter: parent.verticalCenter
        }

        Text {
            text: qsTr("Feels like %1°").arg(Math.round(Data.apparent[Data.nowIndex]))
            color: Theme.color.textMuted
            font.pixelSize: Theme.type.label
            anchors.left: nowTemp.right
            anchors.leftMargin: 10
            anchors.baseline: nowTemp.baseline
        }

        MobileMetricPicker {
            id: picker
            currentId: root.metricId
            anchors.right: parent.right
            anchors.verticalCenter: parent.verticalCenter
            onPicked: function (id) { root.metricRequested(id) }
        }
    }

    // Which day the chart is of. On the desktop the page said this before you
    // ever reached the chart; here the screen is arrived at from a tab bar, so
    // it has to say so itself.
    Text {
        width: parent.width
        text: Detail.observedAt + ", " + Detail.observedOn
        color: Theme.color.textMuted
        font.pixelSize: Theme.type.label
        elide: Text.ElideRight
    }

    HourlyOverview {
        id: chart
        width: parent.width
        metricId: root.metricId
        listView: root.listView

        // 40 px columns rather than the token's 48: at 362 px wide, minus the
        // 40 px value gutter, 48 shows six hours and 40 shows eight. Eight is
        // the difference between reading a morning and reading a slice of one.
        hourWidth: 40

        // The plot gives up 72 px. It is the only part of the card that can:
        // the header band and the precipitation strip are rows of fixed-height
        // content that would become illegible rather than shorter.
        preferredPlotHeight: 180
    }

    MobileCard {
        width: parent.width
        title: qsTr("Daily summary")
        content: Item {
            id: summary
            implicitHeight: summaryText.y + summaryText.height

            WeatherGlyph {
                id: summaryGlyph
                kind: Data.days[root.dayIndex].icon
                glyphSize: 34
                anchors.left: parent.left
            }

            Text {
                id: summaryHigh
                text: Data.days[root.dayIndex].high + "°"
                color: Theme.color.textPrimary
                font.pixelSize: Theme.type.readingPair
                font.bold: true
                anchors.left: summaryGlyph.right
                anchors.leftMargin: 12
                anchors.verticalCenter: summaryGlyph.verticalCenter
            }

            Rectangle {
                id: divider
                width: 1
                height: 20
                color: Theme.color.gridLine
                anchors.left: summaryHigh.right
                anchors.leftMargin: 12
                anchors.verticalCenter: summaryGlyph.verticalCenter
            }

            Text {
                text: Data.days[root.dayIndex].low + "°"
                color: Theme.color.textMuted
                font.pixelSize: Theme.type.readingPair
                anchors.left: divider.right
                anchors.leftMargin: 12
                anchors.verticalCenter: summaryGlyph.verticalCenter
            }

            Text {
                id: summaryText
                text: Detail.temperature.body
                color: Theme.color.textMuted
                font.pixelSize: Theme.type.body
                wrapMode: Text.WordWrap
                width: parent.width
                y: summaryGlyph.height + 12
            }
        }
    }
}
