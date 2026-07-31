// SPDX-FileCopyrightText: 2026 Jowi Aoun
// SPDX-License-Identifier: MPL-2.0

#include "openmeteovariables.h"

namespace clima {
namespace openmeteo {
namespace {

QString join(const QList<QLatin1String> &names)
{
    QString out;
    for (const QLatin1String &name : names) {
        if (!out.isEmpty())
            out += QLatin1Char(',');
        out += name;
    }
    return out;
}

} // namespace

QList<QLatin1String> hourlyVariables()
{
    // Grouped by the card or tab that consumes them, which is also roughly the
    // order app/qml/Clima/metrics.js lists its metrics in.
    return {
        QLatin1String("temperature_2m"),             // Overview tab, hero, DetailTemperatureCard
        QLatin1String("apparent_temperature"),       // Feels-like tab, DetailFeelsLikeCard
        QLatin1String("dew_point_2m"),               // DetailHumidityCard's second number
        QLatin1String("relative_humidity_2m"),       // Humidity tab
        QLatin1String("precipitation_probability"),  // the bucket bars under the chart
        QLatin1String("precipitation"),              // the wash, and the Precipitation tab
        QLatin1String("rain"),                       // the rain/showers/snow split
        QLatin1String("showers"),
        QLatin1String("snowfall"),
        QLatin1String("weather_code"),               // the glyph, and the precipitation type
        QLatin1String("cloud_cover"),                // Cloud cover tab
        QLatin1String("pressure_msl"),               // Pressure tab
        QLatin1String("wind_speed_10m"),             // Wind tab
        QLatin1String("wind_gusts_10m"),             // its overlay
        QLatin1String("wind_direction_10m"),
        QLatin1String("uv_index"),                   // UV tab
        QLatin1String("visibility"),                 // Visibility tab — METRES, see the adapter
        QLatin1String("is_day"),                     // day/night glyphs and the past veil
    };
}

QList<QLatin1String> dailyVariables()
{
    return {
        QLatin1String("temperature_2m_max"),         // the ten-day strip's high
        QLatin1String("temperature_2m_min"),         // and its low
        QLatin1String("apparent_temperature_max"),
        QLatin1String("apparent_temperature_min"),
        QLatin1String("precipitation_sum"),          // DetailPrecipitationCard's 24 h total
        QLatin1String("precipitation_probability_max"), // the strip's per-day percentage
        QLatin1String("precipitation_hours"),
        QLatin1String("weather_code"),               // the strip's day glyph
        QLatin1String("sunrise"),
        QLatin1String("sunset"),
        QLatin1String("daylight_duration"),          // "14 hrs 39 mins", and polar days
        QLatin1String("sunshine_duration"),
        QLatin1String("uv_index_max"),               // DetailUvCard's peak
        QLatin1String("wind_speed_10m_max"),
        QLatin1String("wind_gusts_10m_max"),
        QLatin1String("wind_direction_10m_dominant"),

        // The three that delete a planned task. DetailMoonCard needs a phase,
        // a rise and a set, and the alternative to these was a local ephemeris
        // — Meeus' lunar terms, a few hundred lines of astronomy nobody here
        // would be qualified to review. Open-Meteo serves all three, verified
        // against the live endpoint, so the astronomy stays upstream.
        QLatin1String("moon_phase"),
        QLatin1String("moonrise"),
        QLatin1String("moonset"),
    };
}

QList<QLatin1String> currentVariables()
{
    // No `precipitation_probability`: Open-Meteo has no current value for it,
    // and a `current=` list naming a variable it does not serve is a 400 for
    // the entire request rather than a missing field.
    return {
        QLatin1String("temperature_2m"),
        QLatin1String("apparent_temperature"),
        QLatin1String("dew_point_2m"),
        QLatin1String("relative_humidity_2m"),
        QLatin1String("precipitation"),
        QLatin1String("rain"),
        QLatin1String("showers"),
        QLatin1String("snowfall"),
        QLatin1String("weather_code"),
        QLatin1String("cloud_cover"),
        QLatin1String("pressure_msl"),
        QLatin1String("wind_speed_10m"),
        QLatin1String("wind_gusts_10m"),
        QLatin1String("wind_direction_10m"),
        QLatin1String("uv_index"),
        QLatin1String("visibility"),
        QLatin1String("is_day"),
    };
}

QString hourlyParameter()
{
    return join(hourlyVariables());
}

QString dailyParameter()
{
    return join(dailyVariables());
}

QString currentParameter()
{
    return join(currentVariables());
}

} // namespace openmeteo
} // namespace clima
