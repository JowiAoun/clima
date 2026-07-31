// SPDX-FileCopyrightText: 2026 Jowi Aoun
// SPDX-License-Identifier: GPL-3.0-or-later
// Hand-rolled switch so the prototype needs nothing beyond QtQuick + Shapes.
import QtQuick

Item {
    id: root

    property bool checked: false
    property string label: "Feels like"

    implicitHeight: 22
    implicitWidth: track.width + gap + caption.implicitWidth
    height: implicitHeight
    width: implicitWidth

    readonly property real gap: 10

    Rectangle {
        id: track
        width: 38
        height: 20
        radius: height / 2
        anchors.verticalCenter: parent.verticalCenter
        color: root.checked ? Theme.color.accent : Theme.color.toggleTrack
        Behavior on color {
            ColorAnimation { duration: Theme.motion.tint; easing.type: Easing.OutCubic }
        }

        Rectangle {
            id: knob
            width: 14
            height: 14
            radius: height / 2
            y: (parent.height - height) / 2
            x: root.checked ? parent.width - width - 3 : 3
            color: root.checked ? Theme.color.onAccent : Theme.color.toggleKnob

            // The knob travels, so it takes `move`; everything else here is a
            // recolour and takes `tint`. The knob landing a beat after the
            // track has finished changing colour is the point — it is the one
            // part of the control that actually goes somewhere.
            Behavior on x {
                NumberAnimation { duration: Theme.motion.move; easing.type: Easing.OutCubic }
            }
            Behavior on color {
                ColorAnimation { duration: Theme.motion.tint; easing.type: Easing.OutCubic }
            }
        }
    }

    Text {
        id: caption
        text: root.label
        color: root.checked ? Theme.color.textPrimary : Theme.color.textMuted
        font.pixelSize: 12
        anchors.verticalCenter: parent.verticalCenter
        x: track.width + root.gap
        Behavior on color {
            ColorAnimation { duration: Theme.motion.tint; easing.type: Easing.OutCubic }
        }
    }

    MouseArea {
        anchors.fill: parent
        cursorShape: Qt.PointingHandCursor
        onClicked: root.checked = !root.checked
    }
}
