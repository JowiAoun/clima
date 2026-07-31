// SPDX-FileCopyrightText: 2026 Jowi Aoun
// SPDX-License-Identifier: GPL-3.0-or-later
// A raindrop built from rectangles rather than a Shape.
//
// Qt Quick Shapes escape ancestor clipping, and this glyph lives inside a
// horizontally scrolling Flickable — as a Shape, the droplet of the first
// off-screen bucket drew outside the card entirely. A circle plus a rotated
// square is a coarser teardrop, but it clips like everything else.
import QtQuick

Item {
    id: root

    property real glyphSize: 11
    property color fillColor: Theme.glyph.droplet

    implicitWidth: glyphSize * 0.78
    implicitHeight: glyphSize
    width: implicitWidth
    height: implicitHeight

    Rectangle {                       // tapered top
        width: root.width * 0.74
        height: width
        radius: width * 0.2
        x: (root.width - width) / 2
        y: root.height * 0.06
        rotation: 45
        color: root.fillColor
    }

    Rectangle {                       // round body
        width: root.width
        height: root.width
        radius: width / 2
        y: root.height - height
        color: root.fillColor
    }
}
