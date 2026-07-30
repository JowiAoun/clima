// SPDX-License-Identifier: GPL-3.0-or-later
// The day cards above the chart.
//
// The selected card behaves like a browser tab: it is lighter, wider, and *taller*
// than its neighbours, and its fill runs straight into the chart card below with no
// seam. That merge is what says "the chart underneath is showing this day".
//
// It is done by overhang rather than by drawing a join. The selected card extends
// `mergeDepth` past the bottom of the strip, and the chart card — declared after
// this in Main.qml, so painted over it — covers that overhang, taking the card's
// bottom border with it. Nothing has to line up to the pixel, and it stays correct
// at any card position or window size.
//
// The junction itself is filleted, not squared: a TabFillet sits in the gap either
// side of the raised card and curves its edge outward into the panel. Without them
// the card reads as pasted on top of the panel rather than growing out of it.
import QtQuick
import "theme.js" as Theme
import "mockdata.js" as Data

Item {
    id: root

    property int currentIndex: Data.todayIndex

    readonly property real cardWidth: 172
    readonly property real selectedExtra: 72     // room for the second badge
    readonly property real spacing: 14
    // The selected card used to overhang into the chart card, so the chart —
    // painted after it — would cover the card's bottom border. There is no
    // border to cover any more, and now that both surfaces are translucent an
    // overhang is actively wrong: the overlap would take the wash twice and
    // show as a lighter band across the junction. They abut instead.
    readonly property real mergeDepth: 0
    readonly property real unselectedInset: 20   // how much shorter the others are
    readonly property real filletRadius: Theme.metric.filletRadius

    implicitHeight: 130
    height: implicitHeight

    Flickable {
        id: flick
        anchors.fill: parent
        anchors.bottomMargin: -root.mergeDepth   // room for the overhang to draw into

        // clip bounds the cards; the layer bounds the condition glyphs, which are
        // Shapes and ignore ancestor clipping entirely (§10.8). Without it the
        // glyphs from cards scrolled out of view keep painting to the right of
        // the strip. Every window this ran in was narrow enough that they landed
        // off-window, so it looked fine until the page capped its content column
        // and left visible background beside it.
        clip: true
        layer.enabled: true
        contentWidth: row.width
        contentHeight: height
        flickableDirection: Flickable.HorizontalFlick
        boundsBehavior: Flickable.StopAtBounds

        NumberAnimation {
            id: scrollAnim
            target: flick
            property: "contentX"
            duration: 340
            easing.type: Easing.OutCubic
        }

        function scrollBy(dx) {
            var to = Math.max(0, Math.min(Math.max(0, contentWidth - width), contentX + dx))
            scrollAnim.stop()
            scrollAnim.from = contentX
            scrollAnim.to = to
            scrollAnim.start()
        }

        Row {
            id: row
            spacing: root.spacing
            height: flick.height

            Repeater {
                model: Data.days

                delegate: Item {
                    id: card

                    required property var modelData
                    required property int index

                    readonly property bool selected: index === root.currentIndex

                    width: selected ? root.cardWidth + root.selectedExtra : root.cardWidth
                    height: selected ? root.height + root.mergeDepth
                                     : root.height - root.unselectedInset
                    Behavior on width { NumberAnimation { duration: 190; easing.type: Easing.OutCubic } }
                    Behavior on height { NumberAnimation { duration: 190; easing.type: Easing.OutCubic } }

                    Rectangle {
                        anchors.fill: parent
                        color: card.selected ? Theme.color.cardBg : Theme.color.dayCardBg
                        // No outline on the raised card: it is one surface with the
                        // panel below, and an outline would draw a line across that.
                        border.width: card.selected ? 0 : 1
                        border.color: Theme.color.cardBorder
                        // Square at the bottom when selected: that edge is under the
                        // chart card and must not round away from it.
                        topLeftRadius: Theme.metric.cardRadius
                        topRightRadius: Theme.metric.cardRadius
                        bottomLeftRadius: card.selected ? 0 : Theme.metric.cardRadius
                        bottomRightRadius: card.selected ? 0 : Theme.metric.cardRadius
                        Behavior on color { ColorAnimation { duration: 160 } }
                    }

                    // The outward curves at the base of the raised card. They sit in
                    // the gaps either side of it, above the chart card's top edge.
                    TabFillet {
                        visible: card.selected
                        mirrored: false
                        filletRadius: root.filletRadius
                        extendBelow: root.mergeDepth
                        fillColor: Theme.color.cardBg
                        x: -root.filletRadius
                        y: root.height - root.filletRadius
                    }

                    TabFillet {
                        visible: card.selected
                        mirrored: true
                        filletRadius: root.filletRadius
                        extendBelow: root.mergeDepth
                        fillColor: Theme.color.cardBg
                        x: card.width
                        y: root.height - root.filletRadius
                    }

                    Text {
                        text: card.modelData.date
                        color: Theme.color.textPrimary
                        font.pixelSize: 15
                        font.bold: true
                        x: 16
                        y: 14
                    }

                    Text {
                        text: card.modelData.label
                        color: card.selected ? Theme.color.textPrimary : Theme.color.textMuted
                        font.pixelSize: 12
                        font.bold: card.selected
                        anchors.right: parent.right
                        anchors.rightMargin: 16
                        y: 16
                    }

                    // Selected: day and night conditions, each in a badge.
                    // Unselected: the daytime glyph alone.
                    Row {
                        spacing: 8
                        anchors.left: parent.left
                        anchors.leftMargin: 16
                        y: 46

                        DayIconBadge {
                            visible: card.selected
                            width: visible ? badgeSize : 0
                            kind: card.modelData.icon
                            badgeSize: 50
                        }
                        DayIconBadge {
                            visible: card.selected
                            width: visible ? badgeSize : 0
                            kind: card.modelData.nightIcon
                            night: true
                            badgeSize: 50
                        }
                        WeatherGlyph {
                            visible: !card.selected
                            width: visible ? glyphSize : 0
                            kind: card.modelData.icon
                            glyphSize: 44
                        }
                    }

                    Column {
                        spacing: 2
                        anchors.right: parent.right
                        anchors.rightMargin: 16
                        y: 56

                        Text {
                            text: card.modelData.high + "°"
                            color: Theme.color.textPrimary
                            font.pixelSize: 17
                            font.bold: true
                            anchors.right: parent.right
                        }
                        Text {
                            text: card.modelData.low + "°"
                            color: Theme.color.textMuted
                            font.pixelSize: 15
                            anchors.right: parent.right
                        }
                    }

                    HoverHandler { cursorShape: Qt.PointingHandCursor }
                    TapHandler { onTapped: root.currentIndex = card.index }
                }
            }
        }
    }

    PagerButton {
        pointsLeft: true
        enabledState: flick.contentX > 1
        opacity: enabledState ? 1 : 0
        anchors.left: parent.left
        y: (root.height - root.unselectedInset - height) / 2
        onActivated: flick.scrollBy(-flick.width * 0.7)
    }

    PagerButton {
        pointsLeft: false
        enabledState: flick.contentX < flick.contentWidth - flick.width - 1
        opacity: enabledState ? 1 : 0
        anchors.right: parent.right
        y: (root.height - root.unselectedInset - height) / 2
        onActivated: flick.scrollBy(flick.width * 0.7)
    }
}
