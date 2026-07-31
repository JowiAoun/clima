// SPDX-FileCopyrightText: 2026 Jowi Aoun
// SPDX-License-Identifier: MPL-2.0

#include "libclima/providers/iforecastprovider.h"

#include <QStringList>

namespace clima {

// Out of line, and not `= default` in the header. A polymorphic class whose
// virtual functions are all inline gets its vtable emitted in every translation
// unit that includes it — the "weak vtable" warning — and the key-function rule
// says the vtable is emitted once, here, if exactly one virtual function is
// defined in exactly one place. The destructors are that place.
IProvider::~IProvider() = default;
IForecastProvider::~IForecastProvider() = default;
IAirQualityProvider::~IAirQualityProvider() = default;

// ---- attribution -------------------------------------------------------------

bool Attribution::isComplete() const
{
    return firstMissingField().isEmpty();
}

QString Attribution::firstMissingField() const
{
    // The order is the order a licence complaint would be written in: who,
    // what they asked us to say, where to check, and under what terms. The
    // registry puts the returned name straight into its rejection message, so
    // an author who forgot one is told which one rather than told to look.
    if (name.trimmed().isEmpty())
        return QStringLiteral("name");
    if (creditLine.trimmed().isEmpty())
        return QStringLiteral("creditLine");
    if (!homepage.isValid() || homepage.isEmpty())
        return QStringLiteral("homepage");
    if (licenceName.trimmed().isEmpty())
        return QStringLiteral("licenceName");
    if (!licenceUrl.isValid() || licenceUrl.isEmpty())
        return QStringLiteral("licenceUrl");
    return {};
}

// ---- capabilities ------------------------------------------------------------

Capabilities::Capabilities(CapabilityFlags available, CapabilityFlags undetermined)
    : m_available(available)
    , m_undetermined(undetermined & ~available)
{
    // The overlap is cleared rather than asserted on. A provider that says a
    // capability is both known-good and unknown means the first — it learned
    // the answer and forgot to take it out of the other set — and the cost of
    // being wrong about that is a tab that appears, which is the same thing
    // `available` was already asking for. Crashing the app over a redundant
    // flag helps nobody.
}

bool Capabilities::has(Capability capability) const
{
    return m_available.testFlag(capability);
}

bool Capabilities::isUndetermined(Capability capability) const
{
    return m_undetermined.testFlag(capability);
}

bool Capabilities::isKnownAbsent(Capability capability) const
{
    return !has(capability) && !isUndetermined(capability);
}

bool Capabilities::operator==(const Capabilities &other) const
{
    return m_available == other.m_available && m_undetermined == other.m_undetermined;
}

QString Capabilities::toString() const
{
    // Bit order, which is enum order, which is the order the header lists them
    // in. Stable across runs and across machines: this string ends up in
    // QCOMPARE failure output and in the diagnostics panel, and a set that
    // printed in hash order would make two identical capability sets look
    // different in a diff.
    QStringList parts;
    for (quint32 bit = 0; bit < 32; ++bit) {
        const auto    capability = static_cast<Capability>(1U << bit);
        const QString name       = capabilityName(capability);
        if (name.isEmpty())
            continue;
        if (has(capability))
            parts.append(name);
        else if (isUndetermined(capability))
            parts.append(name + QStringLiteral("?"));
    }
    if (parts.isEmpty())
        return QStringLiteral("none");
    return parts.join(QStringLiteral(", "));
}

QString capabilityName(Capability capability)
{
    // Total over the enum. -Wswitch is on for this library, so a capability
    // added without a name here stops the build — which matters more than it
    // looks, because the name is what the About screen and every test failure
    // print, and an unnamed flag would show up as a silent gap in both.
    switch (capability) {
    case Capability::None:                     return {};
    case Capability::CurrentConditions:        return QStringLiteral("current");
    case Capability::Hourly:                   return QStringLiteral("hourly");
    case Capability::Daily:                    return QStringLiteral("daily");
    case Capability::Minutely15:               return QStringLiteral("minutely15");
    case Capability::Ensemble:                 return QStringLiteral("ensemble");
    case Capability::ModelSelection:           return QStringLiteral("models");
    case Capability::HistoricalArchive:        return QStringLiteral("archive");
    case Capability::Temperature:              return QStringLiteral("temperature");
    case Capability::ApparentTemperature:      return QStringLiteral("apparent");
    case Capability::DewPoint:                 return QStringLiteral("dewpoint");
    case Capability::Humidity:                 return QStringLiteral("humidity");
    case Capability::Precipitation:            return QStringLiteral("precipitation");
    case Capability::PrecipitationType:        return QStringLiteral("precipitation-type");
    case Capability::PrecipitationProbability: return QStringLiteral("precipitation-probability");
    case Capability::Wind:                     return QStringLiteral("wind");
    case Capability::WindGust:                 return QStringLiteral("wind-gust");
    case Capability::Pressure:                 return QStringLiteral("pressure");
    case Capability::CloudCover:               return QStringLiteral("cloud");
    case Capability::Visibility:               return QStringLiteral("visibility");
    case Capability::UvIndex:                  return QStringLiteral("uv");
    case Capability::WeatherCode:              return QStringLiteral("weather-code");
    case Capability::SunTimes:                 return QStringLiteral("sun");
    case Capability::AirQualityIndex:          return QStringLiteral("aqi");
    case Capability::Pollutants:               return QStringLiteral("pollutants");
    case Capability::Ammonia:                  return QStringLiteral("ammonia");
    case Capability::Pollen:                   return QStringLiteral("pollen");
    case Capability::Alerts:                   return QStringLiteral("alerts");
    case Capability::Radar:                    return QStringLiteral("radar");
    }
    return {};
}

} // namespace clima
