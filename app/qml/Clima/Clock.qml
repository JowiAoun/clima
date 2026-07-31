// SPDX-FileCopyrightText: 2026 Jowi Aoun
// SPDX-License-Identifier: GPL-3.0-or-later
//
// What time it is, and what the sky is doing about it.
//
// Not the system clock. `Date.now()` appears nowhere in this tree and is not
// going to: §10.6 of the design system says determinism is not negotiable, and
// a background whose gradient depends on when the screenshot was taken makes
// every golden image of every mobile screen a different picture.
//
// Live data has landed and this file did not change shape, which was the point
// of it. `Detail.sun` is now the real sun at the real place, and the instant it
// is measured against is the engine's injected clock — a SystemClock in the
// product and a FrozenClock under `--fixture`, so the sky over a recorded
// afternoon is that afternoon's sky on every machine, for ever.
//
// ---- why it is a singleton and not two lines in a window --------------------
//
// It was two lines in a window, and then it was two lines in two windows: the
// app's Main.qml and the component gallery's, which paint the same backdrop for
// the same reason and must not disagree about the hour. Worse, the gallery is
// in Clima.Gallery now, so its copy had to reach a directory up for both of the
// JavaScript libraries below — an import that works, because the resource tree
// puts the two modules in a parent and a child directory, and that qmllint and
// qmlls cannot follow, because the *source* tree does not.
//
// So the question is asked in the one module that can ask it plainly, and both
// windows read the answer.
pragma Singleton

import QtQuick
import "sky.js" as Sky

QtObject {
    // Minutes past midnight. `Detail.sun` is the clock rather than anything else
    // on the page because it is the block that carries minutes — and it carries
    // them counted from one reference midnight, which is what keeps the phase
    // sane on a day the sun does not set (libclima/domain/timeaxis.h).
    readonly property int nowMin: Detail.sun.nowMin

    // Which of the four the sky is in: night, dawn, day, dusk. Dawn and dusk are
    // the seventy minutes either side of a crossing — see sky.js, which owns the
    // rule and the reason it is a constant.
    readonly property string skyPhase:
        Sky.phaseAt(nowMin, Detail.sun.riseMin, Detail.sun.setMin)
}
