// SPDX-FileCopyrightText: 2026 Jowi Aoun
// SPDX-License-Identifier: GPL-3.0-or-later
// The phone's headline: the answer to the question the app was opened to ask.
//
// The desktop's CurrentConditions with three things changed, and each of them
// is a consequence of the width rather than a restyling:
//
//   No card.      It sits directly on the page gradient. On the desktop the
//                 hero is one of four sections and a wash is what separates
//                 it from the others; here it is the top of a screen with
//                 nothing above it to be separated from, and a wash would
//                 only hide the sky it is drawn on.
//   No high/low.  The desktop puts them right-aligned in the space the
//                 reference fills with a radar map. There is no such space at
//                 390 px, and the ten-day card two rows down states today's
//                 pair anyway.
//   Slugs wrap.   Six across needs about 790 px. Three by two is the only
//                 arrangement that divides six and fits.
//
// It reads every number from detaildata.js, same as the desktop hero, so the
// two cannot drift.
//
// ---- motion: none ------------------------------------------------------------
// Same reasoning as CurrentConditions, and it applies harder here. This card
// asserts a number, a condition and six readings; an assertion is instant.
// A phone hero that counts up from zero is a hero that briefly reports the
// wrong temperature on the one screen the app exists to show.
import QtQuick
import QtQuick.Shapes
import "chartmath.js" as ChartMath
import "detaildata.js" as Detail

Item {
    id: root

    // The six the desktop shows, without the disclosure chevrons the
    // reference puts on each. A chevron is a promise that tapping the slug
    // opens the measurable, and this prototype has no per-measurable screen on
    // the phone to open — LocationBar's own note about the non-rotating
    // chevron is the same rule: do not draw an affordance the app cannot keep.
    readonly property var slugs: [
        { label: qsTr("Air quality"), value: String(Detail.airQuality.value),
          dot: true,  arrow: -1 },
        { label: qsTr("Wind"),        value: Detail.wind.speed + " " + Detail.wind.unit,
          dot: false, arrow: Detail.wind.directionDeg },
        { label: qsTr("Humidity"),    value: Detail.humidity.value + Detail.humidity.unit,
          dot: false, arrow: -1 },
        { label: qsTr("Visibility"),  value: Detail.visibility.value + " " + Detail.visibility.unit,
          dot: false, arrow: -1 },
        { label: qsTr("Pressure"),    value: Detail.pressure.value + " " + Detail.pressure.unit,
          dot: false, arrow: -1 },
        { label: qsTr("Dew point"),   value: Detail.humidity.dewPoint + Detail.humidity.dewUnit,
          dot: false, arrow: -1 }
    ]

    readonly property color aqiColor: ChartMath.sampleRamp(
        Detail.bands.aqi, Detail.airQuality.value / Detail.airQuality.max)

    implicitHeight: slugGrid.y + slugGrid.height
    height: implicitHeight

    // ---- label and timestamp ----------------------------------------------
    Text {
        id: heading
        text: qsTr("Current weather")
        color: Theme.color.textPrimary
        font.pixelSize: Theme.type.cardTitle
        font.bold: true
        anchors.left: parent.left
    }

    Text {
        text: Detail.observedAt
        color: Theme.color.textMuted
        font.pixelSize: Theme.type.status
        anchors.right: parent.right
        anchors.baseline: heading.baseline
    }

    // ---- the headline ------------------------------------------------------
    Item {
        id: headline
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: heading.bottom
        anchors.topMargin: 14
        height: Math.max(glyph.height, reading.implicitHeight)

        WeatherGlyph {
            id: glyph
            kind: Detail.current.conditionKind
            glyphSize: 52
            anchors.left: parent.left
            anchors.verticalCenter: parent.verticalCenter
        }

        Text {
            id: reading
            text: String(Detail.temperature.value)
            color: Theme.color.textPrimary
            font.pixelSize: Theme.type.heroReading
            anchors.left: glyph.right
            anchors.leftMargin: 10
            anchors.verticalCenter: parent.verticalCenter
        }

        Text {
            id: unit
            text: Detail.current.unitLabel
            color: Theme.color.textPrimary
            font.pixelSize: Theme.type.heroUnit
            anchors.left: reading.right
            anchors.leftMargin: 2
            anchors.top: reading.top
            anchors.topMargin: Math.round(Theme.type.heroReading * 0.13)
        }

        // Takes whatever is left and elides. "Sunny" fits at any width this
        // shell runs at; "Thunderstorms in the area" does not, and a condition
        // that wraps to two lines would push the glyph and the number apart.
        Column {
            spacing: 2
            anchors.left: unit.right
            anchors.leftMargin: 14
            anchors.right: parent.right
            anchors.verticalCenter: parent.verticalCenter

            Text {
                text: Detail.cloudCover.condition
                color: Theme.color.textPrimary
                font.pixelSize: Theme.type.heroCaption
                font.bold: true
                width: parent.width
                elide: Text.ElideRight
            }

            Text {
                text: qsTr("Feels like %1").arg(Detail.feelsLike.value + Detail.feelsLike.unit)
                color: Theme.color.textMuted
                font.pixelSize: Theme.type.heroLabel
                width: parent.width
                elide: Text.ElideRight
            }
        }
    }

    // ---- outlook -----------------------------------------------------------
    Text {
        id: outlook
        text: Detail.current.summary
        color: Theme.color.textPrimary
        font.pixelSize: Theme.type.heroDetail
        wrapMode: Text.WordWrap
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: headline.bottom
        anchors.topMargin: 14
    }

    // ---- slug row ----------------------------------------------------------
    // Three columns, always. The desktop grid picks a count that divides six
    // from the width it is given; here the width is a phone and the answer is
    // three at every size the mobile shell runs at, so choosing is a branch
    // that would only ever be taken one way.
    Grid {
        id: slugGrid
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: outlook.bottom
        anchors.topMargin: 18

        columns: 3
        rowSpacing: 14
        readonly property real cellWidth: Math.floor(width / columns)

        Repeater {
            model: root.slugs

            delegate: Item {
                id: slug
                required property var modelData

                width: slugGrid.cellWidth
                height: slugLabel.height + slugValue.height + 4

                Text {
                    id: slugLabel
                    text: slug.modelData.label
                    color: Theme.color.textMuted
                    font.pixelSize: Theme.type.heroLabel
                    elide: Text.ElideRight
                    width: parent.width - 6
                }

                Row {
                    id: slugValue
                    spacing: 6
                    anchors.top: slugLabel.bottom
                    anchors.topMargin: 3

                    Rectangle {
                        visible: slug.modelData.dot
                        width: 10
                        height: 10
                        radius: 5
                        color: root.aqiColor
                        anchors.verticalCenter: parent.verticalCenter
                    }

                    Text {
                        text: slug.modelData.value
                        color: Theme.color.textPrimary
                        font.pixelSize: Theme.type.heroDetail
                        anchors.verticalCenter: parent.verticalCenter
                    }

                    // Points where the wind is going: the bearing names where
                    // it comes from, so the arrow is that bearing turned round.
                    // Same glyph and same reasoning as the desktop hero.
                    Item {
                        visible: slug.modelData.arrow >= 0
                        width: 14
                        height: 14
                        anchors.verticalCenter: parent.verticalCenter

                        Shape {
                            anchors.fill: parent
                            preferredRendererType: Shape.CurveRenderer
                            rotation: slug.modelData.arrow + 180
                            ShapePath {
                                fillColor: Theme.color.textPrimary
                                strokeColor: "transparent"
                                PathSvg { path: "M 7 0.5 L 10.8 13.5 L 7 10.6 L 3.2 13.5 Z" }
                            }
                        }
                    }
                }
            }
        }
    }
}
