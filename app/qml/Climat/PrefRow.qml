// SPDX-FileCopyrightText: 2026 Jowi Aoun
// SPDX-License-Identifier: GPL-3.0-or-later
// One line of a preferences screen: what the setting is, what it does, and the
// control that changes it.
//
//   PrefRow {
//       title: qsTr("Dynamic background")
//       subtitle: qsTr("The page follows the sky over the place on screen.")
//       control: PrefSwitch { checked: …; onToggled: … }
//   }
//
// ---- the subtitle is not decoration -------------------------------------------
//
// It is the reason this component exists rather than MobileMePage's one-line
// settings row, which is a title on the left and a value on the right. That
// shape is right for "Wind — km/h", where the title names a quantity everybody
// already understands. It is wrong for every setting on this screen: "Dynamic
// background" does not say what it does, and a switch with no sentence under it
// is a switch you have to flip to find out.
//
// So the subtitle is a required part of the row and not an option on it. Rows
// that genuinely have nothing to add — the five per-quantity unit rows — pass
// none and get the compact shape back, which is the one case where the title
// really is self-describing.
//
// ---- the whole row is the target ----------------------------------------------
//
// Not the control. A 38 px switch at the right-hand end of a 500 px row is a
// long way to aim for a thing whose label is on the far left, and every
// platform's own settings screen makes the row itself the button. `activated()`
// fires for a tap anywhere the control is not; the control's own target sits on
// top of it and fires its own signal, so a caller wires both to the same line.
//
// A row with no `onActivated` handler is not tappable at all: no hover wash, no
// pointing cursor, nothing to promise an interaction that does not happen. That
// is what `interactive` reads — see below — rather than a flag the caller has to
// remember to set in step with the handler.
import QtQuick

Item {
    id: root

    property string title
    property string subtitle: ""

    // The control at the trailing edge — a switch, a segment. Given as a
    // Component so the row owns its placement and the caller owns its bindings.
    property Component control

    // The mark at the LEADING edge, before the title. Only the radio dot uses
    // it, and it is a separate slot rather than a second `control` because the
    // two sit on opposite sides of the text and a row could want both.
    property Component leading

    // The hairline under the row. The group turns it off for the last one — a
    // rule against the bottom edge of a card is a second card edge 1 px inside
    // the first.
    property bool ruled: true

    signal activated()

    // Whether anything is listening. `activated` is a signal, and QML has no way
    // to ask a signal how many connections it has — but an unconnected signal
    // and a connected one differ in exactly one observable way, which is whether
    // the caller wrote the handler. So the caller states it, and the default is
    // "yes" because a row with a control is nearly always tappable.
    property bool interactive: true

    readonly property real padH: 16
    readonly property real padV: 11

    // ---- when the control does not fit beside the words ----------------------
    //
    // A three-position segment is 220 px and a two-position one is 162. On a
    // 390 px phone, where a row is 362 px wide, that leaves 98 px for a title
    // and a sentence — and "Used everywhere a time appears, including the
    // desktop widgets" comes out five ragged lines deep beside a control one
    // line tall. The row is legible and it looks like a mistake.
    //
    // So when the words would be squeezed the control moves under them, right
    // aligned, and the sentence gets the full width. On the desktop sheet a row
    // is 528 px and the same segment leaves 264, so nothing stacks there.
    //
    // ---- stated as a floor under the text, not a fraction of the row --------
    //
    // This was `controlWidth > width * 0.45` first, and the two numbers it had
    // to separate were 220 px of segment against a 223 px threshold — three
    // pixels, which a translated label or a different face would have crossed
    // without anybody touching this file. The question is not what fraction the
    // control takes; it is whether a sentence still fits beside it. 200 px is
    // about thirty characters at `type.label`, which is the width below which a
    // subtitle stops being a line and starts being a column.
    //
    // A measurement rather than a viewport class, because the row does not know
    // which shell it is in and should not: what it needs to know is whether two
    // things fit side by side, and that is a question about its own width.
    //
    // ---- measured off the control's IMPLICIT size --------------------------
    //
    // Never off the Loader's own geometry. Reading `controlSlot.width` here is
    // the obvious spelling and it is a binding loop: a Loader resizes its item to
    // its own geometry and takes its geometry from the item, so a layout decision
    // that reads the Loader's width and then changes the row's height comes back
    // round through QQuickLoader::updateSize(). Qt reports it as a loop on
    // `stacked` and on `implicitHeight`, one frame apart, and neither message
    // names the Loader.
    //
    // The item's implicit width is the number this actually wants anyway: how
    // wide the control WANTS to be, which is the question "does it fit".
    // `as Item` rather than reading through `Loader.item` directly, which is
    // declared as QObject: `controlSlot.item.implicitWidth` resolves at run time
    // and is an unresolvable member lookup to qmllint, which means qmlcachegen
    // cannot compile these two ahead of time either.
    readonly property Item controlItem: controlSlot.item as Item
    readonly property Item leadingItem: leadingSlot.item as Item

    readonly property real controlWidth: controlItem ? controlItem.implicitWidth : 0
    readonly property real controlHeight: controlItem ? controlItem.implicitHeight : 0

    readonly property real minTextWidth: 200

    readonly property bool stacked:
        root.controlWidth > 0
        && root.width - root.padH * 2 - root.controlWidth - 12 < root.minTextWidth

    readonly property real gapAboveControl: 10

    width: parent ? parent.width : 0
    implicitHeight: root.stacked
        ? text.height + root.gapAboveControl + root.controlHeight + root.padV * 2
        : Math.max(Theme.metric.hitMin, text.height + root.padV * 2)
    height: implicitHeight

    // The hover wash. Edge to edge inside the group, because the row is the
    // target — a wash inset from the card's sides would say the strip in the
    // margin is not part of what you are about to press.
    Rectangle {
        anchors.fill: parent
        color: (root.interactive && rowTarget.hovered) ? Theme.surface.raised : "transparent"

        Behavior on color {
            ColorAnimation { duration: Theme.motion.tint; easing.type: Easing.OutCubic }
        }
    }

    // FIRST of the interactive children, and that is the whole of what makes
    // the control on the right still work. QML stacks later siblings above
    // earlier ones, so a row-wide target declared last would sit on top of the
    // switch and swallow the taps it exists to receive. Declared here, the
    // switch's own target is above it: a tap on the switch emits `toggled()`, a
    // tap anywhere else emits `activated()`, and a caller wires both to the same
    // line. See TouchTarget's header, which carries AlertBanner's scar from
    // getting this the other way round.
    TouchTarget {
        id: rowTarget
        enabled: root.interactive
        cursorShape: root.interactive ? Qt.PointingHandCursor : Qt.ArrowCursor
        onTapped: root.activated()
    }

    Loader {
        id: leadingSlot
        sourceComponent: root.leading
        anchors.left: parent.left
        anchors.leftMargin: root.padH
        anchors.verticalCenter: parent.verticalCenter
    }

    // Placed with `y` rather than anchored to the vertical centre, because the
    // two arrangements put it in two different places and a conditional anchor
    // leaves the anchor from the other branch still attached. ShellNav's
    // hairline has the same note for the same reason.
    Loader {
        id: controlSlot
        sourceComponent: root.control
        width: root.controlWidth
        height: root.controlHeight
        x: root.width - width - root.padH
        y: root.stacked ? text.y + text.height + root.gapAboveControl
                        : (root.height - height) / 2
    }

    Column {
        id: text
        spacing: 2
        anchors.left: parent.left
        anchors.leftMargin:
            root.padH + (root.leadingItem ? root.leadingItem.implicitWidth + 12 : 0)
        anchors.right: parent.right
        // 12 px of air between the last word and the control, and none when
        // there is no control — an unloaded Loader is 0 wide, so the margin has
        // to be conditional or every row without a control ends 12 px short of
        // the ones that have one. Stacked, the words get the whole width.
        anchors.rightMargin: root.padH
            + (root.stacked ? 0 : (root.controlWidth > 0 ? root.controlWidth + 12 : 0))
        y: root.stacked ? root.padV : (root.height - height) / 2

        Text {
            width: parent.width
            text: root.title
            color: Theme.ink.primary
            font.pixelSize: Theme.type.status
            elide: Text.ElideRight
        }

        Text {
            width: parent.width
            visible: root.subtitle !== ""
            text: root.subtitle
            color: Theme.ink.dim
            font.pixelSize: Theme.type.label
            // Wrapped, not elided. A subtitle is a sentence and half a sentence
            // is worse than a taller row — the row grows, which is what
            // `implicitHeight` is measured from.
            wrapMode: Text.WordWrap
        }
    }

    Rectangle {
        visible: root.ruled
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        height: 1
        color: Theme.line.gridWeak
    }
}
