// SPDX-FileCopyrightText: 2026 Jowi Aoun
// SPDX-License-Identifier: GPL-3.0-or-later
//
// The observation, in the shape the twelve detail cards already read.
//
// This is detaildata.js with a provider behind it, the way
// app/viewmodels/forecastdata.h is mockdata.js with one. The property names,
// the nesting and the field names are that file's, verbatim, which is why the
// nineteen QML files that read `Detail.wind.gust` did not have to change.
//
// ============================================================================
// THREE KINDS OF FIELD, AND ONLY ONE OF THEM IS A MEASUREMENT
//
// detaildata.js mixed them freely and it was right to: a card wants all three
// at once. But they come from three different places and it matters which.
//
//   measurements    `value`, `speed`, `series`, `high`. Straight from the
//                   provider, converted once through app/viewmodels/units.h.
//
//   verdicts        `band`, `status`, `tone`, `beaufortName`. Published scales
//                   applied to a measurement — the WHO's UV bands, the European
//                   AQI's, Beaufort. Ours to compute, not ours to invent, and
//                   each one names its authority at the function that computes
//                   it.
//
//   prose           `body`, `summary`. Sentences, generated here, translated
//                   with qsTr. These are the ones that could be a lie, so each
//                   is built out of numbers that are on the same screen: the
//                   summary says "heavy rain from 2 p.m." only when the hourly
//                   series has heavy rain at 14:00, and it says nothing at all
//                   when there is nothing to say.
//
// ============================================================================
// WHAT HAS NO PROVIDER BEHIND IT, AND HOW THAT IS HANDLED
//
// Two blocks here are not data products anywhere, and docs/08-risks.md R9 —
// "region-gate honestly, never fabricate" — decides what to do about each.
//
//   pollen        IS a product, in Europe only. So it is gated, not computed:
//                 `hasPollen` comes from Capability::Pollen at this coordinate
//                 and the card is *hidden* outside the CAMS European domain
//                 rather than drawn empty. See
//                 libclima/providers/airquality/openmeteoairqualityprovider.h,
//                 which argues that the payload is a better witness than a
//                 bounding box.
//
//   activities    is NOT a product anywhere. Nobody publishes "do you need an
//                 umbrella". So it is computed here, from our own numbers, by
//                 rules written down at `buildActivities()` — and the card says
//                 it is ours. An app that presented a derived verdict as a
//                 provider's is doing the thing R9 is about, whether or not the
//                 arithmetic is sound.

#pragma once

#include "libclima/domain/airquality.h"
#include "libclima/domain/forecast.h"
#include "libclima/domain/place.h"

#include <QDateTime>
#include <QObject>
#include <QQmlEngine>
#include <QTimeZone>
#include <QVariantList>
#include <QVariantMap>

class ConditionsData : public QObject
{
    Q_OBJECT
    QML_NAMED_ELEMENT(Detail)
    QML_SINGLETON

    Q_PROPERTY(QString observedAt READ observedAt NOTIFY changed)
    Q_PROPERTY(QString observedOn READ observedOn NOTIFY changed)
    Q_PROPERTY(QVariantMap location READ location NOTIFY changed)
    Q_PROPERTY(QVariantMap current READ current NOTIFY changed)

    // Index into the twelve-hour context window every card's sparkline draws.
    // Six: six hours behind, five ahead. detaildata.js's number, kept because
    // the cards' layouts are built around where the marker falls.
    Q_PROPERTY(int nowIndex READ nowIndex NOTIFY changed)

    Q_PROPERTY(QVariantMap temperature READ temperature NOTIFY changed)
    Q_PROPERTY(QVariantMap feelsLike READ feelsLike NOTIFY changed)
    Q_PROPERTY(QVariantMap cloudCover READ cloudCover NOTIFY changed)
    Q_PROPERTY(QVariantMap precipitation READ precipitation NOTIFY changed)
    Q_PROPERTY(QVariantMap wind READ wind NOTIFY changed)
    Q_PROPERTY(QVariantMap humidity READ humidity NOTIFY changed)
    Q_PROPERTY(QVariantMap uv READ uv NOTIFY changed)
    Q_PROPERTY(QVariantMap airQuality READ airQuality NOTIFY changed)
    Q_PROPERTY(QVariantMap visibility READ visibility NOTIFY changed)
    Q_PROPERTY(QVariantMap pressure READ pressure NOTIFY changed)
    Q_PROPERTY(QVariantMap sun READ sun NOTIFY changed)
    Q_PROPERTY(QVariantMap moon READ moon NOTIFY changed)

    Q_PROPERTY(QVariantMap pollen READ pollen NOTIFY changed)
    Q_PROPERTY(QVariantList activities READ activities NOTIFY changed)

    // Fixed, and fixed for the reason detaildata.js gave: the grid's order must
    // not depend on the order of a map's keys.
    Q_PROPERTY(QVariantList order READ order CONSTANT)

    // The published colour bands the scale cards ramp over. Not data and not
    // theme: they are the authority's own boundaries, normalised over each
    // card's range, and moving one changes what the card claims.
    Q_PROPERTY(QVariantMap bands READ bands CONSTANT)

public:
    // The parent has NO default value, and that is what makes create() run.
    //
    // QQmlPrivate::singletonConstructionMode() tests is_default_constructible
    // before it looks for a factory, so a QML_SINGLETON that can be
    // default-constructed is default-constructed and create() is never called.
    // app/appoptions.h has the same note and the same fix; the symptom here was
    // perfect — the type registered, every binding evaluated, and QML read a
    // second, empty snapshot while C++ pushed into the one AppEngine owns.
    // Nothing warned. The screen came up with `undefined` in every card.
    explicit ConditionsData(QObject *parent);

    static ConditionsData *create(QQmlEngine *qmlEngine, QJSEngine *jsEngine);

    void setSnapshot(const clima::Forecast &forecast, const clima::AirQuality &airQuality,
                     const QDateTime &now, const clima::Place &place, bool hasPollen);

    // For the one case where there is a place but no forecast yet — the
    // location bar has to say where it is pointing before the data arrives, or
    // the first frame has a hole in it where the name goes.
    void setPlace(const clima::Place &place);

    [[nodiscard]] QString     observedAt() const { return m_observedAt; }
    [[nodiscard]] QString     observedOn() const { return m_observedOn; }
    [[nodiscard]] QVariantMap location() const { return m_location; }
    [[nodiscard]] QVariantMap current() const { return m_current; }
    [[nodiscard]] int         nowIndex() const { return m_nowIndex; }

    [[nodiscard]] QVariantMap temperature() const { return m_temperature; }
    [[nodiscard]] QVariantMap feelsLike() const { return m_feelsLike; }
    [[nodiscard]] QVariantMap cloudCover() const { return m_cloudCover; }
    [[nodiscard]] QVariantMap precipitation() const { return m_precipitation; }
    [[nodiscard]] QVariantMap wind() const { return m_wind; }
    [[nodiscard]] QVariantMap humidity() const { return m_humidity; }
    [[nodiscard]] QVariantMap uv() const { return m_uv; }
    [[nodiscard]] QVariantMap airQuality() const { return m_airQuality; }
    [[nodiscard]] QVariantMap visibility() const { return m_visibility; }
    [[nodiscard]] QVariantMap pressure() const { return m_pressure; }
    [[nodiscard]] QVariantMap sun() const { return m_sun; }
    [[nodiscard]] QVariantMap moon() const { return m_moon; }

    [[nodiscard]] QVariantMap  pollen() const { return m_pollen; }
    [[nodiscard]] QVariantList activities() const { return m_activities; }

    [[nodiscard]] QVariantList order() const;
    [[nodiscard]] QVariantMap  bands() const;

Q_SIGNALS:
    void changed();

private:
    void clear();
    void buildContext();
    void buildTemperature();
    void buildFeelsLike();
    void buildCloud();
    void buildPrecipitation();
    void buildWind();
    void buildHumidity();
    void buildUv();
    void buildAirQuality();
    void buildVisibility();
    void buildPressure();
    void buildSunMoon();
    void buildPollen(bool hasPollen);
    void buildActivities();
    void buildSummary();

    // The twelve-hour window every sparkline draws, as indices into m_hours.
    [[nodiscard]] QVariantList window(clima::Reading clima::HourlyPoint::*field,
                                      int quantity) const;

    // Today's daily row, or a default-constructed one.
    [[nodiscard]] clima::DailyPoint today() const;

    // What "now" reads, which is not always the provider's `current` block.
    //
    // A live response's `current` is stamped within a few minutes of the
    // request and is the better reading — it is a nowcast at fifteen-minute
    // resolution, not an hourly average. A RECORDED response's `current` is
    // stamped at the moment of recording, which is a different instant from the
    // one the fixture's clock is frozen at, and using it puts a hero reading
    // eighteen hours out of step with the "Now" column three inches below it.
    //
    // So: the provider's block when it is within an hour of the clock, and the
    // hour we are standing in otherwise. One rule, no fixture branch, and the
    // live path is unchanged because live `current` is always within the hour.
    clima::CurrentConditions m_observation;

    clima::Forecast   m_forecast;
    clima::AirQuality m_air;
    clima::Place      m_place;
    QTimeZone         m_zone = QTimeZone::UTC;
    QDateTime         m_now;

    QList<clima::HourlyPoint> m_hours;      // hour-starting, like ForecastData's
    int                       m_hourNow  = 0; // index of the hour containing now
    int                       m_from     = 0; // first index of the twelve-hour window
    int                       m_nowIndex = 6;
    bool                      m_hasPollen = false;

    QString     m_observedAt;
    QString     m_observedOn;
    QVariantMap m_location;
    QVariantMap m_current;

    QVariantMap m_temperature, m_feelsLike, m_cloudCover, m_precipitation, m_wind;
    QVariantMap m_humidity, m_uv, m_airQuality, m_visibility, m_pressure, m_sun, m_moon;

    QVariantMap  m_pollen;
    QVariantList m_activities;
};
