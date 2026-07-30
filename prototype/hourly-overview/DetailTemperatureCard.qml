// SPDX-License-Identifier: GPL-3.0-or-later
// Temperature detail card — the worked example the other detail cards follow.
//
// The visualisation is a twelve-hour sparkline with the reading printed over
// it. The line is what the temperature is *doing*; the number is where it is
// now. Neither alone answers the question the card is asked.
//
// On arrival the curve draws itself in from the left, because the line is a
// history and history runs that way: the reader watches the morning happen and
// arrives at "now" with it. The number does not count up — a temperature has no
// meaningful zero to climb from, and a reading that is wrong for half a second
// is worse than one that is simply there.
import QtQuick
import QtQuick.Shapes
import "theme.js" as Theme
import "chartmath.js" as ChartMath
import "detaildata.js" as Detail

DetailCard {
    id: root

    readonly property var d: Detail.temperature

    title: qsTr("Temperature")
    status: d.status
    trend: d.trend
    body: d.body

    content: Item {
        id: viz

        readonly property var series: root.d.series
        // Pad the range so the curve never touches the box edges — a line
        // flush against the top reads as clipped rather than as a maximum.
        readonly property real lo: Math.min.apply(null, series) - 2
        readonly property real hi: Math.max.apply(null, series) + 2

        // The line occupies the upper half; the reading sits in the lower.
        readonly property real lineTop: 6
        readonly property real lineBottom: height * 0.52

        function xAt(i) { return i * (width - 14) / (series.length - 1) + 7 }
        function yAt(v) {
            return lineBottom - (v - lo) / (hi - lo) * (lineBottom - lineTop)
        }

        readonly property var pts: {
            var out = []
            for (var i = 0; i < series.length; ++i)
                out.push({ x: xAt(i), y: yAt(series[i]) })
            return out
        }

        readonly property real nowX: xAt(root.d.nowIndex !== undefined ? root.d.nowIndex : Detail.nowIndex)
        readonly property real nowY: yAt(series[Detail.nowIndex])

        // Where "now" falls along the sweep. The drawing edge is at
        // `width * reveal`, so comparing `reveal` against this is the same
        // instant the line reaches the mark — no second timer to keep in step.
        readonly property real nowP: width > 0 ? nowX / width : 1

        // The sweep. A window over the chart whose right edge travels left to
        // right on `reveal`, so the curve, its wash and the dimmed forecast all
        // appear in the order the hours did.
        //
        // It is a clip rather than a regenerated path: re-splining a growing
        // subset of the points would shift the control points of the stretch
        // already drawn, so the tail would wriggle as the line grew. The clip
        // draws the final curve from the first frame and only uncovers it.
        //
        // `layer.enabled` is what makes the clip bite: Qt Quick Shapes escape
        // ancestor clipping (docs/10-design-system.md §10.8) and would paint
        // straight out over the window's edge. A child outside the layer's
        // texture is never drawn into it, which is the bound we want. Filmed
        // both ways to be sure: with `clip: true` alone the first two frames of
        // the sweep paint the whole twelve hours, then the clip starts biting
        // part-way through — an animation that works only once it is nearly
        // over. The layer costs a little stroke antialiasing and is not
        // optional.
        Item {
            id: sweep

            // Never zero: a layer with no area is a texture Qt has to complain
            // about, and at 1 px nothing of the curve is inside it anyway.
            width: Math.max(1, viz.width * root.reveal)
            height: viz.height
            clip: true
            layer.enabled: true

            Shape {
                width: viz.width
                height: viz.height
                preferredRendererType: Shape.CurveRenderer

                // Area under the curve, fading out downward so it reads as weight
                // beneath the line rather than as a second shape.
                ShapePath {
                    strokeColor: "transparent"
                    fillGradient: LinearGradient {
                        x1: 0; y1: viz.lineTop; x2: 0; y2: viz.lineBottom
                        GradientStop { position: 0.0; color: "#4cff6b4a" }
                        GradientStop { position: 1.0; color: "#00ff6b4a" }
                    }
                    PathSvg { path: ChartMath.areaPath(viz.pts, viz.lineBottom) }
                }

                // The observed stretch, up to now.
                ShapePath {
                    fillColor: "transparent"
                    strokeColor: "#ff5c4a"
                    strokeWidth: 3
                    capStyle: ShapePath.RoundCap
                    PathSvg { path: ChartMath.smooth(viz.pts.slice(0, Detail.nowIndex + 1), "M") }
                }

                // The forecast stretch, dimmed: same line, less certainty.
                ShapePath {
                    fillColor: "transparent"
                    strokeColor: Theme.color.forecastDim
                    strokeWidth: 3
                    capStyle: ShapePath.RoundCap
                    PathSvg { path: ChartMath.smooth(viz.pts.slice(Detail.nowIndex), "M") }
                }
            }
        }

        // "Now", ringed so it stays visible wherever the line puts it.
        //
        // Outside the sweep, and grown in when the drawing edge passes it: a
        // mark sitting there before the line arrives is pointing at nothing,
        // and one inside the window gets sliced in half on the way past.
        Rectangle {
            width: 14; height: 14; radius: 7
            color: "#ff5c4a"
            border.width: 2.5
            border.color: Theme.color.textPrimary
            x: viz.nowX - width / 2
            y: viz.nowY - height / 2

            scale: root.reveal >= viz.nowP ? 1 : 0
            Behavior on scale {
                NumberAnimation {
                    duration: Theme.motion.move
                    easing.type: Easing.OutCubic
                }
            }
        }

        Text {
            text: root.d.value + root.d.unit
            color: Theme.color.textPrimary
            font.pixelSize: Theme.type.reading
            font.bold: true
            // Bottom-left, which is where the other eleven put their reading.
            // This card had it bottom-right and was the only one that did.
            anchors.left: parent.left
            anchors.bottom: parent.bottom
        }
    }
}
