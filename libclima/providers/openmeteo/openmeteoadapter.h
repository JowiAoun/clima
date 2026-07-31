// SPDX-FileCopyrightText: 2026 Jowi Aoun
// SPDX-License-Identifier: MPL-2.0
//
// Open-Meteo's JSON, field by field, into libclima/domain/forecast.h.
//
// A free function over a QByteArray and nothing else: no clock, no network, no
// member state. That is what lets tests/tst_openmeteoadapter.cpp run the whole
// mapping against eight recorded responses without an event loop, and it is
// what lets docs/04-architecture.md §4.8 put this on the parse pool — a pure
// function has nothing to synchronise.
//
// ============================================================================
// THE FOUR THINGS THIS FILE EXISTS TO GET RIGHT
//
// Each of them ships silently. None of them makes anything crash, go empty, or
// fail a test that is not looking for it.
//
// ---- 1. UNITS ----------------------------------------------------------------
//
// `visibility` arrives in METRES and the domain stores kilometres. Unconverted,
// a clear day reads 24 100 against an axis whose maximum is 25, so the
// Visibility tab is a flat line pinned to the top of the chart — which looks
// like a working chart of a variable that never changes.
//
// `snowfall` arrives in CENTIMETRES while `precipitation`, `rain` and `showers`
// arrive in millimetres, all four in the same object. The domain keeps
// centimetres (it is the unit snow is spoken about in) and the field is called
// `snowfall` beside a `precipitation` in millimetres, so this is written down
// here as well as in forecast.h. Ten to one is not a factor anybody notices in
// a number between 0 and 5.
//
// Everything else is already canonical: °C, km/h, hPa, mm, %, degrees from
// north, seconds. Deliberately NOT requested in other units even though
// Open-Meteo offers `temperature_unit` and `wind_speed_unit` — they are not
// per-quantity, there is no pressure parameter at all, and a unit-tagged
// response makes the cache unit-keyed, so a user toggling °C to °F would
// refetch every forecast they have ever looked at. Conversion happens once,
// downstream, in app/qml/Clima/metrics.js's `format()`.
//
// ---- 2. TIME IS NOT WHAT THE TIMESTAMPS SAY ----------------------------------
//
// `timezone=auto` returns naive local strings built by adding ONE fixed
// `utc_offset_seconds` to a UTC series — there is no second offset and no
// transition, so Open-Meteo's local day is always 24 rows and its sunrise is an
// hour wrong for half the year. libclima/domain/timeaxis.h has the measurement
// and the reasoning; this file simply never trusts a label and always
// reconstructs the instant.
//
// ---- 3. THE PRECIPITATION HOUR -----------------------------------------------
//
// Open-Meteo's accumulations are the PRECEDING hour. So is
// libclima/domain/forecast.h's convention, which means this adapter does *not*
// shift — the shift belongs at the boundary where domain data becomes chart
// data, because MET Norway's adapter shifts the other way into the same
// convention and doing it twice here would undo that.
//
// `libclima/domain/hourconvention.h`'s `asHourStarting()` is that boundary, it
// carries the measurement proving which direction is correct, and
// tests/tst_openmeteoadapter.cpp asserts the result against a recorded response
// with an isolated one-hour spike in it. If you are reading this file looking
// for the shift, it is there and not here.
//
// ---- 4. THE WEATHER CODE IS NOT A NUMBER THE UI UNDERSTANDS ------------------
//
// It is a WMO code, and app/qml/Clima/precip.js's `cellFor(mm, tempC, code)`
// wants one of its own six type names. Passing 95 straight through misses
// `STYLE[c.type]`, falls back to rain, and draws a thunderstorm as drizzle-
// adjacent rain with nothing anywhere saying so. The translation is
// libclima/domain/weathercode.h and it is deliberately not in this file:
// MET Norway's adapter produces WMO codes too and needs the same table.
//
// ============================================================================
// WHAT ABSENT MEANS HERE
//
// JSON `null` becomes an absent Reading, and it is ordinary rather than
// exceptional. The live Toronto response recorded in the fixtures has a null
// hour in the middle of its series and a null tail on the sixteenth day;
// `toronto-ecmwf-gaps.json` has `uv_index` and `visibility` null for all 72
// hours because ECMWF IFS does not carry them, which is the difference between
// "no value this hour" and "no such variable here" — and the second is what
// decides whether a metric tab is drawn at all.
//
// A variable missing from the response *entirely* is the same thing said more
// briefly, and produces the same absent Readings rather than a shorter series.
// Columns of unequal length are the one shape that is rejected: they mean the
// payload is not what the contract promised, and a series silently truncated
// to its shortest column is precisely the partial success §4.4 forbids.

#pragma once

#include "libclima/core/result.h"
#include "libclima/domain/forecast.h"
#include "libclima/providers/iforecastprovider.h"

#include <QByteArray>

namespace clima {
namespace openmeteo {

// Parse one `/v1/forecast` response body.
//
// `providerId` is stamped onto the Forecast and onto any Error, so that a
// diagnostic can name the source without the caller remembering what it asked.
//
// Returns Error(ErrorKind::Parse) for malformed JSON, for Open-Meteo's own
// `{"error":true,"reason":…}` envelope, for a response with no hourly series,
// and for columns whose lengths disagree. Never a half-filled Forecast.
Result<Forecast> adaptForecast(const QByteArray &body, const QString &providerId);

// The credit this data must be shown with. docs/02-data-sources.md §2.9 makes
// it an obligation with a deadline rather than a nicety, and
// libclima/providers/iforecastprovider.h makes an incomplete one a registration
// failure.
Attribution attribution();

// What a parsed payload turns out to carry — read off the columns rather than
// declared, because "which tabs exist here" is a question only the response can
// answer. tests/fixtures/openmeteo/toronto-ecmwf-gaps.json is 72 hours of null
// UV and null visibility beside a complete temperature series, which is "no
// such variable here" and not "no value this hour".
//
// Nothing it returns is ever undetermined: a payload has been seen, so the
// witness has testified. Lives here rather than in the live provider because
// the fixture provider learns the same fact from the same bytes, and two copies
// of this table would be two answers to "is there a UV tab".
Capabilities capabilitiesFor(const Forecast &forecast);

} // namespace openmeteo
} // namespace clima
