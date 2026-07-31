// SPDX-FileCopyrightText: 2026 Jowi Aoun
// SPDX-License-Identifier: MPL-2.0
//
// The variable lists, in one place, because the request and the parser have to
// agree and there is no compiler that will say so.
//
// Every name below is asked for in a query string and read out of a JSON
// object by the same spelling. Split across two files they drift the moment
// somebody adds a variable to one of them, and the symptom is a column of
// absent Readings for a variable the response does not contain — which looks
// exactly like a provider that does not have it at that location, which is a
// thing that genuinely happens (see tests/fixtures/openmeteo/toronto-ecmwf-gaps.json).
// So the two live here and are iterated rather than typed twice.
//
// ---- what is NOT here, and why ------------------------------------------------
//
// `cloud_cover_low` / `_mid` / `_high` and `snow_depth`. Open-Meteo serves all
// four and the plan called for them; nothing in app/qml/Clima/ reads them —
// DetailCloudCoverCard.qml uses the single total and nothing shows lying snow —
// so asking for them would be four columns fetched, parsed, cached and thrown
// away on every refresh, forever, against a free service's rate limit. The
// recorded fixtures *do* contain them, which is deliberate: the day a card
// wants them, the golden files already have the answer and only this list has
// to change.
//
// `minutely_15` is not here either. It is a separate request with a separate
// cache row (DataKind::Nowcast, five minutes) and it is region-limited —
// docs/02-data-sources.md §2.2, "Central Europe + North America only" — so it
// belongs to the nowcast ribbon rather than to the forecast.

#pragma once

#include <QLatin1String>
#include <QList>
#include <QString>

namespace clima {
namespace openmeteo {

// The `hourly=` list. Order is the order they go in the URL, which is the
// order they come back in, which makes a recorded response readable.
QList<QLatin1String> hourlyVariables();

// The `daily=` list.
QList<QLatin1String> dailyVariables();

// The `current=` list. A subset of the hourly one — Open-Meteo does not offer
// every variable as a current value, and asking for one it does not have
// fails the whole request rather than omitting that field.
QList<QLatin1String> currentVariables();

// The three lists as the comma-separated strings a query string wants.
QString hourlyParameter();
QString dailyParameter();
QString currentParameter();

} // namespace openmeteo
} // namespace clima
