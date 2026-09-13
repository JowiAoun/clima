// SPDX-FileCopyrightText: 2026 Jowi Aoun
// SPDX-License-Identifier: GPL-3.0-or-later
// Units and measurements: the two presets, then the five preferences they are
// a shortcut for.
//
// ---- why both halves are on the screen ----------------------------------------
//
// docs/04-architecture.md §4.10 and app/settings.h are emphatic that units in
// this app are per quantity and that there is no metric/imperial switch: people
// want °C with mph, or inHg with mm, and an app offering two bundles cannot
// express either. That is right, and on its own it produces a settings screen
// where a reader who wants Fahrenheit has to change five rows and know that
// inHg goes with miles.
//
// So both. The presets write the five at once and the five rows underneath are
// still the truth — change one and the preset reads "custom", which is a state
// the radio draws by filling neither dot. Nothing here can express a preference
// the per-quantity model cannot hold, and nothing is unreachable.
//
// The reference screen this is modelled on arrives at the same place from the
// other direction: it offers °C or °F, and then a separate "precipitation in
// inches" switch that overrides it. That switch is an admission that the bundle
// is not enough, and it is the second row of this group for exactly that reason
// — it is the override people actually reach for, promoted out of the five.
//
// ---- the lists are Units', not this file's ------------------------------------
//
// `Units.systemChoices()` and `Units.choicesFor()` both come from C++, and both
// exist so that what a reader can pick and what the program can convert are one
// list. A hardcoded °F here would be a unit this screen offers and nothing
// downstream understands.
//
// `Bound` because the preset rows are a Repeater delegate whose radio dot reads
// the delegate's own model role from inside a Component — a scope of its own,
// where an unqualified lookup is what stops qmlcachegen compiling the binding
// ahead of time.
pragma ComponentBehavior: Bound

import QtQuick

PrefGroup {
    id: root

    title: qsTr("Units & measurements")

    // The next option in a quantity's list, wrapping. Same helper MobileMePage
    // carried, moved here with the rows that use it.
    function cycle(quantity, current) {
        var options = Units.choicesFor(quantity)
        for (var i = 0; i < options.length; ++i)
            if (options[i].id === current)
                return options[(i + 1) % options.length].id
        return options.length > 0 ? options[0].id : current
    }

    // A dot rather than a check: a radio in a group of two mutually exclusive
    // options, which is what this is, and a check mark would say the two could
    // both be on. Drawn with two Rectangles instead of a Shape because it is a
    // circle inside a circle — a Shape here would be a scene-graph node and an
    // offscreen pass to draw eleven pixels.
    component RadioDot: Item {
        id: dot

        property bool selected: false

        // Implicit as well as actual, because PrefRow measures the slot's item
        // by its implicit size — a Loader whose item states only `width` reports
        // zero, and the title then starts underneath the dot.
        implicitWidth: 18
        implicitHeight: 18
        width: implicitWidth
        height: implicitHeight

        Rectangle {
            anchors.fill: parent
            radius: width / 2
            color: dot.selected ? Theme.accent.fill : "transparent"
            border.width: dot.selected ? 0 : 1
            border.color: Theme.line.control

            Behavior on color {
                ColorAnimation { duration: Theme.motion.tint; easing.type: Easing.OutCubic }
            }
        }

        Rectangle {
            anchors.centerIn: parent
            width: 7
            height: 7
            radius: width / 2
            visible: dot.selected
            color: Theme.accent.ink
        }
    }

    // ---- the two presets ----------------------------------------------------
    Repeater {
        model: Units.systemChoices()

        delegate: PrefRow {
            id: preset
            required property var modelData

            title: preset.modelData.label
            subtitle: preset.modelData.blurb
            leading: RadioDot { selected: Units.system === preset.modelData.id }
            onActivated: Units.applySystem(preset.modelData.id)
        }
    }

    // ---- the override that is worth promoting -------------------------------
    //
    // Precipitation, on its own, above the other four. Not because it is more
    // important than wind but because it is the one quantity people mix: a
    // reader on Celsius who reads rainfall in inches is common enough that the
    // reference screen has this exact switch, and reaching it through the row
    // below would mean cycling a control whose other position they do not want.
    //
    // Switching it on when the preset is metric is what makes `Units.system`
    // answer "custom", and both radios above go empty. That is the model being
    // honest rather than a glitch: the units are no longer either bundle.
    PrefRow {
        title: qsTr("Precipitation in inches")
        subtitle: qsTr("Rain and snow depth in inches rather than millimetres.")
        control: PrefSwitch {
            checked: Units.precipitation === "in"
            onToggled: Settings.precipitationUnit =
                Units.precipitation === "in" ? "mm" : "in"
        }
        onActivated: Settings.precipitationUnit = Units.precipitation === "in" ? "mm" : "in"
    }

    // ---- the five, individually ---------------------------------------------
    //
    // Tapping one cycles it, which is the control MobileMePage established and
    // the right one here: there are five wind units, and a picker for a row a
    // reader touches twice a year is a sheet nobody wanted. The current value is
    // on the row, so the cycle shows its result rather than hiding it.
    //
    // No subtitles. These are the rows where the title genuinely says everything
    // — "Wind — km/h" needs no sentence — and PrefRow's compact shape is for
    // exactly this case.
    PrefRow {
        title: qsTr("Temperature")
        control: Text {
            text: Units.bareSymbol(Units.Temperature)
            color: Theme.ink.muted
            font.pixelSize: Theme.type.status
        }
        onActivated: Settings.temperatureUnit =
            root.cycle(Units.Temperature, Settings.temperatureUnit)
    }

    PrefRow {
        title: qsTr("Wind")
        control: Text {
            text: Units.bareSymbol(Units.Wind)
            color: Theme.ink.muted
            font.pixelSize: Theme.type.status
        }
        onActivated: Settings.windUnit = root.cycle(Units.Wind, Settings.windUnit)
    }

    PrefRow {
        title: qsTr("Pressure")
        control: Text {
            text: Units.bareSymbol(Units.Pressure)
            color: Theme.ink.muted
            font.pixelSize: Theme.type.status
        }
        onActivated: Settings.pressureUnit =
            root.cycle(Units.Pressure, Settings.pressureUnit)
    }

    PrefRow {
        title: qsTr("Precipitation")
        control: Text {
            text: Units.bareSymbol(Units.Precipitation)
            color: Theme.ink.muted
            font.pixelSize: Theme.type.status
        }
        onActivated: Settings.precipitationUnit =
            root.cycle(Units.Precipitation, Settings.precipitationUnit)
    }

    PrefRow {
        title: qsTr("Visibility")
        control: Text {
            text: Units.bareSymbol(Units.Visibility)
            color: Theme.ink.muted
            font.pixelSize: Theme.type.status
        }
        onActivated: Settings.visibilityUnit =
            root.cycle(Units.Visibility, Settings.visibilityUnit)
    }
}
