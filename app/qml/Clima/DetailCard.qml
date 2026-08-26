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

Item {
    id: root

    property string title
    property string status
    property string body
    property string trend: "none"        // "up" | "down" | "steady" | "none"

    // The visualisation. Anchor it to fill; it is already the right size.
    property Component content

    // ---- the reveal --------------------------------------------------------
    // These cards have no changing data: the provider values are fixed for the
    // life of the process, so nothing about a card ever transitions from one
    // state to another on its own. The piece of motion that is honest anyway is
    // the *arrival* — a dial sweeping up to its reading, a bar growing off its
    // baseline, a curve drawing itself in — which is worth having because it
    // shows the reader where the value sits on the scale rather than just
    // asserting it.
    //
    // It is no longer the only one. Pointing at a card asks it a question, and
    // what a card is allowed to answer with is the subject of the hover block
    // further down.
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

    // ---- the hover ---------------------------------------------------------
    // The second occasion for motion on a card, and the reveal block above
    // rules out the first premise it was written on: these cards do have an
    // interaction after all. Pointing at one is a question, and the question
    // is "tell me more".
    //
    // What a card may answer with is narrow, and the rule has to be written
    // down or the grid becomes twelve fidgets:
    //
    //   **Hover says the one true thing the resting card leaves out.**
    //
    // For most of them that thing is already on the card, unsaid. Every card
    // carries a trend badge; a badge is a direction with no magnitude, and the
    // value it was computed against is sitting in the same data. So the cards
    // whose visualisation is a scale walk their paint head from the reading to
    // that value and back — the UV dial climbs to today's peak, the air-quality
    // and cloud dials to where they will be in three hours, the sight line out
    // to the day's clearest. Sun and Moon walk their mark to the crossing their
    // stretch is measured by, which is the same sentence told in time rather
    // than in units. Moon phase advances the terminator two nights, because a
    // waxing crescent and a waning one are the same picture. Wind is the only
    // card that answers with something that is not a number: its second fact is
    // that wind does not hold still, and on this card it is holding still.
    //
    // Four cards answer with the tint and nothing else, and that is a finding
    // rather than an omission. Temperature, Feels like and Pressure draw twelve
    // hours of history and their badge points at an hour three ahead — off the
    // right-hand edge of a chart that does not hold the future, and a card must
    // not draw data it does not have. Precipitation's columns are already the
    // whole story, and Humidity's comparison hour is one of the eight bars it
    // has drawn.
    //
    // Two rules, and like the reveal's they are not negotiable:
    //
    //   - Everything a card moves on hover is multiplied by `hoverPhase`, which
    //     is exactly 0 at rest. That is what keeps a resting card identical to
    //     the card before any of this existed, which is what the golden images
    //     assert and how they can go on asserting it.
    //   - `hoverPhase` is pinned to 0 under `Theme.stillness`, so a reader who
    //     asked their desktop for less movement gets the tint alone, and a
    //     capture cannot be caught mid-gesture even with something under the
    //     pointer. The tint itself survives, because a colour is a state and
    //     not a movement.
    //
    // Mouse and touchpad only. A touch screen delivers a synthetic hover that
    // arrives with the press and never leaves, so on a phone every card the
    // reader had ever touched would still be lit — and the phone does not draw
    // this grid to be pointed at in the first place.
    HoverHandler {
        id: cardHover
        acceptedDevices: PointerDevice.Mouse | PointerDevice.TouchPad
    }

    readonly property alias hovered: cardHover.hovered

    // The envelope. Every hover gesture in the grid is scaled by it, so leaving
    // the card retires the gesture along the shortest path from wherever it had
    // got to — no gesture has to know how to finish itself, and none of them
    // can be left standing off their reading.
    property real hoverPhase: (root.hovered && !Theme.stillness) ? 1 : 0

    Behavior on hoverPhase {
        NumberAnimation {
            duration: Theme.motion.move
            easing.type: Easing.OutCubic
        }
    }

    // The rhythm the walking cards share, and the reason they share it: seven
    // cards each timing their own out-and-back would be seven cards that agree
    // about what hover means and disagree about how long it takes.
    //
    // Out over the reveal's own duration, because it is the reveal's gesture
    // being made a second time; a pause at the far end long enough to read the
    // value it went to; back a little quicker; and then the longest pause of
    // the four at rest, so the card spends most of a cycle showing the reading
    // it actually has. `hoverBeat` is the rhythm alone — cards want `hoverWalk`,
    // which is the rhythm inside the envelope.
    property real hoverBeat: 0
    readonly property real hoverWalk: root.hoverBeat * root.hoverPhase

    SequentialAnimation on hoverBeat {
        running: root.hoverPhase > 0
        loops: Animation.Infinite
        NumberAnimation { to: 1; duration: Theme.motion.reveal; easing.type: Easing.OutCubic }
        PauseAnimation { duration: Theme.motion.dwell }
        NumberAnimation { to: 0; duration: Theme.motion.view; easing.type: Easing.InOutSine }
        PauseAnimation { duration: Theme.motion.rest }
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

        // One rectangle changing colour, not a hover wash laid over the card:
        // two washes composite to a patch lighter than either and the seam is
        // exactly what you notice (§10.1). The next rung of the surface ladder
        // rather than a tint invented here, and the same pair the hourly list
        // uses on its rows, so the two surfaces that respond to a pointer
        // respond by the same amount.
        color: root.hovered ? Theme.surface.raised : Theme.surface.base

        // A state and not a gesture, so it is left out of `hoverPhase` and
        // survives stillness — where the duration collapses to zero and the
        // card simply is the other colour.
        Behavior on color {
            ColorAnimation {
                duration: Theme.motion.tint
                easing.type: Easing.OutCubic
            }
        }
    }

    Text {
        id: titleText
        text: root.title
        color: Theme.ink.primary
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
            color: Theme.ink.primary
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
        color: Theme.ink.muted
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
