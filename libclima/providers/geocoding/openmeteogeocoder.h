// SPDX-FileCopyrightText: 2026 Jowi Aoun
// SPDX-License-Identifier: MPL-2.0
//
// Forward geocoding against Open-Meteo's hosted GeoNames index.
//
//   https://geocoding-api.open-meteo.com/v1/search?name=&count=&language=&format=json
//   https://geocoding-api.open-meteo.com/v1/get?id=<geonameid>
//
// No key, no registration, no quota to sign up for. Verified against the live
// service: `name=Kigali` returns geonameid 202061 with country, admin1 and
// timezone attached, and `id=6167865` returns Toronto.
//
// ---- why the hosted index and not the bundled one ---------------------------
//
// libclima already carries a packed cities15000 for reverse geocoding, and it
// would be a small matter to search it by name. It would also be a much worse
// search. Open-Meteo's copy holds every GeoNames entry rather than the 31 673
// with a population over fifteen thousand, indexes the *alternate names*
// column — a hundred spellings of Toronto in forty scripts, which is two
// thirds of the upstream file and the first thing the packer throws away — and
// resolves postcodes. Typing "Кигали" or "M5V" and getting the right answer is
// the feature; a substring match over 31 673 ASCII names is not.
//
// So the split is: search is online and generous, reverse is offline and
// exact. Both are GeoNames, so both produce the same `Place::geonamesId`, which
// is what stops the app holding a searched Toronto and a detected Toronto as
// two rows.
//
// ---- the provider id is its own, and that is deliberate ---------------------
//
// "open-meteo-geocoding", not "open-meteo". HttpClient disables a provider for
// the life of the process on a 403 and never sends again
// (libclima/net/httpclient.h, promise 2), and the unit it disables is the
// provider id. Sharing an id with the forecast provider would mean a refusal
// from geocoding-api.open-meteo.com taking down forecasts from
// api.open-meteo.com — a different host, with its own policy, answering a
// different question. A user who cannot search for a city should still get the
// weather for the city they already saved.
//
// ---- caching: seven days, and stale beats nothing ---------------------------
//
// §4.5's row for geocoding is "7 days, keyed by query+lang", and the key is
// built by RequestKey from the provider, the endpoint and the parameters —
// which include the query and the language, so the keying is structural rather
// than remembered.
//
// A fresh hit is answered without a request. A *stale* hit is held in reserve:
// if the network then fails, the stale answer is returned rather than the
// error. That is design principle 1 (§4.1) applied to a search box — a user on
// a train who searched "Toronto" last week should get Toronto, not a spinner
// that ends in a network message.
//
// ---- what this class does not do --------------------------------------------
//
// It does not debounce. Search-as-you-type sends a request per keystroke and
// the fix for that is a timer, but a timer belongs to the thing that knows
// when the user stopped typing — libclima/places/placesearchmodel.h, which
// holds the 250 ms one. A debounce inside the provider would also debounce the
// programmatic callers, who are not typing.

#pragma once

#include "libclima/net/httprequest.h"
#include "libclima/providers/geocoding/igeocoder.h"

#include <QObject>
#include <QString>
#include <QUrl>

namespace clima {

class CacheStore;
class Clock;
class HttpClient;

class OpenMeteoGeocoder final : public QObject, public IForwardGeocoder
{
    Q_OBJECT

public:
    // `http` and `clock` must outlive this. `cache` may be null, which turns
    // off both the seven-day cache and the stale fallback and is what a test
    // about parsing rather than about caching passes.
    OpenMeteoGeocoder(HttpClient *http, CacheStore *cache, Clock *clock,
                      QObject *parent = nullptr);
    ~OpenMeteoGeocoder() override;

    // Below this, `search` resolves to an empty list without a request.
    //
    // Two, because that is what the service supports: Open-Meteo documents
    // exact matching from two characters and fuzzy matching from three
    // (docs/02-data-sources.md §2.7). One character would match a large
    // fraction of the planet and is a request nobody wanted to make.
    static constexpr int minimumQueryLength = 2;

    [[nodiscard]] QString     id() const override;
    [[nodiscard]] QStringList attribution() const override;

    QFuture<Result<QList<Place>>> search(const GeocodeQuery &query) override;
    QFuture<Result<Place>>        resolve(qint64 geonamesId) override;

    // Points the provider at a different origin. For the loopback stub in
    // tests — there is no production reason to change it, and a setter is
    // cheaper than threading a URL through three constructors.
    void                setBaseUrl(const QUrl &url);
    [[nodiscard]] QUrl  baseUrl() const { return m_baseUrl; }

private:
    struct Cached {
        QByteArray payload;
        bool       fresh = false;
    };

    [[nodiscard]] Cached  lookUp(const QString &key) const;
    void                  store(const QString &key, const QString &endpoint,
                                const HttpResponse &response);

    HttpClient *m_http = nullptr;
    CacheStore *m_cache = nullptr;
    Clock      *m_clock = nullptr;
    QUrl        m_baseUrl;
};

} // namespace clima
