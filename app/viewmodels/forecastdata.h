// SPDX-FileCopyrightText: 2026 Jowi Aoun
// SPDX-License-Identifier: GPL-3.0-or-later
//
// The hourly and daily series, in the shape thirteen QML files already read.
//
// ============================================================================
// THIS FILE IS mockdata.js, WITH A PROVIDER BEHIND IT
//
// app/qml/Clima/mockdata.js said so in its own header, before any of this
// existed:
//
//     "Shape of the API deliberately mirrors what libclima's ForecastProvider
//      will return: parallel per-hour arrays plus derived helpers, no
//      formatting decisions baked in."
//
// So this class keeps that shape exactly — the same property names, the same
// array layout, the same helper functions — and the QML that reads it did not
// have to change. `Data` is still `Data`; it is a C++ singleton in the Clima
// module rather than a JavaScript library, which is why the thirteen files that
// used to say `import "mockdata.js" as Data` now say nothing at all: a
// singleton in the same module is in scope without an import, the way `Theme`
// and `Viewports` already were.
//
// Two things did change and both are marked at their declarations:
// `labelIndices` and `precipBuckets` are properties rather than functions, so
// that a Repeater bound to one rebuilds when a refresh lands.
//
// ============================================================================
// THE WINDOW: FORTY-EIGHT HOURS, FIFTEEN OF THEM BEHIND
//
// A provider hands over several hundred hours. The chart draws forty-eight of
// them, starting fifteen before now — which is not a number chosen here. It is
// mockdata.js's, and mockdata.js explains it:
//
//     "Fifteen observed hours is a lot of past to carry, and it is the honest
//      amount — the series starts at 21:00 the evening before."
//
// The one difference is that `nowIndex` is computed rather than fixed at 15.
// It has to be: MET Norway's series begins at the current hour and has no past
// at all, and a window that insisted on fifteen observed hours would open on
// data that does not exist. So the window starts at whichever is later — the
// series' own first hour, or fifteen hours ago — and `nowIndex` says where
// "now" landed. `firstLabelIndex` follows from it, because "Now" has to be a
// labelled column or it is never drawn.
//
// ============================================================================
// UNITS: THE SERIES ARE CANONICAL EXCEPT WHERE THE READER SEES THEM
//
// Temperatures, wind speeds, pressures and visibilities are converted here, on
// the way out, through app/viewmodels/units.h — because a QML file that writes
// `Data.temperature[i] + "°"` is right in Celsius and in Fahrenheit, and one
// that writes `+ " km/h"` is not, which is why HourlyList asks Units instead.
//
// `precipMm` is the exception and stays in millimetres. It has to: precip.js
// classifies intensity against the NWS bands — 2.5 mm/h moderate, 7.6 mm/h
// heavy — and those are statements about millimetres. Converted to inches they
// would silently reclassify every rain band in the app to "light". The chart
// converts it at the plotting boundary instead (app/viewmodels/metrics.h), and
// this is the one series where those two are different places.

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

class ForecastData : public QObject
{
    Q_OBJECT
    QML_NAMED_ELEMENT(Data)
    QML_SINGLETON

    // ---- the window --------------------------------------------------------
    Q_PROPERTY(int count READ count NOTIFY changed)
    Q_PROPERTY(int nowIndex READ nowIndex NOTIFY changed)
    Q_PROPERTY(int startHour READ startHour NOTIFY changed)
    Q_PROPERTY(int firstLabelIndex READ firstLabelIndex NOTIFY changed)
    Q_PROPERTY(int labelStep READ labelStep NOTIFY changed)

    // ---- the series --------------------------------------------------------
    //
    // Parallel arrays, `count` long, indexed by hour. Named exactly as
    // metrics.js's `series` field names them, because that registry looks them
    // up by string — `Data[metric.series]` — and a rename here is a tab that
    // silently draws nothing.
    Q_PROPERTY(QVariantList temperature READ temperature NOTIFY changed)
    Q_PROPERTY(QVariantList apparent READ apparent NOTIFY changed)
    Q_PROPERTY(QVariantList precipProb READ precipProb NOTIFY changed)
    Q_PROPERTY(QVariantList precipMm READ precipMm NOTIFY changed)
    Q_PROPERTY(QVariantList cloud READ cloud NOTIFY changed)
    Q_PROPERTY(QVariantList humidity READ humidity NOTIFY changed)
    Q_PROPERTY(QVariantList windSpeed READ windSpeed NOTIFY changed)
    Q_PROPERTY(QVariantList windGust READ windGust NOTIFY changed)
    Q_PROPERTY(QVariantList windDirection READ windDirection NOTIFY changed)
    Q_PROPERTY(QVariantList pressure READ pressure NOTIFY changed)
    Q_PROPERTY(QVariantList uvIndex READ uvIndex NOTIFY changed)
    Q_PROPERTY(QVariantList visibility READ visibility NOTIFY changed)
    Q_PROPERTY(QVariantList airQuality READ airQuality NOTIFY changed)

    // Whether `apparent` carries anything at all. MET Norway has no apparent
    // temperature and Open-Meteo does, so this is the difference between a
    // "Feels like" toggle that shows a second curve and one that redraws the
    // first — see HourlyOverview, which hides the control rather than offering
    // a switch whose only effect is to claim something untrue.
    Q_PROPERTY(bool hasApparent READ hasApparent NOTIFY changed)

    // One of precip.js's six type names per hour, empty where the hour is dry.
    // The provider's WMO code decides, which is the whole reason it is here:
    // thunder and hail cannot be derived from an amount and a temperature, and
    // precip.js said so before there was a code to read.
    Q_PROPERTY(QVariantList precipTypes READ precipTypes NOTIFY changed)

    // ---- days --------------------------------------------------------------
    Q_PROPERTY(QVariantList days READ days NOTIFY changed)
    Q_PROPERTY(int todayIndex READ todayIndex NOTIFY changed)

    // ---- the calendar ------------------------------------------------------
    Q_PROPERTY(QVariantMap month READ month NOTIFY changed)
    Q_PROPERTY(QVariantList monthDays READ monthDays NOTIFY changed)
    Q_PROPERTY(QVariantList weekdayNames READ weekdayNames CONSTANT)

    // ---- the sky -----------------------------------------------------------
    Q_PROPERTY(QVariantList sunEvents READ sunEvents NOTIFY changed)
    Q_PROPERTY(QVariantMap moonPhase READ moonPhase NOTIFY changed)

    // ---- derived, and properties rather than functions ----------------------
    //
    // These two were `labelIndices()` and `precipBuckets()` in mockdata.js. A
    // Repeater whose model is a function call never re-runs it, because a
    // binding subscribes to the properties it reads and a method call is not
    // one — so a refresh would land in the arrays above and leave the header
    // band drawing the hours from before it. Properties, and the call sites
    // lost their parentheses.
    Q_PROPERTY(QVariantList labelIndices READ labelIndices NOTIFY changed)
    Q_PROPERTY(QVariantList precipBuckets READ precipBuckets NOTIFY changed)

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
    explicit ForecastData(QObject *parent);

    static ForecastData *create(QQmlEngine *qmlEngine, QJSEngine *jsEngine);

    // The traditional name for a phase, localised. libclima returns an
    // identifier — "waning-gibbous" — precisely so that the wording is the
    // app's; public and static here so that the Sun & Moon card and the chart
    // legend cannot end up with two spellings of the same moon.
    [[nodiscard]] static QString moonPhaseLabel(const QString &identifier);

    // Rebuilds everything from one snapshot. `now` is the clock's, never the
    // wall clock's, which is what makes the "Now" column, the past veil and the
    // sky phase land on the same hour in a fixture run as they did on the
    // afternoon it was recorded.
    void setSnapshot(const clima::Forecast &forecast, const clima::AirQuality &airQuality,
                     const QDateTime &now, const clima::Place &place);

    [[nodiscard]] int count() const { return m_count; }
    [[nodiscard]] int nowIndex() const { return m_nowIndex; }
    [[nodiscard]] int startHour() const { return m_startHour; }
    [[nodiscard]] int firstLabelIndex() const { return m_firstLabelIndex; }
    [[nodiscard]] int labelStep() const { return m_labelStep; }

    [[nodiscard]] QVariantList temperature() const { return m_temperature; }
    [[nodiscard]] QVariantList apparent() const { return m_apparent; }
    [[nodiscard]] QVariantList precipProb() const { return m_precipProb; }
    [[nodiscard]] QVariantList precipMm() const { return m_precipMm; }
    [[nodiscard]] QVariantList cloud() const { return m_cloud; }
    [[nodiscard]] QVariantList humidity() const { return m_humidity; }
    [[nodiscard]] QVariantList windSpeed() const { return m_windSpeed; }
    [[nodiscard]] QVariantList windGust() const { return m_windGust; }
    [[nodiscard]] QVariantList windDirection() const { return m_windDirection; }
    [[nodiscard]] QVariantList pressure() const { return m_pressure; }
    [[nodiscard]] QVariantList uvIndex() const { return m_uvIndex; }
    [[nodiscard]] QVariantList visibility() const { return m_visibility; }
    [[nodiscard]] QVariantList airQuality() const { return m_airQuality; }
    [[nodiscard]] QVariantList precipTypes() const { return m_precipTypes; }
    [[nodiscard]] bool         hasApparent() const { return m_hasApparent; }

    [[nodiscard]] QVariantList days() const { return m_days; }
    [[nodiscard]] int          todayIndex() const { return m_todayIndex; }

    [[nodiscard]] QVariantMap  month() const { return m_month; }
    [[nodiscard]] QVariantList monthDays() const { return m_monthDays; }
    [[nodiscard]] QVariantList weekdayNames() const;

    [[nodiscard]] QVariantList sunEvents() const { return m_sunEvents; }
    [[nodiscard]] QVariantMap  moonPhase() const { return m_moonPhase; }

    [[nodiscard]] QVariantList labelIndices() const { return m_labelIndices; }
    [[nodiscard]] QVariantList precipBuckets() const { return m_precipBuckets; }

    // ---- the helpers mockdata.js had ---------------------------------------
    //
    // Still functions, because they take an index and there is no property
    // shape for that. They are safe as functions because every one of them is
    // called inside a Repeater delegate whose model is one of the properties
    // above — so a refresh rebuilds the delegates and the calls happen again.
    [[nodiscard]] Q_INVOKABLE bool    isNight(int index) const;
    [[nodiscard]] Q_INVOKABLE QString conditionFor(int index) const;
    [[nodiscard]] Q_INVOKABLE QString conditionText(int index) const;
    [[nodiscard]] Q_INVOKABLE QString hourLabel(int index) const;
    [[nodiscard]] Q_INVOKABLE QString clockLabel(int index) const;

    // The entry in `days` for a calendar date, or an empty map. The calendar
    // screen's one lookup.
    [[nodiscard]] Q_INVOKABLE QVariantMap dayFor(int date, int month) const;
    [[nodiscard]] Q_INVOKABLE int         weekdayOf(int date) const;

Q_SIGNALS:
    // One signal for the whole snapshot. There is no cheaper granularity worth
    // having: a new forecast changes every series at once, and a per-property
    // signal set would be twenty ways to forget one.
    void changed();

private:
    void clear();
    void buildWindow(const QDateTime &now);
    void buildSeries();
    void buildDays(const QDateTime &now);
    void buildMonth(const QDateTime &now);
    void buildSunEvents();
    void buildBuckets();

    // The local time of the hour at `index`, or an invalid QDateTime.
    [[nodiscard]] QDateTime localTimeAt(int index) const;

    clima::Forecast   m_forecast;
    clima::AirQuality m_air;
    clima::Place      m_place;
    QTimeZone         m_zone = QTimeZone::UTC;
    QDateTime         m_now;

    // The hourly series after libclima/domain/hourconvention.h's shift, so
    // that every accumulated quantity in it describes the hour STARTING at its
    // timestamp — the convention precip.js draws on, and the one place in the
    // whole app where that shift happens.
    QList<clima::HourlyPoint> m_hours;

    int m_start           = 0;   // index into m_hours of column 0
    int m_count           = 0;
    int m_nowIndex        = 0;
    int m_startHour       = 0;
    int m_firstLabelIndex = 1;
    int m_labelStep       = 2;

    QVariantList m_temperature, m_apparent, m_precipProb, m_precipMm, m_cloud, m_humidity;
    QVariantList m_windSpeed, m_windGust, m_windDirection, m_pressure, m_uvIndex;
    QVariantList m_visibility, m_airQuality, m_precipTypes;
    bool         m_hasApparent = false;

    QVariantList m_days;
    int          m_todayIndex = 0;

    QVariantMap  m_month;
    QVariantList m_monthDays;

    QVariantList m_sunEvents;
    QVariantMap  m_moonPhase;

    QVariantList m_labelIndices;
    QVariantList m_precipBuckets;
};
