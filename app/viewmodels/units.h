// SPDX-FileCopyrightText: 2026 Jowi Aoun
// SPDX-License-Identifier: GPL-3.0-or-later
//
// Every unit conversion in Clima, in one file, applied at one boundary.
//
// ============================================================================
// WHERE THE CONVERSION HAPPENS, AND WHY IT IS NOT WHERE YOU WOULD PUT IT
//
// libclima speaks one set of units and only one: °C, km/h, hPa, km, mm, and
// libclima/domain/forecast.h says so on every field. That is not a metric
// preference — it is the property that lets a cached payload survive a change
// of preference, and that keeps `precip.js`'s NWS intensity thresholds (2.5
// mm/h moderate, 7.6 mm/h heavy) meaning what the NWS meant by them when the
// reader has asked for inches.
//
// So the conversion is *late*: it happens in app/viewmodels/, on the way out,
// and everything upstream of that line is canonical. app/qml/Clima/metrics.js
// wrote this rule down before there was anything to enforce it —
//
//     "Header/readout formatting. Kept here so the chart never decides units."
//
// — and this file is that sentence promoted from a comment above one function
// to the only place in the program that knows a conversion factor. There is no
// other. Grep for 1.8 and 0.621 and 0.02953; they are here and nowhere.
//
// ============================================================================
// THE AXIS IS PRODUCED IN THE DISPLAY UNIT, NOT CONVERTED AFTER
//
// This is the half that is easy to get wrong and impossible to un-see
// afterwards. metrics.js's temperature axis runs 0 → 40 in steps of 10, which
// is four round numbers a person would have chosen. Convert those four numbers
// and the Fahrenheit axis reads 32.0 / 50.0 / 68.0 / 86.0 / 104.0 — every
// gridline a fraction, none of them a number anybody thinks in.
//
// The fix is not to round afterwards, which moves the gridlines off the values
// they label. It is to pick the axis *in the unit it will be drawn in*: a
// Fahrenheit temperature axis is 40 → 100 by 20, chosen for Fahrenheit, and
// this class carries one such choice per (quantity, unit) pair. See
// app/viewmodels/metrics.h, which asks for it.
//
// ============================================================================
// PER QUANTITY. THERE IS NO METRIC/IMPERIAL SWITCH AND THERE WILL NOT BE ONE
//
// docs/04-architecture.md §4.10, and app/settings.h repeats it: people want °C
// with mph, or inHg with mm. Five independent preferences, five conversions,
// and a `Quantity` enum with no "system" anywhere in it.

#pragma once

#include <QObject>
#include <QQmlEngine>
#include <QString>
#include <QVariantList>

class Settings;

class Units : public QObject
{
    Q_OBJECT
    QML_ELEMENT
    QML_SINGLETON

    // The five, mirrored from Settings so that QML binds to one object rather
    // than to two. Read-only here: writing a preference goes through Settings,
    // which is the thing that owns the file.
    Q_PROPERTY(QString temperature READ temperatureUnit NOTIFY changed)
    Q_PROPERTY(QString wind READ windUnit NOTIFY changed)
    Q_PROPERTY(QString pressure READ pressureUnit NOTIFY changed)
    Q_PROPERTY(QString visibility READ visibilityUnit NOTIFY changed)
    Q_PROPERTY(QString precipitation READ precipitationUnit NOTIFY changed)

    // "metric" | "imperial" | "custom" — a READING of the five above, never a
    // sixth preference. See `applySystem` for why the distinction is the whole
    // of the design here.
    Q_PROPERTY(QString system READ system NOTIFY changed)

public:
    // The quantities that have a unit preference, plus the ones that do not
    // and still need a symbol. `None` is not "dimensionless" — the UV index and
    // the air-quality index are genuinely unitless and go through here so that
    // a caller never has to branch on whether a conversion applies.
    enum class Quantity {
        None,
        Temperature,
        Wind,
        Pressure,
        Visibility,
        Precipitation,
        Percentage,
        Direction,
    };
    Q_ENUM(Quantity)

    static Units *instance();
    static Units *create(QQmlEngine *qmlEngine, QJSEngine *jsEngine);

    [[nodiscard]] QString temperatureUnit() const;
    [[nodiscard]] QString windUnit() const;
    [[nodiscard]] QString pressureUnit() const;
    [[nodiscard]] QString visibilityUnit() const;
    [[nodiscard]] QString precipitationUnit() const;

    // Canonical → display. The one function. Everything else in this class is
    // either a lookup of which unit is selected or a wrapper around this.
    //
    // Q_INVOKABLE and also plain C++, because both callers matter: the view
    // models convert whole series in C++, and a QML file with a stray number
    // that has not been through a view model can still ask.
    Q_INVOKABLE double convert(Quantity quantity, double canonical) const;

    // Display → canonical. Not currently called by anything in the product and
    // deliberately present anyway: the day a UI takes a temperature as *input*
    // — a threshold for a notification, a units field in a text box — the
    // inverse has to exist, and a half-invertible conversion table is one where
    // somebody eventually writes the other direction by hand at the call site.
    [[nodiscard]] double toCanonical(Quantity quantity, double display) const;

    // "°", " km/h", " mm". Leading space where the reference sets one, because
    // "13 km/h" and "27°" are both right and the difference is the unit's, not
    // the caller's. Matches the `unit` strings metrics.js used verbatim.
    Q_INVOKABLE QString symbol(Quantity quantity) const;

    // The bare symbol with no leading space, for a settings row and for a
    // column header. "km/h", not " km/h".
    Q_INVOKABLE QString bareSymbol(Quantity quantity) const;

    // How many decimals this quantity is worth showing in *this* unit. Inches
    // of rain need two and millimetres need one, which is the same fact about
    // the reader's resolution expressed in two different numbers.
    Q_INVOKABLE int decimals(Quantity quantity) const;

    // value + symbol, rounded to decimals(). The choke point metrics.js's
    // format() was, moved somewhere a preference can reach it.
    Q_INVOKABLE QString format(Quantity quantity, double canonical) const;

    // Same, but the caller has already converted. For a view model that holds
    // display values and needs the label back.
    Q_INVOKABLE QString formatDisplay(Quantity quantity, double display) const;

    // The axis for a quantity, IN THE DISPLAY UNIT — see the header. Returns
    // {min, max, step}; an empty list for a quantity with no fixed axis.
    [[nodiscard]] QList<double> axis(Quantity quantity) const;

    // What a settings screen offers, as {id, label} pairs. Generated here so
    // that the list of units a user can pick and the list this class can
    // convert cannot drift apart.
    Q_INVOKABLE QVariantList choicesFor(Quantity quantity) const;

    // ========================================================================
    // THE TWO PRESETS, AND WHY THEY DO NOT CONTRADICT THE HEADER
    //
    // The header above says there is no metric/imperial switch and there will
    // not be one, and there still is not. What follows is a pair of *shortcuts*
    // that write the five preferences at once, plus a readout of whether the
    // five currently happen to spell one of them.
    //
    // The difference is not a word game and it is visible in one keystroke.
    // Choosing "imperial" here sets fahrenheit/mph/inHg/mi/in — and then setting
    // precipitation back to millimetres is allowed, leaves the other four alone,
    // and makes `system()` answer "custom". A real switch could not do that:
    // it would own the five, and °C-with-mph — the single most repeated
    // complaint under every weather app's reviews — would be unreachable.
    //
    // So the preset is an accelerator for the common case and the per-quantity
    // rows underneath it remain the truth. A settings screen showing both is
    // showing the model rather than hiding it: docs/04-architecture.md §4.10.
    //
    // `system()` returns "custom" for any mixture, which is a state the UI has
    // to be able to draw — neither radio filled — rather than one it may round
    // to the nearest preset.
    [[nodiscard]] QString system() const;

    // Writes all five. A no-op for an id that is neither "metric" nor
    // "imperial", because the alternative is a typo silently resetting somebody
    // to Celsius.
    Q_INVOKABLE void applySystem(const QString &system);

    // {id, label, blurb} for the presets, in the order a screen should list
    // them. Here rather than in QML for the same reason `choicesFor` is: the
    // list a reader can pick from and the list `applySystem` understands are one
    // list, and the blurb names the units the preset actually writes.
    Q_INVOKABLE QVariantList systemChoices() const;

    // The spelling → Quantity, for QML and for the metric registry. Unknown
    // reads as None rather than as an error: a metric with no unit is a
    // legitimate thing.
    Q_INVOKABLE static Quantity quantityFor(const QString &name);

Q_SIGNALS:
    // One signal for all five. A view model rebuilds its whole snapshot when
    // any unit changes — there is no cheaper granularity worth having, because
    // the temperature series and the "feels like" series and the day strip all
    // move together — and five signals connected to one slot is five ways to
    // forget the fifth.
    void changed();

private:
    Units();

    [[nodiscard]] Settings *settings() const;
};
