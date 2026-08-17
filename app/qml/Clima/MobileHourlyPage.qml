// SPDX-FileCopyrightText: 2026 Jowi Aoun
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
// ---- the day the chart is of -------------------------------------------------
// The week strip writes `Data.selectedDay` and the chart reads the window that
// moves with it, so picking a day here redraws the chart under it. The reading
// row above it does not move — it says what the weather is doing *now*, which
// is a different question from what the chart is answering, and it asks
// `Data.ahead(0)` rather than the window so that it keeps saying so.
import QtQuick

MobilePage {
    id: root

    property string metricId: "overview"
    property bool listView: false
    property alias dayIndex: week.currentIndex
    property alias feelsLike: chart.feelsLike

    // The one screen under the mobile shell with an animatable chart on it, so
    // it is the one that has to accept `--grab`'s freeze. Same alias the
    // desktop page carries, onto the same component.
    property alias animated: chart.animated

    // The shell owns it, so a metric picked here survives a trip to the map and
    // back. See MobileShell's note on why it travels as a request rather than as
    // a binding — and on why the day does not travel at all: the strip below
    // reads and writes `Data.selectedDay`, which outlives this page by itself.
    signal metricRequested(string id)

    // The selected day, guarded. A live series is as long as the provider sent
    // and the selection outlives the page, so the index can outlive the row it
    // pointed at — MET Norway serves nine and a half days where Open-Meteo
    // serves sixteen, and a fallback that shortened the strip under a selection
    // of 12 would take the page down with it.
    readonly property var day:
        (root.dayIndex >= 0 && root.dayIndex < Data.days.length) ? Data.days[root.dayIndex] : null

    MobileWeekStrip {
        id: week
        width: root.spanWidth(2)
    }

    // The reading and the metric button share a row, and the row is lifted
    // above its siblings so the picker's list paints over the chart rather
    // than under it. Later children in a Column paint on top by default, and
    // a dropdown that opens *behind* the thing it configures is the whole
    // failure mode of building a menu without a popup layer.
    Item {
        id: readingRow
        width: root.spanWidth(2)
        height: 44
        z: 10

        WeatherGlyph {
            id: nowGlyph
            kind: Data.ahead(0).condition
            glyphSize: 34
            anchors.left: parent.left
            anchors.verticalCenter: parent.verticalCenter
        }

        Text {
            id: nowTemp
            text: Units.formatDisplay(Units.Temperature, Data.ahead(0).temperature)
            color: Theme.ink.primary
            font.pixelSize: Theme.type.heroCaption
            anchors.left: nowGlyph.right
            anchors.leftMargin: 8
            anchors.verticalCenter: parent.verticalCenter
        }

        Text {
            text: qsTr("Feels like %1").arg(Units.formatDisplay(Units.Temperature, Data.ahead(0).apparent))
            color: Theme.ink.muted
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
    // it has to say so itself — and now that the strip above actually moves the
    // window, "which day" is a question with more than one answer.
    //
    // Today's window is the one around the present and the observation stamp is
    // what dates it. Any other day is a date, and its weekday and number are
    // what name it; the stamp would be a time from a different day entirely.
    Text {
        width: root.spanWidth(2)
        text: (Data.nowInWindow || !root.day)
              ? Detail.observedAt + ", " + Detail.observedOn
              : root.day.weekday + " " + root.day.date
        color: Theme.ink.muted
        font.pixelSize: Theme.type.label
        elide: Text.ElideRight
    }

    HourlyOverview {
        id: chart
        width: root.spanWidth(2)
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
        // One column, not two. The body is a paragraph, and a paragraph
        // set across both columns of a landscape tablet is a 95-character
        // measure — half again the widest line typography has ever called
        // comfortable. It leaves the right column empty under the chart,
        // which is what a page with one card left in it looks like.
        width: root.spanWidth(1)
        title: qsTr("Daily summary")
        content: Item {
            id: summary
            implicitHeight: summaryText.y + summaryText.height

            WeatherGlyph {
                id: summaryGlyph
                kind: root.day ? root.day.icon : ""
                glyphSize: 34
                anchors.left: parent.left
            }

            Text {
                id: summaryHigh
                text: root.day ? Units.formatDisplay(Units.Temperature, root.day.high) : "—"
                color: Theme.ink.primary
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
                color: Theme.line.grid
                anchors.left: summaryHigh.right
                anchors.leftMargin: 12
                anchors.verticalCenter: summaryGlyph.verticalCenter
            }

            Text {
                text: root.day ? Units.formatDisplay(Units.Temperature, root.day.low) : "—"
                color: Theme.ink.muted
                font.pixelSize: Theme.type.readingPair
                anchors.left: divider.right
                anchors.leftMargin: 12
                anchors.verticalCenter: summaryGlyph.verticalCenter
            }

            // Today's sentence, and only under today's numbers.
            //
            // The glyph and the pair above come from the selected day and
            // always did; this line comes from `Detail`, which is the twelve
            // detail cards' view model and is built entirely around the present
            // — "Peaks at 4:00 p.m." is a claim about today. Under a high and
            // low read off Monday it is a claim about Monday, and a wrong one.
            //
            // Hidden rather than rewritten because there is nothing honest to
            // put here yet: a day's peak hour is in the hourly series and
            // `Detail` does not window it. That is the day-scoped detail work
            // in docs/known-gaps.md, not a line of QML.
            Text {
                id: summaryText
                visible: Data.nowInWindow
                height: visible ? implicitHeight : 0
                text: Detail.temperature.body
                color: Theme.ink.muted
                font.pixelSize: Theme.type.body
                wrapMode: Text.WordWrap
                width: parent.width
                y: summaryGlyph.height + 12
            }
        }
    }
}
