// SPDX-FileCopyrightText: 2026 Jowi Aoun
// SPDX-License-Identifier: GPL-3.0-or-later
//
// The three things about the mobile shell that a screenshot cannot check.
//
// Every other assertion about this shell is a picture: the golden images cover
// what the phone, the portrait tablet and the landscape tablet look like. What
// they cannot cover is *identity* and *sequence* — whether the page you are
// looking at is the same object it was a moment ago, and whether a value that
// is pushed rather than bound is still arriving after the thing that pushes it
// has changed twice.
//
// Those are exactly where this shell's known defects live. MobileShell's own
// header records that `Qt.binding()` into a page is quietly broken here, and
// the reason the layout is pushed by a function call is that the binding
// version passed review and then stopped working the first time anybody
// touched a control.
import QtQuick
import QtTest
import Clima

import "qrc:/qt/qml/Clima/mobiletabs.js" as Tabs

TestCase {
    name: "MobileShell"
    when: windowShown

    // Built inside the TestCase and not in a Window of its own, which was the
    // first attempt and does not work: `keyClick` posts to the window the
    // TestCase is in, so a shell parented anywhere else never receives a key
    // and every back-navigation assertion failed with the tab unmoved.
    //
    // `when: windowShown` is what makes that window real. It is also what makes
    // the shell visible, which the layout needs — an item in a view that was
    // never shown lays out but reports `visible: false` all the way down.
    Item {
        id: host
        anchors.fill: parent
    }

    function build(w, h, cls) {
        var c = Qt.createComponent("Clima", "MobileShell")
        verify(c !== null && c.status !== Component.Error,
               c === null ? "no such type" : c.errorString())
        var shell = c.createObject(host, { width: w, height: h, viewportClass: cls })
        verify(shell !== null)
        wait(0)
        return shell
    }

    // ---- back ---------------------------------------------------------------
    //
    // Android's gesture, and Escape on a desktop, which is how it is reachable
    // here. Any tab but the first goes to the first; the first leaves the event
    // alone so the platform can close the app.
    function test_backReturnsToTheFirstTabAndThenGivesUp() {
        var shell = build(390, 844, "mobile")
        shell.forceActiveFocus()

        shell.tab = "monthly"
        keyClick(Qt.Key_Escape)
        compare(shell.tab, Tabs.list[0].id, "back should return to the first tab")

        // Already there: the shell must not swallow it, or a reader on the home
        // screen is holding a gesture that does nothing at all.
        keyClick(Qt.Key_Escape)
        compare(shell.tab, Tabs.list[0].id)

        shell.destroy()
    }

    function test_backClosesASheetBeforeItChangesTab() {
        var shell = build(390, 844, "mobile")
        shell.forceActiveFocus()

        shell.tab = "monthly"
        shell.pickerOpen = true
        keyClick(Qt.Key_Escape)
        compare(shell.pickerOpen, false, "the sheet on top goes first")
        compare(shell.tab, "monthly", "and the tab underneath does not move")

        keyClick(Qt.Key_Escape)
        compare(shell.tab, Tabs.list[0].id)

        shell.destroy()
    }

    // ---- rotation -----------------------------------------------------------
    //
    // The whole of the landscape work rests on this. The nav moves from the
    // bottom to the left, the page loses 76 px of width and gains the height
    // the bar was taking, and the content splits into two columns — and none of
    // that may touch `Loader.source`, because a page rebuilt on rotation
    // re-runs every card's reveal and loses where the reader had scrolled to.
    function test_turningTheDeviceDoesNotRebuildThePage() {
        var shell = build(834, 1112, "tablet")
        var before = shell.currentPage
        verify(before !== null, "a page should be loaded")
        compare(shell.railed, false, "a portrait tablet keeps the bottom bar")

        shell.width = 1112
        shell.height = 834
        wait(0)

        compare(shell.railed, true, "a landscape tablet takes the rail")
        compare(shell.currentPage, before,
                "the page was rebuilt by a rotation — check Loader.source")

        shell.destroy()
    }

    // Changing tab is the one thing that IS allowed to rebuild it, and the
    // assertion above would pass just as well if the loader had stopped working
    // altogether. This is the other half of it.
    function test_changingTabDoesRebuildThePage() {
        var shell = build(390, 844, "mobile")
        var before = shell.currentPage
        shell.tab = "monthly"
        wait(0)
        verify(shell.currentPage !== before, "a tab change loads another page")

        shell.destroy()
    }

    // ---- what the shell pushes down -----------------------------------------
    //
    // Pushed, never bound — see MobileShell's note. A push that stops arriving
    // is invisible: the page keeps whatever it was last told, which is a
    // plausible number, so the failure looks like a layout that is slightly
    // wrong rather than like a mechanism that is dead.
    function test_theLayoutIsPushedIntoThePageOnEveryChange() {
        var shell = build(834, 1112, "tablet")
        var page = shell.currentPage
        verify(page !== null)

        compare(page.viewportClass, "tablet")
        compare(page.columns, 2, "806 px of content is two columns")
        verify(page.bottomInset > 0, "the bottom bar has to be padded for")

        shell.width = 1112
        shell.height = 834
        wait(0)

        compare(page.bottomInset, 0, "a rail takes no height from the page")
        compare(page.columns, 2)

        // Narrow enough that a second column stops being worth it. It is below
        // the rail's threshold too, and it has to be: a rail needs 900 px, and
        // 900 minus the rail minus the margins is still 796 — so a shell that
        // has a rail always has two columns, and there is no width at which
        // this shell shows one column beside a rail.
        shell.width = 700
        shell.height = 600
        wait(0)
        compare(shell.railed, false)
        compare(page.columns, 1, "672 px of content is one column")

        shell.destroy()
    }

    // A phone is one column at any width, because `--viewport mobile --size
    // 900x844` is a request to review the phone rather than to be handed a
    // tablet.
    function test_aForcedPhoneStaysOneColumn() {
        var shell = build(900, 844, "mobile")
        compare(shell.currentPage.columns, 1)
        compare(shell.railed, false, "and it keeps the bottom bar")
        shell.destroy()
    }
}
