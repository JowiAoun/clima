// SPDX-FileCopyrightText: 2026 Jowi Aoun
// SPDX-License-Identifier: GPL-3.0-or-later
//
// A day of temperature as one unlabelled line.
//
// ============================================================================
// THE AXIS TRAVELS EVEN THOUGH NOTHING DRAWS IT
//
// This is the widget the wire format's rule 1 was written for. The tile asks
// for `hourly.temperature` and the daemon sends `hourly.time` whether it was
// asked or not, because twelve numbers with no axis look perfectly usable and
// are silently wrong the moment the slice starts anywhere other than the hour
// you assumed. Nothing here prints a time — and the first and last labels below
// come off that axis, so a slice that started an hour late would say so.
//
// ============================================================================
// A GAP IS A GAP
//
// Wire.points() leaves an absent hour out of the path rather than drawing it at
// the baseline. A dive to the bottom of the plot is what a missing reading
// looks like if you let Number(null) happen, and on a sparkline it reads as a
// cold snap.

import QtQuick

import "wire.js" as Wire

WidgetSurface {
    id: root
    widgetId: "temperature-sparkline"

    readonly property var hourly: Wire.obj(Wire.at(root.snap, "hourly"))
    readonly property var series: Wire.arr(root.hourly.temperature)
    readonly property var range: Wire.extent(root.series)
    readonly property var axis: Wire.padded(root.range, 3)

    Text {
        id: nowReading
        anchors.left: parent.left
        anchors.top: parent.top
        text: Units.format(Units.Temperature,
                           Wire.num(Wire.at(root.snap, "current.temperature")))
        color: Theme.ink.primary
        font.pixelSize: Math.max(18, Math.round(root.height * 0.22))
        font.bold: true
    }

    // The range the line covers, which is the sentence a sparkline cannot say
    // for itself. Without it the shape is relative to nothing.
    Text {
        anchors.right: parent.right
        anchors.baseline: nowReading.baseline
        text: root.range === null
              ? ""
              : qsTr("%1 to %2")
                    .arg(Units.format(Units.Temperature, root.range.lo))
                    .arg(Units.format(Units.Temperature, root.range.hi))
        color: Theme.ink.dim
        font.pixelSize: Theme.type.axis
    }

    Item {
        id: plot
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: nowReading.bottom
        anchors.bottom: labels.top
        anchors.topMargin: 4
        anchors.bottomMargin: 2

        SeriesArea {
            anchors.fill: parent
            points: Wire.points(root.series, plot.width, plot.height,
                                root.axis.lo, root.axis.hi)
            baselineY: plot.height
            gradientTop: 0
            gradientBottom: plot.height
            fillRamp: Theme.ramp.temp.fill
            lineRamp: Theme.ramp.temp.line
            lineWidth: 2
        }
    }

    // First and last hour, off the axis the daemon sent. Two labels rather than
    // a scale: a sparkline that needs a scale is a chart, and there is one of
    // those on the tile next to it.
    Item {
        id: labels
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        height: fromLabel.implicitHeight

        Text {
            id: fromLabel
            anchors.left: parent.left
            text: Wx.clockTime(Wire.arr(root.hourly.time)[0])
            color: Theme.ink.dim
            font.pixelSize: Theme.type.axis
        }

        Text {
            anchors.right: parent.right
            text: Wx.clockTime(Wire.arr(root.hourly.time)[Wire.arr(root.hourly.time).length - 1])
            color: Theme.ink.dim
            font.pixelSize: Theme.type.axis
        }
    }
}
