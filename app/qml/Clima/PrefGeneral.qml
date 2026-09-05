// SPDX-FileCopyrightText: 2026 Jowi Aoun
// SPDX-License-Identifier: GPL-3.0-or-later
// The general preferences: how the app looks, and how it writes a time.
//
// One definition, two places. The desktop opens it in PreferencesSheet and the
// phone shows it on the Me tab, and they are the same object rather than two
// screens kept in step — which is the failure this group exists to prevent. The
// units below it are PrefUnits, split out for the same reason MobileMePage's
// cards are separate: on a tablet they are two half-width cards side by side,
// and one file could only ever be one card.
//
// ---- every row here is bound, not stored ------------------------------------
//
// `checked: Settings.x`, `onToggled: Settings.x = !Settings.x`. The control
// never holds the value. That is what makes the metric/imperial preset in
// PrefUnits work — it writes five preferences, and every row bound to one of
// them redraws — and it is why PrefSwitch does not toggle itself. See its header.
import QtQuick

PrefGroup {
    id: root

    title: qsTr("General")

    // ---- the sky ------------------------------------------------------------
    //
    // What this switches is stated in the subtitle rather than left to the word
    // "dynamic", because the reader cannot see the alternative. The reference
    // app this row is modelled on says "App background changes based on current
    // weather conditions (restart required)", which is wrong about Clima twice:
    // the gradient follows the *hour*, not the conditions, and it changes as
    // soon as it is switched because Main.qml binds it rather than reading it at
    // startup. Saying "restart required" when none is would be copying a
    // limitation along with the layout.
    PrefRow {
        title: qsTr("Dynamic background")
        subtitle: qsTr("The page follows the sky over the place on screen — "
                     + "night, dawn, day and dusk. Off holds it at one palette.")
        control: PrefSwitch {
            checked: Settings.dynamicBackground
            onToggled: Settings.dynamicBackground = !Settings.dynamicBackground
        }
        onActivated: Settings.dynamicBackground = !Settings.dynamicBackground
    }

    // ---- light or dark ------------------------------------------------------
    //
    // Three positions, because "system" is a real answer and not the absence of
    // one: it means follow the desktop, which Clima does live over
    // org.freedesktop.portal.Settings. A two-position control would make that
    // reachable only by deleting a line from an INI file.
    //
    // The subtitle says what "system" resolved to, and says so only when it is
    // selected — on Light or Dark the resolved scheme and the choice are the
    // same fact, and printing it twice is noise. When nothing answered, it says
    // that instead of reporting a preference it never received.
    PrefRow {
        title: qsTr("Theme")
        subtitle: {
            if (AppOptions.scheme !== "")
                //: %1 is "light" or "dark"; --scheme is a command-line flag
                return qsTr("Forced to %1 by --scheme for this run.").arg(AppOptions.scheme)
            if (Settings.appearance !== "system")
                return ""
            if (!SystemAppearance.available)
                return qsTr("The desktop has no colour-scheme preference to follow.")
            return SystemAppearance.colorScheme === "light"
                   ? qsTr("Following the desktop, which is light.")
                   : qsTr("Following the desktop, which is dark.")
        }
        control: PrefSegment {
            options: [{ id: "system", label: qsTr("System") },
                      { id: "light",  label: qsTr("Light") },
                      { id: "dark",   label: qsTr("Dark") }]
            currentId: Settings.appearance
            onSelected: function (id) { Settings.appearance = id }
        }
        // No `onActivated`: a segment has three positions and a row-wide tap
        // cannot mean one of them. `interactive: false` is what takes the hover
        // wash and the pointing cursor away, so the row does not promise a press
        // that would do nothing.
        interactive: false
    }

    // ---- the clock ----------------------------------------------------------
    //
    // The ids are Settings' own spellings, so nothing here translates between a
    // label and a stored value. app/viewmodels/timeformat.h is the only thing
    // that reads the key, and it reaches every clock in the app and in the
    // widgets — the hour axis, the hourly list, the observation stamp, both sun
    // and moon readings, the nine body sentences and the alert banner.
    // ---- the interruption ---------------------------------------------------
    //
    // Hidden entirely where there is nothing to post to — a build with no Qt
    // D-Bus, or a session with no bus — because a switch that cannot do
    // anything is worse than an absent one: it teaches the reader that the
    // preferences lie. `Engine.notificationsAvailable` is the same question
    // Notifier::available() answers.
    //
    // The subtitle says what it costs, because it is the only preference here
    // that changes what the app does when nobody is looking at it. A reader
    // who is on a metered connection deserves to know that before they tap.
    PrefRow {
        title: qsTr("Warn me about severe weather")
        subtitle: qsTr("A desktop notification when a warning arrives and Clima is not "
                       + "on screen. Checks every 15 minutes while it is hidden.")
        visible: Engine.notificationsAvailable
        height: visible ? implicitHeight : 0
        control: PrefSwitch {
            checked: Settings.alertNotifications
            onToggled: Settings.alertNotifications = !Settings.alertNotifications
        }
        onActivated: Settings.alertNotifications = !Settings.alertNotifications
    }

    PrefRow {
        title: qsTr("Time format")
        subtitle: qsTr("Used everywhere a time appears, including the desktop widgets.")
        control: PrefSegment {
            options: [{ id: "24h", label: qsTr("24 hour") },
                      { id: "12h", label: qsTr("AM / PM") }]
            currentId: Settings.clockFormat
            onSelected: function (id) { Settings.clockFormat = id }
        }
        interactive: false
    }
}
