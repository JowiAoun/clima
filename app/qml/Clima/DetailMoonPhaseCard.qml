// SPDX-FileCopyrightText: 2026 Jowi Aoun
// SPDX-License-Identifier: GPL-3.0-or-later
// Moon phase card — the moon's face, where DetailMoonCard is the moon's night.
//
// The pair splits one subject along the line the reader already reads it along.
// "When does the moon rise" is a question about tonight and is answered by an
// arc over a horizon; "what does the moon look like" is a question about the
// month and is answered by a disc. They were one card, and the disc was a 14 px
// mark riding the arc — correct, and far too small to be the answer to the
// second question.
//
// So this card is the disc at the size that makes a gibbous distinguishable
// from a quarter at a glance, the fraction as a number beside it, and the one
// date the cycle is actually navigated by. Everything on it comes from a single
// phase reading; nothing here needs a rise or a set.
//
// ---- the arrival -------------------------------------------------------------
// The terminator sweeps: the disc opens at new and fills to tonight's phase.
// That is DetailCard's rule read literally — "a dial sweeping up to its
// reading" — and it is the one motion available to a shape whose whole content
// is where one edge has got to. It also degrades correctly: at reveal 0 the
// card is a thin crescent rather than an empty box, so the moon is on the card
// before the animation starts, which is what "readable at reveal = 0" asks for.
// A card is a `DetailCard { content: Item { id: viz } }`, so everything drawn
// here lives inside a Component and reaches the two ids around it — `root` for
// the card and `viz` for the visualisation — across that boundary. Without this
// pragma neither is resolvable at compile time: qmllint reports every one of
// them as an unqualified access, and qmlcachegen, which is the half that costs
// something, cannot ahead-of-time compile the binding and leaves it to be
// interpreted on every evaluation. That is the first-paint budget in
// docs/03-tech-stack.md §3.4 being spent on lookups the compiler could have
// done.
//
// Bound makes the enclosing scope's ids lexical, which is what they already
// read as. It is safe here because every delegate in this file declares its
// `required property` — that is the one thing Bound takes away, and none of
// these were relying on it.
pragma ComponentBehavior: Bound

import QtQuick

DetailCard {
    id: root

    readonly property var d: Detail.moonPhase

    title: qsTr("Moon phase")
    status: d.available ? d.status : qsTr("Not reported")
    trend: d.trend
    body: d.available ? d.body
                      : qsTr("This forecast does not carry a moon phase for here.")

    content: Item {
        id: viz

        // Square, and as large as the row will take. The disc is the card, and
        // it is the size that makes a gibbous tell from a quarter at a glance —
        // which the 14 px mark on DetailMoonCard's arc cannot.
        readonly property real discSize: Math.min(height, 84)

        MoonGlyph {
            id: disc
            glyphSize: viz.discSize
            anchors.left: parent.left
            anchors.verticalCenter: parent.verticalCenter

            // Sweeping the terminator rather than fading the disc in. The
            // fraction IS the picture, so growing it is the card drawing its
            // own reading; a fade would just delay it.
            visible: root.d.available
            illuminated: root.d.illumination * root.reveal
            waxing: root.d.waxing
        }

        Column {
            anchors.left: disc.right
            anchors.leftMargin: 16
            anchors.right: parent.right
            anchors.verticalCenter: parent.verticalCenter
            spacing: 1

            // An em dash where the provider has no moon at all. Nought per
            // cent is not "we were not told" — it is a new moon, and a disc
            // drawn at 0 with "0%" beside it is the card asserting one. MET
            // Norway carries no phase, and the fallback serves whenever
            // Open-Meteo is down.
            Text {
                text: root.d.available ? root.d.percent + "%" : "—"
                color: Theme.ink.primary
                font.pixelSize: Theme.type.reading
                font.bold: true
            }

            Text {
                text: qsTr("Illuminated")
                color: Theme.ink.dim
                font.pixelSize: Theme.type.label
            }

            Item { width: 1; height: 10 }

            // The date the month is navigated by, and the only forward-looking
            // thing on the card. Hidden rather than blanked where the provider
            // carries no phase at all — an empty row reads as a value that
            // failed to load, which is a different claim from "this provider
            // does not do moons".
            Row {
                visible: root.d.nextFullLabel !== ""
                spacing: 7

                // A full moon, drawn as one: the same disc at the phase it
                // names. A plain accent dot would have been a bullet, and this
                // row is the one place on the card where a picture is a label.
                MoonGlyph {
                    glyphSize: 13
                    illuminated: 1
                    anchors.verticalCenter: parent.verticalCenter
                }

                Column {
                    spacing: 0
                    Text {
                        text: qsTr("Next full moon")
                        color: Theme.ink.dim
                        font.pixelSize: Theme.type.axis
                    }
                    Text {
                        text: root.d.nextFullLabel
                        color: Theme.ink.primary
                        font.pixelSize: Theme.type.body
                        font.bold: true
                    }
                }
            }
        }
    }
}
