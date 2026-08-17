// SPDX-FileCopyrightText: 2026 Jowi Aoun
// SPDX-License-Identifier: GPL-3.0-or-later
//
// Where the hourly chart opens, and how much of the past it spends doing it.
//
// ============================================================================
// WHY THIS IS NOT A GOLDEN IMAGE
//
// It partly is — `mobile-hourly` and `desktop` record the answer at two widths.
// What they cannot record is the two ways of arriving at it, and both of them
// were wrong:
//
//   * A COLD START HAS NO DATA. The chart positioned itself in
//     Component.onCompleted, which on a live first fetch runs before the first
//     snapshot: contentWidth is 0, every position clamps to 0, and the card
//     opens on last night. Every capture in this repository uses a fixture, and
//     a fixture is published before the QML engine loads — so the defect was
//     invisible to all fifty of them and obvious the first time somebody opened
//     the app on a cold cache.
//
//   * LAYOUT ARRIVES IN STAGES. A card in a tablet's grid is handed a narrow
//     width and then its real one. The number of observed hours is a function
//     of that width, so a position computed at the first is a position computed
//     for a phone — which is exactly what the tablet-landscape golden did when
//     the width rule was first written.
//
// So what is asserted here is the *relationship*: whatever width this card ends
// up at, and whenever it learns it, the left edge lands one column before a
// labelled hour and spends as little of the view on the past as the axis allows.
//
// ============================================================================
// WHY THE COUNT IS ODD, WHICH IS THE ONE SURPRISING PART
//
// The header band draws an entry per labelled hour, two columns wide and
// centred on its label, so the left edge has to fall exactly one column before
// a label or the card opens with half a glyph and a sliced "AM". Labels run
// every `labelStep` from "Now" — which is itself always one — so the count of
// observed hours has to be odd. One or three; two is not available at any
// width, and a rule that returned it would be a rule that clips.
import QtQuick
import QtQuick.Window
import QtTest
import Clima

TestCase {
    name: "HourlyChart"

    Window {
        id: host
        width: 1340
        height: 900
        visible: true

        // The desktop card, at the width WeatherPage gives it.
        HourlyOverview {
            id: wide
            width: 1280
            height: 420
        }

        // The phone's, at MobileHourlyPage's width and column size.
        HourlyOverview {
            id: narrow
            y: 430
            width: 362
            height: 380
            hourWidth: 40
        }

        // A card that is laid out late: zero width until a test gives it one,
        // which is what a tablet's grid does to the real one.
        Item {
            id: cage
            y: 820
            width: 0
            height: 380

            HourlyOverview {
                id: late
                anchors.fill: parent
                hourWidth: 40
            }
        }
    }

    // Today, whatever ran before this. `Data` is one object for the whole
    // process, so the day another test file selected is the day this one would
    // measure — and on a day window `nowIndex` is an offset outside the window,
    // which is a different set of numbers entirely.
    function initTestCase() {
        Data.selectedDay = Data.todayIndex
    }

    // The chart's Flickable, found by what it carries rather than by where it
    // sits: the path down to it runs through three items that are layout and
    // could be rearranged without changing anything this test is about.
    function scroller(chart) {
        var found = null
        function walk(item) {
            for (var i = 0; i < item.children.length && !found; ++i) {
                var child = item.children[i]
                if (child.openPastHours !== undefined && child.contentX !== undefined)
                    found = child
                else
                    walk(child)
            }
        }
        walk(chart)
        return found
    }

    function test_bothCardsHaveAScroller() {
        verify(scroller(wide) !== null, "the desktop card has no flickable")
        verify(scroller(narrow) !== null, "the phone card has no flickable")
    }

    // The user-visible rule, at the two widths it distinguishes: three observed
    // hours where three is an eighth of the view, one where it would be a third.
    function test_awideCardOpensOnThreeObservedHours() {
        compare(scroller(wide).openPastHours, 3)
    }

    function test_aNarrowCardOpensOnOne() {
        compare(scroller(narrow).openPastHours, 1)
    }

    // The constraint that rules out two, checked across every width either shell
    // can produce rather than at the two above.
    function test_theCountIsOddAtEveryWidth() {
        var flick = scroller(narrow)
        for (var w = 200; w <= 1400; w += 17) {
            narrow.width = w
            compare(flick.openPastHours % 2, 1,
                    "at " + w + " px the chart would open on " + flick.openPastHours
                    + " observed hours, which does not land on a label")
        }
        narrow.width = 362
    }

    // And what odd is *for*: the label immediately inside the left edge is a
    // labelled hour, so it is drawn whole.
    function test_theLeftEdgeLandsOneColumnBeforeALabel() {
        for (var i = 0; i < 2; ++i) {
            var flick = scroller(i === 0 ? wide : narrow)
            var firstWholeLabel = Data.nowIndex - flick.openPastHours + 1
            compare((firstWholeLabel - Data.firstLabelIndex) % Data.labelStep, 0,
                    "column " + firstWholeLabel + " is inside the left edge and is not a label")
        }
    }

    // The tablet defect. A card given its width after it was built has to
    // position itself against the width it ended up with, not the one it was
    // born with.
    function test_aCardLaidOutLateStillOpensCorrectly() {
        var flick = scroller(late)
        verify(flick !== null)

        // Born with nothing to divide by, and it must not have guessed.
        compare(cage.width, 0)
        compare(flick.contentX, 0)

        cage.width = 900
        wait(0)

        compare(flick.openPastHours, 3)
        var expected = Math.max(0, Math.min(flick.contentWidth - flick.width,
                                            (Data.nowIndex - 3) * late.hourWidth))
        fuzzyCompare(flick.contentX, expected, 0.5)
        verify(flick.contentX > 0, "a late-laid-out card never left the start of the series")
    }

    // …and once the reader has moved it, a later width change must not walk them
    // back. This is the whole reason the guard is "has it been touched" rather
    // than "has it been positioned".
    function test_aScrolledCardIsNotDraggedBackByALayoutChange() {
        var flick = scroller(late)
        cage.width = 900
        wait(0)

        flick.touched = true
        flick.contentX = 0

        cage.width = 700
        wait(0)
        compare(flick.contentX, 0, "a resize moved a chart the reader had scrolled")
    }
}
