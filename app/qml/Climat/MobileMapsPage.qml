// SPDX-FileCopyrightText: 2026 Jowi Aoun
// SPDX-License-Identifier: GPL-3.0-or-later
// Maps: the tab that exists so the nav bar is real, and says so.
//
// See MapPlaceholder for why it is drawn the way it is. This file's only job
// is to give it the screen.
import QtQuick

MobilePage {
    id: root

    MapPlaceholder {
        width: root.spanWidth(2)

        // Fills what is left of the screen rather than taking a height of its
        // own. A map is the one thing on this shell that is not a card in a
        // scrolling column — it wants the viewport — and the placeholder
        // standing in for it should occupy the space the real one will.
        //
        // Both insets, not just the nav's. A severe-weather banner moves
        // the column down by its own height, and a placeholder measured
        // from the window rather than from what is left of it runs that
        // far past the bottom of the screen.
        height: Math.max(340, root.height - root.margin * 2
                              - root.topInset - root.bottomInset)
    }
}
