// SPDX-License-Identifier: GPL-3.0-or-later
// Sun detail card — the day drawn as the sun's own path across the sky.
//
// The arc is a sinusoid in solar altitude: zero at sunrise and sunset, one at
// solar noon, negative outside. The horizon line is altitude zero, so the
// stretch above it *is* the daylight, and its width on the card is the day
// length. Nothing here is a fixed decorative bell — move riseMin or setMin and
// the crossings, the sun dot and the warm stretch all move with them.
//
// Horizontal span is a full 24 hours centred on solar noon, which is why the
// two horizon crossings sit symmetrically about the middle of the box.
import QtQuick
import QtQuick.Shapes
import "theme.js" as Theme
import "chartmath.js" as ChartMath
import "detaildata.js" as Detail

DetailCard {
    id: root

    readonly property var d: Detail.sun

    title: qsTr("Sun")
    status: d.status
    trend: d.trend
    body: d.body

    content: Item {
        id: viz

        // Warm ramp keyed on altitude: p = 0 is the zenith, p = 1 the horizon.
        // The same ramp fills the arc gradient and tints the sun dot, so the
        // dot's colour reads as "how high the sun is" rather than as branding.
        readonly property var skyRamp: [
            { p: 0.00, c: "#ffe488" },
            { p: 0.45, c: "#ffc63f" },
            { p: 1.00, c: "#ef7526" }
        ]
        readonly property color nightArc: "#5d6486"   // below the horizon: dim

        readonly property real riseMin: root.d.riseMin
        readonly property real setMin: root.d.setMin
        readonly property real nowMin: root.d.nowMin
        readonly property real dayLenMin: setMin - riseMin
        readonly property real solarNoon: (riseMin + setMin) / 2

        // A whole day, centred on solar noon.
        readonly property real tMin: solarNoon - 720
        readonly property real tMax: solarNoon + 720

        readonly property real padX: 4

        function altAt(t) {
            return Math.sin(Math.PI * (t - riseMin) / dayLenMin)
        }
        function xAt(t) {
            return padX + (t - tMin) / (tMax - tMin) * (width - padX * 2)
        }
        function yAt(t) { return horizonY - altAt(t) * amp }

        function arcPoints(t0, t1, n) {
            var out = []
            for (var i = 0; i <= n; ++i) {
                var t = t0 + (t1 - t0) * i / n
                out.push({ x: xAt(t), y: yAt(t) })
            }
            return out
        }

        // How far below the horizon the visible night stretch reaches, as a
        // fraction of the daytime amplitude. Derived, not guessed, so the arc
        // is scaled to exactly fill the band left above the type.
        readonly property real dipFrac:
            Math.max(0.05, -Math.min(altAt(tMin), altAt(tMax)))

        // The arc uses the whole band above the type. The day-length label sits
        // level with the arc's low points rather than below them — the night
        // stretch has left the middle of that line empty, and the card is too
        // short to spend a row on white space.
        readonly property real arcTopY: 3
        readonly property real arcBottomY: figures.y - 2
        readonly property real amp: Math.max(8, (arcBottomY - arcTopY) / (1 + dipFrac))
        readonly property real horizonY: arcTopY + amp

        readonly property real strokeW: 5

        Shape {
            anchors.fill: parent
            preferredRendererType: Shape.CurveRenderer

            // Night: the same curve, dimmed. It is drawn because a bare
            // daylight hump gives no sense of where in the whole day we are.
            ShapePath {
                fillColor: "transparent"
                strokeColor: viz.nightArc
                strokeWidth: viz.strokeW
                capStyle: ShapePath.RoundCap
                PathSvg {
                    path: ChartMath.smooth(viz.arcPoints(viz.tMin, viz.riseMin, 10), "M")
                        + " " + ChartMath.smooth(viz.arcPoints(viz.setMin, viz.tMax, 10), "M")
                }
            }

            // Daylight, drawn as a ribbon because a ShapePath can gradient-fill
            // but not gradient-stroke. Bright at the zenith, sunset-orange as it
            // meets the horizon at either end.
            ShapePath {
                strokeColor: "transparent"
                fillGradient: LinearGradient {
                    x1: 0; y1: viz.arcTopY; x2: 0; y2: viz.horizonY
                    GradientStop { position: 0.00; color: "#ffe488" }
                    GradientStop { position: 0.45; color: "#ffc63f" }
                    GradientStop { position: 1.00; color: "#ef7526" }
                }
                PathSvg {
                    path: ChartMath.ribbonPath(viz.arcPoints(viz.riseMin, viz.setMin, 28),
                                               viz.strokeW / 2)
                }
            }
        }

        // The horizon. Altitude zero, and the only reference the arc needs.
        Rectangle {
            x: 0
            y: viz.horizonY - height / 2
            width: viz.width
            height: 1
            color: Theme.color.gridLine
        }

        // Sunrise and sunset, where the curve crosses.
        Repeater {
            model: [viz.riseMin, viz.setMin]
            delegate: Rectangle {
                required property var modelData
                width: 10; height: 10; radius: 5
                color: Theme.color.textMuted
                border.width: 2
                border.color: Theme.color.textPrimary
                x: viz.xAt(modelData) - width / 2
                y: viz.horizonY - height / 2
            }
        }

        // Now, on the arc and ringed so it stays readable against the ribbon.
        Rectangle {
            width: 16; height: 16; radius: 8
            color: ChartMath.sampleRamp(viz.skyRamp,
                                        1 - Math.max(0, viz.altAt(viz.nowMin)))
            border.width: 2.5
            border.color: Theme.color.textPrimary
            x: viz.xAt(viz.nowMin) - width / 2
            y: viz.yAt(viz.nowMin) - height / 2
        }

        Text {
            id: dayLenText
            text: root.d.dayLength
            color: Theme.color.textPrimary
            font.pixelSize: 11
            font.bold: true
            anchors.horizontalCenter: parent.horizontalCenter
            y: viz.arcBottomY - height
        }

        // Sunrise and sunset as two labelled figures: time large, unit and
        // label small beside it, each centred in half the box.
        Item {
            id: figures
            width: viz.width
            height: 32
            y: viz.height - height

            Repeater {
                model: [
                    { fTime: root.d.riseLabel, fSuffix: root.d.riseSuffix, fLabel: qsTr("Sunrise") },
                    { fTime: root.d.setLabel,  fSuffix: root.d.setSuffix,  fLabel: qsTr("Sunset") }
                ]
                delegate: Item {
                    required property int index
                    required property var modelData

                    width: figures.width / 2
                    height: figures.height
                    x: index * width

                    Row {
                        spacing: 5
                        anchors.horizontalCenter: parent.horizontalCenter
                        anchors.verticalCenter: parent.verticalCenter

                        Text {
                            text: modelData.fTime
                            color: Theme.color.textPrimary
                            font.pixelSize: 24
                            font.bold: true
                        }

                        Column {
                            spacing: 0
                            Text {
                                text: modelData.fSuffix
                                color: Theme.color.textMuted
                                font.pixelSize: 11
                                font.bold: true
                            }
                            Text {
                                text: modelData.fLabel
                                color: Theme.color.textMuted
                                font.pixelSize: 11
                            }
                        }
                    }
                }
            }
        }
    }
}
