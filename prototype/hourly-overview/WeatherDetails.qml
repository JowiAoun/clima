// SPDX-License-Identifier: GPL-3.0-or-later
// The weather-detail grid: twelve cards, one per measurable.
//
// The cards are deliberately uniform — same box, same anatomy, same rhythm —
// so the eye can sweep the grid and land only where a value is unusual. Twelve
// differently-shaped cards would each demand to be read.
//
// The grid lays out in full and reports its height; it does not scroll itself.
// It used to, which was right while it was the only thing on screen and wrong
// the moment it became one section of a scrolling page: a scroll area inside a
// scroll area gives the reader two things to drag and no way to tell which one
// they got. The page owns the scrolling, and with it the layer that keeps these
// cards' Shapes inside the viewport — see docs/10-design-system.md §10.8.
import QtQuick
import "theme.js" as Theme
import "detaildata.js" as Detail

Item {
    id: root

    readonly property int columns: Math.max(1, Math.floor(
        (width + Theme.metric.detailGap)
        / (Theme.metric.detailCardWidth + Theme.metric.detailGap)))

    implicitHeight: grid.y + grid.height
    height: implicitHeight

    SectionHeader {
        id: header
        title: qsTr("Weather details")
        stamp: Detail.observedAt
    }

    Grid {
        id: grid
        anchors.top: header.bottom
        anchors.topMargin: 14
        anchors.left: parent.left
        columns: root.columns
        spacing: Theme.metric.detailGap

        Repeater {
            // Fixed order, so the layout does not depend on key order.
            model: ["Temperature", "FeelsLike", "CloudCover", "Precipitation",
                    "Wind", "Humidity", "Uv", "AirQuality", "Visibility",
                    "Pressure", "Sun", "Moon"]

            delegate: Loader {
                required property string modelData
                source: "Detail" + modelData + "Card.qml"
                // A card that fails to load leaves a hole rather than
                // collapsing the grid, so the gap names the culprit.
                onStatusChanged: if (status === Loader.Error)
                    console.warn("details: card failed to load —", source)

                Rectangle {
                    anchors.fill: parent
                    visible: parent.status === Loader.Error
                    radius: Theme.metric.detailRadius
                    color: Theme.color.surfaceRecede
                    Text {
                        anchors.centerIn: parent
                        text: parent.parent.modelData
                        color: Theme.color.textDim
                        font.pixelSize: Theme.type.body
                    }
                }
            }
        }
    }
}
