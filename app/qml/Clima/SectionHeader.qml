// SPDX-FileCopyrightText: 2026 Jowi Aoun
// SPDX-License-Identifier: GPL-3.0-or-later
// A heading over a section of the page: "Weather details", "Hourly".
//
// It exists so the page's sections cannot disagree about what a heading is.
// Before there was one, the hourly section set its title at 18 px and the
// details grid set its at 15 — invisible while each was the only thing on
// screen, and the first thing you see once they are stacked.
//
// The timestamp sits on the title's baseline rather than under it: it qualifies
// the heading ("details, as of 12:28") rather than introducing the section.
import QtQuick
import "theme.js" as Theme

Item {
    id: root

    property string title
    property string stamp: ""

    implicitWidth: titleText.width + (stamp === "" ? 0 : stampText.width + 10)
    implicitHeight: 26
    height: implicitHeight

    Text {
        id: titleText
        text: root.title
        color: Theme.color.textPrimary
        font.pixelSize: Theme.type.sectionTitle
        font.bold: true
        anchors.verticalCenter: parent.verticalCenter
    }

    Text {
        id: stampText
        text: root.stamp
        visible: root.stamp !== ""
        color: Theme.color.textMuted
        font.pixelSize: Theme.type.heroLabel
        anchors.left: titleText.right
        anchors.leftMargin: 10
        anchors.baseline: titleText.baseline
    }
}
