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
//       MobileCard { width: root.spanWidth(2); ... }
//       MobileCard { width: root.spanWidth(1); ... }
//   }
//
// Children need a width and nothing else, and the width is stated in COLUMNS
// rather than in pixels — see `spanWidth`. On a phone there is one column and
// both answers are the same number, which is why every page reads the same
// there as it did before columns existed.
//
// The layer is not optional. Every chart in this prototype draws with Qt Quick
// Shapes, and Shapes ignore ancestor clipping entirely — see §10.8. Without it
// the hourly strip paints straight over the nav bar as the page scrolls.
import QtQuick

Item {
    id: root

    default property alias content: column.data

    // How much of the bottom of the page is covered by the nav bar. The shell
    // hands this down rather than the page reading the token itself, so a page
    // shown without a shell — in the gallery, say — is not padded for chrome
    // that is not there.
    property real bottomInset: 0

    // How much of the TOP is covered, by the alert banner. Zero on the ordinary
    // day, which is most of them.
    //
    // An inset rather than the banner taking space above the page, and the
    // difference is visible: every mobile page draws its own sky, so a banner
    // that pushed the page down would leave a strip of window background above
    // the gradient. It floats over the page and the content moves out from
    // under it, which is exactly what `bottomInset` already does for the nav.
    property real topInset: 0

    // Forwarded so the harness can scroll and flick a mobile page the same way
    // it does the desktop one. `--poke scroll=` assigns contentY; `--poke
    // flick=` goes through flick() so the view genuinely moves.
    property alias contentY: scroll.contentY
    readonly property real maxContentY: Math.max(0, scroll.contentHeight - scroll.height)
    function flickBy(velocity) { scroll.flick(0, velocity) }

    readonly property real margin: Theme.metric.mobileMargin

    // ---- columns -------------------------------------------------------------
    //
    // Which viewport class the shell is running. Pushed down by MobileShell,
    // for two reasons that the page cannot work out for itself: the window's
    // width is not the page's — a landscape rail takes 76 px of it — and
    // `--viewport mobile --size 900x844` means review the phone at 900 px, not
    // promote it to a tablet.
    //
    // Empty means nobody said, which is the gallery staging a page with no
    // shell around it. Then the width is the only signal there is, and using it
    // is what makes a page framed at the Tablet preset review as a tablet.
    property string viewportClass: ""

    readonly property string effectiveClass:
        viewportClass !== "" ? viewportClass : Viewports.classOf(width)

    readonly property int columns:
        Viewports.contentColumns(root.effectiveClass, root.width - root.margin * 2)

    // The content block: every column plus the gaps between them. It stops
    // growing rather than stretching — at 834 px a full-width hero puts the
    // temperature and the condition at opposite ends of the screen with a
    // hand-span of nothing between them — and the cap is per column, so a
    // tablet gets two cards the size of a phone's rather than one card twice
    // the size of anything the design has been reviewed at.
    readonly property real columnWidth:
        Math.min(width - margin * 2,
                 columns * Theme.metric.mobileContentMax
                 + (columns - 1) * Theme.metric.mobileGap)

    // The width a section gets, in columns.
    //
    // A function and not an attached `span` property, because QML has no
    // attached properties without C++ behind them and the alternative — the
    // page walking `children` at every resize and assigning widths — is a
    // masonry pass, which §10.6 rules out for the reason it always does: a
    // layout that is computed from what happens to be in it is a layout nobody
    // can predict from reading the file.
    //
    // Floored so two halves plus a gap can never exceed the block by a
    // rounding error, which in a Flow is not a rounding error — it is the
    // second card wrapping to its own row.
    function spanWidth(span) {
        var n = Math.min(span, root.columns)
        var gap = Theme.metric.mobileGap
        var unit = Math.floor((root.columnWidth - gap * (root.columns - 1)) / root.columns)
        return unit * n + gap * (n - 1)
    }

    Flickable {
        id: scroll
        anchors.fill: parent
        clip: true
        layer.enabled: true

        contentWidth: width
        contentHeight: column.height + root.margin + root.topInset + root.bottomInset
        flickableDirection: Flickable.VerticalFlick
        boundsBehavior: Flickable.StopAtBounds

        // A Flow and not a Column, and the difference only shows on a tablet:
        // a full-width child takes a row to itself and two half-width children
        // share one, which is exactly the two-column arrangement with no code
        // to arrange it. At one column every child is full width, every row
        // holds one, and a Flow *is* a Column — which is what lets the phone's
        // eight golden images stay byte for byte what they were.
        //
        // Flow's single `spacing` is both the gap between columns and the gap
        // between rows. That is not a compromise here: `mobileGap` was already
        // the answer to both questions.
        Flow {
            id: column
            y: root.margin + root.topInset
            x: Math.round((scroll.width - width) / 2)
            width: root.columnWidth
            spacing: Theme.metric.mobileGap
        }
    }
}
