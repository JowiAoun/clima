// SPDX-FileCopyrightText: 2026 Jowi Aoun
// SPDX-License-Identifier: GPL-3.0-or-later
// The scrolling container every mobile screen is built in.
//
// It is the phone's answer to what WeatherPage does on the desktop: one
// vertical scroll owned by the page, a centred content column, and sections
// separated by space alone. Written once because five screens needed the same
// four decisions, and the first two of them had already disagreed about the
// bottom padding — which on a phone means the last row of a page sitting
// underneath the nav bar where nobody can read it.
//
//   MobilePage {
//       MobileCard { width: parent.width; ... }
//       MobileCard { width: parent.width; ... }
//   }
//
// Children are added to a Column, so they need a width and nothing else.
//
// The layer is not optional. Every chart in this prototype draws with Qt Quick
// Shapes, and Shapes ignore ancestor clipping entirely — see §10.8. Without it
// the hourly strip paints straight over the nav bar as the page scrolls.
import QtQuick
import "theme.js" as Theme

Item {
    id: root

    default property alias content: column.data

    // How much of the bottom of the page is covered by the nav bar. The shell
    // hands this down rather than the page reading the token itself, so a page
    // shown without a shell — in the gallery, say — is not padded for chrome
    // that is not there.
    property real bottomInset: 0

    // Forwarded so the harness can scroll and flick a mobile page the same way
    // it does the desktop one. `--poke scroll=` assigns contentY; `--poke
    // flick=` goes through flick() so the view genuinely moves.
    property alias contentY: scroll.contentY
    readonly property real maxContentY: Math.max(0, scroll.contentHeight - scroll.height)
    function flickBy(velocity) { scroll.flick(0, velocity) }

    readonly property real margin: Theme.metric.mobileMargin

    // The column stops growing rather than stretching. At 834 px — the shell
    // runs on tablets too — a full-width hero puts the temperature and the
    // condition at opposite ends of the screen with a hand-span of nothing
    // between them.
    readonly property real columnWidth:
        Math.min(width - margin * 2, Theme.metric.mobileContentMax)

    Flickable {
        id: scroll
        anchors.fill: parent
        clip: true
        layer.enabled: true

        contentWidth: width
        contentHeight: column.height + root.margin + root.bottomInset
        flickableDirection: Flickable.VerticalFlick
        boundsBehavior: Flickable.StopAtBounds

        Column {
            id: column
            y: root.margin
            x: Math.round((scroll.width - width) / 2)
            width: root.columnWidth
            spacing: Theme.metric.mobileGap
        }
    }
}
