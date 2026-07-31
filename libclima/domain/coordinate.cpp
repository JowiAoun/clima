// SPDX-FileCopyrightText: 2026 Jowi Aoun
// SPDX-License-Identifier: MPL-2.0

#include "coordinate.h"

#include <QLatin1Char>

#include <cmath>

namespace clima {

namespace {

double roundTo(double value, int decimals)
{
    const double scale = std::pow(10.0, decimals);

    // std::round is half-away-from-zero, which is what a human means by
    // rounding and — more to the point — is symmetric about the equator and
    // the prime meridian. std::nearbyint would follow the current rounding
    // mode, which is a global somebody else can change.
    return std::round(value * scale) / scale;
}

QString fixed(double value, int decimals)
{
    // 'f' with an explicit precision, and the QString overload that takes no
    // QLocale. QLocale::toString would give a French machine "52,5200".
    return QString::number(value, 'f', decimals);
}

} // namespace

bool Coordinate::isValid() const
{
    return std::isfinite(latitude) && std::isfinite(longitude)
        && latitude >= -90.0 && latitude <= 90.0
        && longitude >= -180.0 && longitude <= 180.0;
}

Coordinate Coordinate::rounded(int decimals) const
{
    return Coordinate{ roundTo(latitude, decimals), roundTo(longitude, decimals) };
}

QString Coordinate::toKeyString(int decimals) const
{
    return latitudeString(decimals) + QLatin1Char(',') + longitudeString(decimals);
}

QString Coordinate::latitudeString(int decimals) const
{
    return fixed(roundTo(latitude, decimals), decimals);
}

QString Coordinate::longitudeString(int decimals) const
{
    return fixed(roundTo(longitude, decimals), decimals);
}

bool Coordinate::operator==(const Coordinate &other) const
{
    // Exact, on purpose. This is not "are these the same place" — that question
    // is answered by comparing rounded() values, and every place in the
    // codebase that cares is asking it of an already-rounded pair. An epsilon
    // here would make equality non-transitive and quietly break QHash.
    return latitude == other.latitude && longitude == other.longitude;
}

} // namespace clima
