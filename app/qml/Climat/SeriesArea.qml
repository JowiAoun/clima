// SPDX-FileCopyrightText: 2026 Jowi Aoun
// SPDX-License-Identifier: GPL-3.0-or-later
// Filled series: gradient area plus a gradient "curve".
//
// Generalised from the temperature chart — the gradient runs down the *value*
// axis and its stops are keyed by normalised position, so the same component
// serves °C, %, km/h and hPa without knowing what it is drawing.
import QtQuick
import QtQuick.Shapes
import "chartmath.js" as ChartMath

Item {
    id: root

    property var points: []
    property var overlayPoints: []      // optional second line, e.g. wind gusts

    // Dashed for an envelope, solid for a second reading — see the wind
    // metric in metrics.cpp, which is the only caller that asks for dashes.
    property bool overlayDashed: false

    // The overview's overlay is the reader's to turn off, so it fades rather
    // than vanishing. Everything else leaves this at 1.
    property real overlayOpacity: 1

    // The token is a string, and a string has no `.r`. Coerced once here so the
    // fade below is arithmetic on a colour rather than four lookups qmllint
    // cannot resolve and qmlcachegen therefore cannot compile.
    readonly property color overlayInk: Theme.line.series
    property real baselineY: height
    property real gradientTop: 0
    property real gradientBottom: height
    property var fillRamp: []
    property var lineRamp: []
    property real lineWidth: 2.4

    // 0 = folded flat onto the baseline, 1 = drawn at full extent.
    //
    // A metric switch hands over *at the baseline*, which is the one place a
    // temperature curve and a wind curve genuinely agree: the outgoing series
    // folds onto it, the axis becomes a different axis while nothing is drawn,
    // and the incoming series grows back off it. SeriesBars carries the same
    // property under the same name, which is what lets area → bars read as one
    // gesture instead of a cut — the two have nothing else in common.
    //
    // Geometry only. The gradient stays keyed to the axis, not to the fold, so
    // the fill never claims a value the series is not at.
    property real growth: 1

    function folded(pts) {
        if (growth >= 1 || !pts || pts.length === 0)
            return pts
        var out = []
        for (var i = 0; i < pts.length; ++i)
            out.push({ x: pts[i].x, y: baselineY + (pts[i].y - baselineY) * growth })
        return out
    }

    // The points as drawn, as opposed to the points as given.
    readonly property var drawnPoints: folded(points)
    readonly property var drawnOverlay: folded(overlayPoints)

    function stopColor(which, i) {
        var r = which === "fill" ? fillRamp : lineRamp
        return (r && r[i]) ? r[i].c : "#00000000"
    }

    function stopPos(which, i) {
        var r = which === "fill" ? fillRamp : lineRamp
        return (r && r[i]) ? r[i].p : (i / 7)
    }

    Shape {
        anchors.fill: parent
        preferredRendererType: Shape.CurveRenderer

        // ---- filled area -------------------------------------------------
        ShapePath {
            strokeColor: "transparent"
            fillGradient: LinearGradient {
                x1: 0; y1: root.gradientTop
                x2: 0; y2: root.gradientBottom
                GradientStop { position: root.stopPos("fill", 0); color: root.stopColor("fill", 0) }
                GradientStop { position: root.stopPos("fill", 1); color: root.stopColor("fill", 1) }
                GradientStop { position: root.stopPos("fill", 2); color: root.stopColor("fill", 2) }
                GradientStop { position: root.stopPos("fill", 3); color: root.stopColor("fill", 3) }
                GradientStop { position: root.stopPos("fill", 4); color: root.stopColor("fill", 4) }
                GradientStop { position: root.stopPos("fill", 5); color: root.stopColor("fill", 5) }
                GradientStop { position: root.stopPos("fill", 6); color: root.stopColor("fill", 6) }
                GradientStop { position: root.stopPos("fill", 7); color: root.stopColor("fill", 7) }
            }
            PathSvg { path: ChartMath.areaPath(root.drawnPoints, root.baselineY) }
        }

        // ---- curve, as a gradient-fillable ribbon ------------------------
        ShapePath {
            strokeColor: "transparent"
            fillGradient: LinearGradient {
                x1: 0; y1: root.gradientTop
                x2: 0; y2: root.gradientBottom
                GradientStop { position: root.stopPos("line", 0); color: root.stopColor("line", 0) }
                GradientStop { position: root.stopPos("line", 1); color: root.stopColor("line", 1) }
                GradientStop { position: root.stopPos("line", 2); color: root.stopColor("line", 2) }
                GradientStop { position: root.stopPos("line", 3); color: root.stopColor("line", 3) }
                GradientStop { position: root.stopPos("line", 4); color: root.stopColor("line", 4) }
                GradientStop { position: root.stopPos("line", 5); color: root.stopColor("line", 5) }
                GradientStop { position: root.stopPos("line", 6); color: root.stopColor("line", 6) }
                GradientStop { position: root.stopPos("line", 7); color: root.stopColor("line", 7) }
            }
            PathSvg { path: ChartMath.ribbonPath(root.drawnPoints, root.lineWidth / 2) }
        }

        // ---- optional overlay line (no fill) -----------------------------
        ShapePath {
            // Faded through the colour rather than through an `opacity` on a
            // wrapper: a layer around half a Shape is an FBO the other half is
            // not in, and the two ends of one chart can then disagree about
            // edge quality (§10.8).
            strokeColor: Qt.rgba(root.overlayInk.r, root.overlayInk.g, root.overlayInk.b,
                                 root.overlayInk.a * root.overlayOpacity)
            strokeWidth: root.drawnOverlay.length > 1 && root.overlayOpacity > 0 ? 1.4 : -1
            strokeStyle: root.overlayDashed ? ShapePath.DashLine : ShapePath.SolidLine
            dashPattern: [4, 3]
            fillColor: "transparent"
            capStyle: ShapePath.RoundCap
            PathSvg {
                path: root.drawnOverlay.length > 1
                      ? ChartMath.smooth(root.drawnOverlay, "M") : ""
            }
        }
    }
}
