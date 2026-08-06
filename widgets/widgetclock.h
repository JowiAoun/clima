// SPDX-FileCopyrightText: 2026 Jowi Aoun
// SPDX-License-Identifier: GPL-3.0-or-later
//
// What time the tiles think it is.
//
// ============================================================================
// WHY A CLOCK AT ALL IN A PROCESS THAT ONLY DRAWS
//
// Two things on a tile move without any new data arriving, and both of them
// read the wall clock rather than the snapshot:
//
//   the sun mark    SkyArc's `nowMin`. Sunrise and sunset do not change between
//                   publishes, so a mark driven by `generatedAt` would step
//                   forward once every five minutes and freeze completely the
//                   moment the daemon stopped — the one thing on the tile that
//                   looked broken while everything around it was right.
//
//   the age footer  "updated 40 minutes ago", which has to keep counting up
//                   through a daemon outage. That is the whole point of it.
//
// Both are correct behaviour and both make a screenshot different every time it
// is taken. `docs/images/` is byte-compared in CI — `scripts/shots.sh check` —
// so a widget image would fail that gate on the second run and go on failing.
//
// ============================================================================
// SO: ONE FUNCTION, FROZEN ONLY WHEN ASKED
//
// The same shape as libclima's injectable Clock and for the same reason: it is
// the single mechanism that makes a capture deterministic without one
// `if (testing)` anywhere in the drawing code. Unfrozen — which is every run
// that is not a screenshot — `now()` is `QDateTime::currentDateTimeUtc()` and
// costs a function call.

#pragma once

#include <QDateTime>

namespace clima::widgets {

// UTC, always. Every caller converts into the place's own offset, which comes
// off the wire rather than from here.
[[nodiscard]] QDateTime now();

// `--now <iso8601>`. An invalid QDateTime restores the real clock, which is
// what makes this safe to call unconditionally from option parsing.
void freezeClock(const QDateTime &instant);

[[nodiscard]] bool clockIsFrozen();

} // namespace clima::widgets
