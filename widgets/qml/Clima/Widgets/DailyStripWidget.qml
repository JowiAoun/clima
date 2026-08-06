// SPDX-FileCopyrightText: 2026 Jowi Aoun
// SPDX-License-Identifier: GPL-3.0-or-later
//
// A week, as seven columns.
//
// ============================================================================
// THE BAR IS THE WEEK'S RANGE, NOT EACH DAY'S
//
// Every day's high-low bar is positioned inside the *week's* range, so a warm
// Thursday sits visibly higher than a cold Monday. Scaling each bar to its own
// day would make every day look identical, which is the one thing a seven-day
// strip is for noticing.
//
// A day with no reading gets no bar rather than a bar at the bottom. The
// numbers beside it are dashes and the column is visibly empty, which is what
// "the model does not go that far" should look like.

import QtQuick

import "wire.js" as Wire

WidgetSurface {
    id: root
    widgetId: "daily-strip"

    readonly property var daily: Wire.obj(Wire.at(root.snap, "daily"))
    readonly property var dates: Wire.arr(root.daily.date)
    readonly property var highs: Wire.arr(root.daily.temperatureMax)
    readonly property var lows: Wire.arr(root.daily.temperatureMin)
    readonly property var codes: Wire.arr(root.daily.weatherCode)

    // The week's own axis. `padded` gives a couple of degrees of headroom so a
    // flat week is a band across the middle rather than bars filling the tile.
    readonly property var axis: Wire.padded(
        Wire.extent(root.highs.concat(root.lows)), 6)

    Row {
        id: strip
        anchors.fill: parent

        readonly property int count: Math.min(root.dates.length, 7)

        Repeater {
            model: strip.count

            Item {
                id: day
                required property int index

                readonly property real high: Wire.num(root.highs[day.index])
                readonly property real low: Wire.num(root.lows[day.index])
                readonly property bool known: !isNaN(day.high) && !isNaN(day.low)

                width: strip.count > 0 ? strip.width / strip.count : 0
                height: strip.height

                Text {
                    id: name
                    anchors.horizontalCenter: parent.horizontalCenter
                    anchors.top: parent.top
                    // Today is "Today" and not "Thu". A seven-day strip whose
                    // first column is a weekday name is one somebody has to
                    // work out the start of.
                    text: day.index === 0 ? qsTr("Today") : Wx.shortDay(root.dates[day.index])
                    color: day.index === 0 ? Theme.ink.primary : Theme.ink.dim
                    font.pixelSize: Theme.type.axis
                }

                WeatherGlyph {
                    id: glyph
                    anchors.horizontalCenter: parent.horizontalCenter
                    anchors.top: name.bottom
                    anchors.topMargin: 2
                    glyphSize: Math.min(day.width * 0.7, 18)
                    // A daily code has no hour, so it is drawn as a daytime
                    // sky. A moon over a summary of a whole day would be wrong
                    // for sixteen hours of it.
                    kind: Wx.glyphKind(root.codes[day.index], 1)
                    onLightBackground: Theme.isLight
                }

                // The bar, spanning low to high inside the week's range.
                Rectangle {
                    id: bar
                    anchors.horizontalCenter: parent.horizontalCenter
                    width: Math.max(3, day.width * 0.22)
                    radius: width / 2
                    visible: day.known
                    color: Theme.accent.fill
                    opacity: 0.85

                    readonly property real track: highLabel.y - (glyph.y + glyph.height) - 8
                    readonly property real span: root.axis.hi - root.axis.lo

                    y: glyph.y + glyph.height + 4
                       + bar.track * (1 - (day.high - root.axis.lo) / bar.span)
                    height: Math.max(2, bar.track * (day.high - day.low) / bar.span)
                }

                Text {
                    id: highLabel
                    anchors.horizontalCenter: parent.horizontalCenter
                    anchors.bottom: lowLabel.top
                    text: Units.format(Units.Temperature, day.high)
                    color: Theme.ink.primary
                    font.pixelSize: Theme.type.axis
                }

                Text {
                    id: lowLabel
                    anchors.horizontalCenter: parent.horizontalCenter
                    anchors.bottom: parent.bottom
                    text: Units.format(Units.Temperature, day.low)
                    color: Theme.ink.dim
                    font.pixelSize: Theme.type.axis
                }
            }
        }
    }
}
