// SPDX-FileCopyrightText: 2026 Jowi Aoun
// SPDX-License-Identifier: GPL-3.0-or-later
//
// Where the sun is between rising and setting.
//
// The arc is the app's own SkyArc, unchanged. That component already knows
// that a body which sets before it rises is normal, that the mark rides a
// slot rather than a "sun or moon" enum, and how to place a label under each
// end — none of which is worth writing twice.
//
// ============================================================================
// THE MARK MOVES WHEN THE DATA DOES NOT
//
// `nowMin` comes from the reader's own clock moved into the place's offset, not
// from the snapshot's `generatedAt`. A sun that stepped forward once every five
// minutes and then froze the moment the daemon stopped would be the one thing
// on the tile that looked broken while everything around it was correct — and
// sunrise and sunset do not change between publishes, so there is nothing to
// wait for. Wx::nowMinutesInZoneOf has the caveat.
//
// ============================================================================
// WHY THE ARC IS SIZED FROM THE TILE'S HEIGHT
//
// SkyArc derives its radius from its *width* and its height from that radius,
// so anchoring it to a tile's full width makes a semicircle taller than the
// card it is in. The width below is the inverse of its own implicitHeight
// formula, which keeps it whole at whatever size somebody drags the tile to.

import QtQuick

import "wire.js" as Wire

WidgetSurface {
    id: root
    widgetId: "sun-arc"

    readonly property var daily: Wire.obj(Wire.at(root.snap, "daily"))
    readonly property var sunrise: Wire.arr(root.daily.sunrise)[0]
    readonly property var sunset: Wire.arr(root.daily.sunset)[0]

    SkyArc {
        anchors.horizontalCenter: parent.horizontalCenter
        anchors.verticalCenter: parent.verticalCenter

        // 62 px is what SkyArc puts below its arc: the span line, the gap, and
        // the two figures. Anything left over becomes the radius.
        width: Math.min(parent.width, Math.max(48, parent.height - 62) * 2 + 20)

        tint: Theme.glyph.sunWarm

        riseMin: Wx.minutesFromMidnight(root.sunrise)
        setMin: Wx.minutesFromMidnight(root.sunset)
        nowMin: Wx.nowMinutesInZoneOf(root.sunrise)

        riseLabel: Wx.clockLabel(root.sunrise)
        riseSuffix: Wx.clockSuffix(root.sunrise)
        riseName: qsTr("Sunrise")
        setLabel: Wx.clockLabel(root.sunset)
        setSuffix: Wx.clockSuffix(root.sunset)
        setName: qsTr("Sunset")
        span: Wx.spanBetween(root.sunrise, root.sunset)

        mark: Rectangle {
            width: 11
            height: 11
            radius: 5.5
            color: Theme.glyph.sunWarm
            border.width: 2
            border.color: Theme.ink.primary
        }
    }
}
