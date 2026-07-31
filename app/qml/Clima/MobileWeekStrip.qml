// SPDX-FileCopyrightText: 2026 Jowi Aoun
// SPDX-License-Identifier: GPL-3.0-or-later
// The week selector at the top of the Hourly screen.
//
// Seven days across a phone, so a column is about 48 px and everything in it
// has to fit that: a one-letter weekday, the date, and a condition glyph. That
// is the whole reason this is not DayStrip — that component's cards carry a
// high, a low, a day and a night glyph and a tab that grows into the chart
// below, and none of it survives being a seventh of 362 px.
//
// The selected day is marked with a filled pill, not a wash. A 0.10 surface
// over the page would be legible enough on its own, but this strip sits
// directly on the gradient with cards below it, and a faint lighter rectangle
// at the top of the screen reads as a card that failed to draw rather than as
// a selection.
import QtQuick

Item {
    id: root

    // Index into Data.days. The shell owns it so a day chosen here survives a
    // trip to another tab.
    property int currentIndex: Data.todayIndex

    readonly property int count: 7

    // Today first, then the six days after it. Not centred on today: a strip
    // that shows three days of history is three columns spent on the one thing
    // an hourly forecast cannot tell you anything about.
    readonly property var week: Data.days.slice(Data.todayIndex,
                                                Data.todayIndex + count)

    readonly property real cellWidth: width / count

    implicitHeight: 78
    height: implicitHeight

    Row {
        anchors.fill: parent

        Repeater {
            model: root.week

            delegate: Item {
                id: cell
                required property int index
                required property var modelData

                // The model is a slice, so its index is not the index into
                // `days` that the rest of the app speaks in.
                readonly property int dayIndex: Data.todayIndex + index
                readonly property bool isCurrent: dayIndex === root.currentIndex

                width: root.cellWidth
                height: root.height

                Rectangle {
                    anchors.fill: parent
                    anchors.margins: 2
                    radius: Theme.metric.controlRadius
                    color: cell.isCurrent ? Theme.color.surfaceRaised : "transparent"

                    Behavior on color {
                        ColorAnimation { duration: Theme.motion.tint; easing.type: Easing.OutCubic }
                    }
                }

                Column {
                    anchors.centerIn: parent
                    spacing: 3

                    // One letter. Two days in seven share theirs — Saturday
                    // and Sunday, Tuesday and Thursday — which is why the date
                    // under it is not optional.
                    Text {
                        text: cell.modelData.weekday.charAt(0)
                        color: Theme.color.textMuted
                        font.pixelSize: Theme.type.axis
                        anchors.horizontalCenter: parent.horizontalCenter
                    }

                    Text {
                        text: String(cell.modelData.date)
                        color: Theme.color.textPrimary
                        font.pixelSize: Theme.type.status
                        font.bold: cell.isCurrent
                        anchors.horizontalCenter: parent.horizontalCenter
                    }

                    WeatherGlyph {
                        kind: cell.modelData.icon
                        glyphSize: 24
                        anchors.horizontalCenter: parent.horizontalCenter
                    }
                }

                TapHandler {
                    onTapped: root.currentIndex = cell.dayIndex
                }
            }
        }
    }
}
