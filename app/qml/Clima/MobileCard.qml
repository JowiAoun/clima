// SPDX-FileCopyrightText: 2026 Jowi Aoun
// SPDX-License-Identifier: GPL-3.0-or-later
// The shell every card on a mobile screen is built in.
//
// Same argument as DetailCard: a title, an optional link out to the screen
// that goes deeper, a rule, and a body. Only the body differs. Six cards
// written independently produced four header heights and three link colours
// before this existed.
//
//   MobileCard {
//       width: parent.width
//       title: qsTr("Today")
//       link: qsTr("Hourly Forecast")
//       onLinkActivated: shell.tab = "hourly"
//       content: MyBody { }
//   }
//
// The body reports its own height and the card grows to it. That is the
// opposite of DetailCard, which is a fixed 300x250 box in a grid — on a phone
// a card is as tall as what is in it, and a fixed height would mean either a
// clipped hourly strip or a pollen card with a hand-span of nothing under it.
//
// `bleed` gives the body the card's full width with no inset. The hourly strip
// needs it: its shaded band is a horizon that runs edge to edge, and a band
// with 16 px of card showing either side of it stops reading as one.
import QtQuick
import "theme.js" as Theme

Item {
    id: root

    property string title
    property string link: ""
    property bool bleed: false

    // The body. Anchor nothing — it is given a width and asked how tall it is.
    property Component content

    signal linkActivated()

    readonly property real padH: Theme.metric.mobileCardPadH
    readonly property real padV: Theme.metric.mobileCardPadV
    readonly property bool hasHeader: title !== ""
    readonly property real contentWidth: bleed ? width : width - padH * 2

    implicitHeight: body.y + body.height + padV
    height: implicitHeight

    // No border. Contrast against the page defines a card — §10.1.
    Rectangle {
        anchors.fill: parent
        radius: Theme.metric.cardRadius
        color: Theme.color.cardBg
    }

    // ---- header ------------------------------------------------------------
    Item {
        id: header
        visible: root.hasHeader
        height: visible ? Math.max(headerTitle.height, linkRow.height) + root.padV * 2 : 0
        anchors.left: parent.left
        anchors.right: parent.right

        Text {
            id: headerTitle
            text: root.title
            color: Theme.color.textPrimary
            font.pixelSize: Theme.type.cardTitle
            font.bold: true
            anchors.left: parent.left
            anchors.leftMargin: root.padH
            anchors.verticalCenter: parent.verticalCenter
            // The link is allowed to elide, the title is not: a card whose
            // title reads "Health & Activi…" has lost the only thing telling
            // the reader what they are looking at.
            width: Math.min(implicitWidth, parent.width - root.padH * 2 - linkRow.width - 12)
            elide: Text.ElideRight
        }

        Row {
            id: linkRow
            visible: root.link !== ""
            spacing: 3
            anchors.right: parent.right
            anchors.rightMargin: root.padH
            anchors.verticalCenter: parent.verticalCenter

            Text {
                id: linkText
                text: root.link
                color: linkHover.hovered ? Theme.color.textPrimary : Theme.color.textMuted
                font.pixelSize: Theme.type.status
                anchors.verticalCenter: parent.verticalCenter

                Behavior on color {
                    ColorAnimation { duration: Theme.motion.tint; easing.type: Easing.OutCubic }
                }
            }

            ChevronGlyph {
                direction: "right"
                glyphSize: 15
                tint: linkText.color
                anchors.verticalCenter: parent.verticalCenter
            }

            // Declared inside the Row, not beside it, so the hover region is
            // the link and not the whole header. A pointer handler is not a
            // visual item, so it takes no cell in the Row's layout.
            //
            // On the header this would tint the link from anywhere along a
            // 350 px bar — promising a tap where there is nothing to tap.
            HoverHandler { id: linkHover; cursorShape: Qt.PointingHandCursor }
            TapHandler { onTapped: root.linkActivated() }
        }
    }

    // A rule, not a gap. The header names what is below it, so the two belong
    // to each other more than either belongs to the next card — and space
    // alone, which is what separates sections on this page, would say the
    // opposite.
    Rectangle {
        id: rule
        visible: root.hasHeader
        height: visible ? 1 : 0
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: header.bottom
        color: Theme.color.gridLineWeak
    }

    Loader {
        id: body
        sourceComponent: root.content
        width: root.contentWidth
        x: root.bleed ? 0 : root.padH
        y: rule.y + rule.height + root.padV
    }
}
