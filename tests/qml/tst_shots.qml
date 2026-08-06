// SPDX-FileCopyrightText: 2026 Jowi Aoun
// SPDX-License-Identifier: GPL-3.0-or-later
//
// The shot catalogue, and the copy of it that lives in C++.
//
// `--shot` has to reject a bad id before any QML has loaded, so GalleryOptions
// carries its own list — C++ cannot read a `.pragma library`. That is a second
// copy of shots.js, and a second copy with nothing checking it is a flag that
// quietly refuses a sheet somebody added, or offers one that no longer exists.
// The symptom in both directions is "--shot foo says foo is not a shot", which
// sends you to look at the wrong file.
//
// So this is the thing that holds them together. It is deliberately strict
// about order as well as membership: --help prints the C++ list, and a reader
// comparing it against shots.js should not have to sort either one.
//
// It also builds every sheet, which is the part that actually catches things.
// A sheet is three shells, a Row, a Repeater and a size computed from
// Viewports — a shape with several ways to come out empty, and an empty sheet
// renders as a correctly sized window full of sky.
import QtQuick
import QtTest
import Clima
import Clima.Gallery
import Clima.Test

// The same route tst_specimen takes to gallery.js. A module's JS lives in the
// resource tree at the URI's path, and a test outside the module cannot reach
// it any other way.
import "qrc:/qt/qml/Clima/Gallery/shots.js" as Shots

TestCase {
    id: testCase
    name: "Shots"
    when: windowShown
    width: 400
    height: 300

    Component {
        id: sheetComponent
        ShotSheet { }
    }

    function test_theCppListMatchesTheCatalogue() {
        var fromJs = Shots.ids()
        var fromCpp = GalleryOptions.shotIds

        verify(fromJs.length > 0, "shots.js declares no sheets at all")
        compare(fromCpp.length, fromJs.length,
                "GalleryOptions::shotIds() and shots.js disagree about how many "
                + "sheets there are: C++ [" + fromCpp.join(", ") + "] vs JS ["
                + fromJs.join(", ") + "]")

        for (var i = 0; i < fromJs.length; ++i)
            compare(fromCpp[i], fromJs[i], "sheet " + i + " differs")
    }

    function test_everySheetBuilds_data() {
        var rows = []
        var ids = Shots.ids()
        for (var i = 0; i < ids.length; ++i)
            rows.push({ tag: ids[i], shot: ids[i] })
        return rows
    }

    function test_everySheetBuilds(data) {
        QmlWarnings.clear()

        var sheet = sheetComponent.createObject(testCase, { shot: data.shot })
        verify(sheet !== null, "ShotSheet would not instantiate for " + data.shot)

        // A sheet with no devices in it is the failure this is really for: it
        // produces a window of the right size containing nothing but sky, which
        // looks like a rendering problem rather than a catalogue one.
        verify(sheet.devices.length > 0, data.shot + " names no devices")

        // Every id in the sheet has to be a preset Viewports knows, or the
        // frame silently falls back to 390x844 and a "desktop" shot comes out
        // phone-shaped.
        for (var i = 0; i < sheet.devices.length; ++i) {
            verify(Viewports.byId(sheet.devices[i]) !== null,
                   data.shot + " names \"" + sheet.devices[i]
                   + "\", which is not a viewport preset")
        }

        // The window binds its size to these. Zero would be a window collapsed
        // to nothing, which `--grab` writes out as a 1x1 PNG without complaint.
        verify(sheet.implicitWidth > 0, data.shot + " has no width")
        verify(sheet.implicitHeight > 0, data.shot + " has no height")

        // The sheet has to be at least as tall as its tallest device, or the
        // bottom-alignment arithmetic has gone wrong and a frame is clipped.
        verify(sheet.implicitHeight >= sheet.rowHeight,
               data.shot + " is shorter than the devices in it")

        compare(QmlWarnings.count, 0,
                data.shot + " built with warnings — " + QmlWarnings.summary())

        sheet.destroy()
    }
}
