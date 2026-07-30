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

    // Headless capture, for design review and CI golden images:
    //   qml Main.qml -- --grab shot.png [--metric wind]
    Timer {
        id: grabTimer
        interval: 1200
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

        var i = args.indexOf("--grab")
        if (i >= 0 && i + 1 < args.length) {
            grabTimer.target = args[i + 1]
            grabTimer.start()
        }
    }
}
