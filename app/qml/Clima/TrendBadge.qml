// SPDX-FileCopyrightText: 2026 Jowi Aoun
// SPDX-License-Identifier: GPL-3.0-or-later
// The small round arrow beside a detail card's status line.
//
// Rising, falling and steady are the three things a reading can be doing, and
// the badge says which at a glance without spending a word on it.
//
// ---- this component deliberately does not animate --------------------------
//
// `direction` never changes on a live badge, anywhere in this prototype:
//
//   - On the page, `DetailCard` binds it to a card's `trend`, which comes from
//     `Detail`. A refresh replaces the whole snapshot at once and rebuilds the
//     card with it, so a badge is constructed with its direction rather than
//     watched changing its mind — and under `--fixture` the clock is frozen and
//     the value cannot move at all (§10.6).
//   - In the gallery, the four arrows on the "Trend badge" page are four
//     separate specimens with four fixed props, not one badge cycling. A
//     remount destroys and rebuilds them; it does not change anyone's mind.
//
// So a direction transition — the arrow swinging, the disc cross-fading between
// trendUp and trendDown — would be motion built for a state that does not
// exist, tested only by the developer who wrote it.
//
// Nor does it animate on arrival. The badge is punctuation on the status line,
// and §10.6 requires the status to be legible at rest position zero; the card's
// one piece of arrival motion is its visualisation sweeping to its reading,
// which `DetailCard.reveal` already drives. A 16 px disc popping in beside a
// bold word is the twelve-cards-at-once flicker that rule exists to prevent.
import QtQuick
import QtQuick.Shapes

Item {
    id: root

    property string direction: "none"    // "up" | "down" | "steady" | "none"
    property real badgeSize: 16

    visible: direction !== "none"
    width: visible ? badgeSize : 0
    height: badgeSize

    readonly property color tint: direction === "up" ? Theme.state.trendUp
                                : direction === "down" ? Theme.state.trendDown
                                                       : Theme.state.trendSteady

    Rectangle {
        anchors.fill: parent
        radius: width / 2
        color: root.tint
    }

    Shape {
        anchors.fill: parent
        preferredRendererType: Shape.CurveRenderer

        ShapePath {
            fillColor: "transparent"
            strokeColor: Theme.accent.ink
            strokeWidth: 1.6
            capStyle: ShapePath.RoundCap
            joinStyle: ShapePath.RoundJoin

            PathSvg {
                path: {
                    var s = root.badgeSize
                    var a = s * 0.3, b = s * 0.7      // arrow extents
                    if (root.direction === "steady")
                        return "M " + a + " " + (s / 2) + " L " + b + " " + (s / 2)
                    // Diagonal shaft with a two-stroke head at the tip.
                    var y0 = root.direction === "up" ? b : a
                    var y1 = root.direction === "up" ? a : b
                    return "M " + a + " " + y0 + " L " + b + " " + y1
                         + " M " + b + " " + y1 + " L " + (b - s * 0.22) + " " + y1
                         + " M " + b + " " + y1 + " L " + b + " "
                         + (root.direction === "up" ? y1 + s * 0.22 : y1 - s * 0.22)
                }
            }
        }
    }
}
