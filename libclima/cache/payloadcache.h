// SPDX-FileCopyrightText: 2026 Jowi Aoun
// SPDX-License-Identifier: MPL-2.0
//
// The three lines every provider needs in order for docs/04-architecture.md
// §4.1's first design principle to be true, written once.
//
// ============================================================================
// "THE UI RENDERS FROM CACHE, THEN RECONCILES WITH THE NETWORK"
//
// That is principle 1, and it is a promise about where the bytes come from, not
// about what the view model does with them. A view model cannot keep it: by the
// time a Forecast reaches app/viewmodels/ the bytes are gone, and an app that
// wanted to render before the network answered would have to have its own
// second cache holding its own second copy of the same data in some other
// shape. Two caches is how the two disagree.
//
// So the cache read belongs beside the fetch, in the provider, and this is the
// shared body of it. OpenMeteoGeocoder wrote it first — see its `lookUp` and
// `store` — and the three providers that came after would each have written it
// again, slightly differently, which is the interesting failure: they would
// have disagreed about whether a stale entry is a hit.
//
// ============================================================================
// STALE IS NOT A MISS, AND THE FRESHNESS FLAG IS THE WHOLE POINT
//
// §4.5's table has a stale-while-revalidate column, and every forecast row in
// it is ticked. Which means three outcomes, not two:
//
//     fresh     serve it, send nothing. The TTL has not expired.
//     stale     serve it *now*, and send a conditional request anyway.
//     absent    there is nothing to draw; the request is all there is.
//
// A `Hit` says which. What it deliberately does NOT carry is a "this is stale"
// flag for the UI, because there already is one and it is better: the entry's
// own `fetchedAt`. A Forecast carrying the timestamp it was actually fetched at
// renders as "updated 40 minutes ago" without anything downstream being told
// that a fallback happened — and the day the network comes back the same line
// reads "updated just now" for the same reason. One number, no second state to
// keep in step.
//
// ============================================================================
// A CACHE FAILURE IS NEVER A REQUEST FAILURE
//
// Every function here swallows its errors. A cache miss, an unwritable
// database, a corrupt row: all of them mean "no help from here", and none of
// them mean the forecast could not be fetched. The one thing this file must
// never do is turn a disk problem into a weather problem.

#pragma once

#include "libclima/cache/cachepolicy.h"
#include "libclima/domain/coordinate.h"
#include "libclima/net/httprequest.h"

#include <QByteArray>
#include <QDateTime>
#include <QString>

namespace clima {

class CacheStore;

namespace payloadcache {

// What was found, if anything.
struct Hit {
    QByteArray payload;
    QDateTime  fetchedAt;

    // There is a row and it has bytes.
    bool present = false;

    // …and its TTL has not run out. `present && !fresh` is the stale-while-
    // revalidate case, which is the one worth having a word for.
    bool fresh = false;
};

// Never fails. A null store, a closed database and a missing row are all the
// same answer.
[[nodiscard]] Hit lookUp(const CacheStore *cache, const QString &key);

// Writes the response body under `key`. Silent about failure — see the header.
// An empty body is not written: a 304 carries none, and overwriting a good row
// with nothing is the one way this could make things worse.
void store(CacheStore *cache, const QString &key, const QString &providerId,
           const QString &endpoint, DataKind kind, Coordinate coordinate,
           const HttpResponse &response);

// After a 304. Rewrites the freshness stamps without touching the bytes.
void touch(CacheStore *cache, const QString &key, DataKind kind, const HttpResponse &response);

} // namespace payloadcache
} // namespace clima
