// SPDX-FileCopyrightText: 2026 Jowi Aoun
// SPDX-License-Identifier: GPL-3.0-or-later
// The weather page: every component the prototype has, in one scrolling column.
//
//   location bar          where this is for
//   current conditions    the headline
//   hourly                metric tabs → day strip → chart or list
//   weather details       twelve cards, one per measurable
//
// The order is the reference's, and it is an argument rather than a habit: the
// page answers "what is it doing right now", then "what will it do today", then
// "tell me about one thing in particular". Each section is a narrower question
// than the one above it.
//
// Sections are separated by space alone — no rules, no wrapper panels. A panel
// around a section would be a wash containing washes, which is the one thing
// §10.1 says never to build: the cards inside it would come out at 0.135 and
// read as a lighter patch. Contrast against the page is what defines a surface
// here, so a section that is not itself a surface has to be pure layout.
//
// ---- motion -----------------------------------------------------------------
// The shell animates one thing: the scroll thumb's colour. That is the whole
// budget, and the reason is that the page is not short of movement — it is a
// container for four sections that each already move. The tab bar tints, the day
// strip slides its selection, the chart crossfades to the list and blends its
// feels-like series, the details grid arrives as a twelve-card wave. Motion
// added at the shell level does not join that; it competes with it, and it does
// so across the reader's whole viewport rather than inside one card.
//
// So the section-by-section entrance on load is rejected, and not only because
// §10.6 forbids a component that is unreadable until its animation finishes.
// Staggering four sections in means the headline — the one thing the app was
// opened to read — is the thing being withheld, and the wave the details grid
// already runs would then be a wave inside a wave.
import QtQuick
import "theme.js" as Theme

Item {
    id: root

    // Forwarded so --metric / --day / --list still reach the control that owns
    // the state, now that the page is between them and Main.
    property alias metricId: tabs.currentId
    property alias listView: tabs.listView
    property alias dayIndex: dayStrip.currentIndex
    property alias feelsLike: chart.feelsLike
    property alias animated: chart.animated

    property alias contentY: scroll.contentY

    // A real flick, not an assignment. Setting `contentY` goes through
    // QQuickFlickable::setContentY(), which calls movementEnding() — so `moving`
    // never becomes true and the scroll thumb's recolour, the only animation
    // this shell has, could not be filmed at all. Driven by `--poke flick=`.
    function flickBy(velocity) { scroll.flick(0, velocity) }
    readonly property real maxContentY: Math.max(0, scroll.contentHeight - scroll.height)

    readonly property real margin: Theme.metric.pageMargin

    // A content page, not a canvas. Past about this width the slug row's cells
    // and the grid's trailing gap grow without anything being easier to read,
    // so the column stops and centres instead.
    readonly property real maxContentWidth: 1320

    Flickable {
        id: scroll
        anchors.fill: parent

        // clip bounds the rectangles; the layer bounds the Shapes, which ignore
        // ancestor clipping entirely — every chart on this page draws with one,
        // and without the layer they paint straight over the sections above and
        // below as it scrolls. See docs/10-design-system.md §10.8.
        clip: true
        layer.enabled: true

        contentWidth: width
        contentHeight: column.height + root.margin * 2
        flickableDirection: Flickable.VerticalFlick
        boundsBehavior: Flickable.StopAtBounds

        Column {
            id: column
            y: root.margin
            x: Math.round((scroll.width - width) / 2)
            width: Math.min(scroll.width - root.margin * 2, root.maxContentWidth)
            spacing: Theme.metric.sectionGap

            // ---- where -----------------------------------------------------
            // The name and the card it names are one thing, so they sit closer
            // together than two sections do.
            Column {
                width: parent.width
                spacing: 14

                LocationBar { }

                CurrentConditions { width: parent.width }
            }

            // ---- hourly ----------------------------------------------------
            // Not a Column: the day strip and the chart card have to abut with
            // no gap at all, because the selected day card is a tab growing out
            // of the panel below it. One spacing value cannot be both 16 and 0.
            Item {
                width: parent.width
                implicitHeight: tabs.height + 16 + dayStrip.height + chart.height
                height: implicitHeight

                MetricTabBar {
                    id: tabs
                    anchors.left: parent.left
                    anchors.right: parent.right
                    anchors.top: parent.top
                }

                DayStrip {
                    id: dayStrip
                    anchors.left: parent.left
                    anchors.right: parent.right
                    anchors.top: tabs.bottom
                    anchors.topMargin: 16
                }

                // Declared after the day strip on purpose: it paints over the
                // junction, which is what makes the selected card and the panel
                // read as one surface.
                HourlyOverview {
                    id: chart
                    anchors.left: parent.left
                    anchors.right: parent.right
                    anchors.top: dayStrip.bottom
                    metricId: tabs.currentId
                    listView: tabs.listView
                }
            }

            // ---- details ---------------------------------------------------
            WeatherDetails {
                id: details
                width: parent.width
            }
        }
    }

    // ---- scroll indicator --------------------------------------------------
    // Drawn rather than imported: the prototype runs on QtQuick and
    // QtQuick.Shapes alone, with no QtQuick.Controls, so there is no ScrollBar
    // to reach for. It is also the whole affordance telling you the page has
    // more below it, so it should not be invisible until you already know.
    //
    // Which settles the one animation a page shell is always offered: the
    // overlay-scrollbar fade, in on scroll and out again on idle. It is the
    // obvious motion here and it is wrong here, because the thing it fades away
    // is the only cue that there is a page below the fold — on a page this tall
    // the indicator is never redundant, so there is never a moment it is right
    // to hide. Keeping it and changing its weight says the same thing without
    // taking the cue back.
    //
    // Its `visible` binding is left as a hard toggle, which is the second
    // tempting animation and also wrong. The state does occur — at 900x2800 the
    // whole page fits and the indicator correctly disappears — but the only
    // thing that can reach it is a window resize, and what it is really
    // reporting is whether the content still overflows the viewport. That makes
    // it layout, and §10.6 is unambiguous that layout does not animate on
    // resize: fading here would mean the indicator ghosting in and out under
    // the cursor as a window drag crosses the threshold, which reads as the app
    // struggling to keep up rather than as a transition.
    Rectangle {
        id: scrollTrack
        visible: root.maxContentY > 0
        width: 4
        radius: 2
        color: Theme.color.gridLineWeak
        anchors.right: parent.right
        anchors.rightMargin: 5
        anchors.top: parent.top
        anchors.bottom: parent.bottom
        anchors.topMargin: root.margin
        anchors.bottomMargin: root.margin

        Rectangle {
            width: parent.width
            radius: parent.radius

            // The one state the shell has: the reader is moving the page, or
            // is not. `moving` covers dragging, flicking and the wheel.
            //
            // No `--poke` reaches it, so this is the one animation here that
            // film.sh cannot show: `--poke scroll=N` and `--scroll N` both
            // assign `contentY`, and QQuickFlickable::setContentY() calls
            // movementEnding() before it moves, so `moving` is false in every
            // frame the harness can grab. A `--poke flick=<velocity>` calling
            // `scroll.flick()` would make it reviewable; that is Main.qml.
            color: scroll.moving ? Theme.color.textMuted : Theme.color.trackLine

            // Size and position are bound straight through, never animated.
            // The thumb is a readout of where the page is, and a readout that
            // eases is a readout that lies: during a flick it would trail the
            // content it reports on, and on a window drag it would ease to a
            // new height, which is §10.6's "layout does not animate on resize"
            // wearing a different hat. Both are the affordance lagging the
            // thing it exists to describe.
            height: Math.max(28, parent.height * scroll.height / Math.max(1, scroll.contentHeight))
            y: (parent.height - height) * (scroll.contentY / Math.max(1, root.maxContentY))

            // Was a literal 160 — one of the eight durations for four jobs that
            // §10.6 was written to stop. A weight change is a tint.
            Behavior on color {
                ColorAnimation { duration: Theme.motion.tint; easing.type: Easing.OutCubic }
            }
        }
    }
}
