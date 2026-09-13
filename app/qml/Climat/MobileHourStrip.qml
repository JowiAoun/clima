// SPDX-FileCopyrightText: 2026 Jowi Aoun
// SPDX-License-Identifier: GPL-3.0-or-later
// The hourly strip on the Today screen: the next day, two hours at a time.
//
// It is a chart, not a row of icons. The band's top edge *is* the temperature,
// so the shape of the next twelve hours is readable before any of the numbers
// are — which is the whole reason the Today screen carries an hourly section
// at all rather than sending you straight to the Hourly tab.
//
// The reference draws a flat band here and puts the temperatures above it. A
// flat band is a decoration: it costs the same pixels and says nothing. Making
// its edge the series is the one change worth making, and it is why this reads
// as a summary of the day rather than as a legend for one.
//
// The window is 24 hours at a 2-hour step. Every hour would be 48 columns of
// near-identical values on a screen 4 columns wide; every 3 would put sunrise
// and the afternoon peak in the same cell.
//
// ---- what this deliberately does not do --------------------------------------
// No axis, no gridlines, no crosshair, no value scale. The Hourly tab is one
// tap away and it has all four. A strip that grew them would be a worse copy
// of the screen it exists to preview.
import QtQuick
import "chartmath.js" as ChartMath

Item {
    id: root

    signal activated()

    readonly property int step: 2
    readonly property int columns: 12

    // A floor, for the reason MobileDailyStrip states at greater length: twelve
    // columns of 74 overflow a phone and scroll, and fall 120 px short inside a
    // landscape tablet's card. At 362 px this is max(74, 30) and nothing moves.
    readonly property real columnWidth:
        Math.max(74, root.width / Math.max(1, columns))

    // The hours themselves, counted forward from the present — not indices into
    // the chart's window, which is what these were.
    //
    // The distinction did not exist until the day strip started moving that
    // window: `Data.nowIndex + i * step` meant "now, and every second hour
    // after it" only while the window was always the one around now. Pick
    // Friday on the Hourly tab and come back here, and this screen would have
    // gone on saying "Now" over a column of Friday afternoon. `ahead()` indexes
    // the whole series from the present and cannot be moved by a control on
    // another screen.
    readonly property var hours: {
        var out = []
        for (var i = 0; i < columns; ++i) {
            if (i * step < Data.aheadCount)
                out.push(Data.ahead(i * step))
        }
        return out
    }

    // ---- geometry -----------------------------------------------------------
    readonly property real tempRowHeight: 24
    readonly property real bandHeight: 92
    readonly property real labelRowHeight: 32

    // The curve lives in the top of the band; the rest is where the condition
    // and the chance of rain go. Both are drawn over the fill rather than
    // under it, which is also what the reference does.
    readonly property real axisTop: 7
    readonly property real axisBottom: 33

    // Where the two marks inside the band sit. Written as constants rather
    // than anchored to each other because the glyph is 30 px of drawing inside
    // a 30 px box and the droplet row is text: anchoring the second to the
    // first put the two 4 px apart on one row and overlapping on the next.
    readonly property real glyphTop: 36
    readonly property real precipTop: 70

    implicitHeight: tempRowHeight + bandHeight + labelRowHeight
    height: implicitHeight

    readonly property var values: {
        var out = []
        for (var i = 0; i < hours.length; ++i)
            out.push(hours[i].temperature)
        return out
    }

    // Padded so the warmest hour does not ride the top edge and the coolest
    // does not sit flat on the icons. Auto-scaled rather than fixed: over one
    // day the whole spread is often six degrees, and on a 0–40 axis that is a
    // flat line — the same argument precipitation's axis makes in metrics.js.
    readonly property real vMin: {
        var m = values.length ? values[0] : 0
        for (var i = 1; i < values.length; ++i)
            m = Math.min(m, values[i])
        return m - 1.5
    }
    readonly property real vMax: {
        var m = values.length ? values[0] : 1
        for (var i = 1; i < values.length; ++i)
            m = Math.max(m, values[i])
        return m + 1.5
    }

    function xFor(i) { return i * columnWidth + columnWidth / 2 }
    function yFor(v) {
        var t = (v - vMin) / Math.max(0.001, vMax - vMin)
        return axisBottom - t * (axisBottom - axisTop)
    }

    // Column centres, plus an anchor at each edge holding the end value.
    //
    // Without them the fill starts at the middle of the first column and stops
    // at the middle of the last, leaving half a column of bare card at both
    // ends of a band that is supposed to run edge to edge. The anchors are
    // flat extensions of the end values rather than extrapolations: a curve
    // that invents a rise in the half-column before the first sample is
    // drawing data that is not there.
    readonly property var curve: {
        var out = []
        if (values.length === 0)
            return out
        out.push({ x: 0, y: yFor(values[0]) })
        for (var i = 0; i < values.length; ++i)
            out.push({ x: xFor(i), y: yFor(values[i]) })
        out.push({ x: contentWidth, y: yFor(values[values.length - 1]) })
        return out
    }

    readonly property real contentWidth: hours.length * columnWidth

    Flickable {
        id: scroll
        anchors.fill: parent
        clip: true

        // Bounds the Shapes. Every glyph in here draws with one and Shapes
        // ignore ancestor clipping outright — without this the condition icon
        // from the 11 PM column paints over the ten-day card below. §10.8.
        layer.enabled: true

        contentWidth: root.contentWidth
        contentHeight: height
        flickableDirection: Flickable.HorizontalFlick
        boundsBehavior: Flickable.StopAtBounds

        Item {
            width: root.contentWidth
            height: scroll.height

            // ---- the band ---------------------------------------------------
            SeriesArea {
                y: root.tempRowHeight
                width: root.contentWidth
                height: root.bandHeight

                points: root.curve
                baselineY: root.bandHeight
                fillRamp: Theme.ramp.temp.fill
                lineRamp: Theme.ramp.temp.line
                lineWidth: 2

                // Keyed to the *absolute* temperature scale, not to the band.
                //
                // The geometry auto-scales — it has to, or a day that spans
                // six degrees draws as a flat line — but colour must not.
                // §10.5 and theme.js's ramp note are both explicit that colour
                // encodes the value, which is what lets a 19° hour read the
                // same green here as it does on the Hourly tab's chart. Key
                // the ramp to the local window instead and the coolest hour of
                // a heatwave comes out the same colour as the coolest hour of
                // a cold snap.
                //
                // Both ends land outside the band, which is exactly right: the
                // fill shows the slice of the ramp this day actually occupies.
                gradientTop: root.yFor(Theme.scale.tempMax)
                gradientBottom: root.yFor(Theme.scale.tempMin)
            }

            // ---- per-column marks -------------------------------------------
            Repeater {
                model: root.hours

                delegate: Item {
                    id: column
                    required property int index
                    required property var modelData

                    readonly property var hour: modelData
                    readonly property bool isNow: index === 0

                    x: index * root.columnWidth
                    width: root.columnWidth
                    height: scroll.height

                    // The reading, above the band and over the column it
                    // belongs to rather than over the point on the curve —
                    // a label chasing a shallow curve wanders by a few pixels
                    // per column and reads as misalignment.
                    Text {
                        text: Units.formatDisplay(Units.Temperature, column.hour.temperature)
                        color: Theme.ink.primary
                        font.pixelSize: Theme.type.status
                        font.bold: column.isNow
                        anchors.horizontalCenter: parent.horizontalCenter
                        y: 2
                    }

                    WeatherGlyph {
                        kind: column.hour.condition
                        glyphSize: 30
                        anchors.horizontalCenter: parent.horizontalCenter
                        y: root.tempRowHeight + root.glyphTop
                    }

                    Row {
                        spacing: 3
                        anchors.horizontalCenter: parent.horizontalCenter
                        y: root.tempRowHeight + root.precipTop

                        DropletGlyph {
                            glyphSize: 11
                            anchors.verticalCenter: parent.verticalCenter
                        }

                        Text {
                            text: column.hour.precipProb + "%"
                            color: Theme.ink.muted
                            font.pixelSize: Theme.type.axis
                            anchors.verticalCenter: parent.verticalCenter
                        }
                    }

                    Text {
                        text: column.hour.label
                        color: column.isNow ? Theme.ink.primary : Theme.ink.muted
                        font.pixelSize: Theme.type.label
                        font.bold: column.isNow
                        anchors.horizontalCenter: parent.horizontalCenter
                        y: root.tempRowHeight + root.bandHeight
                           + (root.labelRowHeight - height) / 2
                    }
                }
            }
        }
    }

    // A rule under the band rather than a lighter strip behind the hours.
    // The reference tints that row, and a tint here would be a second wash
    // inside the card's own — 0.07 over 0.07 is the stacked surface §10.1
    // exists to prevent. A hairline separates the axis from the plot without
    // claiming to be a surface.
    Rectangle {
        anchors.left: parent.left
        anchors.right: parent.right
        y: root.tempRowHeight + root.bandHeight
        height: 1
        color: Theme.line.gridWeak
    }
}
