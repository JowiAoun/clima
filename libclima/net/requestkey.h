// SPDX-FileCopyrightText: 2026 Jowi Aoun
// SPDX-License-Identifier: MPL-2.0
//
// One string that names one request, everywhere.
//
// The same key identifies an entry in the cache, an in-flight request in the
// coalescer, and a stored ETag in the validator store. That is deliberate and
// it is the reason this is a type rather than three string concatenations: a
// second spelling of "the same request" means a conditional GET sent against
// the wrong entity, or a cache write that never gets read, and neither of
// those fails loudly.
//
// ---- the four components ----------------------------------------------------
//
//   providerId   a 403 disables a provider, and a provider's answers must not
//                collide with another's for the same coordinates.
//   endpoint     "forecast" and "air-quality" at the same place are two rows.
//   coordinate   ROUNDED FIRST. This is the whole point — see below.
//   parameters   everything else, hashed, order-insensitive.
//
// ---- rounded first, and what happens if it is not ---------------------------
//
// A map drag emits centre coordinates at full double precision. Hash those and
// every frame is a distinct key: a distinct in-flight slot, so the coalescer
// never coalesces; a distinct cache row, so nothing ever hits; a distinct
// request, so a hundred forecasts get fetched for a hundred points inside one
// 11 km grid cell. Nothing errors. The tab is just slow and the provider's logs
// fill up with us.
//
// Rounding to four decimals before hashing collapses that entire drag into one
// key. It is one line, and it is the single most load-bearing line in this
// file.
//
// ---- why the parameters are hashed and the rest is not ----------------------
//
// A key is read by a human roughly as often as it is read by a machine: it goes
// in log lines, and it is the primary key you would sort a cache file by when
// something is wrong. So the parts that a human recognises stay legible and
// only the long tail — thirty comma-separated Open-Meteo variable names — is
// collapsed to a digest:
//
//     open-meteo/forecast@52.5200,13.4050#9f2c1ab4e70d3c58
//
// Sixteen hex characters of SHA-256. Collision resistance is not the property
// being bought here — the namespace is one user's cache — legibility is, and
// sixty-four characters of hex would swamp the readable part.

#pragma once

#include "libclima/net/httprequest.h"

#include <QString>

namespace clima {

class RequestKey
{
public:
    RequestKey() = default;

    // Rounds the coordinate to Coordinate::keyDecimals before it hashes
    // anything. Callers do not get to skip that step; there is no overload
    // that takes a pre-built string.
    static RequestKey forRequest(const HttpRequest &request);

    [[nodiscard]] QString toString() const { return m_key; }
    [[nodiscard]] bool    isEmpty() const { return m_key.isEmpty(); }

    bool operator==(const RequestKey &other) const { return m_key == other.m_key; }
    bool operator!=(const RequestKey &other) const { return m_key != other.m_key; }

private:
    QString m_key;
};

size_t qHash(const RequestKey &key, size_t seed = 0) noexcept;

} // namespace clima
