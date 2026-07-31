// SPDX-License-Identifier: GPL-3.0-or-later
// What every surface in this prototype is composited over.
//
// Extracted from Main.qml when the gallery grew device frames. A specimen
// framed at 390x844 inside a 950 px window was being drawn over whatever slice
// of the window's own gradient happened to be behind it, which is not the
// slice it gets in the app — and the gallery's entire premise is that a
// component is reviewed on the background it is actually composited over.
// Painting the backdrop *inside* the frame is what makes that true again.
//
// It is a gradient rather than a flat fill because every surface above it is
// translucent: the cards have no colour of their own and take whatever this is
// doing behind them at that height. Flatten it and the whole page flattens.
//
// Five stops, declared in theme.js and written out here — QML cannot generate
// GradientStop elements from a Repeater, so if you add a stop, add it in both
// places. §10.2.
import QtQuick
import "theme.js" as Theme

Rectangle {
    gradient: Gradient {
        GradientStop { position: 0.00; color: Theme.color.pageStop0 }
        GradientStop { position: 0.06; color: Theme.color.pageStop1 }
        GradientStop { position: 0.30; color: Theme.color.pageStop2 }
        GradientStop { position: 0.60; color: Theme.color.pageStop3 }
        GradientStop { position: 1.00; color: Theme.color.pageStop4 }
    }
}
