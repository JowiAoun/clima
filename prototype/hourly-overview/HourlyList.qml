// SPDX-License-Identifier: GPL-3.0-or-later
// The list alternative to the chart.
//
// A chart answers "what is the shape of the day"; a list answers "what exactly is
// it at 3pm". Both are worth having, which is why the reference carries a switch
// for it — and why leaving that switch inert was the wrong place to stop.
//
// The past is dimmed rather than hidden, and "now" is marked, so the same rule the
// chart follows holds here: observed hours are real data, just not forecast.
import QtQuick
import "theme.js" as Theme
import "mockdata.js" as Data

Item {
    id: root

    readonly property real rowHeight: 42

    Rectangle {
        anchors.fill: parent
        radius: Theme.metric.panelRadius
        color: Theme.color.panelBg
    }

    // Column geometry lives in one place so the header and the rows cannot drift.
    QtObject {
        id: cols
        readonly property real time: 92
        readonly property real icon: 40
        readonly property real condition: 168
        readonly property real temp: 74
        readonly property real feels: 84
        readonly property real precip: 92
        readonly property real wind: 104
        readonly property real humidity: 84
    }

    component Cell: Text {
        property real cellWidth: 80
        width: cellWidth
        color: Theme.color.textPrimary
        font.pixelSize: 12
        horizontalAlignment: Text.AlignRight
        verticalAlignment: Text.AlignVCenter
        height: parent ? parent.height : 0
    }

    component HeaderCell: Text {
        property real cellWidth: 80
        width: cellWidth
        color: Theme.color.textDim
        font.pixelSize: 11
        horizontalAlignment: Text.AlignRight
        verticalAlignment: Text.AlignVCenter
        height: parent ? parent.height : 0
    }

    // ---- header ----------------------------------------------------------
    Item {
        id: header
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: parent.top
        anchors.leftMargin: 18
        anchors.rightMargin: 18
        height: 34

        Row {
            anchors.fill: parent
            spacing: 12

            HeaderCell {
                cellWidth: cols.time + cols.icon + 12
                text: qsTr("Time")
                horizontalAlignment: Text.AlignLeft
            }
            HeaderCell { cellWidth: cols.condition; text: qsTr("Condition"); horizontalAlignment: Text.AlignLeft }
            HeaderCell { cellWidth: cols.temp;      text: qsTr("Temp") }
            HeaderCell { cellWidth: cols.feels;     text: qsTr("Feels like") }
            HeaderCell { cellWidth: cols.precip;    text: qsTr("Precip") }
            HeaderCell { cellWidth: cols.wind;      text: qsTr("Wind") }
            HeaderCell { cellWidth: cols.humidity;  text: qsTr("Humidity") }
        }

        Rectangle {
            anchors.bottom: parent.bottom
            anchors.left: parent.left
            anchors.right: parent.right
            height: 1
            color: Theme.color.gridLine
        }
    }

    // ---- rows ------------------------------------------------------------
    ListView {
        id: view
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: header.bottom
        anchors.bottom: parent.bottom
        anchors.leftMargin: 18
        anchors.rightMargin: 18
        anchors.bottomMargin: 10
        clip: true
        // Qt Quick Shapes escape ancestor clipping: the condition glyphs of
        // out-of-view rows were drawing over the header and past the bottom edge.
        // A layer bounds the whole subtree, which plain clip: true does not.
        layer.enabled: true
        cacheBuffer: 0
        model: Data.count
        boundsBehavior: Flickable.StopAtBounds
        currentIndex: Data.nowIndex

        delegate: Item {
            required property int index

            readonly property bool isNow: index === Data.nowIndex
            readonly property bool isPast: index < Data.nowIndex

            width: view.width
            height: root.rowHeight
            opacity: isPast ? 0.5 : 1

            Rectangle {
                anchors.fill: parent
                anchors.topMargin: 1
                radius: Theme.metric.controlRadius
                color: parent.isNow ? Theme.color.nowRowBg
                                    : (index % 2 === 0 ? "transparent" : Theme.color.listRowAlt)
            }

            Rectangle {
                visible: parent.isNow
                width: 3
                height: parent.height - 10
                radius: 1.5
                anchors.verticalCenter: parent.verticalCenter
                color: Theme.color.accent
            }

            Row {
                anchors.fill: parent
                spacing: 12

                Text {
                    width: cols.time
                    height: parent.height
                    verticalAlignment: Text.AlignVCenter
                    leftPadding: 12
                    text: Data.hourLabel(index)
                    color: Theme.color.textPrimary
                    font.pixelSize: 12
                    font.bold: parent.parent.isNow
                }

                Item {
                    width: cols.icon
                    height: parent.height
                    WeatherGlyph {
                        anchors.centerIn: parent
                        kind: Data.conditionFor(index)
                        glyphSize: 24
                    }
                }

                Text {
                    width: cols.condition
                    height: parent.height
                    verticalAlignment: Text.AlignVCenter
                    text: Data.conditionText(index)
                    color: Theme.color.textMuted
                    font.pixelSize: 12
                }

                Cell { cellWidth: cols.temp;     text: Math.round(Data.temperature[index]) + "°" ; font.bold: true }
                Cell { cellWidth: cols.feels;    text: Math.round(Data.apparent[index]) + "°" ; color: Theme.color.textMuted }
                Cell { cellWidth: cols.precip;   text: Data.precipProb[index] + "%" ; color: Theme.color.droplet }
                Cell { cellWidth: cols.wind;     text: Math.round(Data.windSpeed[index]) + " km/h" ; color: Theme.color.textMuted }
                Cell { cellWidth: cols.humidity; text: Data.humidity[index] + "%" ; color: Theme.color.textMuted }
            }
        }
    }
}
