// SPDX-FileCopyrightText: 2026 Jowi Aoun
// SPDX-License-Identifier: GPL-3.0-or-later
//
// Twelve hours: the sky above, the temperature under it, the rain beneath that.
//
// ============================================================================
// WHAT THIS TILE DELIBERATELY IS NOT
//
// It is not the app's HourlyOverview at a quarter of the size. That chart has
// an axis, a grid, a now-line, a hover readout and a metric selector, and every
// one of those is a thing you interact with. A tile is looked at from across
// the room while doing something else, so this is twelve columns and no
// furniture.
//
// There is also no precipitation particle field, and that is a rule rather than
// an omission: a standing animation is defensible in a chart you are looking
// at, and indefensible in something that sits on a wallpaper for eight hours.
// It is also the whole of the idle-CPU budget in docs/03-tech-stack.md §3.4.

import QtQuick

import "wire.js" as Wire

WidgetSurface {
    id: root
    widgetId: "hourly-strip"

    readonly property var hourly: Wire.obj(Wire.at(root.snap, "hourly"))
    readonly property var times: Wire.arr(root.hourly.time)
    readonly property var temperatures: Wire.arr(root.hourly.temperature)
    readonly property var rain: Wire.arr(root.hourly.precipitation)
    readonly property var codes: Wire.arr(root.hourly.weatherCode)

    // The rain axis is the tile's own, not the app's. A widget showing 0-10 mm
    // would draw an ordinary shower as a hairline; showing the largest hour in
    // the window as full height would let a 0.2 mm drizzle look like a
    // downpour. So: a floor of 2 mm, and above that it follows the data.
    readonly property real rainMax: {
        var extent = Wire.extent(root.rain)
        return extent === null ? 2 : Math.max(2, extent.hi)
    }

    Row {
        id: strip
        anchors.fill: parent
        spacing: 0

        // The number of columns, once. `root.width / count` inside the delegate
        // would be the *tile's* width rather than this row's, which is the tile
        // minus its padding — and the last column would hang over the edge.
        readonly property int count: Math.min(root.times.length, 12)

        Repeater {
            model: strip.count

            Item {
                id: hour
                required property int index

                readonly property real value: Wire.num(root.temperatures[hour.index])
                readonly property real millimetres: Wire.num(root.rain[hour.index])

                width: strip.count > 0 ? strip.width / strip.count : 0
                height: strip.height

                // Every other hour, and the empty columns keep their height so
                // the glyphs below stay on one line.
                //
                // Twelve labels across 340 px is 28 px each, and "10 PM" wants
                // 34 — so the first render ran them together into "9 PM0 PM1
                // PM", which is not a crowded axis but a wrong one: it reads as
                // an hour that does not exist. Six labels is the same
                // information at a width it fits in.
                Text {
                    id: clock
                    anchors.horizontalCenter: parent.horizontalCenter
                    anchors.top: parent.top
                    text: hour.index % 2 === 0 ? Wx.hourLabel(root.times[hour.index]) : " "
                    color: Theme.ink.dim
                    font.pixelSize: Theme.type.axis
                }

                WeatherGlyph {
                    anchors.horizontalCenter: parent.horizontalCenter
                    anchors.top: clock.bottom
                    anchors.topMargin: 3
                    glyphSize: Math.min(hour.width * 0.8, 20)
                    kind: Wx.glyphKind(root.codes[hour.index],
                                       Wire.at(root.snap, "current.isDay"))
                    ground: Theme.isLight ? "pale" : "card"
                }

                Text {
                    id: reading
                    anchors.horizontalCenter: parent.horizontalCenter
                    anchors.bottom: bar.top
                    anchors.bottomMargin: 4
                    text: Units.format(Units.Temperature, hour.value)
                    color: Theme.ink.primary
                    font.pixelSize: Theme.type.label
                }

                // The rain, as a bar under the temperature rather than as a
                // wash behind it. A wash needs a plot to sit in and this column
                // is 28 px wide.
                //
                // `visible` on a real reading, not on a non-zero one: an hour
                // the provider has no precipitation figure for draws nothing,
                // and an hour it says is dry draws a hairline. Those are
                // different facts and a tile that renders both as nothing is
                // the null-as-zero mistake upside down.
                Rectangle {
                    id: bar
                    anchors.horizontalCenter: parent.horizontalCenter
                    anchors.bottom: parent.bottom
                    width: Math.max(3, hour.width * 0.42)
                    radius: width / 2
                    visible: Wire.has(root.rain[hour.index])
                    height: Math.max(2, (strip.height * 0.24)
                                        * Math.min(1, hour.millimetres / root.rainMax))
                    color: Theme.precip.wash[Wx.precipType(root.codes[hour.index])] !== undefined
                           ? Theme.precip.wash[Wx.precipType(root.codes[hour.index])]
                           : Theme.precip.wash.rain
                    opacity: hour.millimetres > 0 ? 0.9 : 0.25
                }
            }
        }
    }
}
