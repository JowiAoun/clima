// SPDX-License-Identifier: GPL-3.0-or-later
// The place the page is about: name, a disclosure chevron, a home marker.
//
// It sits directly on the page gradient, not on a surface. Wrapping it in its
// own wash would put a fourth surface immediately above the hero card with
// nothing between them, and two washes that close together read as one badly
// aligned panel rather than as two things.
import QtQuick
import QtQuick.Shapes
import "theme.js" as Theme
import "detaildata.js" as Detail

Item {
    id: root

    property string label: Detail.location.label
    property bool isHome: Detail.location.isHome

    signal changeRequested()
    signal homeToggled()

    implicitWidth: bar.width
    implicitHeight: 26
    height: implicitHeight

    Row {
        id: bar
        spacing: 10
        anchors.verticalCenter: parent.verticalCenter

        Text {
            id: placeName
            text: root.label
            color: Theme.color.textPrimary
            font.pixelSize: Theme.type.sectionTitle
            anchors.verticalCenter: parent.verticalCenter
        }

        // ---- disclosure chevron ------------------------------------------
        Item {
            width: 14
            height: 14
            anchors.verticalCenter: parent.verticalCenter

            Shape {
                anchors.fill: parent
                preferredRendererType: Shape.CurveRenderer
                ShapePath {
                    strokeColor: chevronHover.hovered ? Theme.color.textPrimary
                                                      : Theme.color.textMuted
                    strokeWidth: 1.6
                    fillColor: "transparent"
                    capStyle: ShapePath.RoundCap
                    joinStyle: ShapePath.RoundJoin
                    PathSvg { path: "M 2.5 5.5 L 7 10 L 11.5 5.5" }
                }
            }

            HoverHandler { id: chevronHover; cursorShape: Qt.PointingHandCursor }
            TapHandler { onTapped: root.changeRequested() }
        }

        // ---- home marker --------------------------------------------------
        // A ring rather than a filled plate: a filled circle here would be the
        // only opaque thing on the page.
        Item {
            width: 24
            height: 24
            anchors.verticalCenter: parent.verticalCenter

            Rectangle {
                anchors.fill: parent
                radius: width / 2
                color: homeHover.hovered ? Theme.color.surfaceRaised : "transparent"
                border.width: 1
                border.color: root.isHome ? Theme.color.switchBorder : Theme.color.gridLine
                Behavior on color { ColorAnimation { duration: 140 } }
            }

            Shape {
                anchors.centerIn: parent
                width: 14
                height: 14
                preferredRendererType: Shape.CurveRenderer
                ShapePath {
                    fillColor: root.isHome ? Theme.color.textPrimary : Theme.color.textMuted
                    strokeColor: "transparent"
                    PathSvg {
                        path: "M 7 2.4 L 12.8 7.6 L 11.3 7.6 L 11.3 12.4 "
                            + "L 8.4 12.4 L 8.4 9.3 L 5.6 9.3 L 5.6 12.4 "
                            + "L 2.7 12.4 L 2.7 7.6 L 1.2 7.6 Z"
                    }
                }
            }

            HoverHandler { id: homeHover; cursorShape: Qt.PointingHandCursor }
            TapHandler { onTapped: root.homeToggled() }
        }
    }
}
