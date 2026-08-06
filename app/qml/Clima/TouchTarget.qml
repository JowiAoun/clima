// SPDX-FileCopyrightText: 2026 Jowi Aoun
// SPDX-License-Identifier: GPL-3.0-or-later
// An invisible tap area that is never smaller than a fingertip.
//
//   Item {                       the mark, at whatever size it should be drawn
//       width: 15; height: 15
//       ChevronGlyph { ... }
//       TouchTarget { onTapped: root.open() }
//   }
//
// It centres itself on its parent and grows to `Theme.metric.hitMin` in any
// direction the parent is short of it. Nothing moves on screen: an 11 px
// dismiss cross stays an 11 px dismiss cross, and the 44 px around it that a
// thumb needs is a rectangle nobody can see.
//
// ---- why the target and not the mark ------------------------------------------
//
// The alternative is to make the control itself bigger, and for some controls
// that is right — a settings row at 42 px should simply be 44, and a dropdown
// item at 38 should be 44, because those are surfaces a reader is aiming at and
// their size IS the affordance. It is wrong for a mark. A 44 px dismiss cross
// or a 44 px disclosure chevron is a shape shouting at the reader, and the
// screens here are full of small marks sitting in generous padding — which is
// exactly the shape of layout where the target can grow into space that is
// already empty.
//
// ---- two things to know before using it ---------------------------------------
//
// It sits ON TOP of anything declared before it, because QML stacks later
// siblings above earlier ones. That is what makes it work at all, and it is
// also how it goes wrong: put it before a control it should not cover, and the
// control below stops receiving anything. AlertBanner has the scar.
//
// And an ancestor with `clip: true` clips input, not just paint. A target that
// grows past a clipping edge grows into nothing there. Both of the banner's
// controls are inside a clipping plate and both still fit, but a 44 px target
// on a mark 4 px from a clipped edge would silently be 40.
//
// Overlapping neighbours are the case this component cannot solve, and it does
// not pretend to: two marks 29 px apart cannot both have a 44 px target, and
// the answer there is to move them apart. LocationBar is where that came up.
//
// Finally, `area` exists because a Row lays out every visual child it has. A
// pointer handler inside a Row takes no cell; this does, and dropping one into
// MobileCard's link row silently added a third column to it. Where the mark
// lives in a positioner, put this beside the positioner and point `area` at
// what it should cover.
import QtQuick

Item {
    id: root

    signal tapped()

    // Exposed so the control can tint on hover without declaring a second
    // handler over the same area. Read-only: a caller that wants to force a
    // hover state wants a different property on their own component.
    readonly property alias hovered: hover.hovered

    // Overridable for the rare control that needs more, never less. Reading the
    // token rather than writing 44 is what makes raising the design system's
    // floor a one-line change that re-runs the whole audit.
    property real minSize: Theme.metric.hitMin

    // A pointer's cursor. `Qt.ArrowCursor` for a target that is a whole row
    // rather than a control, so the caller can turn it off.
    property int cursorShape: Qt.PointingHandCursor

    // What to cover. The parent, which is the ordinary case, or any sibling —
    // see the note above about positioners.
    property Item area: parent

    anchors.centerIn: area
    width:  Math.max(area ? area.width  : 0, root.minSize)
    height: Math.max(area ? area.height : 0, root.minSize)

    HoverHandler {
        id: hover
        cursorShape: root.cursorShape
    }

    TapHandler {
        onTapped: root.tapped()
    }
}
