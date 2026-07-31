// SPDX-FileCopyrightText: 2026 Jowi Aoun
// SPDX-License-Identifier: GPL-3.0-or-later
// Monthly: the whole month at a glance.
//
// The reference puts a month picker beside the title. This does not, and the
// reason is the one LocationBar states about its chevron: there is one month
// of data behind this screen, so a picker here would be a control that opens a
// list with one thing in it, or worse, changes the title and not the numbers.
// When the provider arrives it is a dropdown next to the heading and a
// `monthDays` that takes an argument.
import QtQuick

MobilePage {
    id: root

    Text {
        text: Data.month.name + " " + Data.month.year
        color: Theme.ink.primary
        font.pixelSize: Theme.type.sectionTitle
        font.bold: true
    }

    MobileCard {
        width: parent.width
        content: MobileCalendar { }
    }
}
