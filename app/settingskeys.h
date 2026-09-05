// SPDX-FileCopyrightText: 2026 Jowi Aoun
// SPDX-License-Identifier: GPL-3.0-or-later
//
// The INI keys, named once.
//
// app/settings.cpp reads and writes them through Settings, which QML binds to.
// clima-cli reads them through a bare QSettings, because Settings is a QML
// singleton and a command-line tool that linked a QML engine to learn whether
// the reader prefers Fahrenheit would be paying for the whole of Qt Quick to
// print one character. Two readers, one list — a key renamed here is renamed
// in both, and a key renamed in only one would be a preference the app wrote
// and the CLI silently ignored.
//
// Plain string constants in a namespace, with no Qt object in sight, so that
// including this from a GUI-free binary costs nothing.

#pragma once

namespace clima::settingskeys {

// "12h" | "24h"
constexpr auto clockFormat = "time/format";


// The five unit preferences. Values are the spellings libclima/domain/units.h
// converts by: "celsius" | "fahrenheit", "kmh" | "mph" | "ms" | "kn" | "bft",
// "hpa" | "mb" | "inhg" | "mmhg", "km" | "mi", "mm" | "in".
constexpr auto temperatureUnit   = "units/temperature";
constexpr auto windUnit          = "units/wind";
constexpr auto pressureUnit      = "units/pressure";
constexpr auto visibilityUnit    = "units/visibility";
constexpr auto precipitationUnit = "units/precipitation";

} // namespace clima::settingskeys
