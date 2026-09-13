// SPDX-FileCopyrightText: 2026 Jowi Aoun
// SPDX-License-Identifier: GPL-3.0-or-later
// The ten-day strip on the Today screen.
//
// One column per day: what it is called, what it looks like, its high over its
// low, and the chance of rain. The desktop's DayStrip is a different component
// and deliberately so — that one is a *control*, with a selected card that
// grows a tab into the chart below it. This is a readout. Nothing here is
// selectable, because there is nothing on this screen for a selection to
// change.
//
// Highs are bold and lows are not. The pair is the reading, and a reader
// scanning ten columns for "when does it get hot" should be able to follow one
// row of numbers without the other competing.
import QtQuick

Item {
    id: root

    // 72 on a phone, where ten of them are wider than the screen and the strip
    // scrolls. On a tablet ten columns of 72 is 720 px inside a card that may
    // be 1008 wide, and a row of days that stops two thirds of the way across
    // reads as a strip that failed to load the rest — so past the point where
    // the whole week fits, the columns take the room instead of leaving it.
    //
    // A floor and not a fixed width, so the phone is untouched: at 362 px this
    // is max(72, 36) and the strip flicks exactly as it did.
    readonly property real columnWidth:
        Math.max(72, root.width / Math.max(1, forecast.length))

    // Today and the nine days after it. `days` also carries yesterday, which
    // belongs on the desktop strip — where the reader can page backwards — and
    // not on a card called "10 Day".
    readonly property var forecast: Data.days.slice(Data.todayIndex,
                                                    Data.todayIndex + 10)

    readonly property real contentWidth: forecast.length * columnWidth

    implicitHeight: 152
    height: implicitHeight

    Flickable {
        id: scroll
        anchors.fill: parent
        clip: true
        layer.enabled: true       // bounds the condition glyphs' Shapes, §10.8

        contentWidth: root.contentWidth
        contentHeight: height
        flickableDirection: Flickable.HorizontalFlick
        boundsBehavior: Flickable.StopAtBounds

        Row {
            Repeater {
                model: root.forecast

                delegate: Item {
                    id: day
                    required property int index
                    required property var modelData

                    readonly property bool isToday: index === 0

                    width: root.columnWidth
                    height: scroll.height

                    Column {
                        anchors.horizontalCenter: parent.horizontalCenter
                        anchors.top: parent.top
                        spacing: 7

                        // The day, then its date. Two lines rather than one,
                        // because "Thu 6" on one line at this column width is
                        // either elided or a smaller size than every other
                        // label on the screen.
                        Text {
                            text: day.isToday ? qsTr("Today") : day.modelData.label
                            color: Theme.ink.primary
                            font.pixelSize: Theme.type.label
                            font.bold: day.isToday
                            anchors.horizontalCenter: parent.horizontalCenter
                        }

                        // Today keeps its date too. Hiding it looks tidier in
                        // isolation and is wrong in a row: a Column skips an
                        // invisible child, so the first column's icon, high and
                        // low all rode 20 px above the nine beside them.
                        Text {
                            text: String(day.modelData.date)
                            color: Theme.ink.muted
                            font.pixelSize: Theme.type.axis
                            anchors.horizontalCenter: parent.horizontalCenter
                        }

                        WeatherGlyph {
                            kind: day.modelData.icon
                            glyphSize: 32
                            anchors.horizontalCenter: parent.horizontalCenter
                        }

                        Text {
                            text: Units.formatDisplay(Units.Temperature, day.modelData.high)
                            color: Theme.ink.primary
                            font.pixelSize: Theme.type.status
                            font.bold: true
                            anchors.horizontalCenter: parent.horizontalCenter
                        }

                        Text {
                            text: Units.formatDisplay(Units.Temperature, day.modelData.low)
                            color: Theme.ink.muted
                            font.pixelSize: Theme.type.status
                            anchors.horizontalCenter: parent.horizontalCenter
                        }

                        Row {
                            spacing: 3
                            anchors.horizontalCenter: parent.horizontalCenter

                            DropletGlyph {
                                glyphSize: 11
                                anchors.verticalCenter: parent.verticalCenter
                            }

                            Text {
                                text: day.modelData.precip + "%"
                                color: Theme.ink.muted
                                font.pixelSize: Theme.type.axis
                                anchors.verticalCenter: parent.verticalCenter
                            }
                        }
                    }
                }
            }
        }
    }
}
