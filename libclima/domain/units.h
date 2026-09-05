// SPDX-FileCopyrightText: 2026 Jowi Aoun
// SPDX-License-Identifier: MPL-2.0
//
// The one place a conversion factor is written down.
//
// Every number libclima hands out is canonical — °C, km/h, hPa, km, mm — and
// every number a reader sees has been through exactly one of the functions
// below. They used to live in app/viewmodels/units.cpp, behind the QML
// singleton that also knows which unit the reader chose; they moved here the
// day a second binary needed them. clima-cli prints a temperature and links
// no QML engine, and a second copy of "0.621371" in cli/ would have been the
// thing docs/04-architecture.md §4.10 warns about: two tables that agree today.
//
// The unit is a NAME, passed in, never looked up: "fahrenheit", "mph", "inhg".
// Which name applies is a preference, and a preference is the caller's to
// hold — Settings in the app, an INI read in the CLI, a flag in a test. A
// name this file does not know converts by the identity and prints the
// canonical symbol, so an INI hand-edited to a spelling that does not exist
// still puts a number on the screen.

#pragma once

#include <QString>

namespace clima::units {

enum class Quantity {
    None,
    Temperature,
    Wind,
    Pressure,
    Visibility,
    Precipitation,
    Percentage,
    Direction,
};

// Canonical -> the named unit, and back.
[[nodiscard]] double convert(Quantity quantity, const QString &unit, double canonical);
[[nodiscard]] double toCanonical(Quantity quantity, const QString &unit, double display);

// "°C", "mph", "inHg". Empty for a quantity with no unit.
[[nodiscard]] QString symbol(Quantity quantity, const QString &unit);

// How many decimals a displayed value wants: two for inches of rain, none for
// a temperature.
[[nodiscard]] int decimals(Quantity quantity, const QString &unit);

// The two presets, as the five names each one writes. `metric` is also what
// a fresh install reads, so a settings file with no unit keys is a metric one
// and not a "custom" one.
struct Preset {
    QString id;
    QString temperature;
    QString wind;
    QString pressure;
    QString visibility;
    QString precipitation;
};

[[nodiscard]] const Preset &metric();
[[nodiscard]] const Preset &imperial();

// The preset a set of five names amounts to: "metric", "imperial" or "custom".
[[nodiscard]] QString presetFor(const QString &temperature, const QString &wind,
                                const QString &pressure, const QString &visibility,
                                const QString &precipitation);

} // namespace clima::units
