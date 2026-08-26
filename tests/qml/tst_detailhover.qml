// SPDX-FileCopyrightText: 2026 Jowi Aoun
// SPDX-License-Identifier: GPL-3.0-or-later
//
// The hover gestures on the weather-detail grid, and the contract that lets
// them exist at all.
//
// DetailCard.qml's hover block sets out the rule — a card moves on hover only
// where the resting card leaves something true unsaid — and two invariants that
// everything else in this product depends on:
//
//   - a card at rest is the card that existed before any of this, to the pixel,
//     because that is what fifty golden images assert every run;
//   - `Theme.stillness` suppresses the movement and keeps the tint, because
//     that one switch serves both a reader who asked their desktop for less
//     motion and a capture that must not be caught mid-gesture.
//
// So the assertions here are made on pixels rather than on properties. A test
// that read `hoverWalk` back would prove the envelope arithmetic and nothing
// about whether a single card is wired to it: the card that quietly forgot to
// multiply by `hoverPhase` passes that test and fails every golden. Grabbing
// the card at rest, again mid-gesture, and again after the pointer leaves
// catches both halves — that something moved, and that it all came back.
//
// It also pins the split. Eight cards have a gesture and five must not, and
// "must not" is the half that rots: the sixth card to be given one because the
// grid looked uneven is the one that breaks the rule, and it fails here with
// its own name in the message.
//
// One gap, stated rather than papered over. Whether a gesture *fires* depends
// on the fixture giving it somewhere to go, and on Toronto in July the sight
// line has nowhere: visibility runs 23–40 km against a scale that stops at 20,
// so both ends of its walk clamp to full and the card is correctly still. Its
// gesture is therefore built and returned-to-rest here but never actually
// exercised. Pointing the QML harness at a hazy fixture is what would close
// that, and it needs a way to choose one — see docs/known-gaps.md.
import QtQuick
import QtTest
import Clima

TestCase {
    id: testCase
    name: "DetailHover"
    when: windowShown

    // A TestCase is invisible by default and an invisible subtree is never
    // rendered, so every grab would come back the clear colour — which reads as
    // "nothing moved" and would make this file agree with itself forever.
    // tst_weatherglyph.qml learnt this the same way.
    visible: true
    width: Theme.metric.detailCardWidth + 40
    height: Theme.metric.detailCardHeight + 40

    Rectangle {
        id: stage
        anchors.centerIn: parent
        width: Theme.metric.detailCardWidth
        height: Theme.metric.detailCardHeight
        color: Theme.page.bg
    }

    // Somewhere wide enough for the real grid to lay out more than one column.
    // Kept apart from `stage` because every grab in this file is a grab of
    // `stage`, and a grid parked in it would be in all of them.
    Item {
        id: page
        anchors.fill: parent
    }

    // The whole grid, in WeatherDetails.qml's order. Adding a card to the app
    // and not to this list is caught by the count check below.
    //
    // `gesture` is whether the card has one at all, and it is the half of this
    // table that is fixed: DetailCard.qml's hover block argues each of the five
    // that do not, and a sixth appearing here is a design decision that belongs
    // in that block first.
    //
    // `gap` is whether the *fixture* gives that gesture anywhere to go, and it
    // has to be asked rather than assumed. Four of the eight walk from the
    // reading to a second number in the same data, and two numbers that happen
    // to be equal make a card that is correctly, honestly still: today's UV is
    // already the peak, or the air is not going anywhere in three hours. The
    // sight line is the one this actually bites on — its scale stops at 20 km
    // because a public forecast stops distinguishing past it, so on a clear day
    // both ends of its walk clamp to "as far as you like" and there is nothing
    // to draw. That is the card being right, and a test that demanded movement
    // there would be demanding a lie.
    //
    // No `gap` means the gesture is unconditional. Wind veers because its band
    // is never zero-width; the sun and moon always have a crossing that is not
    // now; the terminator always moves in two nights.
    readonly property var grid: [
        { tag: "Temperature",   type: "DetailTemperatureCard",   gesture: false },
        { tag: "FeelsLike",     type: "DetailFeelsLikeCard",     gesture: false },
        { tag: "CloudCover",    type: "DetailCloudCoverCard",    gesture: true,
          gap: function() { return Detail.cloudCover.soon - Detail.cloudCover.value } },
        { tag: "Precipitation", type: "DetailPrecipitationCard", gesture: false },
        { tag: "Wind",          type: "DetailWindCard",          gesture: true },
        { tag: "Humidity",      type: "DetailHumidityCard",      gesture: false },
        { tag: "Uv",            type: "DetailUvCard",            gesture: true,
          gap: function() { return Detail.uv.peak - Detail.uv.value } },
        { tag: "AirQuality",    type: "DetailAirQualityCard",    gesture: true,
          gap: function() { return Detail.airQuality.soon - Detail.airQuality.value } },
        { tag: "Visibility",    type: "DetailVisibilityCard",    gesture: true,
          // Both ends against the ceiling, because that is where the card reads
          // them: the lit length is a fraction of the scale, not of the sky.
          gap: function() {
              var top = Detail.visibility.scaleMax
              return Math.min(Detail.visibility.peak, top)
                   - Math.min(Detail.visibility.value, top)
          } },
        { tag: "Pressure",      type: "DetailPressureCard",      gesture: false },
        { tag: "Sun",           type: "DetailSunCard",           gesture: true },
        { tag: "Moon",          type: "DetailMoonCard",          gesture: true },
        { tag: "MoonPhase",     type: "DetailMoonPhaseCard",     gesture: true }
    ]

    // Whether this card, on this fixture, has something to say.
    function hasGap(row) { return typeof row.gap === "function" }

    function expectedToMove(row) {
        return row.gesture && (!hasGap(row) || Math.abs(row.gap()) > 0)
    }

    // For the failure message only, and it must stay lazy about `gap`: a row
    // without one is the common case and calling it anyway is what broke this
    // file the first time.
    function travel(row) {
        return hasGap(row) ? " and " + row.gap() + " to travel" : ""
    }

    // The card the running test function built, so `cleanup()` can get rid of it
    // even when an assertion or a scripting error walked out of the function
    // first. Every grab in this file is a grab of `stage`, so one orphan left
    // parented there — animating, because it was hovered when the function
    // died — silently joins every later comparison. That is not hypothetical:
    // it is how one eager expression in a failure message turned a single
    // failing row into ten.
    property var live: null

    property var liveGrid: null

    function cleanup() {
        if (testCase.liveGrid !== null) {
            testCase.liveGrid.destroy()
            testCase.liveGrid = null
        }
        if (testCase.live !== null) {
            testCase.live.destroy()
            testCase.live = null
        }
        Theme.stillness = false
        // destroy() is deferred, and the next row's first grab must not catch
        // the outgoing card still parented.
        tryVerify(function() { return stage.children.length === 0 }, 1000,
                  "a card outlived the test that built it")
    }

    function build(typeName) {
        var component = Qt.createComponent("Clima", typeName)
        verify(component !== null && component.status !== Component.Error,
               typeName + " does not build: "
               + (component === null ? "no such type" : component.errorString()))
        var card = component.createObject(stage, {})
        verify(card !== null, typeName + ": createObject returned null")
        testCase.live = card
        card.width = stage.width
        card.height = stage.height
        // Past the reveal, which is a one-shot on a timer and would otherwise be
        // the thing moving between two grabs.
        tryVerify(function() { return card.reveal === 1 }, 3000)
        wait(Theme.motion.reveal + 120)
        return card
    }

    // ---- the rest state ----------------------------------------------------

    function test_everyCardIsBuiltStillAndStaysThere_data() { return testCase.grid }

    function test_everyCardIsBuiltStillAndStaysThere(row) {
        var card = build(row.type)
        compare(card.hovered, false, row.tag + " reports a pointer nobody moved")
        compare(card.hoverPhase, 0, row.tag + " opens its envelope unasked")
        compare(card.hoverWalk, 0, row.tag + " is already walking")
    }

    function test_theGridHasThirteenCards() {
        // WeatherDetails.qml's Repeater is the other end of this list. A card
        // added there and not here would never be asked whether it moves.
        compare(testCase.grid.length, 13)
    }

    // ---- the gesture -------------------------------------------------------

    // `hoverPhase` is written rather than the pointer moved, and deliberately:
    // it isolates the *gesture* from the card's hover tint, which is a property
    // of the card's own background and would make all thirteen differ. What is
    // under test here is whether the visualisation moved.
    //
    // The waits are read off the rhythm rather than written down again, so a
    // change to `Theme.motion` moves the sampling with it. `farEnd` lands in
    // the middle of the pause at the top of the walk — where the walking cards
    // are furthest from their reading and the wind is near its widest veer —
    // and `closed` is past the envelope's own close, after which nothing is
    // left running.
    readonly property int farEnd: Theme.motion.reveal + Theme.motion.dwell / 2
    readonly property int closed: Theme.motion.move * 2 + 60

    function test_hoverMovesTheRightCardsAndLeavingRestoresThem_data() { return testCase.grid }

    function test_hoverMovesTheRightCardsAndLeavingRestoresThem(row) {
        var card = build(row.type)

        var rest = grabImage(stage)

        card.hoverPhase = 1
        wait(testCase.farEnd)
        var stirred = grabImage(stage)

        if (expectedToMove(row))
            verify(!stirred.equals(rest),
                   row.tag + " has a gesture" + travel(row) + " and drew the same card. "
                   + "It is not reaching the visualisation — check that the walk is "
                   + "multiplied by `hoverWalk` or `hoverPhase`.")
        else if (row.gesture)
            verify(stirred.equals(rest),
                   row.tag + " has nowhere to go in this fixture and moved anyway. A "
                   + "walk of zero has to draw zero, or the card is inventing a "
                   + "reading between two numbers that are the same.")
        else
            verify(stirred.equals(rest),
                   row.tag + " moved on hover and has no gesture. DetailCard.qml's "
                   + "hover block says why it should not: if that has been "
                   + "reconsidered, the argument goes there and this row gains one.")

        card.hoverPhase = 0
        wait(testCase.closed)
        var settled = grabImage(stage)

        verify(settled.equals(rest),
               row.tag + " did not come back. Every hover gesture is multiplied by "
               + "`hoverPhase`, which is exactly 0 at rest — a card that lands "
               + "anywhere else has a gesture the envelope does not reach, and the "
               + "golden images for that card become a coin toss.")
    }

    // ---- the pointer, and stillness ----------------------------------------

    // The one test that moves a real pointer. Everything above writes
    // `hoverPhase`, which proves the arithmetic and would go on passing if the
    // HoverHandler were deleted.
    function test_aPointerOpensAndClosesTheEnvelope() {
        var card = build("DetailWindCard")

        mouseMove(card, card.width / 2, card.height / 2)
        tryVerify(function() { return card.hovered }, 2000,
                  "the pointer is over the card and it does not know")
        tryVerify(function() { return card.hoverPhase === 1 }, 2000,
                  "hovered, and the envelope never opened")

        // Off the card and out of the stage, since the stage is exactly the
        // card: a move to a corner would still be inside it.
        mouseMove(testCase, 2, 2)
        tryVerify(function() { return !card.hovered }, 2000,
                  "the pointer left and the card is still lit")
        tryVerify(function() { return card.hoverPhase === 0 }, 2000,
                  "the pointer left and the envelope stayed open")
    }

    // The one test that hovers a card where the app actually puts it: inside
    // WeatherDetails' grid, which reaches each card through a Loader. Everything
    // above builds a card straight onto the stage, and a Loader that swallowed
    // hover would leave all of it passing and the feature dead on the page.
    //
    // The grid is the app's, not a copy: it lays out its own twelve Loaders off
    // `Theme.metric.detailCardWidth`, so pointing at the middle of the first
    // card's box is pointing at the first card.
    function test_aCardInsideTheRealGridTakesAHover() {
        var component = Qt.createComponent("Clima", "WeatherDetails")
        verify(component !== null && component.status !== Component.Error,
               "WeatherDetails does not build: "
               + (component === null ? "no such type" : component.errorString()))

        var grid = component.createObject(page, {})
        verify(grid !== null, "WeatherDetails: createObject returned null")
        testCase.liveGrid = grid
        grid.width = page.width
        tryVerify(function() { return grid.height > 0 }, 2000, "the grid never laid out")

        // Into the first card, and far enough in to clear its edge.
        mouseMove(grid, 40, grid.y + 60)

        // Walked rather than indexed: the grid's own children are a header and a
        // Grid, and the cards are a Loader deeper again. What is under test is
        // that one of them took the hover, not where it sits in the tree.
        var lit = countHovered(grid)

        compare(lit, 1,
                "pointing at the details grid lit " + lit + " cards. One is a card "
                + "that knows where the pointer is; none means a Loader or the "
                + "page's Flickable is eating the hover, which is the whole "
                + "feature gone with every other test in this file still passing.")
    }

    function countHovered(item) {
        var n = item.hovered === true ? 1 : 0
        for (var i = 0; i < item.children.length; ++i)
            n += countHovered(item.children[i])
        return n
    }

    // The reduced-motion contract, and the capture's. Stillness is the switch
    // both go through, so this is the assertion that stops a hover gesture from
    // becoming the second infinite animation in the product for a reader who
    // asked for none.
    function test_stillnessKeepsTheTintAndRefusesTheGesture() {
        var card = build("DetailWindCard")
        var rest = grabImage(stage)

        // Restored by cleanup(), which runs whether or not this function
        // reaches its end. A test that left stillness on would silence every
        // row after it and report a clean grid.
        Theme.stillness = true

        mouseMove(card, card.width / 2, card.height / 2)
        tryVerify(function() { return card.hovered }, 2000)

        // Hovered, and pinned. Not "settles back to 0" — it never leaves it.
        compare(card.hoverPhase, 0,
                "a card under stillness opened its envelope; a --grab that caught "
                + "a pointer would then photograph a card mid-gesture")

        // The tint is a state and not a gesture, so it still answers — instantly
        // here, because stillness collapses its duration too. This is the half
        // that must survive: a reader who has asked for less movement still gets
        // to see which card they are pointing at.
        wait(50)
        var lit = grabImage(stage)
        verify(!lit.equals(rest),
               "a card under stillness did not take the hover tint either — the "
               + "affordance is a colour, and a colour is not a movement")

        wait(testCase.farEnd)
        verify(grabImage(stage).equals(lit),
               "a card under stillness moved its visualisation")

        mouseMove(testCase, 2, 2)
    }
}
