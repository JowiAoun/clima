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

        HourlyOverview {
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.top: dayStrip.bottom
            anchors.topMargin: -1        // meet the selected day card exactly
            anchors.bottom: parent.bottom
            metricId: tabs.currentId
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

        var i = args.indexOf("--grab")
        if (i >= 0 && i + 1 < args.length) {
            grabTimer.target = args[i + 1]
            grabTimer.start()
        }
    }
}
