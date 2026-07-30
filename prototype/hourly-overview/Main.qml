// SPDX-License-Identifier: GPL-3.0-or-later
import QtQuick
import QtQuick.Window
import "theme.js" as Theme

Window {
    id: win

    visible: true
    width: 1328
    height: 560
    minimumWidth: 640
    minimumHeight: 460
    color: Theme.color.pageBg
    title: qsTr("Clima — Hourly overview (prototype)")

    HourlyOverview {
        anchors.fill: parent
        anchors.margins: 22
    }

    // Headless capture, for design review and CI golden images:
    //   qml Main.qml -- --grab shot.png
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
        var i = args.indexOf("--grab")
        if (i >= 0 && i + 1 < args.length) {
            grabTimer.target = args[i + 1]
            grabTimer.start()
        }
    }
}
