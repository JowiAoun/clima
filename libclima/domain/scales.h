// SPDX-FileCopyrightText: 2026 Jowi Aoun
// SPDX-License-Identifier: MPL-2.0
//
// Somebody else's tables, transcribed once.
//
// Every function here turns a number into the word an authority has already
// decided it deserves: the WHO's UV bands, the European AQI's own bands, the
// Beaufort scale, the sixteen-point compass. None of it is a judgement of
// ours, and none of it may be adjusted to make a card look better balanced.
//
// ---- why this is in libclima and not in the view model ----------------------
//
// It used to be six file-static functions at the top of
// app/viewmodels/conditionsdata.cpp, which was the right place while the app
// was the only thing that drew weather. It stopped being the right place the
// moment a widget had to put "Very high" under a UV dial: a second copy of a
// threshold table is a copy that can drift, and the failure when it does is
// the app and the tile on the same desktop disagreeing about the same number.
// Two readings of the same air is exactly the defect
// docs/10-design-system.md §10.5 is about.
//
// domain/ rather than a new directory, because these are properties of the
// quantity rather than of any provider or of any screen. No I/O, no Qt Gui,
// nothing that needs a cache — a pure function from a double to a string, which
// is why the widget host can link this file alone and carry none of the
// engine with it.
//
// ---- NaN ---------------------------------------------------------------------
//
// Every one of these takes a plain double rather than a Reading, and every one
// answers an empty string for NaN rather than a band name. A caller that has
// no reading has to say "—" itself; being handed "Low" for a UV index nobody
// measured is the null-drawn-as-zero mistake with a word on it.

#pragma once

#include <QString>

namespace clima::scales {

// WHO, and this is the whole table: low 0–2, moderate 3–5, high 6–7,
// very high 8–10, extreme 11+.
[[nodiscard]] QString uvBand(double index);

// The European AQI's own bands: 0–20 good, 20–40 fair, 40–60 moderate,
// 60–80 poor, 80–100 very poor, 100+ extremely poor.
//
// The *European* index specifically. libclima/wire/snapshot.cpp sends this one
// everywhere for the same reason app/viewmodels/conditionsdata.cpp shows it
// everywhere: two scales would let a widget and the card behind it put
// different words on the same air.
[[nodiscard]] QString aqiBand(double index);

// Kilometres, in the five steps an aviation-adjacent reader recognises.
[[nodiscard]] QString visibilityBand(double km);

// Beaufort force from km/h, inverting the standard v = 0.836·B^1.5 in m/s.
// Bounded to 0–12: the scale has no 13, and extrapolating one would be
// inventing a category.
[[nodiscard]] int beaufortForce(double kmh);

[[nodiscard]] QString beaufortName(int force);

// Sixteen points and not thirty-two. A forecast's wind direction is a model
// average, and NNE-by-E is a precision it does not have.
[[nodiscard]] QString compassPoint(double degrees);

// A pollutant's machine id — `clima::pollutantId()`'s "pm2_5", "no2", "o3" —
// as the name a chemist would write.
//
// airquality.h is explicit that its ids are not user-facing, and until there
// was a second thing that displayed them the app got away with uppercasing
// one: the air-quality card has been printing "PM2_5" where every published
// index in the world writes "PM2.5". Doing that in two places would have made
// it two defects, so the spelling lives here with the rest of the transcribed
// tables. Unknown ids come back uppercased, which is the old behaviour and the
// only honest answer for a species we have no name for.
[[nodiscard]] QString pollutantLabel(const QString &id);

} // namespace clima::scales
