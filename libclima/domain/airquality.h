// SPDX-FileCopyrightText: 2026 Jowi Aoun
// SPDX-License-Identifier: MPL-2.0
//
// Air quality: two indices, six pollutants, six pollens, and one honest answer
// about which of them exist where you are standing.
//
// ============================================================================
// WHY THERE ARE TWO INDICES AND NOT ONE
//
// Because they are not two spellings of the same number. The US AQI runs 0-500
// over rolling averages and is dominated by PM2.5; the European AQI runs 0-100+
// and is dominated, most days, by ozone. The same air is 29 on one scale and 14
// on the other — those are the readings the Toronto fixture in
// tests/fixtures/airquality/ actually carries, at the same hour, for the same
// air.
//
// Open-Meteo returns both, everywhere, with no key. Verified: `european_aqi`
// and `us_aqi` are both non-null in Toronto, so the Air Quality tab is a global
// feature rather than a European one. That is worth stating because the pollen
// series in the same response are not, and the difference between the two is
// the subject of the rest of this file.
//
// ============================================================================
// THE DOMINANT POLLUTANT IS THE ARGMAX OF THE NORMALISED SUB-INDICES
//
// app/qml/Clima/detaildata.js wants a pollutant name, a concentration and a
// unit: "Deteriorating, with PM2.5 the primary pollutant." Picking the largest
// *concentration* would answer that with carbon monoxide every single time — CO
// is measured in the hundreds of µg/m³ while SO2 is measured in ones, and
// 204 µg/m³ of CO is clean air while 204 µg/m³ of NO2 is not. Concentrations of
// different gases are not comparable numbers.
//
// What is comparable is each pollutant's own sub-index: its concentration run
// through its own breakpoint table onto the shared 0-100+ European scale. The
// EAQI is *defined* as the maximum of those sub-indices, so the argmax is not
// an approximation of the dominant pollutant — it is the pollutant the
// published index is currently reporting. Verified on the recorded fixtures:
// over 144 hours across two continents, `european_aqi` equals the maximum of
// the five published sub-indices exactly, every hour, with no rounding slack.
//
// ---- and why we ask for the sub-indices instead of computing them -----------
//
// Because computing them from the hourly concentrations gives a different
// answer, and the difference is not small. Measured against the same fixtures:
//
//     pollutant          our hourly computation vs. the published sub-index
//     -----------------  ------------------------------------------------
//     ozone              max error 0.45   (a rounding step)
//     nitrogen dioxide   max error 0.50   (a rounding step)
//     sulphur dioxide    max error 0.50   (a rounding step)
//     PM10               max error 11.3
//     PM2.5              max error 22.4
//
// The three gases agree because the EAQI defines their sub-indices on hourly
// values, which is what a forecast response contains. The two particulates do
// not, because the EAQI defines *theirs* on a 24-hour running mean — and the
// first hour of a forecast has no preceding twenty-four hours to average. A
// 22-point error on PM2.5 is the difference between "good" and "moderate", and
// on a rising evening it is exactly the error that hands the argmax to the
// wrong pollutant.
//
// So `europeanSubIndices` carries what the provider published, and
// europeanSubIndex() below computes one locally only for a provider that
// published none. The rule is written into dominantPollutant(): published
// first, computed second, and never a mixture that would compare one of each.
//
// Carbon monoxide has no sub-index in either mode. The EAQI does not include
// CO, so it cannot be dominant — which is correct rather than a limitation: an
// index cannot be dominated by a pollutant it does not measure. Its
// concentration is still reported, because a detail card lists it.
//
// ============================================================================
// POLLEN IS EUROPE-ONLY, AND WE DO NOT PRETEND OTHERWISE
//
// CAMS produces the six pollen species for its European domain and for nowhere
// else; ammonia is the same. Open-Meteo answers a Toronto request for them with
// a well-formed array of nulls rather than with an error, which is the trap: a
// parser that reads null as 0.0 produces a pollen card saying "Grass: 0,
// Birch: 0 — Low" for a city that has no pollen product at all, and it is
// *plausible*, which is what makes it worse than a crash.
//
// So `pollen` is an optional whole. Absent means "this place has no pollen
// product", not "no pollen today", and there is no way to read a species out of
// an absent one. docs/08-risks.md R9 and docs/02-data-sources.md §2.6 both say
// region-gate it honestly; this type is what makes doing so the path of least
// resistance.

#pragma once

#include "libclima/domain/coordinate.h"
#include "libclima/domain/reading.h"

#include <QDateTime>
#include <QList>
#include <QMap>
#include <QString>
#include <QTimeZone>

#include <optional>

namespace clima {

// The pollutants Open-Meteo's air-quality product carries, in the order a
// detail card lists them. `Count` is not a pollutant; it is the size of the
// enum, and it exists so that a loop over all of them cannot fall out of step
// with the enum the day one is added.
enum class Pollutant {
    Pm2_5,
    Pm10,
    Ozone,
    NitrogenDioxide,
    SulphurDioxide,
    CarbonMonoxide,

    Count
};

// CAMS Europe's six species. Same ordering as the API's parameter list, so the
// series and the enum can be zipped without a lookup.
enum class PollenSpecies {
    Alder,
    Birch,
    Grass,
    Mugwort,
    Olive,
    Ragweed,

    Count
};

// Stable machine names — "pm2_5", "grass_pollen" — for cache keys, log lines
// and the property names a QML model exposes. These are also the Open-Meteo
// parameter names, deliberately: one string does for the outbound query, the
// key and the property, and three spellings of "pm2_5" is three places for a
// typo that reads as no data.
//
// Not user-facing. "PM2.5" with the right subscript, translated, is the app's
// job and goes through Qt Linguist.
QString pollutantId(Pollutant pollutant);
QString pollenSpeciesId(PollenSpecies species);

// The name of the provider-published European sub-index series for a
// pollutant: "european_aqi_pm2_5". Empty for a pollutant the EAQI does not
// define, which is how the query builder knows not to ask for one.
QString europeanSubIndexId(Pollutant pollutant);

// µg/m³ for every pollutant Open-Meteo reports, including CO — it publishes CO
// in µg/m³ where the WHO guideline values are quoted in mg/m³, and a card that
// printed the WHO's unit next to Open-Meteo's number would be wrong by a factor
// of a thousand in the reassuring direction.
QString pollutantUnit(Pollutant pollutant);

// A local computation of the sub-index, for a provider that publishes none.
//
// Read the header before using this. It is exact for ozone, nitrogen dioxide
// and sulphur dioxide, and it is materially wrong for PM2.5 and PM10 because
// the EAQI defines those on a 24-hour running mean that an hourly forecast
// series does not contain. Returns nullopt for carbon monoxide, which has no
// EAQI sub-index, and for a negative concentration, which is a broken payload
// rather than a small number.
std::optional<double> europeanSubIndex(Pollutant pollutant, double concentration);

// ---- one hour of air ---------------------------------------------------------

struct AirQualityPoint {
    QDateTime time;

    // 0-100+ and 0-500 respectively. Both are integers as published: the
    // indices are band memberships, and a European AQI of 51.33 is reported by
    // CAMS as 51.
    std::optional<int> europeanAqi;
    std::optional<int> usAqi;

    // Keyed rather than a fixed struct of six, because a pollutant that is null
    // for this hour must be *missing* from the map instead of present and zero.
    // The map's absence-is-absence is the same argument as Reading's.
    QMap<Pollutant, double> pollutants;

    // What the provider published for each pollutant on the European scale.
    // Empty for a provider that publishes no sub-indices, in which case
    // dominantPollutant() falls back to europeanSubIndex().
    QMap<Pollutant, double> europeanSubIndices;

    Reading dust;                  // µg/m³, Saharan and other mineral dust
    Reading aerosolOpticalDepth;   // dimensionless, 550 nm
    Reading ammonia;               // µg/m³ — Europe only, like pollen
    Reading uvIndex;

    // Absent outside the CAMS Europe domain. Absent means no product, not zero
    // pollen — see the header. A species inside a present map may still be
    // missing for an hour the model did not produce.
    std::optional<QMap<PollenSpecies, double>> pollen;

    // The argmax over the sub-indices, and the sub-index it won with. Absent
    // when no pollutant with a sub-index was reported.
    [[nodiscard]] std::optional<Pollutant> dominantPollutant() const;
    [[nodiscard]] std::optional<double>    dominantSubIndex() const;

    // The dominant pollutant's own concentration, in pollutantUnit() of it.
    // This is `airQuality.pollutantValue` in app/qml/Clima/detaildata.js — the
    // number a user reads — as distinct from the sub-index, which is the number
    // that decided *which* pollutant to name.
    [[nodiscard]] std::optional<double> dominantConcentration() const;
};

// ---- the whole answer --------------------------------------------------------

struct AirQuality {
    QString    providerId;
    Coordinate coordinate;
    QTimeZone  timeZone;

    QDateTime fetchedAt;

    AirQualityPoint        current;
    QList<AirQualityPoint> hourly;

    // True when at least one pollen sample in the whole response was non-null.
    // This is the Europe gate, computed from the payload rather than from a
    // bounding box — see openmeteoairqualityprovider.h for why the payload is
    // the more trustworthy source, and what is remembered so that the tab bar
    // can be built before the payload arrives.
    bool hasPollen = false;

    // Same test, same reason, different series. Ammonia is Europe-only too, and
    // discovering that from the data rather than from a second hardcoded box
    // means the two cannot disagree.
    bool hasAmmonia = false;

    [[nodiscard]] bool isEmpty() const { return hourly.isEmpty() && !current.time.isValid(); }
};

} // namespace clima
