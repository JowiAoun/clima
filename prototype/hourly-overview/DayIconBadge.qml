// SPDX-License-Identifier: GPL-3.0-or-later
// The circular icon badge the selected day card uses: a pale disc for the daytime
// condition, a blue disc for the night one. Unselected cards show a bare glyph, so
// the badge is part of how selection reads rather than decoration.
import QtQuick
import "theme.js" as Theme

Item {
    id: root

    property string kind: "clear-day"
    property bool night: false
    property real badgeSize: 50

    implicitWidth: badgeSize
    implicitHeight: badgeSize
    width: badgeSize
    height: badgeSize

    Rectangle {
        anchors.fill: parent
        radius: width / 2
        gradient: Gradient {
            GradientStop {
                position: 0.0
                color: root.night ? Theme.color.badgeNightTop : Theme.color.badgeDayTop
            }
            GradientStop {
                position: 1.0
                color: root.night ? Theme.color.badgeNightBottom : Theme.color.badgeDayBottom
            }
        }
    }

    WeatherGlyph {
        anchors.centerIn: parent
        kind: root.kind
        glyphSize: root.badgeSize * 0.68
        // A white cloud on a near-white disc is invisible; shift its gradient down.
        onLightBackground: !root.night
    }
}
