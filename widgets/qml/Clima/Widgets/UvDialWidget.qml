// SPDX-FileCopyrightText: 2026 Jowi Aoun
// SPDX-License-Identifier: GPL-3.0-or-later
//
// The UV index now, against today's maximum.
//
// The arc runs 0 to 11, because that is where the WHO's scale stops naming
// bands — 11 and over is "extreme" and there is nothing above it to scale
// against. A reading past 11 fills the arc and the word says the rest.
//
// Today's maximum is a footnote rather than a second arc. It is the number that
// changes the decision ("it is 3 now and it will be 9 at two o'clock"), and a
// second arc on a 160 px tile is two things to read where there was one.
//
// ============================================================================
// THE RAMP IS INDEXED FROM THE TOP OF AN AXIS, NOT FROM ZERO
//
// `Theme.ramp.uv.fill` runs purple at p = 0 to green at p = 1, because
// chartmath.js's sampleRamp() takes a *normalised axis position* and 0 is the
// top of a chart — where the biggest number is. A dial's fraction runs the
// other way, so it is inverted here.
//
// This is the sort of thing that produces a perfectly pretty widget colouring
// a UV of 10 in reassuring green, which is why it has a paragraph and a test.

import QtQuick

import "chartmath.js" as ChartMath
import "wire.js" as Wire

WidgetSurface {
    id: root
    widgetId: "uv-dial"

    readonly property real index: Wire.num(Wire.at(root.snap, "current.uvIndex"))
    readonly property real todayMax: Wire.num(Wire.arr(Wire.at(root.snap, "daily.uvIndexMax"))[0])
    readonly property real level: Wire.fraction(root.index, 0, 11)

    DialGauge {
        anchors.fill: parent

        fraction: root.level
        tint: isNaN(root.level)
              ? Theme.line.track
              : ChartMath.sampleRamp(Theme.ramp.uv.fill, 1 - root.level)
        reading: Wire.text(root.index, root.index < 10 ? 1 : 0)
        caption: Wx.uvBand(root.index)
        footnote: Wire.has(root.todayMax)
                  ? qsTr("max %1").arg(Wire.text(root.todayMax, 0))
                  : ""
    }
}
