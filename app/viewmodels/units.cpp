// SPDX-FileCopyrightText: 2026 Jowi Aoun
// SPDX-License-Identifier: GPL-3.0-or-later

#include "units.h"

#include "libclima/domain/units.h"

#include "settings.h"

#include <QQmlEngine>
#include <QVariantMap>

namespace {

QVariantMap choice(const QString &id, const QString &label)
{
    return QVariantMap{ { QStringLiteral("id"), id }, { QStringLiteral("label"), label } };
}

// The arithmetic lives in libclima/domain/units.h now — the factors, the
// symbols, the decimals and the two presets — because clima-cli prints a
// temperature and links no QML engine. This class is what is left: the
// reader's CHOICE, read from Settings and pushed into those functions, and
// the QML-facing shape of the result.
clima::units::Quantity bridged(Units::Quantity quantity)
{
    switch (quantity) {
    case Units::Quantity::Temperature:   return clima::units::Quantity::Temperature;
    case Units::Quantity::Wind:          return clima::units::Quantity::Wind;
    case Units::Quantity::Pressure:      return clima::units::Quantity::Pressure;
    case Units::Quantity::Visibility:    return clima::units::Quantity::Visibility;
    case Units::Quantity::Precipitation: return clima::units::Quantity::Precipitation;
    case Units::Quantity::Percentage:    return clima::units::Quantity::Percentage;
    case Units::Quantity::Direction:     return clima::units::Quantity::Direction;
    case Units::Quantity::None:          break;
    }
    return clima::units::Quantity::None;
}

} // namespace

Units::Units()
{
    // One connection per preference into one signal. See the header on why the
    // granularity stops here.
    Settings *store = Settings::instance();
    connect(store, &Settings::temperatureUnitChanged, this, &Units::changed);
    connect(store, &Settings::windUnitChanged, this, &Units::changed);
    connect(store, &Settings::pressureUnitChanged, this, &Units::changed);
    connect(store, &Settings::visibilityUnitChanged, this, &Units::changed);
    connect(store, &Settings::precipitationUnitChanged, this, &Units::changed);
}

Units *Units::instance()
{
    static Units units;
    return &units;
}

Units *Units::create(QQmlEngine *, QJSEngine *)
{
    Units *units = instance();
    QQmlEngine::setObjectOwnership(units, QQmlEngine::CppOwnership);
    return units;
}

Settings *Units::settings() const
{
    return Settings::instance();
}

// Which unit the reader chose for a quantity — the one thing this class knows
// that libclima/domain/units.h does not.
QString Units::unitFor(Quantity quantity) const
{
    switch (quantity) {
    case Quantity::Temperature:   return temperatureUnit();
    case Quantity::Wind:          return windUnit();
    case Quantity::Pressure:      return pressureUnit();
    case Quantity::Visibility:    return visibilityUnit();
    case Quantity::Precipitation: return precipitationUnit();
    case Quantity::Percentage:
    case Quantity::Direction:
    case Quantity::None:
        break;
    }
    return {};
}

QString Units::temperatureUnit() const   { return settings()->temperatureUnit(); }
QString Units::windUnit() const          { return settings()->windUnit(); }
QString Units::pressureUnit() const      { return settings()->pressureUnit(); }
QString Units::visibilityUnit() const    { return settings()->visibilityUnit(); }
QString Units::precipitationUnit() const { return settings()->precipitationUnit(); }

// ---- the conversions -----------------------------------------------------------
//
// The whole table. Canonical in, display out, and an unrecognised preference
// falls through to the canonical unit rather than to zero — a settings file
// edited by hand should give the wrong unit at worst, never a blank chart.

double Units::convert(Quantity quantity, double canonical) const
{
    return clima::units::convert(bridged(quantity), unitFor(quantity), canonical);
}

double Units::toCanonical(Quantity quantity, double display) const
{
    return clima::units::toCanonical(bridged(quantity), unitFor(quantity), display);
}

// ---- what it is called ----------------------------------------------------------

QString Units::bareSymbol(Quantity quantity) const
{
    return clima::units::symbol(bridged(quantity), unitFor(quantity));
}

QString Units::symbol(Quantity quantity) const
{
    // A degree sign hugs its number and a word does not. This is the same
    // spacing metrics.js and detaildata.js already used — "27°" but "13 km/h"
    // — recorded once rather than at every call site.
    switch (quantity) {
    case Quantity::Temperature:
    case Quantity::Direction:
        return QStringLiteral("°");
    case Quantity::Percentage:
        return QStringLiteral("%");
    case Quantity::None:
        return {};
    default:
        return QLatin1Char(' ') + bareSymbol(quantity);
    }
}

int Units::decimals(Quantity quantity) const
{
    return clima::units::decimals(bridged(quantity), unitFor(quantity));
}

QString Units::format(Quantity quantity, double canonical) const
{
    return formatDisplay(quantity, convert(quantity, canonical));
}

QString Units::formatDisplay(Quantity quantity, double display) const
{
    if (qIsNaN(display))
        return QStringLiteral("–");
    return QString::number(display, 'f', decimals(quantity)) + symbol(quantity);
}

// ---- the axes ------------------------------------------------------------------
//
// {min, max, step} in the display unit. See the header: these are chosen for
// the unit, not converted into it, which is the difference between a
// Fahrenheit axis reading 40/60/80/100 and one reading 32.0/50.0/68.0/86.0.

QList<double> Units::axis(Quantity quantity) const
{
    switch (quantity) {
    case Quantity::Temperature:
        if (temperatureUnit() == QLatin1String("fahrenheit"))
            return { 20, 110, 30 };     // −7 °C to 43 °C, four gridlines
        return { 0, 40, 10 };

    case Quantity::Wind: {
        const QString unit = windUnit();
        if (unit == QLatin1String("mph")) return { 0, 25, 5 };
        if (unit == QLatin1String("ms"))  return { 0, 12, 3 };
        if (unit == QLatin1String("kn"))  return { 0, 24, 6 };
        if (unit == QLatin1String("bft")) return { 0, 8, 2 };
        return { 0, 40, 10 };
    }

    case Quantity::Pressure: {
        const QString unit = pressureUnit();
        if (unit == QLatin1String("inhg")) return { 29.4, 30.4, 0.25 };
        if (unit == QLatin1String("mmhg")) return { 746, 773, 9 };
        return { 995, 1030, 10 };
    }

    case Quantity::Visibility:
        if (visibilityUnit() == QLatin1String("mi"))
            return { 0, 15, 3 };
        return { 0, 25, 5 };

    // Precipitation has no fixed axis: metrics.js opted it into auto-scaling
    // because rain is the one variable whose range spans orders of magnitude,
    // and the "nice maximum" ladder in app/viewmodels/metrics.cpp is what
    // decides. Returning nothing here is what says so.
    case Quantity::Precipitation:
    case Quantity::Percentage:
    case Quantity::Direction:
    case Quantity::None:
        break;
    }
    return {};
}

// ---- the settings screen's options -----------------------------------------------

QVariantList Units::choicesFor(Quantity quantity) const
{
    switch (quantity) {
    case Quantity::Temperature:
        return { choice(QStringLiteral("celsius"), QStringLiteral("°C")),
                 choice(QStringLiteral("fahrenheit"), QStringLiteral("°F")) };
    case Quantity::Wind:
        return { choice(QStringLiteral("kmh"), QStringLiteral("km/h")),
                 choice(QStringLiteral("mph"), QStringLiteral("mph")),
                 choice(QStringLiteral("ms"), QStringLiteral("m/s")),
                 choice(QStringLiteral("kn"), QStringLiteral("kn")),
                 choice(QStringLiteral("bft"), QStringLiteral("Bft")) };
    case Quantity::Pressure:
        return { choice(QStringLiteral("hpa"), QStringLiteral("hPa")),
                 choice(QStringLiteral("mb"), QStringLiteral("mb")),
                 choice(QStringLiteral("inhg"), QStringLiteral("inHg")),
                 choice(QStringLiteral("mmhg"), QStringLiteral("mmHg")) };
    case Quantity::Visibility:
        return { choice(QStringLiteral("km"), QStringLiteral("km")),
                 choice(QStringLiteral("mi"), QStringLiteral("mi")) };
    case Quantity::Precipitation:
        return { choice(QStringLiteral("mm"), QStringLiteral("mm")),
                 choice(QStringLiteral("in"), QStringLiteral("in")) };
    default:
        return {};
    }
}

// ---- the two presets ---------------------------------------------------------

QString Units::system() const
{
    return clima::units::presetFor(temperatureUnit(), windUnit(), pressureUnit(),
                                   visibilityUnit(), precipitationUnit());
}

void Units::applySystem(const QString &system)
{
    const clima::units::Preset *preset =
        system == QLatin1String("metric")     ? &clima::units::metric()
        : system == QLatin1String("imperial") ? &clima::units::imperial()
                                              : nullptr;
    if (preset == nullptr) {
        qWarning("units: %s is not a unit system", qPrintable(system));
        return;
    }
    Settings *store = settings();
    store->setTemperatureUnit(preset->temperature);
    store->setWindUnit(preset->wind);
    store->setPressureUnit(preset->pressure);
    store->setVisibilityUnit(preset->visibility);
    store->setPrecipitationUnit(preset->precipitation);
}

QVariantList Units::systemChoices() const
{
    const auto entry = [](const QString &id, const QString &label, const QString &blurb) {
        return QVariantMap{ { QStringLiteral("id"), id },
                            { QStringLiteral("label"), label },
                            { QStringLiteral("blurb"), blurb } };
    };

    return {
        entry(QStringLiteral("metric"), QStringLiteral("°C"),
              tr("Celsius, km/h, hPa, kilometres and millimetres.")),
        entry(QStringLiteral("imperial"), QStringLiteral("°F"),
              tr("Fahrenheit, mph, inHg, miles and inches.")),
    };
}

Units::Quantity Units::quantityFor(const QString &name)
{
    if (name == QLatin1String("temperature"))   return Quantity::Temperature;
    if (name == QLatin1String("wind"))          return Quantity::Wind;
    if (name == QLatin1String("pressure"))      return Quantity::Pressure;
    if (name == QLatin1String("visibility"))    return Quantity::Visibility;
    if (name == QLatin1String("precipitation")) return Quantity::Precipitation;
    if (name == QLatin1String("percentage"))    return Quantity::Percentage;
    if (name == QLatin1String("direction"))     return Quantity::Direction;
    return Quantity::None;
}
