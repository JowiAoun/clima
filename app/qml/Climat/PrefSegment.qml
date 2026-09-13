// SPDX-FileCopyrightText: 2026 Jowi Aoun
// SPDX-License-Identifier: GPL-3.0-or-later
// A few mutually exclusive options, all of them visible at once.
//
//   PrefSegment {
//       options: [{ id: "24h", label: qsTr("24 hour") },
//                 { id: "12h", label: qsTr("AM / PM") }]
//       currentId: Settings.clockFormat
//       onSelected: function (id) { Settings.clockFormat = id }
//   }
//
// ---- why a segment and not a cycling row --------------------------------------
//
// MobileMePage's unit rows cycle: tap and the value advances, wrapping. That is
// the right control for five wind units on a phone, and the wrong one for two.
// A cycle of two options never shows the reader the option they are not on, so
// "AM / PM" on a row is a value with no visible alternative — it looks like a
// readout, and the way to discover it is a control is to tap it and watch
// something change. A segment says "these are the two, you are on this one"
// without being touched, which is what a preferences screen is for.
//
// It stops being the right control at about four options, where the cells get
// too narrow for their labels; at that point the answer is the menu
// MobileMetricPicker draws, not a wider segment.
//
// ---- the selection is drawn once and moved ------------------------------------
//
// Same decision as ShellNav's pill and for the same reason: one rectangle that
// travels reads as one object in a new place, and n rectangles crossfading read
// as two objects. `move` for the travel, `tint` for the labels.
//
// `Bound` because the cells are a Repeater delegate reading `root` and `cell`
// ids from the file around them, which is unqualified access to qmllint and is
// what stops qmlcachegen compiling the bindings ahead of time.
pragma ComponentBehavior: Bound

import QtQuick

Item {
    id: root

    // [{ id, label }]. The list is the caller's — usually a C++ table, so that
    // what a reader can pick and what the program understands are one list.
    property var options: []
    property string currentId: ""

    signal selected(string id)

    readonly property int currentIndex: {
        for (var i = 0; i < root.options.length; ++i)
            if (root.options[i].id === root.currentId)
                return i
        // -1, not 0. A segment whose value is none of its options — "custom"
        // units, a hand-edited INI — has to draw with nothing selected rather
        // than lie about being on the first one.
        return -1
    }

    // Every cell is the width of the widest label plus padding, so the control
    // does not resize when the selection moves and the cells are not different
    // sizes. Measured off real Text items rather than assumed from character
    // counts, because the bundled face is proportional and "AM / PM" is not the
    // width of "24 hour".
    //
    // ---- assigned, not bound, and that is not a shortcut ---------------------
    //
    // The obvious spelling is a binding that sets one hidden Text's `text` in a
    // loop and takes the widest `implicitWidth` — and it is a binding loop, which
    // Qt detects and reports on every frame: the binding reads a property it
    // has itself just written. One hidden Text per option and an assignment out
    // of `remeasure()` is the version with no cycle in it.
    //
    // The triggers matter as much as the arithmetic. `Component.onCompleted`
    // alone is not enough, because a Text reports a width for a face that has
    // not finished loading and then reports a different one — so each measurer
    // also calls back when its own width changes, which covers the font
    // arriving, a translation landing and `options` being replaced.
    property real cellWidth: 0

    implicitWidth: cellWidth * options.length + 4
    implicitHeight: 30
    width: implicitWidth
    height: implicitHeight

    function remeasure() {
        var widest = 0
        for (var i = 0; i < measure.children.length; ++i)
            widest = Math.max(widest, measure.children[i].implicitWidth)
        root.cellWidth = Math.ceil(widest) + 22
    }

    // An Item and not a Row: nothing here is positioned, only measured, and a
    // positioner would be a layout pass this control does not need. They sit on
    // top of each other at the origin, invisible, and report their widths.
    Item {
        id: measure
        visible: false

        Repeater {
            model: root.options

            delegate: Text {
                id: measured
                required property var modelData

                text: measured.modelData.label
                font.pixelSize: Theme.type.status

                onImplicitWidthChanged: root.remeasure()
                Component.onCompleted: root.remeasure()
            }
        }
    }

    Rectangle {
        anchors.fill: parent
        radius: Theme.metric.controlRadius
        color: Theme.surface.raised
    }

    Rectangle {
        id: pill
        visible: root.currentIndex >= 0
        width: root.cellWidth
        height: parent.height - 4
        y: 2
        x: 2 + Math.max(0, root.currentIndex) * root.cellWidth
        radius: Theme.metric.controlRadius
        color: Theme.accent.fill

        Behavior on x {
            NumberAnimation { duration: Theme.motion.move; easing.type: Easing.OutCubic }
        }
    }

    // ---- the cells, placed rather than positioned ----------------------------
    //
    // A Row would be the obvious container and it is the wrong one here, for a
    // reason that only shows up off screen. A positioner lays out in the polish
    // phase and skips children whose `visible` is false — and `visible` is
    // EFFECTIVE visibility, which an item in a window that has not rendered
    // reports as false all the way down (tst_hittargets' header has the same
    // note from the other direction).
    //
    // Measured, in tests/qml/tst_preferences.qml before this changed: the Row
    // reported `width: 0` with its two cells built and never positioned, stacked
    // at the origin. The control renders correctly in the app and is
    // un-clickable in a test — which is exactly the shape of bug that gets a
    // test deleted rather than a control fixed.
    //
    // `x: 2 + index * cellWidth` is the same arithmetic a Row would do, in a
    // binding, with no layout pass to depend on. A segment is n equal cells in a
    // line; there is nothing here a positioner was going to work out.
    Repeater {
        model: root.options

        delegate: Item {
            id: cell
            required property var modelData
            required property int index

            x: 2 + cell.index * root.cellWidth
            y: 2
            width: root.cellWidth
            height: root.height - 4

            readonly property bool isCurrent: cell.index === root.currentIndex

            Text {
                anchors.centerIn: parent
                text: cell.modelData.label
                color: cell.isCurrent ? Theme.accent.ink
                                      : (target.hovered ? Theme.ink.primary : Theme.ink.muted)
                font.pixelSize: Theme.type.status

                Behavior on color {
                    ColorAnimation { duration: Theme.motion.tint; easing.type: Easing.OutCubic }
                }
            }

            // A 30 px control in a 44 px row: the target grows above and below
            // into the row's own padding, and stays exactly one cell wide so two
            // adjacent options cannot steal each other's taps.
            TouchTarget {
                id: target
                onTapped: root.selected(cell.modelData.id)
            }
        }
    }
}
