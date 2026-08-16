// SPDX-FileCopyrightText: 2026 Jowi Aoun
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
import "precip.js" as Precip

Item {
    id: root

    property string metricId: "overview"
    property bool listView: false

    // Ambient motion — currently the precipitation field, which is the only
    // thing here that moves when nothing has changed. Off under `--grab`, so a
    // headless frame is the same frame every time.
    property bool animated: true

    // Exposed so a headless film can drive the feels-like morph, which is
    // otherwise reachable only by clicking the toggle.
    property alias feelsLike: toggle.checked

    // What the chart is *drawing*, as opposed to what has been asked for. It
    // lags `metricId` by the outgoing half of the handover below, so the axis
    // bounds, the tick labels, the colour ramp, the header values, the legend,
    // the card title and the series type all change in the single frame where
    // there is nothing on the plot to contradict them.
    property string shownMetricId: "overview"
    readonly property var metric: Metrics.byId(shownMetricId)
    // The toggle is offered only where there is a second series to show. A
    // "Feels like" switch that redraws the temperature curve is a control that
    // lies about what it did, which is worse than one that is not there.
    readonly property bool supportsFeelsLike: metric.id === "overview" && Data.hasApparent

    // ---- scales and metrics ---------------------------------------------
    // Settable, not readonly: the mobile shell runs this same card at 362 px,
    // where the token's 48 px column leaves room for six hours and a gutter.
    // It is still the token by default — a caller overriding it is stating a
    // width the token cannot know about, not disagreeing with it.
    property real hourWidth: Theme.metric.hourWidth
    readonly property real axisTopPad: Theme.metric.axisTopPad
    readonly property real headerBandHeight: Theme.metric.headerBandHeight
    readonly property real stripHeight: Theme.metric.stripHeight
    readonly property real stripGap: Theme.metric.stripGap
    readonly property real panelPadding: Theme.metric.panelPadding
    readonly property real cardPadding: Theme.metric.cardPadding
    readonly property real gutterWidth: Theme.metric.gutterWidth

    readonly property real contentW: (Data.count - 1) * hourWidth
    readonly property var labelIndices: Data.labelIndices
    // Where the present falls on the plot. `Data.nowIndex` is an offset that
    // may sit outside the window — see forecastdata.h — so this can be negative
    // for a day still ahead and past `contentW` for one already gone, and both
    // are the answer rather than a case to guard. Clamped only because the two
    // things that read it are a width and an x, and a negative width is a Qt
    // warning rather than an empty rectangle.
    readonly property real nowX:
        ChartMath.clamp(xForIndex(Data.nowIndex), 0, contentW)

    // Axis bounds come from the metric, except where it opts into auto-scaling.
    readonly property real axisMin: metric.min
    readonly property real axisMax: Metrics.axisMax(metric, seriesValues(metric))
    readonly property var valueTicks: Metrics.axisTicks(metric, seriesValues(metric))

    readonly property real plotHeight: Math.max(
        170, body.height - 2 * panelPadding - headerBandHeight - stripGap - stripHeight)

    // The card takes whatever height it is given and hands the remainder to the
    // plot. That works when it fills a window and does nothing at all in a
    // scrolling column, where an item has to say how tall it wants to be — so
    // this is the same relation solved the other way round: the height at which
    // the plot comes out at its preferred size.
    //
    // `Theme.metric.plotHeight` had been sitting in the token table unused.
    // This is the one place that knows what it means.
    //
    // Settable for the same reason `hourWidth` is. A 252 px plot plus its
    // header band and strip is 398 px of card, which on a 390x844 phone is
    // half the screen for one of five things on it — and the plot is the part
    // that can afford to give: the header band and the strip are fixed-height
    // rows that would become illegible rather than merely shorter.
    property real preferredPlotHeight: Theme.metric.plotHeight

    readonly property real preferredBodyHeight: preferredPlotHeight
        + 2 * panelPadding + headerBandHeight + stripGap + stripHeight

    implicitHeight: cardPadding + title.height + 14 + preferredBodyHeight + 14
                    + legend.height + cardPadding - 4

    // 0 = the metric's own series, 1 = apparent temperature. Animated, so toggling
    // morphs the curve instead of cutting to it. Only meaningful on Overview.
    //
    // This is a genuine tween and not a crossfade: the points themselves are
    // interpolated and the path is regenerated from them, so every intermediate
    // frame is a curve the renderer could have been given as data. `view`,
    // because it is one view of today becoming another — the same hours, read a
    // second way — and deliberately the same token as the metric handover's
    // incoming half, which is the other thing on this card that replaces the
    // series without moving the frame around it. It was 430 ms, one of the eight
    // strays §10.6 was written to end.
    property real feelsBlend: (supportsFeelsLike && toggle.checked) ? 1 : 0
    Behavior on feelsBlend {
        // A metric handover already animates the series; feels-like is
        // meaningless anywhere but Overview, so let it snap into place under
        // cover of the handover rather than run a second tween through it.
        enabled: !handover.running
        NumberAnimation { duration: Theme.motion.view; easing.type: Easing.OutCubic }
    }

    // ---- metric handover --------------------------------------------------
    // Switching metric is a view transition, not a tween.
    //
    // Temperature and wind are different quantities, on different axes, in
    // different units. Morphing one curve into the other would claim they are
    // the same measurement changing; sliding the axis from 40 °C to 40 km/h
    // would put a tick label through numbers that mean nothing on either scale.
    // And area → bars cannot be tweened at all — §10.7 makes bars and curves
    // different *claims about the data*, so an in-between shape would be a
    // claim we do not have.
    //
    // What every metric on this card does share is the axis baseline. So the
    // outgoing series folds onto it, everything that names the metric changes
    // at that instant — when the plot is empty and nothing can be misread — and
    // the incoming series grows back off it. That is §10.6's own description of
    // honest motion, "a bar growing off its baseline", applied to a switch
    // rather than to an arrival, and it is the one gesture that serves a curve
    // and a bar chart equally.
    //
    // The frame does not take part. The hour labels, the condition glyphs, the
    // past veil, the now line, the sun markers, the precipitation strip and the
    // precipitation wash and field are all functions of time, not of metric,
    // and they stay exactly where they are so the reader keeps their place
    // while the data underneath changes. The rain in particular has to: it
    // falls in the same hours whichever metric is plotted, so folding it onto
    // the baseline with the series would say the weather changed when only the
    // question did.
    //
    // 1 = the series at full extent, 0 = flat on the baseline.
    property real seriesExtent: 1

    // Set once the object is built, so properties handed in at construction —
    // the gallery builds specimens with `metricId` already set — configure the
    // chart instead of animating it.
    property bool ready: false

    SequentialAnimation {
        id: handover

        NumberAnimation {
            target: root; property: "seriesExtent"; to: 0
            duration: Theme.motion.move
            // InCubic rather than the default, for exactly the reason §10.6
            // gives for the default: things ease *into place* because they are
            // arriving. This half is a departure, so it accelerates away.
            easing.type: Easing.InCubic
        }

        ScriptAction { script: root.shownMetricId = root.metricId }

        NumberAnimation {
            target: root; property: "seriesExtent"; to: 1
            duration: Theme.motion.view
            easing.type: Easing.OutCubic
        }
    }

    onMetricIdChanged: {
        if (!ready)
            shownMetricId = metricId
        else if (metricId !== shownMetricId)
            handover.restart()
    }

    Component.onCompleted: {
        shownMetricId = metricId
        ready = true
    }

    function xForIndex(i) { return i * hourWidth }

    function yForValue(v) {
        var span = axisMax - axisMin
        if (span <= 0)
            return 0
        return axisTopPad + (plotHeight - axisTopPad) * (axisMax - v) / span
    }

    // ---- canonical in, display out ----------------------------------------
    // `Data` holds one series per metric in the engine's own units, and the
    // axis above is in the reader's. Everything that reaches a pixel goes
    // through Metrics, which is the only thing here that knows which is which —
    // a curve plotted in millimetres against an axis in inches draws perfectly
    // and is off by a factor of twenty-five.
    function seriesValue(i, blend) {
        if (supportsFeelsLike) {
            var t = Data.temperature[i]
            var a = Data.apparent[i]
            // MET Norway carries no apparent temperature. `t * 1 + NaN * 0` is
            // NaN in JavaScript, so a blend of ZERO over an absent series erased
            // a temperature curve that was completely intact — an empty chart
            // under a hero reading 28°. Seen the first time the fallback served.
            if (isNaN(a))
                return Metrics.display(metric, t)
            if (isNaN(t))
                return Metrics.display(metric, a)
            return Metrics.display(metric, t * (1 - blend) + a * blend)
        }
        var arr = Data[metric.series]
        return arr ? Metrics.display(metric, arr[i]) : 0
    }

    function seriesValues(m) {
        var arr = Data[m.series]
        return arr ? Metrics.displayAll(m, arr) : []
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
        var shown = Metrics.displayAll(m, arr)
        var pts = []
        for (var i = 0; i < Data.count; ++i)
            pts.push({ x: i * hw, y: yForValue(shown[i]) })
        return pts
    }

    // The wash and the field, classified where the thresholds live. `Data`
    // supplies the millimetres — canonical, always, because these bands are
    // statements about millimetres — and the type per hour, which comes off the
    // provider's WMO code and is the only way to know that an hour is thunder
    // rather than heavy rain.
    readonly property var precipCells: Precip.cellsTyped(Data.precipMm, Data.precipTypes)

    readonly property var curvePoints: buildPoints(feelsBlend, plotHeight, hourWidth, metric)
    readonly property var overlayPoints: buildOverlay(plotHeight, hourWidth, metric)

    // ---- card ------------------------------------------------------------
    // No border: this card's top edge is where the selected day card merges into
    // it, and a 1px outline drawn across that junction is exactly the seam we are
    // trying not to have. Fill contrast against the page defines the card instead.
    Rectangle {
        anchors.fill: parent
        radius: Theme.metric.cardRadius
        color: Theme.surface.base
    }

    Text {
        id: title
        text: root.metric.label
        color: Theme.ink.primary
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
        // …but staying loaded is not the same as staying *visible*, and that
        // distinction went missing. While every surface was an opaque navy fill
        // the list's own panel hid the chart behind it and nobody noticed the
        // chart was still being painted. Surfaces then became translucent
        // washes, and from that change onward the chart has shown straight
        // through the list: hour labels, gridlines and the area fill interleaved
        // with the rows. It is on `main` today and it took assembling the page
        // to see it, because nothing re-checked list view afterwards.
        //
        // opacity, not visible: `visible: false` on this subtree is the thing
        // that corrupts clipping for unrelated nodes. Zero alpha leaves the
        // scene graph exactly as it was and only stops it painting.
        Loader {
            anchors.fill: parent
            active: true
            opacity: root.listView ? 0 : 1
            enabled: !root.listView
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
            color: Theme.surface.panel

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
                        color: Theme.ink.dim
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

                // Open on now, not on the start of the series.
                //
                // The series begins at 21:00 the evening before, so at midday
                // fifteen of its forty-eight hours have already happened —
                // which is the shape a provider hands over, not an artefact of
                // the mock. Opening at contentX 0 therefore opens the section
                // called *Hourly* on last night. `HourlyList` has scrolled to
                // now on mount since it was written; this is the same rule for
                // the other half of the same card, and the two views of one
                // series disagreeing about where they start was only invisible
                // while "now" was three columns in.
                //
                // Observed hours are kept on screen rather than none. The past
                // treatment is a reading — those hours happened, and the veil
                // and the hatch are what say so — and a chart that opens
                // exactly on the now line never shows it at all.
                //
                // How many is not a taste: it is one label interval, put one
                // column inside the left edge. The header band draws an entry
                // per labelled hour, so landing on a label rather than between
                // two is the difference between opening with a legible column
                // and opening with half a glyph and "AM" sliced off by the
                // clip. `nowIndex` is itself a labelled index — it has to be,
                // or "Now" would never be drawn — so stepping back by
                // `labelStep` lands on the label before it.
                //
                // Assigned on completion rather than bound: this is where the
                // chart opens, not where it has to stay, and a binding would
                // haul the reader back here after every flick.
                // A function rather than a one-off, because the window is no
                // longer fixed for the life of the card: the day strip replaces
                // it, and a chart of Friday left at yesterday's contentX opens
                // on whichever hours happened to sit at that many pixels.
                function openOnStart() {
                    contentX = ChartMath.clamp(
                        root.xForIndex(Data.nowIndex - Data.labelStep) - root.hourWidth,
                        0, Math.max(0, contentWidth - width))
                }

                Component.onCompleted: openOnStart()

                // On a day that is not today the clamp does the work: `nowIndex`
                // is negative for a day ahead, so this asks for a negative
                // contentX and gets 0 — midnight, which is where a chart of a
                // date should open. For a day behind it asks for more than
                // there is and gets the end of the day.
                Connections {
                    target: Data
                    function onSelectedDayChanged() { flick.openOnStart() }
                }

                // A pager step is three quarters of the plot: one view of the day
                // becoming another, hence `view`. A *drag* is not animated — the
                // content tracks the finger, and interposing an easing between
                // the two is what makes a scroll feel like it is on elastic.
                NumberAnimation {
                    id: scrollAnim
                    target: flick
                    property: "contentX"
                    duration: Theme.motion.view
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
                            readonly property bool isNow:
                                Data.nowInWindow && hourIndex === Data.nowIndex

                            x: root.xForIndex(hourIndex) - width / 2
                            y: 0
                            width: root.hourWidth * 2
                            height: root.headerBandHeight

                            Text {
                                text: Data.hourLabel(hourIndex)
                                color: parent.isNow ? Theme.ink.primary : Theme.ink.muted
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
                                text: Metrics.formatDisplay(root.metric,
                                                     root.seriesValue(hourIndex, root.feelsBlend))
                                color: Theme.ink.primary
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
                                strokeColor: Theme.line.gridWeak
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
                                strokeColor: Theme.line.grid
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

                        // Precipitation, under the series: the hours it falls
                        // in, washed. Under and not over, because the series'
                        // own colour is a value and a wash laid over it would
                        // be stating a different one — see PrecipBands.qml.
                        PrecipBands {
                            anchors.fill: parent
                            cells: root.precipCells
                            hourWidth: root.hourWidth
                            contentWidth: root.contentW
                        }

                        SeriesArea {
                            visible: root.metric.kind === "area"
                            anchors.fill: parent
                            points: visible ? root.curvePoints : []
                            overlayPoints: visible ? root.overlayPoints : []
                            growth: root.seriesExtent
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
                            growth: root.seriesExtent
                            hourWidth: root.hourWidth
                            axisTop: root.yForValue(root.axisMax)
                            axisBottom: root.yForValue(root.axisMin)
                            minValue: root.axisMin
                            maxValue: root.axisMax
                            ramp: Theme.ramp[root.metric.ramp].fill
                        }

                        // …and the falling half over it. Declared before the past
                        // veil so observed rain is veiled along with everything
                        // else: it fell, and nothing here should imply the
                        // forecast is still promising it.
                        PrecipField {
                            anchors.fill: parent
                            cells: root.precipCells
                            hourWidth: root.hourWidth
                            contentWidth: root.contentW
                            // Nothing worth animating behind the list view, and
                            // --grab wants a frame it can hold against a golden
                            // image rather than whichever one it happened to catch.
                            animated: root.animated && !root.listView
                        }

                        // The past, veiled and hatched *over* the series: observed hours
                        // are still real data, so they stay visible, but they are visibly
                        // not forecast. Blanking them would read as a rendering bug.
                        //
                        // No branch for the day strip: a day that has not started
                        // yet puts the present at or before column zero and this
                        // comes out empty, and a day that is over puts it past
                        // the last column and this covers the plot. Which is
                        // exactly right — every hour of Tuesday is observed.
                        Item {
                            width: root.nowX
                            height: plot.height
                            Rectangle {
                                anchors.fill: parent
                                color: Theme.overlay.past
                            }
                            HatchPattern {
                                anchors.fill: parent
                                spacing: 11
                                lineColor: Theme.overlay.pastHatch
                            }
                        }

                        // now — drawn only where there is a now to draw. On any
                        // other day the line would sit on the plot's edge and
                        // claim midnight, or the last hour, was this moment.
                        Rectangle {
                            visible: Data.nowInWindow
                            x: root.nowX
                            width: 1
                            height: plot.height
                            color: Theme.line.now
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
                                    color: Theme.overlay.caption
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
                                        color: Theme.ink.muted
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

                            // The last hour actually pointed at, held after the
                            // pointer leaves so the crosshair can fade out where
                            // it stood instead of flying off to index -1.
                            property int heldIdx: 0

                            onPositionChanged: (mouse) => {
                                var i = Math.round(mouse.x / root.hourWidth)
                                idx = ChartMath.clamp(i, 0, Data.count - 1)
                                heldIdx = idx
                            }
                            onExited: idx = -1
                        }

                        // The crosshair fades in and out, and *snaps* between
                        // hours. It is a cursor: it selects one hour, there is no
                        // half-past-two on this chart, and easing a cursor
                        // between samples both invents positions the series does
                        // not have and makes the readout feel dragged behind the
                        // pointer. Only its arrival and departure are motion.
                        Item {
                            id: crosshair
                            opacity: probe.idx >= 0 ? 1 : 0
                            visible: opacity > 0
                            x: root.xForIndex(probe.heldIdx)
                            y: 0
                            width: 1
                            height: plot.height

                            Behavior on opacity {
                                NumberAnimation {
                                    duration: Theme.motion.tint
                                    easing.type: Easing.OutCubic
                                }
                            }

                            // The same rule the "now" mark is drawn with, and
                            // deliberately the same token: both answer "which
                            // hour is this", and a chart with two whites for
                            // that reads as one of them being a mistake.
                            Rectangle {
                                width: 1
                                height: parent.height
                                color: Theme.line.now
                            }

                            Rectangle {
                                visible: root.metric.kind === "area"
                                width: 7
                                height: 7
                                radius: 3.5
                                color: Theme.ink.primary
                                x: -3
                                y: (root.curvePoints.length > probe.heldIdx
                                        ? root.curvePoints[probe.heldIdx].y : 0) - 3.5
                            }
                        }

                        // Not faded with the crosshair: §10.6 rules out text that
                        // fades, and this panel is nothing but a number.
                        Rectangle {
                            id: readout
                            visible: probe.idx >= 0
                            radius: Theme.metric.controlRadius
                            color: Theme.overlay.readout
                            border.width: 1
                            border.color: Theme.line.card
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
                                color: Theme.ink.primary
                                font.pixelSize: 11
                                text: probe.idx < 0 ? "" :
                                      Data.clockLabel(probe.idx) + "   "
                                      + Metrics.formatDisplay(root.metric,
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
                // The swatch is the metric's colour identity, and a fill is what
                // `tint` is for. It carries the ramp across the handover so the
                // legend does not blink to a new colour on its own — the stops
                // are bound through animatable properties because a Behavior
                // cannot be attached to a GradientStop.
                Rectangle {
                    id: swatch
                    width: 11
                    height: 11
                    radius: 5.5
                    anchors.verticalCenter: parent.verticalCenter

                    property color rampTop:
                        ChartMath.sampleRamp(Theme.ramp[root.metric.ramp].fill, 0.15)
                    property color rampBottom:
                        ChartMath.sampleRamp(Theme.ramp[root.metric.ramp].fill, 0.85)

                    Behavior on rampTop {
                        ColorAnimation { duration: Theme.motion.tint; easing.type: Easing.OutCubic }
                    }
                    Behavior on rampBottom {
                        ColorAnimation { duration: Theme.motion.tint; easing.type: Easing.OutCubic }
                    }

                    gradient: Gradient {
                        GradientStop { position: 0.0; color: swatch.rampTop }
                        GradientStop { position: 1.0; color: swatch.rampBottom }
                    }
                }
                Text {
                    text: root.supportsFeelsLike && toggle.checked
                          ? qsTr("Feels like") : root.metric.legend
                    color: Theme.ink.muted
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
                    color: Theme.line.series
                    anchors.verticalCenter: parent.verticalCenter
                }
                Text {
                    text: root.metric.overlayLegend ? root.metric.overlayLegend : ""
                    color: Theme.ink.muted
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
                color: Theme.ink.muted
                font.pixelSize: 12
                anchors.verticalCenter: parent.verticalCenter
            }
        }
    }
}
