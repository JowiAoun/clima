// SPDX-FileCopyrightText: 2026 Jowi Aoun
// SPDX-License-Identifier: MPL-2.0

#include "libclima/cache/payloadcache.h"

#include "libclima/cache/cachestore.h"

namespace clima {
namespace payloadcache {

Hit lookUp(const CacheStore *cache, const QString &key)
{
    Hit hit;
    if (cache == nullptr || !cache->isOpen())
        return hit;

    const Result<CacheEntry> entry = cache->get(key);
    if (!entry)
        return hit;

    hit.payload   = entry.value().payload;
    hit.fetchedAt = entry.value().fetchedAt;
    hit.present   = !hit.payload.isEmpty();
    hit.fresh     = hit.present && cache->isFresh(entry.value());
    return hit;
}

void store(CacheStore *cache, const QString &key, const QString &providerId,
           const QString &endpoint, DataKind kind, Coordinate coordinate,
           const HttpResponse &response)
{
    if (cache == nullptr || !cache->isOpen() || response.body.isEmpty())
        return;

    CacheEntry entry;
    entry.key         = key;
    entry.providerId  = providerId;
    entry.endpoint    = endpoint;
    entry.kind        = kind;
    entry.coordinate  = coordinate;
    entry.payload     = response.body;
    entry.contentType = response.contentType;
    entry.fetchedAt   = response.fetchedAt;

    // The response's own expiry when it has one — HttpClient has already taken
    // the later of the server's Expires and our table's TTL — and our table's
    // answer otherwise, so that a provider sending no cache headers at all
    // still lands in the right row of §4.5.
    entry.expiresAt = response.expiresAt.isValid() ? response.expiresAt
                                                   : expiryFor(kind, response.fetchedAt);
    entry.validators = response.validators;

    cache->put(entry);
}

void touch(CacheStore *cache, const QString &key, DataKind kind, const HttpResponse &response)
{
    if (cache == nullptr || !cache->isOpen())
        return;

    const QDateTime expires = response.expiresAt.isValid()
        ? response.expiresAt
        : expiryFor(kind, response.fetchedAt);

    cache->touch(key, response.fetchedAt, expires);
}

} // namespace payloadcache
} // namespace clima
