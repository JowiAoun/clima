// SPDX-License-Identifier: GPL-3.0-or-later
// The component gallery: every component in the prototype, on one screen, on
// the gradient it is actually composited over.
//
// It exists because almost every defect found in this prototype so far was
// invisible in the code and obvious in a render — and because a component is
// easiest to get wrong in the states no current screen happens to use. The
// catalogue is gallery.js; this file is only the browser around it.
//
//   ./run.sh --gallery              open it
//   ./run.sh --gallery uv           open it on a particular component
//   ./run.sh --grab g.png --gallery --size 1500x950
import QtQuick
import "theme.js" as Theme
import "gallery.js" as Catalogue

Item {
    id: root

    focus: true

    // Pre-selects an entry by name or file, so `--gallery moon` lands on the
    // moon card instead of on whatever happens to be first.
    property string pick: ""

    readonly property real railWidth: 232

    // The catalogue, filtered. Groups that empty out drop away with their
    // heading rather than leaving a bare label over nothing.
    readonly property var view: {
        var q = filterField.text.toLowerCase()
        var out = []
        for (var g = 0; g < Catalogue.groups.length; ++g) {
            var grp = Catalogue.groups[g]
            var kept = []
            for (var i = 0; i < grp.items.length; ++i) {
                var it = grp.items[i]
                if (q === ""
                        || it.name.toLowerCase().indexOf(q) >= 0
                        || (it.file || "").toLowerCase().indexOf(q) >= 0
                        || grp.name.toLowerCase().indexOf(q) >= 0)
                    kept.push(it)
            }
            if (kept.length > 0)
                out.push({ name: grp.name, items: kept })
        }
        return out
    }

    // The same entries in one list, which is what arrow keys move through.
    readonly property var flat: {
        var out = []
        for (var g = 0; g < view.length; ++g)
            for (var i = 0; i < view[g].items.length; ++i)
                out.push(view[g].items[i])
        return out
    }

    property int cursor: 0
    readonly property var current: flat.length > 0
        ? flat[Math.min(cursor, flat.length - 1)] : null

    // Where the selected row sits inside the rail, reported by the row itself.
    // Routed through a property rather than called directly because the row
    // knows its position before the rail knows its own content height, and
    // scrolling to a row in a list that still measures zero clamps to the top.
    property real currentRowY: -1
    onCurrentRowYChanged: if (currentRowY >= 0) railScroll.ensureVisible(currentRowY, 28)

    readonly property int total: {
        var n = 0
        for (var g = 0; g < Catalogue.groups.length; ++g)
            n += Catalogue.groups[g].items.length
        return n
    }

    // Selection is keyed on the name, not on object identity. A Repeater over
    // a JS array hands its delegate a wrapper around the entry rather than the
    // entry itself, so `modelData === current` is false even for the row that
    // is selected — which silently cost the rail both its highlight and its
    // scroll-to-selection. Item names are unique across the catalogue.
    readonly property string currentName: current ? current.name : ""

    // Two component entries in a row share one sourceComponent, so the
    // Loader does not reload and its onLoaded reset never fires. Scroll to
    // the bottom of the hourly chart, arrow on to the droplet, and the pane
    // is still scrolled past everything the droplet has.
    onCurrentNameChanged: { pane.contentX = 0; pane.contentY = 0 }

    // One shared fallback rather than a fresh `[{...}]` per evaluation, so a
    // component with no declared variants keeps the *same* model object as the
    // last one did and the Repeater reuses its delegate instead of destroying
    // and rebuilding a specimen on every keystroke.
    readonly property var singleVariant: [{ label: "", props: ({}) }]
    readonly property var currentVariants: current
        ? (current.variants ? current.variants : singleVariant) : []

    function select(name) {
        for (var i = 0; i < flat.length; ++i)
            if (flat[i].name === name) { cursor = i; return }
    }

    function step(d) {
        if (flat.length === 0)
            return
        cursor = (Math.min(cursor, flat.length - 1) + d + flat.length) % flat.length
    }

    // Both, deliberately: the pick can arrive with the object or after it,
    // depending on whether the Loader was activated before it was assigned.
    Component.onCompleted: applyPick()
    onPickChanged: applyPick()

    // Applied once per value: it is invoked from both Component.onCompleted and
    // onPickChanged, since the pick can arrive before or after the object, and
    // without this a name that matches nothing warns twice.
    property string appliedPick: ""

    function applyPick() {
        if (pick === "" || pick === appliedPick)
            return
        appliedPick = pick
        var q = pick.toLowerCase()
        for (var i = 0; i < flat.length; ++i) {
            var it = flat[i]
            if (it.name.toLowerCase().indexOf(q) >= 0
                    || (it.file || "").toLowerCase().indexOf(q) >= 0) {
                cursor = i
                return
            }
        }
        console.warn("gallery: no component matching", pick)
    }

    Keys.onUpPressed: root.step(-1)
    Keys.onDownPressed: root.step(1)

    // ---- the rail --------------------------------------------------------

    Rectangle {
        id: rail
        width: root.railWidth
        height: parent.height
        color: Theme.color.surfaceRecede

        Text {
            id: railTitle
            x: 16; y: 16
            text: qsTr("Components")
            color: Theme.color.textPrimary
            font.pixelSize: Theme.type.cardTitle
            font.bold: true
        }

        Text {
            anchors.left: railTitle.right
            anchors.leftMargin: 8
            anchors.baseline: railTitle.baseline
            text: root.flat.length === root.total
                  ? root.total
                  : root.flat.length + "/" + root.total
            color: Theme.color.textDim
            font.pixelSize: Theme.type.body
        }

        Rectangle {
            id: filterBox
            x: 12
            y: railTitle.y + railTitle.height + 12
            width: rail.width - 24
            height: 28
            radius: Theme.metric.controlRadius
            color: filterField.activeFocus ? Theme.color.surfaceRaised
                                           : Theme.color.surfaceBase

            TextInput {
                id: filterField
                anchors.fill: parent
                anchors.leftMargin: 9
                anchors.rightMargin: 9
                verticalAlignment: TextInput.AlignVCenter
                color: Theme.color.textPrimary
                font.pixelSize: Theme.type.body
                selectByMouse: true
                selectionColor: Theme.color.accent
                selectedTextColor: Theme.color.onAccent
                clip: true
                onTextChanged: root.cursor = 0
                Keys.onEscapePressed: { text = ""; root.forceActiveFocus() }
                Keys.onUpPressed: root.step(-1)
                Keys.onDownPressed: root.step(1)

                Text {
                    anchors.fill: parent
                    verticalAlignment: Text.AlignVCenter
                    visible: filterField.text === "" && !filterField.activeFocus
                    text: qsTr("Filter")
                    color: Theme.color.textDim
                    font.pixelSize: Theme.type.body
                }
            }
        }

        Flickable {
            id: railScroll
            anchors.top: filterBox.bottom
            anchors.topMargin: 12
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.bottom: parent.bottom
            clip: true
            contentWidth: width
            contentHeight: railList.height + 16
            boundsBehavior: Flickable.StopAtBounds

            // The rail measures zero until its rows are laid out, so a reveal
            // that arrives first would clamp to the top and stay there.
            onContentHeightChanged: if (root.currentRowY >= 0)
                                        ensureVisible(root.currentRowY, 28)

            // Arrowing past the bottom of the rail, or picking a component from
            // the command line, otherwise moves a selection you cannot see.
            function ensureVisible(y, h) {
                var top = contentY
                var bottom = contentY + height
                if (y < top)
                    contentY = y
                else if (y + h > bottom)
                    contentY = y + h - height
                contentY = Math.max(0, Math.min(contentY,
                                                Math.max(0, contentHeight - height)))
            }

            Column {
                id: railList
                width: parent.width

                Repeater {
                    model: root.view

                    delegate: Column {
                        required property var modelData
                        width: railList.width

                        Text {
                            x: 16
                            topPadding: 14
                            bottomPadding: 4
                            text: parent.modelData.name.toUpperCase()
                            color: Theme.color.textDim
                            font.pixelSize: Theme.type.axis
                            font.letterSpacing: 0.8
                        }

                        Repeater {
                            model: parent.modelData.items

                            delegate: Rectangle {
                                id: row
                                required property var modelData
                                readonly property bool isCurrent: modelData.name === root.currentName

                                width: railList.width
                                height: 28
                                color: isCurrent
                                       ? Theme.color.surfaceRaised
                                       : (hover.hovered ? Theme.color.surfaceBase : "transparent")

                                function report() {
                                    root.currentRowY = mapToItem(railList, 0, 0).y
                                }

                                // Deferred: during construction the row has not
                                // been positioned yet, so mapToItem would answer
                                // about where it used to be.
                                onIsCurrentChanged: if (isCurrent) Qt.callLater(report)
                                onYChanged: if (isCurrent) Qt.callLater(report)
                                Component.onCompleted: if (isCurrent) Qt.callLater(report)

                                // The selected row is marked, not merely tinted:
                                // a 0.10 wash over a 0.05 rail is a small step.
                                Rectangle {
                                    width: 2
                                    height: parent.height
                                    color: Theme.color.accent
                                    visible: row.isCurrent
                                }

                                Text {
                                    x: 16
                                    anchors.verticalCenter: parent.verticalCenter
                                    width: parent.width - 26
                                    elide: Text.ElideRight
                                    text: row.modelData.name
                                    color: row.isCurrent ? Theme.color.textPrimary
                                                         : Theme.color.textMuted
                                    font.pixelSize: Theme.type.body
                                }

                                HoverHandler { id: hover }
                                TapHandler {
                                    onTapped: {
                                        root.select(row.modelData.name)
                                        root.forceActiveFocus()
                                    }
                                }
                            }
                        }
                    }
                }
            }
        }
    }

    // ---- the stage -------------------------------------------------------

    Item {
        id: stage
        anchors.left: rail.right
        anchors.right: parent.right
        anchors.top: parent.top
        anchors.bottom: parent.bottom
        anchors.margins: 24
        visible: root.current !== null

        Text {
            id: heading
            text: root.current ? root.current.name : ""
            color: Theme.color.textPrimary
            font.pixelSize: 20
            font.bold: true
        }

        Text {
            id: fileLabel
            anchors.left: heading.right
            anchors.leftMargin: 10
            anchors.baseline: heading.baseline
            text: root.current && root.current.file ? root.current.file : ""
            color: Theme.color.textDim
            font.pixelSize: Theme.type.body
        }

        Text {
            id: blurb
            anchors.top: heading.bottom
            anchors.topMargin: 4
            width: stage.width
            text: root.current ? root.current.blurb : ""
            color: Theme.color.textMuted
            font.pixelSize: Theme.type.body
            wrapMode: Text.WordWrap
        }

        Flickable {
            id: pane
            anchors.top: blurb.bottom
            anchors.topMargin: 18
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.bottom: parent.bottom
            clip: true
            // Specimens draw with Shapes, which ignore ancestor clipping — see
            // docs/10-design-system.md §10.8. Without the layer, a specimen
            // taller than the pane paints its chart over the heading.
            layer.enabled: true
            contentWidth: Math.max(width, body.width)
            contentHeight: Math.max(height, body.height)
            boundsBehavior: Flickable.StopAtBounds

            Loader {
                id: body
                sourceComponent: {
                    if (!root.current) return null
                    if (root.current.kind === "palette") return paletteView
                    if (root.current.kind === "type") return typeView
                    return specimenView
                }
                onLoaded: pane.contentY = 0
            }
        }
    }

    // ---- views -----------------------------------------------------------

    Component {
        id: specimenView

        Flow {
            spacing: 26
            width: pane.width

            Repeater {
                // A component with no declared variants still gets one cell, so
                // both cases go down the same path.
                model: root.currentVariants

                delegate: Column {
                    required property var modelData
                    spacing: 8

                    Specimen {
                        source: root.current.file
                        props: parent.modelData.props ? parent.modelData.props : ({})
                        stageWidth: root.current.stage ? root.current.stage.w : 0
                        stageHeight: root.current.stage ? root.current.stage.h : 0
                    }

                    Text {
                        text: {
                            var lbl = parent.modelData.label
                            var sz = parent.children[0].width + " × " + parent.children[0].height
                            return lbl ? lbl + "  ·  " + sz : sz
                        }
                        color: Theme.color.textDim
                        font.pixelSize: Theme.type.axis
                    }
                }
            }
        }
    }

    Component {
        id: paletteView

        Flow {
            spacing: 14
            width: pane.width

            Repeater {
                // Read off theme.js rather than transcribed, so a token added
                // there appears here without anyone remembering to add it.
                model: Object.keys(Theme.color)

                delegate: Column {
                    required property var modelData
                    spacing: 5

                    Rectangle {
                        width: 116
                        height: 52
                        radius: Theme.metric.controlRadius
                        color: Theme.color[parent.modelData]
                    }

                    Text {
                        text: parent.modelData
                        color: Theme.color.textPrimary
                        font.pixelSize: Theme.type.axis
                    }

                    Text {
                        text: String(Theme.color[parent.modelData])
                        color: Theme.color.textDim
                        font.pixelSize: Theme.type.axis
                    }
                }
            }
        }
    }

    Component {
        id: typeView

        Column {
            spacing: 18
            width: pane.width

            Repeater {
                model: Object.keys(Theme.type)

                delegate: Row {
                    required property var modelData
                    spacing: 16

                    Text {
                        width: 150
                        text: parent.modelData
                        color: Theme.color.textDim
                        font.pixelSize: Theme.type.axis
                        anchors.baseline: parent.children[2].baseline
                    }

                    Text {
                        width: 44
                        text: Theme.type[parent.modelData] + "px"
                        color: Theme.color.textDim
                        font.pixelSize: Theme.type.axis
                        anchors.baseline: parent.children[2].baseline
                    }

                    Text {
                        text: qsTr("Partly cloudy, 27°")
                        color: Theme.color.textPrimary
                        font.pixelSize: Theme.type[parent.modelData]
                        font.bold: parent.modelData.indexOf("reading") === 0
                                   || parent.modelData === "status"
                                   || parent.modelData === "cardTitle"
                    }
                }
            }
        }
    }

    Text {
        anchors.centerIn: parent
        visible: root.current === null
        text: qsTr("Nothing matches that filter.")
        color: Theme.color.textDim
        font.pixelSize: Theme.type.status
    }
}
