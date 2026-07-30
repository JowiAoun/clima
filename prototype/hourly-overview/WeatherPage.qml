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
import QtQuick
import "theme.js" as Theme

Item {
    id: root

    // Forwarded so --metric / --day / --list still reach the control that owns
    // the state, now that the page is between them and Main.
    property alias metricId: tabs.currentId
    property alias listView: tabs.listView
    property alias dayIndex: dayStrip.currentIndex

    property alias contentY: scroll.contentY
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
            color: scroll.moving ? Theme.color.textMuted : Theme.color.trackLine
            height: Math.max(28, parent.height * scroll.height / Math.max(1, scroll.contentHeight))
            y: (parent.height - height) * (scroll.contentY / Math.max(1, root.maxContentY))
            Behavior on color { ColorAnimation { duration: 160 } }
        }
    }
}
