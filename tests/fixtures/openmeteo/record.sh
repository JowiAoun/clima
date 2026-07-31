#!/usr/bin/env bash
# SPDX-FileCopyrightText: 2026 Jowi Aoun
# SPDX-License-Identifier: GPL-3.0-or-later
#
# How the fixtures beside this file were captured. Provenance you can run
# rather than provenance you have to believe.
#
# NOT part of the build and NOT run by CI. Nothing in tests/ may reach a
# network (docs/04-architecture.md §4.11); this is a developer action taken
# once, whose *output* is what the tests use.
#
# Read README.md before running it. These are golden files: every assertion in
# tests/tst_openmeteoadapter.cpp names a specific index, a specific millimetre
# and a specific minute, and a fresh capture changes all of them for no gain.
# Re-record only when a variable is being added to the request, and expect to
# re-check every literal in that file by hand afterwards.
#
#   ./record.sh            write into this directory
#   ./record.sh /tmp/new   write somewhere else, to diff against what is here

set -euo pipefail

out="${1:-$(dirname "$0")}"
mkdir -p "$out"

forecast="https://api.open-meteo.com/v1/forecast"
archive="https://archive-api.open-meteo.com/v1/archive"

# The variable lists. These must stay identical to
# libclima/providers/openmeteo/openmeteovariables.cpp, with four exceptions
# that are deliberate: cloud_cover_low/mid/high and snow_depth are recorded but
# not requested in production. Nothing in app/qml/Clima/ reads them today, so
# asking for them on every refresh would be waste — but a fixture that already
# contains them means the day a card wants one, the golden files have the
# answer and only the production list has to change.
hourly='temperature_2m,apparent_temperature,dew_point_2m,relative_humidity_2m,precipitation_probability,precipitation,rain,showers,snowfall,snow_depth,weather_code,cloud_cover,cloud_cover_low,cloud_cover_mid,cloud_cover_high,pressure_msl,surface_pressure,wind_speed_10m,wind_gusts_10m,wind_direction_10m,uv_index,visibility,is_day'
daily='temperature_2m_max,temperature_2m_min,apparent_temperature_max,apparent_temperature_min,precipitation_sum,precipitation_probability_max,precipitation_hours,weather_code,sunrise,sunset,daylight_duration,sunshine_duration,uv_index_max,wind_speed_10m_max,wind_gusts_10m_max,wind_direction_10m_dominant,moon_phase,moonrise,moonset'
current='temperature_2m,apparent_temperature,dew_point_2m,relative_humidity_2m,precipitation,rain,showers,snowfall,weather_code,cloud_cover,pressure_msl,surface_pressure,wind_speed_10m,wind_gusts_10m,wind_direction_10m,uv_index,visibility,is_day'

# The archive endpoint serves a smaller variable set. That is fine: the two DST
# fixtures are about the time axis and nothing else.
archive_hourly='temperature_2m,apparent_temperature,dew_point_2m,relative_humidity_2m,precipitation,rain,snowfall,snow_depth,weather_code,cloud_cover,cloud_cover_low,cloud_cover_mid,cloud_cover_high,pressure_msl,surface_pressure,wind_speed_10m,wind_gusts_10m,wind_direction_10m,is_day'
archive_daily='temperature_2m_max,temperature_2m_min,apparent_temperature_max,apparent_temperature_min,precipitation_sum,precipitation_hours,weather_code,sunrise,sunset,daylight_duration,sunshine_duration,wind_speed_10m_max,wind_gusts_10m_max,wind_direction_10m_dominant'

grab() {
    local name="$1" url="$2"
    curl --fail --silent --show-error --output "$out/$name" "$url"
    printf '%8d B  %s\n' "$(stat -c%s "$out/$name")" "$name"
}

# The canonical response: sixteen days forward, one back, everything.
grab toronto-summer.json \
    "$forecast?latitude=43.6532&longitude=-79.3832&timezone=auto&past_days=1&forecast_days=16&current=$current&hourly=$hourly&daily=$daily"

# One isolated wet hour, which is what makes an off-by-one visible. Found by
# scanning a dozen cities for a hour above 0.4 mm with dry hours either side —
# models smear precipitation, so these are rarer than you would expect.
grab kampala-precip-spike.json \
    "$forecast?latitude=0.3152&longitude=32.5816&timezone=auto&forecast_days=2&current=$current&hourly=$hourly&daily=$daily"

# Thunder and hail (WMO 95, 96), which no amount-plus-temperature fallback can
# derive, plus the heaviest rain in the set.
grab miami-thunder.json \
    "$forecast?latitude=25.7617&longitude=-80.1918&timezone=auto&forecast_days=7&current=$current&hourly=$hourly&daily=$daily"

# Snow in July, at 2500 m in the Chilean Andes: WMO 71/73/75/85/86, and
# snowfall in centimetres beside precipitation in millimetres.
grab andes-snow.json \
    "$forecast?latitude=-33.45&longitude=-70.05&timezone=auto&forecast_days=7&current=$current&hourly=$hourly&daily=$daily"

# Midnight sun, and a moon that neither rises nor sets.
grab svalbard-midnight-sun.json \
    "$forecast?latitude=78.2232&longitude=15.6267&timezone=auto&forecast_days=4&current=$current&hourly=$hourly&daily=$daily"

# A model that does not carry UV or visibility. Same endpoint, same parameters,
# one extra: `models`.
grab toronto-ecmwf-gaps.json \
    "$forecast?latitude=43.6532&longitude=-79.3832&timezone=auto&forecast_days=3&models=ecmwf_ifs025&current=$current&hourly=$hourly&daily=$daily"

# The two DST days. From the archive because the forecast endpoint only reaches
# 92 days back and 16 forward, and no transition falls in that window in July.
# README.md explains why the substitution is sound.
grab toronto-dst-fall.json \
    "$archive?latitude=43.6532&longitude=-79.3832&timezone=auto&start_date=2025-11-01&end_date=2025-11-03&hourly=$archive_hourly&daily=$archive_daily"

grab toronto-dst-spring.json \
    "$archive?latitude=43.6532&longitude=-79.3832&timezone=auto&start_date=2026-03-07&end_date=2026-03-09&hourly=$archive_hourly&daily=$archive_daily"

echo
echo "Recorded into $out. Now re-check every literal in tests/tst_openmeteoadapter.cpp."
