// SPDX-FileCopyrightText: 2026 Jowi Aoun
// SPDX-License-Identifier: GPL-3.0-or-later
// Health & activities: five verdicts, one per row.
//
// Nothing here is a measurement, so nothing here is a chart. Each row is a
// question the reader already knows the units of — do I need a coat, do I need
// an umbrella — answered in words, with a dot carrying the same answer for the
// eye that is scanning rather than reading.
//
// The dot is the last thing on the row, not the first. A column of coloured
// dots down the left edge would be the strongest thing on the card and would
// be read before the labels that say what each one is about.
import QtQuick
import "theme.js" as Theme
import "detaildata.js" as Detail

Item {
    id: root

    readonly property real rowHeight: 40

    function toneColor(tone) {
        return tone === "poor" ? Theme.color.statusPoor
             : tone === "caution" ? Theme.color.statusCaution
                                  : Theme.color.statusGood
    }

    implicitHeight: list.height
    height: implicitHeight

    Column {
        id: list
        width: parent.width

        Repeater {
            model: Detail.activities

            delegate: Item {
                id: row
                required property int index
                required property var modelData

                width: list.width
                height: root.rowHeight

                Text {
                    text: row.modelData.name
                    color: Theme.color.textPrimary
                    font.pixelSize: Theme.type.status
                    anchors.left: parent.left
                    anchors.verticalCenter: parent.verticalCenter
                }

                Row {
                    spacing: 8
                    anchors.right: parent.right
                    anchors.verticalCenter: parent.verticalCenter

                    Text {
                        text: row.modelData.status
                        color: Theme.color.textMuted
                        font.pixelSize: Theme.type.status
                        anchors.verticalCenter: parent.verticalCenter
                    }

                    Rectangle {
                        width: 9
                        height: 9
                        radius: 4.5
                        color: root.toneColor(row.modelData.tone)
                        anchors.verticalCenter: parent.verticalCenter
                    }
                }

                // Between rows, not under the last one — a rule under the
                // final row would be a line with nothing below it, which reads
                // as a list that has been cut off.
                Rectangle {
                    visible: row.index < Detail.activities.length - 1
                    anchors.left: parent.left
                    anchors.right: parent.right
                    anchors.bottom: parent.bottom
                    height: 1
                    color: Theme.color.gridLineWeak
                }
            }
        }
    }
}
