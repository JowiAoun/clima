// SPDX-FileCopyrightText: 2026 Jowi Aoun
// SPDX-License-Identifier: GPL-3.0-or-later
//
// Every tappable area on every screen a finger reaches, measured against
// Theme.metric.hitMin.
//
// The gallery's overlay is how a person reviews this — HitTargets.qml, the
// "Touch targets" toggle in the rail — and it found four defects the first time
// it was switched on. This is the same scan with nobody looking at it, which is
// what stops the fifth from arriving: a target is small because somebody sized
// a control to its mark instead of to a fingertip, and that is a mistake that
// looks completely correct in a screenshot.
//
// ---- what is in scope ----------------------------------------------------------
//
// The catalogue's mobile and alert groups, driven from gallery.js so that a
// screen added to the app is a screen added to this test with no second list to
// remember. The desktop groups are deliberately out: `usesMobileShell` is false
// there, the input device is a pointer whose contact patch is one pixel, and a
// 17 px pager chevron on a 1340 px page is not the same defect as a 17 px link
// on a phone. If the desktop page ever runs on a touch screen — a tablet in
// landscape does not, it runs this shell — that decision is worth revisiting.
//
// ---- what a failure looks like -------------------------------------------------
//
// The message names the component, the kind of handler, and the measured size,
// because "MobileCard · TapHandler 62×17" is a defect somebody can go and fix
// and "a target was too small" is not.
import QtQuick
import QtQuick.Window
import QtTest
import Clima
import Clima.Gallery

import "qrc:/qt/qml/Clima/Gallery/gallery.js" as Catalogue

TestCase {
    name: "HitTargets"

    // A phone, because the phone is the narrowest thing that runs these and a
    // target that clears the floor at 390 px clears it at 834.
    readonly property int stageWidth: 390
    readonly property int stageHeight: 844

    // What the mobile shell would hand a card at this width: the frame minus
    // the page margin on both sides. Measuring a card at the full device width
    // is measuring a card 28 px wider than it is ever drawn.
    readonly property int cardWidth: stageWidth - Theme.metric.mobileMargin * 2

    readonly property var groupsUnderTest: ["Mobile screens", "Mobile parts", "Alerts"]

    // A real window, shown, and that is not incidental. `Item.visible` is
    // effective visibility: an item inside a QQuickView that was never shown
    // reports false, and so does every one of its descendants — so the scan's
    // "skip what is hidden" rule would skip an entire screen and report a phone
    // with no controls on it. Under the offscreen platform this window costs
    // nothing and makes `visible` mean here what it means in the app.
    //
    // It also makes the hidden things genuinely hidden, which is the other half:
    // a card with no link really does have no link target, and the nav rail's
    // five cells really are absent while the bar is the one being drawn.
    Window {
        id: host
        width: 390
        height: 844
        visible: true

        Item {
            id: stage
            anchors.fill: parent

            HitTargets {
                id: scanner
                anchors.fill: parent
                visible: false      // no poll timer; this test scans on demand
            }
        }
    }

    function specimens() {
        var out = []
        for (var g = 0; g < Catalogue.groups.length; ++g) {
            var group = Catalogue.groups[g]
            if (groupsUnderTest.indexOf(group.name) < 0)
                continue
            for (var i = 0; i < group.items.length; ++i) {
                var item = group.items[i]
                if (!item.file)
                    continue
                var variants = item.variants ? item.variants : [{ label: "", props: {} }]
                for (var v = 0; v < variants.length; ++v) {
                    out.push({
                        tag: item.name + (variants[v].label ? " · " + variants[v].label : ""),
                        file: item.file,
                        props: variants[v].props ? variants[v].props : ({}),
                        fills: item.fills === true,
                        stage: item.stage ? item.stage : null
                    })
                }
            }
        }
        return out
    }

    function test_thereIsSomethingToMeasure() {
        var all = specimens()
        verify(all.length > 10,
               "only " + all.length + " mobile specimens — is the catalogue loading?")
    }

    // The scan itself, pinned against a component whose target count is a fact
    // about the design rather than an accident of layout: the nav has five
    // destinations and each one is a whole cell.
    //
    // Without this, a Qt release that renamed `gesturePolicy` or `containsMouse`
    // would turn every assertion below into a scan that found nothing and passed
    // — which is the exact way the first version of HitTargets failed.
    function test_theScannerFindsHandlers() {
        var nav = Qt.createComponent("Clima", "ShellNav").createObject(stage, {})
        verify(nav !== null)
        nav.width = stageWidth
        wait(0)

        scanner.subject = nav
        scanner.rescan()
        compare(scanner.targets.length, 5,
                "expected one target per destination, got " + scanner.targets.length)
        compare(scanner.targets[0].kind, "TapHandler")

        scanner.subject = null
        nav.destroy()
    }

    function test_everyTargetClearsTheFloor_data() {
        return specimens()
    }

    function test_everyTargetClearsTheFloor(data) {
        var typeName = data.file.replace(/\.qml$/, "")
        var component = Qt.createComponent("Clima", typeName)
        verify(component !== null && component.status !== Component.Error,
               data.file + " does not build")

        var instance = component.createObject(stage, data.props)
        verify(instance !== null, "createObject returned null for " + data.file)

        if (data.fills) {
            instance.width = stageWidth
            instance.height = stageHeight
        } else if (data.stage) {
            if (data.stage.w > 0)
                instance.width = Math.min(data.stage.w, cardWidth)
            if (data.stage.h > 0)
                instance.height = data.stage.h
        }

        // Bindings, Component.onCompleted, and a Flickable clamping its content.
        // A scan run before any of that measures a layout that has not happened.
        wait(0)

        scanner.subject = instance
        scanner.rescan()

        var bad = scanner.failures()
        var report = []
        for (var i = 0; i < bad.length; ++i)
            report.push(bad[i].owner + " " + bad[i].kind + " " + bad[i].label)

        scanner.subject = null
        instance.destroy()

        compare(bad.length, 0,
                data.tag + " has " + bad.length + " target(s) under "
                + Theme.metric.hitMin + " px: " + report.join(", "))
    }
}
