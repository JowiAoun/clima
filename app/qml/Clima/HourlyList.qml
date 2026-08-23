// SPDX-FileCopyrightText: 2026 Jowi Aoun
// SPDX-License-Identifier: GPL-3.0-or-later
// The list alternative to the chart: one card per hour, laid out along the day.
//
// A chart answers "what is the shape of the day"; a list answers "what exactly is
// it at 3pm". Both are worth having, which is why the reference carries a switch
// for it — and why leaving that switch inert was the wrong place to stop.
//
// ---- why it runs across and not down ----------------------------------------
//
// It was a table: one row per hour, seven columns, scrolling down. That reads
// perfectly and sits wrongly, because of what is directly above it. The day
// strip runs left to right and the chart under it runs left to right, both on
// the same axis — time — and the list is a third view of that same axis. Turning
// it through ninety degrees made the reader turn with it, and cost the one thing
// the day strip is for: you could no longer see the hours and the day they
// belong to as one line.
//
// So each hour is a card and the cards run along the day. The card carries what
// the row's seven columns carried, stacked: the hour, its sky, its temperature
// and the words for it, then the four readings that qualify them.
//
// The past is dimmed rather than hidden, and "now" is marked, so the same rule
// the chart follows holds here: observed hours are real data, just not forecast.
//
// ---- the arrows --------------------------------------------------------------
//
// Left and right walk the hours, and walking off the end of a day arrives at the
// next one. That is the whole navigation model of this card in one sentence, and
// it is the same sentence the chart's arrows obey — the chart just has no hours
// left to walk first, because it draws the day whole.
//
// A day reached by paging opens at the edge you came through: forward into
// midnight, back into the last hour. Reading a run of hours across a day
// boundary is the thing this makes continuous, and opening the next day on
// "now" — which is what a tap on the day strip should do, and does — would put
// a gap in the middle of it.
//
// ---- motion -----------------------------------------------------------------
// Two: the card under the pointer tints, and a pager press eases the row along.
// Everything else here is deliberately still, and the reasons are worth writing
// down because "add an arrival" is the obvious thing to reach for and every
// version of it is wrong:
//
//   * A staggered card reveal delays the one thing the reader just asked for.
//     They pressed "List" to read 3 AM's numbers; making them watch the day
//     arrive is charging admission for data that was already on screen.
//   * It would fire on scroll. ListView builds delegates as they come into
//     view — `cacheBuffer: 0`, so exactly as they come into view — and §10.6
//     forbids a reveal that re-triggers, "nothing fires on scrolling into
//     view" in particular. A per-delegate animation is that bug by
//     construction, not by accident.
//   * It would replay on every toggle. `HourlyOverview` loads this file with
//     `active: root.listView`, so the whole list is rebuilt each time the
//     switch is flipped, and an on-create reveal replays chart→list→chart.
//   * Every cell in here is text, and §10.6 says text does not fly, fade or
//     slide.
//
// The list also has no state to transition between: it is metric-agnostic, and
// the day it draws is whichever one `Data`'s window is of, so a day change
// replaces the model rather than transitioning it. Arrival is the switch's
// motion and the switch belongs to `HourlyOverview`, which is the only place
// that can sequence it against the chart underneath.
//
// The pragma is qmllint's ask, and the same one DayStrip makes at greater
// length: a delegate cannot see an outer id without it, so every `root.` and
// `view.` in the card below is an unqualified access and a binding qmlcachegen
// declines to compile ahead of time. The requirement it brings — that a
// delegate declare what it takes from the model with `required` — this delegate
// already met.
pragma ComponentBehavior: Bound

import QtQuick

Item {
    id: root

    // One card per hour, and the card is the unit the pagers step in.
    readonly property real cardWidth: 136
    readonly property real cardSpacing: 10
    readonly property real cardStride: cardWidth + cardSpacing

    // The card is as tall as what is in it, not as tall as the panel.
    //
    // The panel's height is the chart's — the two share a body so that flipping
    // the switch does not resize the page under the reader — and the chart
    // needs a plot, a header band and a precipitation strip where this needs
    // eight lines of text. Stretched to fill, a 124 px card came out 370 tall
    // with a hand's width of nothing across its middle. So the cards keep their
    // own proportion and sit centred in the panel, and the space left over
    // reads as the panel's padding rather than as a hole in a card.
    readonly property real cardHeight: 300

    Rectangle {
        anchors.fill: parent
        radius: Theme.metric.panelRadius
        color: Theme.surface.panel
    }

    // ---- one hour's readings, as a key and a value ------------------------
    // The row's column headings, kept as words rather than turned into icons.
    // A thermometer, a droplet and a windsock read at a glance only once you
    // have been taught them, and this card has room for the word.
    component Reading: Item {
        id: readingRoot

        property string label: ""
        property string value: ""
        property color valueColor: Theme.ink.primary

        width: parent ? parent.width : 0
        height: 17

        // The value keeps its width and the label gives way. "Humidity" is 44 px
        // in English and 85 in German, and the one of the two that must stay
        // whole is the number — a row reading "Luftfeuchtig… 52%" still answers
        // the question, and one reading "Luftfeuchtigkeit 5…" does not.
        Text {
            id: valueText
            anchors.right: parent.right
            anchors.verticalCenter: parent.verticalCenter
            text: readingRoot.value
            color: readingRoot.valueColor
            font.pixelSize: Theme.type.axis
        }

        Text {
            anchors.left: parent.left
            anchors.right: valueText.left
            anchors.rightMargin: 6
            anchors.verticalCenter: parent.verticalCenter
            text: readingRoot.label
            color: Theme.ink.dim
            font.pixelSize: Theme.type.axis
            elide: Text.ElideRight
        }
    }

    ListView {
        id: view
        anchors.fill: parent
        anchors.margins: Theme.metric.panelPadding
        orientation: ListView.Horizontal
        spacing: root.cardSpacing
        clip: true
        // Qt Quick Shapes escape ancestor clipping: the condition glyphs of
        // out-of-view cards were drawing over the panel edge and past it. A
        // layer bounds the whole subtree, which plain clip: true does not.
        layer.enabled: true
        cacheBuffer: 0
        model: Data.count
        boundsBehavior: Flickable.StopAtBounds

        // No `currentIndex` binding. Nothing here draws off being current — the
        // now card draws off `index === Data.nowIndex` — and binding it costs
        // something real: a view tracks its current item, and tracking IS a
        // scroll. `nowIndex` changes on every refresh and on every day change,
        // so the list would yank itself back under a reader who had paged
        // somewhere else, on a timer.

        // ---- where a day opens ------------------------------------------
        //
        // `currentIndex` alone does not decide where the view rests: nothing is
        // bound to it — the now card draws off `index === Data.nowIndex`, not
        // off being current — so all it does is make Qt track that delegate,
        // and where tracking lands depends on the view's width when the
        // delegate happened to be created. Positioning explicitly makes it the
        // same list in the page and in the gallery.
        //
        // "Now" is the answer for a day arrived at directly: the past is dimmed
        // context you can scroll back to, not the thing you came for. Clamped,
        // because the day strip moves the window — `Data.nowIndex` is an offset
        // to the present, so on any day but today it points outside the list,
        // where positionViewAtIndex does nothing at all and the list would be
        // left wherever the previous day had scrolled it. Clamping opens a
        // future day on midnight and a past one on its last hour, which is what
        // the chart beside it does with the same number.
        //
        // A day arrived at through a pager opens at the edge it was reached
        // through instead. See the header.
        property int arrivedFrom: 0

        // Whether the reader has moved the list themselves. The same guard the
        // chart's Flickable carries, and for the same reason: what decides
        // "still where it opened" is whether they have touched it, not how many
        // times it has been positioned.
        property bool touched: false
        onMovementStarted: view.touched = true

        function openOnNow() {
            positionViewAtIndex(Math.max(0, Math.min(Data.count - 1, Data.nowIndex)),
                                ListView.Beginning)
        }

        // Deferred, and it has to be. `setSelectedDay` emits
        // `selectedDayChanged` and only then `changed`, so at the instant this
        // is called the ListView is still holding the previous day's model and
        // therefore the previous day's `contentWidth` — and arriving "at the
        // end" would land on the end of the day being left. Qt.callLater runs
        // it once, after the bindings have settled.
        //
        // Stopping the animation first: a pager press that lands on a day
        // change returns without stopping it, and a NumberAnimation driving
        // `contentX` rewrites it on every tick from its own from/to.
        function arrive() {
            scrollAnim.stop()
            if (view.arrivedFrom > 0)
                view.positionViewAtBeginning()
            else if (view.arrivedFrom < 0)
                view.positionViewAtEnd()
            else
                view.openOnNow()
            view.arrivedFrom = 0
        }

        function openForArrival() { Qt.callLater(view.arrive) }

        // A Loader sizes its item AFTER the incubator reports Ready, so at
        // Component.onCompleted this view is 28 px narrower than nothing and
        // every position clamps to zero — the list opened on midnight however
        // late in the day it was. The chart's Flickable carries the same pair
        // of hooks and a long note about the tablet grid that found it there.
        Component.onCompleted: openOnNow()
        onWidthChanged: if (!view.touched) view.openOnNow()

        Connections {
            target: Data
            function onSelectedDayChanged() { view.openForArrival() }
        }

        // A pager step is a whole number of cards, so the row lands on a card
        // boundary everywhere except against the far end, where the clamp below
        // takes over and the last card sits flush instead. `view`, the same
        // token the chart's pagers used to move on: one view of the day
        // becoming another.
        NumberAnimation {
            id: scrollAnim
            target: view
            property: "contentX"
            duration: Theme.motion.view
            easing.type: Easing.OutCubic
        }

        // How far a press moves: as many whole cards as fit, less one, so a
        // card stays on screen across the step and the reader keeps their place.
        readonly property real pageStride:
            Math.max(1, Math.floor(width / root.cardStride) - 1) * root.cardStride

        readonly property bool atStart: contentX <= 1
        readonly property bool atEnd:   contentX >= contentWidth - width - 1

        // Whether stepping in this direction leads anywhere at all: more of
        // this day, or another day to spend it on.
        function canStep(delta) {
            if (delta < 0)
                return !atStart || Data.selectedDay > 0
            return !atEnd || Data.selectedDay < Data.days.length - 1
        }

        function step(delta) {
            // A pager press is the reader moving the list, and it has to say so
            // here: it animates `contentX` directly, which never raises
            // `movementStarted`, so without this a resize would walk them back
            // to now under a press they had just made.
            view.touched = true

            if ((delta < 0 && atStart) || (delta > 0 && atEnd)) {
                view.arrivedFrom = delta
                Data.stepDay(delta)
                return
            }

            var to = Math.max(0, Math.min(contentWidth - width,
                                          contentX + delta * pageStride))
            scrollAnim.stop()
            scrollAnim.from = contentX
            scrollAnim.to = to
            scrollAnim.start()
        }

        delegate: Item {
            id: hourCard
            required property int index

            readonly property bool isNow: Data.nowInWindow && index === Data.nowIndex
            readonly property bool isPast: index < Data.nowIndex

            width: root.cardWidth
            height: view.height
            opacity: isPast ? 0.5 : 1

            Item {
                id: card
                width: parent.width
                height: Math.min(parent.height, root.cardHeight)
                anchors.verticalCenter: parent.verticalCenter

                // A reading aid, not an affordance. On the card rather than on
                // the full-height column it is centred in, so the tint follows
                // what the pointer is actually over. No `cursorShape` — nothing
                // in this list is clickable and a pointing hand would promise
                // that it is.
                HoverHandler { id: cardHover }

                Rectangle {
                    anchors.fill: parent
                    radius: Theme.metric.controlRadius

                    // One rectangle changing colour, not a hover panel laid
                    // over the card: two washes stack to a patch lighter than
                    // either (§10.1), and the seam is exactly what you would
                    // notice.
                    //
                    // The now card is exempt. Its fill *is* the mark — swapping
                    // the yellow for a neutral wash would blank the one card
                    // the reader came to find, the moment they point at it.
                    color: hourCard.isNow ? Theme.surface.rowNow
                                          : (cardHover.hovered ? Theme.surface.raised
                                                               : Theme.surface.rowAlt)

                    // Behaviors do not fire for a property's initial binding,
                    // so a delegate built as it scrolls into view arrives at
                    // its colour rather than fading up to it.
                    Behavior on color {
                        ColorAnimation {
                            duration: Theme.motion.tint
                            easing.type: Easing.OutCubic
                        }
                    }
                }

                // The now mark, turned through ninety degrees with the list:
                // the rows carried a bar down their leading edge, the cards
                // carry it across their top.
                Rectangle {
                    visible: hourCard.isNow
                    width: parent.width - 26
                    height: 3
                    radius: 1.5
                    y: 0
                    anchors.horizontalCenter: parent.horizontalCenter
                    color: Theme.accent.fill
                }

                Column {
                    anchors.top: parent.top
                    anchors.left: parent.left
                    anchors.right: parent.right
                    anchors.topMargin: 18
                    anchors.leftMargin: 15
                    anchors.rightMargin: 15
                    spacing: 12

                    Text {
                        // The arrays, not the functions of the same name. A
                        // binding subscribes to the properties it reads and a
                        // method call is not one — and this view's model is
                        // `Data.count`, which is 24 on Thursday and 24 on
                        // Friday, so Qt rebuilds no delegate and nothing
                        // re-runs. See forecastdata.h.
                        text: Data.hourLabels[hourCard.index]
                        color: Theme.ink.primary
                        font.pixelSize: Theme.type.body
                        font.bold: hourCard.isNow
                    }

                    WeatherGlyph {
                        kind: Data.conditions[hourCard.index]
                        glyphSize: 40
                    }

                    Text {
                        text: Units.formatDisplay(Units.Temperature,
                                                  Data.temperature[hourCard.index])
                        color: Theme.ink.primary
                        font.pixelSize: Theme.type.readingPair
                        font.bold: true
                    }

                    Text {
                        width: parent.width
                        text: Data.conditionTexts[hourCard.index]
                        color: Theme.ink.muted
                        font.pixelSize: Theme.type.axis
                        wrapMode: Text.WordWrap
                        maximumLineCount: 2
                        elide: Text.ElideRight
                    }
                }

                // The qualifiers, pinned to the bottom edge rather than run on
                // from the block above: the condition is one line or two
                // depending on the words, and readings that moved up and down
                // with it would make a row of cards look ragged for a reason
                // that is not about the weather.
                Column {
                    anchors.bottom: parent.bottom
                    anchors.left: parent.left
                    anchors.right: parent.right
                    anchors.bottomMargin: 18
                    anchors.leftMargin: 15
                    anchors.rightMargin: 15
                    spacing: 2

                    Rectangle {
                        width: parent.width
                        height: 1
                        color: Theme.line.divider
                    }

                    Item { width: 1; height: 5 }

                    Reading {
                        label: qsTr("Feels")
                        value: Units.formatDisplay(Units.Temperature,
                                                   Data.apparent[hourCard.index])
                    }
                    // Percentages through Units too, rather than `+ "%"`.
                    // Precipitation probability is Open-Meteo's alone and
                    // humidity is absent from two of the four providers, and an
                    // absent reading is NaN — which concatenates to "NaN%" and
                    // formats to an em dash.
                    Reading {
                        label: qsTr("Precip")
                        value: Units.formatDisplay(Units.Percentage,
                                                   Data.precipProb[hourCard.index])
                        valueColor: Theme.glyph.droplet
                    }
                    // formatDisplay, not format: `Data.windSpeed` is already in
                    // the reader's unit (forecastdata.h converts on the way
                    // out), and `format` converts what it is given — so this
                    // printed a 20 km/h wind as 8 mph, correct-looking and
                    // wrong, for anyone who had changed the setting.
                    Reading {
                        label: qsTr("Wind")
                        value: Units.formatDisplay(Units.Wind,
                                                   Data.windSpeed[hourCard.index])
                    }
                    Reading {
                        label: qsTr("Humidity")
                        value: Units.formatDisplay(Units.Percentage,
                                                   Data.humidity[hourCard.index])
                    }
                }
            }
        }
    }

    // ---- pagers --------------------------------------------------------------
    //
    // Hours first, then days — see the header. Declared after the ListView so
    // they take the presses over it.
    PagerButton {
        pointsLeft: true
        enabledState: view.canStep(-1)
        anchors.left: parent.left
        anchors.leftMargin: 2
        anchors.verticalCenter: parent.verticalCenter
        onActivated: view.step(-1)
    }

    PagerButton {
        pointsLeft: false
        enabledState: view.canStep(1)
        anchors.right: parent.right
        anchors.rightMargin: 2
        anchors.verticalCenter: parent.verticalCenter
        onActivated: view.step(1)
    }
}
