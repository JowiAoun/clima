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
    // Ordered narrow → wide. `w`/`h` are the window size the preset opens at,
    // and `cls` is the viewport class it belongs to — usually the same word as
    // its id, and deliberately a separate field because of the one case where
    // it is not.
    //
    // `pinned` marks a preset whose class its width cannot produce. There is
    // exactly one, and it is the whole reason this shape changed: a tablet held
    // in landscape is 1112 px wide, which is past the desktop threshold, and it
    // is not a desktop. Width is the only signal a window gives — see `classOf`
    // — so that preset has to be asked for rather than derived, and a run-time
    // tablet is identified by its platform instead (Main.qml).
    readonly property var presets: [
        { id: "mobile",  cls: "mobile",  label: "Mobile",  w: 390,  h: 844,
          blurb: "A small phone. The bottom nav owns navigation and every section is one column." },
        { id: "tablet",  cls: "tablet",  label: "Tablet",  w: 834,  h: 1112,
          blurb: "Portrait tablet. The mobile shell, in two content columns rather than one." },
        { id: "tablet-landscape", cls: "tablet", pinned: true,
          label: "Tablet ↔", w: 1112, h: 834,
          blurb: "The same tablet, turned. Navigation moves to a left rail: five targets across 1112 px is a row a thumb cannot cross." },
        { id: "desktop", cls: "desktop", label: "Desktop", w: 1340, h: 762,
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
    //
    // Width is the only thing a desktop window tells you, and it is not always
    // enough. A tablet in landscape is 1112 px across and is not a desktop; a
    // desktop window dragged to 1112 px is. The same number, two answers, and
    // nothing in the geometry separates them — so this function answers the
    // question width alone can answer, and the two callers that know better
    // override it: `--viewport tablet-landscape` in review, and the platform at
    // run time, where Android is never a desktop whatever its width.
    function classOf(width) {
        if (width >= root.minWidth.desktop)
            return "desktop";
        if (width >= root.minWidth.tablet)
            return "tablet";
        return "mobile";
    }

    // Tablet runs the mobile shell, and this is still the only question that
    // decides which shell the app builds. It is not a third layout: the five
    // destinations, the cards and the pages are the phone's, and a tablet is
    // the same product with more room.
    //
    // What the room buys is answered by the two functions below rather than
    // here, and keeping them separate is the point. `usesMobileShell` means
    // "the five-tab shell"; a tablet that gained a second column and a side
    // rail is still that shell, and folding either arrangement into this
    // predicate would make the answer to "which shell" depend on how wide the
    // window happens to be at the moment a page is loaded.
    function usesMobileShell(cls) {
        return cls === "mobile" || cls === "tablet";
    }

    // ---- what the room buys -------------------------------------------------

    // The narrowest PITCH — column plus the gap beside it — at which two
    // columns are still better than one.
    //
    // 360 is the window's own minimum width, which makes it the narrowest
    // screen this layout is ever drawn on and so the narrowest column it is
    // known to survive rather than a number chosen for looking right. A pitch
    // rather than a bare column width because the gap belongs to Theme and this
    // file is pure geometry policy with no palette behind it: at the 14 px the
    // shell actually uses, a 360 pitch puts each column at 353, which is
    // between the 332 a 360 px window gives a card and the 362 a 390 px phone
    // does. Both of those are drawn every day.
    //
    // The second column therefore arrives at 720 px of usable content, which is
    // a 748 px window — a 768 px tablet with its margins taken off is 740, so
    // the smallest tablet anyone still ships gets two columns and nothing
    // narrower does.
    readonly property int minColumnWidth: 360

    // How many columns the content splits into, given the width the page is
    // actually handed. Not the window's width: a landscape rail takes 76 px of
    // it, and a rule written against the window would hand a landscape tablet
    // and a portrait one different columns at the same content width.
    //
    // Capped at two on purpose. Three columns of a card designed at 362 px
    // needs 1122 px of content, which is a desktop — and the desktop already
    // has a layout, with a twelve-card grid this shell has no equivalent of.
    function contentColumns(cls, usableWidth) {
        // The tablet class and no other. Not `usesMobileShell`, which is also
        // true of a phone: `--viewport mobile --size 900x844` is a request to
        // review the phone at 900 px, and answering it with two columns is
        // answering a question nobody asked. A real phone never reaches 720 px
        // of content anyway, so the guard only ever fires on a forced one.
        if (cls !== "tablet")
            return 1;
        return usableWidth >= root.minColumnWidth * 2 ? 2 : 1;
    }

    // Below this the rail is not worth its width: at 834 px it would leave
    // 758 px of content, which is one column, and a rail beside one column is
    // 76 px spent to save 70.
    readonly property int minRailWidth: 900

    // Which side navigation lives on.
    //
    //   "bottom"  the five-tab bar under the content. Every phone, and a
    //             tablet held upright.
    //   "rail"    a vertical strip down the left. A tablet in landscape: five
    //             targets spread across 1112 px is a row a thumb cannot cross
    //             without moving the hand that is holding the device, and the
    //             70 px the bar wants is 8 % of a 834 px screen's height —
    //             the dimension a landscape tablet has least of.
    //   "none"    the desktop page, which has no shell navigation at all.
    //
    // Landscape is `width > height` and not an orientation sensor. A tablet
    // propped in a stand and a window dragged wider are the same problem, and
    // the second one is the one a developer can actually try.
    function navStyle(cls, width, height) {
        if (!root.usesMobileShell(cls))
            return "none";
        // The tablet class only, for `contentColumns`' reason: a phone forced
        // to 900x844 for review is a phone, and handing it a rail would answer
        // a question nobody asked. A real phone that wide is a tablet by
        // `classOf` anyway, so this costs nothing outside the gallery.
        if (cls === "tablet" && width >= root.minRailWidth && width > height)
            return "rail";
        return "bottom";
    }

    // The class a preset belongs to. Its own id for three of the four; see the
    // `pinned` note on `presets` for the fourth.
    function classFor(id) {
        var p = root.byId(id);
        return p === null ? "" : p.cls;
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
