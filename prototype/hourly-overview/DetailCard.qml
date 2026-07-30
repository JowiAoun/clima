// SPDX-License-Identifier: GPL-3.0-or-later
// The shell every weather-detail card is built in.
//
// Twelve cards share one anatomy: a quiet title, a visualisation that carries
// the reading, a bold status line, and a sentence of context. Only the
// visualisation differs. Putting the other four in one place is what keeps
// twelve independently-written cards looking like one set — the alternative is
// twelve slightly different paddings and four different title sizes.
//
// Fill the `content` slot with the visualisation and nothing else. It is given
// `contentWidth` x `contentHeight` to work in and should not reach outside
// them; the card owns its own padding.
//
//   DetailCard {
//       title: "UV"
//       status: "High"
//       trend: "up"
//       body: "Maximum UV exposure today will be high, expected at 2:00 p.m."
//       content: MyVisualisation { anchors.fill: parent }
//   }
import QtQuick
import "theme.js" as Theme

Item {
    id: root

    property string title
    property string status
    property string body
    property string trend: "none"        // "up" | "down" | "steady" | "none"

    // The visualisation. Anchor it to fill; it is already the right size.
    property Component content

    readonly property real contentWidth: card.width - Theme.metric.detailPadH * 2
    readonly property real contentHeight: contentArea.height

    implicitWidth: Theme.metric.detailCardWidth
    implicitHeight: Theme.metric.detailCardHeight
    width: implicitWidth
    height: implicitHeight

    Rectangle {
        id: card
        anchors.fill: parent
        radius: Theme.metric.detailRadius
        color: Theme.color.surfaceBase
    }

    Text {
        id: titleText
        text: root.title
        color: Theme.color.textPrimary
        font.pixelSize: 14
        x: Theme.metric.detailPadH
        y: Theme.metric.detailPadV
        width: root.contentWidth
        elide: Text.ElideRight
    }

    // The visualisation gets whatever is left once the four fixed rows are
    // accounted for, so every card's chart region is the same height and the
    // grid reads as a grid.
    Item {
        id: contentArea
        x: Theme.metric.detailPadH
        y: titleText.y + titleText.height + 10
        width: root.contentWidth
        height: statusRow.y - y - 10

        Loader {
            anchors.fill: parent
            sourceComponent: root.content
        }
    }

    Row {
        id: statusRow
        spacing: 6
        x: Theme.metric.detailPadH
        y: root.height - Theme.metric.detailPadV - bodyText.height - height - 4

        Text {
            text: root.status
            color: Theme.color.textPrimary
            font.pixelSize: 14
            font.bold: true
        }

        TrendBadge {
            direction: root.trend
            anchors.verticalCenter: parent.verticalCenter
        }
    }

    Text {
        id: bodyText
        text: root.body
        color: Theme.color.textMuted
        font.pixelSize: 12
        lineHeight: 1.25
        wrapMode: Text.WordWrap
        maximumLineCount: 2
        elide: Text.ElideRight
        x: Theme.metric.detailPadH
        y: root.height - Theme.metric.detailPadV - height
        width: root.contentWidth
    }
}
