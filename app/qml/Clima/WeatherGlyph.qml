// SPDX-FileCopyrightText: 2026 Jowi Aoun
// SPDX-License-Identifier: GPL-3.0-or-later
// Procedural weather icons.
//
// Deliberately drawn rather than shipped as assets: it keeps the prototype a
// single `qml` invocation with no asset pipeline. Production swaps these for
// Meteocons (MIT) converted to Qt Quick Shapes via svgtoqml — decision D10.
//
// ---- the vocabulary ---------------------------------------------------------
//
// Thirteen kinds, and they are exactly `clima::ConditionKind` spelled out —
// see libclima/domain/weathercode.h, which owns the WMO table that produces
// them. That equality is the point rather than a coincidence: this file used
// to know seven, so `drawableToday()` existed in the engine to fold the other
// six into them before they ever reached QML, and a thunderstorm arrived here
// already relabelled as rain. Nothing downstream could tell, which is how the
// ten-day strip came to draw a plain shower over a day the forecast said would
// have lightning in it.
//
// So the fold is gone and the six are drawn. If a kind is ever added to the
// enum without a picture here it renders empty rather than wrong — every one
// of the booleans below is false — and tests/qml/tst_weatherglyph.qml fails on
// exactly that, by grabbing each kind and looking at the pixels.
//
// ---- what is deliberately not distinguished ---------------------------------
//
// `rain` and `rain-night` draw the same picture. The enum keeps them apart
// because the *badge* behind the glyph is a day plate or a night plate, and
// that is where the difference is carried; a moon behind a cloud that is
// raining is a sky nobody can see.
//
// Six kinds have no night form at all, for the same reason — an overcast sky
// with something falling out of it looks the same at midnight — and
// weathercode.h's `dayAndNightDifferOnlyWhereTheSkyIsVisible` is the test that
// pins it.
// Every delegate below reads `root` for the icon box it is a fraction of, and
// unqualified access is what stops qmlcachegen compiling a binding ahead of
// time. Bound requires the model roles to be `required`, which they are.
pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Shapes
import "chartmath.js" as ChartMath

Item {
    id: root

    property string kind: "clear-day"
    property real glyphSize: 26
    property bool onLightBackground: false

    implicitWidth: glyphSize
    implicitHeight: glyphSize
    width: glyphSize
    height: glyphSize

    readonly property bool hasSun:   kind === "clear-day" || kind === "partly-day"
    readonly property bool hasMoon:  kind === "clear-night" || kind === "partly-night"

    // What is coming out of the sky, or "" for the six dry kinds. Everything
    // below reads this rather than the kind, because the arrangements group
    // differently from the names: `rain` and `rain-night` fall identically,
    // and `fog` is not precipitation but is drawn in the same band under the
    // cloud and has to reserve the same room.
    readonly property string falls:
        kind === "drizzle"                       ? "drizzle"
      : kind === "rain" || kind === "rain-night" ? "rain"
      : kind === "sleet"                         ? "sleet"
      : kind === "snow"                          ? "snow"
      : kind === "hail"                          ? "hail"
      : kind === "thunder"                       ? "thunder"
      : kind === "fog"                           ? "fog"
      : ""

    // A cloud with nothing beside it fills the box; a cloud sharing the box
    // with a sun or a moon is smaller and sits low-right. Anything that falls
    // needs the whole width to fall across, so every one of those is solo.
    readonly property bool soloCloud: kind === "cloudy" || root.falls !== ""
    readonly property bool hasCloud: kind === "partly-day" || kind === "partly-night"
                                     || root.soloCloud

    // The marks are essential — they are the difference between snow and rain —
    // so each carries the pale-ground value the day plate needs. See the note
    // on `rainOnLight` in theme.js for the measurement that put it there.
    readonly property color dropInk:   onLightBackground ? Theme.glyph.rainOnLight : Theme.glyph.rain
    readonly property color iceInk:    onLightBackground ? Theme.glyph.snowOnLight : Theme.glyph.snow
    readonly property color boltInk:   onLightBackground ? Theme.glyph.boltOnLight : Theme.glyph.bolt
    readonly property color hazeInk:   onLightBackground ? Theme.glyph.cloudBottomOnLight
                                                         : Theme.glyph.cloudBottom

    // ---- sun -------------------------------------------------------------
    Item {
        visible: root.hasSun
        width: root.soloCloud ? 0 : (root.kind === "clear-day" ? root.width * 0.72 : root.width * 0.56)
        height: width
        x: root.kind === "clear-day" ? (root.width - width) / 2 : root.width * 0.04
        y: root.kind === "clear-day" ? (root.height - height) / 2 : root.height * 0.06

        // A glow, not a disc.
        //
        // This was a flat circle of `sunGlyphWarm` at 16 %, 1.28x the sun's
        // diameter. A flat circle has an edge you can trace, which is §10.1's
        // test for a stacked wash rather than a glow — and at the 26 px this
        // glyph is normally drawn at, the edge is a couple of pixels and nobody
        // ever saw it. Staged at 72 px on the current-conditions card it is an
        // unmistakable hard-rimmed ring around the sun.
        //
        // Fading to zero alpha at the rim is what it was always meant to be.
        Shape {
            id: halo
            anchors.centerIn: parent
            width: parent.width * 1.9
            height: width
            preferredRendererType: Shape.CurveRenderer

            readonly property color tint: Theme.glyph.sunWarm

            ShapePath {
                strokeColor: "transparent"
                fillGradient: RadialGradient {
                    centerX: halo.width / 2
                    centerY: halo.height / 2
                    centerRadius: halo.width / 2
                    focalX: centerX
                    focalY: centerY
                    // Flat out to the sun's own rim, then away to nothing.
                    GradientStop {
                        position: 0.0
                        color: Qt.rgba(halo.tint.r, halo.tint.g, halo.tint.b, 0.20)
                    }
                    GradientStop {
                        position: 0.52
                        color: Qt.rgba(halo.tint.r, halo.tint.g, halo.tint.b, 0.18)
                    }
                    GradientStop {
                        position: 1.0
                        color: Qt.rgba(halo.tint.r, halo.tint.g, halo.tint.b, 0.0)
                    }
                }
                PathAngleArc {
                    centerX: halo.width / 2
                    centerY: halo.height / 2
                    radiusX: halo.width / 2
                    radiusY: halo.height / 2
                    startAngle: 0
                    sweepAngle: 360
                }
            }
        }
        Rectangle {
            anchors.fill: parent
            radius: width / 2
            gradient: Gradient {
                GradientStop { position: 0.0; color: Theme.glyph.sunWarm }
                GradientStop { position: 1.0; color: Theme.glyph.sunCool }
            }
        }
    }

    // ---- moon ------------------------------------------------------------
    Shape {
        visible: root.hasMoon
        width: root.width
        height: root.height
        preferredRendererType: Shape.CurveRenderer

        ShapePath {
            fillColor: Theme.glyph.moon
            strokeColor: "transparent"
            // Crescent sits centred when alone, upper-left when a cloud is in front.
            PathSvg {
                path: ChartMath.moonPath(
                          root.width  * (root.kind === "clear-night" ? 0.50 : 0.34),
                          root.height * (root.kind === "clear-night" ? 0.47 : 0.34),
                          root.width  * (root.kind === "clear-night" ? 0.34 : 0.26),
                          0.42,
                          // Not a phase. This crescent means "night", and the
                          // sky it is drawn over is a forecast hour rather than
                          // a date — so it says which way it points rather than
                          // taking a default that would look like a claim.
                          false)
            }
        }
    }

    // ---- cloud -----------------------------------------------------------
    Shape {
        visible: root.hasCloud
        width: root.width
        height: root.height
        preferredRendererType: Shape.CurveRenderer

        ShapePath {
            strokeColor: "transparent"
            fillGradient: LinearGradient {
                x1: 0; y1: root.height * (root.soloCloud ? 0.24 : 0.42)
                x2: 0; y2: root.height * (root.soloCloud ? 0.80 : 0.92)
                GradientStop {
                    position: 0.0
                    color: root.onLightBackground ? Theme.glyph.cloudTopOnLight
                                                  : Theme.glyph.cloudTop
                }
                GradientStop {
                    position: 1.0
                    color: root.onLightBackground ? Theme.glyph.cloudBottomOnLight
                                                  : Theme.glyph.cloudBottom
                }
            }
            PathSvg { path: root.cloudPath(root.width, root.height, root.soloCloud) }
        }
    }

    // ---- what falls ------------------------------------------------------
    //
    // One band under the cloud — y 0.74 to the bottom edge — and four shapes
    // that share it. `rain`'s three drops keep the exact fractions, widths and
    // 12-degree lean they have had since the prototype, so every recorded rainy
    // frame under tests/golden still matches byte for byte and only the kinds
    // that used to be *drawn as* rain move.
    //
    // The marks are described by the four functions at the bottom of the file
    // rather than by four `model:` expressions here, because the interesting
    // part of each is why the counts and offsets are what they are, and that
    // argument belongs next to the numbers.

    // Drops: rain, drizzle, and the wet half of sleet.
    Repeater {
        model: root.dropMarks()
        delegate: Rectangle {
            required property var modelData
            width: Math.max(modelData.min, root.width * modelData.w)
            height: root.height * modelData.h
            radius: width / 2
            color: root.dropInk
            x: root.width * modelData.x
            y: root.height * modelData.y
            transform: Rotation { angle: 12 }
        }
    }

    // Ice pellets: the frozen half of sleet, and hail.
    Repeater {
        model: root.iceMarks()
        delegate: Rectangle {
            required property var modelData
            width: Math.max(2.4, root.height * modelData.d)
            height: width
            radius: width / 2
            color: root.iceInk
            x: root.width * modelData.cx - width / 2
            y: root.height * modelData.cy - height / 2
        }
    }

    // Flakes. Three crossed bars rather than a drawn snowflake: at the 26 px
    // this glyph is usually rendered at, a six-armed star with detail on the
    // arms is a grey blob, and a grey blob is what a drop already looks like.
    // Bars at 60 degrees keep the six points at any size.
    Repeater {
        model: root.flakeMarks()
        delegate: Item {
            id: flake
            required property var modelData
            width: root.height * flake.modelData.d
            height: width
            x: root.width * flake.modelData.cx - width / 2
            y: root.height * flake.modelData.cy - height / 2

            Repeater {
                model: 3
                delegate: Rectangle {
                    required property int index
                    anchors.centerIn: parent
                    width: flake.width
                    height: Math.max(1.2, root.height * 0.032)
                    radius: height / 2
                    color: root.iceInk
                    rotation: index * 60
                }
            }
        }
    }

    // Fog. Bars in the cloud's own underside colour, because fog *is* the
    // cloud — the one condition where the sky is at eye level.
    Repeater {
        model: root.hazeMarks()
        delegate: Rectangle {
            required property var modelData
            width: root.width * modelData.w
            height: Math.max(1.6, root.height * 0.055)
            radius: height / 2
            color: root.hazeInk
            x: root.width * modelData.x
            y: root.height * modelData.y
        }
    }

    // ---- the bolt --------------------------------------------------------
    //
    // Last, so it lies over the cloud it comes out of: the flat top of the
    // polygon is hidden inside the cloud's underside, which is what makes it
    // read as emerging rather than as a sticker.
    //
    // Hail has one too. `ConditionKind::Hail` is reached only from WMO 96 and
    // 99, and both of those are "thunderstorm with slight/heavy hail" — so a
    // hail glyph that dropped the lightning would be claiming a calmer sky
    // than the forecast did, and would differ from `thunder` in the wrong
    // direction.
    Shape {
        visible: root.falls === "thunder" || root.falls === "hail"
        width: root.width
        height: root.height
        preferredRendererType: Shape.CurveRenderer

        ShapePath {
            fillColor: root.boltInk
            strokeColor: "transparent"
            PathSvg { path: root.boltPath(root.width, root.height) }
        }
    }

    // A cloud from four cubic segments: flat base, tall bump left of centre,
    // shorter bump to the right. Coordinates are fractions of the icon box.
    // Two decimals, through a typed parameter.
    //
    // The obvious spelling — the sum with `.toFixed(2)` written on the end of
    // it — is four qmllint `missing-property` warnings: the locals those
    // functions add are `var`, so the sum comes back as a QJSPrimitiveValue,
    // and `toFixed` is not a member of one. Handing the number to something
    // declared `real` is what tells qmllint it is a double, and a double has
    // `toFixed`.
    //
    // The rounding is not cosmetic. It is what makes a path string identical
    // between two runs, which is the property the golden images rest on.
    function fixed2(v: real): string { return v.toFixed(2) }

    function cloudPath(w: real, h: real, solo: bool): string {
        var s  = solo ? 1.0 : 0.74;                 // scale
        var ox = solo ? 0.04 * w : 0.26 * w;        // origin
        var oy = solo ? 0.24 * h : 0.40 * h;
        var uw = 0.92 * w * s;
        var uh = 0.56 * h * s;

        function px(f: real): string { return root.fixed2(ox + f * uw) }
        function py(f: real): string { return root.fixed2(oy + f * uh) }

        return "M " + px(0.13) + " " + py(1.00)
             + " C " + px(0.04) + " " + py(1.00) + " " + px(0.00) + " " + py(0.79) + " " + px(0.00) + " " + py(0.66)
             + " C " + px(0.00) + " " + py(0.48) + " " + px(0.07) + " " + py(0.36) + " " + px(0.16) + " " + py(0.34)
             + " C " + px(0.19) + " " + py(0.12) + " " + px(0.32) + " " + py(0.00) + " " + px(0.46) + " " + py(0.00)
             + " C " + px(0.58) + " " + py(0.00) + " " + px(0.68) + " " + py(0.09) + " " + px(0.73) + " " + py(0.24)
             + " C " + px(0.77) + " " + py(0.20) + " " + px(0.82) + " " + py(0.18) + " " + px(0.87) + " " + py(0.18)
             + " C " + px(0.96) + " " + py(0.18) + " " + px(1.00) + " " + py(0.38) + " " + px(1.00) + " " + py(0.62)
             + " C " + px(1.00) + " " + py(0.83) + " " + px(0.94) + " " + py(1.00) + " " + px(0.84) + " " + py(1.00)
             + " Z";
    }

    // A six-point zigzag in a box that starts inside the cloud and ends on the
    // icon's bottom edge. Coordinates are fractions of that box, not of the
    // icon, so the bolt keeps its proportions while the box moves.
    function boltPath(w: real, h: real): string {
        var ox = 0.30 * w, oy = 0.50 * h;
        var bw = 0.42 * w, bh = 0.50 * h;

        function px(f: real): string { return root.fixed2(ox + f * bw) }
        function py(f: real): string { return root.fixed2(oy + f * bh) }

        return "M " + px(0.62) + " " + py(0.00)
             + " L " + px(0.16) + " " + py(0.58)
             + " L " + px(0.46) + " " + py(0.58)
             + " L " + px(0.30) + " " + py(1.00)
             + " L " + px(0.88) + " " + py(0.38)
             + " L " + px(0.52) + " " + py(0.38)
             + " Z";
    }

    // ---- the marks, as fractions of the icon box -------------------------
    //
    // `x`/`y` are a top-left corner and `cx`/`cy` a centre, matching how each
    // shape is easiest to place. `min` is a floor in device pixels: a drop
    // 0.07 of an 18 px icon is 1.26 px, which antialiases to a grey smear, and
    // the smallest mark that still reads as a mark is what that number is.

    function dropMarks(): list<var> {
        var out = [];
        var i;

        if (root.falls === "rain") {
            // The prototype's three, moved by nothing.
            for (i = 0; i < 3; ++i)
                out.push({ x: 0.25 + i * 0.21, y: 0.74 + (i === 1 ? 0.07 : 0),
                           w: 0.070, h: 0.22, min: 1.8 });
        } else if (root.falls === "drizzle") {
            // Four, finer and shorter. Drizzle is not rain drawn smaller — it
            // is more drops carrying less water, so the count is half of what
            // says so and the length is the other half.
            for (i = 0; i < 4; ++i)
                out.push({ x: 0.215 + i * 0.17, y: 0.78 + (i % 2 === 0 ? 0 : 0.05),
                           w: 0.055, h: 0.13, min: 1.4 });
        } else if (root.falls === "sleet") {
            // Rain's outer two columns, at rain's exact size. The middle one
            // is frozen, below — a sleet glyph is a rain glyph with one drop
            // turned to ice, which is what sleet is.
            out.push({ x: 0.25, y: 0.74, w: 0.070, h: 0.22, min: 1.8 });
            out.push({ x: 0.67, y: 0.74, w: 0.070, h: 0.22, min: 1.8 });
        }
        return out;
    }

    function iceMarks(): list<var> {
        if (root.falls === "sleet")
            return [{ cx: 0.495, cy: 0.86, d: 0.15 }];

        // Hail flanks the bolt rather than sitting under it: the two are the
        // same storm and overlapping them would lose both. Close in, though —
        // pushed out to the icon's edges they stop reading as part of the same
        // picture and start reading as two dots the cloud has grown.
        if (root.falls === "hail")
            return [{ cx: 0.27, cy: 0.90, d: 0.17 },
                    { cx: 0.73, cy: 0.90, d: 0.17 }];

        return [];
    }

    function flakeMarks(): list<var> {
        if (root.falls !== "snow")
            return [];

        // Rain's three columns, at their centres, with the same middle-column
        // stagger — a snowy hour and a rainy hour should sit at the same
        // rhythm in a row of glyphs, and differ only in what is falling.
        var out = [];
        for (var i = 0; i < 3; ++i)
            out.push({ cx: 0.285 + i * 0.21, cy: 0.845 + (i === 1 ? 0.06 : 0), d: 0.18 });
        return out;
    }

    function hazeMarks(): list<var> {
        if (root.falls !== "fog")
            return [];

        // Two, unequal and offset, entirely below the cloud.
        //
        // Three was tried and the room is not there: the solo cloud's flat base
        // is at 0.80 and the icon ends at 1.00, so three bars and two gaps fit
        // only by making both gaps about a pixel at the size this is normally
        // drawn — the first bar merged into the cloud's underside and the glyph
        // read as a cloud standing on a shelf. Two bars with real air between
        // them say the same thing and survive being small.
        //
        // Unequal and offset because two centred bars of one width is an equals
        // sign, and the stagger is what makes this read as air.
        return [{ x: 0.10, y: 0.83, w: 0.68 },
                { x: 0.22, y: 0.93, w: 0.62 }];
    }
}
