// SPDX-FileCopyrightText: 2026 Jowi Aoun
// SPDX-License-Identifier: GPL-3.0-or-later
//
// One wet afternoon is one band.
//
// A precipitation run is drawn with an edge on each of its ends and captioned
// once, so how the hours are grouped into runs is not an internal detail — it is
// what the chart claims happened. Split a spell in two and the chart draws two
// edges where the weather did nothing, prints its name twice, and steps the
// wash's alpha in the middle. Reported from a real screen as "rain over rain
// twice on the same hour", which is exactly what it looks like.
//
// It happened because providers switch between drizzle and rain codes hour by
// hour inside a single spell — 51, 51, 61, 51, 51 is an ordinary Open-Meteo day
// — and the grouping was on the type. It is on the family now, and this file is
// the difference: what still splits a run, and what no longer does.
import QtQuick
import QtTest

import "qrc:/qt/qml/Clima/precip.js" as Precip

TestCase {
    name: "PrecipSpells"

    // The shape forecastdata.cpp publishes: one entry per hour, null where dry.
    function cells(spec) {
        var out = []
        for (var i = 0; i < spec.length; ++i) {
            var s = spec[i]
            out.push(s === null ? null
                                : { type: s[0],
                                    intensity: Precip.intensityFor(s[0], s[1]),
                                    mm: s[1] })
        }
        return out
    }

    // 51, 51, 61, 51, 51 — five hours of one spell, in the codes a provider
    // actually sends. Three runs before, one now.
    function test_aRainHourInsideDrizzleDoesNotCutTheSpellInThree() {
        var s = Precip.spans(cells([null,
                                    ["drizzle", 0.1], ["drizzle", 0.1],
                                    ["rain", 2.0],
                                    ["drizzle", 0.3], ["drizzle", 0.1],
                                    null]))
        compare(s.length, 1, "one spell, not three")
        compare(s[0].from, 1)
        compare(s[0].to, 5)
    }

    // The caption and the alpha come from the worst hour in the spell, not from
    // whichever hour happens to start it. Naming five hours "Drizzle" when one of
    // them is 2 mm of rain is the under-report the merge must not introduce.
    function test_aSpellIsNamedForItsPeakHourAndNotItsFirst() {
        var s = Precip.spans(cells([["drizzle", 0.1], ["rain", 2.0], ["drizzle", 0.1]]))
        compare(s.length, 1)
        compare(s[0].type, "rain")
        compare(s[0].peakMm, 2.0)
        compare(s[0].label, "Light rain")
    }

    // Providers round consecutive hours to the same tenth of a millimetre often
    // enough that a tie is the common case rather than the odd one, and a tie
    // broken by arrival order made the same two hours read "Drizzle" or "Rain"
    // depending which came first. The amount still decides first; this only
    // settles a draw.
    function test_aTieOnTheAmountIsBrokenTowardsTheStrongerWord_data() {
        return [
            { tag: "drizzle first", a: "drizzle", b: "rain" },
            { tag: "rain first",    a: "rain",    b: "drizzle" },
        ]
    }

    function test_aTieOnTheAmountIsBrokenTowardsTheStrongerWord(data) {
        var s = Precip.spans(cells([[data.a, 3.0], [data.b, 3.0]]))
        compare(s.length, 1)
        compare(s[0].type, "rain", data.tag + ": a run holding rain is not named drizzle")
    }

    // The tie-break is not a second opinion about strength. A heavier drizzle
    // hour beside a lighter rain one is still named for the heavier.
    function test_theAmountStillOutranksTheTieBreak() {
        var s = Precip.spans(cells([["drizzle", 4.0], ["rain", 0.5]]))
        compare(s.length, 1)
        compare(s[0].type, "drizzle")
        compare(s[0].peakMm, 4.0)
    }

    // Nothing else merges. Rain turning to snow is the change the seam between
    // two bands was invented to say, and thunder has to stay its own band for a
    // second reason: PrecipField flashes the band whose type is `thunder`, so a
    // storm folded into the rain around it would light up the whole afternoon.
    function test_everyOtherChangeOfWeatherIsStillTwoBands_data() {
        return [
            { tag: "rain to snow",    a: "rain",  b: "snow" },
            { tag: "rain to sleet",   a: "rain",  b: "sleet" },
            { tag: "rain to hail",    a: "rain",  b: "hail" },
            { tag: "rain to thunder", a: "rain",  b: "thunder" },
            { tag: "snow to thunder", a: "snow",  b: "thunder" },
        ]
    }

    function test_everyOtherChangeOfWeatherIsStillTwoBands(data) {
        var s = Precip.spans(cells([[data.a, 1.0], [data.a, 1.0], [data.b, 1.0], [data.b, 1.0]]))
        compare(s.length, 2, data.tag + " is two events")
        compare(s[0].to, 1)
        compare(s[1].from, 2)
    }

    // A dry hour still ends a spell however alike the weather either side of it
    // is: the band means "it is falling here", and drawing through a gap would
    // be the one claim the wash is not allowed to make.
    function test_aDryHourStillEndsASpell() {
        var s = Precip.spans(cells([["rain", 1.0], null, ["drizzle", 0.4]]))
        compare(s.length, 2)
        compare(s[0].from, 0)
        compare(s[0].to, 0)
        compare(s[1].from, 2)
        compare(s[1].to, 2)
    }

    // Runs never split on intensity — rain easing off is the same rain. This was
    // true before the family change and is the property it had to preserve, so
    // it is asserted rather than assumed.
    function test_intensityStillDoesNotSplitARun() {
        var s = Precip.spans(cells([["rain", 0.5], ["rain", 9.0], ["rain", 0.5]]))
        compare(s.length, 1)
        compare(s[0].intensity, "heavy")
    }

    // Bands never overlap — the other half of "two overlays on the same hour",
    // and the half that measured clean when it was reported. Only overlap, not
    // "no gap": a gap between two bands is a dry hour, which is the one thing
    // the wash is required to leave alone.
    function test_bandsNeverOverlap() {
        var spec = [null, ["drizzle", 0.1], ["rain", 2.0], ["drizzle", 0.2], null,
                    ["snow", 1.0], ["snow", 2.0], null, ["thunder", 5.0]]
        var s = Precip.spans(cells(spec))
        var hw = 10
        for (var i = 1; i < s.length; ++i) {
            var endOfPrevious = Precip.bandX(s[i - 1], hw)
                                + Precip.bandW(s[i - 1], hw, spec.length * hw)
            verify(Precip.bandX(s[i], hw) >= endOfPrevious,
                   "band " + i + " starts at or after the one before it ends")
        }
    }
}
