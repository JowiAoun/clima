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

#include <QLatin1StringView>
#include <QLocale>
#include <QString>

namespace clima::settingskeys {

// "12h" | "24h"
constexpr auto clockFormat = "time/format";

// What a reader who has never touched the switch gets: their own locale's
// answer. Both readers of the INI call this, which is the point of it being
// here — the app through Settings, the CLI through its own QSettings, and a
// default that differed between them would be the two of them disagreeing
// about what time it is on the same machine.
//
// QLocale spells a 12-hour format with AP or ap and a 24-hour one with neither,
// so the presence of that letter is the question. It is asked of the SHORT
// format because that is the one this app prints — a long format carries a
// timezone name nothing here shows.
//
// This used to be a flat "12h", and docs/known-gaps.md carried the entry
// saying why that was wrong outside North America: a reader in Paris got
// "3 PM" until they found the switch. What kept it flat was the capture path
// rather than the clock — every golden image runs under LC_ALL=C.UTF-8, whose
// short format is 24-hour, so a locale-derived default would have re-recorded
// most of the reference images and made them a picture of the C locale rather
// than of the product. scripts/golden.sh and scripts/shots.sh now write the
// preference into their own scratch config instead, which pins the pictures
// explicitly and leaves the default free to follow the reader.
[[nodiscard]] inline QString defaultClockFormat()
{
    const QString format = QLocale().timeFormat(QLocale::ShortFormat);
    return format.contains(QLatin1Char('A'), Qt::CaseInsensitive) ? QStringLiteral("12h")
                                                                  : QStringLiteral("24h");
}

// The five unit preferences. Values are the spellings libclima/domain/units.h
// converts by: "celsius" | "fahrenheit", "kmh" | "mph" | "ms" | "kn" | "bft",
// "hpa" | "mb" | "inhg" | "mmhg", "km" | "mi", "mm" | "in".
constexpr auto temperatureUnit   = "units/temperature";
constexpr auto windUnit          = "units/wind";
constexpr auto pressureUnit      = "units/pressure";
constexpr auto visibilityUnit    = "units/visibility";
constexpr auto precipitationUnit = "units/precipitation";

} // namespace clima::settingskeys
