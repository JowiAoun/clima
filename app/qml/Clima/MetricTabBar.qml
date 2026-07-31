// SPDX-FileCopyrightText: 2026 Jowi Aoun
// SPDX-License-Identifier: GPL-3.0-or-later
// Section title, metric pills, and the chart/list view switch.
//
// The pills are generated from the Metrics singleton, so a metric is added by
// editing one list.
// In the real app that registry is populated from provider capabilities, and a tab
// that the active provider cannot supply for this location simply never appears.
//
// Motion here is deliberately small. The pills are a tab strip driving the chart
// below them, and the chart is the thing worth watching: a selection that threw
// its own party would be competing with the answer it just asked for. So the
// selected pill only changes colour — see §10.6, which measured the reference's
// 0.2s linear fill and kept the idea while tightening the timing. The one thing
// that *moves* is the view switch's indicator, because a switch with two lamps
// has no position and a switch with one indicator does.
import QtQuick
import QtQuick.Shapes

Item {
    id: root

    property string title: qsTr("Hourly")
    property string currentId: "overview"
    property bool listView: false

    implicitHeight: 38
    height: implicitHeight

    // One half of the view switch. Declared as a type rather than as a Repeater
    // delegate because the indicator behind the two halves has to be a single
    // object that both of them can be measured against, and two named siblings
    // give that for free where a Repeater would need itemAt() and a rebuild
    // hook. There are two view modes and there is no registry of them; the pills
    // are the data-driven control on this bar, not this.
    component ViewSegment: Item {
        id: seg

        property string caption
        property string glyphPath

        signal tapped()

        implicitWidth: segLabel.implicitWidth + segGlyph.width + 26
        width: implicitWidth
        height: 30

        Row {
            anchors.centerIn: parent
            spacing: 7

            Item {
                id: segGlyph
                width: 13
                height: 13
                anchors.verticalCenter: parent.verticalCenter

                // chart: four bars of differing height; list: three rules
                Shape {
                    anchors.fill: parent
                    preferredRendererType: Shape.CurveRenderer
                    ShapePath {
                        strokeColor: Theme.ink.muted
                        strokeWidth: 1.4
                        fillColor: "transparent"
                        capStyle: ShapePath.RoundCap
                        PathSvg { path: seg.glyphPath }
                    }
                }
            }

            Text {
                id: segLabel
                text: seg.caption
                color: Theme.ink.primary
                font.pixelSize: 12
                anchors.verticalCenter: parent.verticalCenter
            }
        }

        HoverHandler { cursorShape: Qt.PointingHandCursor }
        TapHandler { onTapped: seg.tapped() }
    }

    // The title is inline with the pills rather than on its own row, so this is
    // not a SectionHeader — but it is the same *role*, and it takes the same
    // token. The two used to disagree by 3 px.
    Text {
        id: heading
        text: root.title
        color: Theme.ink.primary
        font.pixelSize: Theme.type.sectionTitle
        font.bold: true
        anchors.verticalCenter: parent.verticalCenter
        x: 0
    }

    // ---- view switch (right-aligned, laid out first so pills can clip to it)
    //
    // The active half used to carry its own fill and the two of them crossfaded,
    // which halfway through showed both sides half-lit and neither selected —
    // two lamps, not a switch. One indicator that travels says the control has a
    // *position*, which is the whole idea of a switch. It is also why the
    // indicator is now rounded on all four corners: the old half-pill was
    // squared where it met the other side, and with nothing drawn on that side
    // there was no junction for the square edge to serve.
    Item {
        id: viewSwitch
        anchors.right: parent.right
        anchors.verticalCenter: parent.verticalCenter
        width: switchRow.width
        height: switchRow.height

        // The indicator's geometry comes from the segments, and a segment is
        // zero-wide until its label has been laid out. Without this gate the
        // Behaviors treat that first layout as a change and the indicator grows
        // out of nothing on load — a mount animation for a control that has not
        // been touched yet.
        property bool settled: false
        Component.onCompleted: Qt.callLater(function () { viewSwitch.settled = true })

        Rectangle {
            id: thumb

            readonly property Item seat: root.listView ? listSeg : chartSeg

            x: seat.x
            width: seat.width
            height: parent.height
            radius: Theme.metric.controlRadius
            color: Theme.surface.raised
            border.width: 1
            border.color: Theme.line.control

            // Both halves are labelled, and "Chart" is wider than "List", so the
            // indicator changes size as well as position. One duration for both,
            // or it arrives twice.
            Behavior on x {
                enabled: viewSwitch.settled
                NumberAnimation { duration: Theme.motion.move; easing.type: Easing.OutCubic }
            }
            Behavior on width {
                enabled: viewSwitch.settled
                NumberAnimation { duration: Theme.motion.move; easing.type: Easing.OutCubic }
            }
        }

        Row {
            id: switchRow
            spacing: 0

            ViewSegment {
                id: chartSeg
                caption: qsTr("Chart")
                glyphPath: "M 1 12 L 1 6 M 5 12 L 5 2 M 9 12 L 9 8 M 12.5 12 L 12.5 4"
                onTapped: root.listView = false
            }

            ViewSegment {
                id: listSeg
                caption: qsTr("List")
                glyphPath: "M 1 3 L 12 3 M 1 6.5 L 12 6.5 M 1 10 L 12 10"
                onTapped: root.listView = true
            }
        }
    }

    // ---- metric pills ----------------------------------------------------
    Flickable {
        id: pillFlick
        anchors.left: heading.right
        anchors.leftMargin: 20
        anchors.right: viewSwitch.left
        anchors.rightMargin: 16
        anchors.verticalCenter: parent.verticalCenter
        height: 34
        clip: true
        contentWidth: pills.width
        contentHeight: height
        flickableDirection: Flickable.HorizontalFlick
        boundsBehavior: Flickable.StopAtBounds

        Row {
            id: pills
            spacing: 4
            height: pillFlick.height

            Repeater {
                model: Metrics.list

                delegate: Item {
                    id: pill

                    required property var modelData

                    readonly property bool active: modelData.id === root.currentId

                    // Measured bold whether or not this pill is selected.
                    // Selecting one bolds its label and therefore widened its
                    // box, which shoved every pill to its right a few pixels
                    // sideways on every tap: a whole row twitching to report a
                    // state change that the fill has already reported, and the
                    // only thing on this bar that moved without meaning to. The
                    // reference measured the same — "unselected pill: no fill,
                    // same box".
                    TextMetrics {
                        id: pillMetrics
                        text: pill.modelData.label
                        font.pixelSize: pillLabel.font.pixelSize
                        font.bold: true
                    }

                    width: Math.ceil(pillMetrics.width) + 26
                    height: pillFlick.height

                    Rectangle {
                        anchors.fill: parent
                        radius: height / 2
                        color: pill.active ? Theme.accent.fill
                                           : (pillHover.hovered ? Theme.surface.raised : "transparent")
                        Behavior on color {
                            ColorAnimation { duration: Theme.motion.tint; easing.type: Easing.OutCubic }
                        }
                    }

                    Text {
                        id: pillLabel
                        text: pill.modelData.label
                        anchors.centerIn: parent
                        color: pill.active ? Theme.accent.ink : Theme.ink.muted
                        font.pixelSize: 13
                        font.bold: pill.active
                        Behavior on color {
                            ColorAnimation { duration: Theme.motion.tint; easing.type: Easing.OutCubic }
                        }
                    }

                    HoverHandler { id: pillHover; cursorShape: Qt.PointingHandCursor }
                    TapHandler { onTapped: root.currentId = pill.modelData.id }

                    // A pill knows its own geometry, so it is the thing that
                    // asks to be brought into view rather than the bar hunting
                    // for it through the Repeater.
                    onActiveChanged: if (active) root.showPill(pill)
                }
            }
        }
    }

    // Bring the selected pill into the strip if the strip has been scrolled past
    // it. A window narrow enough to clip the row can otherwise be left showing a
    // metric name in the chart below and no selection at all up here, which is
    // the control lying about its state. Nothing moves when the whole row fits,
    // which it does at every width the page is laid out for.
    function showPill(item) {
        if (pillFlick.width <= 0 || pillFlick.contentWidth <= pillFlick.width)
            return

        var pad = 8
        var wanted = pillFlick.contentX
        if (item.x + item.width + pad > wanted + pillFlick.width)
            wanted = item.x + item.width + pad - pillFlick.width
        if (item.x - pad < wanted)
            wanted = item.x - pad

        wanted = Math.max(0, Math.min(wanted, pillFlick.contentWidth - pillFlick.width))
        if (Math.abs(wanted - pillFlick.contentX) < 0.5)
            return

        pillScroll.to = wanted
        pillScroll.restart()
    }

    // Not a Behavior on contentX: a Behavior would also intercept every flick
    // and drag, and animating the content away from the finger is how a
    // Flickable stops feeling like one.
    NumberAnimation {
        id: pillScroll
        target: pillFlick
        property: "contentX"
        duration: Theme.motion.move
        easing.type: Easing.OutCubic
    }
}
