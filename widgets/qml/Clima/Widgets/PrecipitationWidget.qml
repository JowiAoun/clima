// SPDX-FileCopyrightText: 2026 Jowi Aoun
// SPDX-License-Identifier: GPL-3.0-or-later
//
// Rain in the next six hours: how much, and how likely.
//
// ============================================================================
// TWO NUMBERS THAT ARE ROUTINELY CONFUSED
//
// Amount and probability are not the same reading and they are not redundant.
// 0.2 mm at 90 % is a certainty of nothing much; 6 mm at 30 % is an unlikely
// soaking. A tile that showed only one of them would be answering a different
// question from the one people ask a rain widget, which is "do I need a coat".
//
// So the bar height is millimetres and the number under it is the chance. They
// are visibly different encodings on purpose — docs/04 §4.10 forbids colour as
// the only carrier of meaning, and this is the same argument about height.
//
// ============================================================================
// A DRY HOUR AND AN UNKNOWN HOUR
//
// A dry hour draws a flat hairline at the baseline. An hour the provider has no
// figure for draws nothing at all and its label is a dash. libclima's wire
// format keeps those distinguishable all the way here (rule 2 in
// libclima/wire/snapshot.h) and this is the tile that would otherwise throw the
// distinction away.

import QtQuick

import "wire.js" as Wire

WidgetSurface {
    id: root
    widgetId: "precipitation-6h"

    readonly property var hourly: Wire.obj(Wire.at(root.snap, "hourly"))
    readonly property var times: Wire.arr(root.hourly.time)
    readonly property var amounts: Wire.arr(root.hourly.precipitation)
    readonly property var chances: Wire.arr(root.hourly.precipitationProbability)

    readonly property real axisMax: {
        var extent = Wire.extent(root.amounts)
        return extent === null ? 2 : Math.max(2, extent.hi)
    }

    // The sum, which is the one number somebody actually repeats out loud.
    // Absent hours are skipped rather than counted as zero, and the total is a
    // dash when every hour is absent — "0 mm expected" is a forecast and we do
    // not have one.
    readonly property real total: {
        var sum = NaN
        for (var i = 0; i < root.amounts.length; ++i) {
            var value = Wire.num(root.amounts[i])
            if (isNaN(value))
                continue
            sum = isNaN(sum) ? value : sum + value
        }
        return sum
    }

    Text {
        id: headline
        anchors.left: parent.left
        anchors.top: parent.top
        // "– expected" is a sentence with a hole in it. When there is no figure
        // at all the tile says so in words instead.
        text: isNaN(root.total)
              ? qsTr("No rain figure")
              : qsTr("%1 expected").arg(Units.format(Units.Precipitation, root.total))
        color: Theme.ink.primary
        font.pixelSize: Math.max(14, Math.round(root.height * 0.17))
    }

    Row {
        id: strip
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: headline.bottom
        anchors.bottom: parent.bottom
        anchors.topMargin: 6
        spacing: 0

        readonly property int count: Math.min(root.times.length, 6)

        Repeater {
            model: strip.count

            Item {
                id: hour
                required property int index

                readonly property real millimetres: Wire.num(root.amounts[hour.index])
                readonly property real chance: Wire.num(root.chances[hour.index])

                width: strip.count > 0 ? strip.width / strip.count : 0
                height: strip.height

                Rectangle {
                    id: bar
                    anchors.horizontalCenter: parent.horizontalCenter
                    anchors.bottom: chanceLabel.top
                    anchors.bottomMargin: 3
                    width: Math.max(4, hour.width * 0.44)
                    radius: 2
                    visible: Wire.has(root.amounts[hour.index])
                    height: Math.max(2, (hour.height - chanceLabel.height - clock.height - 8)
                                        * Math.min(1, hour.millimetres / root.axisMax))
                    color: Theme.precip.wash.rain
                    opacity: hour.millimetres > 0 ? 0.95 : 0.3
                }

                Text {
                    id: chanceLabel
                    anchors.horizontalCenter: parent.horizontalCenter
                    anchors.bottom: clock.top
                    text: Wire.text(hour.chance, 0, "%")
                    color: hour.chance >= 50 ? Theme.ink.primary : Theme.ink.dim
                    font.pixelSize: Theme.type.axis
                }

                Text {
                    id: clock
                    anchors.horizontalCenter: parent.horizontalCenter
                    anchors.bottom: parent.bottom
                    text: Wx.hourLabel(root.times[hour.index])
                    color: Theme.ink.dim
                    font.pixelSize: Theme.type.axis
                }
            }
        }
    }
}
