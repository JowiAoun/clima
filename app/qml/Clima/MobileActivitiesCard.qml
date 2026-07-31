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
//
// ---- these are OURS, and the card says so -----------------------------------
//
// Nobody publishes "do you need an umbrella". There is no such product at
// Open-Meteo, at MET Norway or anywhere else in docs/02-data-sources.md, so
// every verdict here is a rule Clima applies to numbers that are already on
// this screen — the rules are written out in
// app/viewmodels/conditionsdata.cpp's buildActivities().
//
// Hence the footnote. docs/08-risks.md R9 is about not fabricating, and a
// derived verdict is not a fabrication — but a derived verdict presented in the
// same voice as a measurement is, whether or not the arithmetic is sound. One
// line is what the difference costs.
import QtQuick

Item {
    id: root

    readonly property real rowHeight: 40

    function toneColor(tone) {
        return tone === "poor" ? Theme.state.poor
             : tone === "caution" ? Theme.state.caution
                                  : Theme.state.good
    }

    implicitHeight: list.height + note.height + 10
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
                    color: Theme.ink.primary
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
                        color: Theme.ink.muted
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
                    color: Theme.line.gridWeak
                }
            }
        }
    }

    Text {
        id: note
        anchors.top: list.bottom
        anchors.topMargin: 10
        width: parent.width
        text: qsTr("Worked out by Clima from the forecast above — not a published forecast "
                 + "product.")
        color: Theme.ink.dim
        font.pixelSize: Theme.type.label
        wrapMode: Text.WordWrap
    }
}
