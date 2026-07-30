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

    implicitWidth: stageWidth > 0 ? stageWidth
                                  : (instance ? Math.max(instance.width, 24) : 120)
    implicitHeight: stageHeight > 0 ? stageHeight
                                    : (instance ? Math.max(instance.height, 24) : 40)
    width: implicitWidth
    height: implicitHeight

    onSourceChanged: rebuild()
    onPropsChanged: rebuild()
    Component.onCompleted: rebuild()

    function rebuild() {
        if (instance) {
            instance.destroy()
            instance = null
        }
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
