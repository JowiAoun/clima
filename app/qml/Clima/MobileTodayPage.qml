// SPDX-FileCopyrightText: 2026 Jowi Aoun
// SPDX-License-Identifier: GPL-3.0-or-later
// Today: the screen the app opens on.
//
// The order is an argument, the same one WeatherPage makes on the desktop and
// for the same reason: what is it doing now, what will it do today, what will
// it do this week, and then the things you look up rather than glance at.
//
//   location            where this is for
//   current weather     the headline, on the sky rather than on a card
//   today               the next 24 hours as a strip
//   10 day              the week and a half after that
//   sun & moon          what is overhead and for how long
//   pollen              a band and its three sources
//   health & activities five verdicts
//
// Two of the cards link onward — the hourly strip to the Hourly tab, the
// ten-day strip to Monthly. They are the two sections that are genuinely a
// preview of another screen, and the link says so rather than leaving the
// reader to find the tab. The other three have nowhere deeper to go and so
// carry no link; a header chevron that opens nothing is the affordance
// LocationBar's chevron note is about.
import QtQuick

MobilePage {
    id: root

    // Emitted when a card header's link asks for another tab. The shell
    // listens; the page has no idea a nav bar exists.
    signal navigate(string tabId)

    // Same shape, for the picker. The page cannot own the sheet: MobilePage is
    // a scrolling column inside a shell, and a panel parented into it would
    // scroll and would be clipped by the nav bar. So the request goes up and
    // the shell puts the sheet over everything, including the nav.
    signal pickerRequested()
    property bool pickerOpen: false

    Item {
        width: root.spanWidth(2)
        height: 30

        LocationBar {
            anchors.horizontalCenter: parent.horizontalCenter
            anchors.verticalCenter: parent.verticalCenter
            disclosed: root.pickerOpen
            onChangeRequested: root.pickerRequested()
            onHomeToggled: Engine.toggleHome(Engine.places.currentIndex)
        }
    }

    MobileCurrentWeather {
        width: root.spanWidth(2)
    }

    MobileCard {
        width: root.spanWidth(2)
        title: qsTr("Today")
        link: qsTr("Hourly")
        bleed: true
        onLinkActivated: root.navigate("hourly")
        content: MobileHourStrip { }
    }

    MobileCard {
        width: root.spanWidth(2)
        title: qsTr("10 Day")
        link: qsTr("Monthly")
        bleed: true
        onLinkActivated: root.navigate("monthly")
        content: MobileDailyStrip { }
    }

    MobileCard {
        width: root.spanWidth(1)
        title: qsTr("Sun & Moon")
        content: MobileSunMoonCard { }
    }

    // Pollen exists in Europe and nowhere else — CAMS produces it for its
    // European domain only — so outside that domain the card is ABSENT rather
    // than empty. `hasPollen` is Capability::Pollen at this coordinate, learned
    // from the payload rather than from a bounding box we typed in, and it is
    // false both where pollen is known-absent and where the answer is not yet
    // known. The second half is what stops the card popping in two seconds
    // after Berlin opens: undetermined draws nothing and then draws the card,
    // which is one appearance, not an appearance and a correction.
    MobileCard {
        visible: Engine.hasPollen
        width: root.spanWidth(1)
        title: qsTr("Pollen")
        content: MobilePollenCard { }
    }

    MobileCard {
        width: root.spanWidth(1)
        title: qsTr("Health & activities")
        content: MobileActivitiesCard { }
    }
}
