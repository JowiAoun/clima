// SPDX-License-Identifier: GPL-3.0-or-later
// Current conditions — the page headline.
//
// The one card that answers the question the app was opened to ask, so it is
// allowed to be loud in a way nothing else on the page is: a 64 px number, a
// 72 px glyph, and a plain-language sentence. Everything below it is detail.
//
// It reads every number from detaildata.js rather than carrying its own, so the
// headline and the detail card for the same measurable cannot drift apart.
//
// Anatomy, top to bottom:
//   label + timestamp     what this is, and how fresh
//   glyph, reading, unit  the headline, with the condition and feels-like beside
//   summary               one sentence of outlook
//   slug row              six measurables, each also a card further down
import QtQuick
import QtQuick.Shapes
import "theme.js" as Theme
import "chartmath.js" as ChartMath
import "detaildata.js" as Detail

Item {
    id: root

    readonly property real pad: Theme.metric.cardPadding

    // The six measurables MSN puts under the summary. Each is also a card in
    // the grid below: this row is the glance, that grid is the answer.
    //
    // `dot` and `arrow` are the two that carry more than a number — air quality
    // is meaningless without its band, and a wind speed without a bearing is
    // half a reading.
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

    implicitHeight: slugGrid.y + slugGrid.height + pad
    height: implicitHeight

    // No border. Contrast against the page defines the card — see
    // docs/10-design-system.md §10.1.
    Rectangle {
        anchors.fill: parent
        radius: Theme.metric.cardRadius
        color: Theme.color.cardBg
    }

    // ---- label and timestamp ---------------------------------------------
    Text {
        id: heading
        text: qsTr("Current weather")
        color: Theme.color.textPrimary
        font.pixelSize: Theme.type.cardTitle
        font.bold: true
        x: root.pad
        y: root.pad
    }

    Text {
        id: stamp
        text: Detail.observedAt
        color: Theme.color.textMuted
        font.pixelSize: Theme.type.body
        anchors.left: heading.left
        anchors.top: heading.bottom
        anchors.topMargin: 3
    }

    // ---- the headline -----------------------------------------------------
    Item {
        id: summaryGroup
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.leftMargin: root.pad
        anchors.rightMargin: root.pad
        anchors.top: stamp.bottom
        anchors.topMargin: 18
        height: Math.max(glyph.height, reading.implicitHeight)

        WeatherGlyph {
            id: glyph
            kind: Detail.current.conditionKind
            glyphSize: 72
            anchors.left: parent.left
            anchors.verticalCenter: parent.verticalCenter
        }

        // Regular weight, not bold. At 64 px the reference sets this in book
        // weight and it is right: bold at that size stops reading as a number
        // and starts reading as a shout.
        Text {
            id: reading
            text: String(Detail.temperature.value)
            color: Theme.color.textPrimary
            font.pixelSize: Theme.type.heroReading
            anchors.left: glyph.right
            anchors.leftMargin: 16
            anchors.verticalCenter: parent.verticalCenter
        }

        // Rides at the top of the digits rather than on their baseline, which
        // is where a degree suffix belongs.
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

        Column {
            id: conditionColumn
            spacing: 4
            anchors.left: unit.right
            anchors.leftMargin: 30
            anchors.verticalCenter: parent.verticalCenter

            Text {
                text: Detail.cloudCover.condition
                color: Theme.color.textPrimary
                font.pixelSize: Theme.type.heroCaption
                font.bold: true
            }

            Row {
                spacing: 8
                Text {
                    text: qsTr("Feels like")
                    color: Theme.color.textMuted
                    font.pixelSize: Theme.type.heroLabel
                    anchors.verticalCenter: parent.verticalCenter
                }
                Text {
                    text: Detail.feelsLike.value + Detail.feelsLike.unit
                    color: Theme.color.textPrimary
                    font.pixelSize: Theme.type.heroDetail
                    anchors.verticalCenter: parent.verticalCenter
                }
            }
        }

        // Today's range, right-aligned.
        //
        // A divergence: the reference fills this space with a radar map, which
        // we have nothing to draw. Left empty, a card this wide reads as a
        // layout that ran out of content — and the high and low are the two
        // numbers the summary sentence gestures at without stating.
        Row {
            id: rangeRow
            spacing: 22
            anchors.right: parent.right
            anchors.verticalCenter: parent.verticalCenter
            visible: parent.width > 620

            Repeater {
                model: [{ label: qsTr("High"), value: Detail.temperature.high },
                        { label: qsTr("Low"),  value: Detail.temperature.low }]

                delegate: Column {
                    required property var modelData
                    spacing: 2

                    Text {
                        text: modelData.label
                        color: Theme.color.textMuted
                        font.pixelSize: Theme.type.heroLabel
                        anchors.horizontalCenter: parent.horizontalCenter
                    }
                    Text {
                        text: modelData.value + Detail.temperature.unit
                        color: Theme.color.textPrimary
                        font.pixelSize: Theme.type.heroCaption
                        anchors.horizontalCenter: parent.horizontalCenter
                    }
                }
            }
        }
    }

    // ---- outlook ----------------------------------------------------------
    Text {
        id: outlook
        text: Detail.current.summary
        color: Theme.color.textPrimary
        font.pixelSize: Theme.type.heroDetail
        elide: Text.ElideRight
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.leftMargin: root.pad
        anchors.rightMargin: root.pad
        anchors.top: summaryGroup.bottom
        anchors.topMargin: 20
    }

    // ---- slug row ---------------------------------------------------------
    // Equal columns rather than the reference's natural widths: at 620 px its
    // six slugs sit shoulder to shoulder, and at our width the same treatment
    // leaves them huddled against the left edge of a very wide card. Equal
    // columns stretch, and they wrap when the window is too narrow to hold six.
    Grid {
        id: slugGrid
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.leftMargin: root.pad
        anchors.rightMargin: root.pad
        anchors.top: outlook.bottom
        anchors.topMargin: 22

        // Only counts that divide six, so the last row is never one orphan slug
        // sitting under five. At 760 px the naive "as many as fit" gave 5 + 1,
        // which reads as a layout accident rather than as a wrap.
        readonly property int fits: Math.floor(width / 132)
        columns: fits >= 6 ? 6 : (fits >= 3 ? 3 : 2)
        readonly property real cellWidth: Math.floor(width / columns)
        rowSpacing: 14

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
                    width: parent.width - 8
                }

                Row {
                    id: slugValue
                    spacing: 7
                    anchors.top: slugLabel.bottom
                    anchors.topMargin: 4

                    Rectangle {
                        visible: slug.modelData.dot
                        width: 11
                        height: 11
                        radius: 5.5
                        color: root.aqiColor
                        anchors.verticalCenter: parent.verticalCenter
                    }

                    Text {
                        text: slug.modelData.value
                        color: Theme.color.textPrimary
                        font.pixelSize: Theme.type.heroDetail
                        anchors.verticalCenter: parent.verticalCenter
                    }

                    // Points where the wind is going, matching the wedge in
                    // DetailWindCard: the bearing names where it comes from, so
                    // the arrow is that bearing turned around.
                    Item {
                        visible: slug.modelData.arrow >= 0
                        width: 16
                        height: 16
                        anchors.verticalCenter: parent.verticalCenter

                        // The notch is deep on purpose. A shallow one leaves a
                        // squat triangle that reads as a generic marker at this
                        // size, and the whole point of the glyph is that you can
                        // tell instantly which way it points.
                        Shape {
                            anchors.fill: parent
                            preferredRendererType: Shape.CurveRenderer
                            rotation: slug.modelData.arrow + 180
                            ShapePath {
                                fillColor: Theme.color.textPrimary
                                strokeColor: "transparent"
                                PathSvg { path: "M 8 0.6 L 12.3 15.4 L 8 12.1 L 3.7 15.4 Z" }
                            }
                        }
                    }
                }
            }
        }
    }
}
