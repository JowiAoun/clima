// SPDX-FileCopyrightText: 2026 Jowi Aoun
// SPDX-License-Identifier: GPL-3.0-or-later
//
// Viewport classes, and the widths that separate them.
//
// Two things read this and they must not disagree:
//
//   Main.qml    picks which shell the app runs — the desktop page, or the
//               mobile shell with its bottom nav.
//   Gallery.qml frames a specimen in a device-sized box, so a component can be
//               reviewed at the width it will actually be given.
//
// Which is the whole reason the breakpoints are data rather than two literals
// in two files. A gallery that framed a component at 390 px while the app
// switched shells at 420 would review a layout the app never renders.
//
// This was viewports.js until the tokens became singletons, and unlike theme.js
// it did not survive the move as a data file underneath. It had no reason to:
// theme.js is still on disk because gallery.js reads the ramp table out of it
// and a `.pragma library` cannot import a QML singleton, and nothing but QML has
// ever asked this file a question. A singleton *is* the shared source, so a JS
// library under it would be a second copy of the same facts kept in step by
// hand — the exact failure the file was written to prevent.
pragma Singleton

import QtQuick

QtObject {
    id: root

    // The presets are *review* sizes, not device sizes. There is no point
    // chasing a particular handset: what a layout has to survive is the narrow
    // end of a class, so `mobile` is a small phone rather than an average one
    // and a page that works there works on the large ones for free.
    //
    // Ordered narrow → wide. `w`/`h` are the window size the preset opens at.
    readonly property var presets: [
        { id: "mobile",  label: "Mobile",  w: 390,  h: 844,
          blurb: "A small phone. The bottom nav owns navigation and every section is one column." },
        { id: "tablet",  label: "Tablet",  w: 834,  h: 1112,
          blurb: "Portrait tablet. Same shell as mobile, with the content column capped rather than stretched." },
        { id: "desktop", label: "Desktop", w: 1340, h: 762,
          blurb: "The scrolling page: four sections in one column, no nav bar." }
    ]

    // Lower bound of each class, in window pixels.
    //
    // 600 is where a phone stops and a tablet starts by every convention worth
    // following, and it is also where this prototype's own content stops
    // fitting a single column comfortably. 1024 is where the desktop page's day
    // strip and twelve-card grid have room to be themselves — below it they are
    // a worse version of the mobile layout rather than a better one.
    // An inline component and not a bare QtObject, for the same reason Theme.qml
    // declares one per token group: a property typed `QtObject` tells qmllint
    // and qmlls that it has QObject's members and no others, so every read of
    // `Viewports.minWidth.tablet` — including the two in `classOf` below — comes
    // back as "Member not found" in the editor and in the lint target.
    component Breakpoints: QtObject {
        readonly property int mobile:  0
        readonly property int tablet:  600
        readonly property int desktop: 1024
    }
    readonly property Breakpoints minWidth: Breakpoints { }

    // The class a window of this width belongs to. Widest match wins.
    function classOf(width) {
        if (width >= root.minWidth.desktop)
            return "desktop";
        if (width >= root.minWidth.tablet)
            return "tablet";
        return "mobile";
    }

    // Tablet runs the mobile shell. It is not a third layout and should not
    // become one without a reason: the bottom nav, the single column and the
    // full-bleed cards all still read at 834 px, and the honest difference is
    // that the column stops growing. Ask this rather than testing the id, so
    // the day a tablet layout does diverge there is one place that decides it.
    function usesMobileShell(cls) {
        return cls === "mobile" || cls === "tablet";
    }

    function byId(id) {
        for (var i = 0; i < root.presets.length; ++i)
            if (root.presets[i].id === id)
                return root.presets[i];
        return null;
    }

    function ids() {
        var out = [];
        for (var i = 0; i < root.presets.length; ++i)
            out.push(root.presets[i].id);
        return out;
    }
}
