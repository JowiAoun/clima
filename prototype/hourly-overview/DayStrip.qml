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
// Motion. Selecting a card is the one event here and it changes four things at
// once — fill, outline, size and the second badge — so they are choreographed
// from this file rather than from the components. TabFillet and DayIconBadge
// stay dumb; a fillet that animated itself would animate in the gallery too,
// where nothing is selecting anything.
import QtQuick
import "theme.js" as Theme
import "mockdata.js" as Data

Item {
    id: root

    property int currentIndex: Data.todayIndex

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

                    // How big the fillets either side of this card currently are.
                    //
                    // Arriving it runs the card's own curve and duration, so the
                    // junction grows out of nothing with the card instead of snapping
                    // to full size beside a card that is still 20 px short of the
                    // panel. Leaving it goes in one frame, with the selection.
                    //
                    // That asymmetry is geometry, not taste. The fillet is concave and
                    // the card's bottom corners are convex the moment the card stops
                    // being the tab; the two cannot share an edge, and shrinking the
                    // fillet past a corner that has already rounded leaves a 10 px
                    // wedge of background between them. Filmed it: it is worse than
                    // the fillet simply going.
                    property real filletSize: selected ? root.filletRadius : 0
                    Behavior on filletSize {
                        NumberAnimation {
                            duration: card.selected ? Theme.motion.move : 0
                            easing.type: Easing.OutCubic
                        }
                    }

                    width: selected ? root.cardWidth + root.selectedExtra : root.cardWidth
                    height: selected ? root.height + root.mergeDepth
                                     : root.height - root.unselectedInset
                    Behavior on width { NumberAnimation { duration: Theme.motion.move; easing.type: Easing.OutCubic } }
                    Behavior on height { NumberAnimation { duration: Theme.motion.move; easing.type: Easing.OutCubic } }

                    Rectangle {
                        id: surface
                        anchors.fill: parent
                        color: card.selected ? Theme.color.cardBg : Theme.color.dayCardBg
                        // No outline on the raised card: it is one surface with the
                        // panel below, and an outline would draw a line across that.
                        // It is faded out rather than switched off — the width stays
                        // 1 and the colour lands on the fill colour, so the ring
                        // disappears *into* the card over the same beat as the fill.
                        // Qt draws the border band in place of the fill rather than
                        // over it, so this is one wash and not two (§10.1).
                        border.width: 1
                        border.color: card.selected ? Theme.color.cardBg : Theme.color.cardBorder
                        // Square at the bottom when selected: that edge is under the
                        // chart card and must not round away from it.
                        topLeftRadius: Theme.metric.cardRadius
                        topRightRadius: Theme.metric.cardRadius
                        bottomLeftRadius: card.selected ? 0 : Theme.metric.cardRadius
                        bottomRightRadius: card.selected ? 0 : Theme.metric.cardRadius
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
                        color: Theme.color.textPrimary
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
                        color: card.selected ? Theme.color.textPrimary : Theme.color.textMuted
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
                        // been given room beside yet. Now it waits a `stagger` and then
                        // fades over `tint`: 45 + 150 puts it fully there as the card
                        // stops widening. Leaving, it goes straight away, so it is gone
                        // before the room closes.
                        DayIconBadge {
                            kind: card.modelData.nightIcon
                            night: true
                            badgeSize: root.badgeSize
                            opacity: card.selected ? 1 : 0
                            visible: opacity > 0
                            Behavior on opacity {
                                SequentialAnimation {
                                    PauseAnimation {
                                        duration: card.selected ? Theme.motion.stagger : 0
                                    }
                                    NumberAnimation {
                                        duration: Theme.motion.tint
                                        easing.type: Easing.OutCubic
                                    }
                                }
                            }
                        }
                    }

                    Column {
                        spacing: 2
                        anchors.right: parent.right
                        anchors.rightMargin: 16
                        y: 56

                        Text {
                            text: card.modelData.high + "°"
                            color: Theme.color.textPrimary
                            font.pixelSize: 17
                            font.bold: true
                            anchors.right: parent.right
                        }
                        Text {
                            text: card.modelData.low + "°"
                            color: Theme.color.textMuted
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
