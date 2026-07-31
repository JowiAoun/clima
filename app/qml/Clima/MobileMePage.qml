// SPDX-FileCopyrightText: 2026 Jowi Aoun
// SPDX-License-Identifier: GPL-3.0-or-later
// Me: units, places, where the data comes from.
//
// The reference has no screenshot of this tab and the nav bar has five slots,
// so this is the one screen here that is a proposal rather than a rebuild.
// It is deliberately the smallest thing that makes the tab honest: the four
// settings this prototype's own data already implies, the attribution the
// licence requires us to show somewhere, and a line saying nothing on it is
// wired up.
//
// Not built, and listed here rather than left implied: the unit switch that
// re-renders every screen, the place picker LocationBar's chevron would open,
// and the provider chooser. All three are M2 and all three need state that
// lives above the shell.
import QtQuick
import "detaildata.js" as Detail

MobilePage {
    id: root

    // One row shape, three cards. A settings list is the one place where
    // writing the rows out longhand really would produce three different row
    // heights, which is what this component exists to prevent.
    component SettingRow: Item {
        id: settingRow

        property string label
        property string value
        property bool last: false

        width: parent ? parent.width : 0
        height: 42

        Text {
            text: settingRow.label
            color: Theme.color.textPrimary
            font.pixelSize: Theme.type.status
            anchors.left: parent.left
            anchors.verticalCenter: parent.verticalCenter
        }

        Text {
            text: settingRow.value
            color: Theme.color.textMuted
            font.pixelSize: Theme.type.status
            anchors.right: parent.right
            anchors.verticalCenter: parent.verticalCenter
        }

        Rectangle {
            visible: !settingRow.last
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.bottom: parent.bottom
            height: 1
            color: Theme.color.gridLineWeak
        }
    }

    Text {
        text: qsTr("Me")
        color: Theme.color.textPrimary
        font.pixelSize: Theme.type.sectionTitle
        font.bold: true
    }

    MobileCard {
        width: parent.width
        title: qsTr("Units")
        content: Column {
            SettingRow { label: qsTr("Temperature"); value: "°C" }
            SettingRow { label: qsTr("Wind");        value: Detail.wind.unit }
            SettingRow { label: qsTr("Pressure");    value: Detail.pressure.unit }
            SettingRow { label: qsTr("Visibility");  value: Detail.visibility.unit; last: true }
        }
    }

    MobileCard {
        width: parent.width
        title: qsTr("Places")
        content: Column {
            SettingRow {
                label: Detail.location.label
                value: Detail.location.isHome ? qsTr("Home") : ""
                last: true
            }
        }
    }

    MobileCard {
        width: parent.width
        title: qsTr("Data sources")
        content: Column {
            SettingRow { label: qsTr("Forecast");  value: "Open-Meteo" }
            SettingRow { label: qsTr("Fallback");  value: "MET Norway" }
            SettingRow { label: qsTr("Basemap");   value: "OpenStreetMap"; last: true }
        }
    }

    // Said outright rather than left for the reader to discover by tapping.
    Text {
        width: parent.width
        text: qsTr("Nothing on this screen is wired up. Units, places and "
                 + "providers are all read from the prototype's mock data.")
        color: Theme.color.textDim
        font.pixelSize: Theme.type.body
        wrapMode: Text.WordWrap
    }
}
