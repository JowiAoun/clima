// SPDX-License-Identifier: GPL-3.0-or-later
// The hourly chart card, driven by whichever metric the tab bar has selected.
//
// Layout, top to bottom inside the panel:
//   header band  hour label + condition icon + the active metric's value
//   plot         dotted value grid, past veil, series, sun events
//   strip        precipitation probability per label interval
//
// Everything except the series renderer is metric-agnostic: axis, grid, header,
// scrolling, crosshair and the past treatment are shared, and a metric chooses
// only its range, its colour ramp and whether it draws as an area or as bars.
import QtQuick
import QtQuick.Shapes
import "chartmath.js" as ChartMath
import "theme.js" as Theme
import "mockdata.js" as Data
import "metrics.js" as Metrics

Item {
    id: root

    property string metricId: "overview"
    property bool listView: false
    readonly property var metric: Metrics.byId(metricId)
    readonly property bool supportsFeelsLike: metric.id === "overview"

    // ---- scales and metrics ---------------------------------------------
    readonly property real hourWidth: Theme.metric.hourWidth
    readonly property real axisTopPad: Theme.metric.axisTopPad
    readonly property real headerBandHeight: Theme.metric.headerBandHeight
    readonly property real stripHeight: Theme.metric.stripHeight
    readonly property real stripGap: Theme.metric.stripGap
    readonly property real panelPadding: Theme.metric.panelPadding
    readonly property real cardPadding: Theme.metric.cardPadding
    readonly property real gutterWidth: Theme.metric.gutterWidth

    readonly property real contentW: (Data.count - 1) * hourWidth
    readonly property var labelIndices: Data.labelIndices()
    readonly property real nowX: xForIndex(Data.nowIndex)

    // Axis bounds come from the metric, except where it opts into auto-scaling.
    readonly property real axisMin: metric.min
    readonly property real axisMax: Metrics.axisMax(metric, seriesValues(metric))
    readonly property var valueTicks: Metrics.axisTicks(metric, seriesValues(metric))

    readonly property real plotHeight: Math.max(
        170, body.height - 2 * panelPadding - headerBandHeight - stripGap - stripHeight)

    // 0 = the metric's own series, 1 = apparent temperature. Animated, so toggling
    // morphs the curve instead of cutting to it. Only meaningful on Overview.
    property real feelsBlend: (supportsFeelsLike && toggle.checked) ? 1 : 0
    Behavior on feelsBlend { NumberAnimation { duration: 430; easing.type: Easing.OutCubic } }

    function xForIndex(i) { return i * hourWidth }

    function yForValue(v) {
        var span = axisMax - axisMin
        if (span <= 0)
            return 0
        return axisTopPad + (plotHeight - axisTopPad) * (axisMax - v) / span
    }

    function seriesValue(i, blend) {
        if (supportsFeelsLike)
            return Data.temperature[i] * (1 - blend) + Data.apparent[i] * blend
        var arr = Data[metric.series]
        return arr ? arr[i] : 0
    }

    function seriesValues(m) {
        var arr = Data[m.series]
        return arr ? arr : []
    }

    // Explicit arguments so the binding re-evaluates on every input that moves.
    function buildPoints(blend, ph, hw, m) {
        var pts = []
        for (var i = 0; i < Data.count; ++i)
            pts.push({ x: i * hw, y: yForValue(seriesValue(i, blend)) })
        return pts
    }

    function buildOverlay(ph, hw, m) {
        if (!m.overlay)
            return []
        var arr = Data[m.overlay]
        if (!arr)
            return []
        var pts = []
        for (var i = 0; i < Data.count; ++i)
            pts.push({ x: i * hw, y: yForValue(arr[i]) })
        return pts
    }

    readonly property var curvePoints: buildPoints(feelsBlend, plotHeight, hourWidth, metric)
    readonly property var overlayPoints: buildOverlay(plotHeight, hourWidth, metric)

    // ---- card ------------------------------------------------------------
    // No border: this card's top edge is where the selected day card merges into
    // it, and a 1px outline drawn across that junction is exactly the seam we are
    // trying not to have. Fill contrast against the page defines the card instead.
    Rectangle {
        anchors.fill: parent
        radius: Theme.metric.cardRadius
        color: Theme.color.cardBg
    }

    Text {
        id: title
        text: root.metric.label
        color: Theme.color.textPrimary
        font.pixelSize: 15
        font.bold: true
        x: root.cardPadding
        y: root.cardPadding
    }

    FeelsLikeToggle {
        id: toggle
        visible: root.supportsFeelsLike && !root.listView
        anchors.right: parent.right
        anchors.rightMargin: root.cardPadding
        anchors.verticalCenter: title.verticalCenter
    }

    // ---- body ------------------------------------------------------------
    // Chart and list are *unloaded*, not hidden.
    //
    // Toggling `visible` on the chart panel corrupted clipping elsewhere in the
    // scene: the section heading in the tab bar stopped painting, and list rows
    // escaped the ListView's clip. Both are the same underlying problem — the
    // panel contains a clipped Flickable, and hiding that subtree leaves the clip
    // state wrong for other nodes. A Loader avoids it entirely, and not keeping an
    // invisible chart alive is the better shape anyway.
    Item {
        id: body
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: title.bottom
        anchors.bottom: legend.top
        anchors.leftMargin: root.cardPadding
        anchors.rightMargin: root.cardPadding
        anchors.topMargin: 14
        anchors.bottomMargin: 14

        // active: true, not `!root.listView`.
        //
        // Removing the chart subtree from the scene makes unrelated Text items
        // elsewhere in the window stop painting — the section heading in the tab
        // bar and the "Chart" switch label both vanish. Those items report as
        // perfectly healthy at runtime (right text, size, colour, visible, opacity
        // 1), so the scene is correct and only the render is wrong. Ruled out:
        // text renderType, Shape.CurveRenderer, the list's own content (an empty
        // list reproduces it), layer isolation, and grab timing. Keeping the chart
        // alive underneath is the one thing that reliably fixes it.
        //
        // Cost is a chart that stays built while the list is shown. Revisit when
        // the C++ port lands (decision D3) — this smells like a Qt scene-graph bug
        // that a QSGGeometryNode implementation may simply not trip.
        Loader {
            anchors.fill: parent
            active: true
            sourceComponent: chartPanel
        }

        Loader {
            anchors.fill: parent
            active: root.listView
            source: "HourlyList.qml"
        }
    }

    Component {
        id: chartPanel

        Rectangle {
            id: panel
            anchors.fill: parent
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
                        text: root.metric.unit === "°" ? modelData + "°" : modelData
                        color: Theme.color.textDim
                        font.pixelSize: 11
                        x: gutter.width - width - 7
                        y: root.yForValue(modelData) - height / 2
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
                // `clip` bounds the rectangles and the text; it does not bound
                // Qt Quick Shapes (docs/10-design-system.md §10.8), and the hour
                // glyphs are Shapes. Off the right of a 1340-wide window the
                // escapees land outside the window and are never seen — the
                // component gallery, which stages this panel at 1000, is where
                // they showed up: cloud and sun glyphs floating in open page
                // 300px past the panel edge.
                clip: true
                layer.enabled: true
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
                                text: Metrics.format(root.metric,
                                                     root.seriesValue(hourIndex, root.feelsBlend))
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
                                                             root.yForValue)
                                }
                            }
                        }

                        SeriesArea {
                            visible: root.metric.kind === "area"
                            anchors.fill: parent
                            points: visible ? root.curvePoints : []
                            overlayPoints: visible ? root.overlayPoints : []
                            baselineY: root.yForValue(root.axisMin)
                            gradientTop: root.yForValue(root.axisMax)
                            gradientBottom: root.yForValue(root.axisMin)
                            fillRamp: Theme.ramp[root.metric.ramp].fill
                            lineRamp: Theme.ramp[root.metric.ramp].line
                        }

                        SeriesBars {
                            visible: root.metric.kind === "bars"
                            anchors.fill: parent
                            values: visible ? root.seriesValues(root.metric) : []
                            hourWidth: root.hourWidth
                            axisTop: root.yForValue(root.axisMax)
                            axisBottom: root.yForValue(root.axisMin)
                            minValue: root.axisMin
                            maxValue: root.axisMax
                            ramp: Theme.ramp[root.metric.ramp].fill
                        }

                        // The past, veiled and hatched *over* the series: observed hours
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

                            delegate: Item {
                                id: sunMarker
                                required property var modelData

                                width: sunRow.width + 14
                                height: sunRow.height + 8
                                x: root.xForIndex(modelData.index) - width / 2
                                y: plot.height - height - 10

                                // Markers sit over whatever the series happens to be, and
                                // an orange AQI bar behind grey text is unreadable. The
                                // scrim costs nothing and makes them legible everywhere.
                                Rectangle {
                                    anchors.fill: parent
                                    radius: height / 2
                                    color: "#99111a2b"
                                }

                                Row {
                                    id: sunRow
                                    spacing: 5
                                    anchors.centerIn: parent

                                    SunEventGlyph {
                                        kind: sunMarker.modelData.kind
                                        glyphSize: 15
                                        anchors.verticalCenter: parent.verticalCenter
                                    }
                                    Text {
                                        text: sunMarker.modelData.text
                                        color: Theme.color.textMuted
                                        font.pixelSize: 11
                                        anchors.verticalCenter: parent.verticalCenter
                                    }
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
                                visible: root.metric.kind === "area"
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
                            radius: Theme.metric.controlRadius
                            color: "#e6141d33"
                            border.width: 1
                            border.color: Theme.color.cardBorder
                            width: readoutText.implicitWidth + 16
                            height: readoutText.implicitHeight + 10
                            x: ChartMath.clamp(root.xForIndex(probe.idx) - width / 2,
                                               0, plot.width - width)
                            y: ChartMath.clamp(
                                   (probe.idx >= 0 && root.metric.kind === "area"
                                        ? root.curvePoints[probe.idx].y
                                        : root.yForValue(root.axisMax)) - height - 12,
                                   0, plot.height - height)

                            Text {
                                id: readoutText
                                anchors.centerIn: parent
                                horizontalAlignment: Text.AlignHCenter
                                color: Theme.color.textPrimary
                                font.pixelSize: 11
                                text: probe.idx < 0 ? "" :
                                      Data.clockLabel(probe.idx) + "   "
                                      + Metrics.format(root.metric,
                                                       root.seriesValue(probe.idx, root.feelsBlend))
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
            spacing: 16
            anchors.left: parent.left
            anchors.verticalCenter: parent.verticalCenter

            Row {
                spacing: 8
                visible: !root.listView
                Rectangle {
                    width: 11
                    height: 11
                    radius: 5.5
                    anchors.verticalCenter: parent.verticalCenter
                    gradient: Gradient {
                        GradientStop {
                            position: 0.0
                            color: ChartMath.sampleRamp(Theme.ramp[root.metric.ramp].fill, 0.15)
                        }
                        GradientStop {
                            position: 1.0
                            color: ChartMath.sampleRamp(Theme.ramp[root.metric.ramp].fill, 0.85)
                        }
                    }
                }
                Text {
                    text: root.supportsFeelsLike && toggle.checked
                          ? qsTr("Feels like") : root.metric.legend
                    color: Theme.color.textMuted
                    font.pixelSize: 12
                    anchors.verticalCenter: parent.verticalCenter
                }
            }

            Row {
                spacing: 8
                visible: root.metric.overlay !== undefined && !root.listView
                Rectangle {
                    width: 14
                    height: 2
                    radius: 1
                    color: "#8cffffff"
                    anchors.verticalCenter: parent.verticalCenter
                }
                Text {
                    text: root.metric.overlayLegend ? root.metric.overlayLegend : ""
                    color: Theme.color.textMuted
                    font.pixelSize: 12
                    anchors.verticalCenter: parent.verticalCenter
                }
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
