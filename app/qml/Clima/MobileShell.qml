// SPDX-FileCopyrightText: 2026 Jowi Aoun
// SPDX-License-Identifier: GPL-3.0-or-later
// The phone and tablet shell: five destinations under a bottom nav.
//
// The desktop answer to "what is the weather doing" is one tall scrolling page
// — WeatherPage — because a 1340 px window can afford to put the hero, the
// hourly chart and twelve detail cards in the same column and let the reader
// scroll. A phone cannot: the same page at 390 px is roughly eight screens
// deep, and the fourth of them is unreachable in any sense that matters.
//
// So the phone splits the page rather than shrinking it, and the split is the
// reference's: today, the rest of today, the rest of the month, the map, and
// settings. mobiletabs.js is the list.
//
// ---- motion ------------------------------------------------------------------
// The nav pill slides; the page does not transition. That is a decision and
// not an omission.
//
// A fade-through between two full screens is the obvious thing to add here and
// it is the wrong thing to add here. §10.6's rule is that the reader can read
// the component at rest position zero, and the whole content area of a phone
// is the worst possible place to break it: a reader who tapped "Hourly" is
// waiting on the one thing they asked for. The pill is what has to move,
// because the pill is what changed — it is the same object in a new place, and
// watching it travel is what tells you the bar has five positions and you are
// now at the second. The page underneath is not a transition, it is a
// destination.
import QtQuick
import "mobiletabs.js" as Tabs

Item {
    id: root

    property string tab: Tabs.list[0].id

    // Forwarded to whichever page wants them, so `--metric`, `--day`,
    // `--list` and `--poke feels=` still reach the chart now that a screen
    // sits between them and Main. A page that does not have a chart ignores
    // them; the properties live here because the shell outlives the page and
    // a metric chosen on the hourly screen should survive a trip to the map.
    property string metricId: "overview"
    property bool listView: false
    property int dayIndex: 1
    property bool feelsLike: false

    // Ambient motion, forwarded the same way. The precipitation field behind
    // the hourly chart is the only thing under this shell that moves when
    // nothing has changed, and `--grab` clears this so a headless frame is the
    // same frame every run — frozen it still draws rain, because precip.js
    // seeds every drop from its hour.
    //
    // It travels with the four above rather than being reached for directly
    // from Main, and for the same reason they do: the screen that owns the
    // chart is destroyed and rebuilt on every tab change, so the shell is the
    // only thing here that a flag parsed once at startup can still address.
    property bool animated: true

    // Scrolling, forwarded to the current page.
    //
    // Write-only, deliberately. Main assigns `contentY` to place the view for
    // a headless grab and reads `maxContentY` to clamp it; making this a live
    // two-way mirror of a Flickable that is destroyed and rebuilt on every tab
    // change buys nothing and costs a binding loop. Reading it back gives you
    // the last value assigned, not where the reader has scrolled to.
    property real contentY: 0
    onContentYChanged: if (pageLoader.item) pageLoader.item.contentY = contentY

    readonly property real maxContentY: pageLoader.item ? pageLoader.item.maxContentY : 0
    function flickBy(velocity) { if (pageLoader.item) pageLoader.item.flickBy(velocity) }

    readonly property var currentTab: Tabs.byId(tab)

    Loader {
        id: pageLoader
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: parent.top
        anchors.bottom: nav.top

        source: root.currentTab.page

        onLoaded: {
            // The nav floats over the page, so the page has to know how much
            // of its own bottom is covered or its last row is unreadable.
            item.bottomInset = nav.height
            root.adopt(item)
        }

        onStatusChanged: if (status === Loader.Error)
            console.warn("mobile shell: could not load", source)
    }

    // ---- shell state, page state -------------------------------------------
    // Values are *pushed* down and requests come back up as signals. The
    // obvious alternative — Qt.binding() from the shell into the page — was
    // written first and is quietly broken: the moment the reader touches the
    // control the page assigns its own property, which destroys the binding,
    // and every later push from the shell silently stops arriving. A poke that
    // works until someone clicks the thing first is worse than one that never
    // works, because it passes review.
    //
    // Each property is offered rather than assumed. Assigning one a page does
    // not have is an error in QML, and this is what keeps the map and settings
    // screens from having to declare four chart properties to be loadable.
    function adopt(page) {
        if (page === null)
            return

        push(page)

        if (page.navigate !== undefined)
            page.navigate.connect(root.go)
        if (page.metricRequested !== undefined)
            page.metricRequested.connect(function (id) { root.metricId = id })
        if (page.dayRequested !== undefined)
            page.dayRequested.connect(function (i) { root.dayIndex = i })
    }

    function push(page) {
        if (page === null)
            return
        if (page.metricId !== undefined)  page.metricId = root.metricId
        if (page.listView !== undefined)  page.listView = root.listView
        if (page.dayIndex !== undefined)  page.dayIndex = root.dayIndex
        if (page.feelsLike !== undefined) page.feelsLike = root.feelsLike
        if (page.animated !== undefined)  page.animated = root.animated
    }

    onMetricIdChanged: push(pageLoader.item)
    onListViewChanged: push(pageLoader.item)
    onDayIndexChanged: push(pageLoader.item)
    onFeelsLikeChanged: push(pageLoader.item)
    onAnimatedChanged: push(pageLoader.item)

    // A card header's link is a request to go somewhere, not a scroll. The
    // page emits it and the shell is the only thing that knows the tab bar
    // exists.
    function go(id) {
        if (Tabs.indexOf(id) >= 0)
            root.tab = id
    }

    BottomNav {
        id: nav
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        currentId: root.tab
        onSelected: function (id) { root.tab = id }
    }
}
