// SPDX-FileCopyrightText: 2026 Jowi Aoun
// SPDX-License-Identifier: GPL-3.0-or-later
// A rise-to-set arc: how far through its time up a body is.
//
// The phone's answer to DetailSunCard, and a different drawing on purpose.
// That card plots real altitude against a 24-hour span, so the width of the
// daylight hump *is* the day length and the tails below the horizon are drawn.
// It needs 260 px to say that. Here there are two of these side by side in
// roughly 150 px each, and at that size the altitude sinusoid is a bump with
// no readable difference between a summer day and a winter one.
//
// So this is a semicircle: not the body's path, but its progress. The track is
// the whole time up, the filled part is what has elapsed, and the mark is now.
// It claims less and it is legible at half the size — and it does not pretend
// to be a sinusoid it has not got the pixels to draw.
//
// ---- motion ------------------------------------------------------------------
// The fill sweeps from the rise to now, once, on arrival. Same hook and same
// justification as a detail card's reveal: the journey is the reading. Bind
// `reveal` from outside — the card owns the timing so both arcs on it leave
// together.
import QtQuick
import QtQuick.Shapes
import "theme.js" as Theme

Item {
    id: root

    // Minutes past midnight. `setMin` before `riseMin` is normal and means the
    // body sets the next day, which is what the moon usually does.
    property real riseMin: 0
    property real setMin: 720
    property real nowMin: 360

    property string riseLabel: ""
    property string riseSuffix: ""
    property string riseName: qsTr("Rise")
    property string setLabel: ""
    property string setSuffix: ""
    property string setName: qsTr("Set")

    // The one number the arc exists to give: how long it is up for.
    property string span: ""

    property color tint: Theme.color.accent

    // 0 → 1, driven by the card. See the note above.
    property real reveal: 1

    // What rides the mark. Left as a slot rather than a "sun or moon" enum
    // because the moon's is its phase, which is a parameter and not a picture.
    property Component mark

    readonly property real padX: 10
    readonly property real arcRadius: Math.max(18, (width - padX * 2) / 2)
    readonly property real centreX: width / 2

    implicitHeight: arcRadius + 6 + spanText.height + 10 + figures.height
    height: implicitHeight

    readonly property real baseY: arcRadius + 4

    // Elapsed fraction of the time up. Both differences are taken modulo a day
    // so a set-before-rise pair measures the up-window rather than a negative
    // number — the same normalisation DetailSunCard does, for the same reason.
    readonly property real upMin: ((setMin - riseMin) % 1440 + 1440) % 1440
    readonly property real sinceRise: ((nowMin - riseMin) % 1440 + 1440) % 1440
    readonly property real progress:
        upMin <= 0 ? 0 : Math.max(0, Math.min(1, sinceRise / upMin))

    // Angles run clockwise from 3 o'clock in Qt's y-down space, so 180 is the
    // left horizon, 270 the apex and 360 the right horizon.
    readonly property real sweptDegrees: 180 * progress * reveal
    readonly property real markAngle: (180 + sweptDegrees) * Math.PI / 180

    readonly property real markX: centreX + arcRadius * Math.cos(markAngle)
    readonly property real markY: baseY + arcRadius * Math.sin(markAngle)

    Shape {
        anchors.fill: parent
        preferredRendererType: Shape.CurveRenderer

        // The whole time up, dotted. Dotted rather than solid because it is
        // the part that has not happened: the reference draws it the same way
        // and it is the same idea as `trackLine` — the unfilled remainder of
        // something.
        ShapePath {
            fillColor: "transparent"
            strokeColor: Theme.color.trackLine
            strokeWidth: 1.6
            strokeStyle: ShapePath.DashLine
            dashPattern: [1.6, 2.6]
            capStyle: ShapePath.RoundCap

            PathAngleArc {
                centerX: root.centreX
                centerY: root.baseY
                radiusX: root.arcRadius
                radiusY: root.arcRadius
                startAngle: 180
                sweepAngle: 180
            }
        }

        // Elapsed.
        ShapePath {
            fillColor: "transparent"
            strokeColor: root.tint
            // A zero-length arc under a round cap paints a bead the width of
            // the stroke, which at reveal 0 would put a dot on the horizon
            // before anything has been drawn.
            strokeWidth: root.sweptDegrees > 0.5 ? 2.2 : -1
            capStyle: ShapePath.RoundCap

            PathAngleArc {
                centerX: root.centreX
                centerY: root.baseY
                radiusX: root.arcRadius
                radiusY: root.arcRadius
                startAngle: 180
                sweepAngle: root.sweptDegrees
            }
        }
    }

    // The horizon. Altitude zero, and the only reference the arc needs.
    Rectangle {
        x: root.centreX - root.arcRadius - 4
        y: Math.round(root.baseY)
        width: (root.arcRadius + 4) * 2
        height: 1
        color: Theme.color.gridLine
    }

    Loader {
        id: markItem
        sourceComponent: root.mark
        x: root.markX - width / 2
        y: root.markY - height / 2
    }

    Text {
        id: spanText
        text: root.span
        color: Theme.color.textPrimary
        font.pixelSize: Theme.type.label
        font.bold: true
        anchors.horizontalCenter: parent.horizontalCenter
        y: root.baseY + 8
    }

    // The two clock figures, one under each end of the arc.
    Item {
        id: figures
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: spanText.bottom
        anchors.topMargin: 10
        height: riseColumn.height

        Column {
            id: riseColumn
            spacing: 1
            anchors.left: parent.left

            Text {
                text: root.riseName
                color: Theme.color.textMuted
                font.pixelSize: Theme.type.axis
            }
            Text {
                text: root.riseLabel + " " + root.riseSuffix
                color: Theme.color.textPrimary
                font.pixelSize: Theme.type.label
                font.bold: true
            }
        }

        Column {
            spacing: 1
            anchors.right: parent.right

            Text {
                text: root.setName
                color: Theme.color.textMuted
                font.pixelSize: Theme.type.axis
                anchors.right: parent.right
            }
            Text {
                text: root.setLabel + " " + root.setSuffix
                color: Theme.color.textPrimary
                font.pixelSize: Theme.type.label
                font.bold: true
                anchors.right: parent.right
            }
        }
    }
}
