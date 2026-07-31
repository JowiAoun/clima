// SPDX-FileCopyrightText: 2026 Jowi Aoun
// SPDX-License-Identifier: GPL-3.0-or-later
//
// The metric registry: the tab bar and the chart are both driven from this
// list.
//
// This is app/qml/Clima/metrics.js, promoted. Its header said what it would
// become:
//
//     "Adding a metric is a data change, not a code change — which is the
//      point. In libclima this becomes a C++ registry populated from provider
//      capabilities, so a tab only appears when the active provider actually
//      has that variable for that location."
//
// Half of that has happened here. The list is still a list and adding a metric
// is still one entry in it; what moved is *why* it had to move, which is the
// last two lines of its other comment: "Header/readout formatting. Kept here so
// the chart never decides units." A `.pragma library` cannot see a QML
// singleton — gallery/CMakeLists.txt already had to work around the same wall
// for theme.js — so `format()` had no way of reaching a preference, and a
// registry whose formatter cannot know whether the reader wants Fahrenheit is a
// registry that has to hand the decision back to the chart.
//
// The capability gate is not here yet. `list` is unconditional, and the day it
// is filtered by ProviderRegistry::forecastCapabilitiesAt() the filtering
// belongs in this file and nowhere else.
//
// ============================================================================
// EVERY NUMBER THIS CLASS HANDS OUT IS IN THE DISPLAY UNIT
//
// `min`, `max`, `step`, the tick values, the formatted readout and the series
// the chart plots. All of them, so that no consumer ever holds two numbers on
// different scales at once — which is the failure this arrangement exists to
// prevent, and it is a silent one: a Fahrenheit axis with a Celsius curve on it
// draws perfectly and reads 27° as freezing.
//
// The conversion itself is app/viewmodels/units.h's, always. This class picks
// *which* axis to use for the unit in force; it does not know a factor.

#pragma once

#include <QObject>
#include <QQmlEngine>
#include <QVariantList>
#include <QVariantMap>

class Metrics : public QObject
{
    Q_OBJECT
    QML_ELEMENT
    QML_SINGLETON

    // Every metric, in tab order. Each entry carries the same keys metrics.js
    // used — id, label, kind, series, unit, min, max, step, ramp, decimals,
    // legend, and optionally autoScale, overlay and overlayLegend — because
    // MetricTabBar and MobileMetricPicker read them by name.
    Q_PROPERTY(QVariantList list READ list NOTIFY changed)

public:
    static Metrics *instance();
    static Metrics *create(QQmlEngine *qmlEngine, QJSEngine *jsEngine);

    [[nodiscard]] QVariantList list() const;

    // The entry for an id, or the first entry. Never an empty map: a chart
    // handed one would bind `metric.min` to undefined and draw an axis of NaN.
    [[nodiscard]] Q_INVOKABLE QVariantMap byId(const QString &id) const;

    // Canonical → display, for one value and for a whole series. The chart
    // plots what these return, against the axis below.
    [[nodiscard]] Q_INVOKABLE double       display(const QVariantMap &metric, double canonical) const;
    [[nodiscard]] Q_INVOKABLE QVariantList displayAll(const QVariantMap &metric,
                                                      const QVariantList &canonical) const;

    // The axis, honouring autoScale. `values` are DISPLAY values — the same
    // ones the chart is about to plot — because a "nice maximum" chosen from
    // millimetres and then converted to inches is not a nice maximum.
    [[nodiscard]] Q_INVOKABLE double       axisMax(const QVariantMap &metric,
                                                   const QVariantList &values) const;
    [[nodiscard]] Q_INVOKABLE QVariantList axisTicks(const QVariantMap &metric,
                                                     const QVariantList &values) const;

    // The readout. metrics.js's format(), with a unit preference behind it.
    [[nodiscard]] Q_INVOKABLE QString formatDisplay(const QVariantMap &metric, double value) const;

Q_SIGNALS:
    void changed();

private:
    Metrics();
};
