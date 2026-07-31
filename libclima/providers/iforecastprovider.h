// SPDX-FileCopyrightText: 2026 Jowi Aoun
// SPDX-License-Identifier: MPL-2.0
//
// What a provider is, and the two things it must be able to say about itself
// before it is allowed to answer anything.
//
// This is the sketch in docs/04-architecture.md §4.4, built, with one
// correction that the rest of this comment is about.
//
// ============================================================================
// THE CORRECTION: CAPABILITIES ARE PER (PROVIDER, LOCATION), NOT PER PROVIDER
//
// §4.4 sketches `Capabilities capabilities() const`. That signature cannot
// express the thing the UI actually needs to know, and app/qml/Clima/metrics.js
// already wrote down what that is, in its own header, before any of this
// existed:
//
//     "In libclima this becomes a C++ registry populated from provider
//      capabilities, so a tab only appears when the active provider actually
//      has that variable FOR THAT LOCATION (Open-Meteo has no 15-minute data
//      outside Central Europe and North America, for instance)."
//
// Pollen is the case that proves it. Open-Meteo has a pollen product. It is
// the same endpoint, the same parameters, the same account — none — and in
// Toronto every one of the six species is null for every hour, because CAMS
// produces pollen for its European domain and nowhere else. A per-provider
// `capabilities()` has exactly two things it can say about that, and both are
// wrong: claim pollen and draw an empty card in Toronto, or disclaim it and
// hide a card that works perfectly in Berlin.
//
// So the signature takes a coordinate. Everything else follows from that.
//
// ---- three-valued, because the honest answer is sometimes "not yet" ---------
//
// A coordinate alone is not always enough either. Whether Open-Meteo has pollen
// at 43.70,-79.42 is a fact about the CAMS Europe domain, and the domain's
// boundary is not something this codebase should be holding a copy of — see
// libclima/providers/airquality/openmeteoairqualityprovider.h, which argues at
// length that the response is a more trustworthy witness than a bounding box we
// typed in.
//
// Which means that before the first fetch, the truthful answer is "I do not
// know yet". Not "no" — a UI that renders "no" hides the pollen card in Berlin
// for the two seconds before the payload lands, and then pops it in, which
// looks like a bug and is one. Not "yes" either, for the same reason in
// reverse.
//
// Hence Capabilities holds two sets:
//
//     available     known to work here. Draw it.
//     undetermined  unknown until something is fetched. Draw a placeholder, or
//                   draw nothing, but do not draw an empty card and do not
//                   flicker.
//
// and everything in neither set is known-absent. After one successful fetch the
// undetermined set for that place is empty and stays empty, because the verdict
// is remembered — again, see the air-quality provider for what is remembered
// and at what resolution.
//
// ============================================================================
// ATTRIBUTION IS A PURE VIRTUAL, AND THE REGISTRY REFUSES AN INVALID ONE
//
// docs/08-risks.md R12: "Attribution drift — a new provider gets added without
// its credit", mitigated by "`Attribution` is a required member of every
// provider interface; the About screen is generated from the registry, so it
// cannot go stale."
//
// A pure virtual gets half of that. It forces an author to type something; it
// does not stop them typing `return {};` to make the compiler quiet, and the
// About screen then renders a provider with no credit line, which is a licence
// breach that looks like a layout bug.
//
// So there are two gates. `attribution()` is pure virtual — you cannot forget
// it — and ProviderRegistry::add() *rejects* a provider whose Attribution is
// not complete, at the moment it is registered, with an error naming the
// missing field. An uncredited provider is not "added but not shown"; it is not
// added, which means its data never reaches a screen either. That is the only
// version of this rule that cannot be worked around by accident.
//
// ============================================================================
// EVERY FETCH IS EITHER A VALUE OR A TYPED ERROR
//
// §4.4 again: "Never returns a partial success silently." A provider that got
// half a payload returns Error(Parse), not a Forecast with three hours in it.
// The chain in libclima/providers/registry.h branches on ErrorKind to decide
// whether to fall through to the next provider, and it cannot branch on a
// success that is secretly a failure.

#pragma once

#include "libclima/core/result.h"
#include "libclima/domain/airquality.h"
#include "libclima/domain/coordinate.h"
#include "libclima/domain/forecast.h"

#include <QFlags>
#include <QFuture>
#include <QString>
#include <QStringList>
#include <QTimeZone>
#include <QUrl>

namespace clima {

// ---- attribution -------------------------------------------------------------

// Everything docs/02-data-sources.md §2.9 obliges us to display for one source.
//
// A struct rather than a formatted string, because §2.9's obligations differ in
// shape — Open-Meteo wants a credit line plus the names of the model owners
// behind it, ECCC wants one exact sentence, MET Norway wants a credit plus the
// User-Agent we identify ourselves with — and a screen that has to render all
// of them needs the parts, not one provider's idea of a paragraph.
struct Attribution {
    // "Open-Meteo", "MET Norway". The source, not the model behind it.
    QString name;

    // The exact sentence the licence asks for: "Weather data by Open-Meteo.com".
    // Not generated from `name`; ECCC's required wording is a sentence nobody
    // would guess, and getting it exactly right is the obligation.
    QString creditLine;

    // Where a user goes to check what we said about them.
    QUrl homepage;

    // "CC-BY 4.0", and the text of it. Both, because a credit line without a
    // licence identifier does not tell a user what they may do with what they
    // are looking at.
    QString licenceName;
    QUrl    licenceUrl;

    // The model owners named behind an aggregator — "ECMWF", "NOAA", "DWD",
    // "Météo-France". §2.9 requires these for Open-Meteo specifically, and an
    // empty list is legitimate for a source that is its own model.
    QStringList upstream;

    // Anything else a human maintaining this needs to know, shown in the About
    // screen's small print. MET Norway's identifying-User-Agent requirement
    // lives here.
    QString note;

    // What the registry checks. Deliberately not "is any field set": the four
    // that are required are the four that are legally required, and a provider
    // that supplied three of them fails with the name of the fourth.
    [[nodiscard]] bool    isComplete() const;
    [[nodiscard]] QString firstMissingField() const;
};

// ---- capabilities ------------------------------------------------------------

// What a provider can supply. One flag per thing the UI decides to draw or not
// draw, which is why the list is finer-grained than a data model would need:
// the tab bar in app/qml/Clima/metrics.js is built from these, and so is every
// row of the detail grid in detaildata.js.
//
// The rule for adding a flag is the same as for adding an ErrorKind: somebody
// has to be able to name the different thing the UI would *do*. Two flags that
// always hide or show the same pixels are one flag.
//
// ---- thirty-two bits, and no gaps between the groups ------------------------
//
// The underlying type is quint32 and the values are contiguous, which looks
// like premature tidiness and is not. QFlags only grew support for enums with a
// 64-bit underlying type in Qt 6.9, and D2's floor is 6.8 — so a flag at bit 32
// compiles on the developer's Qt and silently truncates on the one the LTS
// floor promises. There are four bits spare below; when they run out the answer
// is a second flag set for a second product family, not a wider integer.
enum class Capability : quint32 {
    None = 0,

    // Shapes of answer.
    CurrentConditions = 1U << 0,
    Hourly            = 1U << 1,
    Daily             = 1U << 2,
    Minutely15        = 1U << 3,   // the next-hour precipitation ribbon
    Ensemble          = 1U << 4,   // percentile fan, spaghetti members
    ModelSelection    = 1U << 5,   // the model-comparison view
    HistoricalArchive = 1U << 6,

    // Variables. One per metrics.js tab or detaildata.js row that can be
    // absent, because "which tabs exist here" is the question this enum was
    // built to answer.
    Temperature              = 1U << 7,
    ApparentTemperature      = 1U << 8,
    DewPoint                 = 1U << 9,
    Humidity                 = 1U << 10,
    Precipitation            = 1U << 11,
    PrecipitationType        = 1U << 12,   // the rain / showers / snow split
    PrecipitationProbability = 1U << 13,
    Wind                     = 1U << 14,
    WindGust                 = 1U << 15,
    Pressure                 = 1U << 16,
    CloudCover               = 1U << 17,
    Visibility               = 1U << 18,
    UvIndex                  = 1U << 19,
    WeatherCode              = 1U << 20,
    SunTimes                 = 1U << 21,

    // Air quality. Four flags and not one, because they are gated
    // independently: the indices and the pollutants are global, ammonia and
    // pollen are the CAMS Europe domain only, and Toronto gets the first two
    // without the last two.
    AirQualityIndex = 1U << 22,
    Pollutants      = 1U << 23,
    Ammonia         = 1U << 24,
    Pollen          = 1U << 25,

    // Products that arrive in later milestones. Declared now so that the
    // registry's routing table can be written against a stable enum rather than
    // grown a value at a time — an enum that changes shape every milestone is
    // an enum every persisted capability set has to be migrated against.
    Alerts = 1U << 26,
    Radar  = 1U << 27,
};
Q_DECLARE_FLAGS(CapabilityFlags, Capability)
Q_DECLARE_OPERATORS_FOR_FLAGS(CapabilityFlags)

// A three-valued answer: available, undetermined, or (by omission from both)
// known-absent. The header explains why two sets rather than one.
class Capabilities
{
public:
    Capabilities() = default;
    explicit Capabilities(CapabilityFlags available, CapabilityFlags undetermined = {});

    // Draw it. True only for a capability known to work at the coordinate that
    // was asked about.
    [[nodiscard]] bool has(Capability capability) const;

    // Do not draw an empty version of it, and do not decide yet. A UI showing a
    // tab bar before the first payload lands should reserve the space or leave
    // it out — but must not draw the card, because there is nothing in it.
    [[nodiscard]] bool isUndetermined(Capability capability) const;

    // The provider has this product and does not have it here. Hide it, and
    // keep hiding it: this answer does not change without moving.
    [[nodiscard]] bool isKnownAbsent(Capability capability) const;

    [[nodiscard]] CapabilityFlags available() const { return m_available; }
    [[nodiscard]] CapabilityFlags undetermined() const { return m_undetermined; }

    // For the diagnostics panel and for QCOMPARE failure output: a sorted,
    // comma-separated list of the flag names that are set.
    [[nodiscard]] QString toString() const;

    bool operator==(const Capabilities &other) const;
    bool operator!=(const Capabilities &other) const { return !(*this == other); }

private:
    CapabilityFlags m_available;

    // Invariant: disjoint from m_available. A capability cannot be both known
    // to work and unknown, and the constructor enforces it by clearing the
    // overlap rather than by asserting — a provider that says both means the
    // first, and crashing the app over it helps nobody.
    CapabilityFlags m_undetermined;
};

QString capabilityName(Capability capability);

// ---- what a caller asks for ---------------------------------------------------

enum class Resolution {
    Hourly,
    Minutely15,
};

struct ForecastRequest {
    Coordinate coord;

    // Days ahead. Open-Meteo serves 16, MET Norway about 9.5 — a provider
    // clamps rather than failing, because a fallback that refused a request the
    // primary would have accepted is not a fallback.
    int days = 10;

    Resolution resolution = Resolution::Hourly;

    // Empty means the provider's own default or blend. Only meaningful where
    // Capability::ModelSelection is available.
    QStringList models;

    bool wantEnsemble = false;

    // Answer from the cache or not at all. No socket is opened; an absent entry
    // is ErrorKind::NotFound, and a stale one is served *as an answer* carrying
    // the timestamp it was fetched at.
    //
    // This is step 1 of docs/04-architecture.md §4.1's first principle — "the UI
    // renders from cache, then reconciles" — and it is a request flag rather
    // than a separate method because the two steps have to go through the same
    // chain: a cached read that skipped the registry would not know which
    // provider last served this place, and would draw MET Norway's forecast
    // labelled Open-Meteo the morning after an outage.
    //
    // A provider with no cache answers NotFound, which is correct: it has
    // nothing, and saying so lets the caller go straight to step 2.
    bool cachedOnly = false;

    // The zone the daily rollup is grouped by, and the zone the UI formats in.
    //
    // Not an afterthought: Open-Meteo resolves it from the coordinate with
    // `timezone=auto` and reports it back, and MET Norway does not report one at
    // all — its timestamps are UTC and it has no opinion about local midnight.
    // Which means a MET Norway daily series has to be grouped by a zone
    // somebody supplied, and the somebody is the app, which knows the location's
    // zone from the search result that created it.
    //
    // Invalid means UTC, and a provider that had to fall back on that says so by
    // not advertising Capability::Daily — a ten-day view whose days start at the
    // wrong midnight is worse than no ten-day view.
    QTimeZone timeZone;
};

// ---- the interfaces -----------------------------------------------------------

// Common to every provider, whatever product it serves. Split out so that the
// registry can hold a heterogeneous set of them and still ask every one for its
// credit line — which is what makes the About screen generated rather than
// maintained.
class IProvider
{
public:
    virtual ~IProvider();

    // Stable, lowercase, hyphenated: "open-meteo", "met-no". This is the string
    // a 403 disables in HttpClient, the string in a cache key, and the string
    // the About screen sorts by. It never changes; the display name is what
    // changes.
    [[nodiscard]] virtual QString id() const = 0;

    // For humans. Translated by the app, not here.
    [[nodiscard]] virtual QString displayName() const = 0;

    // Required. See the header: the registry refuses an incomplete one.
    [[nodiscard]] virtual Attribution attribution() const = 0;

    // Whether this provider serves this place at all. A regional provider — NWS
    // in the United States, ECCC in Canada — answers false elsewhere and is
    // left out of the chain rather than tried and failed.
    [[nodiscard]] virtual bool covers(Coordinate coord) const = 0;

    // What it can supply HERE. The correction this file exists for.
    //
    // Const and cheap: this is called while building a tab bar, possibly per
    // frame during a location switch, and it must not touch the network. A
    // provider that learns a capability from a payload remembers the verdict
    // and answers from memory; before it has learned, it answers
    // "undetermined".
    [[nodiscard]] virtual Capabilities capabilitiesAt(Coordinate coord) const = 0;
};

class IForecastProvider : public IProvider
{
public:
    ~IForecastProvider() override;

    // Either a Forecast or a typed Error. Never a partial success — see the
    // header — because ProviderRegistry branches on the error's kind to decide
    // whether the next provider in the chain should be tried.
    virtual QFuture<Result<Forecast>> fetchForecast(const ForecastRequest &request) = 0;
};

class IAirQualityProvider : public IProvider
{
public:
    ~IAirQualityProvider() override;

    virtual QFuture<Result<AirQuality>> fetchAirQuality(const ForecastRequest &request) = 0;
};

} // namespace clima
