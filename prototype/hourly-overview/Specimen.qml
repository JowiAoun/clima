// SPDX-License-Identifier: GPL-3.0-or-later
// One instance of one component, built from a file name and a property bag.
//
// The gallery is data-driven — see gallery.js — which means it cannot declare
// its specimens statically. Qt.createComponent + createObject takes both the
// file and the properties as values, so a new entry in the catalogue is a few
// lines of data rather than another QML file.
//
// A component that fails to build renders as a labelled red box rather than as
// nothing. A gallery that silently omits what it cannot load is worse than no
// gallery: it reports health it has not checked.
import QtQuick
import "theme.js" as Theme

Item {
    id: root

    property string source
    property var props: ({})

    // Components with no implicit size of their own — a chart panel, a tab bar —
    // are given a stage to fill. Everything else is left at its natural size,
    // which is part of what the gallery is for.
    property real stageWidth: 0
    property real stageHeight: 0

    property Item instance: null
    property string error: ""

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
    // Coalescing — source, props and the two stage dimensions all change
    // together when the gallery moves to another component. Qt.callLater
    // dedupes by function identity, so four triggers in one pass build once
    // instead of four times.
    onSourceChanged: Qt.callLater(rebuild)
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
        if (source === "")
            return

        var c = Qt.createComponent(source)
        if (c.status === Component.Error) {
            error = c.errorString()
            return
        }

        var o = c.createObject(root, props ? props : {})
        if (o === null) {
            error = "createObject returned null for " + source
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
            color: Theme.color.textPrimary
            font.pixelSize: Theme.type.body
            wrapMode: Text.Wrap
            verticalAlignment: Text.AlignVCenter
        }
    }
}
