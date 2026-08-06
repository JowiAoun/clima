// SPDX-FileCopyrightText: 2026 Jowi Aoun
// SPDX-License-Identifier: GPL-3.0-or-later
// Every touch target in a specimen, drawn and measured.
//
// A layout defect you can see is a layout defect somebody fixes. A control that
// is 32 px tall looks exactly like a control that is 44 px tall — the mark
// inside it is the same size either way — so the only way to review touch is to
// make the *target* visible, which is what this does: a rectangle over every
// tappable area, green at or above `Theme.metric.hitMin` in both directions and
// red below it, with the measurement written on the ones that fail.
//
// ---- why the tree is walked rather than annotated -----------------------------
//
// The alternative is a marker property on each control — `hitTarget: true` —
// and it is worse in the one way that matters: it reports what somebody
// remembered to declare. A target added in a hurry is exactly the target that
// is too small, and it is the one an annotation-based overlay would not draw.
// Walking the object tree finds what is there.
//
// ---- what counts as a target ---------------------------------------------------
//
//   MouseArea      its own bounds
//   TapHandler     its parent item's bounds, which is the area a tap in fact
//                  activates
//
// HoverHandler is deliberately not here. Hovering is a pointer's affordance and
// a finger never does it; a 20 px hover region is not a defect, and drawing it
// beside the real targets would bury them.
//
// Detection is by shape rather than by type name, because QML gives JS no way
// to ask an object what C++ class it is. `containsMouse` with `hoverEnabled`
// and a width is a MouseArea and nothing else in this tree; `gesturePolicy` is
// a TapHandler and not the HoverHandler beside it. tst_hittargets pins both by
// asserting a known count on a known component, so a Qt release that renamed
// either of those would fail a test rather than quietly find nothing — which is
// exactly how the first version of this file failed, and it failed looking like
// a screen with no controls on it.
//
// ---- used twice ---------------------------------------------------------------
//
// The gallery draws it. tests/qml/tst_hittargets.qml builds one invisibly and
// asserts `failures()` is empty for every screen the phone has — which is the
// half of this that keeps working when nobody is looking.
import QtQuick
import Clima

Item {
    id: root

    // What to scan. Anything under it, at any depth.
    property Item subject: null

    // The floor a target is measured against. Read from the token rather than
    // written here, so raising the design system's floor re-runs this audit.
    property int hitMin: Theme.metric.hitMin

    // The scan, as a list of { x, y, w, h, ok, kind, label } in this item's
    // coordinates. Rebuilt by rescan(); never assigned from outside.
    property var targets: []

    readonly property int failureCount: {
        var n = 0
        for (var i = 0; i < targets.length; ++i)
            if (!targets[i].ok)
                ++n
        return n
    }

    // The failures alone, for a test that wants to name them.
    function failures() {
        var out = []
        for (var i = 0; i < targets.length; ++i)
            if (!targets[i].ok)
                out.push(targets[i])
        return out
    }

    clip: true

    // ---- the walk ------------------------------------------------------------

    function isItem(o) {
        return o !== null && o !== undefined
               && o.childrenRect !== undefined && o.visible !== undefined
    }

    function isMouseArea(o) {
        return o !== null && o !== undefined
               && o.containsMouse !== undefined && o.hoverEnabled !== undefined
               && o.width !== undefined
    }

    // `gesturePolicy` alone. Measured against the tree rather than assumed:
    // a HoverHandler reports it as undefined and a TapHandler as 0, so the
    // property's presence separates the two handlers this file cares about
    // telling apart. DragHandler and PinchHandler do not have it either, which
    // is correct — neither is a tap target.
    function isTapHandler(o) {
        return o !== null && o !== undefined && o.gesturePolicy !== undefined
    }

    // The item a handler is attached to, which is the area a tap on it in fact
    // activates.
    //
    // `parent` and not `parentItem`, which is what the C++ getter is called and
    // what a first attempt used: QQuickPointerHandler declares the Q_PROPERTY as
    // `parent`, so `handler.parentItem` from QML is undefined — and undefined
    // silently failed every guard, so the overlay drew nothing at all and looked
    // like a component that had no targets rather than a scan that found none.
    function targetOf(handler) {
        return handler.parent
    }

    // Which component a target belongs to, by name.
    //
    // "MobileCard · TapHandler 62×17" is a defect somebody can go and fix;
    // "TapHandler 62×17" is a puzzle. QML gives JS no way to ask an object its
    // type, but its JS wrapper stringifies as `MobileCard_QMLTYPE_87(0x…)` —
    // so the name is there, and walking up to the nearest ancestor that is not
    // a built-in QQuick* type lands on the .qml file that declared the control.
    //
    // Presentational, not load-bearing: nothing branches on it, and if a Qt
    // release changes the spelling the worst case is a report that says
    // "QQuickItem".
    function componentName(o) {
        var s = String(o)
        var paren = s.indexOf("(0x")
        if (paren > 0)
            s = s.substring(0, paren)
        return s.replace(/_QMLTYPE_\d+$/, "").replace(/_QML_\d+$/, "")
    }

    function ownerOf(item) {
        var o = item
        while (o !== null && o !== undefined) {
            var n = componentName(o)
            if (n.indexOf("QQuick") !== 0)
                return n
            o = o.parent
        }
        return "?"
    }

    function measure(area, source, kind) {
        // mapFromItem's four-argument form returns a rect, which is the whole
        // reason the geometry is read here rather than at draw time: a target
        // eleven items deep inside a scrolled Flickable has no useful x of its
        // own, and this is the only place that knows where it landed.
        var r = root.mapFromItem(area, 0, 0, area.width, area.height)
        return {
            x: r.x, y: r.y, w: r.width, h: r.height,
            ok: Math.min(r.width, r.height) >= root.hitMin - 0.01,
            kind: kind,
            owner: ownerOf(area),
            label: Math.round(r.width) + "×" + Math.round(r.height)
        }
    }

    // Hidden means skipped, and `visible` is the only test.
    //
    // Worth knowing before reusing this: QQuickItem's `visible` is EFFECTIVE
    // visibility — false if any ancestor is hidden — and a QtQuickTest lives in
    // a QQuickView that is never shown, so a screen built there reports false
    // for every item on it and this walk finds nothing at all. tst_hittargets
    // therefore builds a real Window rather than teaching this function about
    // its own harness. That is the right way round: the alternative was a scan
    // that behaved differently in the test than in the app, on the one
    // condition the test exists to check.
    function walk(item, out) {
        if (!isItem(item) || !item.visible)
            return

        // `data` and not `children`: a pointer handler is not a visual child,
        // so it appears in one list and not the other. `children` would find
        // every MouseArea and no TapHandler at all, which in this tree is most
        // of them.
        var kids = item.data
        for (var i = 0; i < kids.length; ++i) {
            var o = kids[i]
            if (o === null || o === undefined)
                continue

            if (o.enabled !== false) {
                if (isMouseArea(o) && o.width > 0 && o.height > 0)
                    out.push(measure(o, o, "MouseArea"))
                else if (isTapHandler(o) && isItem(targetOf(o))
                         && targetOf(o).width > 0 && targetOf(o).height > 0)
                    out.push(measure(targetOf(o), o, "TapHandler"))
            }

            if (isItem(o))
                walk(o, out)
        }
    }

    function rescan() {
        if (subject === null) {
            if (targets.length > 0)
                targets = []
            return
        }
        var out = []
        walk(subject, out)

        // Assigned only when it changed. A `var` property emits on every
        // assignment whether or not the value differs, and the Repeater below
        // rebuilds every delegate when it does — which under the poll timer is
        // a flicker four times a second.
        if (JSON.stringify(out) !== JSON.stringify(targets))
            targets = out
    }

    onSubjectChanged: rescan()
    onWidthChanged: rescan()
    onHeightChanged: rescan()
    onVisibleChanged: if (visible) rescan()
    Component.onCompleted: rescan()

    // Polled, and there is no better answer available. A specimen settles
    // asynchronously — text metrics, an image, a Flickable clamping its content
    // — and there is no one signal that means "this subtree has stopped
    // moving". The poll costs a tree walk of a few hundred objects four times a
    // second while the overlay is up, and it stops the moment it is put away.
    //
    // It does not fight a capture: the scan is idempotent once the scene is at
    // rest, so a grab taken after the shutter's settle delay sees the same
    // rectangles every run.
    Timer {
        interval: 250
        repeat: true
        running: root.visible && root.subject !== null
        onTriggered: root.rescan()
    }

    // ---- the drawing ---------------------------------------------------------
    //
    // Literal colours, and they are not a lapse. An instrument that reported in
    // theme tokens would change what it says when the palette changes, and a
    // red that got softer in light mode would be a red that is harder to see on
    // exactly the page where the audit is hardest. The palette page's contrast
    // column made the same call for the same reason.
    Repeater {
        model: root.targets

        delegate: Rectangle {
            id: box
            required property var modelData

            x: modelData.x
            y: modelData.y
            width: modelData.w
            height: modelData.h

            color: modelData.ok ? "#1a3ad17a" : "#33ff3b30"
            border.width: 1
            border.color: modelData.ok ? "#993ad17a" : "#ffff3b30"

            // Only the failures are labelled. A green box that says 48×48 is
            // noise on a screen with thirty of them; a red one that says 32×32
            // is the entire finding.
            Rectangle {
                visible: !box.modelData.ok
                anchors.centerIn: parent
                width: tag.implicitWidth + 8
                height: tag.implicitHeight + 3
                radius: 3
                color: "#ee2a0906"

                Text {
                    id: tag
                    anchors.centerIn: parent
                    text: box.modelData.label
                    color: "#ffff6a60"
                    font.pixelSize: 10
                    font.family: Theme.type.family
                }
            }
        }
    }
}
