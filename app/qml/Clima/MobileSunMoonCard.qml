// SPDX-FileCopyrightText: 2026 Jowi Aoun
// SPDX-License-Identifier: GPL-3.0-or-later
// Sun & Moon: the two things overhead, and how far through their day each is.
//
// The desktop gives these a card each in the twelve-card grid. The phone puts
// them together because they are one question — what is the sky doing over the
// next few hours — and because at 390 px two full DetailSunCards is a screen
// and a half of scrolling for four clock times.
//
// The top row is the pair of readings that are *not* times: how strong the sun
// is, and what shape the moon is in. Both are one tap deep in the reference and
// stated outright here, on the grounds that a number you can read is worth more
// than a link to it.
//
// ---- motion ------------------------------------------------------------------
// One reveal, owned here, driving both arcs. They have to leave together: two
// arcs side by side sweeping on their own timers reads as a race, and there is
// nothing about the sun and the moon that makes one arrive before the other.
import QtQuick

Item {
    id: root

    readonly property var sun: Detail.sun
    readonly property var moon: Detail.moon

    // Same shape as DetailCard's hook: 0 → 1, once, shortly after the card is
    // built, and never re-triggered. §10.6.
    property real reveal: 0

    Timer {
        interval: 60
        running: true
        onTriggered: root.reveal = 1
    }

    Behavior on reveal {
        NumberAnimation { duration: Theme.motion.reveal; easing.type: Easing.OutCubic }
    }

    implicitHeight: arcs.y + arcs.height
    height: implicitHeight

    // ---- the two readings ---------------------------------------------------
    Row {
        id: readings
        width: parent.width

        Repeater {
            model: [
                { glyph: "uv",   label: qsTr("UV index"),
                  value: Detail.uv.reading + " · " + Detail.uv.band },
                { glyph: "moon", label: qsTr("Moon phase"),
                  value: Detail.moon.phase }
            ]

            delegate: Row {
                id: readingCell
                required property var modelData

                width: readings.width / 2
                spacing: 10

                Item {
                    width: 30
                    height: 30
                    anchors.verticalCenter: parent.verticalCenter

                    WeatherGlyph {
                        visible: readingCell.modelData.glyph === "uv"
                        kind: "clear-day"
                        glyphSize: 26
                        anchors.centerIn: parent
                    }

                    MoonGlyph {
                        visible: readingCell.modelData.glyph === "moon"
                        glyphSize: 24
                        illuminated: Detail.moon.illumination
                        anchors.centerIn: parent
                    }
                }

                Column {
                    spacing: 2
                    anchors.verticalCenter: parent.verticalCenter

                    Text {
                        text: readingCell.modelData.label
                        color: Theme.color.textMuted
                        font.pixelSize: Theme.type.label
                    }

                    Text {
                        text: readingCell.modelData.value
                        color: Theme.color.textPrimary
                        font.pixelSize: Theme.type.status
                        font.bold: true
                    }
                }
            }
        }
    }

    // ---- the two arcs -------------------------------------------------------
    Row {
        id: arcs
        anchors.top: readings.bottom
        anchors.topMargin: 20
        width: parent.width
        spacing: 18

        readonly property real cellWidth: (width - spacing) / 2

        SkyArc {
            width: arcs.cellWidth
            reveal: root.reveal
            tint: Theme.color.sunGlyphWarm

            riseMin: root.sun.riseMin
            setMin: root.sun.setMin
            nowMin: root.sun.nowMin
            riseLabel: root.sun.riseLabel
            riseSuffix: root.sun.riseSuffix
            riseName: qsTr("Sunrise")
            setLabel: root.sun.setLabel
            setSuffix: root.sun.setSuffix
            setName: qsTr("Sunset")
            span: root.sun.dayLength

            mark: Rectangle {
                width: 13
                height: 13
                radius: 6.5
                color: Theme.color.sunGlyphWarm
                border.width: 2
                border.color: Theme.color.textPrimary
            }
        }

        SkyArc {
            width: arcs.cellWidth
            reveal: root.reveal
            tint: Theme.color.moonGlyph

            riseMin: root.moon.riseMin
            setMin: root.moon.setMin
            nowMin: root.moon.nowMin
            riseLabel: root.moon.riseLabel
            riseSuffix: root.moon.riseSuffix
            riseName: qsTr("Moonrise")
            setLabel: root.moon.setLabel
            setSuffix: root.moon.setSuffix
            setName: qsTr("Moonset")
            span: root.moon.upLength

            // The moon's mark is its phase, which is why the mark is a slot
            // rather than a "sun or moon" flag on the arc: a waning gibbous is
            // a parameter, and drawing a generic disc here would throw away
            // the one thing the moon's mark can say that the sun's cannot.
            mark: MoonGlyph {
                glyphSize: 14
                illuminated: Detail.moon.illumination
            }
        }
    }
}
