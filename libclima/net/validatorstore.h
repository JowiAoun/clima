// SPDX-FileCopyrightText: 2026 Jowi Aoun
// SPDX-License-Identifier: MPL-2.0
//
// Where HttpClient remembers what the server last told it, so it can ask
// "still the same?" instead of "give me it again".
//
// ---- why this is an interface and not just CacheStore -----------------------
//
// HttpClient needs two strings per request — the ETag and the Last-Modified —
// and CacheStore happens to have them, because it wrote them down next to the
// payload. Handing HttpClient a CacheStore* to get at them would make the
// network layer depend on SQLite, on QStandardPaths, on a schema and on a
// migration runner, for two strings. That is backwards: net/ is the lower
// layer, and it is the one that has to be testable without a database on disk.
//
// So the dependency points the other way through a four-method interface.
// CacheStore implements it — it is already writing the columns — and a test
// implements it in twenty lines with a QHash. The layering matches
// docs/04-architecture.md §4.2, where Net and Cache are siblings inside the
// engine rather than one stacked on the other.
//
// ---- conditional GET is an obligation, not an optimisation ------------------
//
// MET Norway's terms of service require conditional requests: a client that
// refetches an unchanged forecast is using their bandwidth for nothing, and
// they say so. docs/02-data-sources.md §2.9 records it. The saving is real —
// a 304 is a few hundred bytes against a hundred-kilobyte forecast — but the
// reason it is implemented on day one rather than as a later optimisation is
// that it is part of the deal we made to use the service without a key.

#pragma once

#include <QByteArray>
#include <QDateTime>
#include <QString>

#include <optional>

namespace clima {

struct Validators {
    // Exactly as the server sent them, quotes and weak-comparison prefix and
    // all. An ETag is an opaque token: `W/"abc"` and `"abc"` are different
    // tokens and a client that strips the prefix to be tidy gets a 200 where
    // it expected a 304, silently, forever.
    QByteArray entityTag;
    QByteArray lastModified;

    // The server's own Expires, when it sent one and it parsed. Kept because
    // §4.5's TTLs are *our* policy and a provider is entitled to a longer
    // opinion about its own data — see HttpClient, which takes the later of
    // the two.
    QDateTime expires;

    [[nodiscard]] bool isEmpty() const { return entityTag.isEmpty() && lastModified.isEmpty(); }
};

class ValidatorStore
{
public:
    virtual ~ValidatorStore();

    // Keyed by RequestKey::toString(). Nothing else may be used as the key:
    // the whole point of that type is that one string identifies one request
    // across the cache, the coalescer and this store, and a second spelling
    // would mean a conditional GET sent against the wrong entity.
    [[nodiscard]] virtual std::optional<Validators> validatorsFor(const QString &key) const = 0;

    virtual void storeValidators(const QString &key, const Validators &validators) = 0;
};

} // namespace clima
