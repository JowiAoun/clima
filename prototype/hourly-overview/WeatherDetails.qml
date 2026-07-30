// SPDX-License-Identifier: GPL-3.0-or-later
// The weather-detail grid: twelve cards, one per measurable.
//
// The cards are deliberately uniform — same box, same anatomy, same rhythm —
// so the eye can sweep the grid and land only where a value is unusual. Twelve
// differently-shaped cards would each demand to be read.
import QtQuick
import "theme.js" as Theme
import "detaildata.js" as Detail

Item {
    id: root

    readonly property int columns: Math.max(1, Math.floor(
        (width + Theme.metric.detailGap)
        / (Theme.metric.detailCardWidth + Theme.metric.detailGap)))

    Text {
        id: heading
        text: qsTr("Weather details")
        color: Theme.color.textPrimary
        font.pixelSize: Theme.type.cardTitle
        font.bold: true
    }

    Text {
        id: stamp
        text: Detail.observedAt
        color: Theme.color.textMuted
        font.pixelSize: Theme.type.body
        anchors.left: heading.right
        anchors.leftMargin: 10
        anchors.baseline: heading.baseline
    }

    Flickable {
        anchors.top: heading.bottom
        anchors.topMargin: 14
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        // Qt Quick Shapes ignore ancestor clipping, and every card in this grid
        // draws its visualisation with one — so `clip: true` alone bounds the
        // card rectangles while their charts scroll straight out over the
        // heading. Rendering the viewport through a layer is what actually
        // bounds them, because a child outside the texture is never drawn into
        // it. Safe on a transparent group: source-over compositing is
        // associative, so flattening the cards and then laying the result over
        // the page gradient gives the same pixels as compositing each in turn.
        clip: true
        layer.enabled: true
        contentWidth: width
        contentHeight: grid.height
        flickableDirection: Flickable.VerticalFlick
        boundsBehavior: Flickable.StopAtBounds

        Grid {
            id: grid
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
}
