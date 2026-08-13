// SPDX-FileCopyrightText: 2026 Jowi Aoun
// SPDX-License-Identifier: GPL-3.0-or-later
// A titled card holding a run of PrefRows.
//
//   PrefGroup {
//       title: qsTr("Units & measurements")
//       PrefRow { … }
//       PrefRow { … }
//   }
//
// Children go straight in — the default property is the column's — and the group
// takes its height from them.
//
// ---- why this is not MobileCard ----------------------------------------------
//
// MobileCard is the same three parts: a title, a rule under it, a body. It was
// the obvious thing to reuse and it is wrong here by one detail, which is that
// its body is inset by `mobileCardPadH` on both sides. A preferences row is a
// button the width of the card, and a row inset by 16 px leaves a strip down each
// edge that looks like part of the row, highlights nothing on hover, and does
// nothing when pressed. Every platform's own settings list draws the row edge to
// edge for that reason.
//
// So the padding moves from the card to the row — PrefRow owns `padH` — and this
// is the shell that lets it. The two files are also not the same shape of thing:
// MobileCard is a *mobile* card with a link-out affordance and a bleed mode,
// and this one appears on the desktop too.
//
// ---- the last row's rule -------------------------------------------------------
//
// Turned off from here rather than by the caller counting its own children. A
// rule against the bottom edge of a card is a second card edge one pixel inside
// the first, and the row that needs it switched off is the one most likely to be
// added or moved. `Column.children` is the list, so the group can always answer
// which one is last — which is exactly the kind of thing a caller should not
// have to keep in step.
import QtQuick

Item {
    id: root

    property string title: ""

    default property alias rows: column.data

    readonly property bool hasHeader: title !== ""

    implicitHeight: column.y + column.height
    height: implicitHeight

    // No border in dark, a hairline in light. `line.card` is the token that
    // carries that difference — §10.1's exception, written down in themelight.js
    // rather than branched on here.
    Rectangle {
        anchors.fill: parent
        radius: Theme.metric.cardRadius
        color: Theme.surface.base
        border.width: 1
        border.color: Theme.line.card

        // The rows run edge to edge, so the bottom one would square off the
        // card's rounded corners if anything drew outside them. Nothing does —
        // the last row has no wash of its own at rest — but a hover on it would,
        // so the corners are clipped.
        clip: true
    }

    Text {
        id: heading
        visible: root.hasHeader
        text: root.title
        color: Theme.ink.muted
        font.pixelSize: Theme.type.label
        anchors.left: parent.left
        anchors.leftMargin: 16
        anchors.top: parent.top
        anchors.topMargin: 13
    }

    Rectangle {
        id: rule
        visible: root.hasHeader
        height: visible ? 1 : 0
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: heading.bottom
        anchors.topMargin: root.hasHeader ? 11 : 0
        color: Theme.line.gridWeak
    }

    Column {
        id: column
        y: root.hasHeader ? rule.y + rule.height : 0
        anchors.left: parent.left
        anchors.right: parent.right

        // The bottom row's rule, switched off here. `children` and not `data`:
        // `data` holds every child including the non-visual ones a caller might
        // add, and the last of those is not the last row on screen.
        onChildrenChanged: root.markLast()
        Component.onCompleted: root.markLast()
    }

    // Assigns rather than binds, which is the one thing to know about it: a
    // caller that binds `ruled` on a row will have the binding destroyed here.
    // Nothing does, and nothing should — which row is last is this group's
    // question, not the row's.
    //
    // "Last" is the last child that is a row, in declaration order, and
    // deliberately not the last VISIBLE one. Effective visibility is false for
    // every item in a window that has not been shown yet, and component
    // completion runs before the show — so a visibility test here answers "none
    // of them" at exactly the moment this is called, leaves `last` null, and
    // rules every row including the bottom one. It passed by accident in the app
    // and failed in the QML suite, which builds into a view it never shows.
    function markLast() {
        var last = null
        for (var i = 0; i < column.children.length; ++i)
            // `ruled !== undefined` is what excludes the non-rows. A Repeater is
            // an Item and therefore a child of this Column — a group whose last
            // declared child is a Repeater would otherwise hand the title to a
            // zero-height object and leave the bottom row ruled against the card
            // edge.
            if (column.children[i].ruled !== undefined)
                last = column.children[i]
        for (var j = 0; j < column.children.length; ++j)
            if (column.children[j].ruled !== undefined)
                column.children[j].ruled = column.children[j] !== last
    }
}
