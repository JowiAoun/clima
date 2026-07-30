// SPDX-License-Identifier: GPL-3.0-or-later
import QtQuick
import QtQuick.Window
import "theme.js" as Theme

Window {
    id: win

    visible: true
    width: 1340
    height: 762
    minimumWidth: 680
    minimumHeight: 560
    color: Theme.color.pageBg
    title: qsTr("Clima (prototype)")

    // The page background is painted as an item, not left to Window.color.
    // grabToImage() captures contentItem, which does not include the window's
    // clear colour — so every headless screenshot came out with a black page
    // behind the cards, which is not what is on screen.
    //
    // It is a gradient rather than a flat fill because every surface above it
    // is translucent: the cards have no colour of their own, and take whatever
    // the gradient is doing behind them. Flatten this and the whole page
    // flattens with it.
    Rectangle {
        anchors.fill: parent
        gradient: Gradient {
            GradientStop { position: 0.00; color: Theme.color.pageStop0 }
            GradientStop { position: 0.06; color: Theme.color.pageStop1 }
            GradientStop { position: 0.30; color: Theme.color.pageStop2 }
            GradientStop { position: 0.60; color: Theme.color.pageStop3 }
            GradientStop { position: 1.00; color: Theme.color.pageStop4 }
        }
    }

    // `--card Uv` renders one detail card on the page gradient and nothing
    // else, so a card can be built and checked without the rest of the screen
    // in the way. `--details` renders the whole grid, `--gallery` the component
    // library.
    property string previewCard: ""
    property bool previewGrid: false
    property bool previewGallery: false
    property string galleryPick: ""

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
            Gallery { pick: win.galleryPick }
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
    WeatherPage {
        id: page
        anchors.fill: parent
        visible: win.previewCard === "" && !win.previewGrid && !win.previewGallery
    }

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
        onTriggered: page.contentY = Math.min(target, page.maxContentY)
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

            switch (k) {
            case "metric":  page.metricId = v; break
            case "day":     page.dayIndex = parseInt(v); break
            case "list":    page.listView = on; break
            case "feels":   page.feelsLike = on; break
            case "scroll":  page.contentY = parseFloat(v); break
            // Negative velocity carries the content upward, i.e. scrolls down.
            case "flick":
                var vel = parseFloat(v)
                page.flickBy(-Math.abs(isNaN(vel) || vel === 0 ? 1400 : vel))
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
        var m = args.indexOf("--metric")
        if (m >= 0 && m + 1 < args.length)
            page.metricId = args[m + 1]

        var d = args.indexOf("--day")
        if (d >= 0 && d + 1 < args.length)
            page.dayIndex = parseInt(args[d + 1])

        if (args.indexOf("--list") >= 0)
            page.listView = true

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
            win.width = Math.max(win.width, 1500)
            win.height = Math.max(win.height, 950)
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

        // --size 1340x900, so a headless grab can be tall enough to hold the
        // whole grid. Reviewing a grid through a viewport that cuts off its
        // last row is how a misaligned card in that row stays unnoticed.
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
            grabTimer.target = args[i + 1]
            grabTimer.start()
        }
    }
}
