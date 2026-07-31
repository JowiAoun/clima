// SPDX-FileCopyrightText: 2026 Jowi Aoun
// SPDX-License-Identifier: GPL-3.0-or-later
// Bar series for quantities that are sums or banded indices rather than
// continuities: precipitation amount, UV index, air quality.
//
// Drawing rain as a smooth curve would be a lie — 0.4 mm in one hour and 0 mm in
// the next is not a gradual transition, and published indices (WHO UV, European
// AQI) are banded, so each bar takes the flat colour of its own band.
import QtQuick
import "chartmath.js" as ChartMath

Item {
    id: root

    property var values: []             // one value per hour
    property real hourWidth: 48
    property real axisTop: 0            // y of maxValue
    property real axisBottom: height    // y of minValue
    property real minValue: 0
    property real maxValue: 100
    property var ramp: []
    property real barFraction: 0.62     // of hourWidth
    property real minBarHeight: 2

    // 0 = flat on the baseline, 1 = full height. The counterpart of
    // SeriesArea.growth, so a metric switch between a curve and bars is one
    // gesture across the axis baseline rather than a cut.
    //
    // Geometry only, deliberately: `norm`, and therefore the band colour, stays
    // keyed to the bar's own value the whole way up. Scaling the *value* instead
    // would walk each bar down through the ramp as it grew — a UV 9 bar coming
    // up through green — and the colour here is the reading, not decoration.
    property real growth: 1

    // Normalised axis position: 0 at the top of the axis, 1 at the bottom.
    function normalised(v) {
        var span = maxValue - minValue
        return span <= 0 ? 1 : ChartMath.clamp((maxValue - v) / span, 0, 1)
    }

    Repeater {
        model: root.values.length

        delegate: Rectangle {
            required property int index

            // Named barTop/barValue rather than top/value: Item declares some of
            // those as FINAL, and shadowing them fails at component creation.
            readonly property real barValue: root.values[index]
            readonly property real norm: root.normalised(barValue)
            readonly property real barTop: root.axisTop + (root.axisBottom - root.axisTop) * norm
            readonly property real fullHeight: Math.max(root.minBarHeight,
                                                        root.axisBottom - barTop)

            visible: barValue > root.minValue
            width: root.hourWidth * root.barFraction
            x: index * root.hourWidth - width / 2
            // Grown off the baseline, not out of the middle: the bottom edge is
            // where the value is measured from and it does not move.
            height: Math.max(root.minBarHeight, fullHeight * root.growth)
            y: root.axisBottom - height
            radius: Math.min(3, width / 2)
            color: ChartMath.sampleRamp(root.ramp, norm)
        }
    }
}
