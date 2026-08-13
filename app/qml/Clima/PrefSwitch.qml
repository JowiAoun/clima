// SPDX-FileCopyrightText: 2026 Jowi Aoun
// SPDX-License-Identifier: GPL-3.0-or-later
// The on/off control in a preferences row.
//
// The same switch FeelsLikeToggle draws, without the caption beside it. That is
// the whole difference between the two files and it is the reason there are two:
// the chart's toggle is a control *with a label*, laid out as one object,
// because the word "Feels like" is what the switch means. A preferences row has
// already said what the switch means, in a title and a subtitle, on the left —
// so a second caption here would be the third time the same sentence is on the
// row.
//
// Everything else is shared with it: the track and knob sizes, `accent.fill`
// when on and `control.toggleTrack` when off, `tint` for the recolours and
// `move` for the one thing that travels.
//
// ---- it does not toggle itself ------------------------------------------------
//
// `checked` is a plain property and `toggled()` is a signal; tapping emits and
// changes nothing. That is deliberate and it is the opposite of what
// FeelsLikeToggle does, because these switches are bound to a stored preference:
//
//     PrefSwitch {
//         checked: Settings.dynamicBackground
//         onToggled: Settings.dynamicBackground = !Settings.dynamicBackground
//     }
//
// A control that flipped `checked` itself would destroy that binding on the
// first tap — assigning to a bound property is what breaks the binding — and
// from then on the switch would show its own state rather than the setting's.
// The failure is invisible until something else writes the preference, which for
// the unit switches is the metric/imperial preset one group above.
import QtQuick

Item {
    id: root

    property bool checked: false

    signal toggled()

    implicitWidth: track.width
    implicitHeight: 22
    width: implicitWidth
    height: implicitHeight

    Rectangle {
        id: track
        width: 38
        height: 20
        radius: height / 2
        anchors.verticalCenter: parent.verticalCenter
        color: root.checked ? Theme.accent.fill : Theme.control.toggleTrack

        Behavior on color {
            ColorAnimation { duration: Theme.motion.tint; easing.type: Easing.OutCubic }
        }

        Rectangle {
            width: 14
            height: 14
            radius: height / 2
            y: (parent.height - height) / 2
            x: root.checked ? parent.width - width - 3 : 3
            color: root.checked ? Theme.accent.ink : Theme.control.toggleKnob

            Behavior on x {
                NumberAnimation { duration: Theme.motion.move; easing.type: Easing.OutCubic }
            }
            Behavior on color {
                ColorAnimation { duration: Theme.motion.tint; easing.type: Easing.OutCubic }
            }
        }
    }

    // 22 px tall in a 44 px row, so the target grows into padding that is
    // already there. The row is a target too — see PrefRow — and this one sits
    // on top of it, which is what makes a tap on the switch and a tap on the row
    // do the same thing rather than two things.
    TouchTarget {
        onTapped: root.toggled()
    }
}
