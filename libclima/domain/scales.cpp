// SPDX-FileCopyrightText: 2026 Jowi Aoun
// SPDX-License-Identifier: MPL-2.0

#include "libclima/domain/scales.h"

#include <QCoreApplication>
#include <QtGlobal>

#include <cmath>

namespace clima::scales {
namespace {

// One translation context for the file, so a translator sees "Low", "Moderate"
// and "High" as a set rather than scattered across whichever class happened to
// hold them. Previously that context was ConditionsData, which is a view model
// — a name that told a translator nothing about which "Moderate" this was.
QString tr(const char *source)
{
    return QCoreApplication::translate("clima::scales", source);
}

} // namespace

QString uvBand(double index)
{
    if (qIsNaN(index)) return {};
    if (index < 3)     return tr("Low");
    if (index < 6)     return tr("Moderate");
    if (index < 8)     return tr("High");
    if (index < 11)    return tr("Very high");
    return tr("Extreme");
}

QString aqiBand(double index)
{
    if (qIsNaN(index))  return {};
    if (index <= 20)    return tr("Good");
    if (index <= 40)    return tr("Fair");
    if (index <= 60)    return tr("Moderate");
    if (index <= 80)    return tr("Poor");
    if (index <= 100)   return tr("Very poor");
    return tr("Extremely poor");
}

QString visibilityBand(double km)
{
    if (qIsNaN(km)) return {};
    if (km >= 16)   return tr("Excellent");
    if (km >= 10)   return tr("Good");
    if (km >= 4)    return tr("Moderate");
    if (km >= 1)    return tr("Poor");
    return tr("Very poor");
}

int beaufortForce(double kmh)
{
    if (qIsNaN(kmh))
        return 0;
    const double ms = kmh / 3.6;
    return qBound(0, int(std::floor(std::pow(ms / 0.836, 2.0 / 3.0) + 0.5)), 12);
}

QString beaufortName(int force)
{
    switch (force) {
    case 0:  return tr("Calm");
    case 1:  return tr("Light air");
    case 2:  return tr("Light breeze");
    case 3:  return tr("Gentle breeze");
    case 4:  return tr("Moderate breeze");
    case 5:  return tr("Fresh breeze");
    case 6:  return tr("Strong breeze");
    case 7:  return tr("Near gale");
    case 8:  return tr("Gale");
    case 9:  return tr("Severe gale");
    case 10: return tr("Storm");
    case 11: return tr("Violent storm");
    default: return tr("Hurricane force");
    }
}

QString compassPoint(double degrees)
{
    static const char *const points[] = { "N",  "NNE", "NE", "ENE", "E",  "ESE", "SE", "SSE",
                                          "S",  "SSW", "SW", "WSW", "W",  "WNW", "NW", "NNW" };
    if (qIsNaN(degrees))
        return {};
    const int index = int(std::lround(degrees / 22.5)) & 15;
    return QString::fromLatin1(points[index]);
}

} // namespace clima::scales
