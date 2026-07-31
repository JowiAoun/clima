// SPDX-FileCopyrightText: 2026 Jowi Aoun
// SPDX-License-Identifier: MPL-2.0
//
// One measurement, or the absence of one.
//
// Two lines of code and a page of reasoning, because this alias is the single
// decision that keeps a fallback provider honest.
//
// ---- what a plain double would cost -----------------------------------------
//
// MET Norway's compact product has no wind gust. If HourlyPoint::windGust were
// a `double`, the adapter would leave it at 0, the gust row on the detail card
// would read "0 km/h", and a user standing in a gale would be told the wind is
// steady. Nothing would have failed. No test would go red. The app would simply
// have said something untrue, in the calm voice of a number.
//
// A sentinel — NaN, -9999 — moves the problem rather than solving it: every
// consumer has to remember to check, the check is a different one per type, and
// the first arithmetic that forgets propagates the sentinel into an average.
//
// std::optional makes the absence unreadable as a number. `*value` on an empty
// optional is a bug at the call site rather than a wrong pixel three layers
// away, and `value_or(0)` is a thing an author has to type on purpose.
//
// ---- and what it does not cost ----------------------------------------------
//
// sizeof(std::optional<double>) is 16 rather than 8. An hourly series of 384
// points with twenty fields is 123 kB instead of 61 kB, once, per location.
// That is not a budget anybody is near — docs/03-tech-stack.md's budgets are
// about the binary and about frame time — and the copy is still a memcpy,
// which is what keeps the domain types "immutable and copy-cheap so snapshots
// cross threads without locking" (docs/04-architecture.md §4.8).
//
// ---- the rule -------------------------------------------------------------
//
// Every measurement in libclima/domain/ is a Reading. Not "every measurement a
// provider might not have" — every measurement. A field that is optional only
// for the providers that lack it is a field whose type has to change the day a
// third provider is added, and by then there are consumers.
//
// The capability flags in libclima/providers/iforecastprovider.h are the same
// statement made ahead of time, for a UI that has to decide whether to draw a
// row before any data exists. The two are checked against each other: a
// provider that advertises a capability and returns nullopt for every hour of
// it is lying, and there is a test that says so.

#pragma once

#include <optional>

namespace clima {

using Reading = std::optional<double>;

} // namespace clima
