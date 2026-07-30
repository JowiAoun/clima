// SPDX-License-Identifier: GPL-3.0-or-later
// The list alternative to the chart.
//
// A chart answers "what is the shape of the day"; a list answers "what exactly is
// it at 3pm". Both are worth having, which is why the reference carries a switch
// for it — and why leaving that switch inert was the wrong place to stop.
//
// The past is dimmed rather than hidden, and "now" is marked, so the same rule the
// chart follows holds here: observed hours are real data, just not forecast.
//
// ---- motion -----------------------------------------------------------------
// One animation: the row under the pointer tints. Everything else here is
// deliberately still, and the reasons are worth writing down because "add an
// arrival" is the obvious thing to reach for and every version of it is wrong:
//
//   * A staggered row reveal delays the one thing the reader just asked for.
//     They pressed "List" to read 3 AM's numbers; making them watch 48 rows
//     arrive is charging admission for data that was already on screen.
//   * It would fire on scroll. ListView builds delegates as they come into
//     view — `cacheBuffer: 0`, so exactly as they come into view — and §10.6
//     forbids a reveal that re-triggers, "nothing fires on scrolling into
//     view" in particular. A per-delegate animation is that bug by
//     construction, not by accident.
//   * It would replay on every toggle. `HourlyOverview` loads this file with
//     `active: root.listView`, so the whole list is rebuilt each time the
//     switch is flipped, and an on-create reveal replays chart→list→chart.
//   * Every cell in here is text, and §10.6 says text does not fly, fade or
//     slide.
//
// The list also has no state to transition between: it is metric-agnostic and
// day-agnostic, `nowIndex` is fixed for the life of the process, and the past
// dimming never changes. Arrival is the switch's motion and the switch belongs
// to `HourlyOverview`, which is the only place that can sequence it against the
// chart underneath.
import QtQuick
import "theme.js" as Theme
import "mockdata.js" as Data

Item {
    id: root

    readonly property real rowHeight: 42

    Rectangle {
        anchors.fill: parent
        radius: Theme.metric.panelRadius
        color: Theme.color.panelBg
    }

    // Column geometry lives in one place so the header and the rows cannot drift.
    QtObject {
        id: cols
        readonly property real time: 92
        readonly property real icon: 40
        readonly property real condition: 168
        readonly property real temp: 74
        readonly property real feels: 84
        readonly property real precip: 92
        readonly property real wind: 104
        readonly property real humidity: 84
    }

    component Cell: Text {
        property real cellWidth: 80
        width: cellWidth
        color: Theme.color.textPrimary
        font.pixelSize: 12
        horizontalAlignment: Text.AlignRight
        verticalAlignment: Text.AlignVCenter
        height: parent ? parent.height : 0
    }

    component HeaderCell: Text {
        property real cellWidth: 80
        width: cellWidth
        color: Theme.color.textDim
        font.pixelSize: 11
        horizontalAlignment: Text.AlignRight
        verticalAlignment: Text.AlignVCenter
        height: parent ? parent.height : 0
    }

    // ---- header ----------------------------------------------------------
    Item {
        id: header
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: parent.top
        anchors.leftMargin: 18
        anchors.rightMargin: 18
        height: 34

        Row {
            anchors.fill: parent
            spacing: 12

            HeaderCell {
                cellWidth: cols.time + cols.icon + 12
                text: qsTr("Time")
                horizontalAlignment: Text.AlignLeft
            }
            HeaderCell { cellWidth: cols.condition; text: qsTr("Condition"); horizontalAlignment: Text.AlignLeft }
            HeaderCell { cellWidth: cols.temp;      text: qsTr("Temp") }
            HeaderCell { cellWidth: cols.feels;     text: qsTr("Feels like") }
            HeaderCell { cellWidth: cols.precip;    text: qsTr("Precip") }
            HeaderCell { cellWidth: cols.wind;      text: qsTr("Wind") }
            HeaderCell { cellWidth: cols.humidity;  text: qsTr("Humidity") }
        }

        Rectangle {
            anchors.bottom: parent.bottom
            anchors.left: parent.left
            anchors.right: parent.right
            height: 1
            color: Theme.color.gridLine
        }
    }

    // ---- rows ------------------------------------------------------------
    ListView {
        id: view
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: header.bottom
        anchors.bottom: parent.bottom
        anchors.leftMargin: 18
        anchors.rightMargin: 18
        anchors.bottomMargin: 10
        clip: true
        // Qt Quick Shapes escape ancestor clipping: the condition glyphs of
        // out-of-view rows were drawing over the header and past the bottom edge.
        // A layer bounds the whole subtree, which plain clip: true does not.
        layer.enabled: true
        cacheBuffer: 0
        model: Data.count
        boundsBehavior: Flickable.StopAtBounds
        currentIndex: Data.nowIndex

        // Open on "now", explicitly.
        //
        // `currentIndex` alone does not decide where the view rests: nothing is
        // bound to it — the now row draws off `index === Data.nowIndex`, not off
        // being current — so all it does is make Qt track that delegate, and
        // where tracking lands depends on the view's height when the delegate
        // happened to be created. That is why the same list opened on 9 PM
        // inside the page and on "now" in the gallery. Positioning explicitly
        // makes it the same list in both, and "now" is the answer: the past is
        // dimmed context you can scroll back to, not the thing you came for.
        Component.onCompleted: positionViewAtIndex(Data.nowIndex, ListView.Beginning)

        delegate: Item {
            id: hourRow
            required property int index

            readonly property bool isNow: index === Data.nowIndex
            readonly property bool isPast: index < Data.nowIndex

            width: view.width
            height: root.rowHeight
            opacity: isPast ? 0.5 : 1

            // A reading aid, not an affordance. Seven columns spread over a
            // metre of screen and the eye loses the line somewhere around
            // Wind; the pointer's row lifting to the raised wash gives it a
            // rail to run along. No `cursorShape` — nothing in this list is
            // clickable and a pointing hand would promise that it is.
            HoverHandler { id: rowHover }

            Rectangle {
                anchors.fill: parent
                anchors.topMargin: 1
                radius: Theme.metric.controlRadius

                // One rectangle changing colour, not a hover panel laid over
                // the stripe: two washes stack to a patch lighter than either
                // (§10.1), and the seam is exactly what you would notice.
                //
                // The now row is exempt. Its fill *is* the mark — swapping the
                // yellow for a neutral wash would blank the one row the reader
                // came to find, the moment they point at it.
                color: hourRow.isNow ? Theme.color.nowRowBg
                                     : (rowHover.hovered
                                        ? Theme.color.surfaceRaised
                                        : (hourRow.index % 2 === 0 ? "transparent"
                                                                   : Theme.color.listRowAlt))

                // Behaviors do not fire for a property's initial binding, so a
                // delegate built as it scrolls into view arrives at its stripe
                // colour rather than fading up to it.
                Behavior on color {
                    ColorAnimation {
                        duration: Theme.motion.tint
                        easing.type: Easing.OutCubic
                    }
                }
            }

            Rectangle {
                visible: hourRow.isNow
                width: 3
                height: parent.height - 10
                radius: 1.5
                anchors.verticalCenter: parent.verticalCenter
                color: Theme.color.accent
            }

            Row {
                anchors.fill: parent
                spacing: 12

                Text {
                    width: cols.time
                    height: parent.height
                    verticalAlignment: Text.AlignVCenter
                    leftPadding: 12
                    text: Data.hourLabel(hourRow.index)
                    color: Theme.color.textPrimary
                    font.pixelSize: 12
                    font.bold: hourRow.isNow
                }

                Item {
                    width: cols.icon
                    height: parent.height
                    WeatherGlyph {
                        anchors.centerIn: parent
                        kind: Data.conditionFor(index)
                        glyphSize: 24
                    }
                }

                Text {
                    width: cols.condition
                    height: parent.height
                    verticalAlignment: Text.AlignVCenter
                    text: Data.conditionText(index)
                    color: Theme.color.textMuted
                    font.pixelSize: 12
                }

                Cell { cellWidth: cols.temp;     text: Math.round(Data.temperature[index]) + "°" ; font.bold: true }
                Cell { cellWidth: cols.feels;    text: Math.round(Data.apparent[index]) + "°" ; color: Theme.color.textMuted }
                Cell { cellWidth: cols.precip;   text: Data.precipProb[index] + "%" ; color: Theme.color.droplet }
                Cell { cellWidth: cols.wind;     text: Math.round(Data.windSpeed[index]) + " km/h" ; color: Theme.color.textMuted }
                Cell { cellWidth: cols.humidity; text: Data.humidity[index] + "%" ; color: Theme.color.textMuted }
            }
        }
    }
}
