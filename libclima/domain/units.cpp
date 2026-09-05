// SPDX-FileCopyrightText: 2026 Jowi Aoun
// SPDX-License-Identifier: MPL-2.0

#include "units.h"

#include <cmath>

namespace clima::units {

double convert(Quantity quantity, const QString &unit, double canonical)
{
    switch (quantity) {
    case Quantity::Temperature:
        if (unit == QLatin1String("fahrenheit"))
            return canonical * 9.0 / 5.0 + 32.0;
        return canonical;

    case Quantity::Wind:
        if (unit == QLatin1String("mph"))
            return canonical * 0.621371;
        if (unit == QLatin1String("ms"))
            return canonical / 3.6;
        if (unit == QLatin1String("kn"))
            return canonical * 0.539957;
        if (unit == QLatin1String("bft"))
            // Beaufort from km/h, the inverse of the standard v = 0.836 B^1.5
            // in m/s. Rounded by the caller like everything else; the fraction
            // is meaningless but the truncation belongs at the formatting step
            // so that a chart can still draw a smooth curve.
            return std::pow(canonical / 3.6 / 0.836, 2.0 / 3.0);
        return canonical;

    case Quantity::Pressure:
        if (unit == QLatin1String("inhg"))
            return canonical * 0.02952998;
        if (unit == QLatin1String("mmhg"))
            return canonical * 0.7500617;
        // "mb" is hPa under another name — one millibar is one hectopascal
        // exactly — and it is offered because that is the word half the world's
        // forecasts use.
        return canonical;

    case Quantity::Visibility:
        if (unit == QLatin1String("mi"))
            return canonical * 0.621371;
        return canonical;

    case Quantity::Precipitation:
        if (unit == QLatin1String("in"))
            return canonical / 25.4;
        return canonical;

    case Quantity::None:
    case Quantity::Percentage:
    case Quantity::Direction:
        return canonical;
    }
    return canonical;
}

double toCanonical(Quantity quantity, const QString &unit, double display)
{
    switch (quantity) {
    case Quantity::Temperature:
        if (unit == QLatin1String("fahrenheit"))
            return (display - 32.0) * 5.0 / 9.0;
        return display;

    case Quantity::Wind:
        if (unit == QLatin1String("mph"))
            return display / 0.621371;
        if (unit == QLatin1String("ms"))
            return display * 3.6;
        if (unit == QLatin1String("kn"))
            return display / 0.539957;
        if (unit == QLatin1String("bft"))
            return 0.836 * std::pow(display, 1.5) * 3.6;
        return display;

    case Quantity::Pressure:
        if (unit == QLatin1String("inhg"))
            return display / 0.02952998;
        if (unit == QLatin1String("mmhg"))
            return display / 0.7500617;
        return display;

    case Quantity::Visibility:
        if (unit == QLatin1String("mi"))
            return display / 0.621371;
        return display;

    case Quantity::Precipitation:
        if (unit == QLatin1String("in"))
            return display * 25.4;
        return display;

    case Quantity::None:
    case Quantity::Percentage:
    case Quantity::Direction:
        return display;
    }
    return display;
}

QString symbol(Quantity quantity, const QString &unit)
{
    switch (quantity) {
    case Quantity::Temperature:
        return unit == QLatin1String("fahrenheit") ? QStringLiteral("°F") : QStringLiteral("°C");

    case Quantity::Wind:
        if (unit == QLatin1String("mph")) return QStringLiteral("mph");
        if (unit == QLatin1String("ms"))  return QStringLiteral("m/s");
        if (unit == QLatin1String("kn"))  return QStringLiteral("kn");
        if (unit == QLatin1String("bft")) return QStringLiteral("Bft");
        return QStringLiteral("km/h");

    case Quantity::Pressure:
        if (unit == QLatin1String("inhg")) return QStringLiteral("inHg");
        if (unit == QLatin1String("mmhg")) return QStringLiteral("mmHg");
        if (unit == QLatin1String("mb"))   return QStringLiteral("mb");
        return QStringLiteral("hPa");

    case Quantity::Visibility:
        return unit == QLatin1String("mi") ? QStringLiteral("mi") : QStringLiteral("km");

    case Quantity::Precipitation:
        return unit == QLatin1String("in") ? QStringLiteral("in") : QStringLiteral("mm");

    case Quantity::Percentage:
        return QStringLiteral("%");

    case Quantity::Direction:
        return QStringLiteral("°");

    case Quantity::None:
        break;
    }
    return {};
}

int decimals(Quantity quantity, const QString &unit)
{
    switch (quantity) {
    case Quantity::Precipitation:
        return unit == QLatin1String("in") ? 2 : 1;
    case Quantity::Pressure:
        return unit == QLatin1String("inhg") ? 2 : 0;
    case Quantity::Wind:
        return unit == QLatin1String("ms") ? 1 : 0;
    default:
        return 0;
    }
}

const Preset &metric()
{
    static const Preset preset{ QStringLiteral("metric"),
                                QStringLiteral("celsius"), QStringLiteral("kmh"),
                                QStringLiteral("hpa"), QStringLiteral("km"),
                                QStringLiteral("mm") };
    return preset;
}

const Preset &imperial()
{
    // inHg and not mb, miles and not km: this is the bundle a US reader means by
    // "imperial", and every entry is one the app's unit picker already offers.
    static const Preset preset{ QStringLiteral("imperial"),
                                QStringLiteral("fahrenheit"), QStringLiteral("mph"),
                                QStringLiteral("inhg"), QStringLiteral("mi"),
                                QStringLiteral("in") };
    return preset;
}

QString presetFor(const QString &temperature, const QString &wind, const QString &pressure,
                  const QString &visibility, const QString &precipitation)
{
    const auto matches = [&](const Preset &preset) {
        return temperature == preset.temperature && wind == preset.wind
            && pressure == preset.pressure && visibility == preset.visibility
            && precipitation == preset.precipitation;
    };
    if (matches(metric()))
        return metric().id;
    if (matches(imperial()))
        return imperial().id;
    return QStringLiteral("custom");
}

} // namespace clima::units
