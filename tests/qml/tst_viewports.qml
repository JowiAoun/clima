// SPDX-FileCopyrightText: 2026 Jowi Aoun
// SPDX-License-Identifier: GPL-3.0-or-later
//
// Which shell a window width gets.
//
// One function decides whether this application draws a phone or a desktop, and
// it is four lines long. It is tested at the *boundaries* and one pixel either
// side of them, because an off-by-one here is not a subtle defect: it is the
// wrong entire layout, and it appears only on the handful of window widths
// nobody drags to on purpose.
import QtQuick
import QtTest
import Clima

TestCase {
    name: "Viewports"

    // 599/600 and 1023/1024 are the two places the answer changes. The pairs
    // are written out rather than generated from minWidth, so that a test
    // reading its expectations out of the same table it is checking cannot pass
    // by agreeing with itself.
    function test_classAtBoundaries_data() {
        return [
            { tag: "zero",          w: 0,    cls: "mobile"  },
            { tag: "small phone",   w: 320,  cls: "mobile"  },
            { tag: "preset phone",  w: 390,  cls: "mobile"  },
            { tag: "below tablet",  w: 599,  cls: "mobile"  },
            { tag: "tablet floor",  w: 600,  cls: "tablet"  },
            { tag: "preset tablet", w: 834,  cls: "tablet"  },
            { tag: "below desktop", w: 1023, cls: "tablet"  },
            { tag: "desktop floor", w: 1024, cls: "desktop" },
            { tag: "preset desktop",w: 1340, cls: "desktop" },
            { tag: "very wide",     w: 3840, cls: "desktop" }
        ]
    }

    function test_classAtBoundaries(data) {
        compare(Viewports.classOf(data.w), data.cls,
                data.w + "px should be " + data.cls)
    }

    // The tablet runs the phone's shell. This is the assertion that stops
    // somebody "fixing" the tablet by giving it the desktop page — which is a
    // reasonable-sounding change that would drop the bottom nav on an 834 px
    // screen and leave no way to reach four of the five sections.
    function test_tabletRunsTheMobileShell() {
        verify(Viewports.usesMobileShell("mobile"))
        verify(Viewports.usesMobileShell("tablet"))
        verify(!Viewports.usesMobileShell("desktop"))
    }

    // An unknown class must not silently become a phone. It is a typo, and the
    // shell it should get is the one that can show everything.
    function test_unknownClassIsNotMobile() {
        verify(!Viewports.usesMobileShell("phablet"))
    }

    // Every preset resolves, and every preset's own width classifies as the
    // preset it belongs to. The gallery frames a specimen at exactly the width
    // the app would switch at, so a preset whose width lands in a different
    // class than its id claims would review a phone card at desktop width.
    function test_presetsAgreeWithTheirOwnWidths() {
        var ids = Viewports.ids()
        verify(ids.length > 0)

        for (var i = 0; i < ids.length; ++i) {
            var preset = Viewports.byId(ids[i])
            verify(preset !== null, ids[i] + " should resolve")
            compare(Viewports.classOf(preset.w), preset.id,
                    preset.id + " opens at " + preset.w + "px, which classifies as "
                    + Viewports.classOf(preset.w))
        }
    }

    function test_unknownIdIsNull() {
        compare(Viewports.byId("watch"), null)
    }

    // Narrow to wide. `classOf` returns the widest match, so a presets array
    // that fell out of order would still classify correctly and would put the
    // gallery's rail in a nonsensical sequence — the kind of defect that is
    // obvious in a picture and invisible in a unit test unless it is stated.
    function test_presetsAreOrderedNarrowToWide() {
        var ids = Viewports.ids()
        for (var i = 1; i < ids.length; ++i) {
            var previous = Viewports.byId(ids[i - 1])
            var current = Viewports.byId(ids[i])
            verify(current.w > previous.w,
                   current.id + " (" + current.w + ") should be wider than "
                   + previous.id + " (" + previous.w + ")")
        }
    }
}
