// SPDX-FileCopyrightText: 2026 Jowi Aoun
// SPDX-License-Identifier: GPL-3.0-or-later
//
// The three ways of looking at a piece of the app without the rest of the app
// in the way: one detail card, the details grid, or the component library.
//
// They live together in one file because they leave together. `--gallery`,
// `--card` and `--details` are a development tool that happens to be reachable
// from the product's binary, and the next step is a `clima-gallery` executable
// that owns all three — at which point this file moves and Main.qml loses four
// lines rather than fifty.
//
// Until then it stays wired up and working, because a tool that stops working
// during the refactor that was going to improve it is a tool nobody trusts
// afterwards.
import QtQuick
import "sky.js" as Sky
import "detaildata.js" as Detail

Item {
    id: root

    // Whether anything here is showing, which is also the answer to "should the
    // real shell be running". Exactly one of the two is ever on screen.
    readonly property bool active: AppOptions.card !== ""
                                   || AppOptions.details
                                   || AppOptions.gallery

    // The Gallery instance, or null. ScreenshotController reaches for it to
    // step through components (`--walk`) and to rebuild a specimen
    // (`--poke remount`); nothing else should.
    //
    // `as Item` because Loader.item is declared QObject and qmllint will not
    // narrow it for you. Without the assertion this is an incompatible-type
    // warning on a line that is perfectly correct at runtime, and a lint that
    // warns about correct code is a lint people stop reading.
    readonly property Item gallery: galleryLoader.item as Item

    // `--card Uv` renders one detail card on the page gradient and nothing
    // else, so a card can be built and checked without the rest of the screen
    // in the way.
    Loader {
        active: AppOptions.card !== ""
        anchors.centerIn: parent
        sourceComponent: Component {
            Loader {
                source: "Detail" + AppOptions.card + "Card.qml"
                onStatusChanged: if (status === Loader.Error)
                    console.warn("preview: no such card —", source)
            }
        }
    }

    // The grid lays out in full and does not scroll itself — the page owns
    // that. Shown on its own it still needs somewhere to scroll and something
    // to bound its cards' Shapes, so the preview supplies both.
    Loader {
        active: AppOptions.details
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
        active: AppOptions.gallery
        anchors.fill: parent
        sourceComponent: Component {
            Gallery {
                pick: AppOptions.galleryPick

                // `--viewport` means something different in here, and it has
                // to: resizing the window to 390 px would leave 158 px of stage
                // beside a 232 px rail. The gallery frames its specimen at the
                // preset instead and keeps its own window.
                viewport: AppOptions.viewport

                // Whatever the app would be showing at this hour, so a
                // component framed as a phone is reviewed on the sky the phone
                // would actually give it — stars included. Not Main's skyPhase,
                // which is `dusk` on a desktop-width window.
                skyPhase: AppOptions.sky !== ""
                          ? AppOptions.sky
                          : Sky.phaseAt(Detail.sun.nowMin, Detail.sun.riseMin,
                                        Detail.sun.setMin)
            }
        }
    }
}
