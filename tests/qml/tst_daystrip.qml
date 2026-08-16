// SPDX-FileCopyrightText: 2026 Jowi Aoun
// SPDX-License-Identifier: GPL-3.0-or-later
//
// The day strip's tab merge, checked at every point along it rather than at
// the two ends.
//
// This exists because of a defect that no golden image could have caught and
// none did. A selected card reaches down to the chart panel and the junction
// either side of it is filleted; both ends of that looked right, and for the
// 190 ms in between, the fillet was drawn as a rounded wedge floating in the
// page background beside a card that had not touched the panel yet. Measured
// off a user's screenshot: the panel's top edge at y=140, the card the wedge
// belonged to at y=120, the card arriving next to it at y=134. Three surfaces
// that are one surface at rest.
//
// docs/screenshots.md says the thing this test is the answer to: "an animation
// that is wrong, or missing altogether, grabs identically to one that is
// right". Every recorded image of this strip is a resting state, and a resting
// state is exactly where the bug was invisible.
//
// ---- why it drives `merge` and not the clock ---------------------------------
//
// The merge is a pure function of one number, so the invariant is algebraic and
// can be checked at a hundred points in no time at all. Waiting on the real
// animation would sample it four or five times — measured: `scripts/film.sh`
// asking for 8 ms between frames gets nearer 50, because grabbing a frame costs
// more than a frame does — and four samples of a 190 ms window is how this got
// shipped in the first place.
//
// Assigning `merge` replaces its binding, which is why every delegate is only
// swept once and nothing here reads a card's selection afterwards. It also goes
// through the Behavior — a Behavior intercepts writes from JavaScript exactly as
// it does writes from a binding — so `Theme.stillness` is on for the whole file.
// That is the same switch reduced motion and `--grab` use, it collapses the
// duration to zero, and without it every step of the sweep starts a 190 ms
// animation and reads back the value it had before.
import QtQuick
import QtQuick.Window
import QtTest
import Clima

TestCase {
    name: "DayStrip"

    // The gallery stages this specimen at 1100x130 and so does this.
    Window {
        id: host
        width: 1100
        height: 200
        visible: true

        DayStrip {
            id: strip
            width: parent.width
            anchors.top: parent.top
        }
    }

    property bool wasStill: false

    function initTestCase() {
        wasStill = Theme.stillness
        Theme.stillness = true
    }

    function cleanupTestCase() {
        Theme.stillness = wasStill
    }

    // The delegates, found by what they are rather than by where they sit: the
    // path down to them runs Flickable -> contentItem -> Row -> Repeater and
    // its items, and three of those four are Qt's to rearrange. A day card is
    // the thing in there with a `merge` on it.
    function cards() {
        var found = []
        function walk(item) {
            for (var i = 0; i < item.children.length; ++i) {
                var child = item.children[i]
                if (child.merge !== undefined && child.landed !== undefined)
                    found.push(child)
                else
                    walk(child)
            }
        }
        walk(strip)
        return found
    }

    function test_thereAreCardsToMeasure() {
        verify(cards().length >= 3, "the strip staged no day cards")
    }

    // The whole of the fix, as one sentence: a junction is a corner between two
    // surfaces that touch, so the fillet may only be drawn where the card's
    // bottom edge is on the panel.
    function test_aFilletIsOnlyDrawnOnceTheCardHasLanded() {
        var card = cards()[0]
        var seated = strip.height + strip.mergeDepth

        for (var i = 0; i <= 100; ++i) {
            card.merge = i / 100

            if (card.filletSize > 0) {
                compare(card.landed, 1,
                        "merge " + card.merge + ": a fillet of " + card.filletSize
                        + " px beside a card that is only " + card.landed + " landed")
                compare(card.height, seated,
                        "merge " + card.merge + ": a fillet beside a card whose bottom edge is "
                        + (seated - card.height) + " px clear of the panel")
            }
        }
    }

    // The other half of the same sentence. The card's bottom corners are convex
    // and the fillet is concave; they cannot share an edge, so a fillet must not
    // be drawn while the corner beside it is still rounded. The Rectangle binds
    // its bottom radii to `cardRadius * (1 - landed)`, so this is that.
    function test_aFilletIsOnlyDrawnOnceTheCornerBesideItIsSquare() {
        var card = cards()[1]

        for (var i = 0; i <= 100; ++i) {
            card.merge = i / 100
            if (card.joined > 0)
                compare(1 - card.landed, 0,
                        "merge " + card.merge + ": joining " + card.joined
                        + " while the bottom corner is still " + (1 - card.landed) + " round")
        }
    }

    // Nothing here asks which direction the change is going, and this is what
    // that buys: run the same expressions backwards and the join comes apart
    // before the card lifts, because both are monotonic in the one driver. The
    // spelling this replaced asked — `duration: selected ? move : 0` — and a
    // Behavior can fire before the binding feeding its duration is re-evaluated,
    // so "leaving goes in one frame" held on some runs and not on others.
    function test_bothBeatsRunTheSameWayBackwards() {
        var card = cards()[2]
        var lastLanded = -1
        var lastJoined = -1

        for (var i = 0; i <= 100; ++i) {
            card.merge = i / 100
            verify(card.landed >= lastLanded,
                   "merge " + card.merge + ": landing went backwards")
            verify(card.joined >= lastJoined,
                   "merge " + card.merge + ": the join went backwards")
            lastLanded = card.landed
            lastJoined = card.joined
        }
    }

    // And the two ends, which are what the golden images record. A change that
    // moved either of them would move 46 recorded pixels-exact images with it,
    // so it should have to say so here first.
    function test_theRestingStatesAreUnchanged() {
        var card = cards()[3]

        card.merge = 0
        compare(card.landed, 0, "an unselected card is partly landed")
        compare(card.joined, 0, "an unselected card is partly joined")
        compare(card.filletSize, 0, "an unselected card has a fillet")
        compare(card.height, strip.height - strip.unselectedInset,
                "an unselected card is not the short one")
        compare(card.width, strip.cardWidth, "an unselected card is not the narrow one")

        card.merge = 1
        compare(card.landed, 1, "a selected card has not landed")
        compare(card.joined, 1, "a selected card has not joined")
        compare(card.filletSize, strip.filletRadius, "a selected card's fillet is not full size")
        compare(card.height, strip.height + strip.mergeDepth,
                "a selected card does not reach the panel")
        compare(card.width, strip.cardWidth + strip.selectedExtra,
                "a selected card is not the wide one")
    }
}
