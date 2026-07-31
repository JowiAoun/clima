// SPDX-FileCopyrightText: 2026 Jowi Aoun
// SPDX-License-Identifier: GPL-3.0-or-later
// Me: units, places, where the data comes from.
//
// The reference has no screenshot of this tab and the nav bar has five slots,
// so this is the one screen here that is a proposal rather than a rebuild.
//
// It used to end with a line saying "nothing on this screen is wired up", and
// listed three things as not built: the unit switch that re-renders every
// screen, the place picker LocationBar's chevron would open, and the provider
// chooser. The first two are here. The third is not, and is deliberately not
// listed as missing any more — a provider chooser is a feature for somebody who
// already knows which model they prefer, and the fallback chain means the app
// answers that question for everybody else.
//
// ---- the units rows are the settings, not a picture of them -----------------
// Tapping one cycles it. A cycle rather than a menu because there are two
// temperature units and five wind units, and a picker for two options is a
// dialog nobody wanted; the current value is on the row, so the cycle shows its
// result rather than hiding it behind a sheet. Everything downstream — the
// hero, the chart axis, the hourly list, the day strip — is bound to the same
// Units singleton, so the whole app changes on the tap.
//
// ---- the data sources card is GENERATED -------------------------------------
// docs/08-risks.md R12 is "a new provider gets added without its credit", and
// its mitigation is that this card comes out of the provider registry. It does:
// `Engine.sources` is ProviderRegistry::attributions(), and the registry
// REFUSES to hold a provider whose Attribution is incomplete — an uncredited
// provider is not added, and never added means its data cannot reach this
// screen either.
//
// So there is nothing to keep in step here. Registering MET Norway put MET
// Norway on this card; registering the fixture provider put the recording on
// it, saying which afternoon it is.
import QtQuick

MobilePage {
    id: root

    // One row shape, three cards. A settings list is the one place where
    // writing the rows out longhand really would produce three different row
    // heights, which is what this component exists to prevent.
    component SettingRow: Item {
        id: settingRow

        property string text
        property string value
        property bool last: false
        property bool tappable: false

        signal activated()

        width: parent ? parent.width : 0
        height: 42

        Rectangle {
            anchors.fill: parent
            anchors.leftMargin: -8
            anchors.rightMargin: -8
            radius: Theme.metric.controlRadius
            color: rowHover.hovered ? Theme.surface.raised : "transparent"
            Behavior on color {
                ColorAnimation { duration: Theme.motion.tint; easing.type: Easing.OutCubic }
            }
        }

        Text {
            text: settingRow.text
            color: Theme.ink.primary
            font.pixelSize: Theme.type.status
            anchors.left: parent.left
            anchors.verticalCenter: parent.verticalCenter
        }

        Text {
            text: settingRow.value
            color: Theme.ink.muted
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
            color: Theme.line.gridWeak
        }

        HoverHandler {
            id: rowHover
            enabled: settingRow.tappable
            cursorShape: Qt.PointingHandCursor
        }
        TapHandler {
            enabled: settingRow.tappable
            onTapped: settingRow.activated()
        }
    }

    // The next option in a quantity's list, wrapping. The list is Units' — there
    // is no second copy of "which units exist" anywhere in the QML, which is
    // what stops a row offering a unit nothing can convert to.
    function cycle(quantity, current) {
        var options = Units.choicesFor(quantity)
        for (var i = 0; i < options.length; ++i)
            if (options[i].id === current)
                return options[(i + 1) % options.length].id
        return options.length > 0 ? options[0].id : current
    }

    Text {
        text: qsTr("Me")
        color: Theme.ink.primary
        font.pixelSize: Theme.type.sectionTitle
        font.bold: true
    }

    MobileCard {
        width: parent.width
        title: qsTr("Units")
        content: Column {
            SettingRow {
                text: qsTr("Temperature")
                value: Units.bareSymbol(Units.Temperature)
                tappable: true
                onActivated: Settings.temperatureUnit =
                    root.cycle(Units.Temperature, Settings.temperatureUnit)
            }
            SettingRow {
                text: qsTr("Wind")
                value: Units.bareSymbol(Units.Wind)
                tappable: true
                onActivated: Settings.windUnit = root.cycle(Units.Wind, Settings.windUnit)
            }
            SettingRow {
                text: qsTr("Pressure")
                value: Units.bareSymbol(Units.Pressure)
                tappable: true
                onActivated: Settings.pressureUnit =
                    root.cycle(Units.Pressure, Settings.pressureUnit)
            }
            SettingRow {
                text: qsTr("Precipitation")
                value: Units.bareSymbol(Units.Precipitation)
                tappable: true
                onActivated: Settings.precipitationUnit =
                    root.cycle(Units.Precipitation, Settings.precipitationUnit)
            }
            SettingRow {
                text: qsTr("Visibility")
                value: Units.bareSymbol(Units.Visibility)
                tappable: true
                last: true
                onActivated: Settings.visibilityUnit =
                    root.cycle(Units.Visibility, Settings.visibilityUnit)
            }
        }
    }

    MobileCard {
        width: parent.width
        title: qsTr("Places")
        content: Column {
            Repeater {
                model: Engine.places

                delegate: SettingRow {
                    required property int index
                    required property string label
                    required property bool isHome

                    text: label
                    value: isHome ? qsTr("Home") : ""
                    last: index === Engine.places.count - 1
                    tappable: true
                    onActivated: Engine.selectPlace(index)
                }
            }
        }
    }

    MobileCard {
        width: parent.width
        title: qsTr("Data sources")
        content: Column {
            spacing: 12

            Repeater {
                model: Engine.sources

                delegate: Column {
                    required property var modelData

                    width: parent ? parent.width : 0
                    spacing: 2

                    Text {
                        // The exact sentence the licence asks for, not a
                        // paraphrase built from the provider's name. Open-Meteo
                        // wants "Weather data by Open-Meteo.com"; ECCC's
                        // required wording, when that provider lands, is a
                        // sentence nobody would guess.
                        text: modelData.creditLine
                        color: Theme.ink.primary
                        font.pixelSize: Theme.type.status
                        width: parent.width
                        wrapMode: Text.WordWrap
                    }

                    Text {
                        text: modelData.licenceName + "  ·  " + modelData.homepage
                        color: Theme.ink.muted
                        font.pixelSize: Theme.type.label
                        width: parent.width
                        elide: Text.ElideRight
                    }

                    Text {
                        // §2.9 requires the model owners behind an aggregator to
                        // be named separately, which is the part a paraphrase
                        // would drop.
                        visible: modelData.upstream.length > 0
                        text: qsTr("Models: ") + modelData.upstream.join(", ")
                        color: Theme.ink.dim
                        font.pixelSize: Theme.type.label
                        width: parent.width
                        wrapMode: Text.WordWrap
                    }

                    Text {
                        visible: modelData.note !== ""
                        text: modelData.note
                        color: Theme.ink.dim
                        font.pixelSize: Theme.type.label
                        width: parent.width
                        wrapMode: Text.WordWrap
                    }
                }
            }
        }
    }

    // What this run is doing, said outright. Under `--fixture` every time on
    // screen is a recorded afternoon's, and a reviewer holding a screenshot
    // deserves to be told that by the screenshot rather than by the command
    // that produced it.
    Text {
        width: parent.width
        visible: Engine.fixtureMode
        text: qsTr("Showing the recorded “%1” fixture at its frozen clock. "
                 + "No network request was made for this forecast.").arg(Engine.fixtureName)
        color: Theme.ink.dim
        font.pixelSize: Theme.type.body
        wrapMode: Text.WordWrap
    }
}
