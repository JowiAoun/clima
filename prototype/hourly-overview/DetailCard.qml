// SPDX-FileCopyrightText: 2026 Jowi Aoun
// SPDX-License-Identifier: GPL-3.0-or-later
// The shell every weather-detail card is built in.
//
// Twelve cards share one anatomy: a quiet title, a visualisation that carries
// the reading, a bold status line, and a sentence of context. Only the
// visualisation differs. Putting the other four in one place is what keeps
// twelve independently-written cards looking like one set — the alternative is
// twelve slightly different paddings and four different title sizes.
//
// Fill the `content` slot with the visualisation and nothing else. It is given
// `contentWidth` x `contentHeight` to work in and should not reach outside
// them; the card owns its own padding.
//
//   DetailCard {
//       title: "UV"
//       status: "High"
//       trend: "up"
//       body: "Maximum UV exposure today will be high, expected at 2:00 p.m."
//       content: MyVisualisation { anchors.fill: parent }
//   }
import QtQuick
import "theme.js" as Theme

Item {
    id: root

    property string title
    property string status
    property string body
    property string trend: "none"        // "up" | "down" | "steady" | "none"

    // The visualisation. Anchor it to fill; it is already the right size.
    property Component content

    // ---- the reveal --------------------------------------------------------
    // These cards have no interaction and no changing data: the provider values
    // are fixed for the life of the process, so nothing about a card ever
    // transitions from one state to another. The one honest piece of motion
    // available to them is the *arrival* — a dial sweeping up to its reading, a
    // bar growing off its baseline, a curve drawing itself in — which is worth
    // having because it shows the reader where the value sits on the scale
    // rather than just asserting it.
    //
    // `reveal` runs 0 → 1 once, shortly after the card is built. Bind whatever
    // should grow, sweep or draw to it:
    //
    // A card is a `DetailCard { id: root }`, so from inside the content slot the
    // hook is `root.reveal`:
    //
    //     PathAngleArc { sweepAngle: fullSweep * root.reveal }
    //     Rectangle { height: barHeight * root.reveal }
    //
    // Three rules, and they are not negotiable:
    //
    //   - It is a **one-shot**. Nothing re-triggers it. In particular it must
    //     never fire on scrolling into view: a grid that re-animates every time
    //     it passes the fold turns a page of information into a slot machine.
    //   - It must be **finished** well inside a second, or `--grab` starts
    //     catching cards mid-sweep and every golden image becomes a coin toss.
    //   - The card must be **readable at reveal = 0**. Titles, status lines and
    //     bodies do not fade in. If the whole card assembles itself out of
    //     nothing, the reader waits for a page they could already have read.
    property real reveal: 0
    property int revealDelay: 0        // set by the grid, to stagger the wave

    Timer {
        id: revealStart
        interval: 60 + root.revealDelay
        running: true
        onTriggered: root.reveal = 1
    }

    Behavior on reveal {
        NumberAnimation {
            duration: Theme.motion.reveal
            easing.type: Easing.OutCubic
        }
    }

    readonly property real contentWidth: card.width - Theme.metric.detailPadH * 2
    readonly property real contentHeight: contentArea.height

    // Two lines of body are reserved whether the sentence fills them or not.
    // Everything above is positioned off this reserve rather than off the text's
    // own height: measure the text and a card with a one-line body pulls its
    // status line 15px down and gets a 15px taller chart than its neighbours.
    // In a grid of twelve that misalignment is the first thing the eye finds.
    // Measured rather than computed: line spacing is not `pixelSize * lineHeight`,
    // and an arithmetic guess that comes out a few pixels short makes every body
    // in the grid elide to one line — which is exactly what it did.
    readonly property real bodyReserve: bodyProbe.height

    Text {
        id: bodyProbe
        visible: false
        text: "X\nX"
        font: bodyText.font
        lineHeight: bodyText.lineHeight
    }

    implicitWidth: Theme.metric.detailCardWidth
    implicitHeight: Theme.metric.detailCardHeight
    width: implicitWidth
    height: implicitHeight

    Rectangle {
        id: card
        anchors.fill: parent
        radius: Theme.metric.detailRadius
        color: Theme.color.surfaceBase
    }

    Text {
        id: titleText
        text: root.title
        color: Theme.color.textPrimary
        font.pixelSize: Theme.type.detailTitle
        x: Theme.metric.detailPadH
        y: Theme.metric.detailPadV
        width: root.contentWidth
        elide: Text.ElideRight
    }

    // The visualisation gets whatever is left once the four fixed rows are
    // accounted for, so every card's chart region is the same height and the
    // grid reads as a grid.
    Item {
        id: contentArea
        x: Theme.metric.detailPadH
        y: titleText.y + titleText.height + 10
        width: root.contentWidth
        height: statusRow.y - y - 10

        Loader {
            anchors.fill: parent
            sourceComponent: root.content
        }
    }

    Row {
        id: statusRow
        spacing: 6
        x: Theme.metric.detailPadH
        y: root.height - Theme.metric.detailPadV - root.bodyReserve - height - 4

        Text {
            text: root.status
            color: Theme.color.textPrimary
            font.pixelSize: Theme.type.status
            font.bold: true
        }

        TrendBadge {
            direction: root.trend
            anchors.verticalCenter: parent.verticalCenter
        }
    }

    Text {
        id: bodyText
        text: root.body
        color: Theme.color.textMuted
        font.pixelSize: Theme.type.body
        lineHeight: 1.25
        wrapMode: Text.WordWrap
        maximumLineCount: 2
        elide: Text.ElideRight
        x: Theme.metric.detailPadH
        // Positioned off the reserve, sized by its own content: a one-line body
        // leaves the slack at the bottom of the card rather than dragging the
        // status line down with it. Giving it an explicit height instead makes
        // Qt elide to whatever fits, which is not what the reserve is for.
        y: root.height - Theme.metric.detailPadV - root.bodyReserve
        width: root.contentWidth
    }
}
