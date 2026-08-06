// SPDX-FileCopyrightText: 2026 Jowi Aoun
// SPDX-License-Identifier: GPL-3.0-or-later
//
// The palette, as a set of promises rather than as a picture.
//
// Three kinds of defect live here and all three are silent. A token misspelled
// at the point of use resolves to `undefined`, which QML turns into a
// transparent colour — so the component renders invisibly and reports success.
// A token added to theme.js and forgotten in themelight.js is the same failure
// wearing one theme. And a colour that is simply too close to what it is drawn
// on cannot be caught by any amount of instantiation, because nothing about it
// is wrong except that you cannot see it.
//
// The contrast section is the gallery's Colour page asserted rather than
// rendered. The page is for a person deciding what a palette should be; this is
// for CI noticing that it stopped being that.
import QtQuick
import QtTest
import Clima

import "qrc:/qt/qml/Clima/sky.js" as Sky

// The audit arithmetic, out of the gallery module rather than reimplemented.
// By resource URL and not by a relative path: this file is loaded off disk by
// the QtQuickTest runner, so `../../gallery/...` would work today and break the
// day the test moves, while the resource path is the module's own address and
// is the same one Gallery.qml resolves. A second copy of WCAG's transfer
// function is the one thing that would make this test agree with a bug.
import "qrc:/qt/qml/Clima/Gallery/contrast.js" as Contrast

TestCase {
    name: "Theme"

    // Both raw tables, built the way the palette page builds them.
    function tableFor(scheme) {
        var table = {}
        for (var i = 0; i < Theme.colorRoles.length; ++i)
            table[Theme.colorRoles[i]] = Theme.tokensFor(Theme.colorRoles[i], scheme)
        return table
    }

    // ---- shape ------------------------------------------------------------

    function test_everyRoleExistsInBothTables() {
        for (var i = 0; i < Theme.colorRoles.length; ++i) {
            var role = Theme.colorRoles[i]
            verify(Theme.tokensFor(role, "dark") !== undefined, "dark has no " + role)
            verify(Theme.tokensFor(role, "light") !== undefined, "light has no " + role)
            verify(Object.keys(Theme.tokensFor(role, "dark")).length > 0, role + " is empty")
        }
    }

    // Key-for-key, in both directions. Theme.qml warns about the first
    // direction at startup, which is a line on stderr in a run nobody is
    // watching; this is the same check as a failure. The second direction — a
    // key in light that dark does not have — nothing checked at all, and it is
    // the more insidious one: the light value is simply never read, so the
    // theme silently falls back to a dark token and looks *almost* right.
    function test_lightMirrorsDarkKeyForKey() {
        for (var i = 0; i < Theme.colorRoles.length; ++i) {
            var role = Theme.colorRoles[i]
            var dark = Object.keys(Theme.tokensFor(role, "dark")).sort()
            var light = Object.keys(Theme.tokensFor(role, "light")).sort()
            compare(light.join(","), dark.join(","), "themelight.js drifted from theme.js in " + role)
        }
    }

    // The grouped properties are what components actually read, and Theme.qml
    // lists every token by hand inside an inline component. A token added to
    // theme.js and not to that list exists in the table, appears on the palette
    // page, and is invisible to `Theme.ink.whatever` — which is the one place
    // it needs to work.
    function test_groupedPropertiesExposeEveryToken() {
        for (var i = 0; i < Theme.colorRoles.length; ++i) {
            var role = Theme.colorRoles[i]
            var inTable = Object.keys(Theme.tokensFor(role, "dark")).sort()
            var exposed = Theme.names(Theme[role]).sort()
            compare(exposed.join(","), inTable.join(","),
                    "Theme." + role + " does not expose the same tokens theme.js declares")
        }
    }

    // A colour string that QML cannot parse is transparent, not an error.
    function test_everyTokenParses() {
        var schemes = ["dark", "light"]
        for (var s = 0; s < schemes.length; ++s) {
            var table = tableFor(schemes[s])
            for (var role in table) {
                var group = table[role]
                for (var token in group) {
                    var value = group[token]
                    verify(typeof value === "string" && value.length > 0,
                           schemes[s] + " " + role + "." + token + " is not a string")
                    verify(value === "transparent" || /^#([0-9a-fA-F]{6}|[0-9a-fA-F]{8})$/.test(value),
                           schemes[s] + " " + role + "." + token + " is \"" + value
                           + "\", which is neither transparent nor a long-form hex colour")
                }
            }
        }
    }

    // ---- the contrast contract --------------------------------------------

    function test_everyTokenHasARule() {
        for (var i = 0; i < Theme.colorRoles.length; ++i) {
            var role = Theme.colorRoles[i]
            var group = Theme.tokensFor(role, "dark")
            for (var token in group) {
                var rule = Theme.contrastRule(role + "." + token)
                verify(rule !== undefined && rule.duty !== undefined,
                       role + "." + token + " has no contrast duty")
                verify(["text", "essential", "incidental"].indexOf(rule.duty) >= 0,
                       role + "." + token + " has duty \"" + rule.duty + "\"")
            }
        }
    }

    // A ground that names a token nothing declares composites against the page
    // instead, silently, and every ratio measured through it is wrong in the
    // direction that hides a failure.
    function test_everyGroundResolves() {
        var table = tableFor("dark")
        for (var i = 0; i < Theme.colorRoles.length; ++i) {
            var role = Theme.colorRoles[i]
            var group = table[role]
            for (var token in group) {
                var rule = Theme.contrastRule(role + "." + token)
                if (rule.on === null || rule.on === undefined)
                    continue
                var parts = rule.on.split(".")
                verify(table[parts[0]] !== undefined && table[parts[0]][parts[1]] !== undefined,
                       role + "." + token + " is measured on \"" + rule.on + "\", which does not exist")
            }
        }
    }

    // A pair must be mutual. `a.pair = b` with `b` naming something else, or
    // nothing, means one stop is scored against the other's ground and the
    // gradient is judged twice by two different rules.
    function test_pairsAreMutual() {
        var table = tableFor("dark")
        for (var i = 0; i < Theme.colorRoles.length; ++i) {
            var role = Theme.colorRoles[i]
            for (var token in table[role]) {
                var path = role + "." + token
                var rule = Theme.contrastRule(path)
                if (rule.pair === undefined)
                    continue
                var back = Theme.contrastRule(rule.pair)
                compare(back.pair, path, path + " pairs with " + rule.pair + ", which pairs back with "
                        + back.pair)
            }
        }
    }

    // The assertion this whole apparatus exists for. Both schemes, every token,
    // against the contract in Theme.qml — and the failure message names the
    // tokens, so a red row is a line in the log rather than a picture somebody
    // has to go and look at.
    function test_noTokenMissesItsContrastFloor_data() {
        return [{ tag: "dark", scheme: "dark" }, { tag: "light", scheme: "light" }]
    }

    function test_noTokenMissesItsContrastFloor(data) {
        var table = tableFor(data.scheme)
        var failures = []

        for (var i = 0; i < Theme.colorRoles.length; ++i) {
            var role = Theme.colorRoles[i]
            for (var token in table[role]) {
                var path = role + "." + token
                var audit = Contrast.audit(table, Theme.contrastRule, Theme.contrastFloor, path)
                if (audit.verdict === "fail")
                    failures.push(path + " " + audit.ratio.toFixed(2) + ":1 (needs "
                                  + audit.floor + ":1 on " + Theme.contrastRule(path).on + ")")
            }
        }

        compare(failures.length, 0,
                data.scheme + " has " + failures.length + " token(s) under their floor:\n  "
                + failures.join("\n  "))
    }

    // ---- severity ----------------------------------------------------------
    //
    // The alert palette is a table keyed by data — `Theme.severity[key]` — so
    // it is not in `colorRoles` and the audit above walks straight past it. It
    // is also the group with the most at stake: a severity `ink` that fails its
    // floor is a tornado warning nobody can read.
    //
    // So it is checked here, by hand, with the same arithmetic. The ground is
    // the COMPOSITED PLATE — the wash over the page — and not the page itself,
    // because that is what the text is actually drawn on. Auditing the ink
    // against `page.bg` would score a colour against a background it never
    // touches, which is the mistake theme.js's contrast contract exists to
    // stop making.

    function test_everySeverityHasEveryToken() {
        for (var scheme in { dark: 0, light: 0 }) {
            var table = Theme.tokensFor("severity", scheme)
            verify(table !== undefined, scheme + " has no severity table")

            for (var i = 0; i < Theme.severityKeys.length; ++i) {
                var key = Theme.severityKeys[i]
                verify(table[key] !== undefined,
                       scheme + " severity table has no '" + key + "'")

                for (var token in { wash: 0, edge: 0, glyph: 0, ink: 0 }) {
                    verify(table[key][token] !== undefined,
                           scheme + " severity." + key + " has no '" + token + "'")
                }
            }
        }
    }

    function test_severityInkAndEdgeClearTheirFloors_data() {
        return [{ tag: "dark", scheme: "dark" }, { tag: "light", scheme: "light" }]
    }

    function test_severityInkAndEdgeClearTheirFloors(data) {
        var severity = Theme.tokensFor("severity", data.scheme)
        var page = Theme.tokensFor("page", data.scheme).bg
        var failures = []

        // Same duties as everything else: `ink` carries words at body size and
        // owes 4.5:1; `edge` and `glyph` are WCAG 1.4.11 graphical objects
        // required to understand the content and owe 3:1.
        var floors = { ink: 4.5, edge: 3.0, glyph: 3.0 }

        for (var i = 0; i < Theme.severityKeys.length; ++i) {
            var key = Theme.severityKeys[i]
            var tones = severity[key]
            var plate = Contrast.compose(tones.wash, page)

            for (var token in floors) {
                var r = Contrast.ratio(Contrast.compose(tones[token], plate), plate)
                if (r < floors[token])
                    failures.push("severity." + key + "." + token + " " + r.toFixed(2)
                                  + ":1 (needs " + floors[token] + ":1 on its own plate)")
            }

            // And the plate has to separate from the page, or the banner is a
            // rectangle of nothing with a coloured rail. 1.2:1 is the same step
            // the surface ladder holds itself to.
            var plateRatio = Contrast.ratio(plate, page)
            if (plateRatio < 1.2)
                failures.push("severity." + key + ".wash " + plateRatio.toFixed(2)
                              + ":1 against the page (needs 1.2:1)")
        }

        compare(failures.length, 0,
                data.scheme + " severity palette has " + failures.length + " failure(s):\n  "
                + failures.join("\n  "))
    }

    // Extreme must be the most present thing on the screen. Checked on the
    // saturated tokens rather than on the wash: in dark, amber on navy is
    // simply brighter than red on navy, so moderate's PLATE sits a hair above
    // extreme's and always will — theme.js records that measurement. The rail,
    // the glyph and the word are what carry the ordering, and they can.
    function test_severityIsDistinguishableGradeToGrade_data() {
        return [{ tag: "dark", scheme: "dark" }, { tag: "light", scheme: "light" }]
    }

    function test_severityIsDistinguishableGradeToGrade(data) {
        var severity = Theme.tokensFor("severity", data.scheme)
        var seen = {}

        for (var i = 0; i < Theme.severityKeys.length; ++i) {
            var key = Theme.severityKeys[i]
            var edge = severity[key].edge.toLowerCase()

            // Two grades sharing an edge colour would make the rail — the one
            // part of the banner readable from across a room — say nothing.
            verify(seen[edge] === undefined,
                   data.scheme + ": severity." + key + " and severity." + seen[edge]
                   + " share the edge colour " + edge)
            seen[edge] = key
        }
    }

    // ---- ramps -------------------------------------------------------------

    function test_everyRampHasBothPartsInBothSchemes() {
        var dark = Theme.rampsFor("dark")
        var light = Theme.rampsFor("light")
        compare(Object.keys(light).sort().join(","), Object.keys(dark).sort().join(","),
                "the two schemes declare different ramps")

        for (var name in dark) {
            for (var part in { fill: 0, line: 0 }) {
                verify(dark[name][part] !== undefined && dark[name][part].length > 0,
                       "dark ramp " + name + "." + part + " is missing or empty")
                verify(light[name][part] !== undefined && light[name][part].length > 0,
                       "light ramp " + name + "." + part + " is missing or empty")
            }
        }
    }

    // Stops run 0 to 1, in order. `ChartMath.sampleRamp` walks the array
    // assuming that and returns the last stop when it falls off the end, so an
    // out-of-order table does not throw — it draws a flat band.
    function test_rampStopsAreOrderedAndSpanTheRange() {
        var schemes = ["dark", "light"]
        for (var s = 0; s < schemes.length; ++s) {
            var ramps = Theme.rampsFor(schemes[s])
            for (var name in ramps) {
                var parts = ["fill", "line"]
                for (var p = 0; p < parts.length; ++p) {
                    var stops = ramps[name][parts[p]]
                    var label = schemes[s] + " " + name + "." + parts[p]
                    compare(stops[0].p, 0.00, label + " does not start at 0")
                    compare(stops[stops.length - 1].p, 1.00, label + " does not end at 1")
                    for (var i = 1; i < stops.length; ++i)
                        verify(stops[i].p > stops[i - 1].p,
                               label + " stop " + i + " is at " + stops[i].p
                               + ", not after " + stops[i - 1].p)
                }
            }
        }
    }

    // The rule docs/10-design-system.md §10.5 states: where an authority
    // publishes the bands, the bands are the palette. Light inverts the
    // lightness of the six continuous ramps and must leave these three exactly
    // as they are, because recolouring the WHO's UV scale makes the app
    // disagree with the source it is quoting.
    function test_categoricalRampsAreIdenticalInBothSchemes() {
        var dark = Theme.rampsFor("dark")
        var light = Theme.rampsFor("light")

        verify(Theme.categoricalRamps.length > 0)
        for (var i = 0; i < Theme.categoricalRamps.length; ++i) {
            var name = Theme.categoricalRamps[i]
            verify(dark[name] !== undefined, name + " is not a ramp")
            compare(JSON.stringify(light[name]), JSON.stringify(dark[name]),
                    "the light theme changed the published band " + name)
        }
    }

    // And the converse, so that the list cannot quietly grow to cover a ramp
    // somebody did not want to author: a continuous ramp that came out
    // identical in both schemes was not inverted, it was copied.
    function test_continuousRampsDifferBetweenSchemes() {
        var dark = Theme.rampsFor("dark")
        var light = Theme.rampsFor("light")

        for (var name in dark) {
            if (Theme.categoricalRamps.indexOf(name) >= 0)
                continue
            verify(JSON.stringify(light[name]) !== JSON.stringify(dark[name]),
                   "continuous ramp " + name + " is identical in both schemes")
        }
    }

    // ---- motion ------------------------------------------------------------

    // Stillness is what makes a capture reproducible and what serves a reader
    // who asked their desktop for less movement. Both want the same thing:
    // every duration at zero, with none missed. A single animation left running
    // is a golden image that fails one run in twenty.
    function test_stillnessZeroesEveryDuration() {
        var was = Theme.stillness
        Theme.stillness = true

        var names = Theme.names(Theme.motion)
        verify(names.length > 0)
        for (var i = 0; i < names.length; ++i) {
            if (names[i] === "easing")
                continue
            compare(Theme.motion[names[i]], 0, "Theme.motion." + names[i] + " still runs when still")
        }

        Theme.stillness = false
        var moving = 0
        for (var j = 0; j < names.length; ++j)
            if (names[j] !== "easing" && Theme.motion[names[j]] > 0)
                moving++
        verify(moving > 0, "no duration is non-zero when stillness is off")

        Theme.stillness = was
    }

    // ---- the scheme switch -------------------------------------------------

    // The grouped objects must keep their identity across a switch. That is the
    // whole reason Theme became a singleton: a JS library value produced no
    // change notification, so `color: Theme.ink.primary` was evaluated once and
    // never again. If a switch replaced the group object, every binding through
    // it would go stale in a different way.
    function test_switchingSchemeKeepsGroupIdentity() {
        var was = Theme.scheme

        Theme.scheme = "dark"
        var groups = []
        for (var i = 0; i < Theme.colorRoles.length; ++i)
            groups.push(Theme[Theme.colorRoles[i]])

        var darkInk = Theme.ink.primary
        Theme.scheme = "light"

        for (var j = 0; j < Theme.colorRoles.length; ++j)
            verify(Theme[Theme.colorRoles[j]] === groups[j],
                   "Theme." + Theme.colorRoles[j] + " was replaced by the scheme switch")

        verify(Theme.ink.primary !== darkInk, "ink.primary did not change with the scheme")
        verify(Theme.isLight)

        Theme.scheme = was
    }

    // ---- the drawing tier ---------------------------------------------------
    //
    // Two facts, and the second is the one the whole idea rests on: a reduced
    // sky must be a PREFIX of the full one. sky.js seeds every star from its own
    // index, so `field(70)` is the first seventy of the same hundred and thirty
    // — which is what makes a reduced sky the same sky with fewer stars rather
    // than a different sky, and what lets a golden image exist per tier.
    function test_theReducedTierIsSmallerInEveryDimension() {
        var was = Theme.perfTier

        Theme.perfTier = "full"
        var full = { f: Theme.perf.starField, b: Theme.perf.starBeacons,
                     c: Theme.perf.constellations }

        Theme.perfTier = "reduced"
        verify(Theme.perf.starField < full.f, "the field should shrink")
        verify(Theme.perf.starBeacons < full.b, "the beacons should shrink")
        verify(Theme.perf.constellations < full.c, "the figures should shrink")
        verify(Theme.perf.constellations >= 1, "one figure still says what they are")

        // An unknown tier is `full` rather than an empty sky. A typo in a flag
        // should not silently produce a page with no stars on it.
        Theme.perfTier = "nonsense"
        compare(Theme.perf.starField, full.f)

        Theme.perfTier = was
    }

    function test_theReducedSkyIsAPrefixOfTheFullOne() {
        var full = Sky.field(130)
        var lean = Sky.field(70)
        compare(lean.length, 70)
        for (var i = 0; i < lean.length; ++i) {
            compare(lean[i].x, full[i].x, "star " + i + " moved between tiers")
            compare(lean[i].y, full[i].y, "star " + i + " moved between tiers")
        }
    }
}
