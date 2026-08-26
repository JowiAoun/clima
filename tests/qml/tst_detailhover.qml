// SPDX-FileCopyrightText: 2026 Jowi Aoun
// SPDX-License-Identifier: GPL-3.0-or-later
//
// The wind rose's hover, the rule that keeps it the only one, and the compass
// gaps it is drawn over.
//
// DetailCard.qml's hover block sets out the rule — a card moves on hover only
// where the still card is silent about something the reading itself does — and
// two invariants everything else depends on:
//
//   - a card at rest is the card that existed before any of this, to the pixel,
//     because that is what fifty golden images assert every run;
//   - `Theme.stillness` suppresses it, because that one switch serves both a
//     reader who asked their desktop for less motion and a capture that must
//     not be caught mid-gesture.
//
// The assertions are made on pixels rather than on properties. A test that read
// `hoverPhase` back would prove the envelope arithmetic and nothing about
// whether the wedge is wired to it: a card that forgot to multiply by it passes
// that test and fails every golden. Grabbing the card at rest, again mid-drift
// and again after the pointer leaves catches both halves — that something moved,
// and that it all came back.
//
// The twelve rows that must *not* move are the half that rots. A grid where one
// card answers a pointer and eleven do not looks uneven, and the fix somebody
// reaches for is to give the others something to do. That is the argument in
// DetailCard.qml, and this is where ignoring it fails with a name attached.
import QtQuick
import QtTest
import Clima

import "qrc:/qt/qml/Clima/chartmath.js" as ChartMath

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

    // Somewhere wide enough for the real grid to lay out. Kept apart from
    // `stage` because every grab here is a grab of `stage`, and a grid parked in
    // it would be in all of them.
    Item {
        id: page
        anchors.fill: parent
    }

    // The whole grid, in WeatherDetails.qml's order.
    readonly property var grid: [
        { tag: "Temperature",   type: "DetailTemperatureCard",   moves: false },
        { tag: "FeelsLike",     type: "DetailFeelsLikeCard",     moves: false },
        { tag: "CloudCover",    type: "DetailCloudCoverCard",    moves: false },
        { tag: "Precipitation", type: "DetailPrecipitationCard", moves: false },
        { tag: "Wind",          type: "DetailWindCard",          moves: true  },
        { tag: "Humidity",      type: "DetailHumidityCard",      moves: false },
        { tag: "Uv",            type: "DetailUvCard",            moves: false },
        { tag: "AirQuality",    type: "DetailAirQualityCard",    moves: false },
        { tag: "Visibility",    type: "DetailVisibilityCard",    moves: false },
        { tag: "Pressure",      type: "DetailPressureCard",      moves: false },
        { tag: "Sun",           type: "DetailSunCard",           moves: false },
        { tag: "Moon",          type: "DetailMoonCard",          moves: false },
        { tag: "MoonPhase",     type: "DetailMoonPhaseCard",     moves: false }
    ]

    // The card the running function built, so `cleanup()` can get rid of it even
    // when an assertion or a scripting error walked out first. Every grab is a
    // grab of `stage`, so one orphan left parented there — drifting, because it
    // was hovered when the function died — silently joins every later
    // comparison.
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
    }

    function test_theGridHasThirteenCardsAndOneThatMoves() {
        // WeatherDetails.qml's Repeater is the other end of the first number. A
        // card added there and not here is never asked whether it moves.
        compare(testCase.grid.length, 13)

        var movers = 0
        for (var i = 0; i < testCase.grid.length; ++i)
            if (testCase.grid[i].moves)
                ++movers
        compare(movers, 1,
                "a second card has been given a hover gesture. That is a design "
                + "decision and it belongs in DetailCard.qml's hover block first, "
                + "with the argument for why the still card was silent.")
    }

    // ---- the gesture -------------------------------------------------------

    // `hoverPhase` is written rather than the pointer moved, and deliberately:
    // it drives the gesture without touching anything else, so a difference
    // between two grabs is the visualisation and nothing but.
    //
    // The waits are read off the rhythm rather than written down again. The
    // wedge crosses the dial in `driftPeriod`, which is 2.6 s at a dead calm and
    // less as the wind gets up; a third of the slowest crossing is far enough
    // along to have moved and not so far as to have wrapped.
    readonly property int crossing: 900
    readonly property int closed: Theme.motion.move * 2 + 60

    function test_hoverMovesTheRightCardsAndLeavingRestoresThem_data() { return testCase.grid }

    function test_hoverMovesTheRightCardsAndLeavingRestoresThem(row) {
        var card = build(row.type)

        var rest = grabImage(stage)

        card.hoverPhase = 1
        wait(testCase.crossing)
        var stirred = grabImage(stage)

        if (row.moves)
            verify(!stirred.equals(rest),
                   row.tag + " drew the same card a third of a crossing in. The "
                   + "drift is not reaching the wedge — check that `driftBy` is "
                   + "multiplied into the path and by `hoverPhase`.")
        else
            verify(stirred.equals(rest),
                   row.tag + " moved on hover. DetailCard.qml's hover block says "
                   + "why it should not: if that has been reconsidered, the "
                   + "argument goes there and this row's `moves` becomes true.")

        card.hoverPhase = 0
        wait(testCase.closed)
        var settled = grabImage(stage)

        verify(settled.equals(rest),
               row.tag + " did not come back. Every hover gesture is multiplied by "
               + "`hoverPhase`, which is exactly 0 at rest — a card that lands "
               + "anywhere else has a gesture the envelope does not reach, and the "
               + "golden images for that card become a coin toss.")
    }

    // The drift is a translation along one axis and nothing else. A wedge that
    // rotated, grew or breathed would satisfy the test above just as well —
    // and rotating is exactly what this gesture did in its first form and was
    // wrong for. A bearing is a measurement; turning the wedge off it draws a
    // wind that is not blowing.
    //
    // Two measurements off the accent-coloured pixels. Their count says nothing
    // grew or shrank. Their centroid says where the paint went, and the axis it
    // went along is the test: downwind is a direction the data names, so the
    // movement can be resolved into a component along it and one across it.
    // A translation is all of the first and none of the second; a rotation about
    // the rose is mostly the second.
    //
    // The gust band is painted in the same accent and does not move, so it sits
    // in both measurements as a constant. That shortens the centroid's travel
    // and cannot tilt it, which is why this is a ratio rather than a distance.
    function test_theWedgeTravelsDownwindWithoutTurning() {
        var card = build("DetailWindCard")

        card.hoverPhase = 1
        wait(400)
        var a = accentStats(grabImage(stage))
        wait(testCase.crossing)
        var b = accentStats(grabImage(stage))

        verify(a.n > 40 && b.n > 40, "the wedge is not being painted at all")

        // Antialiasing moves a handful of edge pixels; a wedge that was growing
        // would move hundreds.
        verify(Math.abs(b.n - a.n) < a.n * 0.12,
               "the accent covers " + a.n + " pixels and then " + b.n
               + ". A drift changes where the wedge is, not how much of it there "
               + "is — check that the path is built about a moved centre rather "
               + "than being stretched.")

        // Screen axes: x right, y down. A bearing is degrees clockwise from
        // north and names where the wind comes *from*, so downwind is the
        // negative of it.
        var rad = Detail.wind.directionDeg * Math.PI / 180
        var dx = -Math.sin(rad), dy = Math.cos(rad)

        var mx = b.cx - a.cx, my = b.cy - a.cy
        var along  = mx * dx + my * dy
        var across = Math.abs(mx * -dy + my * dx)

        verify(along > 1.5,
               "the wedge moved " + along.toFixed(2) + " px downwind. From "
               + Detail.wind.directionDeg + "° it should be carried away from the "
               + "quarter it blows from, not toward it and not nowhere.")

        verify(across < along * 0.4,
               "the wedge moved " + across.toFixed(2) + " px across the wind "
               + "against " + along.toFixed(2) + " along it. That is a turn, and a "
               + "bearing is a measurement — the wedge keeps its angle and only "
               + "its position may change.")
    }

    // How much accent is painted, and where its middle is.
    function accentStats(img) {
        var sx = 0, sy = 0, n = 0
        for (var y = 0; y < stage.height; ++y) {
            for (var x = 0; x < stage.width; ++x) {
                var c = img.pixel(x, y)
                if (c.g - c.r > 0.10 && c.g - c.b > 0.10) {
                    sx += x; sy += y; ++n
                }
            }
        }
        return { cx: n > 0 ? sx / n : 0, cy: n > 0 ? sy / n : 0, n: n }
    }

    // ---- the compass gaps --------------------------------------------------

    // The ring is broken at each cardinal so the letter has somewhere to sit,
    // and the gust band is the ring painted green — so the band has to be broken
    // in the same four places. It was not: the band was one span with its ends
    // pushed clear of any gap they landed in, which cannot help when the band's
    // *centre* is in the gap. A wind from within 12° of a cardinal drew the arc
    // straight through the letter, and the Toronto fixture blows from 194° —
    // fourteen degrees off due south — so that is what shipped.
    //
    // Green rather than the accent's exact value: the point is that no part of
    // the band is inside a letter, not what colour the band is this week, and a
    // literal here would go on passing through a palette change.
    function test_theGustBandStaysOutOfTheCompassLetters() {
        var card = build("DetailWindCard")
        var img = grabImage(stage)

        var letters = cardinals(card)
        compare(letters.length, 4, "found " + letters.length + " cardinal labels, not 4")

        for (var i = 0; i < letters.length; ++i) {
            var box = letters[i]
            var green = 0
            for (var y = Math.max(0, box.y); y < Math.min(stage.height, box.y + box.h); ++y) {
                for (var x = Math.max(0, box.x); x < Math.min(stage.width, box.x + box.w); ++x) {
                    var c = img.pixel(x, y)
                    if (c.g - c.r > 0.10 && c.g - c.b > 0.10)
                        ++green
                }
            }
            compare(green, 0,
                    "the gust band paints " + green + " pixels inside the \"" + box.text
                    + "\" label. The ring has no arc in that gap and neither may the "
                    + "band — see `gustPath`.")
        }
    }

    // The pixel check above can only ever ask about the one bearing the fixture
    // has, and Toronto blows from 194° — fourteen degrees clear of due south and
    // therefore outside the S gap, which is exactly where the old code happened
    // to work. It shipped a band drawn through the letter at 191° and every
    // golden image agreed with it.
    //
    // So the invariant is asserted where it can be asserted everywhere: on the
    // arithmetic, at every bearing on the compass. This is the check that would
    // have caught it.
    readonly property int gapHalf: 12

    function test_noBandIsEverDrawnInsideACompassGap() {
        var worst = null

        for (var deg = 0; deg < 360; ++deg) {
            // The narrowest band this card can draw is ±8° and the widest ±38°.
            for (var half = 8; half <= 38; half += 2) {
                var spans = ChartMath.compassSpans(deg - half, deg + half, testCase.gapHalf)
                for (var i = 0; i < spans.length; ++i) {
                    if (insideAGap(spans[i].from) || insideAGap(spans[i].to)) {
                        worst = { deg: deg, half: half,
                                  from: spans[i].from, to: spans[i].to }
                        break
                    }
                }
                if (worst !== null) break
            }
            if (worst !== null) break
        }

        compare(worst, null,
                worst === null ? "" :
                "from " + worst.deg + "° at ±" + worst.half + "° the band runs "
                + worst.from.toFixed(1) + "° to " + worst.to.toFixed(1)
                + "°, which is inside a cardinal's gap. The ring has no arc there, "
                + "so the letter is sitting on green.")
    }

    // Nothing is lost either. Clipping a band to the gaps must remove the gaps
    // and not a degree more — a band that came back short would be understating
    // how unsettled the wind is, which is the one thing it is drawn to say.
    function test_clippingRemovesTheGapsAndNothingElse() {
        for (var deg = 0; deg < 360; deg += 7) {
            var half = 24
            var spans = ChartMath.compassSpans(deg - half, deg + half, testCase.gapHalf)

            var drawn = 0
            for (var i = 0; i < spans.length; ++i)
                drawn += spans[i].to - spans[i].from

            // What the gaps take out of this span, counted directly.
            var lost = 0
            for (var a = deg - half; a < deg + half; a += 0.25)
                if (insideAGap(a))
                    lost += 0.25

            verify(Math.abs(drawn - (2 * half - lost)) < 1,
                   "from " + deg + "° the band should draw " + (2 * half - lost).toFixed(1)
                   + "° and draws " + drawn.toFixed(1) + "°")
        }
    }

    // Same call, and this is the whole point of there being one: ask it for the
    // full circle and it must hand back the ring exactly as the ring is drawn.
    function test_theRingIsTheSameCallAsTheBand() {
        var spans = ChartMath.compassSpans(0, 360, testCase.gapHalf)
        compare(spans.length, 4, "the ring is four arcs")
        for (var k = 0; k < 4; ++k) {
            compare(spans[k].from, k * 90 + testCase.gapHalf)
            compare(spans[k].to, (k + 1) * 90 - testCase.gapHalf)
        }
    }

    // Is this angle in the break a cardinal letter sits in? Written out rather
    // than derived, so it is an independent statement of the rule and not the
    // implementation checking itself.
    function insideAGap(deg) {
        var a = ((deg % 360) + 360) % 360
        for (var k = 0; k < 4; ++k) {
            var c = k * 90
            var lo = ((c - testCase.gapHalf) + 360) % 360
            var hi = (c + testCase.gapHalf) % 360
            if (lo > hi ? (a > lo || a < hi) : (a > lo && a < hi))
                return true
        }
        return false
    }

    // The four compass letters, in stage coordinates. Walked rather than
    // indexed: they are a Repeater's delegates inside the card's content
    // Component, and what matters is where they ended up.
    function cardinals(item) {
        var out = []
        collectCardinals(item, out)
        return out
    }

    function collectCardinals(item, out) {
        var t = item.text
        if (t === "N" || t === "E" || t === "S" || t === "W") {
            var p = item.mapToItem(stage, 0, 0)
            out.push({ text: t, x: Math.round(p.x), y: Math.round(p.y),
                       w: Math.ceil(item.width), h: Math.ceil(item.height) })
        }
        for (var i = 0; i < item.children.length; ++i)
            collectCardinals(item.children[i], out)
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

    // Hovered where the app actually puts a card: inside WeatherDetails' grid,
    // which reaches each one through a Loader. Everything above builds a card
    // straight onto the stage, and a Loader that swallowed hover would leave all
    // of it passing and the feature dead on the page.
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
    // both go through, so this is what stops the drift from becoming the second
    // standing animation in the product for a reader who asked for none.
    function test_stillnessRefusesTheGesture() {
        var card = build("DetailWindCard")
        var rest = grabImage(stage)

        // Restored by cleanup(), which runs whether or not this function reaches
        // its end. A test that left stillness on would silence every row after
        // it and report a clean grid.
        Theme.stillness = true

        mouseMove(card, card.width / 2, card.height / 2)
        tryVerify(function() { return card.hovered }, 2000)

        // Hovered, and pinned. Not "settles back to 0" — it never leaves it.
        compare(card.hoverPhase, 0,
                "a card under stillness opened its envelope; a --grab that caught "
                + "a pointer would then photograph a card mid-drift")

        wait(testCase.crossing)
        verify(grabImage(stage).equals(rest),
               "a card under stillness drifted anyway")

        mouseMove(testCase, 2, 2)
    }
}
