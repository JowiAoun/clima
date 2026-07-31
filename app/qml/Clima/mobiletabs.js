// SPDX-FileCopyrightText: 2026 Jowi Aoun
// SPDX-License-Identifier: GPL-3.0-or-later
// The five destinations of the mobile shell.
//
// One list, read by two things that must not disagree: `BottomNav` draws it,
// and `MobileShell` loads `page` for whichever id is current. Same argument as
// metrics.js — adding a screen is a line of data, and a screen that exists in
// the tree but not in this list shows up as a tab that is missing rather than
// as a page nobody can reach.
//
// The order is the reference's and it is a claim about how often each is
// wanted: today, then the rest of today, then the rest of the month, then the
// map, then the settings you touch twice a year.
.pragma library

var list = [
    { id: "today",   label: "Today",   glyph: "today",   page: "MobileTodayPage.qml" },
    { id: "hourly",  label: "Hourly",  glyph: "hourly",  page: "MobileHourlyPage.qml" },
    { id: "monthly", label: "Monthly", glyph: "monthly", page: "MobileMonthlyPage.qml" },
    { id: "maps",    label: "Maps",    glyph: "maps",    page: "MobileMapsPage.qml" },
    { id: "me",      label: "Me",      glyph: "me",      page: "MobileMePage.qml" }
];

function byId(id) {
    for (var i = 0; i < list.length; ++i)
        if (list[i].id === id)
            return list[i];
    return list[0];
}

// -1 for an id that is not a tab, so a caller can tell "not found" from "the
// first one". Anything positioning something in the bar wants
// Math.max(0, indexOf(id)) — a nav pill has to be somewhere.
function indexOf(id) {
    for (var i = 0; i < list.length; ++i)
        if (list[i].id === id)
            return i;
    return -1;
}
