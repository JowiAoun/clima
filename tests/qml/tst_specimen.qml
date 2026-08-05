// SPDX-FileCopyrightText: 2026 Jowi Aoun
// SPDX-License-Identifier: GPL-3.0-or-later
//
// Every component in the tree, built once, in every state the catalogue knows
// about.
//
// This is the broadest test in the repository and very nearly the cheapest,
// because the hard part already exists: gallery.js is a hand-maintained list of
// every component with the properties it needs to stand up, written so a person
// could review them, and it works just as well for a machine that only wants to
// know whether they stand up at all.
//
// What it catches that nothing else does:
//
//   * a QML file that does not compile — trivially, but nothing checked it, and
//     a component nobody has opened since a refactor is exactly the one that
//     stopped compiling
//   * a binding referring to a token, function or property that no longer
//     exists. `Theme.ink.typo` is `undefined`, which is a transparent colour
//     rather than an error, so the component draws nothing and reports success.
//     The warning on stderr is the only evidence, and QmlWarnings turns that
//     stream into an assertion
//   * a component that only works in the one state some screen happens to use
//     it in. The catalogue's `variants` are the states no current screen uses,
//     which is where this kind of rot lives
//
// It deliberately does not check what anything *looks* like. That is the golden
// images' job, and keeping the two apart means this one stays fast and stays
// honest about what a green run proves.
import QtQuick
import QtTest
import Clima
import Clima.Gallery
// QmlWarnings, which is the whole reason this test can see the defects it is
// written for. Its own module, built in tests/, so that a type that exists to
// let a test assert about stderr is not importable from the app.
import Clima.Test

import "qrc:/qt/qml/Clima/Gallery/gallery.js" as Catalogue

TestCase {
    name: "Specimen"

    // Somewhere for the instances to live. A component created with no parent
    // is not in a window, so nothing lays it out and a good half of the
    // bindings under test never evaluate — which would make this a compile
    // check wearing a smoke test's name.
    Item {
        id: stage
        width: 1340
        height: 900
    }

    // The catalogue, flattened to one row per (component, variant). A component
    // with no declared variants still gets one row, so both go down the same
    // path — the same rule the gallery's own repeater follows.
    function specimens() {
        var out = []
        for (var g = 0; g < Catalogue.groups.length; ++g) {
            var group = Catalogue.groups[g]
            for (var i = 0; i < group.items.length; ++i) {
                var item = group.items[i]
                if (!item.file)
                    continue // a generated page: palette, ramps, type
                var variants = item.variants ? item.variants : [{ label: "", props: {} }]
                for (var v = 0; v < variants.length; ++v) {
                    out.push({
                        tag: item.name + (variants[v].label ? " · " + variants[v].label : ""),
                        file: item.file,
                        props: variants[v].props ? variants[v].props : ({}),
                        stage: item.stage ? item.stage : null
                    })
                }
            }
        }
        return out
    }

    function test_catalogueIsNotEmpty() {
        var all = specimens()
        verify(all.length > 20, "only " + all.length + " specimens — is the catalogue loading?")
    }

    function test_everySpecimenBuilds_data() {
        return specimens()
    }

    function test_everySpecimenBuilds(data) {
        // Cleared immediately before, so the count that follows belongs to this
        // component and not to whatever the previous row left behind.
        QmlWarnings.clear()

        var typeName = data.file.replace(/\.qml$/, "")
        var component = Qt.createComponent("Clima", typeName)

        verify(component !== null, "Clima." + typeName + " is not a component name")
        verify(component.status !== Component.Error,
               data.file + " does not compile:\n" + component.errorString())

        var instance = component.createObject(stage, data.props)
        verify(instance !== null, "createObject returned null for " + data.file)

        // The catalogue's stage size stands in for a host that is not here.
        // Applied the same way Specimen.qml applies it — only when the
        // catalogue says the component has no size worth keeping — because a
        // width is what makes a layout actually run.
        if (data.stage) {
            if (data.stage.w > 0)
                instance.width = data.stage.w
            if (data.stage.h > 0)
                instance.height = data.stage.h
        }

        // Let bindings settle and any Component.onCompleted work run. The
        // 469-warning bug was exactly this: bindings that evaluated before
        // their data arrived, which a test that asserted immediately after
        // createObject would have walked straight past.
        wait(0)

        compare(QmlWarnings.count, 0,
                data.file + " built with warnings — " + QmlWarnings.summary())

        instance.destroy()
    }

    // The catalogue names a file; the module has to have that type. This is the
    // check that catches a component renamed in app/qml and updated everywhere
    // except in the list of what exists.
    function test_everyCatalogueFileNamesARealType() {
        var missing = []
        var all = specimens()
        for (var i = 0; i < all.length; ++i) {
            var typeName = all[i].file.replace(/\.qml$/, "")
            var component = Qt.createComponent("Clima", typeName)
            if (component === null || component.status === Component.Error)
                missing.push(all[i].file)
        }
        compare(missing.length, 0, "the catalogue names types that do not exist: " + missing.join(", "))
    }
}
