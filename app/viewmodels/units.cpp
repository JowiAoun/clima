// SPDX-FileCopyrightText: 2026 Jowi Aoun
// SPDX-License-Identifier: GPL-3.0-or-later

#include "units.h"

#include "settings.h"

#include <QQmlEngine>
#include <QVariantMap>

namespace {

QVariantMap choice(const QString &id, const QString &label)
{
    return QVariantMap{ { QStringLiteral("id"), id }, { QStringLiteral("label"), label } };
}

// The two presets, as the five spellings each one writes. In this order:
// temperature, wind, pressure, visibility, precipitation.
//
// `metric` is the same five strings Settings falls back to when nothing has been
// stored, and deliberately so — a reader who opens preferences on a fresh
// install sees "°C" already selected rather than "custom", which is what a
// second table drifting from Settings' defaults would show them.
struct SystemSpec
{
    const char *id;
    QString temperature;
    QString wind;
    QString pressure;
    QString visibility;
    QString precipitation;
};

const SystemSpec &metricSpec()
{
    static const SystemSpec spec{ "metric",
                                  QStringLiteral("celsius"), QStringLiteral("kmh"),
                                  QStringLiteral("hpa"), QStringLiteral("km"),
                                  QStringLiteral("mm") };
    return spec;
}

const SystemSpec &imperialSpec()
{
    // inHg and not mb, miles and not km: this is the bundle a US reader means by
    // "imperial", and every entry is one `choicesFor` already offers.
    static const SystemSpec spec{ "imperial",
                                  QStringLiteral("fahrenheit"), QStringLiteral("mph"),
                                  QStringLiteral("inhg"), QStringLiteral("mi"),
                                  QStringLiteral("in") };
    return spec;
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
    switch (quantity) {
    case Quantity::Temperature:
        if (temperatureUnit() == QLatin1String("fahrenheit"))
            return canonical * 9.0 / 5.0 + 32.0;
        return canonical;

    case Quantity::Wind: {
        const QString unit = windUnit();
        if (unit == QLatin1String("mph"))
            return canonical * 0.621371;
        if (unit == QLatin1String("ms"))
            return canonical / 3.6;
        if (unit == QLatin1String("kn"))
            return canonical * 0.539957;
        if (unit == QLatin1String("bft"))
            // Beaufort from km/h, the inverse of the standard v = 0.836 B^1.5
            // in m/s. Rounded by the caller like everything else; the fraction
            // is meaningless but the truncation belongs at the formatting step
            // so that a chart can still draw a smooth curve.
            return std::pow(canonical / 3.6 / 0.836, 2.0 / 3.0);
        return canonical;
    }

    case Quantity::Pressure: {
        const QString unit = pressureUnit();
        if (unit == QLatin1String("inhg"))
            return canonical * 0.02952998;
        if (unit == QLatin1String("mmhg"))
            return canonical * 0.7500617;
        // "mb" is hPa under another name — one millibar is one hectopascal
        // exactly — and it is offered because that is the word half the world's
        // forecasts use. detaildata.js labelled the pressure card "mb".
        return canonical;
    }

    case Quantity::Visibility:
        if (visibilityUnit() == QLatin1String("mi"))
            return canonical * 0.621371;
        return canonical;

    case Quantity::Precipitation:
        if (precipitationUnit() == QLatin1String("in"))
            return canonical / 25.4;
        return canonical;

    case Quantity::None:
    case Quantity::Percentage:
    case Quantity::Direction:
        return canonical;
    }
    return canonical;
}

double Units::toCanonical(Quantity quantity, double display) const
{
    switch (quantity) {
    case Quantity::Temperature:
        if (temperatureUnit() == QLatin1String("fahrenheit"))
            return (display - 32.0) * 5.0 / 9.0;
        return display;

    case Quantity::Wind: {
        const QString unit = windUnit();
        if (unit == QLatin1String("mph"))
            return display / 0.621371;
        if (unit == QLatin1String("ms"))
            return display * 3.6;
        if (unit == QLatin1String("kn"))
            return display / 0.539957;
        if (unit == QLatin1String("bft"))
            return 0.836 * std::pow(display, 1.5) * 3.6;
        return display;
    }

    case Quantity::Pressure: {
        const QString unit = pressureUnit();
        if (unit == QLatin1String("inhg"))
            return display / 0.02952998;
        if (unit == QLatin1String("mmhg"))
            return display / 0.7500617;
        return display;
    }

    case Quantity::Visibility:
        if (visibilityUnit() == QLatin1String("mi"))
            return display / 0.621371;
        return display;

    case Quantity::Precipitation:
        if (precipitationUnit() == QLatin1String("in"))
            return display * 25.4;
        return display;

    case Quantity::None:
    case Quantity::Percentage:
    case Quantity::Direction:
        return display;
    }
    return display;
}

// ---- what it is called ----------------------------------------------------------

QString Units::bareSymbol(Quantity quantity) const
{
    switch (quantity) {
    case Quantity::Temperature:
        return temperatureUnit() == QLatin1String("fahrenheit") ? QStringLiteral("°F")
                                                                : QStringLiteral("°C");
    case Quantity::Wind: {
        const QString unit = windUnit();
        if (unit == QLatin1String("mph")) return QStringLiteral("mph");
        if (unit == QLatin1String("ms"))  return QStringLiteral("m/s");
        if (unit == QLatin1String("kn"))  return QStringLiteral("kn");
        if (unit == QLatin1String("bft")) return QStringLiteral("Bft");
        return QStringLiteral("km/h");
    }
    case Quantity::Pressure: {
        const QString unit = pressureUnit();
        if (unit == QLatin1String("inhg")) return QStringLiteral("inHg");
        if (unit == QLatin1String("mmhg")) return QStringLiteral("mmHg");
        if (unit == QLatin1String("mb"))   return QStringLiteral("mb");
        return QStringLiteral("hPa");
    }
    case Quantity::Visibility:
        return visibilityUnit() == QLatin1String("mi") ? QStringLiteral("mi")
                                                       : QStringLiteral("km");
    case Quantity::Precipitation:
        return precipitationUnit() == QLatin1String("in") ? QStringLiteral("in")
                                                          : QStringLiteral("mm");
    case Quantity::Percentage:
        return QStringLiteral("%");
    case Quantity::Direction:
        return QStringLiteral("°");
    case Quantity::None:
        break;
    }
    return {};
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
    switch (quantity) {
    case Quantity::Precipitation:
        // Two for inches, one for millimetres. 0.4 mm is 0.016 in: rounded to
        // one place it is 0.0, which draws a bar and labels it as nothing.
        return precipitationUnit() == QLatin1String("in") ? 2 : 1;
    case Quantity::Pressure:
        return pressureUnit() == QLatin1String("inhg") ? 2 : 0;
    case Quantity::Wind:
        return windUnit() == QLatin1String("ms") ? 1 : 0;
    default:
        return 0;
    }
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
    const auto matches = [this](const SystemSpec &spec) {
        return temperatureUnit() == spec.temperature && windUnit() == spec.wind
            && pressureUnit() == spec.pressure && visibilityUnit() == spec.visibility
            && precipitationUnit() == spec.precipitation;
    };

    if (matches(metricSpec()))
        return QStringLiteral("metric");
    if (matches(imperialSpec()))
        return QStringLiteral("imperial");
    return QStringLiteral("custom");
}

void Units::applySystem(const QString &system)
{
    const SystemSpec *spec = system == QLatin1String("metric")     ? &metricSpec()
                             : system == QLatin1String("imperial") ? &imperialSpec()
                                                                   : nullptr;
    if (spec == nullptr) {
        qWarning("units: %s is not a unit system", qPrintable(system));
        return;
    }

    // Five writes, each of which emits its own Settings signal and so five
    // `changed()` in a row. That is not worth suppressing: every consumer of
    // this signal rebuilds a snapshot, and rebuilding five times in one call
    // stack costs a few hundred microseconds once, on a click.
    Settings *store = settings();
    store->setTemperatureUnit(spec->temperature);
    store->setWindUnit(spec->wind);
    store->setPressureUnit(spec->pressure);
    store->setVisibilityUnit(spec->visibility);
    store->setPrecipitationUnit(spec->precipitation);
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
