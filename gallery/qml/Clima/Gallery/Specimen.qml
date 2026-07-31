// SPDX-FileCopyrightText: 2026 Jowi Aoun
// SPDX-License-Identifier: GPL-3.0-or-later
// One instance of one component, built from a file name and a property bag.
//
// The gallery is data-driven — see gallery.js — which means it cannot declare
// its specimens statically. Qt.createComponent + createObject takes both the
// component and the properties as values, so a new entry in the catalogue is a
// few lines of data rather than another QML file.
//
// A component that fails to build renders as a labelled red box rather than as
// nothing. A gallery that silently omits what it cannot load is worse than no
// gallery: it reports health it has not checked.
//
// ---- how a bare file name finds its component -------------------------------
//
// `file` is what the catalogue writes down — "DetailUvCard.qml" — and until
// this file moved out of the Clima module that was also a working relative URL,
// because the specimen and its subject sat in the same directory. They do not
// any more: this is Clima.Gallery and DetailUvCard.qml is in Clima, and
// `Qt.createComponent("DetailUvCard.qml")` from here resolves against
// `qrc:/qt/qml/Clima/Gallery/` and finds nothing. Every one of the sixty
// entries in the catalogue would have come up as a red box reading "No such
// file or directory", which is at least the failure this file was built to
// show, but it is still sixty red boxes.
//
// The fix is not a longer path. Qt.createComponent's two-argument form takes a
// module URI and a *type* name and resolves it the way `import Clima` does —
// through the engine's import machinery, which knows where the module actually
// is whether that is a compiled-in resource, a directory on QML_IMPORT_PATH or
// a plugin. A relative `../DetailUvCard.qml` would also work today and would go
// on working right up until someone set RESOURCE_PREFIX.
//
// Which leaves one conversion, `"DetailUvCard.qml"` to `DetailUvCard`, and it
// is a sound one rather than string surgery that happens to work: a type
// declared by a QML file *is* named for that file, with the extension off. That
// is the rule qmldir is generated from. A name that is not in the module comes
// back as a component in the Error state carrying `Module "Clima" contains no
// type named "Nope"`, which lands in the same red box as before.
import QtQuick
import Clima

Item {
    id: root

    // The catalogue's file name, e.g. "DetailUvCard.qml". Not a URL — see
    // above — which is why this is `file` and not `source`: a QML property
    // called `source` is a URL everywhere else in Qt, and this one has not been
    // one since the gallery moved out of the module it stages.
    property string file
    property var props: ({})

    // Components with no implicit size of their own — a chart panel, a tab bar —
    // are given a stage to fill. Everything else is left at its natural size,
    // which is part of what the gallery is for.
    property real stageWidth: 0
    property real stageHeight: 0

    property Item instance: null
    property string error: ""

    // The red box is for a person looking at the stage. This is for the run with
    // nobody looking at it: stepping `--walk 0 … --walk 59 --grab frame.png`
    // over the catalogue is the cheap way to ask CI whether all sixty entries
    // still instantiate, and a failure reported only in pixels is a failure CI
    // cannot read. So both, from one place.
    onErrorChanged: if (error !== "") console.warn("specimen:", file, "—", error)

    // Everything this specimen has ever created, so nothing can be orphaned by
    // a rebuild that lost track of it — which is exactly what happened: two
    // cards ended up drawn on top of each other in one box.
    property var built: []

    // The failure box needs room for the message it exists to carry; at the
    // placeholder size it would clip the very thing you need to read.
    implicitWidth: error !== "" ? Math.max(stageWidth, 380)
                 : stageWidth > 0 ? stageWidth
                                  : (instance ? Math.max(instance.width, 24) : 120)
    implicitHeight: error !== "" ? Math.max(stageHeight, 130)
                  : stageHeight > 0 ? stageHeight
                                    : (instance ? Math.max(instance.height, 24) : 40)
    width: implicitWidth
    height: implicitHeight

    // Scheduled, never called directly. Two reasons, both real:
    //
    // Re-entrancy — `createObject(root, props)` reads `props`, and reading a
    // binding that has not been evaluated yet evaluates it and emits
    // propsChanged, which lands back in rebuild() while the first one is still
    // inside createObject. Both then finished, and neither had seen the other's
    // instance to destroy it.
    //
    // Coalescing — file, props and the two stage dimensions all change together
    // when the gallery moves to another component. Qt.callLater dedupes by
    // function identity, so four triggers in one pass build once instead of
    // four times.
    // Bumped from outside to force a rebuild without changing anything about
    // what is being built. A detail card's data never changes while the app
    // runs, so the only motion it has is whatever it does on mount — and the
    // only way to watch that twice is to mount it again.
    property int remountToken: 0
    onRemountTokenChanged: Qt.callLater(rebuild)

    onFileChanged: Qt.callLater(rebuild)
    onPropsChanged: Qt.callLater(rebuild)
    onStageWidthChanged: Qt.callLater(rebuild)
    onStageHeightChanged: Qt.callLater(rebuild)
    Component.onCompleted: Qt.callLater(rebuild)

    // A deferred rebuild outlives its specimen: moving from a component with
    // two variants to one with a single variant makes the Repeater drop a
    // delegate, and the callLater still fires — on an object whose QML context
    // has gone, where Qt.createComponent fails with "Cannot create a component
    // in an invalid context". Harmless to the render and noisy in the log, and
    // it is the kind of message that trains you to ignore the log.
    property bool alive: true
    Component.onDestruction: alive = false

    function rebuild() {
        if (!alive)
            return
        buildNow()
    }

    function buildNow() {
        for (var i = 0; i < built.length; ++i)
            if (built[i])
                built[i].destroy()
        built = []
        instance = null

        error = ""
        if (file === "")
            return

        // The .qml comes off because Qt.createComponent's module form wants a
        // type name; see the note at the top of this file for why it is the
        // module form at all.
        var typeName = file.replace(/\.qml$/, "")

        // Null rather than a component in the Error state, and only for an
        // argument QML would not accept at all — an empty type name is the one
        // way to get here. Checked because `c.status` on a null would throw out
        // of a callLater, where nothing is left to catch it and the specimen
        // renders as neither a component nor an error.
        var c = Qt.createComponent("Clima", typeName)
        if (c === null) {
            error = "Clima." + typeName + " is not a component name"
            return
        }
        if (c.status === Component.Error) {
            error = c.errorString()
            return
        }

        var o = c.createObject(root, props ? props : {})
        if (o === null) {
            error = "createObject returned null for " + file
            return
        }
        built.push(o)

        // Only override the size the component chose for itself when the
        // catalogue says it has none worth keeping.
        if (stageWidth > 0)
            o.width = stageWidth
        if (stageHeight > 0)
            o.height = stageHeight

        instance = o
    }

    Rectangle {
        anchors.fill: parent
        visible: root.error !== ""
        radius: Theme.metric.controlRadius
        color: "#33ff5c4a"
        border.width: 1
        border.color: "#88ff5c4a"

        Text {
            anchors.fill: parent
            anchors.margins: 10
            text: root.error
            color: Theme.ink.primary
            font.pixelSize: Theme.type.body
            wrapMode: Text.Wrap
            verticalAlignment: Text.AlignVCenter
        }
    }
}
