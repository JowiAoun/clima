// SPDX-FileCopyrightText: 2026 Jowi Aoun
// SPDX-License-Identifier: GPL-3.0-or-later
//
// The tile everything else is a variation on: one temperature, one sky.
//
// The glyph is the app's own WeatherGlyph, not a second drawing of the same
// weather. That is the point of the widget module compiling the app's
// presentation files rather than copying them — a cloud that gains a highlight
// in the app gains it here on the same commit.

import QtQuick

import "wire.js" as Wire

WidgetSurface {
    id: root
    widgetId: "current-conditions"

    readonly property var current: Wire.obj(Wire.at(root.snap, "current"))

    Column {
        id: readings
        anchors.left: parent.left
        anchors.verticalCenter: parent.verticalCenter
        spacing: 2
        width: parent.width * 0.55

        Text {
            // Sized off the tile rather than off a type token, because this is
            // the one number the tile exists to show and the tile is resizable
            // by whoever put it on their desktop. A fixed point size would
            // leave a 400 px card with a 34 px number in the corner of it.
            text: Units.format(Units.Temperature, Wire.num(root.current.temperature))
            color: Theme.ink.primary
            font.pixelSize: Math.max(24, Math.round(root.height * 0.36))
            font.bold: true
        }

        Text {
            text: qsTr("Feels like %1")
                      .arg(Units.format(Units.Temperature,
                                        Wire.num(root.current.apparentTemperature)))
            color: Theme.ink.muted
            font.pixelSize: Theme.type.label
            width: parent.width
            elide: Text.ElideRight
        }
    }

    Column {
        anchors.right: parent.right
        anchors.verticalCenter: parent.verticalCenter
        spacing: 4
        width: parent.width * 0.42

        WeatherGlyph {
            anchors.horizontalCenter: parent.horizontalCenter
            glyphSize: Math.max(28, Math.round(root.height * 0.34))
            kind: Wx.glyphKind(root.current.weatherCode, root.current.isDay)

            // The sky is the one place a widget has to know what it is sitting
            // on. A card is a card, but a glyph is drawn with highlights that
            // assume a dark surface underneath, and in light mode there is a
            // paler set already written for the day badge.
            ground: Theme.isLight ? "pale" : "card"
        }

        Text {
            anchors.horizontalCenter: parent.horizontalCenter
            width: parent.width
            horizontalAlignment: Text.AlignHCenter
            text: Wx.conditionText(root.current.weatherCode, root.current.isDay)
            color: Theme.ink.muted
            font.pixelSize: Theme.type.label
            elide: Text.ElideRight
        }
    }
}
