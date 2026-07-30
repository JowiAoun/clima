// SPDX-License-Identifier: GPL-3.0-or-later
// The falling half of the precipitation effect: what is coming down, and how
// hard. Drawn over the series — the wash it belongs with is drawn under it, see
// PrecipBands.qml for why they are two components.
//
// Everything is a Rectangle, deliberately. A streak is a thin rounded one, a
// flake is a round one, a splash is four of them; there is not a Shape in the
// file. Shapes escape ancestor clipping (§10.8) and would need a layer of their
// own to bound, and a hundred layered particles is not a thing to do to a
// scene graph — where plain rectangles are batched, clipped by the Flickable
// that already clips everything else, and free.
//
// Motion is one clock. §10.6 says never animate on a timer that runs when
// nothing is happening, and this is the exception that proves it: rain is not a
// state *transition*, it is a state, and the only honest way to draw it is
// moving. So the rule is kept where it can be — the clock does not run when
// there is no precipitation, when the chart is behind the list view, or under
// `--grab`, and there is exactly one of it however heavy the weather gets.
import QtQuick
import "theme.js" as Theme
import "precip.js" as Precip

Item {
    id: root

    // One entry per hour, null where dry. See precip.js.
    property var cells: []
    property real hourWidth: Theme.metric.hourWidth
    property real contentWidth: width

    // False freezes the field at a deterministic frame rather than emptying it,
    // so a headless grab still shows rain — the same rain, every run.
    property bool animated: true

    // Spell captions. Off where something else already names the weather, on
    // where the band is all there is.
    property bool labels: true

    readonly property var spans: Precip.spans(root.cells)
    readonly property var drops: Precip.drops(root.cells, root.hourWidth, root.height)
    readonly property var splashes: Precip.splashes(root.cells, root.hourWidth, root.height)
    readonly property bool wet: root.drops.length > 0

    // Seconds, wrapping every Precip.LOOP of them. Every particle's own
    // progress is `(clock * rate + offset) mod 1`, and precip.js quantises the
    // rates so that product is whole at the wrap — otherwise the entire field
    // jumps at once, once a minute, which is exactly rare enough to get blamed
    // on something else.
    property real clock: 0

    NumberAnimation on clock {
        running: root.animated && root.wet
        loops: Animation.Infinite
        from: 0
        to: Precip.LOOP
        duration: Precip.LOOP * 1000
    }

    function progress(rate, offset) {
        return (root.clock * rate + offset) % 1;
    }

    // Fade in as a drop enters and out as it leaves, so nothing pops into
    // existence at the top of the plot or vanishes against the bottom edge.
    function taper(t) {
        if (t < 0.08)
            return t / 0.08;
        if (t > 0.84)
            return (1 - t) / 0.16;
        return 1;
    }

    // ---- lightning ---------------------------------------------------------
    // A storm band brightening, twice, quickly. Drawn under the drops so the
    // rain stays visible through it.
    Repeater {
        model: root.spans

        delegate: Rectangle {
            id: flash
            required property var modelData
            required property int index

            visible: modelData.type === "thunder"
            x: Precip.bandX(modelData, root.hourWidth)
            width: Precip.bandW(modelData, root.hourWidth, root.contentWidth)
            height: root.height
            color: Theme.precip.flash

            // Roughly one strike every six seconds, and the second band on a
            // chart does not flash in step with the first. The +0.5 puts the
            // frozen frame between strikes: caught mid-flash, a static grab
            // shows a pale rectangle over the chart and reads as a bug.
            readonly property real t: root.progress(10 / Precip.LOOP,
                                                    (0.5 + index * 0.37) % 1)
            opacity: !visible ? 0
                   : t < 0.010 ? 0.17
                   : t < 0.022 ? 0.05
                   : t < 0.038 ? 0.11
                               : 0
        }
    }

    // ---- drops -------------------------------------------------------------
    Repeater {
        model: root.drops

        delegate: Rectangle {
            id: drop
            required property var modelData

            readonly property real t: root.progress(modelData.rate, modelData.offset)

            width: modelData.width
            height: modelData.len
            radius: width / 2
            // Explicit, because a flake is three pixels across and Qt only
            // antialiases a rounded rectangle when asked. Left off, every
            // snowflake on the chart is a little square.
            antialiasing: true
            color: Theme.precip.drop[modelData.type]
                   ? Theme.precip.drop[modelData.type]
                   : Theme.precip.drop.rain
            opacity: modelData.alpha * root.taper(t)
            rotation: modelData.slant

            // Snow does not fall, it wanders. The sway is what separates a flake
            // from a fast small raindrop, which is otherwise the same rectangle.
            x: modelData.x - width / 2
               + (modelData.sway > 0
                  ? modelData.sway * Math.sin((t + modelData.swayPhase) * 2 * Math.PI)
                  : 0)
            y: -height + t * (root.height + height)
        }
    }

    // ---- splashes ----------------------------------------------------------
    // Four rectangles: the puddle line, the rebound jet above it, and two
    // droplets thrown out to the sides. Copied from the reference at 16x, where
    // what looks like a tick mark turns out to be all four.
    Repeater {
        model: root.splashes

        delegate: Item {
            id: splash
            required property var modelData

            readonly property real t: root.progress(modelData.rate, modelData.offset)

            // A splash is the back half of a cycle. The rest of the time this
            // item costs one comparison and paints nothing.
            readonly property real u: t < 0.52 ? -1 : (t - 0.52) / 0.48

            visible: u >= 0
            x: modelData.x
            y: modelData.y
            opacity: u < 0 ? 0 : (1 - u * u) * modelData.alpha
            scale: u < 0 ? 0 : 0.5 + u * 0.7

            readonly property real s: modelData.size

            Rectangle {                                  // puddle
                width: splash.s
                height: 1
                x: -width / 2
                color: Theme.precip.splash
            }

            Rectangle {                                  // rebound jet
                width: 1
                height: splash.s * 1.4
                x: -0.5
                y: -height
                color: Theme.precip.splash
            }

            // Thrown clear of the puddle, not straddling it. Left at the jet's
            // own height and leaning the same few degrees, the three of them
            // resolved into one downward arrow — a rendering that says the
            // opposite of what a splash is.
            Repeater {
                model: 2

                delegate: Rectangle {
                    required property int index
                    readonly property real side: index === 0 ? -1 : 1

                    width: 1.2
                    height: splash.s * 0.44
                    radius: width / 2
                    color: Theme.precip.splash
                    x: side * splash.s * 0.46 - width / 2
                    y: -height - splash.s * 0.34
                    rotation: side * 30
                }
            }
        }
    }

    // ---- spell captions ----------------------------------------------------
    // The band says when and the field says how hard, and between them they
    // still do not say the word. Six levels of texture are only legible once
    // you know there are six, so each spell wide enough to hold it is named.
    Repeater {
        model: root.labels ? root.spans : []

        delegate: Rectangle {
            id: caption
            required property var modelData

            readonly property real bandWidth:
                Precip.bandW(modelData, root.hourWidth, root.contentWidth)

            visible: bandWidth >= width + 16
            x: Precip.bandX(modelData, root.hourWidth) + 8
            y: 6
            width: captionText.implicitWidth + 14
            height: captionText.implicitHeight + 7
            radius: height / 2

            // The same scrim the sun markers use. A caption sits over whatever
            // the series happens to be doing, and grey text over an orange AQI
            // bar is not text.
            color: "#99111a2b"

            Text {
                id: captionText
                anchors.centerIn: parent
                text: caption.modelData.label
                color: Theme.color.textPrimary
                font.pixelSize: Theme.type.axis
            }
        }
    }
}
