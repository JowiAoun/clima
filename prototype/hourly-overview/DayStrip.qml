// SPDX-License-Identifier: GPL-3.0-or-later
// The day cards above the chart. The selected card is wider, lighter, and squared
// off at the bottom so it reads as joined to the panel below — the same trick the
// reference uses to say "the chart underneath is showing this day".
import QtQuick
import "theme.js" as Theme
import "mockdata.js" as Data

Item {
    id: root

    property int currentIndex: Data.todayIndex

    readonly property real cardWidth: 168
    readonly property real selectedExtra: 74     // room for the second icon
    readonly property real spacing: 8

    implicitHeight: 126
    height: implicitHeight

    Flickable {
        id: flick
        anchors.fill: parent
        clip: true
        contentWidth: row.width
        contentHeight: height
        flickableDirection: Flickable.HorizontalFlick
        boundsBehavior: Flickable.StopAtBounds

        NumberAnimation {
            id: scrollAnim
            target: flick
            property: "contentX"
            duration: 340
            easing.type: Easing.OutCubic
        }

        function scrollBy(dx) {
            var to = Math.max(0, Math.min(Math.max(0, contentWidth - width), contentX + dx))
            scrollAnim.stop()
            scrollAnim.from = contentX
            scrollAnim.to = to
            scrollAnim.start()
        }

        Row {
            id: row
            spacing: root.spacing
            height: flick.height

            Repeater {
                model: Data.days

                delegate: Item {
                    id: card

                    required property var modelData
                    required property int index

                    readonly property bool selected: index === root.currentIndex

                    width: selected ? root.cardWidth + root.selectedExtra : root.cardWidth
                    height: root.height
                    Behavior on width { NumberAnimation { duration: 180; easing.type: Easing.OutCubic } }

                    Rectangle {
                        anchors.fill: parent
                        radius: Theme.metric.cardRadius
                        color: card.selected ? Theme.color.cardBg : Theme.color.dayCardBg
                        border.width: 1
                        border.color: card.selected ? Theme.color.daySelectedBorder
                                                    : Theme.color.cardBorder
                        Behavior on color { ColorAnimation { duration: 160 } }
                    }

                    // Squares off the bottom edge and bleeds one pixel past it, so the
                    // selected card reads as continuous with the chart card below —
                    // the strip's way of saying "that chart is showing this day".
                    Rectangle {
                        visible: card.selected
                        anchors.bottom: parent.bottom
                        anchors.bottomMargin: -1
                        anchors.left: parent.left
                        anchors.leftMargin: 1
                        anchors.right: parent.right
                        anchors.rightMargin: 1
                        height: Theme.metric.cardRadius + 2
                        color: Theme.color.cardBg
                    }

                    Text {
                        text: card.modelData.date
                        color: Theme.color.textPrimary
                        font.pixelSize: 15
                        font.bold: true
                        x: 16
                        y: 14
                    }

                    Text {
                        text: card.modelData.label
                        color: card.selected ? Theme.color.textPrimary : Theme.color.textMuted
                        font.pixelSize: 12
                        font.bold: card.selected
                        anchors.right: parent.right
                        anchors.rightMargin: 16
                        y: 16
                    }

                    Row {
                        spacing: 10
                        anchors.left: parent.left
                        anchors.leftMargin: 16
                        anchors.verticalCenter: parent.verticalCenter
                        anchors.verticalCenterOffset: 12

                        WeatherGlyph {
                            kind: card.modelData.icon
                            glyphSize: 40
                            anchors.verticalCenter: parent.verticalCenter
                        }
                        WeatherGlyph {
                            visible: card.modelData.nightIcon !== ""
                            width: visible ? 40 : 0
                            kind: card.modelData.nightIcon === "" ? "clear-night"
                                                                  : card.modelData.nightIcon
                            glyphSize: 40
                            anchors.verticalCenter: parent.verticalCenter
                        }
                    }

                    Column {
                        spacing: 2
                        anchors.right: parent.right
                        anchors.rightMargin: 16
                        anchors.verticalCenter: parent.verticalCenter
                        anchors.verticalCenterOffset: 12

                        Text {
                            text: card.modelData.high + "°"
                            color: Theme.color.textPrimary
                            font.pixelSize: 17
                            font.bold: true
                            anchors.right: parent.right
                        }
                        Text {
                            text: card.modelData.low + "°"
                            color: Theme.color.textMuted
                            font.pixelSize: 15
                            anchors.right: parent.right
                        }
                    }

                    HoverHandler { cursorShape: Qt.PointingHandCursor }
                    TapHandler { onTapped: root.currentIndex = card.index }
                }
            }
        }
    }

    PagerButton {
        pointsLeft: true
        enabledState: flick.contentX > 1
        opacity: enabledState ? 1 : 0
        anchors.left: parent.left
        anchors.verticalCenter: parent.verticalCenter
        onActivated: flick.scrollBy(-flick.width * 0.7)
    }

    PagerButton {
        pointsLeft: false
        enabledState: flick.contentX < flick.contentWidth - flick.width - 1
        opacity: enabledState ? 1 : 0
        anchors.right: parent.right
        anchors.verticalCenter: parent.verticalCenter
        onActivated: flick.scrollBy(flick.width * 0.7)
    }
}
