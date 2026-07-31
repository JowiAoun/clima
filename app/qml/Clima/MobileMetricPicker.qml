// SPDX-FileCopyrightText: 2026 Jowi Aoun
// SPDX-License-Identifier: GPL-3.0-or-later
// The metric chooser on the Hourly screen: a button that opens a list.
//
// The desktop puts ten metrics in a row of pills, which is the right control
// when they all fit — every option is visible, switching is one tap, and the
// selected one is legible in the reader's peripheral vision while they read
// the chart. None of that survives 362 px. Ten pills there is a horizontally
// scrolling row where the option you want is usually off-screen, and a
// scrolling row of controls directly above a scrolling chart is two things
// that both move sideways under the same thumb.
//
// So the phone trades visibility for reach: one button that says what is
// selected, and a list that shows all ten when asked. It is the same registry
// behind both — metrics.js — so a metric added there appears in both controls.
//
// ---- motion ------------------------------------------------------------------
// The list fades at `tint`. §10.6's "text does not fade" is about a component
// assembling its own resting content, where the reader is left waiting for
// something they asked to read. This is a transient overlay: it is not there,
// then it is, and the fade is what stops it appearing to have been there all
// along. Nothing else moves — no slide, no scale, no stagger down the rows.
import QtQuick
import "metrics.js" as Metrics

Item {
    id: root

    property string currentId: "overview"
    property bool open: false

    signal picked(string id)

    readonly property var metric: Metrics.byId(currentId)

    implicitWidth: button.width
    implicitHeight: button.height
    width: implicitWidth
    height: implicitHeight

    // ---- the button ---------------------------------------------------------
    Rectangle {
        id: button
        width: label.width + chevron.width + 26
        height: 34
        radius: Theme.metric.controlRadius
        color: root.open || hover.hovered ? Theme.color.surfaceRaised
                                          : Theme.color.surfaceBase
        border.width: 1
        border.color: root.open ? Theme.color.accent : Theme.color.switchBorder

        Behavior on color {
            ColorAnimation { duration: Theme.motion.tint; easing.type: Easing.OutCubic }
        }
        Behavior on border.color {
            ColorAnimation { duration: Theme.motion.tint; easing.type: Easing.OutCubic }
        }

        Text {
            id: label
            text: root.metric.label
            color: Theme.color.textPrimary
            font.pixelSize: Theme.type.status
            x: 11
            anchors.verticalCenter: parent.verticalCenter
        }

        ChevronGlyph {
            id: chevron
            // It does turn over, and it is allowed to: unlike LocationBar's,
            // this one discloses something the app actually has.
            direction: root.open ? "up" : "down"
            glyphSize: 14
            tint: Theme.color.textMuted
            anchors.left: label.right
            anchors.leftMargin: 5
            anchors.verticalCenter: parent.verticalCenter
        }

        HoverHandler { id: hover; cursorShape: Qt.PointingHandCursor }
        TapHandler { onTapped: root.open = !root.open }
    }

    // ---- the list -----------------------------------------------------------
    // Right-aligned under the button, because the button is right-aligned in
    // its row and a menu that opens leftward off the screen edge is the one
    // failure this control cannot recover from.
    Rectangle {
        id: menu
        width: 190
        height: menuColumn.height + 10
        radius: Theme.metric.panelRadius
        color: Theme.color.menuBg
        border.width: 1
        border.color: Theme.color.menuBorder

        anchors.right: button.right
        anchors.top: button.bottom
        anchors.topMargin: 6

        opacity: root.open ? 1 : 0
        visible: opacity > 0
        // `enabled` follows, or an invisible menu keeps taking the taps meant
        // for the chart underneath it — the same trap §10.8 records for the
        // chart hidden behind the list view.
        enabled: root.open

        Behavior on opacity {
            NumberAnimation { duration: Theme.motion.tint; easing.type: Easing.OutCubic }
        }

        Column {
            id: menuColumn
            y: 5
            width: parent.width

            Repeater {
                model: Metrics.list

                delegate: Rectangle {
                    id: option
                    required property var modelData

                    readonly property bool isCurrent: modelData.id === root.currentId

                    width: menuColumn.width
                    height: 38
                    color: optionHover.hovered ? Theme.color.surfaceRaised : "transparent"

                    Behavior on color {
                        ColorAnimation { duration: Theme.motion.tint; easing.type: Easing.OutCubic }
                    }

                    Text {
                        text: option.modelData.label
                        color: Theme.color.textPrimary
                        font.pixelSize: Theme.type.status
                        font.bold: option.isCurrent
                        x: 14
                        anchors.verticalCenter: parent.verticalCenter
                    }

                    // A tick, not a highlighted row: the row highlight is
                    // already spoken for by hover, and a control where
                    // selection and hover look alike is one you have to move
                    // the pointer away from to read.
                    Text {
                        visible: option.isCurrent
                        text: "✓"
                        color: Theme.color.accent
                        font.pixelSize: Theme.type.status
                        font.bold: true
                        anchors.right: parent.right
                        anchors.rightMargin: 14
                        anchors.verticalCenter: parent.verticalCenter
                    }

                    HoverHandler { id: optionHover; cursorShape: Qt.PointingHandCursor }
                    TapHandler {
                        onTapped: {
                            root.picked(option.modelData.id)
                            root.open = false
                        }
                    }
                }
            }
        }
    }
}
