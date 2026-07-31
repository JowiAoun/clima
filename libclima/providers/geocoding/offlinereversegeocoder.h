// SPDX-FileCopyrightText: 2026 Jowi Aoun
// SPDX-License-Identifier: MPL-2.0
//
// "Where am I", answered from a table compiled into the binary.
//
// GeonamesIndex does the searching; this turns what it found into a Place —
// the same value type the forward geocoder produces, so that a place detected
// and a place searched for are interchangeable everywhere downstream.
//
// ---- what a point in the ocean returns --------------------------------------
//
// ErrorKind::Unsupported, with a message naming the coordinate.
//
// The alternative is to return the nearest city however far away it is, and
// that is worse than useless. Point Nemo, the oceanic pole of inaccessibility
// at 48.8767 S 123.3933 W, is 2 690 km from the nearest land; the nearest row
// in cities15000 is most of a continent away. "You are in Rikitea" is not a
// less precise answer than the coordinate, it is a false one, and a weather app
// that says it will happily fetch a forecast for somewhere else and label it
// with a place the user has never been.
//
// So there is a cutoff, 250 km by default. Inside it, a name. Outside it, an
// absence the UI is expected to render as the coordinate itself —
// docs/04-architecture.md §4.4: "A provider that returns nothing must make the
// UI *hide* the feature, not show a broken one."
//
// 250 km rather than something tighter because the failure it guards against
// is a wrong name and not a distant one. A ship fifty kilometres off Toronto in
// Lake Ontario should be told Toronto; a point in the central Sahara, where
// cities15000 genuinely has nothing within 250 km, should be told nothing.
// Both of those are right, and neither is a compromise.
//
// ---- the country name comes from Qt, not from the dataset -------------------
//
// The packed index carries the ISO 3166-1 alpha-2 code and not the country's
// name, because QLocale already knows the mapping and a second copy of the
// world's country names is 4 KB of data to keep in step with nothing. The names
// Qt produces — "Canada", "Iceland", "Singapore", "Rwanda" — are the same
// strings Open-Meteo's geocoder returns, which is the property that matters:
// the two paths have to produce the same Place.

#pragma once

#include "libclima/providers/geocoding/geonamesindex.h"
#include "libclima/providers/geocoding/igeocoder.h"

#include <QString>
#include <QStringList>

namespace clima {

class OfflineReverseGeocoder final : public IReverseGeocoder
{
public:
    OfflineReverseGeocoder();
    ~OfflineReverseGeocoder() override;

    // No populated place further than this is ever named. See the header
    // comment for why the number is what it is.
    static constexpr double defaultMaximumDistanceKm = 250.0;

    // Reads the bundled index. Idempotent, and deliberately NOT called from
    // the constructor: decoding is about a megabyte of allocation and the app
    // must not pay for it on a cold start that never asks where it is. The
    // cold-start budget is 400 ms and the home place comes from SQLite.
    Status load();

    // For tests, and for a future user-supplied index.
    Status load(const QByteArray &packed);

    [[nodiscard]] bool isLoaded() const { return m_index.isLoaded(); }
    [[nodiscard]] int  cityCount() const { return m_index.cityCount(); }

    void                  setMaximumDistanceKm(double kilometres);
    [[nodiscard]] double  maximumDistanceKm() const { return m_maximumDistanceKm; }

    [[nodiscard]] QString     id() const override;
    [[nodiscard]] QStringList attribution() const override;

    [[nodiscard]] Result<ReverseMatch> reverse(const Coordinate &at) const override;

    // The country name for an ISO 3166-1 alpha-2 code, or the code itself when
    // Qt does not recognise it. Exposed because the forward geocoder's parser
    // needs the same mapping for a response that omitted the country name, and
    // two spellings of "Czechia" in one places table is exactly the drift this
    // whole module exists to prevent.
    [[nodiscard]] static QString countryName(const QString &countryCode);

private:
    GeonamesIndex m_index;
    double        m_maximumDistanceKm = defaultMaximumDistanceKm;
};

} // namespace clima
