// SPDX-FileCopyrightText: 2026 Jowi Aoun
// SPDX-License-Identifier: GPL-3.0-or-later

#include "metrics.h"

#include "units.h"

#include <QQmlEngine>

#include <cmath>

namespace {

using Quantity = Units::Quantity;

QVariantMap entry(const QString &id, const QString &label, const QString &kind,
                  const QString &series, Quantity quantity, const QString &ramp,
                  const QString &legend)
{
    return QVariantMap{
        { QStringLiteral("id"), id },
        { QStringLiteral("label"), label },
        { QStringLiteral("kind"), kind },
        { QStringLiteral("series"), series },
        { QStringLiteral("quantity"), int(quantity) },
        { QStringLiteral("ramp"), ramp },
        { QStringLiteral("legend"), legend },
    };
}

Quantity quantityOf(const QVariantMap &metric)
{
    return static_cast<Quantity>(metric.value(QStringLiteral("quantity")).toInt());
}

// Smallest "nice" upper bound that contains the data, so an auto-scaled axis
// still lands on round numbers a human would have chosen. metrics.js's ladder,
// with the sub-1 rungs it needed once precipitation could be inches: 0.4 mm of
// rain is 0.016 in, and a ladder that starts at 0.5 would put that under an
// axis running to half an inch and draw it as nothing.
double niceMax(const QVariantList &values, double floor)
{
    double most = 0;
    for (const QVariant &value : values) {
        const double number = value.toDouble();
        if (!qIsNaN(number) && number > most)
            most = number;
    }
    if (most <= 0)
        return floor > 0 ? floor : 1;

    static const double ladder[] = { 0.02, 0.05, 0.1, 0.25, 0.5, 1,  2,   2.5,  5,
                                     10,   20,   25,  50,   100, 200, 250, 500,  1000 };
    for (const double rung : ladder) {
        if (most <= rung)
            return rung;
    }
    return std::ceil(most);
}

} // namespace

Metrics::Metrics()
{
    // The list carries units and axis bounds, so it changes when a preference
    // does. Nothing here caches; the list is rebuilt on every read, which is a
    // dozen QVariantMaps and is not a cost worth a cache that could go stale.
    connect(Units::instance(), &Units::changed, this, &Metrics::changed);
}

Metrics *Metrics::instance()
{
    static Metrics metrics;
    return &metrics;
}

Metrics *Metrics::create(QQmlEngine *, QJSEngine *)
{
    Metrics *metrics = instance();
    QQmlEngine::setObjectOwnership(metrics, QQmlEngine::CppOwnership);
    return metrics;
}

QVariantList Metrics::list() const
{
    const Units *units = Units::instance();

    // Fills in the four fields that depend on the unit in force. `axis()`
    // returns {min, max, step} chosen FOR that unit — see units.h on why a
    // converted axis lands on 32.0 / 50.0 / 68.0.
    const auto scaled = [units](QVariantMap metric, double fallbackMin, double fallbackMax,
                                double fallbackStep, int decimals) {
        const QList<double> axis = units->axis(quantityOf(metric));
        metric[QStringLiteral("min")]  = axis.size() == 3 ? axis.at(0) : fallbackMin;
        metric[QStringLiteral("max")]  = axis.size() == 3 ? axis.at(1) : fallbackMax;
        metric[QStringLiteral("step")] = axis.size() == 3 ? axis.at(2) : fallbackStep;
        metric[QStringLiteral("unit")] = units->symbol(quantityOf(metric));
        metric[QStringLiteral("decimals")] =
            quantityOf(metric) == Quantity::None ? decimals : units->decimals(quantityOf(metric));
        return metric;
    };

    QVariantList out;

    out.append(scaled(entry(QStringLiteral("overview"), tr("Overview"), QStringLiteral("area"),
                            QStringLiteral("temperature"), Quantity::Temperature,
                            QStringLiteral("temp"), tr("Temperature")),
                      0, 40, 10, 0));

    // autoScale: a fixed axis renders a drizzle as a flat line, which reads as
    // "no data" rather than "a little rain". Rain is the one variable whose
    // range genuinely spans orders of magnitude, so its axis follows the data.
    {
        QVariantMap rain = scaled(entry(QStringLiteral("precipitation"), tr("Precipitation"),
                                        QStringLiteral("bars"), QStringLiteral("precipMm"),
                                        Quantity::Precipitation, QStringLiteral("precip"),
                                        tr("Precipitation amount")),
                                  0, 4, 1, 1);
        rain[QStringLiteral("autoScale")] = true;
        // The floor an auto-scaled axis will not go below, in the display unit.
        rain[QStringLiteral("step")] = Units::instance()->convert(Quantity::Precipitation, 1.0);
        out.append(rain);
    }

    {
        QVariantMap wind = scaled(entry(QStringLiteral("wind"), tr("Wind"), QStringLiteral("area"),
                                        QStringLiteral("windSpeed"), Quantity::Wind,
                                        QStringLiteral("wind"), tr("Wind speed")),
                                  0, 40, 10, 0);
        wind[QStringLiteral("overlay")]       = QStringLiteral("windGust");
        wind[QStringLiteral("overlayLegend")] = tr("Gusts");
        out.append(wind);
    }

    out.append(scaled(entry(QStringLiteral("airquality"), tr("Air Quality"), QStringLiteral("bars"),
                            QStringLiteral("airQuality"), Quantity::None, QStringLiteral("aqi"),
                            tr("European AQI")),
                      0, 100, 25, 0));

    out.append(scaled(entry(QStringLiteral("humidity"), tr("Humidity"), QStringLiteral("area"),
                            QStringLiteral("humidity"), Quantity::Percentage,
                            QStringLiteral("humidity"), tr("Relative humidity")),
                      0, 100, 25, 0));

    out.append(scaled(entry(QStringLiteral("cloud"), tr("Cloud cover"), QStringLiteral("area"),
                            QStringLiteral("cloud"), Quantity::Percentage, QStringLiteral("cloud"),
                            tr("Total cloud cover")),
                      0, 100, 25, 0));

    out.append(scaled(entry(QStringLiteral("pressure"), tr("Pressure"), QStringLiteral("area"),
                            QStringLiteral("pressure"), Quantity::Pressure,
                            QStringLiteral("pressure"), tr("Pressure (MSL)")),
                      995, 1030, 10, 0));

    out.append(scaled(entry(QStringLiteral("uv"), tr("UV"), QStringLiteral("bars"),
                            QStringLiteral("uvIndex"), Quantity::None, QStringLiteral("uv"),
                            tr("UV index")),
                      0, 12, 3, 0));

    out.append(scaled(entry(QStringLiteral("visibility"), tr("Visibility"), QStringLiteral("area"),
                            QStringLiteral("visibility"), Quantity::Visibility,
                            QStringLiteral("visibility"), tr("Visibility")),
                      0, 25, 5, 0));

    out.append(scaled(entry(QStringLiteral("feels"), tr("Feels like"), QStringLiteral("area"),
                            QStringLiteral("apparent"), Quantity::Temperature,
                            QStringLiteral("temp"), tr("Feels like")),
                      0, 40, 10, 0));

    return out;
}

QVariantMap Metrics::byId(const QString &id) const
{
    const QVariantList all = list();
    for (const QVariant &candidate : all) {
        if (candidate.toMap().value(QStringLiteral("id")).toString() == id)
            return candidate.toMap();
    }
    return all.constFirst().toMap();
}

double Metrics::display(const QVariantMap &metric, double canonical) const
{
    if (qIsNaN(canonical))
        return canonical;
    return Units::instance()->convert(quantityOf(metric), canonical);
}

QVariantList Metrics::displayAll(const QVariantMap &metric, const QVariantList &canonical) const
{
    const Quantity quantity = quantityOf(metric);
    const Units   *units    = Units::instance();

    QVariantList out;
    out.reserve(canonical.size());
    for (const QVariant &value : canonical) {
        const double number = value.toDouble();
        out.append(qIsNaN(number) ? number : units->convert(quantity, number));
    }
    return out;
}

double Metrics::axisMax(const QVariantMap &metric, const QVariantList &values) const
{
    if (!metric.value(QStringLiteral("autoScale")).toBool())
        return metric.value(QStringLiteral("max")).toDouble();
    return niceMax(values, metric.value(QStringLiteral("step")).toDouble());
}

QVariantList Metrics::axisTicks(const QVariantMap &metric, const QVariantList &values) const
{
    const double min = metric.value(QStringLiteral("min")).toDouble();

    QVariantList ticks;

    if (metric.value(QStringLiteral("autoScale")).toBool()) {
        // Four divisions of whatever the data needed. Rounded to two places
        // because an auto axis over inches produces 0.0125 and a label that
        // long is a label nobody reads.
        const double max = axisMax(metric, values);
        for (int i = 0; i <= 4; ++i)
            ticks.append(std::round((min + (max - min) * i / 4.0) * 1000.0) / 1000.0);
        return ticks;
    }

    const double max  = metric.value(QStringLiteral("max")).toDouble();
    const double step = metric.value(QStringLiteral("step")).toDouble();
    if (step <= 0)
        return { min, max };

    for (double value = min; value <= max + 0.001; value += step)
        ticks.append(std::round(value * 100.0) / 100.0);
    return ticks;
}

QString Metrics::formatDisplay(const QVariantMap &metric, double value) const
{
    if (qIsNaN(value))
        return QStringLiteral("–");
    const int decimals = metric.value(QStringLiteral("decimals")).toInt();
    return QString::number(value, 'f', decimals)
         + metric.value(QStringLiteral("unit")).toString();
}
