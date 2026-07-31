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
import "theme.js" as Theme

MobilePage {
    id: root

    // Emitted when a card header's link asks for another tab. The shell
    // listens; the page has no idea a nav bar exists.
    signal navigate(string tabId)

    Item {
        width: parent.width
        height: 30

        LocationBar {
            anchors.horizontalCenter: parent.horizontalCenter
            anchors.verticalCenter: parent.verticalCenter
        }
    }

    MobileCurrentWeather {
        width: parent.width
    }

    MobileCard {
        width: parent.width
        title: qsTr("Today")
        link: qsTr("Hourly")
        bleed: true
        onLinkActivated: root.navigate("hourly")
        content: MobileHourStrip { }
    }

    MobileCard {
        width: parent.width
        title: qsTr("10 Day")
        link: qsTr("Monthly")
        bleed: true
        onLinkActivated: root.navigate("monthly")
        content: MobileDailyStrip { }
    }

    MobileCard {
        width: parent.width
        title: qsTr("Sun & Moon")
        content: MobileSunMoonCard { }
    }

    MobileCard {
        width: parent.width
        title: qsTr("Pollen")
        content: MobilePollenCard { }
    }

    MobileCard {
        width: parent.width
        title: qsTr("Health & activities")
        content: MobileActivitiesCard { }
    }
}
