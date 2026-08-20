// SPDX-FileCopyrightText: 2026 Jowi Aoun
// SPDX-License-Identifier: GPL-3.0-or-later
//
// Thirteen conditions, thirteen different pictures — asserted on the pixels.
//
// This test exists because of a bug that was invisible in every screenshot the
// project had ever taken. `WeatherGlyph.qml` knew seven of `ConditionKind`'s
// thirteen kinds, so the engine ran everything through a `drawableToday()` that
// folded the other six into those seven before QML saw them: fog and snow to
// cloudy, drizzle, sleet, thunder and hail to rain. A thunderstorm therefore
// rendered as an ordinary shower, and an ordinary shower is a *plausible*
// picture of a thunderstorm — so the ten-day strip looked completely fine while
// telling you the wrong thing about Saturday.
//
// Nothing structural could have caught it. The kind was a valid string, the
// glyph was a valid glyph, the property bound, the component built, and
// tst_specimen was green. The only assertion that fails on it is one that looks
// at what was actually painted, which is what this file does: it grabs each
// kind and compares the bitmaps.
//
// ---- what a failure means -----------------------------------------------------
//
//   "…paints nothing"          a kind reached the vocabulary without a picture.
//                              It renders as an empty box, which reads as
//                              missing data rather than as weather.
//
//   "…are pixel-identical"     two conditions the forecast distinguishes are
//                              being drawn as the same thing. That is the
//                              original defect, restated.
import QtQuick
import QtTest
import Clima

TestCase {
    id: testCase
    name: "WeatherGlyph"
    when: windowShown
    width: 240
    height: 240

    // A TestCase is invisible by default, and an invisible item's subtree is
    // never rendered — a grab of one comes back the window's clear colour,
    // every pixel of it. That reads as "the glyph paints nothing", which is the
    // exact failure this file exists to report, so it would have been a very
    // convincing false positive. Nothing else under tests/qml/ looks at pixels,
    // which is why this is the first time it has mattered.
    visible: true

    // Every kind `clima::ConditionKind` can produce. Kept in the enum's order
    // so a reader can diff the two lists by eye; `conditionKindName()` in
    // libclima/domain/weathercode.cpp is the other end of this contract, and
    // tests/tst_weathercode.cpp asserts the same set from the C++ side.
    readonly property var kinds: [
        "clear-day", "clear-night", "partly-day", "partly-night", "cloudy",
        "fog", "drizzle", "rain", "rain-night", "sleet", "snow", "thunder", "hail"
    ]

    // Two conditions that are *meant* to paint the same pixels, and the only
    // pair allowed to. The enum keeps rain and rain-night apart because the
    // plate behind the glyph is a day plate or a night plate and that is where
    // the difference is carried — a moon behind a raining cloud is a sky nobody
    // can see. If a night form of rain is ever drawn, delete this, do not widen
    // it.
    readonly property var identicalPairs: [["rain", "rain-night"]]

    // Big enough that a 1-px mark is several pixels and a mistake is a
    // difference rather than a rounding. The ground is the page, because that
    // is what a glyph is composited over everywhere except the day badge.
    Rectangle {
        id: stage
        width: 56
        height: 56
        color: Theme.page.bg

        WeatherGlyph {
            id: glyph
            anchors.centerIn: parent
            glyphSize: 52
        }
    }

    // A bitmap reduced to "which pixels are not the background", plus how many
    // there are. Comparing the mask rather than the colours is deliberate: two
    // kinds that differ only in a token's value are still two kinds, and a
    // palette edit should not be able to fail this test.
    function maskOf(kind, onLight) {
        glyph.onLightBackground = onLight === true
        glyph.kind = kind
        // No waitForRendering: under the offscreen platform it reports false
        // whether or not a frame arrived, and `grabImage` renders the subtree
        // synchronously anyway. The writes above have already propagated — a
        // QML property assignment is not deferred.

        var img = grabImage(stage)
        var bg = Qt.color(stage.color)
        var mask = ""
        var marks = 0

        for (var y = 0; y < stage.height; ++y) {
            for (var x = 0; x < stage.width; ++x) {
                var c = img.pixel(x, y)
                var lit = Math.abs(c.r - bg.r) + Math.abs(c.g - bg.g) + Math.abs(c.b - bg.b) > 0.02
                mask += lit ? "1" : "0"
                if (lit)
                    ++marks
            }
        }
        return { mask: mask, marks: marks }
    }

    // Is `target` painted anywhere, give or take the fraction of a channel that
    // antialiasing moves an edge pixel by? Only ever asked about flat fills —
    // a gradient stop is reached at exactly one row and is not a safe needle.
    function paints(kind, target, onLight) {
        glyph.onLightBackground = onLight === true
        glyph.kind = kind
        // No waitForRendering: under the offscreen platform it reports false
        // whether or not a frame arrived, and `grabImage` renders the subtree
        // synchronously anyway. The writes above have already propagated — a
        // QML property assignment is not deferred.

        var img = grabImage(stage)
        var want = Qt.color(target)

        for (var y = 0; y < stage.height; ++y) {
            for (var x = 0; x < stage.width; ++x) {
                var c = img.pixel(x, y)
                if (Math.abs(c.r - want.r) < 0.02
                        && Math.abs(c.g - want.g) < 0.02
                        && Math.abs(c.b - want.b) < 0.02)
                    return true
            }
        }
        return false
    }

    // ---- the vocabulary is complete ---------------------------------------

    function test_everyKindPaintsSomething_data() {
        var rows = []
        for (var i = 0; i < testCase.kinds.length; ++i)
            rows.push({ tag: testCase.kinds[i], kind: testCase.kinds[i] })
        return rows
    }

    function test_everyKindPaintsSomething(data) {
        var got = maskOf(data.kind)
        verify(got.marks > 0,
               "\"" + data.kind + "\" paints nothing — WeatherGlyph.qml has no picture for it, "
               + "so it renders as an empty box and reads as missing data")

        // A floor as well as a non-zero, because a single stray antialiased
        // pixel would satisfy the line above and is not a glyph.
        verify(got.marks > 40,
               "\"" + data.kind + "\" paints only " + got.marks + " pixels at 52 px")
    }

    // ---- and nothing in it is a synonym for anything else -----------------

    function test_noTwoKindsPaintTheSamePixels() {
        var masks = ({})
        var i
        for (i = 0; i < testCase.kinds.length; ++i)
            masks[testCase.kinds[i]] = maskOf(testCase.kinds[i]).mask

        var exempt = ({})
        for (i = 0; i < testCase.identicalPairs.length; ++i) {
            var pair = testCase.identicalPairs[i]
            exempt[pair[0] + "/" + pair[1]] = true
            exempt[pair[1] + "/" + pair[0]] = true
        }

        var clashes = []
        for (i = 0; i < testCase.kinds.length; ++i) {
            for (var j = i + 1; j < testCase.kinds.length; ++j) {
                var a = testCase.kinds[i]
                var b = testCase.kinds[j]
                var same = masks[a] === masks[b]

                if (exempt[a + "/" + b]) {
                    verify(same, a + " and " + b + " are listed as a deliberate pair but no "
                           + "longer paint the same pixels — the list is now the stale half "
                           + "of the contract")
                    continue
                }
                if (same)
                    clashes.push(a + " and " + b)
            }
        }

        compare(clashes.length, 0,
                clashes.length + " condition(s) are pixel-identical to another and the forecast "
                + "cannot tell you which one you are looking at:\n  " + clashes.join("\n  "))
    }

    // The six the engine used to fold away, named one at a time against what it
    // used to fold them into. The test above already covers these — this is the
    // regression stated in the words of the bug, so a failure says which
    // downgrade came back rather than only that two masks matched.
    function test_theFoldedKindsAreNoLongerTheirFallback_data() {
        return [
            { tag: "thunder was rain",   kind: "thunder", was: "rain" },
            { tag: "hail was rain",      kind: "hail",    was: "rain" },
            { tag: "drizzle was rain",   kind: "drizzle", was: "rain" },
            { tag: "sleet was rain",     kind: "sleet",   was: "rain" },
            { tag: "snow was cloudy",    kind: "snow",    was: "cloudy" },
            { tag: "fog was cloudy",     kind: "fog",     was: "cloudy" },
        ]
    }

    function test_theFoldedKindsAreNoLongerTheirFallback(data) {
        var now = maskOf(data.kind).mask
        var old = maskOf(data.was).mask
        verify(now !== old,
               "\"" + data.kind + "\" is being drawn as \"" + data.was + "\" again")
    }

    // Lightning, specifically. The rest of this file would still pass if
    // thunder differed from rain by one extra raindrop, and the thing a reader
    // is looking for on a stormy Saturday is a bolt.
    function test_aStormHasLightningInIt() {
        verify(paints("thunder", Theme.glyph.bolt, false),
               "the thunder glyph paints no Theme.glyph.bolt anywhere")
        verify(paints("hail", Theme.glyph.bolt, false),
               "the hail glyph paints no bolt — WMO 96 and 99 are the only codes that reach it "
               + "and both of them are thunderstorms")
        verify(!paints("rain", Theme.glyph.bolt, false),
               "the plain rain glyph has a bolt in it")
    }

    // ---- the pale plate ----------------------------------------------------

    // DayIconBadge draws the glyph on a near-white disc, which is where a pale
    // mark goes invisible — a `glyph.rain` drop measures 2.13:1 on dark's day
    // plate, under the 3:1 an essential mark owes its ground. Every falling
    // mark therefore has a second token for that plate, and this catches the
    // half-migration where the token was added and the binding was not.
    function test_everyMarkRespectsThePaleGround_data() {
        return [
            { tag: "rain",    kind: "rain",    inks: ["rain"] },
            { tag: "drizzle", kind: "drizzle", inks: ["rain"] },
            { tag: "sleet",   kind: "sleet",   inks: ["rain", "snow"] },
            { tag: "snow",    kind: "snow",    inks: ["snow"] },
            { tag: "hail",    kind: "hail",    inks: ["snow", "bolt"] },
            { tag: "thunder", kind: "thunder", inks: ["bolt"] },
        ]
    }

    function test_everyMarkRespectsThePaleGround(data) {
        for (var i = 0; i < data.inks.length; ++i) {
            var name = data.inks[i]
            var card = Theme.glyph[name]

            verify(paints(data.kind, card, false),
                   "\"" + data.kind + "\" paints no Theme.glyph." + name + " on a card, so this "
                   + "row is measuring nothing")
            verify(!paints(data.kind, card, true),
                   "\"" + data.kind + "\" still paints Theme.glyph." + name + " on the pale day "
                   + "plate, where it needs Theme.glyph." + name + "OnLight")
        }

        // Only the colours are asserted, and not that the mask is unchanged.
        // That was tried and it is not a real invariant: the mask is "differs
        // from the ground", the pale-plate inks are nearer this navy ground
        // than the card inks are, and so a handful of antialiased edge pixels
        // legitimately drop below the threshold. Drizzle and snow — the two
        // finest marks, with the most edge per unit of area — failed it while
        // being pixel-perfect.
    }

    function test_anUnknownKindPaintsNothing() {
        // The other half of "every kind paints something": a name QML does not
        // recognise must render empty rather than pick a neighbour. That is
        // what makes the vocabulary test above able to fail at all.
        var got = maskOf("thunderstorms-with-a-chance-of-meatballs")
        compare(got.marks, 0, "an unrecognised kind painted " + got.marks + " pixels")
    }
}
