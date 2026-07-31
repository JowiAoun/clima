// SPDX-FileCopyrightText: 2026 Jowi Aoun
// SPDX-License-Identifier: GPL-3.0-or-later
// Precipitation-probability strip beneath the plot. One cell per label interval,
// value = the max probability in that interval. Hours already in the past are
// hatched rather than blank, so "no forecast here" reads as deliberate.
import QtQuick
import "mockdata.js" as Data

Item {
    id: root

    property real hourWidth: Theme.metric.hourWidth
    property real nowX: 0
    property real contentWidth: width

    function xForIndex(i) { return i * hourWidth }

    Rectangle {
        anchors.fill: parent
        radius: Theme.metric.controlRadius
        color: Theme.color.stripBg
    }

    Repeater {
        model: Data.precipBuckets()

        delegate: Item {
            required property var modelData
            required property int index

            x: root.xForIndex(modelData.index)
            width: Math.min(modelData.span * root.hourWidth, root.contentWidth - x)
            height: root.height

            readonly property bool past: (x + width / 2) < root.nowX

            Rectangle {
                visible: index > 0
                width: 1
                height: parent.height * 0.5
                anchors.verticalCenter: parent.verticalCenter
                color: Theme.color.stripDivider
            }

            Row {
                visible: !parent.past
                anchors.centerIn: parent
                spacing: 4

                DropletGlyph {
                    glyphSize: 11
                    anchors.verticalCenter: parent.verticalCenter
                }

                Text {
                    text: modelData.prob + "%"
                    color: Theme.color.textPrimary
                    font.pixelSize: 11
                    anchors.verticalCenter: parent.verticalCenter
                }
            }
        }
    }

    // ---- past region -----------------------------------------------------
    Item {
        width: Math.max(0, root.nowX)
        height: root.height
        visible: width > 0

        Rectangle {
            anchors.fill: parent
            radius: Theme.metric.controlRadius
            color: Theme.color.stripPast
        }
        Rectangle {                       // square off the right edge again
            anchors.right: parent.right
            width: Math.min(Theme.metric.controlRadius, parent.width)
            height: parent.height
            color: Theme.color.stripPast
        }
        HatchPattern {
            anchors.fill: parent
            spacing: 7
            lineColor: Theme.color.pastHatch
        }
    }
}
