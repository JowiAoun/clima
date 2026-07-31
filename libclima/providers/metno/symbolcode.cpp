// SPDX-FileCopyrightText: 2026 Jowi Aoun
// SPDX-License-Identifier: MPL-2.0

#include "libclima/providers/metno/symbolcode.h"

#include <QHash>
#include <QSet>

namespace clima {

namespace {

// ---- the table --------------------------------------------------------------
//
// 44 base symbols, MET's whole vocabulary, in the order their legend lists them
// so the two can be read side by side. The WMO codes and the reasoning behind
// the awkward ones are in symbolcode.h; the short version:
//
//   * sleet gets the real WMO mixed-precipitation codes 68/69 and 83/84, which
//     Open-Meteo never emits,
//   * thunder is always 95, because MET does not distinguish hail and 96/99
//     claim it,
//   * WMO has two intensity levels for snow showers and mixed precipitation
//     where MET has three, so "moderate" and "heavy" collapse onto the upper
//     one. The collapse is upward: calling heavy snow "moderate" is a smaller
//     lie than calling moderate snow "heavy", and the direction of a lie about
//     weather severity matters.
const QHash<QString, int> &symbolTable()
{
    static const QHash<QString, int> table = {
        // sky
        { QStringLiteral("clearsky"), 0 },
        { QStringLiteral("fair"), 1 },
        { QStringLiteral("partlycloudy"), 2 },
        { QStringLiteral("cloudy"), 3 },
        { QStringLiteral("fog"), 45 },

        // rain showers
        { QStringLiteral("lightrainshowers"), 80 },
        { QStringLiteral("rainshowers"), 81 },
        { QStringLiteral("heavyrainshowers"), 82 },
        { QStringLiteral("lightrainshowersandthunder"), 95 },
        { QStringLiteral("rainshowersandthunder"), 95 },
        { QStringLiteral("heavyrainshowersandthunder"), 95 },

        // sleet showers — 83/84, "showers of rain and snow mixed"
        { QStringLiteral("lightsleetshowers"), 83 },
        { QStringLiteral("sleetshowers"), 84 },
        { QStringLiteral("heavysleetshowers"), 84 },
        // upstream's typo, and the spelling it would have if it were corrected.
        { QStringLiteral("lightssleetshowersandthunder"), 95 },
        { QStringLiteral("lightsleetshowersandthunder"), 95 },
        { QStringLiteral("sleetshowersandthunder"), 95 },
        { QStringLiteral("heavysleetshowersandthunder"), 95 },

        // snow showers
        { QStringLiteral("lightsnowshowers"), 85 },
        { QStringLiteral("snowshowers"), 86 },
        { QStringLiteral("heavysnowshowers"), 86 },
        { QStringLiteral("lightssnowshowersandthunder"), 95 },
        { QStringLiteral("lightsnowshowersandthunder"), 95 },
        { QStringLiteral("snowshowersandthunder"), 95 },
        { QStringLiteral("heavysnowshowersandthunder"), 95 },

        // steady rain
        { QStringLiteral("lightrain"), 61 },
        { QStringLiteral("rain"), 63 },
        { QStringLiteral("heavyrain"), 65 },
        { QStringLiteral("lightrainandthunder"), 95 },
        { QStringLiteral("rainandthunder"), 95 },
        { QStringLiteral("heavyrainandthunder"), 95 },

        // steady sleet — 68/69, "rain or drizzle and snow"
        { QStringLiteral("lightsleet"), 68 },
        { QStringLiteral("sleet"), 69 },
        { QStringLiteral("heavysleet"), 69 },
        { QStringLiteral("lightsleetandthunder"), 95 },
        { QStringLiteral("sleetandthunder"), 95 },
        { QStringLiteral("heavysleetandthunder"), 95 },

        // steady snow
        { QStringLiteral("lightsnow"), 71 },
        { QStringLiteral("snow"), 73 },
        { QStringLiteral("heavysnow"), 75 },
        { QStringLiteral("lightsnowandthunder"), 95 },
        { QStringLiteral("snowandthunder"), 95 },
        { QStringLiteral("heavysnowandthunder"), 95 },
    };
    return table;
}

} // namespace

SymbolCode parseSymbolCode(const QString &symbol)
{
    SymbolCode parsed;
    if (symbol.isEmpty())
        return parsed;

    // Upstream's legend has a trailing space inside one of the symbol IDs
    // (`lightssleetshowersandthunder `). It has never appeared in a payload, but
    // trimming costs nothing and the alternative is a bug whose only symptom is
    // one missing icon in one weather condition.
    QString base = symbol.trimmed();

    const int underscore = base.indexOf(QLatin1Char('_'));
    if (underscore > 0) {
        const QString variant = base.mid(underscore + 1);
        base = base.left(underscore);

        // polartwilight is neither day nor night — it is the long dusk above
        // the Arctic circle, which is exactly the condition MET's own users
        // care about and exactly the one a boolean cannot hold. Left absent,
        // so a UI picks its twilight artwork from the sun position rather than
        // being told, wrongly, that it is one or the other.
        if (variant == QLatin1String("day"))
            parsed.isDay = true;
        else if (variant == QLatin1String("night"))
            parsed.isDay = false;
    }

    const auto entry = symbolTable().constFind(base);
    if (entry == symbolTable().cend())
        return parsed;   // unknown: no code, and deliberately no guess

    parsed.code = *entry;
    return parsed;
}

QList<int> metNoWeatherCodes()
{
    QSet<int> codes;
    for (auto it = symbolTable().cbegin(); it != symbolTable().cend(); ++it)
        codes.insert(it.value());

    QList<int> sorted = codes.values();
    std::sort(sorted.begin(), sorted.end());
    return sorted;
}

QStringList metNoSymbolNames()
{
    QStringList names = symbolTable().keys();
    names.sort();
    return names;
}

} // namespace clima
