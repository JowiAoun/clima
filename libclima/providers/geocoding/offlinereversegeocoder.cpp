// SPDX-FileCopyrightText: 2026 Jowi Aoun
// SPDX-License-Identifier: MPL-2.0

#include "offlinereversegeocoder.h"

#include <QLocale>

namespace clima {

OfflineReverseGeocoder::OfflineReverseGeocoder() = default;
OfflineReverseGeocoder::~OfflineReverseGeocoder() = default;

Status OfflineReverseGeocoder::load()
{
    return m_index.loadBundled();
}

Status OfflineReverseGeocoder::load(const QByteArray &packed)
{
    return m_index.load(packed);
}

void OfflineReverseGeocoder::setMaximumDistanceKm(double kilometres)
{
    if (kilometres > 0.0)
        m_maximumDistanceKm = kilometres;
}

QString OfflineReverseGeocoder::id() const
{
    return QStringLiteral("geonames-offline");
}

QStringList OfflineReverseGeocoder::attribution() const
{
    // CC-BY 4.0 requires the credit and the licence, and the licence has to be
    // reachable rather than merely named. docs/02-data-sources.md §2.9 lists
    // GeoNames as CC-BY 4.0; REUSE.toml declares the packed file with GeoNames
    // as its copyright holder, which is the same obligation kept in the place
    // a machine reads.
    return {
        QStringLiteral("Place names from GeoNames (https://www.geonames.org), "
                       "CC BY 4.0 (https://creativecommons.org/licenses/by/4.0/)"),
    };
}

QString OfflineReverseGeocoder::countryName(const QString &countryCode)
{
    if (countryCode.isEmpty())
        return {};

    const QLocale::Territory territory = QLocale::codeToTerritory(countryCode);
    if (territory == QLocale::AnyTerritory)
        return countryCode;

    return QLocale::territoryToString(territory);
}

Result<ReverseMatch> OfflineReverseGeocoder::reverse(const Coordinate &at) const
{
    if (!m_index.isLoaded()) {
        return Error(ErrorKind::Storage,
                     QStringLiteral("the offline place index has not been loaded; "
                                    "call OfflineReverseGeocoder::load() first"));
    }

    if (!at.isValid()) {
        return Error(ErrorKind::Unsupported,
                     QStringLiteral("%1 is not a point on the earth").arg(at.toKeyString()));
    }

    const std::optional<GeonamesIndex::Match> found = m_index.nearest(at, m_maximumDistanceKm);
    if (!found) {
        // Unsupported and not NotFound. §4.4's word for "the provider does not
        // cover this coordinate" is Unsupported, and the difference matters to
        // the caller: NotFound invites a retry somewhere else, and there is
        // nowhere else — every provider would say the same thing about the
        // middle of the Pacific.
        return Error(ErrorKind::Unsupported,
                     QStringLiteral("no populated place within %1 km of %2")
                         .arg(m_maximumDistanceKm, 0, 'f', 0)
                         .arg(at.toKeyString()));
    }

    const GeonamesCity city = m_index.cityAt(found->row);

    ReverseMatch match;
    match.distanceKm = found->distanceKm;
    match.insideFootprint = found->insideFootprint;

    match.place.geonamesId = qint64(city.geonamesId);
    match.place.name = city.name;
    match.place.admin1 = city.admin1;
    match.place.countryCode = city.countryCode;
    match.place.country = countryName(city.countryCode);
    match.place.timezone = city.timezone;
    match.place.coordinate = city.coordinate;

    return match;
}

} // namespace clima
