// SPDX-FileCopyrightText: 2026 Jowi Aoun
// SPDX-License-Identifier: GPL-3.0-or-later
import QtQuick
import QtQuick.Shapes
import "theme.js" as Theme

Item {
    id: root

    property bool pointsLeft: true
    property bool enabledState: true
    signal activated()

    implicitWidth: 22
    implicitHeight: 46
    width: implicitWidth
    height: implicitHeight
    // Reaching the end of the strip fades the pager back rather than moving it,
    // so this is `tint` and not `move` — the button changes appearance in place.
    // The caller may override this binding (the day strip fades its pagers all
    // the way out rather than to 0.32); the Behavior still governs whatever the
    // property ends up bound to.
    opacity: enabledState ? 1 : 0.32
    Behavior on opacity {
        NumberAnimation { duration: Theme.motion.tint; easing.type: Easing.OutCubic }
    }

    // §10.8: a faded-out control still takes clicks and still shows a pointing
    // hand unless it is disabled too. The day strip runs this to opacity 0.
    enabled: enabledState

    Rectangle {
        anchors.fill: parent
        radius: Theme.metric.controlRadius
        color: hover.hovered ? Theme.color.pagerBgHover : Theme.color.pagerBg
        opacity: 0.94
        Behavior on color {
            ColorAnimation { duration: Theme.motion.tint; easing.type: Easing.OutCubic }
        }
    }

    Shape {
        anchors.fill: parent
        preferredRendererType: Shape.CurveRenderer

        ShapePath {
            strokeColor: Theme.color.pagerGlyph
            strokeWidth: 1.6
            fillColor: "transparent"
            capStyle: ShapePath.RoundCap
            joinStyle: ShapePath.RoundJoin
            PathSvg {
                path: {
                    var cx = root.width / 2, cy = root.height / 2
                    var dx = 3.2, dy = 5.0
                    var tip = root.pointsLeft ? cx - dx : cx + dx
                    var base = root.pointsLeft ? cx + dx * 0.5 : cx - dx * 0.5
                    return "M " + base.toFixed(2) + " " + (cy - dy).toFixed(2)
                         + " L " + tip.toFixed(2) + " " + cy.toFixed(2)
                         + " L " + base.toFixed(2) + " " + (cy + dy).toFixed(2)
                }
            }
        }
    }

    HoverHandler { id: hover; cursorShape: Qt.PointingHandCursor }

    TapHandler {
        onTapped: if (root.enabledState) root.activated()
    }
}
