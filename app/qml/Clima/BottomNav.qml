// SPDX-FileCopyrightText: 2026 Jowi Aoun
// SPDX-License-Identifier: GPL-3.0-or-later
// The mobile shell's bottom navigation.
//
// Five destinations from mobiletabs.js, one selected. It is the only piece of
// persistent chrome the phone layout has, so it is also the only thing on the
// screen allowed to be more opaque than a wash.
//
// ---- why this is not translucent --------------------------------------------
// §10.1 says every surface is a thin white wash over the page gradient, and
// that rule is right everywhere the surface sits *in* the page. This one sits
// *over* it: the page scrolls underneath, so at 0.07 the reader would watch a
// temperature curve slide through the word "Hourly". The pager buttons already
// established the exception — a thing that floats over moving content is
// tinted and mostly opaque — and `navBg` is that token for this bar.
//
// The hairline along the top is the second half of the same decision. §10.1
// warns off borders because a border across a junction is the seam the
// junction exists to hide, but this is not a junction: it is the edge where a
// floating bar stops and scrolling content begins, and it is the only cue that
// the content continues behind it.
//
// ---- motion ------------------------------------------------------------------
// The pill slides between tabs rather than cutting, at `move`, because it is
// the one element here that is genuinely the same object in a new place. The
// glyphs and labels tint at `tint`. Nothing else moves, and nothing moves on a
// timer — the bar is at rest until it is touched.
import QtQuick
import "mobiletabs.js" as Tabs

Item {
    id: root

    property string currentId: Tabs.list[0].id

    // The bar's own height, without the strip below it. The shell needs both
    // numbers — one to place the bar, one to pad the page — and they must come
    // from the same place or the last row of a page hides under the nav.
    readonly property real barHeight: Theme.metric.navHeight
    readonly property real safeArea: Theme.metric.navSafeArea

    signal selected(string id)

    implicitHeight: barHeight + safeArea
    height: implicitHeight

    readonly property real cellWidth: width / Tabs.list.length
    readonly property real pillWidth: Math.min(64, cellWidth - 8)
    readonly property real pillHeight: 30

    Rectangle {
        anchors.fill: parent
        color: Theme.color.navBg
    }

    Rectangle {
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: parent.top
        height: 1
        color: Theme.color.navHairline
    }

    // The selection, drawn once and moved, rather than one pill per cell
    // switched on and off. That is what lets it travel: five pills fading in
    // and out is a crossfade, and a crossfade between two positions reads as
    // two objects rather than as one that moved.
    Rectangle {
        id: pill
        width: root.pillWidth
        height: root.pillHeight
        radius: height / 2
        color: Theme.color.navPill
        y: 6
        x: Math.max(0, Tabs.indexOf(root.currentId)) * root.cellWidth
           + (root.cellWidth - width) / 2

        Behavior on x {
            NumberAnimation { duration: Theme.motion.move; easing.type: Easing.OutCubic }
        }
    }

    Row {
        anchors.fill: parent

        Repeater {
            model: Tabs.list

            delegate: Item {
                id: cell
                required property var modelData

                readonly property bool isCurrent: modelData.id === root.currentId

                width: root.cellWidth
                height: root.barHeight

                NavGlyph {
                    id: glyph
                    kind: cell.modelData.glyph
                    glyphSize: 21
                    tint: cell.isCurrent ? Theme.color.navGlyphOn : Theme.color.navGlyph
                    anchors.horizontalCenter: parent.horizontalCenter
                    y: 6 + (root.pillHeight - height) / 2
                }

                Text {
                    text: cell.modelData.label
                    color: cell.isCurrent ? Theme.color.textPrimary : Theme.color.textMuted
                    font.pixelSize: Theme.type.axis
                    anchors.horizontalCenter: parent.horizontalCenter
                    anchors.top: glyph.bottom
                    anchors.topMargin: 7

                    Behavior on color {
                        ColorAnimation { duration: Theme.motion.tint; easing.type: Easing.OutCubic }
                    }
                }

                // The whole cell is the target, not the pill. A 30 px pill is
                // under every thumb-size guideline there is, and the label
                // below it is part of what the reader is aiming at.
                TapHandler {
                    onTapped: root.selected(cell.modelData.id)
                }
            }
        }
    }
}
