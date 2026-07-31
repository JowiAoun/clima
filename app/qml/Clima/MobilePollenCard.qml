// SPDX-FileCopyrightText: 2026 Jowi Aoun
// SPDX-License-Identifier: GPL-3.0-or-later
// Pollen: a band, a sentence, and the three sources broken out.
//
// The headline is a *word*, not a number, and that is the whole shape of this
// card. Pollen counts are grains per cubic metre and nobody outside a lab
// knows whether 43 is a lot; the published answer is a band, so the band is
// the reading and it takes the reading size and the band's own colour.
//
// The three rings underneath are the same reading at one level of detail down.
// They are rings rather than bars because the question is "how much of the way
// to bad is this", which is a fraction of something — and a fraction wants an
// unfilled remainder you can see, which is what `trackLine` is for.
import QtQuick
import QtQuick.Shapes
import "detaildata.js" as Detail

Item {
    id: root

    readonly property var d: Detail.pollen

    property real reveal: 0

    Timer {
        interval: 60
        running: true
        onTriggered: root.reveal = 1
    }

    Behavior on reveal {
        NumberAnimation { duration: Theme.motion.reveal; easing.type: Easing.OutCubic }
    }

    // One place that turns a tone into a colour, so the headline word, the
    // rings and the activity list below cannot end up with three greens.
    function toneColor(tone) {
        return tone === "poor" ? Theme.color.statusPoor
             : tone === "caution" ? Theme.color.statusCaution
                                  : Theme.color.statusGood
    }

    implicitHeight: rings.y + rings.height
    height: implicitHeight

    Text {
        id: band
        text: root.d.band
        color: root.toneColor(root.d.tone)
        font.pixelSize: Theme.type.reading
        font.bold: true
    }

    Text {
        id: main
        text: qsTr("Main allergen: %1").arg(root.d.main)
        color: Theme.color.textPrimary
        font.pixelSize: Theme.type.status
        anchors.top: band.bottom
        anchors.topMargin: 8
    }

    Text {
        id: blurb
        text: root.d.body
        color: Theme.color.textMuted
        font.pixelSize: Theme.type.body
        wrapMode: Text.WordWrap
        width: parent.width
        anchors.top: main.bottom
        anchors.topMargin: 4
    }

    Rectangle {
        id: rule
        anchors.top: blurb.bottom
        anchors.topMargin: 16
        width: parent.width
        height: 1
        color: Theme.color.gridLineWeak
    }

    Row {
        id: rings
        anchors.top: rule.bottom
        anchors.topMargin: 16
        width: parent.width

        Repeater {
            model: root.d.items

            delegate: Column {
                id: source
                required property var modelData

                width: rings.width / 3
                spacing: 7

                readonly property color tone: root.toneColor(modelData.tone)

                Item {
                    width: 46
                    height: 46
                    anchors.horizontalCenter: parent.horizontalCenter

                    Shape {
                        anchors.fill: parent
                        preferredRendererType: Shape.CurveRenderer

                        // The remainder, drawn first and in full. A ring with
                        // no track is a fraction with no denominator.
                        ShapePath {
                            fillColor: "transparent"
                            strokeColor: Theme.color.trackLine
                            strokeWidth: 4
                            PathAngleArc {
                                centerX: 23; centerY: 23
                                radiusX: 19; radiusY: 19
                                startAngle: 0
                                sweepAngle: 360
                            }
                        }

                        // Starts at twelve o'clock, which in Qt's y-down
                        // clockwise angles is 270.
                        ShapePath {
                            fillColor: "transparent"
                            strokeColor: source.tone
                            strokeWidth: source.modelData.level * root.reveal > 0.004 ? 4 : -1
                            capStyle: ShapePath.RoundCap
                            PathAngleArc {
                                centerX: 23; centerY: 23
                                radiusX: 19; radiusY: 19
                                startAngle: 270
                                sweepAngle: 360 * source.modelData.level * root.reveal
                            }
                        }
                    }
                }

                Text {
                    text: source.modelData.name
                    color: Theme.color.textMuted
                    font.pixelSize: Theme.type.label
                    anchors.horizontalCenter: parent.horizontalCenter
                }

                Text {
                    text: source.modelData.label
                    color: Theme.color.textPrimary
                    font.pixelSize: Theme.type.label
                    font.bold: true
                    anchors.horizontalCenter: parent.horizontalCenter
                }
            }
        }
    }
}
