// SPDX-License-Identifier: GPL-3.0-or-later
// The MSN Weather "Hourly → Overview" chart, rebuilt in Qt Quick.
//
// Layout, top to bottom inside the panel:
//   header band  hour label + condition icon + temperature, scrolls with the plot
//   plot         dotted value grid, past veil, temperature area, sun events
//   strip        precipitation probability per label interval
import QtQuick
import QtQuick.Shapes
import "chartmath.js" as ChartMath
import "theme.js" as Theme
import "mockdata.js" as Data

Item {
    id: root

    // ---- scales and metrics ---------------------------------------------
    readonly property real hourWidth: Theme.metric.hourWidth
    readonly property real tempMin: Theme.scale.tempMin
    readonly property real tempMax: Theme.scale.tempMax
    readonly property real axisTopPad: Theme.metric.axisTopPad
    readonly property real headerBandHeight: Theme.metric.headerBandHeight
    readonly property real stripHeight: Theme.metric.stripHeight
    readonly property real stripGap: Theme.metric.stripGap
    readonly property real panelPadding: Theme.metric.panelPadding
    readonly property real cardPadding: Theme.metric.cardPadding
    readonly property real gutterWidth: Theme.metric.gutterWidth

    readonly property real contentW: (Data.count - 1) * hourWidth
    readonly property var labelIndices: Data.labelIndices()
    readonly property var valueTicks: Data.valueTicks(tempMin, tempMax, Theme.scale.tickStep)
    readonly property real nowX: xForIndex(Data.nowIndex)

    readonly property real plotHeight: Math.max(
        170, panel.height - 2 * panelPadding - headerBandHeight - stripGap - stripHeight)

    // 0 = dry-bulb temperature, 1 = apparent temperature. Animated, so toggling
    // morphs the curve instead of cutting to it.
    property real feelsBlend: toggle.checked ? 1 : 0
    Behavior on feelsBlend { NumberAnimation { duration: 430; easing.type: Easing.OutCubic } }

    function xForIndex(i) { return i * hourWidth }

    function yForTemp(t) {
        var span = tempMax - tempMin
        if (span <= 0)
            return 0
        return axisTopPad + (plotHeight - axisTopPad) * (tempMax - t) / span
    }

    function valueAt(i, blend) {
        return Data.temperature[i] * (1 - blend) + Data.apparent[i] * blend
    }

    // Explicit arguments so the binding re-evaluates on every input that moves.
    function buildPoints(blend, ph, hw) {
        var pts = []
        for (var i = 0; i < Data.count; ++i)
            pts.push({ x: i * hw, y: yForTemp(valueAt(i, blend)) })
        return pts
    }

    readonly property var curvePoints: buildPoints(feelsBlend, plotHeight, hourWidth)

    // ---- card ------------------------------------------------------------
    Rectangle {
        anchors.fill: parent
        radius: Theme.metric.cardRadius
        color: Theme.color.cardBg
        border.width: 1
        border.color: Theme.color.cardBorder
    }

    Text {
        id: title
        text: qsTr("Overview")
        color: Theme.color.textPrimary
        font.pixelSize: 15
        font.bold: true
        x: root.cardPadding
        y: root.cardPadding
    }

    FeelsLikeToggle {
        id: toggle
        anchors.right: parent.right
        anchors.rightMargin: root.cardPadding
        anchors.verticalCenter: title.verticalCenter
    }

    // ---- plot panel ------------------------------------------------------
    Rectangle {
        id: panel
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: title.bottom
        anchors.bottom: legend.top
        anchors.leftMargin: root.cardPadding
        anchors.rightMargin: root.cardPadding
        anchors.topMargin: 14
        anchors.bottomMargin: 14
        radius: Theme.metric.panelRadius
        color: Theme.color.panelBg

        // value axis, outside the scrolling region
        Item {
            id: gutter
            x: 2
            y: root.panelPadding + root.headerBandHeight
            width: root.gutterWidth
            height: root.plotHeight

            Repeater {
                model: root.valueTicks
                delegate: Text {
                    required property var modelData
                    text: modelData + "°"
                    color: Theme.color.textDim
                    font.pixelSize: 11
                    x: gutter.width - width - 7
                    y: root.yForTemp(modelData) - height / 2
                }
            }
        }

        Flickable {
            id: flick
            anchors.fill: parent
            anchors.leftMargin: gutter.x + gutter.width
            anchors.rightMargin: 8
            anchors.topMargin: root.panelPadding
            anchors.bottomMargin: root.panelPadding
            clip: true
            contentWidth: root.contentW
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
                var to = Math.max(0, Math.min(contentWidth - width, contentX + dx))
                scrollAnim.stop()
                scrollAnim.from = contentX
                scrollAnim.to = to
                scrollAnim.start()
            }

            Item {
                id: content
                width: root.contentW
                height: flick.height

                // ---- header band -----------------------------------------
                Repeater {
                    model: root.labelIndices

                    delegate: Item {
                        required property var modelData
                        readonly property int hourIndex: modelData
                        readonly property bool isNow: hourIndex === Data.nowIndex

                        x: root.xForIndex(hourIndex) - width / 2
                        y: 0
                        width: root.hourWidth * 2
                        height: root.headerBandHeight

                        Text {
                            text: Data.hourLabel(hourIndex)
                            color: parent.isNow ? Theme.color.textPrimary : Theme.color.textMuted
                            font.pixelSize: 12
                            font.bold: parent.isNow
                            anchors.horizontalCenter: parent.horizontalCenter
                            y: 6
                        }

                        WeatherGlyph {
                            kind: Data.conditionFor(hourIndex)
                            glyphSize: 27
                            anchors.horizontalCenter: parent.horizontalCenter
                            y: 26
                        }

                        Text {
                            text: Math.round(root.valueAt(hourIndex, root.feelsBlend)) + "°"
                            color: Theme.color.textPrimary
                            font.pixelSize: 13
                            font.bold: true
                            anchors.horizontalCenter: parent.horizontalCenter
                            y: 57
                        }
                    }
                }

                // ---- plot ------------------------------------------------
                Item {
                    id: plot
                    y: root.headerBandHeight
                    width: parent.width
                    height: root.plotHeight

                    // faint vertical guides at each label
                    Shape {
                        anchors.fill: parent
                        preferredRendererType: Shape.CurveRenderer
                        ShapePath {
                            strokeColor: Theme.color.gridLineWeak
                            strokeWidth: 1
                            fillColor: "transparent"
                            PathSvg {
                                path: ChartMath.guidePath(root.labelIndices, plot.height,
                                                          root.xForIndex)
                            }
                        }
                    }

                    // dotted value gridlines
                    Shape {
                        anchors.fill: parent
                        preferredRendererType: Shape.CurveRenderer
                        ShapePath {
                            strokeColor: Theme.color.gridLine
                            strokeWidth: 1
                            strokeStyle: ShapePath.DashLine
                            dashPattern: [1, 5]
                            capStyle: ShapePath.FlatCap
                            fillColor: "transparent"
                            PathSvg {
                                path: ChartMath.gridPath(plot.width, root.valueTicks,
                                                         root.yForTemp)
                            }
                        }
                    }

                    TemperatureArea {
                        anchors.fill: parent
                        points: root.curvePoints
                        baselineY: root.yForTemp(root.tempMin)
                        gradientTop: root.yForTemp(root.tempMax)
                        gradientBottom: root.yForTemp(root.tempMin)
                        tempMin: root.tempMin
                        tempMax: root.tempMax
                    }

                    // The past, veiled and hatched *over* the curve: observed hours
                    // are still real data, so they stay visible, but they are visibly
                    // not forecast. Blanking them would read as a rendering bug.
                    Item {
                        width: root.nowX
                        height: plot.height
                        Rectangle {
                            anchors.fill: parent
                            color: Theme.color.pastVeil
                        }
                        HatchPattern {
                            anchors.fill: parent
                            spacing: 11
                            lineColor: Theme.color.pastHatch
                        }
                    }

                    // now
                    Rectangle {
                        x: root.nowX
                        width: 1
                        height: plot.height
                        color: Theme.color.nowLine
                    }

                    // sunrise / sunset
                    Repeater {
                        model: Data.sunEvents

                        delegate: Row {
                            required property var modelData
                            spacing: 5
                            x: root.xForIndex(modelData.index) - width / 2
                            y: plot.height - height - 12

                            SunEventGlyph {
                                kind: modelData.kind
                                glyphSize: 15
                                anchors.verticalCenter: parent.verticalCenter
                            }
                            Text {
                                text: modelData.text
                                color: Theme.color.textMuted
                                font.pixelSize: 11
                                anchors.verticalCenter: parent.verticalCenter
                            }
                        }
                    }

                    // ---- hover crosshair ---------------------------------
                    MouseArea {
                        id: probe
                        anchors.fill: parent
                        hoverEnabled: true
                        acceptedButtons: Qt.NoButton      // let the Flickable drag
                        property int idx: -1

                        onPositionChanged: (mouse) => {
                            var i = Math.round(mouse.x / root.hourWidth)
                            idx = ChartMath.clamp(i, 0, Data.count - 1)
                        }
                        onExited: idx = -1
                    }

                    Item {
                        visible: probe.idx >= 0
                        x: root.xForIndex(probe.idx)
                        y: 0
                        width: 1
                        height: plot.height

                        Rectangle {
                            width: 1
                            height: parent.height
                            color: "#59ffffff"
                        }

                        Rectangle {
                            width: 7
                            height: 7
                            radius: 3.5
                            color: Theme.color.textPrimary
                            x: -3
                            y: (probe.idx >= 0 ? root.curvePoints[probe.idx].y : 0) - 3.5
                        }
                    }

                    Rectangle {
                        id: readout
                        visible: probe.idx >= 0
                        radius: 5
                        color: "#e6111a2b"
                        border.width: 1
                        border.color: Theme.color.cardBorder
                        width: readoutText.implicitWidth + 16
                        height: readoutText.implicitHeight + 10
                        x: ChartMath.clamp(root.xForIndex(probe.idx) - width / 2,
                                           0, plot.width - width)
                        y: ChartMath.clamp(
                               (probe.idx >= 0 ? root.curvePoints[probe.idx].y : 0) - height - 12,
                               0, plot.height - height)

                        Text {
                            id: readoutText
                            anchors.centerIn: parent
                            horizontalAlignment: Text.AlignHCenter
                            color: Theme.color.textPrimary
                            font.pixelSize: 11
                            text: probe.idx < 0 ? "" :
                                  Data.clockLabel(probe.idx) + "   "
                                  + root.valueAt(probe.idx, root.feelsBlend).toFixed(1) + "°C"
                                  + "   " + Data.precipProb[probe.idx] + "%"
                        }
                    }
                }

                // ---- precipitation strip ---------------------------------
                PrecipitationStrip {
                    y: root.headerBandHeight + root.plotHeight + root.stripGap
                    width: parent.width
                    height: root.stripHeight
                    hourWidth: root.hourWidth
                    nowX: root.nowX
                    contentWidth: root.contentW
                }
            }
        }

        // ---- pagers ------------------------------------------------------
        PagerButton {
            pointsLeft: true
            enabledState: flick.contentX > 1
            anchors.left: flick.left
            anchors.leftMargin: -4
            y: root.panelPadding + root.headerBandHeight + root.plotHeight / 2 - height / 2
            onActivated: flick.scrollBy(-flick.width * 0.75)
        }

        PagerButton {
            pointsLeft: false
            enabledState: flick.contentX < flick.contentWidth - flick.width - 1
            anchors.right: flick.right
            anchors.rightMargin: -4
            y: root.panelPadding + root.headerBandHeight + root.plotHeight / 2 - height / 2
            onActivated: flick.scrollBy(flick.width * 0.75)
        }
    }

    // ---- legend ----------------------------------------------------------
    Item {
        id: legend
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        anchors.leftMargin: root.cardPadding
        anchors.rightMargin: root.cardPadding
        anchors.bottomMargin: root.cardPadding - 4
        height: 18

        Row {
            spacing: 8
            anchors.left: parent.left
            anchors.verticalCenter: parent.verticalCenter

            Rectangle {
                width: 11
                height: 11
                radius: 5.5
                anchors.verticalCenter: parent.verticalCenter
                gradient: Gradient {
                    GradientStop { position: 0.0; color: "#d9c08f" }
                    GradientStop { position: 1.0; color: "#69b294" }
                }
            }
            Text {
                text: toggle.checked ? qsTr("Feels like") : qsTr("Temperature")
                color: Theme.color.textMuted
                font.pixelSize: 12
                anchors.verticalCenter: parent.verticalCenter
            }
        }

        Row {
            spacing: 8
            anchors.right: parent.right
            anchors.verticalCenter: parent.verticalCenter

            MoonGlyph {
                glyphSize: 15
                illuminated: Data.moonPhase.illuminated
                anchors.verticalCenter: parent.verticalCenter
            }
            Text {
                textFormat: Text.StyledText
                text: qsTr("Moon phase: ") + "<b>" + Data.moonPhase.name + "</b>"
                color: Theme.color.textMuted
                font.pixelSize: 12
                anchors.verticalCenter: parent.verticalCenter
            }
        }
    }
}
