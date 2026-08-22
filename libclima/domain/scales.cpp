// SPDX-FileCopyrightText: 2026 Jowi Aoun
// SPDX-License-Identifier: MPL-2.0

#include "libclima/domain/scales.h"

#include <QCoreApplication>
#include <QHash>
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
    // NaN is "no reading", and 0 is the only int that can carry that. The
    // caller has to test the reading rather than the force — see the header;
    // this is the one function here that cannot answer with an empty string.
    if (qIsNaN(kmh))
        return 0;

    // Below the scale. There is no negative half, and pow() of a negative base
    // to a fractional exponent is NaN, which the cast below cannot survive.
    if (kmh <= 0)
        return 0;

    // Above what an int can hold. `int(...)` on an infinity or on a value past
    // INT_MAX is undefined behaviour, and the way it actually failed was worse
    // than a crash: on x86-64 the cast yields INT_MIN, qBound clamps that to 0,
    // and an infinite wind speed was reported as "Calm". Saturating at the top
    // of the scale is the only end an unbounded number can honestly be at.
    if (!qIsFinite(kmh))
        return 12;

    const double force = std::floor(std::pow((kmh / 3.6) / 0.836, 2.0 / 3.0) + 0.5);
    if (force >= 12.0)
        return 12;
    return int(force);
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

QString pollutantLabel(const QString &id)
{
    // Subscripts as real characters rather than as rich text, because these go
    // into a QML Text with no styled-text parsing and into a widget tile that
    // has room for four glyphs. Not translated: a chemical formula is the same
    // in every language this app will ever ship in.
    //
    // Both spellings of each species, and that is not redundancy.
    //
    // The left column is what `clima::pollutantId()` emits — "ozone",
    // "nitrogen_dioxide" — because that is what the two callers actually pass:
    // app/viewmodels/conditionsdata.cpp calls pollutantLabel(pollutantId(...))
    // and widgets/wx.cpp calls it on the id that arrived over the wire. This
    // table opened with only the chemical short forms, which pollutantId() has
    // never produced, so four of the six species fell through to the fallback
    // and the air-quality card printed OZONE, NITROGEN_DIOXIDE,
    // SULPHUR_DIOXIDE and CARBON_MONOXIDE — the same defect this function was
    // written to fix, surviving in the four rows nobody spot-checked.
    //
    // The short forms stay because a snapshot written by another version of
    // Clima may carry them, and a widget reading one should get a chemist's
    // name rather than a shout. Two rows are cheaper than a wire migration.
    static const QHash<QString, QString> names{
        { QStringLiteral("pm2_5"), QStringLiteral("PM2.5") },
        { QStringLiteral("pm10"), QStringLiteral("PM10") },

        { QStringLiteral("ozone"), QStringLiteral("O₃") },
        { QStringLiteral("o3"), QStringLiteral("O₃") },

        { QStringLiteral("nitrogen_dioxide"), QStringLiteral("NO₂") },
        { QStringLiteral("no2"), QStringLiteral("NO₂") },

        { QStringLiteral("sulphur_dioxide"), QStringLiteral("SO₂") },
        { QStringLiteral("so2"), QStringLiteral("SO₂") },

        { QStringLiteral("carbon_monoxide"), QStringLiteral("CO") },
        { QStringLiteral("co"), QStringLiteral("CO") },
    };
    return names.value(id.toLower(), id.toUpper());
}

} // namespace clima::scales
