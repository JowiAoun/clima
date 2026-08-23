// SPDX-FileCopyrightText: 2026 Jowi Aoun
// SPDX-License-Identifier: GPL-3.0-or-later
//
// What the hourly chart is a chart OF, and — where it cannot show all of it —
// where it opens.
//
// ============================================================================
// THE RULE THE CARD IS BUILT ON
//
// The window is one calendar day (app/viewmodels/forecastdata.h) and the arrows
// either side step to the next one. So the plot has exactly one job: show all of
// that day. A day that has to be scrolled to be read is a day the arrows cannot
// be about, and the pair of controls would then mean two different things at two
// window widths.
//
// The column width is therefore derived — the plot's width divided among the
// day's hours — with `hourWidth` as the floor under it rather than the answer.
// A desktop card lands well above the floor and draws the day whole; a phone
// lands on it and scrolls, because its 268 px plot puts 24 hours at 11.6 px a
// column and the glyphs drawn on them are 27. That is the split this file
// asserts at both ends — including at 1024, the narrowest window this shell
// exists at, which is where the floor was originally set too high and left a
// third of the desktop range scrolling with no scroll control on it.
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

        // The desktop card, at the width WeatherPage gives it in a default
        // window: 1340 less the page's own margins.
        HourlyOverview {
            id: wide
            width: 1252
            height: 420
        }

        // …and at the narrowest window this shell runs in. The one that used to
        // scroll.
        HourlyOverview {
            id: narrowDesktop
            y: 1250
            width: 936
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

    // The rule the whole card is built on: a desktop card is a chart of the
    // day and of nothing else, so there is nothing left over to scroll.
    function test_aWideCardDrawsTheWholeDayWithNothingLeftOver() {
        var flick = scroller(wide)
        fuzzyCompare(flick.contentWidth, flick.width, 0.5,
                     "the desktop chart does not fill its plot exactly")
        compare(flick.contentX, 0, "a chart of a whole day opened part-way along it")
        verify(wide.columnWidth > wide.hourWidth,
               "the fitted column came out narrower than the floor at 1252 px")
    }

    // The cliff. The arrows step days, so a desktop chart that scrolls has
    // content on it the reader has no control to reach — which means the floor
    // has to stay under the fitted width at every width this shell runs at, not
    // just at the default one.
    function test_theWholeDayStillFitsAtTheNarrowestDesktop() {
        var flick = scroller(narrowDesktop)
        fuzzyCompare(flick.contentWidth, flick.width, 0.5,
                     "at a 1024 px window the desktop chart scrolls, and nothing scrolls it")
    }

    // And the other end of it. A phone cannot show 24 legible columns, so the
    // floor takes over and the chart scrolls — with the arrows still meaning
    // days, which is what keeps the control honest at both widths.
    function test_aNarrowCardFallsBackToTheFloorAndScrolls() {
        compare(narrow.columnWidth, narrow.hourWidth,
                "a 362 px card fitted the day instead of falling back to the floor")
        var flick = scroller(narrow)
        verify(flick.contentWidth > flick.width,
               "the phone chart has the whole day inside its plot, which cannot be legible")
    }

    // Where a card too narrow for the day opens: on now, with a little of the
    // past behind it. One observed hour where three would be a third of the view.
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
    // labelled hour, so it is drawn whole. Only the scrolling card has a left
    // edge that can land anywhere — the wide one opens at column 0 and stays
    // there, because there is nowhere else for a chart of a whole day to be.
    function test_theLeftEdgeLandsOneColumnBeforeALabel() {
        var flick = scroller(narrow)
        var firstWholeLabel = Data.nowIndex - flick.openPastHours + 1
        compare((firstWholeLabel - Data.firstLabelIndex) % Data.labelStep, 0,
                "column " + firstWholeLabel + " is inside the left edge and is not a label")
    }

    // ---- the arrows ---------------------------------------------------------
    //
    // They step the day, so what they have to track is how much strip is left
    // either side rather than how much plot. Asserted through `enabledState`,
    // which is the binding that would go stale: the press itself is
    // `Data.stepDay`, and what that does is tests/tst_forecastdata.cpp's.
    function pagers(chart) {
        var found = []
        function walk(item) {
            for (var i = 0; i < item.children.length; ++i) {
                var child = item.children[i]
                if (child.pointsLeft !== undefined)
                    found.push(child)
                else
                    walk(child)
            }
        }
        walk(chart)
        return found
    }

    function test_theArrowsRunOutAtTheEndsOfTheStrip() {
        var found = pagers(wide)
        compare(found.length, 2, "the chart does not carry a pair of arrows")

        var back = found[0].pointsLeft ? found[0] : found[1]
        var forward = found[0].pointsLeft ? found[1] : found[0]

        Data.selectedDay = 0
        compare(back.enabledState, false, "there is a day before the first one")
        compare(forward.enabledState, true)

        Data.selectedDay = Data.days.length - 1
        compare(back.enabledState, true)
        compare(forward.enabledState, false, "there is a day after the last one")

        // Off `selectedDay` rather than off today's position in the strip:
        // Open-Meteo is fetched with a past day and MET Norway is not, so
        // whether today has a day before it is a property of the provider.
        Data.selectedDay = Data.todayIndex
        compare(back.enabledState, Data.selectedDay > 0)
        compare(forward.enabledState, Data.selectedDay < Data.days.length - 1)
    }

    // A day change is not a scroll. Whichever day it is on, the desktop card
    // still draws that day whole — which is the property that lets the arrows
    // mean one thing at that width.
    function test_everyDayFillsTheWideCardExactly() {
        for (var day = 0; day < Data.days.length; ++day) {
            Data.selectedDay = day
            var flick = scroller(wide)
            if (Data.count < 24)
                continue          // a horizon that runs out mid-day; see forecastdata.h
            fuzzyCompare(flick.contentWidth, flick.width, 0.5,
                         "day " + day + " does not fill the plot")
        }
        Data.selectedDay = Data.todayIndex
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
                                            (Data.nowIndex - 3) * late.columnWidth))
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
