// SPDX-FileCopyrightText: 2026 Jowi Aoun
// SPDX-License-Identifier: GPL-3.0-or-later
// Diagonal hatch. Used wherever the chart means "the past — there is no forecast
// here", so the absence of data reads as intentional rather than as a rendering bug.
import QtQuick
import QtQuick.Shapes
import "chartmath.js" as ChartMath

Item {
    id: root

    // A default rather than a required property: the gallery stages this on its
    // own, with nothing to tell it what it is hatching. Every caller in the app
    // says — `overlay.pastHatch` for the chart's past, `scaffold.stroke` for the
    // map placeholder — so this value is only ever the specimen's.
    property color lineColor: Theme.overlay.hatch
    property real lineWidth: 1
    property real spacing: 8
    property real slope: 0.7        // dx per dy

    // `hatchPath` deliberately starts its lines outside the box — that is how the
    // diagonals reach the top-left and bottom-right corners — and trims them with
    // the clip. Except Shapes ignore ancestor clipping (§10.8), so the clip alone
    // never trimmed anything: this only ever looked right because every caller
    // happens to sit inside someone else's layer. The layer belongs here, on the
    // component that makes the promise.
    clip: true
    layer.enabled: true

    Shape {
        anchors.fill: parent
        preferredRendererType: Shape.CurveRenderer

        ShapePath {
            strokeColor: root.lineColor
            strokeWidth: root.lineWidth
            fillColor: "transparent"
            capStyle: ShapePath.FlatCap
            PathSvg {
                path: ChartMath.hatchPath(root.width, root.height, root.spacing, root.slope)
            }
        }
    }
}
