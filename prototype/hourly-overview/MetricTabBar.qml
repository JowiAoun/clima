// SPDX-License-Identifier: GPL-3.0-or-later
// Section title, metric pills, and the chart/list view switch.
//
// The pills are generated from metrics.js, so a metric is added by editing data.
// In the real app that registry is populated from provider capabilities, and a tab
// that the active provider cannot supply for this location simply never appears.
import QtQuick
import QtQuick.Shapes
import "theme.js" as Theme
import "metrics.js" as Metrics

Item {
    id: root

    property string title: qsTr("Hourly")
    property string currentId: "overview"
    property bool listView: false

    implicitHeight: 38
    height: implicitHeight

    Text {
        id: heading
        text: root.title
        color: Theme.color.textPrimary
        font.pixelSize: 18
        font.bold: true
        anchors.verticalCenter: parent.verticalCenter
        x: 0
    }

    // ---- view switch (right-aligned, laid out first so pills can clip to it)
    Row {
        id: viewSwitch
        spacing: 0
        anchors.right: parent.right
        anchors.verticalCenter: parent.verticalCenter

        Repeater {
            model: [{ id: "chart", label: qsTr("Chart") }, { id: "list", label: qsTr("List") }]

            delegate: Item {
                required property var modelData
                required property int index

                readonly property bool active: (index === 1) === root.listView

                width: label.implicitWidth + glyph.width + 26
                height: 30

                Rectangle {
                    anchors.fill: parent
                    color: parent.active ? Theme.color.switchActive : "transparent"
                    topLeftRadius: index === 0 ? 6 : 0
                    bottomLeftRadius: index === 0 ? 6 : 0
                    topRightRadius: index === 1 ? 6 : 0
                    bottomRightRadius: index === 1 ? 6 : 0
                    border.width: 1
                    border.color: parent.active ? Theme.color.switchBorder : "transparent"
                    Behavior on color { ColorAnimation { duration: 140 } }
                }

                Row {
                    anchors.centerIn: parent
                    spacing: 7

                    Item {
                        id: glyph
                        width: 13
                        height: 13
                        anchors.verticalCenter: parent.verticalCenter

                        // chart: four bars of differing height; list: three rules
                        Shape {
                            anchors.fill: parent
                            preferredRendererType: Shape.CurveRenderer
                            ShapePath {
                                strokeColor: Theme.color.textMuted
                                strokeWidth: 1.4
                                fillColor: "transparent"
                                capStyle: ShapePath.RoundCap
                                PathSvg {
                                    path: index === 0
                                          ? "M 1 12 L 1 6 M 5 12 L 5 2 M 9 12 L 9 8 M 12.5 12 L 12.5 4"
                                          : "M 1 3 L 12 3 M 1 6.5 L 12 6.5 M 1 10 L 12 10"
                                }
                            }
                        }
                    }

                    Text {
                        id: label
                        text: modelData.label
                        color: Theme.color.textPrimary
                        font.pixelSize: 12
                        anchors.verticalCenter: parent.verticalCenter
                    }
                }

                HoverHandler { cursorShape: Qt.PointingHandCursor }
                TapHandler { onTapped: root.listView = (index === 1) }
            }
        }
    }

    // ---- metric pills ----------------------------------------------------
    Flickable {
        id: pillFlick
        anchors.left: heading.right
        anchors.leftMargin: 20
        anchors.right: viewSwitch.left
        anchors.rightMargin: 16
        anchors.verticalCenter: parent.verticalCenter
        height: 34
        clip: true
        contentWidth: pills.width
        contentHeight: height
        flickableDirection: Flickable.HorizontalFlick
        boundsBehavior: Flickable.StopAtBounds

        Row {
            id: pills
            spacing: 4
            height: pillFlick.height

            Repeater {
                model: Metrics.list

                delegate: Item {
                    id: pill

                    required property var modelData

                    readonly property bool active: modelData.id === root.currentId

                    width: pillLabel.implicitWidth + 26
                    height: pillFlick.height

                    Rectangle {
                        anchors.fill: parent
                        radius: height / 2
                        color: pill.active ? Theme.color.accent
                                           : (pillHover.hovered ? Theme.color.pillHover : "transparent")
                        Behavior on color { ColorAnimation { duration: 140 } }
                    }

                    Text {
                        id: pillLabel
                        text: pill.modelData.label
                        anchors.centerIn: parent
                        color: pill.active ? Theme.color.cardBg : Theme.color.textMuted
                        font.pixelSize: 13
                        font.bold: pill.active
                        Behavior on color { ColorAnimation { duration: 140 } }
                    }

                    HoverHandler { id: pillHover; cursorShape: Qt.PointingHandCursor }
                    TapHandler { onTapped: root.currentId = pill.modelData.id }
                }
            }
        }
    }
}
