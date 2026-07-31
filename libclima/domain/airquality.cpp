// SPDX-FileCopyrightText: 2026 Jowi Aoun
// SPDX-License-Identifier: MPL-2.0

#include "libclima/domain/airquality.h"

#include <array>

namespace clima {

namespace {

// ---- the European AQI breakpoints -------------------------------------------
//
// Five pollutants, six bands each, from the European Environment Agency's index
// definition. Each band is twenty index points wide, so a concentration is
// mapped by finding its band and interpolating linearly inside it:
//
//     index level     index range     name
//     1               0 - 20          good
//     2               20 - 40         fair
//     3               40 - 60         moderate
//     4               60 - 80         poor
//     5               80 - 100        very poor
//     6               100 +           extremely poor
//
// The numbers are hourly concentrations in µg/m³ and they are the published
// breakpoints, not a fit. "Extremely poor" has no ceiling, so the last band
// keeps rising rather than clamping at 100: clamping would make every
// catastrophic hour look identical to every merely terrible one, and an argmax
// between two clamped pollutants would be decided by enum order.
//
// This table is the *fallback* path. Read airquality.h: it is exact for the
// three gases and materially wrong for the two particulates, because the EAQI
// defines PM2.5 and PM10 on a 24-hour running mean. Everything Open-Meteo
// serves comes with the provider's own sub-indices and takes that route
// instead; this exists so that a provider without them still gets a pollutant
// name rather than nothing.
struct Band {
    double upperConcentration;
    double upperIndex;
};

using Bands = std::array<Band, 6>;

const Bands &bandsFor(Pollutant pollutant)
{
    static const Bands pm2_5 = { { { 10, 20 }, { 20, 40 }, { 25, 60 },
                                   { 50, 80 }, { 75, 100 }, { 800, 120 } } };
    static const Bands pm10 = { { { 20, 20 }, { 40, 40 }, { 50, 60 },
                                  { 100, 80 }, { 150, 100 }, { 1200, 120 } } };
    static const Bands ozone = { { { 50, 20 }, { 100, 40 }, { 130, 60 },
                                   { 240, 80 }, { 380, 100 }, { 800, 120 } } };
    static const Bands nitrogenDioxide = { { { 40, 20 }, { 90, 40 }, { 120, 60 },
                                             { 230, 80 }, { 340, 100 }, { 1000, 120 } } };
    static const Bands sulphurDioxide = { { { 100, 20 }, { 200, 40 }, { 350, 60 },
                                            { 500, 80 }, { 750, 100 }, { 1250, 120 } } };
    static const Bands none = { { { 0, 0 }, { 0, 0 }, { 0, 0 }, { 0, 0 }, { 0, 0 }, { 0, 0 } } };

    switch (pollutant) {
    case Pollutant::Pm2_5:           return pm2_5;
    case Pollutant::Pm10:            return pm10;
    case Pollutant::Ozone:           return ozone;
    case Pollutant::NitrogenDioxide: return nitrogenDioxide;
    case Pollutant::SulphurDioxide:  return sulphurDioxide;
    case Pollutant::CarbonMonoxide:  return none;
    case Pollutant::Count:           return none;
    }
    return none;
}

// The argmax, over whichever map the caller decided is authoritative. Factored
// out so that the published sub-indices and the computed ones cannot be
// compared against each other by accident — a mixture would rank an ozone value
// from CAMS against a PM2.5 value we invented, and the twenty-two-point error
// documented in airquality.h would decide the winner.
std::optional<Pollutant> argmax(const QMap<Pollutant, double> &indices)
{
    std::optional<Pollutant> best;
    double                   bestIndex = -1.0;

    // Enum order breaks a tie, and it is the order a detail card lists —
    // particulates first. Deterministic by construction: a QMap keyed by the
    // enum iterates in that order, so two runs over the same payload name the
    // same pollutant. Determinism is an invariant here rather than a nicety;
    // golden-image tests photograph this string.
    for (auto it = indices.cbegin(); it != indices.cend(); ++it) {
        if (it.value() > bestIndex) {
            bestIndex = it.value();
            best      = it.key();
        }
    }
    return best;
}

} // namespace

QString pollutantId(Pollutant pollutant)
{
    switch (pollutant) {
    case Pollutant::Pm2_5:           return QStringLiteral("pm2_5");
    case Pollutant::Pm10:            return QStringLiteral("pm10");
    case Pollutant::Ozone:           return QStringLiteral("ozone");
    case Pollutant::NitrogenDioxide: return QStringLiteral("nitrogen_dioxide");
    case Pollutant::SulphurDioxide:  return QStringLiteral("sulphur_dioxide");
    case Pollutant::CarbonMonoxide:  return QStringLiteral("carbon_monoxide");
    case Pollutant::Count:           break;
    }
    return {};
}

QString europeanSubIndexId(Pollutant pollutant)
{
    // Empty for carbon monoxide. The query builder reads that as "do not ask",
    // which is the same fact as "CO cannot be the dominant pollutant" expressed
    // where it saves a series rather than where it decides an argmax.
    if (pollutant == Pollutant::CarbonMonoxide || pollutant == Pollutant::Count)
        return {};
    return QStringLiteral("european_aqi_") + pollutantId(pollutant);
}

QString pollenSpeciesId(PollenSpecies species)
{
    switch (species) {
    case PollenSpecies::Alder:   return QStringLiteral("alder_pollen");
    case PollenSpecies::Birch:   return QStringLiteral("birch_pollen");
    case PollenSpecies::Grass:   return QStringLiteral("grass_pollen");
    case PollenSpecies::Mugwort: return QStringLiteral("mugwort_pollen");
    case PollenSpecies::Olive:   return QStringLiteral("olive_pollen");
    case PollenSpecies::Ragweed: return QStringLiteral("ragweed_pollen");
    case PollenSpecies::Count:   break;
    }
    return {};
}

QString pollutantUnit(Pollutant pollutant)
{
    switch (pollutant) {
    case Pollutant::Pm2_5:
    case Pollutant::Pm10:
    case Pollutant::Ozone:
    case Pollutant::NitrogenDioxide:
    case Pollutant::SulphurDioxide:
    case Pollutant::CarbonMonoxide:
        return QStringLiteral("µg/m³");
    case Pollutant::Count:
        break;
    }
    return {};
}

std::optional<double> europeanSubIndex(Pollutant pollutant, double concentration)
{
    if (pollutant == Pollutant::CarbonMonoxide || pollutant == Pollutant::Count)
        return std::nullopt;

    // A negative concentration is not a small one; it is a broken payload, and
    // returning 0 for it would let a corrupt reading win an argmax by being
    // quietly plausible.
    if (concentration < 0.0)
        return std::nullopt;

    const Bands &bands = bandsFor(pollutant);

    double lowerConcentration = 0.0;
    double lowerIndex         = 0.0;
    for (size_t i = 0; i < bands.size(); ++i) {
        const Band &band = bands[i];
        const bool  last = i + 1 == bands.size();

        if (concentration <= band.upperConcentration || last) {
            const double span = band.upperConcentration - lowerConcentration;
            if (span <= 0.0)
                return lowerIndex;
            const double fraction = (concentration - lowerConcentration) / span;
            return lowerIndex + fraction * (band.upperIndex - lowerIndex);
        }

        lowerConcentration = band.upperConcentration;
        lowerIndex         = band.upperIndex;
    }
    return lowerIndex;
}

std::optional<Pollutant> AirQualityPoint::dominantPollutant() const
{
    // Published first, and never both. See airquality.h.
    if (!europeanSubIndices.isEmpty())
        return argmax(europeanSubIndices);

    QMap<Pollutant, double> computed;
    for (auto it = pollutants.cbegin(); it != pollutants.cend(); ++it) {
        const std::optional<double> index = europeanSubIndex(it.key(), it.value());
        if (index.has_value())
            computed.insert(it.key(), *index);
    }
    return argmax(computed);
}

std::optional<double> AirQualityPoint::dominantSubIndex() const
{
    const std::optional<Pollutant> pollutant = dominantPollutant();
    if (!pollutant.has_value())
        return std::nullopt;

    const auto published = europeanSubIndices.constFind(*pollutant);
    if (published != europeanSubIndices.cend())
        return *published;

    return europeanSubIndex(*pollutant, pollutants.value(*pollutant));
}

std::optional<double> AirQualityPoint::dominantConcentration() const
{
    const std::optional<Pollutant> pollutant = dominantPollutant();
    if (!pollutant.has_value())
        return std::nullopt;

    // A provider that published a sub-index but no concentration has told us
    // which pollutant dominates without telling us how much of it there is.
    // Absent, rather than zero.
    const auto concentration = pollutants.constFind(*pollutant);
    if (concentration == pollutants.cend())
        return std::nullopt;
    return *concentration;
}

} // namespace clima
