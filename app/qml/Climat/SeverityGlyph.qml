// SPDX-FileCopyrightText: 2026 Jowi Aoun
// SPDX-License-Identifier: GPL-3.0-or-later
// The mark beside a severe-weather alert. A different SHAPE per grade, not one
// shape in five colours.
//
// docs/04-architecture.md §4.10 forbids colour-only encoding, and a warning is
// the worst possible place to break that rule: about one man in twelve cannot
// separate the red from the amber, and those two are the difference between
// "stay indoors" and "it will be hot". So the grade is carried three times over
// — this shape, the colour, and the word beside it — and any one of the three
// is enough on its own.
//
//   extreme   an octagon. The stop sign, and the only shape here with no
//             orientation, which is what makes it read as different in kind
//             rather than as more of the same.
//   severe    a triangle, point up. The hazard triangle every road sign uses.
//   moderate  a triangle, smaller, with a flat interior bar rather than a
//             point — a caution rather than a hazard.
//   minor     a circle with a bar: information.
//   unknown   a circle with a hollow centre. The issuer declined to grade, and
//             a mark that guessed at one would be inventing what they withheld.
//
// ---- rectangles, not Shapes ---------------------------------------------------
//
// Same reason as DropletGlyph: Qt Quick Shapes escape ancestor clipping, and
// this glyph lives inside a banner that collapses and inside a scrolling sheet.
// A Shape here draws outside both. An octagon assembled from three rotated
// squares is coarser than a path and it clips like everything else.
//
// `Bound` because this file has Repeater delegates that read ids from the file
// around them, which qmllint reports as unqualified access and qmlcachegen
// cannot ahead-of-time compile. Bound scoping is what makes those lookups
// resolvable; the delegates already declare their model roles as `required`,
// which is the other half of what it asks for.
pragma ComponentBehavior: Bound

import QtQuick

Item {
    id: root

    // "extreme", "severe", "moderate", "minor", "unknown". Anything else falls
    // through to the unknown mark, which is the honest answer to a grade this
    // component has never heard of.
    property string severity: "unknown"

    property real glyphSize: 18
    property color fillColor: Theme.severity[root.severity]
                              ? Theme.severity[root.severity].glyph
                              : Theme.severity.unknown.glyph

    readonly property bool triangular: severity === "severe" || severity === "moderate"
    readonly property bool octagonal: severity === "extreme"

    implicitWidth: glyphSize
    implicitHeight: glyphSize
    width: implicitWidth
    height: implicitHeight

    // ---- extreme: an octagon, as three overlapping squares -------------------
    Item {
        anchors.fill: parent
        visible: root.octagonal

        Repeater {
            model: [0, 45]

            Rectangle {
                required property int modelData

                width: root.glyphSize * 0.92
                height: width
                x: (root.width - width) / 2
                y: (root.height - height) / 2
                rotation: modelData
                radius: width * 0.14
                color: root.fillColor
            }
        }

        // The hole. Deliberately a `page.bg`-coloured plug rather than an
        // opacity mask: the banner behind this is a translucent wash over a
        // gradient, and a mask would punch through to whatever the sky happens
        // to be doing there.
        Rectangle {
            width: root.glyphSize * 0.13
            height: root.glyphSize * 0.42
            radius: width / 2
            anchors.centerIn: parent
            anchors.verticalCenterOffset: -root.glyphSize * 0.06
            color: Theme.page.bg
        }
    }

    // ---- severe and moderate: a triangle -------------------------------------
    //
    // A 45°-rotated square whose lower half is clipped away — which gives a
    // triangle — inside an item that stretches it vertically.
    //
    // Both halves of that are needed and the first attempt had neither right.
    // It drew the rotated square and then ADDED a full-width bar under it to
    // widen the base, which produced a house: a pitched roof on a rectangle,
    // and severe and moderate came out indistinguishable. The clip is what
    // makes a triangle; the bar was making it something else.
    //
    // The stretch is the second half. A diamond's top half is always exactly
    // twice as wide as it is tall, and a hazard triangle is not — at 2:1 it
    // reads as an arrowhead. `Scale` on a PARENT rather than on the rectangle
    // itself, because Qt Quick applies the `transform` list in the item's own
    // unrotated coordinates: scaling y there and then rotating 45° yields a
    // tilted rhombus, not a taller triangle.
    Item {
        id: cut
        width: root.width
        height: root.height * 0.88
        visible: root.triangular
        clip: true

        Item {
            anchors.fill: parent
            transform: Scale {
                yScale: 1.5
                origin.x: cut.width / 2
                origin.y: cut.height    // stretch upward from the base line
            }

            Rectangle {
                width: root.glyphSize * (root.severity === "severe" ? 0.62 : 0.54)
                height: width
                rotation: 45
                radius: width * 0.16
                x: (cut.width - width) / 2
                y: cut.height - width / 2
                color: root.fillColor
            }
        }
    }

    // The interior mark, outside the clip so it stays crisp and unstretched.
    // Severe gets an exclamation — a bar and a dot; moderate gets the bar
    // alone. Real hazard iconography makes exactly this distinction, and it is
    // the third copy of a fact the colour and the word already carry.
    Rectangle {
        visible: root.triangular
        width: root.glyphSize * 0.10
        height: root.glyphSize * (root.severity === "severe" ? 0.20 : 0.13)
        radius: width / 2
        x: (root.width - width) / 2
        y: root.height * (root.severity === "severe" ? 0.42 : 0.50)
        color: Theme.page.bg
    }

    Rectangle {
        visible: root.severity === "severe"
        width: root.glyphSize * 0.10
        height: width
        radius: width / 2
        x: (root.width - width) / 2
        y: root.height * 0.68
        color: Theme.page.bg
    }

    // ---- minor and unknown: a disc -------------------------------------------
    Item {
        anchors.fill: parent
        visible: !root.triangular && !root.octagonal

        Rectangle {
            width: root.glyphSize * 0.86
            height: width
            radius: width / 2
            anchors.centerIn: parent
            color: root.fillColor
        }

        // Minor is a solid disc with an information bar; unknown is hollow.
        Rectangle {
            width: root.glyphSize * (root.severity === "minor" ? 0.12 : 0.34)
            height: root.severity === "minor" ? root.glyphSize * 0.38 : width
            radius: width / 2
            anchors.centerIn: parent
            color: Theme.page.bg
        }
    }
}
