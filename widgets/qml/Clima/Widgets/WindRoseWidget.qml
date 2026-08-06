// SPDX-FileCopyrightText: 2026 Jowi Aoun
// SPDX-License-Identifier: GPL-3.0-or-later
//
// Speed, gust, and the direction the wind is blowing *from*.
//
// ============================================================================
// FROM, NOT TOWARDS
//
// Meteorological convention: a "north wind" comes out of the north. Every
// provider reports it that way and every compass rose on a weather site draws
// it that way, so the arrow points at the reader from the edge rather than away
// from the centre. Getting this backwards is a 180° error that looks completely
// plausible, which is why it is written down here and tested in
// tests/tst_widgets.cpp rather than left to whoever next opens this file.
//
// ============================================================================
// THREE ENCODINGS OF ONE READING, ON PURPOSE
//
// The letters ("NW"), the arrow, and the Beaufort name. docs/04 §4.10 forbids
// colour as the only carrier of meaning; the same argument applies to angle. An
// arrow on a 44 px dial is a few degrees of ambiguity, and the two words beside
// it cost nothing.

import QtQuick
import QtQuick.Shapes

import "wire.js" as Wire

WidgetSurface {
    id: root
    widgetId: "wind-rose"

    readonly property var current: Wire.obj(Wire.at(root.snap, "current"))
    readonly property real bearing: Wire.num(root.current.windDirection)

    Item {
        id: dial
        anchors.horizontalCenter: parent.horizontalCenter
        anchors.top: parent.top
        width: Math.min(parent.width, parent.height - readings.height - 4)
        height: width

        readonly property real radius: width / 2 - 2

        // The ring. Drawn rather than a bordered Rectangle, because a
        // Rectangle's border is inside its bounds and the arrow has to reach
        // the edge of the same circle.
        Rectangle {
            anchors.centerIn: parent
            width: dial.radius * 2
            height: width
            radius: width / 2
            color: "transparent"
            border.width: 1
            border.color: Theme.line.track
        }

        // North, so the dial is readable without a legend.
        Text {
            anchors.horizontalCenter: parent.horizontalCenter
            anchors.top: parent.top
            text: qsTr("N")
            color: Theme.ink.dim
            font.pixelSize: Theme.type.axis
        }

        // The arrow. Pointing *inward* from the bearing: it is drawn along the
        // negative-y axis (which is up, i.e. north, in item coordinates) and
        // then rotated by the bearing, so a 0° reading puts it at the top
        // aiming at the centre.
        Shape {
            anchors.centerIn: parent
            width: dial.radius * 2
            height: dial.radius * 2
            visible: Wire.has(root.current.windDirection)
            transform: Rotation {
                origin.x: dial.radius
                origin.y: dial.radius
                angle: root.bearing
            }

            ShapePath {
                strokeWidth: 0
                fillColor: Theme.accent.fill
                startX: dial.radius
                startY: dial.radius * 0.12
                PathLine { x: dial.radius * 0.72; y: dial.radius * 0.5 }
                PathLine { x: dial.radius * 1.28; y: dial.radius * 0.5 }
            }

            ShapePath {
                strokeWidth: 2
                strokeColor: Theme.accent.fill
                fillColor: "transparent"
                startX: dial.radius
                startY: dial.radius * 0.42
                PathLine { x: dial.radius; y: dial.radius * 0.95 }
            }
        }

        Text {
            anchors.centerIn: parent
            text: Wx.compass(root.current.windDirection)
            color: Theme.ink.primary
            font.pixelSize: Math.max(11, Math.round(dial.width * 0.2))
            font.bold: true
        }
    }

    Column {
        id: readings
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        spacing: 0

        Text {
            anchors.horizontalCenter: parent.horizontalCenter
            text: Units.format(Units.Wind, Wire.num(root.current.windSpeed))
            color: Theme.ink.primary
            font.pixelSize: Theme.type.label
            font.bold: true
        }

        // Two lines and not one. Both are qualifiers on the number above, and
        // a 180 px tile elided "gusting 16 km/h · Gentle breeze" to "· Gentle…"
        // — which spends the width on the separator and loses the word.
        Text {
            anchors.horizontalCenter: parent.horizontalCenter
            width: parent.width
            horizontalAlignment: Text.AlignHCenter
            elide: Text.ElideRight
            visible: Wire.has(root.current.windGust)
            text: qsTr("gusting %1").arg(Units.format(Units.Wind,
                                                      Wire.num(root.current.windGust)))
            color: Theme.ink.dim
            font.pixelSize: Theme.type.axis
        }

        Text {
            anchors.horizontalCenter: parent.horizontalCenter
            width: parent.width
            horizontalAlignment: Text.AlignHCenter
            elide: Text.ElideRight
            text: Wx.beaufort(root.current.windSpeed)
            visible: text !== ""
            color: Theme.ink.dim
            font.pixelSize: Theme.type.axis
        }
    }
}
