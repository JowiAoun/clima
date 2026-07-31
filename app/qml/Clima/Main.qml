// SPDX-FileCopyrightText: 2026 Jowi Aoun
// SPDX-License-Identifier: GPL-3.0-or-later
import QtQuick
import QtQuick.Window
import "theme.js" as Theme
import "viewports.js" as Viewports
import "sky.js" as Sky
import "detaildata.js" as Detail

Window {
    id: win

    visible: true
    width: 1340
    height: 762

    // A phone is 390 px wide, so the old 680 floor would have made the layout
    // this window exists to show unreachable by dragging. The floor now only
    // has to keep the narrowest screen readable.
    minimumWidth: 360
    minimumHeight: 480
    color: Theme.color.pageBg
    title: qsTr("Clima (prototype)")

    // ---- which shell -------------------------------------------------------
    // The window's width decides, and viewports.js owns the thresholds so the
    // gallery frames a specimen at exactly the width the app would switch at.
    //
    // `--viewport mobile` pins it and resizes the window to the preset, which
    // is what a headless grab needs: a shell chosen from a width that a
    // screenshot flag then changes is a shell chosen from the wrong width.
    property string forcedViewport: ""
    readonly property string viewportClass:
        forcedViewport !== "" ? forcedViewport : Viewports.classOf(win.width)
    readonly property bool mobile: Viewports.usesMobileShell(viewportClass)

    // ---- the sky -----------------------------------------------------------
    // The phone's background follows the clock: a deep blue by day, a starred
    // indigo at night, warmed at the two crossings. The desktop stays at
    // `dusk`, which is the palette this prototype has always had.
    //
    // That split is deliberate and it is not timidity. The desktop page is a
    // 1340 px window that is mostly cards — the background is a rim around
    // them and a wash under them, and the reader never sees enough of it for a
    // constellation to be anything but noise behind a chart. A phone is the
    // opposite: the hero sits directly on the sky with no card at all, and
    // there is a screen's worth of it above the fold.
    //
    // `--sky night|day|dawn|dusk` forces a phase and turns the field on
    // anywhere, because the one thing the mock data cannot do is be a
    // different time of day.
    //
    // ---- which clock -------------------------------------------------------
    // detaildata.sun, because it is the clock that carries minutes. The two
    // mock files now agree about the instant — `mockdata.nowIndex` is 12:00,
    // the hour this observation falls in, and both take the same sunrise and
    // sunset — so this is a choice of resolution and not a choice of source.
    // The resolution is the whole point: dawn and dusk are the seventy minutes
    // either side of a crossing, and an hour index cannot say where in that
    // band it is.
    property string forcedSky: ""
    readonly property string skyPhase:
        forcedSky !== "" ? forcedSky
      : (win.mobile ? Sky.phaseAt(Detail.sun.nowMin, Detail.sun.riseMin, Detail.sun.setMin)
                    : "dusk")

    // The page background is painted as an item, not left to Window.color.
    // grabToImage() captures contentItem, which does not include the window's
    // clear colour — so every headless screenshot came out with a black page
    // behind the cards, which is not what is on screen.
    PageBackdrop {
        anchors.fill: parent
        phase: win.skyPhase
        stars: win.mobile || win.forcedSky !== ""
    }

    // `--card Uv` renders one detail card on the page gradient and nothing
    // else, so a card can be built and checked without the rest of the screen
    // in the way. `--details` renders the whole grid, `--gallery` the component
    // library.
    property string previewCard: ""
    property bool previewGrid: false
    property bool previewGallery: false
    property string galleryPick: ""

    // Which device frame the gallery stages its specimens in. Empty is free —
    // the component at its own size, which is what the gallery did before
    // frames existed and is still right for a glyph.
    property string galleryViewport: ""

    Loader {
        active: win.previewCard !== ""
        anchors.centerIn: parent
        sourceComponent: Component {
            Loader {
                source: "Detail" + win.previewCard + "Card.qml"
                onStatusChanged: if (status === Loader.Error)
                    console.warn("preview: no such card —", source)
            }
        }
    }

    // The grid lays out in full and does not scroll itself — the page owns that.
    // Shown on its own it still needs somewhere to scroll and something to bound
    // its cards' Shapes, so the preview supplies both.
    Loader {
        active: win.previewGrid
        anchors.fill: parent
        anchors.margins: 22
        sourceComponent: Component {
            Flickable {
                clip: true
                layer.enabled: true
                contentWidth: width
                contentHeight: gridItem.height
                flickableDirection: Flickable.VerticalFlick
                boundsBehavior: Flickable.StopAtBounds

                WeatherDetails {
                    id: gridItem
                    width: parent.width
                }
            }
        }
    }

    // The gallery gets the whole window with no margin: it paints its own rail
    // to the edge, the way a tool window should.
    Loader {
        id: galleryLoader
        active: win.previewGallery
        anchors.fill: parent
        sourceComponent: Component {
            Gallery {
                pick: win.galleryPick
                viewport: win.galleryViewport
                // Whatever the app would be showing at this hour, so a
                // component framed as a phone is reviewed on the sky the phone
                // would actually give it — stars included.
                skyPhase: win.forcedSky !== ""
                          ? win.forcedSky
                          : Sky.phaseAt(Detail.sun.nowMin, Detail.sun.riseMin,
                                        Detail.sun.setMin)
            }
        }
    }

    // `--walk N` steps the gallery N components on before grabbing, so a
    // headless check can exercise *navigation* and not only first paint. Every
    // gallery bug found by hand so far was a bug that only appears on the
    // second component shown, which is exactly what picking one at startup
    // never touches.
    Timer {
        id: walkTimer
        interval: 400
        property int steps: 0
        onTriggered: {
            if (galleryLoader.item === null)
                return
            for (var i = 0; i < steps; ++i)
                galleryLoader.item.step(1)
        }
    }

    // The app itself. Every preview flag above is a way of looking at one piece
    // of this in isolation; with none of them set, this is the product.
    //
    // Two shells, one product. `WeatherPage` is the desktop's single scrolling
    // column; `MobileShell` is the phone's five tabs under a nav bar. Which one
    // runs is a function of the window width and nothing else — there is no
    // "mobile build".
    //
    // A Loader rather than both in the tree with one hidden. Keeping both would
    // build five phone screens on every desktop launch to show none of them,
    // and the shells share no state that survives the swap anyway: `page`
    // below is whichever one is live.
    Loader {
        id: shellLoader
        anchors.fill: parent
        visible: win.previewCard === "" && !win.previewGrid && !win.previewGallery
        active: visible
        sourceComponent: win.mobile ? mobileShell : desktopShell
    }

    Component {
        id: desktopShell
        WeatherPage { }
    }

    Component {
        id: mobileShell
        MobileShell { tab: win.startTab }
    }

    // Whichever shell is live. Every flag and poke below goes through this, so
    // `--metric wind` means the same thing on a phone as on a desktop.
    //
    // Guarded at every call site rather than here: during `--gallery` the
    // Loader is inactive and there is no shell at all, and a poke arriving then
    // should warn rather than throw.
    readonly property Item page: shellLoader.item

    // The tab the mobile shell opens on. Read at construction rather than
    // assigned afterwards, because assigning it later would rebuild the page
    // that has just finished laying itself out.
    property string startTab: "today"

    // --scroll N drops the page N pixels down before grabbing. The details grid
    // is below the fold at every window size that fits on a laptop, so without
    // this a headless review of the page can only ever see its top third — and
    // the sections it cannot see are the ones with twelve charts in them.
    //
    // Deferred for the same reason the walk is: contentHeight is still 0 during
    // Component.onCompleted, and a contentY assigned against it is clamped
    // straight back to zero.
    Timer {
        id: scrollTimer
        interval: 400
        property real target: 0
        onTriggered: if (win.page)
                         win.page.contentY = Math.min(target, win.page.maxContentY)
    }

    // ---- filming ----------------------------------------------------------
    // A still frame cannot show motion. `--grab` is enough to review a layout
    // and useless for reviewing a transition: it lands wherever the animation
    // happened to be when the grab timer fired, which is usually after it
    // finished — deliberately so, since golden images want a settled frame. An
    // animation that is wrong — or missing entirely — grabs identically to one
    // that is right.
    //
    //   --film <prefix> --frames N --every MS
    //
    // writes <prefix>-00.png … and quits. `film.sh` wraps this and tiles the
    // frames into one contact sheet, which is the artefact you actually look at.
    //
    // What makes it show a *transition* rather than N copies of a resting state
    // is `--poke`: frame 00 is grabbed, the poke is applied, and every frame
    // after it is the component changing. See applyPokes().
    property var pokes: []

    function applyPokes() {
        for (var i = 0; i < win.pokes.length; ++i) {
            var eq = win.pokes[i].indexOf("=")
            if (eq < 0) {
                console.warn("--poke: expected target=value, got", win.pokes[i])
                continue
            }
            var k = win.pokes[i].substring(0, eq)
            var v = win.pokes[i].substring(eq + 1)
            var on = (v === "true" || v === "1")

            // Every target below lives on the shell, and under `--gallery`
            // there is no shell. Warning beats throwing: a poke that cannot
            // land should say so, not abort the rest of the list.
            if (k !== "remount" && win.page === null) {
                console.warn("--poke", k + ":", "no shell is running")
                continue
            }

            switch (k) {
            case "metric":  win.page.metricId = v; break
            case "day":     win.page.dayIndex = parseInt(v); break
            case "list":    win.page.listView = on; break
            case "feels":   win.page.feelsLike = on; break
            case "scroll":  win.page.contentY = parseFloat(v); break
            // Only the mobile shell has tabs. On the desktop this is the one
            // poke that is genuinely meaningless rather than merely ignored,
            // so it says so.
            case "tab":
                if (win.mobile)
                    win.page.tab = v
                else
                    console.warn("--poke tab: only the mobile shell has tabs")
                break
            // Negative velocity carries the content upward, i.e. scrolls down.
            case "flick":
                var vel = parseFloat(v)
                win.page.flickBy(-Math.abs(isNaN(vel) || vel === 0 ? 1400 : vel))
                break
            // Rebuilding the specimen replays whatever the component does on
            // mount, which for a detail card is the only animation it has —
            // the data behind these cards never changes while the app runs.
            case "remount":
                if (galleryLoader.item !== null)
                    galleryLoader.item.remount()
                else
                    console.warn("--poke remount: only meaningful with --gallery")
                break
            default:
                console.warn("--poke: unknown target", k)
            }
        }
    }

    Timer {
        id: filmStart
        interval: 900               // let first paint and any mount motion settle
        onTriggered: filmTimer.start()
    }

    // --poke without --film: apply once the scene has settled, so a plain
    // --grab can capture a poked *resting* state rather than a transition.
    Timer {
        id: pokeOnly
        interval: 500
        onTriggered: win.applyPokes()
    }

    Timer {
        id: filmTimer
        repeat: true
        interval: 60
        property string prefix: ""
        property int total: 8
        property int shot: 0
        property int saved: 0

        onTriggered: {
            // On the second tick, so frame 00 is a settled "before" that has
            // definitely rendered, and frame 01 is the first moment of change.
            if (shot === 1)
                win.applyPokes()

            if (shot >= total) {
                stop()              // saves are still in flight; the last one quits
                return
            }

            var idx = shot
            shot++
            var name = filmTimer.prefix + "-" + (idx < 10 ? "0" : "") + idx + ".png"
            var ok = win.contentItem.grabToImage(function (result) {
                if (!result.saveToFile(name))
                    console.warn("film: could not write", name)
                filmTimer.saved++
                if (filmTimer.saved >= filmTimer.total)
                    Qt.quit()
            })
            if (!ok) {
                console.warn("film: grabToImage refused")
                Qt.quit()
            }
        }
    }

    // Headless capture, for design review and CI golden images:
    //   qml Main.qml -- --grab shot.png [--metric wind]
    Timer {
        id: grabTimer
        // Long enough for the details grid's staggered reveal to finish: the
        // last card starts a stagger-per-card into the wave and then takes a
        // full reveal of its own. Grab before that lands and every golden image
        // catches a different card mid-sweep.
        interval: 1600
        property string target: ""
        onTriggered: {
            var ok = win.contentItem.grabToImage(function (result) {
                if (!result.saveToFile(grabTimer.target))
                    console.warn("grab: could not write", grabTimer.target)
                else
                    console.info("grab: wrote", grabTimer.target)
                Qt.quit()
            })
            if (!ok) {
                console.warn("grab: grabToImage refused")
                Qt.quit()
            }
        }
    }

    Component.onCompleted: {
        var args = Qt.application.arguments

        // ---- viewport and size, before anything else -----------------------
        // Both of these change the window width, and the window width decides
        // which shell is loaded. A `--metric wind` applied to the desktop page
        // and *then* resized to a phone is applied to a shell that is
        // destroyed a moment later, and the flag looks like it does nothing.
        //
        // --viewport also sets the size, so `--viewport mobile` is one flag
        // rather than a pin and a matching --size that can drift apart.
        var vp = args.indexOf("--viewport")
        if (vp >= 0 && vp + 1 < args.length) {
            var preset = Viewports.byId(args[vp + 1])
            if (preset !== null) {
                // In the gallery the flag means something different, and it
                // has to: resizing the window to 390 px there would leave 158
                // px of stage beside a 232 px rail. The gallery frames a
                // specimen at the preset instead, and keeps its own window.
                if (args.indexOf("--gallery") >= 0) {
                    win.galleryViewport = preset.id
                } else {
                    win.forcedViewport = preset.id
                    win.width = preset.w
                    win.height = preset.h
                }
            } else {
                console.warn("--viewport: expected one of",
                             Viewports.ids().join(", "), "— got", args[vp + 1])
            }
        }

        // --size 1340x900, so a headless grab can be tall enough to hold the
        // whole grid. Reviewing a grid through a viewport that cuts off its
        // last row is how a misaligned card in that row stays unnoticed.
        //
        // After --viewport on purpose: the preset is a starting point and this
        // is the override, so `--viewport mobile --size 390x1600` grabs a tall
        // phone rather than fighting over the window.
        var s = args.indexOf("--size")
        if (s >= 0 && s + 1 < args.length) {
            var wh = args[s + 1].split("x")
            if (wh.length === 2 && parseInt(wh[0]) > 0 && parseInt(wh[1]) > 0) {
                win.width = parseInt(wh[0])
                win.height = parseInt(wh[1])
            } else {
                console.warn("--size: expected WxH, got", args[s + 1])
            }
        }

        var t = args.indexOf("--tab")
        if (t >= 0 && t + 1 < args.length)
            win.startTab = args[t + 1]

        var sky = args.indexOf("--sky")
        if (sky >= 0 && sky + 1 < args.length) {
            var phase = args[sky + 1]
            if (Theme.sky[phase] !== undefined)
                win.forcedSky = phase
            else
                console.warn("--sky: expected night, dawn, day or dusk — got", phase)
        }

        var m = args.indexOf("--metric")
        if (m >= 0 && m + 1 < args.length && win.page)
            win.page.metricId = args[m + 1]

        var d = args.indexOf("--day")
        if (d >= 0 && d + 1 < args.length && win.page)
            win.page.dayIndex = parseInt(args[d + 1])

        if (args.indexOf("--list") >= 0 && win.page)
            win.page.listView = true

        var c = args.indexOf("--card")
        if (c >= 0 && c + 1 < args.length)
            win.previewCard = args[c + 1]

        if (args.indexOf("--details") >= 0)
            win.previewGrid = true

        var g = args.indexOf("--gallery")
        if (g >= 0) {
            // An optional component name may follow — every word of it up to
            // the next flag, so `--gallery weather glyph` works without quotes.
            var words = []
            for (var w = g + 1; w < args.length && args[w].indexOf("--") !== 0; ++w)
                words.push(args[w])

            // Before previewGallery, not after: setting that activates the
            // Loader synchronously, and a Gallery built with an empty pick has
            // already chosen what to show by the time the pick arrives.
            win.galleryPick = words.join(" ")

            // The gallery wants room for its rail plus a stage, but only when
            // nobody said otherwise. `--size` is now parsed before this — it
            // has to be, since the width decides which shell runs — so this
            // has to ask rather than simply enlarging, or `--gallery --size
            // 900x600` would silently come back at 1500x950.
            if (args.indexOf("--size") < 0) {
                win.width = Math.max(win.width, 1500)
                win.height = Math.max(win.height, 950)
            }
            win.previewGallery = true

            var k = args.indexOf("--walk")
            if (k >= 0 && k + 1 < args.length) {
                // Validated before assigning: `steps` is an int property, so
                // QML coerces a NaN from parseInt to 0 on the way in and the
                // check would then be testing the coercion, not the input.
                var n = parseInt(args[k + 1])
                if (!isNaN(n) && n >= 0) {
                    walkTimer.steps = n
                    if (n > 0)
                        walkTimer.start()
                } else {
                    console.warn("--walk: expected a count >= 0, got", args[k + 1])
                }
            }
        }

        var sc = args.indexOf("--scroll")
        if (sc >= 0 && sc + 1 < args.length) {
            // Validated before assigning, so a typo warns instead of quietly
            // scrolling to the top and looking like the flag does nothing.
            var y = parseFloat(args[sc + 1])
            if (!isNaN(y) && y >= 0) {
                scrollTimer.target = y
                scrollTimer.start()
            } else {
                console.warn("--scroll: expected a distance >= 0, got", args[sc + 1])
            }
        }

        // --poke repeats: --poke metric=uv --poke day=3
        var collected = []
        for (var p = 0; p < args.length; ++p)
            if (args[p] === "--poke" && p + 1 < args.length)
                collected.push(args[p + 1])
        win.pokes = collected

        var fr = args.indexOf("--frames")
        if (fr >= 0 && fr + 1 < args.length) {
            var nf = parseInt(args[fr + 1])
            if (!isNaN(nf) && nf > 0)
                filmTimer.total = nf
            else
                console.warn("--frames: expected a count > 0, got", args[fr + 1])
        }

        var ev = args.indexOf("--every")
        if (ev >= 0 && ev + 1 < args.length) {
            var ms = parseInt(args[ev + 1])
            if (!isNaN(ms) && ms > 0)
                filmTimer.interval = ms
            else
                console.warn("--every: expected ms > 0, got", args[ev + 1])
        }

        var fm = args.indexOf("--film")
        if (fm >= 0 && fm + 1 < args.length && args[fm + 1].indexOf("--") !== 0) {
            filmTimer.prefix = args[fm + 1]
            filmStart.start()
        } else if (win.pokes.length > 0) {
            pokeOnly.start()
        }

        var i = args.indexOf("--grab")
        if (i >= 0 && i + 1 < args.length) {
            // The precipitation field is the one thing on this page that moves
            // without being asked, so a grab of it would otherwise catch a
            // different frame every run and no two golden images would agree.
            // Frozen, it still draws rain — precip.js seeds every drop from its
            // hour, so the frozen frame is a deterministic one rather than an
            // empty one.
            //
            // Offered rather than assigned, which is MobileShell.push()'s rule
            // and is here for the same reason: under `--gallery` there is no
            // shell at all, and a shell whose current screen has no chart has
            // no such property. Assigning one an object does not have throws,
            // and a throw here takes the rest of this block with it — the two
            // lines below included. That is how three of the four capture
            // paths came to print one error and then hang forever, having
            // never started the timer that both writes the file and quits.
            if (win.page !== null && win.page.animated !== undefined)
                win.page.animated = false
            grabTimer.target = args[i + 1]
            grabTimer.start()
        }
    }
}
