// SPDX-FileCopyrightText: 2026 Jowi Aoun
// SPDX-License-Identifier: MPL-2.0

#include "cachepolicy.h"

namespace clima {

using namespace std::chrono_literals;

CachePolicy policyFor(DataKind kind)
{
    // docs/04-architecture.md §4.5, transcribed. Left column is that table's
    // "Data", then TTL, then Revalidation, then Stale-while-revalidate.
    //
    //   Current conditions          10 min    ETag / If-Modified-Since   yes
    //   Hourly / daily forecast     30 min    ETag                       yes
    //   15-minute nowcast            5 min    —                          yes
    //   Ensemble / model comparison 60 min    —                          yes
    //   Air quality                 60 min    —                          yes
    //   Alerts                       3 min    CAP sent/expires           NO
    //   Radar frames                 5 min    timeline manifest          yes
    //   Basemap tiles               30 days   —                          yes
    //   Historical archive / ERA5   immutable —                          n/a
    //   Geocoding results            7 days   —                          yes
    switch (kind) {
    case DataKind::CurrentConditions:
        return { 10min, Revalidation::EntityTag, true, false };

    case DataKind::Forecast:
        return { 30min, Revalidation::EntityTag, true, false };

    case DataKind::Nowcast:
        // Five minutes because that is the resolution of the product itself;
        // caching a fifteen-minute nowcast for longer than one refresh cycle
        // means showing the user rain that has already happened.
        return { 5min, Revalidation::None, true, false };

    case DataKind::Ensemble:
        return { 60min, Revalidation::None, true, false };

    case DataKind::AirQuality:
        // Sixty minutes against a source that updates twice a day. The TTL is
        // not tracking the data's rate of change, it is bounding how long we
        // hold a stale row before asking again — CAMS publishes at
        // unpredictable wall-clock times and there is no validator to ask with.
        return { 60min, Revalidation::None, true, false };

    case DataKind::Alerts:
        // The only row with staleWhileRevalidate false, and the only one where
        // that is a safety property rather than a freshness preference. §4.5:
        // "never show an expired alert". The CAP message's own <expires> is
        // the authority — Revalidation::CapLifetime — and this three-minute
        // TTL only decides how often we ask, not how long a warning is valid.
        return { 3min, Revalidation::CapLifetime, false, false };

    case DataKind::RadarFrame:
        // "Frame lifetime (5 min)". A frame is immutable once published, but
        // the *manifest* that names the current frames is not, so the entry
        // expires with the frame rather than living forever like the archive.
        return { 5min, Revalidation::None, true, false };

    case DataKind::BasemapTile:
        // Thirty days, and the cap on the whole tile directory is a separate
        // concern — §4.5 puts tiles in a size-capped LRU directory of their
        // own, default 200 MB, precisely because a TTL is not a size bound.
        return { 24h * 30, Revalidation::None, true, false };

    case DataKind::HistoricalArchive:
        // ERA5 reanalysis for a day in the past does not change. Refetching it
        // is pure waste, and this is the row that makes the "store the raw
        // payload" decision pay for itself twice over.
        return { 0s, Revalidation::None, true, true };

    case DataKind::Geocoding:
        // Seven days, keyed by query and language — the key is built by the
        // caller and includes both, because "Paris" in French and "Paris" in
        // Japanese are two answers to two questions.
        return { 24h * 7, Revalidation::None, true, false };
    }

    // Unreachable for a total switch, and present because a function that
    // falls off the end is undefined behaviour rather than a warning. A kind
    // that reaches here caches for nothing and revalidates nothing, which
    // fails safe: it fetches every time.
    return {};
}

QString dataKindName(DataKind kind)
{
    switch (kind) {
    case DataKind::CurrentConditions: return QStringLiteral("current");
    case DataKind::Forecast:          return QStringLiteral("forecast");
    case DataKind::Nowcast:           return QStringLiteral("nowcast");
    case DataKind::Ensemble:          return QStringLiteral("ensemble");
    case DataKind::AirQuality:        return QStringLiteral("air-quality");
    case DataKind::Alerts:            return QStringLiteral("alerts");
    case DataKind::RadarFrame:        return QStringLiteral("radar-frame");
    case DataKind::BasemapTile:       return QStringLiteral("basemap-tile");
    case DataKind::HistoricalArchive: return QStringLiteral("historical-archive");
    case DataKind::Geocoding:         return QStringLiteral("geocoding");
    }
    return QStringLiteral("unknown");
}

DataKind dataKindFromName(const QString &name, bool *ok)
{
    static const struct {
        const char *name;
        DataKind    kind;
    } table[] = {
        { "current",            DataKind::CurrentConditions },
        { "forecast",           DataKind::Forecast          },
        { "nowcast",            DataKind::Nowcast           },
        { "ensemble",           DataKind::Ensemble          },
        { "air-quality",        DataKind::AirQuality        },
        { "alerts",             DataKind::Alerts            },
        { "radar-frame",        DataKind::RadarFrame        },
        { "basemap-tile",       DataKind::BasemapTile       },
        { "historical-archive", DataKind::HistoricalArchive },
        { "geocoding",          DataKind::Geocoding         },
    };

    for (const auto &row : table) {
        if (name == QLatin1String(row.name)) {
            if (ok != nullptr)
                *ok = true;
            return row.kind;
        }
    }

    // An unrecognised name is a row written by a newer Clima than this one, or
    // by a corrupted file. Reporting it as Forecast with ok=false lets the
    // caller drop the row rather than crash, and callers do drop it.
    if (ok != nullptr)
        *ok = false;
    return DataKind::Forecast;
}

QDateTime expiryFor(DataKind kind, const QDateTime &fetchedAt)
{
    const CachePolicy policy = policyFor(kind);
    if (policy.immutable)
        return {};
    return fetchedAt.addSecs(policy.ttl.count());
}

} // namespace clima
