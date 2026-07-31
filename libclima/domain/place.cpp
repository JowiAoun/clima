// SPDX-FileCopyrightText: 2026 Jowi Aoun
// SPDX-License-Identifier: MPL-2.0

#include "place.h"

namespace clima {

QString Place::region() const
{
    if (admin1.isEmpty())
        return country;
    if (country.isEmpty())
        return admin1;
    return admin1 + QStringLiteral(", ") + country;
}

QString Place::label() const
{
    // Name plus first-level division, which is what disambiguates the twelve
    // Torontos and the four hundred San Josés. Not the country as well: the
    // location bar is one line at section-title size, and "Toronto, Ontario,
    // Canada" is the string that makes it wrap.
    if (admin1.isEmpty() || admin1 == name)
        return name;
    return name + QStringLiteral(", ") + admin1;
}

bool Place::isSameEntity(const Place &other) const
{
    if (geonamesId != 0 && other.geonamesId != 0)
        return geonamesId == other.geonamesId;

    // One of them is a dropped pin. Two pins inside eleven metres of each other
    // produce the same cache key, the same URL and the same forecast, so they
    // are the same place by every operational definition this engine has.
    return coordinate.rounded() == other.coordinate.rounded();
}

} // namespace clima
