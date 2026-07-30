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
    title: qsTr("Clima — Hourly (prototype)")

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

    Item {
        anchors.fill: parent
        anchors.margins: 22

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

        // Declared after the day strip on purpose: it paints over the selected
        // card's overhang, which is what makes the two read as one surface.
        HourlyOverview {
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.top: dayStrip.bottom
            anchors.bottom: parent.bottom
            metricId: tabs.currentId
            listView: tabs.listView
        }
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
            tabs.currentId = args[m + 1]

        var d = args.indexOf("--day")
        if (d >= 0 && d + 1 < args.length)
            dayStrip.currentIndex = parseInt(args[d + 1])

        if (args.indexOf("--list") >= 0)
            tabs.listView = true

        var i = args.indexOf("--grab")
        if (i >= 0 && i + 1 < args.length) {
            grabTimer.target = args[i + 1]
            grabTimer.start()
        }
    }
}
