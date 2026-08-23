// SPDX-FileCopyrightText: 2026 Jowi Aoun
// SPDX-License-Identifier: GPL-3.0-or-later
// The day cards above the chart.
//
// The selected card behaves like a browser tab: it is lighter, wider, and *taller*
// than its neighbours, and its fill runs straight into the chart card below with no
// seam. That merge is what says "the chart underneath is showing this day".
//
// It is done by overhang rather than by drawing a join. The selected card extends
// `mergeDepth` past the bottom of the strip, and the chart card — declared after
// this in Main.qml, so painted over it — covers that overhang, taking the card's
// bottom border with it. Nothing has to line up to the pixel, and it stays correct
// at any card position or window size.
//
// The junction itself is filleted, not squared: a TabFillet sits in the gap either
// side of the raised card and curves its edge outward into the panel. Without them
// the card reads as pasted on top of the panel rather than growing out of it.
//
// Motion. Selecting a card is the one event here and it changes six things at
// once — fill, outline, width, height, the bottom corners and the fillets — so
// they are choreographed from this file rather than from the components.
// TabFillet and DayIconBadge stay dumb; a fillet that animated itself would
// animate in the gallery too, where nothing is selecting anything.
//
// The geometry of the merge runs off ONE animated number, `merge`, split into
// two beats that cannot overlap: the card travels down to the panel, and then
// the corner it has made fillets outward. Reversed, the join comes apart before
// the card lifts. Everything else here — fill, outline, the day/night crossfade
// — is content rather than junction and keeps its own `tint` beat.
//
// It is worth saying why, because the obvious spelling is a Behavior per
// property and that is what this was. Three clocks on three properties drift:
// the fillet reached two thirds of its radius while the card was still 20 px
// clear of the panel, so a rounded wedge hung in the gap with nothing to join,
// and the bottom corners switched square in one frame under it. Deriving the
// parts from one number is what makes "a junction only exists where two
// surfaces touch" a property of the code rather than of the numbers agreeing.
//
// The pragma is qmllint's ask and this file is the one place it costs nothing:
// a Repeater delegate cannot see an outer id without it, so every `root.` in
// the card below is an unqualified access and a binding qmlcachegen declines to
// compile ahead of time. The requirement it brings — that a delegate declare
// what it takes from the model with `required` — this delegate already met.
pragma ComponentBehavior: Bound

import QtQuick

Item {
    id: root

    // The selection is the model's, not the strip's, and it travels both ways.
    //
    // It has to be the model's because it is what the chart below is a chart
    // *of* — the strip is the control and `Data` is what it controls, and a
    // number kept here with a copy pushed into the model is two numbers that
    // can disagree. It has to travel both ways because the model clamps: ask
    // for a row that a shorter forecast no longer has and the answer comes back
    // different from what was asked, and the card that lights up should be the
    // one being drawn.
    //
    // The Binding element rather than a plain binding on `currentIndex`,
    // because a tap writes to it and a written property has no binding left.
    // RestoreNone: there is nothing to restore to — the value this replaces is
    // the one it just sent.
    property int currentIndex: Data.selectedDay
    onCurrentIndexChanged: {
        Data.selectedDay = root.currentIndex
        root.showSelected()
    }
    Binding {
        target: root
        property: "currentIndex"
        value: Data.selectedDay
        restoreMode: Binding.RestoreNone
    }

    readonly property real cardWidth: 172
    readonly property real selectedExtra: 72     // room for the second badge
    readonly property real spacing: 14
    readonly property real badgeSize: 50
    // The selected card used to overhang into the chart card, so the chart —
    // painted after it — would cover the card's bottom border. There is no
    // border to cover any more, and now that both surfaces are translucent an
    // overhang is actively wrong: the overlap would take the wash twice and
    // show as a lighter band across the junction. They abut instead.
    readonly property real mergeDepth: 0
    readonly property real unselectedInset: 20   // how much shorter the others are

    // How much of the neighbouring card to leave showing beside the selected
    // one, when scrolling to it.
    //
    // A gap of air was enough to say the selected card had not been cut off,
    // and not enough to say there was another day past it: scrolled to the last
    // card that fits, the strip came to rest with 14 px of background beside it
    // and read as the end of the forecast. A slice of the next card is the
    // thing that says "keep going" — the same reason any horizontally
    // scrolling row leaves a partial item at its edge rather than a clean one.
    //
    // At the ends of the strip the clamp inside scrollTo takes it away again,
    // which is still right: the first and last cards ARE flush, and the panel
    // below squares its corner under them.
    readonly property real peek: spacing + 40
    readonly property real filletRadius: Theme.metric.filletRadius

    // Where the card stops travelling and the junction starts forming, as a
    // fraction of the merge. 0.6 under OutCubic is 50 ms of travel and 140 ms of
    // junction: the card arrives quickly, decelerating onto the panel, and the
    // corner it makes takes its time. Filmed the other way round and the card
    // was still drifting downward while the join was already drawn, which is
    // the thing this whole arrangement exists to make impossible.
    readonly property real mergeSplit: 0.6

    // ---- where a tab reaches the ends of the strip ---------------------------
    //
    // How completely a raised card is sitting on each end, 0 to 1. The panel
    // below reads these and stops rounding the corner underneath.
    //
    // A corner with a tab on it is not a corner. The card's bottom edge is
    // straight and the panel's is curving away from it, so the two meet across a
    // 14 px notch of page background — the same seam the fillets exist to close,
    // at the one place a fillet cannot go, because outside the panel there is no
    // panel to fillet into.
    //
    // It is a fraction rather than a flag so that the panel's corner flattens on
    // the same beat as the card's own bottom corners do, off the same `landed`.
    // A flag would switch it in one frame under a tab that is still arriving,
    // which is the defect this file already carries a long note about.
    property real leftCover: 0
    property real rightCover: 0

    // Recomputed rather than bound, and this is the one place in this file that
    // is. The inputs are every delegate's x, width and landed plus the
    // flickable's contentX and width — a binding cannot subscribe to a
    // Repeater's children, and the answer is a maximum over them, which is not
    // a binding at all.
    //
    // Every card, not just the selected one: during a change of day the card
    // leaving is still landed for a beat, and it is the one at the edge. Reading
    // only `currentIndex` would round the corner out from under it.
    function updateCover() {
        var left = 0
        var right = 0

        for (var i = 0; i < row.children.length; ++i) {
            var card = row.children[i]
            if (card.landed === undefined || card.landed <= 0)
                continue

            var from = card.x - flick.contentX
            var to   = from + card.width

            if (from <= 0 && to > 0)
                left = Math.max(left, card.landed)
            if (from < flick.width && to >= flick.width)
                right = Math.max(right, card.landed)
        }

        root.leftCover  = left
        root.rightCover = right
    }

    // Bring the selected card into the strip, at the size it is about to be.
    //
    // The obvious case is a selection this strip did not make — `--day 10`, or
    // a place change putting the selection back on today while the strip is
    // scrolled into next week — where the chart below draws a day whose card is
    // nowhere on screen. MetricTabBar has carried the same rule for its pills
    // since it was written, for the same reason: a control that cannot show its
    // own state is a control lying about it.
    //
    // The case that is easier to miss is a card the reader clicks themselves.
    // Selecting widens it by `selectedExtra`, so the rightmost card you can see
    // is one you can select and then not see — it grows out of the strip under
    // your own cursor.
    //
    // Hence the *final* extent rather than the live one. Every card except the
    // selected one is `cardWidth` wide, so where this one will end up is exact
    // arithmetic and does not have to be watched for 190 ms: a chase would also
    // fight the widening it is reacting to.
    // A selection that arrives before the strip has a width is remembered
    // rather than dropped.
    //
    // Both of the ways a day is chosen without a click land in
    // `Component.onCompleted` — `--day 9` from ScreenshotController, and the
    // model's own selection when the window is rebuilt — and at that point the
    // Flickable is 0 px wide and this function can compute nothing. It used to
    // return, and nothing ever asked again: `--day 9` set the selection, the
    // strip stayed on the first page, and the card the whole flag is about was
    // off the right-hand edge with no sign that it existed.
    //
    // So the first call that has geometry to work with sets `placed`, and until
    // then the Flickable replays the request as its width and content arrive.
    // After that this is only ever the click path, and a later resize does not
    // yank the view back to a selection the reader has deliberately scrolled
    // away from.
    property bool placed: false

    function showSelected() {
        var index = root.currentIndex
        if (index < 0 || index >= Data.days.length)
            return
        if (flick.width <= 0)
            return

        root.placed = true
        if (flick.contentWidth <= flick.width)
            return

        var from = index * (root.cardWidth + root.spacing)
        var to   = from + root.cardWidth + root.selectedExtra

        // A slice of the neighbour beside it rather than a gap of air — see
        // `peek`, which is where the argument for the number is.
        var wanted = flick.contentX
        if (to + root.peek > wanted + flick.width)
            wanted = to + root.peek - flick.width
        if (from - root.peek < wanted)
            wanted = from - root.peek

        flick.scrollTo(wanted, Theme.motion.move)
    }

    implicitHeight: 130
    height: implicitHeight

    Flickable {
        id: flick
        anchors.fill: parent
        anchors.bottomMargin: -root.mergeDepth   // room for the overhang to draw into

        // clip bounds the cards; the layer bounds the condition glyphs, which are
        // Shapes and ignore ancestor clipping entirely (§10.8). Without it the
        // glyphs from cards scrolled out of view keep painting to the right of
        // the strip. Every window this ran in was narrow enough that they landed
        // off-window, so it looked fine until the page capped its content column
        // and left visible background beside it.
        clip: true
        layer.enabled: true
        contentWidth: row.width
        contentHeight: height
        flickableDirection: Flickable.HorizontalFlick
        boundsBehavior: Flickable.StopAtBounds

        // Scrolling moves a tab on or off an end as surely as selecting one
        // does, so the panel's corners follow the strip's position too.
        onContentXChanged: root.updateCover()
        onWidthChanged: {
            root.updateCover()
            if (!root.placed)
                root.showSelected()
        }
        onContentWidthChanged: if (!root.placed) root.showSelected()
        Component.onCompleted: root.updateCover()

        // Not a Behavior on contentX: a Behavior would intercept every flick and
        // drag too, and animating the content away from the finger is how a
        // Flickable stops feeling like one. MetricTabBar says the same thing
        // about its own strip.
        NumberAnimation {
            id: scrollAnim
            target: flick
            property: "contentX"
            easing.type: Easing.OutCubic
        }

        function scrollTo(x, duration) {
            var to = Math.max(0, Math.min(Math.max(0, contentWidth - width), x))
            if (Math.abs(to - contentX) < 0.5)
                return
            scrollAnim.stop()
            scrollAnim.duration = duration
            scrollAnim.from = contentX
            scrollAnim.to = to
            scrollAnim.start()
        }

        // `view` rather than `move`: a pager press swings the strip on by 70 % of
        // its width, so it replaces most of what you were looking at instead of
        // nudging it — one view becoming another.
        function scrollBy(dx) {
            scrollTo(contentX + dx, Theme.motion.view)
        }

        Row {
            id: row
            spacing: root.spacing
            height: flick.height

            Repeater {
                model: Data.days

                delegate: Item {
                    id: card

                    required property var modelData
                    required property int index

                    readonly property bool selected: index === root.currentIndex

                    // How far through becoming the tab this card is: 0 is a card
                    // standing clear of the panel, 1 is a tab that is part of it.
                    //
                    // One number, and every moving part of the merge is read off it.
                    // They used to be three Behaviors on three properties, which is
                    // how they came to disagree: the height said the card was still
                    // 20 px above the panel while the fillet said the junction was
                    // two thirds built, and a junction is not a thing a card can be
                    // two thirds of while it is in mid-air.
                    property real merge: card.selected ? 1 : 0
                    Behavior on merge {
                        NumberAnimation { duration: Theme.motion.move; easing.type: Easing.OutCubic }
                    }

                    // Two beats, and `mergeSplit` is the point between them.
                    //
                    // Beat one: the card reaches down to the panel, its bottom
                    // corners flattening as they arrive. Beat two: the corner it has
                    // just made fillets outward. They cannot overlap, and that is the
                    // whole of the fix — `joined` is above zero only where `landed`
                    // is exactly 1, so a fillet is never drawn beside a card that has
                    // not touched down, and never beside a corner that is still
                    // round. Those were the two ways the old spelling produced a
                    // rounded wedge sitting on its own in the gap.
                    //
                    // Splitting the *range* rather than the duration is also what
                    // gets the order right in both directions without asking which
                    // direction it is: run the same expressions backwards and the
                    // join comes apart before the card lifts, which is the only
                    // order it can come apart in. The old spelling did ask —
                    // `duration: selected ? move : 0` — and a Behavior can fire
                    // before the binding feeding its duration has been re-evaluated,
                    // so "leaving goes in one frame" held on some runs and not on
                    // others. It did not hold in the screenshot that started this.
                    readonly property real landed: Math.min(1, card.merge / root.mergeSplit)
                    readonly property real joined:
                        Math.max(0, (card.merge - root.mergeSplit) / (1 - root.mergeSplit))

                    // How big the fillets either side of this card currently are.
                    // Derived, so it has no clock of its own to drift against.
                    readonly property real filletSize: root.filletRadius * card.joined

                    width: root.cardWidth + root.selectedExtra * card.merge
                    height: root.height - root.unselectedInset
                            + (root.unselectedInset + root.mergeDepth) * card.landed

                    Rectangle {
                        id: surface
                        anchors.fill: parent
                        color: card.selected ? Theme.surface.base : Theme.surface.recede
                        // No outline on the raised card: it is one surface with the
                        // panel below, and an outline would draw a line across that.
                        // It is faded out rather than switched off — the width stays
                        // 1 and the colour lands on the fill colour, so the ring
                        // disappears *into* the card over the same beat as the fill.
                        // Qt draws the border band in place of the fill rather than
                        // over it, so this is one wash and not two (§10.1).
                        border.width: 1
                        border.color: card.selected ? Theme.surface.base : Theme.line.card
                        // Square at the bottom when selected: that edge is against
                        // the chart card and must not round away from it.
                        //
                        // It flattens over beat one rather than switching, so the
                        // bottom edge is already straight by the time it lands and
                        // the fillet has a corner to grow into. Switching it left a
                        // convex corner and a concave fillet claiming the same
                        // pixels for the length of the animation.
                        topLeftRadius: Theme.metric.cardRadius
                        topRightRadius: Theme.metric.cardRadius
                        bottomLeftRadius: Theme.metric.cardRadius * (1 - card.landed)
                        bottomRightRadius: Theme.metric.cardRadius * (1 - card.landed)
                        Behavior on color {
                            ColorAnimation { duration: Theme.motion.tint; easing.type: Easing.OutCubic }
                        }
                        Behavior on border.color {
                            ColorAnimation { duration: Theme.motion.tint; easing.type: Easing.OutCubic }
                        }
                    }

                    // The outward curves at the base of the raised card. They sit in
                    // the gaps either side of it, above the chart card's top edge.
                    //
                    // Both are pinned to the card's *live* bottom edge and take the
                    // card's *live* fill. Pinned to the strip's bottom and painted a
                    // flat cardBg — which is what they were — they spent the whole
                    // 190 ms as a brighter shape floating below the card they belong
                    // to, and only met it in the last frame.
                    TabFillet {
                        visible: card.filletSize > 0.5
                        mirrored: false
                        filletRadius: card.filletSize
                        extendBelow: root.mergeDepth
                        fillColor: surface.color
                        x: -width
                        y: card.height - height
                    }

                    TabFillet {
                        visible: card.filletSize > 0.5
                        mirrored: true
                        filletRadius: card.filletSize
                        extendBelow: root.mergeDepth
                        fillColor: surface.color
                        x: card.width
                        y: card.height - height
                    }

                    Text {
                        text: card.modelData.date
                        color: Theme.ink.primary
                        font.pixelSize: 15
                        font.bold: true
                        x: 16
                        y: 14
                    }

                    Text {
                        text: card.modelData.label
                        // A label may change colour (§10.6) and this one is part of
                        // how selection reads, so it goes over on the same beat as
                        // the fill rather than snapping while the card eases.
                        color: card.selected ? Theme.ink.primary : Theme.ink.muted
                        Behavior on color {
                            ColorAnimation { duration: Theme.motion.tint; easing.type: Easing.OutCubic }
                        }
                        font.pixelSize: 12
                        font.bold: card.selected
                        anchors.right: parent.right
                        anchors.rightMargin: 16
                        y: 16
                    }

                    // Selected: day and night conditions, each in a badge.
                    // Unselected: the daytime glyph alone.
                    Row {
                        spacing: 8
                        anchors.left: parent.left
                        anchors.leftMargin: 16
                        y: 46

                        // The daytime condition in both its forms, stacked in one
                        // cell rather than laid out side by side: it is one reading
                        // drawn two ways, so it cross-fades in place. Two cells would
                        // have made the icon slide sideways to say nothing new.
                        //
                        // Both sit at the cell's top-left, which is where the Row put
                        // each of them when only one of them existed at a time. The
                        // cell exists to stop the width changing, not to re-centre
                        // anything: neither resting layout moves by a pixel.
                        Item {
                            width: root.badgeSize
                            height: root.badgeSize

                            DayIconBadge {
                                kind: card.modelData.icon
                                badgeSize: root.badgeSize
                                opacity: card.selected ? 1 : 0
                                visible: opacity > 0
                                Behavior on opacity {
                                    NumberAnimation { duration: Theme.motion.tint; easing.type: Easing.OutCubic }
                                }
                            }
                            WeatherGlyph {
                                kind: card.modelData.icon
                                glyphSize: 44
                                opacity: card.selected ? 0 : 1
                                visible: opacity > 0
                                Behavior on opacity {
                                    NumberAnimation { duration: Theme.motion.tint; easing.type: Easing.OutCubic }
                                }
                            }
                        }

                        // The night condition is the one thing selection *adds*, and
                        // `selectedExtra` is the room the card widens to fit it. It
                        // used to arrive at full strength on frame one, at the card's
                        // *old* width — hard up against the high/low, which it had not
                        // been given room beside yet.
                        //
                        // So it is read off the same merge the width is, one quarter
                        // in: it cannot be visible before a quarter of its room
                        // exists, and backwards that is the same sentence — it is
                        // gone before the room closes. That was a `stagger` pause
                        // with `duration: selected ? 45 : 0` before, which is the
                        // same direction-branched Behavior as the fillet's and had
                        // the same failure in it.
                        DayIconBadge {
                            kind: card.modelData.nightIcon
                            night: true
                            badgeSize: root.badgeSize
                            opacity: Math.max(0, (card.merge - 0.25) / 0.75)
                            visible: opacity > 0
                        }
                    }

                    Column {
                        spacing: 2
                        anchors.right: parent.right
                        anchors.rightMargin: 16
                        y: 56

                        Text {
                            text: Units.formatDisplay(Units.Temperature, card.modelData.high)
                            color: Theme.ink.primary
                            font.pixelSize: 17
                            font.bold: true
                            anchors.right: parent.right
                        }
                        Text {
                            text: Units.formatDisplay(Units.Temperature, card.modelData.low)
                            color: Theme.ink.muted
                            font.pixelSize: 15
                            anchors.right: parent.right
                        }
                    }

                    // The three inputs the strip's end-cover is a maximum over.
                    // `x` because the cards before this one widen and push it,
                    // `width` because this one does, and `landed` because a card
                    // that is not on the panel is not covering anything.
                    onXChanged: root.updateCover()
                    onWidthChanged: root.updateCover()
                    onLandedChanged: root.updateCover()

                    HoverHandler { cursorShape: Qt.PointingHandCursor }
                    TapHandler { onTapped: root.currentIndex = card.index }
                }
            }
        }
    }

    PagerButton {
        pointsLeft: true
        enabledState: flick.contentX > 1
        opacity: enabledState ? 1 : 0
        anchors.left: parent.left
        y: (root.height - root.unselectedInset - height) / 2
        onActivated: flick.scrollBy(-flick.width * 0.7)
    }

    PagerButton {
        pointsLeft: false
        enabledState: flick.contentX < flick.contentWidth - flick.width - 1
        opacity: enabledState ? 1 : 0
        anchors.right: parent.right
        y: (root.height - root.unselectedInset - height) / 2
        onActivated: flick.scrollBy(flick.width * 0.7)
    }
}
