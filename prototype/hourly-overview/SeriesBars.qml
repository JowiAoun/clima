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

            visible: barValue > root.minValue
            width: root.hourWidth * root.barFraction
            x: index * root.hourWidth - width / 2
            y: Math.min(barTop, root.axisBottom - root.minBarHeight)
            height: Math.max(root.minBarHeight, root.axisBottom - barTop)
            radius: Math.min(3, width / 2)
            color: ChartMath.sampleRamp(root.ramp, norm)
        }
    }
}
