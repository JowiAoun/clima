// SPDX-FileCopyrightText: 2026 Jowi Aoun
// SPDX-License-Identifier: GPL-3.0-or-later
// The component gallery: every component in the app, on one screen, on the
// gradient it is actually composited over.
//
// It exists because almost every defect found in this design so far was
// invisible in the code and obvious in a render — and because a component is
// easiest to get wrong in the states no current screen happens to use. The
// catalogue is gallery.js; this file is only the browser around it.
//
//   clima-gallery                        open it
//   clima-gallery uv                     open it on a particular component
//   clima-gallery --grab g.png --size 1500x950
//
// `import Clima` is what makes the components on the stage the app's components
// rather than a copy of them: Theme, Viewports and PageBackdrop come from it
// here, and every specimen comes from it in Specimen.qml.
import QtQuick
import Clima
import "gallery.js" as Catalogue
import "contrast.js" as Contrast
// The app's own sampler, reached the same way gallery.js reaches theme.js: one
// directory up, because Clima.Gallery maps one level below Clima. The ramps
// page draws its bars with the function the chart draws its columns with, so a
// ramp that reviews cleanly here is the ramp the chart got.
import "../chartmath.js" as ChartMath

Item {
    id: root

    focus: true

    // Pre-selects an entry by name or file, so `--gallery moon` lands on the
    // moon card instead of on whatever happens to be first.
    property string pick: ""

    // ---- device frames -------------------------------------------------------
    // "" is free — the component at whatever size the catalogue gives it, which
    // is what the gallery did before any of this existed and is still the right
    // default for a glyph or a badge.
    //
    // Anything else is a viewports.js preset, and the specimen is staged inside
    // a box of exactly that size with the page gradient painted inside it. The
    // width is the point: almost every layout defect this prototype has had was
    // a component that was fine at the width its author happened to try and
    // broken at the width the app gives it. A frame makes that width a thing you
    // choose rather than a thing you inherit from the window.
    //
    // The frame is not scaled to fit. A 1340x762 desktop frame inside a 1500 px
    // window overflows the pane and the pane scrolls, which is honest — a
    // half-size preview of a 11 px axis label tells you nothing about whether it
    // is legible.
    property string viewport: ""

    // The sky a mobile or tablet frame is painted with. Desktop frames and
    // free mode stay at `dusk` — the palette the desktop page runs on — for
    // the same reason the app does.
    property string skyPhase: "dusk"

    readonly property var preset: viewport === "" ? null : Viewports.byId(viewport)
    readonly property bool framed: preset !== null

    // What the shell at this viewport would actually hand a component: the
    // frame minus the page margin on both sides. A card staged at the full
    // device width is a card reviewed 28 px wider than it will ever be drawn.
    readonly property real frameContentWidth: {
        if (!framed)
            return 0
        var margin = Viewports.usesMobileShell(preset.id) ? Theme.metric.mobileMargin
                                                          : Theme.metric.pageMargin
        return Math.min(preset.w - margin * 2,
                        Viewports.usesMobileShell(preset.id)
                            ? Theme.metric.mobileContentMax : preset.w)
    }

    function cycleViewport(d) {
        // "" then the presets, so stepping wraps through free as one of the
        // options rather than making it a mode you can only leave.
        var ids = [""].concat(Viewports.ids())
        var i = ids.indexOf(viewport)
        viewport = ids[(Math.max(0, i) + d + ids.length) % ids.length]
    }

    // Rebuild every specimen on the stage, replaying whatever they do on mount.
    // Driven by `--poke remount=1`; see Main.qml.
    property int remountToken: 0
    function remount() { remountToken++ }

    readonly property real railWidth: 232

    // ---- the palette page's instrument ---------------------------------------
    //
    // Both raw palettes, side by side, which is the one thing Theme's grouped
    // properties deliberately cannot give you — they answer for the scheme that
    // is running, and a light column beside a dark one is the entire point of a
    // palette page.
    //
    // Built once here rather than inside the delegates. Fifty-nine tokens times
    // two schemes times a ground chain that recurses through up to three
    // translucent layers is a great deal of string parsing to redo on every row
    // of every repaint.
    function rawTable(scheme) {
        var t = {}
        for (var i = 0; i < Theme.colorRoles.length; ++i)
            t[Theme.colorRoles[i]] = Theme.tokensFor(Theme.colorRoles[i], scheme)
        return t
    }

    readonly property var paletteTables: ({
        dark:  root.rawTable("dark"),
        light: root.rawTable("light")
    })

    // The card, composited, in both schemes. Every ramp in the app is drawn on
    // one, and the ramps page draws its bars on the same thing rather than on
    // the pane behind it.
    readonly property var cardGround: ({
        dark:  Contrast.groundOf(root.paletteTables.dark,
                                 Theme.contrastRule, "surface.base"),
        light: Contrast.groundOf(root.paletteTables.light,
                                 Theme.contrastRule, "surface.base")
    })

    // "4.52:1", or an em dash for a token with nothing to measure against.
    // An incidental token still prints its number: it has no floor to clear,
    // but a ratio you can see is how you notice one drifting.
    function ratioText(audit) {
        if (audit.on === null)
            return "—"
        return audit.ratio.toFixed(2) + ":1"
    }

    // Red is reserved for a token that misses the floor its duty sets. An
    // incidental token is drawn dim rather than green, because a page where
    // two thirds of the rows are a passing colour is a page where the four
    // real failures do not stand out — which is the failure mode this column
    // exists to avoid.
    function ratioInk(audit) {
        if (audit.verdict === "fail")
            return Theme.state.poor
        if (audit.verdict === "pass")
            return Theme.ink.muted
        return Theme.ink.dim
    }

    readonly property int colToken:  178
    readonly property int colSwatch: 74
    readonly property int colHex:    76
    readonly property int colDuty:   74
    readonly property int colOn:     214
    readonly property int colRatio:  74
    readonly property int colGap:    12

    // A ramp, drawn.
    //
    // Sampled into cells rather than declared as a gradient because QML cannot
    // build GradientStops from a Repeater — the same wall DetailSunCard and
    // DetailMoonCard hit, where three colours are written twice over precisely
    // this. Sampling with the app's own ChartMath.sampleRamp() is the better
    // answer anyway: the bar is interpolated by the function the chart
    // interpolates with, so a ramp that reviews cleanly here is the ramp the
    // chart got, not a second reading of the same table.
    //
    // The ground matters. Six of the nine ramps carry alpha in every stop —
    // `temp.fill` opens at 80% and closes at 43% — so a bar painted on the pane
    // would be a bar reviewed at a contrast the chart never has.
    // Square corners, and no clip. A rounded bar would need one to keep the
    // sample cells off its corners, and a bar is a bar — the eight px cells are
    // the honest shape for something being read as a sequence of stops rather
    // than as a control.
    component RampBar: Rectangle {

        id: bar

        required property var stops
        required property color ground

        property int samples: 68
        property int cell: 8
        property int barHeight: 26

        width: samples * cell
        height: barHeight
        color: bar.ground

        Row {
            anchors.fill: parent

            Repeater {
                model: bar.samples

                delegate: Rectangle {
                    required property int index
                    width: bar.cell
                    height: bar.barHeight
                    color: ChartMath.sampleRamp(
                               bar.stops,
                               bar.samples <= 1 ? 0 : index / (bar.samples - 1))
                }
            }
        }
    }

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
    Keys.onLeftPressed: root.cycleViewport(-1)
    Keys.onRightPressed: root.cycleViewport(1)

    // ---- how a specimen is sized in a frame ----------------------------------
    // Three cases, and the catalogue says which by what it declares.
    //
    //   fills      a screen or a shell: it takes the whole device.
    //   stage.w    it takes a width from its host, so in a frame that width is
    //              the one the shell at this viewport would give it — not the
    //              number in the catalogue, which was only ever a stand-in for
    //              a host that was not there.
    //   neither    a glyph, a badge, a toggle: natural size, whatever the frame.
    function stageW(entry) {
        if (!framed)
            return entry.stage ? entry.stage.w : 0
        if (entry.fills)
            return preset.w
        return (entry.stage && entry.stage.w > 0) ? frameContentWidth : 0
    }

    function stageH(entry) {
        if (framed && entry.fills)
            return preset.h
        return entry.stage ? entry.stage.h : 0
    }

    // ---- the rail --------------------------------------------------------

    Rectangle {
        id: rail
        width: root.railWidth
        height: parent.height
        color: Theme.surface.recede

        Text {
            id: railTitle
            x: 16; y: 16
            text: qsTr("Components")
            color: Theme.ink.primary
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
            color: Theme.ink.dim
            font.pixelSize: Theme.type.body
        }

        Rectangle {
            id: filterBox
            x: 12
            y: railTitle.y + railTitle.height + 12
            width: rail.width - 24
            height: 28
            radius: Theme.metric.controlRadius
            color: filterField.activeFocus ? Theme.surface.raised
                                           : Theme.surface.base

            TextInput {
                id: filterField
                anchors.fill: parent
                anchors.leftMargin: 9
                anchors.rightMargin: 9
                verticalAlignment: TextInput.AlignVCenter
                color: Theme.ink.primary
                font.pixelSize: Theme.type.body
                selectByMouse: true
                selectionColor: Theme.accent.fill
                selectedTextColor: Theme.accent.ink
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
                    color: Theme.ink.dim
                    font.pixelSize: Theme.type.body
                }
            }
        }

        // ---- viewport control ------------------------------------------------
        // In the rail rather than over the stage, because it is a property of
        // how you are looking rather than of what you are looking at: it
        // persists as you arrow through the catalogue, and a control that
        // persists belongs with the other one that does.
        Text {
            id: viewportLabel
            x: 12
            y: filterBox.y + filterBox.height + 16
            text: qsTr("VIEWPORT")
            color: Theme.ink.dim
            font.pixelSize: Theme.type.axis
            font.letterSpacing: 0.8
        }

        Grid {
            id: viewportPicker
            x: 12
            y: viewportLabel.y + viewportLabel.height + 6
            width: rail.width - 24
            columns: 2
            spacing: 4

            readonly property real cellWidth: (width - spacing) / 2

            Repeater {
                // Free first, then narrow to wide. The order is the order the
                // arrow keys walk, so the control reads the way it steps.
                model: [{ id: "", label: qsTr("Free") }].concat(Viewports.presets)

                delegate: Rectangle {
                    id: vpButton
                    required property var modelData

                    readonly property bool isCurrent: modelData.id === root.viewport

                    width: viewportPicker.cellWidth
                    height: 26
                    radius: Theme.metric.controlRadius
                    color: isCurrent ? Theme.surface.raised
                                     : (vpHover.hovered ? Theme.surface.base : "transparent")
                    border.width: 1
                    border.color: isCurrent ? Theme.accent.fill : Theme.line.grid

                    Behavior on color {
                        ColorAnimation { duration: Theme.motion.tint; easing.type: Easing.OutCubic }
                    }
                    Behavior on border.color {
                        ColorAnimation { duration: Theme.motion.tint; easing.type: Easing.OutCubic }
                    }

                    Text {
                        anchors.centerIn: parent
                        text: vpButton.modelData.label
                        color: vpButton.isCurrent ? Theme.ink.primary : Theme.ink.muted
                        font.pixelSize: Theme.type.axis
                    }

                    HoverHandler { id: vpHover; cursorShape: Qt.PointingHandCursor }
                    TapHandler {
                        onTapped: {
                            root.viewport = vpButton.modelData.id
                            root.forceActiveFocus()
                        }
                    }
                }
            }
        }

        // The frame's own dimensions, so a review note can say what it was
        // taken at without anyone measuring a screenshot.
        Text {
            id: viewportSize
            x: 12
            y: viewportPicker.y + viewportPicker.height + 6
            width: rail.width - 24
            elide: Text.ElideRight
            text: root.framed ? root.preset.w + " × " + root.preset.h
                              : qsTr("component's own size")
            color: Theme.ink.dim
            font.pixelSize: Theme.type.axis
        }

        Flickable {
            id: railScroll
            anchors.top: viewportSize.bottom
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
                            color: Theme.ink.dim
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
                                       ? Theme.surface.raised
                                       : (hover.hovered ? Theme.surface.base : "transparent")

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
                                    color: Theme.accent.fill
                                    visible: row.isCurrent
                                }

                                Text {
                                    x: 16
                                    anchors.verticalCenter: parent.verticalCenter
                                    width: parent.width - 26
                                    elide: Text.ElideRight
                                    text: row.modelData.name
                                    color: row.isCurrent ? Theme.ink.primary
                                                         : Theme.ink.muted
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
            color: Theme.ink.primary
            font.pixelSize: 20
            font.bold: true
        }

        Text {
            id: fileLabel
            anchors.left: heading.right
            anchors.leftMargin: 10
            anchors.baseline: heading.baseline
            text: root.current && root.current.file ? root.current.file : ""
            color: Theme.ink.dim
            font.pixelSize: Theme.type.body
        }

        Text {
            id: blurb
            anchors.top: heading.bottom
            anchors.topMargin: 4
            width: stage.width
            text: root.current ? root.current.blurb : ""
            color: Theme.ink.muted
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
            //
            // A `kind` page draws no Shapes, and the layer is off for those —
            // which is not a micro-optimisation but a bug this page found.
            //
            // Qt 6.11's software renderer is what a headless capture runs, the
            // offscreen platform plugin advertising no GL capability (the long
            // note in scripts/grab.sh). It classifies a square, fully opaque
            // Rectangle as an opaque node, and one such node anywhere inside a
            // layer makes the *whole* layer composite as opaque black —
            // measured: a 300x26 `color: "#36375b"` blacks out the entire
            // 1220x1035 pane, while the same rectangle given a `radius`, or
            // given alpha 254/255, does not. So this is a trap rather than a
            // rule you could follow: the palette page escaped it only because
            // every swatch on it happens to have a corner radius, and the ramps
            // page, whose bars are deliberately square, did not.
            //
            // Turning the layer off where nothing needs it removes the trap
            // from the three generated pages outright. It is still live for
            // specimens, which is worth knowing before adding an opaque square
            // Rectangle to a component — `TabFillet.qml` already has the only
            // opaque colour in the palette.
            layer.enabled: root.current !== null && root.current.kind === undefined
            contentWidth: Math.max(width, body.width)
            contentHeight: Math.max(height, body.height)
            boundsBehavior: Flickable.StopAtBounds

            Loader {
                id: body
                sourceComponent: {
                    if (!root.current) return null
                    if (root.current.kind === "palette") return paletteView
                    if (root.current.kind === "ramps") return rampsView
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
                    id: cell
                    required property var modelData
                    spacing: 8

                    // The device frame, or nothing at all. In free mode this
                    // Item is exactly the specimen's own size and paints
                    // nothing of its own, so the free layout is byte for byte
                    // what it was before frames existed.
                    Item {
                        id: frame
                        width: root.framed ? root.preset.w : spec.width
                        height: root.framed ? root.preset.h : spec.height

                        // The gradient, inside the frame rather than behind it.
                        // A card framed at 390x844 in a 950 px window would
                        // otherwise be composited over the slice of the
                        // window's gradient that happens to be behind it, which
                        // is not the slice the app gives it — and being drawn on
                        // the right background is the whole premise here.
                        PageBackdrop {
                            visible: root.framed
                            anchors.fill: parent
                            radius: 6

                            readonly property bool onPhone:
                                root.framed && Viewports.usesMobileShell(root.preset.id)

                            phase: onPhone ? root.skyPhase : "dusk"
                            stars: onPhone
                        }

                        Specimen {
                            id: spec
                            file: root.current.file
                            props: cell.modelData.props ? cell.modelData.props : ({})
                            stageWidth: root.stageW(root.current)
                            stageHeight: root.stageH(root.current)
                            remountToken: root.remountToken

                            // A screen fills the device. Anything else sits
                            // where the page would put it: inset from the top
                            // by the page margin and centred across.
                            x: root.framed && !root.current.fills
                               ? Math.round((frame.width - width) / 2) : 0
                            y: root.framed && !root.current.fills
                               ? Theme.metric.mobileMargin : 0
                        }

                        // The device edge. A border is the one thing that can
                        // draw it: the frame's whole job is to show where the
                        // screen stops, and a component that runs to the edge
                        // — which every screen here does — has nothing else to
                        // separate it from the pane behind.
                        Rectangle {
                            visible: root.framed
                            anchors.fill: parent
                            radius: 6
                            color: "transparent"
                            border.width: 1
                            border.color: Theme.line.control
                        }
                    }

                    Text {
                        text: {
                            var lbl = cell.modelData.label
                            // Rounded: a specimen sized from text metrics
                            // reports 73.875, and three decimal places in a
                            // caption reads as precision rather than as noise.
                            var sz = Math.round(spec.width) + " × " + Math.round(spec.height)
                            return lbl ? lbl + "  ·  " + sz : sz
                        }
                        color: Theme.ink.dim
                        font.pixelSize: Theme.type.axis
                    }
                }
            }
        }
    }

    Component {
        id: paletteView

        Column {
            spacing: 22
            width: pane.width

            // The column heads, once at the top rather than per role. Eleven
            // repetitions of the same seven words is a page you scroll past
            // instead of read.
            Row {
                spacing: root.colGap

                Text {
                    width: root.colToken
                    text: qsTr("token")
                    color: Theme.ink.dim
                    font.pixelSize: Theme.type.axis
                }
                Text {
                    width: root.colSwatch + root.colGap + root.colHex
                    text: qsTr("dark")
                    color: Theme.ink.dim
                    font.pixelSize: Theme.type.axis
                }
                Text {
                    width: root.colSwatch + root.colGap + root.colHex
                    text: qsTr("light")
                    color: Theme.ink.dim
                    font.pixelSize: Theme.type.axis
                }
                Text {
                    width: root.colDuty
                    text: qsTr("duty")
                    color: Theme.ink.dim
                    font.pixelSize: Theme.type.axis
                }
                Text {
                    width: root.colOn
                    text: qsTr("measured on")
                    color: Theme.ink.dim
                    font.pixelSize: Theme.type.axis
                }
                Text {
                    width: root.colRatio
                    text: qsTr("dark")
                    color: Theme.ink.dim
                    font.pixelSize: Theme.type.axis
                    horizontalAlignment: Text.AlignRight
                }
                Text {
                    width: root.colRatio
                    text: qsTr("light")
                    color: Theme.ink.dim
                    font.pixelSize: Theme.type.axis
                    horizontalAlignment: Text.AlignRight
                }
            }

            Repeater {
                // Read off the theme rather than transcribed, so a token added
                // to a role appears here without anyone remembering to add it.
                // `colorRoles` is the one list that is maintained by hand and it
                // is maintained in Theme.qml, next to the roles, rather than
                // here: a role is a design decision and the tool that draws the
                // palette should not be the place it is recorded.
                model: Theme.colorRoles

                delegate: Column {
                    id: role
                    required property var modelData

                    spacing: 6
                    width: parent.width

                    // The role, named. Without it the page is 59 rows in one
                    // undifferentiated field, which is the flat list this whole
                    // restructure existed to get rid of — and a palette that
                    // does not show its groups cannot show that a token is in
                    // the wrong one.
                    Text {
                        text: role.modelData
                        color: Theme.ink.muted
                        font.pixelSize: Theme.type.cardTitle
                        font.bold: true
                        bottomPadding: 2
                    }

                    Repeater {
                        // Object.keys() on the *raw* table, not Theme.names()
                        // on the grouped property. The grouped one is a QObject
                        // and answers for the running scheme only; this page
                        // has to show the scheme it is not in, which is the
                        // whole reason Theme.tokensFor() exists.
                        //
                        // Everything a row needs is resolved out here, where
                        // the role is in scope: a Repeater's delegate is a
                        // component of its own, so reaching back into the role
                        // from inside one is an unqualified access.
                        model: {
                            var tables = root.paletteTables
                            return Object.keys(tables.dark[role.modelData]).map(function (token) {
                                var path = role.modelData + "." + token
                                var rule = Theme.contrastRule(path)
                                return {
                                    token: token,
                                    on: rule.on === null ? "—" : rule.on,
                                    duty: rule.duty,
                                    darkValue:  String(tables.dark[role.modelData][token]),
                                    lightValue: String(tables.light[role.modelData][token]),
                                    darkAudit:  Contrast.audit(tables.dark,  Theme.contrastRule,
                                                               Theme.contrastFloor, path),
                                    lightAudit: Contrast.audit(tables.light, Theme.contrastRule,
                                                               Theme.contrastFloor, path)
                                }
                            })
                        }

                        delegate: Row {
                            id: tokenRow
                            required property var modelData
                            spacing: root.colGap

                            Text {
                                width: root.colToken
                                text: tokenRow.modelData.token
                                color: Theme.ink.primary
                                font.pixelSize: Theme.type.axis
                                elide: Text.ElideRight
                                anchors.verticalCenter: parent.verticalCenter
                            }

                            // Each swatch is painted on the ground the audit
                            // measured it against, and the token's own value is
                            // laid over it translucent exactly as the app draws
                            // it. A swatch of `#12ffffff` shown on the pane is a
                            // swatch of a colour that never reaches a screen.
                            Rectangle {
                                width: root.colSwatch
                                height: 26
                                radius: Theme.metric.controlRadius
                                color: tokenRow.modelData.darkAudit.on !== null
                                       ? tokenRow.modelData.darkAudit.on : Theme.page.bg
                                anchors.verticalCenter: parent.verticalCenter

                                Rectangle {
                                    anchors.fill: parent
                                    anchors.margins: 3
                                    radius: 2
                                    color: tokenRow.modelData.darkValue
                                }
                            }
                            Text {
                                width: root.colHex
                                text: tokenRow.modelData.darkValue
                                color: Theme.ink.dim
                                font.pixelSize: Theme.type.axis
                                anchors.verticalCenter: parent.verticalCenter
                            }

                            Rectangle {
                                width: root.colSwatch
                                height: 26
                                radius: Theme.metric.controlRadius
                                color: tokenRow.modelData.lightAudit.on !== null
                                       ? tokenRow.modelData.lightAudit.on : "#eef1f7"
                                anchors.verticalCenter: parent.verticalCenter

                                Rectangle {
                                    anchors.fill: parent
                                    anchors.margins: 3
                                    radius: 2
                                    color: tokenRow.modelData.lightValue
                                }
                            }
                            Text {
                                width: root.colHex
                                text: tokenRow.modelData.lightValue
                                color: Theme.ink.dim
                                font.pixelSize: Theme.type.axis
                                anchors.verticalCenter: parent.verticalCenter
                            }

                            Text {
                                width: root.colDuty
                                text: tokenRow.modelData.duty
                                color: Theme.ink.dim
                                font.pixelSize: Theme.type.axis
                                anchors.verticalCenter: parent.verticalCenter
                            }

                            Text {
                                width: root.colOn
                                // A paired token says so, because otherwise its
                                // row is the page's most confusing sight: 1.07:1
                                // and not red. The pair is why.
                                text: tokenRow.modelData.darkAudit.paired !== undefined
                                      ? tokenRow.modelData.on + "  + " + tokenRow.modelData.darkAudit.paired
                                      : tokenRow.modelData.on
                                color: Theme.ink.dim
                                font.pixelSize: Theme.type.axis
                                elide: Text.ElideRight
                                anchors.verticalCenter: parent.verticalCenter
                            }

                            Text {
                                width: root.colRatio
                                text: root.ratioText(tokenRow.modelData.darkAudit)
                                color: root.ratioInk(tokenRow.modelData.darkAudit)
                                font.pixelSize: Theme.type.axis
                                font.bold: tokenRow.modelData.darkAudit.verdict === "fail"
                                horizontalAlignment: Text.AlignRight
                                anchors.verticalCenter: parent.verticalCenter
                            }
                            Text {
                                width: root.colRatio
                                text: root.ratioText(tokenRow.modelData.lightAudit)
                                color: root.ratioInk(tokenRow.modelData.lightAudit)
                                font.pixelSize: Theme.type.axis
                                font.bold: tokenRow.modelData.lightAudit.verdict === "fail"
                                horizontalAlignment: Text.AlignRight
                                anchors.verticalCenter: parent.verticalCenter
                            }
                        }
                    }
                }
            }
        }
    }

    Component {
        id: rampsView

        Column {
            spacing: 24
            width: pane.width

            Repeater {
                model: Object.keys(Theme.rampsFor("dark"))

                delegate: Column {
                    id: rampRole
                    required property var modelData
                    readonly property bool categorical:
                        Theme.categoricalRamps.indexOf(modelData) >= 0

                    spacing: 6
                    width: parent.width

                    Row {
                        spacing: 10

                        Text {
                            text: rampRole.modelData
                            color: Theme.ink.muted
                            font.pixelSize: Theme.type.cardTitle
                            font.bold: true
                        }
                        Text {
                            // Which kind of ramp this is, because it is the one
                            // fact that explains why three of the nine look
                            // identical in both schemes. A published band keeps
                            // its hue: recolouring the WHO's UV scale would make
                            // the app disagree with the source it is quoting.
                            text: rampRole.categorical ? qsTr("categorical · authority bands")
                                                       : qsTr("continuous")
                            color: Theme.ink.dim
                            font.pixelSize: Theme.type.axis
                            anchors.baseline: parent.children[0].baseline
                        }
                        Text {
                            text: qsTr("%1 fill stops").arg(
                                      Theme.rampsFor("dark")[rampRole.modelData].fill.length)
                            color: Theme.ink.dim
                            font.pixelSize: Theme.type.axis
                            anchors.baseline: parent.children[0].baseline
                        }
                    }

                    Repeater {
                        // fill and line are the two halves of every ramp and the
                        // chart draws both — the area under the curve and the
                        // curve itself — so a page showing only the fill would
                        // be reviewing half of what ships.
                        model: [
                            { part: "fill",
                              dark:  Theme.rampsFor("dark")[rampRole.modelData].fill,
                              light: Theme.rampsFor("light")[rampRole.modelData].fill },
                            { part: "line",
                              dark:  Theme.rampsFor("dark")[rampRole.modelData].line,
                              light: Theme.rampsFor("light")[rampRole.modelData].line }
                        ]

                        delegate: Row {
                            id: rampRow
                            required property var modelData
                            spacing: root.colGap

                            Text {
                                width: 40
                                text: rampRow.modelData.part
                                color: Theme.ink.dim
                                font.pixelSize: Theme.type.axis
                                anchors.verticalCenter: parent.verticalCenter
                            }

                            RampBar {
                                stops: rampRow.modelData.dark
                                ground: root.cardGround.dark
                            }

                            RampBar {
                                stops: rampRow.modelData.light
                                ground: root.cardGround.light
                            }
                        }
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

            // The face, named, because it is now a decision the app makes rather
            // than something fontconfig decides on the way past. Every row below
            // is set in it, so a page that showed the sizes and not the family
            // would be answering the smaller half of "what does our type look
            // like".
            Text {
                text: qsTr("family · %1").arg(Theme.type.family)
                color: Theme.ink.dim
                font.pixelSize: Theme.type.axis
            }

            Repeater {
                // Sizes only. `family` is a token in the same group and a string,
                // and a string in `font.pixelSize` is a Text with no height —
                // which on this page reads as a row that silently went missing
                // rather than as an error. Filtering on the type of the value
                // keeps the page generated rather than transcribed: the next
                // non-numeric token added to the group drops out of the ladder on
                // its own, and gets its own line above when someone writes one.
                model: Theme.names(Theme.type).filter(function (token) {
                    return typeof Theme.type[token] === "number"
                })

                delegate: Row {
                    required property var modelData
                    spacing: 16

                    Text {
                        width: 150
                        text: parent.modelData
                        color: Theme.ink.dim
                        font.pixelSize: Theme.type.axis
                        anchors.baseline: parent.children[2].baseline
                    }

                    Text {
                        width: 44
                        text: Theme.type[parent.modelData] + "px"
                        color: Theme.ink.dim
                        font.pixelSize: Theme.type.axis
                        anchors.baseline: parent.children[2].baseline
                    }

                    Text {
                        text: qsTr("Partly cloudy, 27°")
                        color: Theme.ink.primary
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
        color: Theme.ink.dim
        font.pixelSize: Theme.type.status
    }
}
