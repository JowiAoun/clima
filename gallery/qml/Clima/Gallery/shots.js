// SPDX-FileCopyrightText: 2026 Jowi Aoun
// SPDX-License-Identifier: GPL-3.0-or-later
//
// The shot catalogue: the composed images the README and the docs are made of.
//
// Same shape as gallery.js and for the same reason — it is data, consumed by
// three things (ShotSheet.qml builds it, GalleryOptions validates --shot
// against it, scripts/shots.sh iterates it) with no second list anywhere.
//
// What is NOT here is any size. A sheet says which devices it shows and how far
// they are zoomed; the pixel dimensions come from Viewports at build time, so a
// change to what "tablet" means moves these images and cannot leave them
// describing a device the app no longer has. That is also why this file cannot
// compute them itself: a `.pragma library` has no access to QML singletons,
// which is a limitation worth keeping — it is what stops the numbers being
// copied here.
.pragma library

// The two shots the README actually uses. `hero` is the one at the top; the
// per-device ones are for the table under it, where somebody is looking at one
// layout rather than at the range.
//
// `zoom` on the hero is 0.5, and that is a deliberate trade. At half size the
// labels inside the app are not readable, and they are not meant to be — the
// hero is about the shape of the product across three form factors. The
// per-device shots are 1.0, and those are the readable ones.
var sheets = [
    {
        id: "hero",
        label: "All three form factors, together",
        devices: ["mobile", "tablet", "desktop"],
        zoom: 0.5,
        sky: "dusk"
    },
    {
        id: "desktop",
        label: "The desktop window",
        devices: ["desktop"],
        zoom: 1.0,
        sky: "dusk"
    },
    {
        id: "tablet",
        label: "A tablet, two columns under a bottom bar",
        devices: ["tablet"],
        zoom: 1.0,
        sky: "dusk"
    },
    {
        id: "tablet-landscape",
        label: "A tablet turned, with the nav rail on the left",
        devices: ["tablet-landscape"],
        zoom: 1.0,
        sky: "dusk"
    },
    {
        id: "phone",
        label: "A phone, five tabs under a nav bar",
        devices: ["mobile"],
        zoom: 1.0,
        sky: "night"
    }
];

function ids() {
    var out = [];
    for (var i = 0; i < sheets.length; ++i)
        out.push(sheets[i].id);
    return out;
}

function byId(id) {
    for (var i = 0; i < sheets.length; ++i) {
        if (sheets[i].id === id)
            return sheets[i];
    }
    return null;
}
