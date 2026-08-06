// SPDX-FileCopyrightText: 2026 Jowi Aoun
// SPDX-License-Identifier: GPL-3.0-or-later
//
// The European air-quality index, its band, and the species driving it.
//
// The European index everywhere, including in North America, because that is
// what the app's card shows everywhere. Two scales for the same air would let
// a tile and the card behind it disagree — 42 "moderate" on one and 78 "good"
// on the other, both correct, on the same desktop. libclima/wire/snapshot.cpp
// makes that choice once, on the wire.
//
// The arc runs 0 to 100 because the EAQI's named bands stop there; over 100 is
// "extremely poor" and fills it.
//
// The ramp inversion is the same one UvDialWidget.qml has a paragraph about:
// `Theme.ramp.aqi.fill` is indexed by axis position, where 0 is the top.

import QtQuick

import "chartmath.js" as ChartMath
import "wire.js" as Wire

WidgetSurface {
    id: root
    widgetId: "aqi-dial"

    readonly property var air: Wire.obj(Wire.at(root.snap, "airquality"))
    readonly property real index: Wire.num(root.air.index)
    readonly property real level: Wire.fraction(root.index, 0, 100)

    DialGauge {
        anchors.fill: parent

        fraction: root.level
        tint: isNaN(root.level)
              ? Theme.line.track
              : ChartMath.sampleRamp(Theme.ramp.aqi.fill, 1 - root.level)
        reading: Wire.text(root.index, 0)
        caption: Wx.aqiBand(root.index)

        // The dominant pollutant, when the provider names one. Blank rather
        // than "—": an index with no dominant species is a normal reading, not
        // a missing one, and a dash there would read as a hole in the data.
        footnote: Wx.pollutant(root.air.dominant)
    }
}
