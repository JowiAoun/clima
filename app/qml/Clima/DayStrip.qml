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
    onCurrentIndexChanged: Data.selectedDay = root.currentIndex
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
    readonly property real filletRadius: Theme.metric.filletRadius

    // Where the card stops travelling and the junction starts forming, as a
    // fraction of the merge. 0.6 under OutCubic is 50 ms of travel and 140 ms of
    // junction: the card arrives quickly, decelerating onto the panel, and the
    // corner it makes takes its time. Filmed the other way round and the card
    // was still drifting downward while the join was already drawn, which is
    // the thing this whole arrangement exists to make impossible.
    readonly property real mergeSplit: 0.6

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

        // `view` rather than `move`: a pager press swings the strip on by 70 % of
        // its width, so it replaces most of what you were looking at instead of
        // nudging it — one view becoming another.
        NumberAnimation {
            id: scrollAnim
            target: flick
            property: "contentX"
            duration: Theme.motion.view
            easing.type: Easing.OutCubic
        }

        function scrollBy(dx) {
            var to = Math.max(0, Math.min(Math.max(0, contentWidth - width), contentX + dx))
            scrollAnim.stop()
            scrollAnim.from = contentX
            scrollAnim.to = to
            scrollAnim.start()
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
