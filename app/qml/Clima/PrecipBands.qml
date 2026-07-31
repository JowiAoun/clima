// SPDX-FileCopyrightText: 2026 Jowi Aoun
// SPDX-License-Identifier: GPL-3.0-or-later
// The wash that says *when* it precipitates: one tinted band per contiguous
// spell, edge to edge of the hours it covers.
//
// Drawn under the series, never over it — which is why this is a component of
// its own rather than half of `PrecipField`. Every fill on this chart uses
// colour to encode its own value, and on the banded metrics (UV, AQI) those
// colours are a published scale, so a blue wash laid over one would be stating
// a different number (§10.5). Underneath it still reads clearly, because every
// fill here is translucent — which is the same property the reference relies
// on, and measurably so: its rainy stretch shifts the area fill by about a
// third of what it shifts the empty plot above it.
import QtQuick
import "precip.js" as Precip

Item {
    id: root

    // One entry per hour, null where dry. See precip.js.
    property var cells: []
    property real hourWidth: Theme.metric.hourWidth

    // The chart's scrolling content width. The last hour's band runs to the end
    // of that hour, which is one hour-width past the last sample, so without a
    // bound it would hang off the end of the axis.
    property real contentWidth: width

    readonly property var spans: Precip.spans(root.cells)

    Repeater {
        model: root.spans

        delegate: Item {
            id: band
            required property var modelData

            x: Precip.bandX(modelData, root.hourWidth)
            width: Precip.bandW(modelData, root.hourWidth, root.contentWidth)
            height: root.height

            readonly property color wash: Theme.precip.wash[modelData.type]
                                          ? Theme.precip.wash[modelData.type]
                                          : Theme.precip.wash.rain

            Rectangle {
                anchors.fill: parent
                color: Qt.rgba(band.wash.r, band.wash.g, band.wash.b,
                               Theme.precip.washAlpha[band.modelData.intensity])
            }

            // Both edges, always — including where two spells abut. A wash that
            // changes from snow to rain in one step is two events, and the seam
            // between them is the only thing on the chart that says so.
            Rectangle {
                width: 1
                height: parent.height
                color: Theme.precip.edge
            }

            Rectangle {
                x: parent.width - 1
                width: 1
                height: parent.height
                color: Theme.precip.edge
            }
        }
    }
}
