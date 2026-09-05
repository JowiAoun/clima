// SPDX-FileCopyrightText: 2026 Jowi Aoun
// SPDX-License-Identifier: GPL-3.0-or-later
//
// The window tells the alert poll what it is doing.
//
// AlertsData's schedule — three minutes focused, ten idle, stopped when
// hidden — was tested from the day it landed, through setWindowState(). What
// nothing tested was whether anything CALLED setWindowState(), and nothing
// did: the app polled every three minutes from launch to quit, minimised or
// not, and tst_alertsdata could not see it because it drives the view model
// directly. This file drives the window instead.
//
// The window is Main.qml itself, not a stand-in. The wiring under test is
// two handlers and one line in Component.onCompleted, and a test that built a
// Window of its own and called Alerts from it would be testing the test.
import QtQuick
import QtQuick.Window
import QtTest
import Clima
import Clima.Test

TestCase {
    id: testCase
    name: "AlertPoll"
    when: windowShown

    Component {
        id: mainComponent
        Main { }
    }

    function cleanup() {
        // Leave the singleton the way tst_alertsdata's init() would: exposed
        // and focused, so a file that runs after this one starts from the
        // documented defaults rather than from whatever the last case did.
        Alerts.setWindowState(true, true)
    }

    function test_theWindowPushesItsStateAtStartup() {
        // The singleton is put somewhere the window has to move it FROM, which
        // is the whole of this case. AlertsData's own defaults are visible and
        // focused, so asserting "an open window polls" against a fresh
        // singleton asserts nothing at all: it passes with the
        // Component.onCompleted line — the one caller this file exists to
        // protect — deleted outright. That was the first version of this test,
        // and it is the same class of bug as the one the branch started from.
        Alerts.setWindowState(false, false)
        compare(Alerts.pollIntervalMs(), 0, "the singleton did not start from stopped")

        var win = createTemporaryObject(mainComponent, testCase)
        verify(win !== null, "Main did not instantiate")

        // A window that has just opened is exposed. Whether it is *active* is
        // the platform's decision and offscreen makes it one way on one Qt and
        // the other on the next, so the assertion is about visibility alone:
        // exposed means polling, at one of the two visible rates.
        var interval = Alerts.pollIntervalMs()
        verify(interval === 3 * 60 * 1000 || interval === 10 * 60 * 1000,
               "the window did not push its state at startup — the poll is at "
               + interval + ", where an exposed window should be at 3 or 10 minutes")
    }

    function test_hidingTheWindowStopsThePollAndShowingItResumes() {
        var win = createTemporaryObject(mainComponent, testCase)
        verify(win !== null)

        // `visible`, not `visibility`: Main.qml binds the former, and a window
        // with both assigned warns about the conflict on every change.
        win.visible = false
        compare(Alerts.pollIntervalMs(), 0,
                "a hidden window must stop polling — this is the line the "
                + "bandwidth arithmetic in alertsdata.h rests on")

        win.visible = true
        verify(Alerts.pollIntervalMs() > 0, "showing the window did not resume the poll")
    }

    function test_minimisedCountsAsHidden() {
        var win = createTemporaryObject(mainComponent, testCase)
        verify(win !== null)

        // Offscreen may not honour a request to minimise, in which case the
        // property never changes and there is nothing to assert about. Skip
        // rather than pass: a test that passed because the platform ignored
        // it has said nothing.
        win.showMinimized()
        if (win.visibility !== Window.Minimized)
            skip("this platform does not minimise a window on request")

        compare(Alerts.pollIntervalMs(), 0,
                "a window in the dock is a window nobody is looking at")
    }
}
