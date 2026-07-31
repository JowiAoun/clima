// SPDX-FileCopyrightText: 2026 Jowi Aoun
// SPDX-License-Identifier: MPL-2.0

#include "weathercode.h"

#include <QCoreApplication>

namespace clima {
namespace {

// The context every phrase below is translated in. Spelled once so a
// translator sees one context in the .ts file rather than one per call.
const char *const kContext = "clima::WeatherCode";

QString tr(const char *source)
{
    return QCoreApplication::translate(kContext, source);
}

} // namespace

QString precipitationTypeName(PrecipitationType type)
{
    // The six entries of precip.js's TYPES array, spelled its way.
    switch (type) {
    case PrecipitationType::None:    return {};
    case PrecipitationType::Drizzle: return QStringLiteral("drizzle");
    case PrecipitationType::Rain:    return QStringLiteral("rain");
    case PrecipitationType::Sleet:   return QStringLiteral("sleet");
    case PrecipitationType::Snow:    return QStringLiteral("snow");
    case PrecipitationType::Hail:    return QStringLiteral("hail");
    case PrecipitationType::Thunder: return QStringLiteral("thunder");
    }
    return {};
}

QString conditionKindName(ConditionKind kind)
{
    switch (kind) {
    case ConditionKind::ClearDay:     return QStringLiteral("clear-day");
    case ConditionKind::ClearNight:   return QStringLiteral("clear-night");
    case ConditionKind::PartlyDay:    return QStringLiteral("partly-day");
    case ConditionKind::PartlyNight:  return QStringLiteral("partly-night");
    case ConditionKind::Cloudy:       return QStringLiteral("cloudy");
    case ConditionKind::Fog:          return QStringLiteral("fog");
    case ConditionKind::Drizzle:      return QStringLiteral("drizzle");
    case ConditionKind::Rain:         return QStringLiteral("rain");
    case ConditionKind::RainNight:    return QStringLiteral("rain-night");
    case ConditionKind::Sleet:        return QStringLiteral("sleet");
    case ConditionKind::Snow:         return QStringLiteral("snow");
    case ConditionKind::Thunder:      return QStringLiteral("thunder");
    case ConditionKind::Hail:         return QStringLiteral("hail");
    }
    return QStringLiteral("cloudy");
}

ConditionKind drawableToday(ConditionKind kind)
{
    switch (kind) {
    case ConditionKind::Fog:
    case ConditionKind::Snow:
        return ConditionKind::Cloudy;

    case ConditionKind::Drizzle:
    case ConditionKind::Sleet:
    case ConditionKind::Thunder:
    case ConditionKind::Hail:
        return ConditionKind::Rain;

    case ConditionKind::ClearDay:
    case ConditionKind::ClearNight:
    case ConditionKind::PartlyDay:
    case ConditionKind::PartlyNight:
    case ConditionKind::Cloudy:
    case ConditionKind::Rain:
    case ConditionKind::RainNight:
        return kind;
    }
    return kind;
}

PrecipitationType precipitationTypeFor(int wmoCode)
{
    // WMO 4677, as Open-Meteo emits it. Grouped by what falls out of the sky
    // and not by the table's own numbering, because the numbering interleaves
    // intensity with phase (61/63/65 is one thing at three strengths, 66/67 is
    // a different thing at two) and grouping by number would hide that.
    switch (wmoCode) {
    case 51: case 53: case 55:          // drizzle, light → dense
    case 56: case 57:                   // freezing drizzle
        return PrecipitationType::Drizzle;

    case 61: case 63: case 65:          // rain, slight → heavy
    case 80: case 81: case 82:          // rain showers, slight → violent
        return PrecipitationType::Rain;

    case 66: case 67:                   // freezing rain — ice, and it reads as sleet
        return PrecipitationType::Sleet;

    case 71: case 73: case 75:          // snowfall, slight → heavy
    case 77:                            // snow grains
    case 85: case 86:                   // snow showers
        return PrecipitationType::Snow;

    case 95:                            // thunderstorm, slight or moderate
        return PrecipitationType::Thunder;

    case 96: case 99:                   // thunderstorm with hail
        return PrecipitationType::Hail;

    default:
        return PrecipitationType::None;
    }
}

ConditionKind conditionFor(int wmoCode, bool isDay)
{
    switch (wmoCode) {
    case 0:
    case 1:
        // 0 is clear sky, 1 is mainly clear. They get the same glyph because
        // the difference is a few per cent of cloud, which no icon at 26 px
        // can show and no reader would look for.
        return isDay ? ConditionKind::ClearDay : ConditionKind::ClearNight;

    case 2:
        return isDay ? ConditionKind::PartlyDay : ConditionKind::PartlyNight;

    case 3:
        return ConditionKind::Cloudy;

    case 45: case 48:
        return ConditionKind::Fog;

    case 51: case 53: case 55:
    case 56: case 57:
        return ConditionKind::Drizzle;

    case 61: case 63: case 65:
    case 80: case 81: case 82:
        return isDay ? ConditionKind::Rain : ConditionKind::RainNight;

    case 66: case 67:
        return ConditionKind::Sleet;

    case 71: case 73: case 75:
    case 77:
    case 85: case 86:
        return ConditionKind::Snow;

    case 95:
        return ConditionKind::Thunder;

    case 96: case 99:
        return ConditionKind::Hail;

    default:
        // Including -1, which is how the adapter spells "this hour had no
        // code". Overcast is the honest picture for "something is up there and
        // we do not know what".
        return ConditionKind::Cloudy;
    }
}

QString conditionText(int wmoCode, bool isDay)
{
    switch (wmoCode) {
    case 0:  return isDay ? tr("Sunny") : tr("Clear");
    case 1:  return isDay ? tr("Mainly sunny") : tr("Mainly clear");

    // "Partly sunny" by day and "Partly cloudy" by night is not a translation
    // artefact — it is what the reference forecast says and what
    // app/qml/Clima/mockdata.js already writes. A clear night is not sunny.
    case 2:  return isDay ? tr("Partly sunny") : tr("Partly cloudy");

    case 3:  return tr("Cloudy");

    case 45: return tr("Fog");
    case 48: return tr("Freezing fog");

    case 51: return tr("Light drizzle");
    case 53: return tr("Drizzle");
    case 55: return tr("Heavy drizzle");
    case 56: return tr("Light freezing drizzle");
    case 57: return tr("Freezing drizzle");

    case 61: return tr("Light rain");
    case 63: return tr("Rain");
    case 65: return tr("Heavy rain");
    case 66: return tr("Light freezing rain");
    case 67: return tr("Freezing rain");

    case 71: return tr("Light snow");
    case 73: return tr("Snow");
    case 75: return tr("Heavy snow");
    case 77: return tr("Snow grains");

    case 80: return tr("Light rain showers");
    case 81: return tr("Rain showers");
    case 82: return tr("Heavy rain showers");

    case 85: return tr("Light snow showers");
    case 86: return tr("Snow showers");

    case 95: return tr("Thunderstorm");
    case 96: return tr("Thunderstorm with hail");
    case 99: return tr("Thunderstorm with heavy hail");

    default: return {};
    }
}

} // namespace clima
