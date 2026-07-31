// SPDX-FileCopyrightText: 2026 Jowi Aoun
// SPDX-License-Identifier: MPL-2.0
//
// Which provider answers, in what order, and what happens when the first one
// does not.
//
// docs/04-architecture.md §4.4 sketches this as a function:
//
//     resolve(coord, kind) -> [primary, fallback…]
//       forecast : open-meteo → met.no
//       alerts   : if US → nws ; else if EU/UK/IL → meteoalarm ;
//                  else if CA → eccc ; else ∅
//       radar    : if US → iem ; else if CA → eccc ; else librewxr ; else ∅
//
// ============================================================================
// THE ROUTING TABLE IS NOT A SWITCH STATEMENT
//
// Written literally, that sketch is a chain of `if` on region, per product,
// naming providers — which puts every provider's name in one central file and
// makes adding a regional source an edit to shared code. §4.1's second design
// principle is that providers are pluggable; a registry that has to be taught
// about each one is not a plug.
//
// So the same routing falls out of two things each provider already has to
// answer for itself:
//
//     covers(coord)   NWS says false outside the United States, ECCC says false
//                     outside Canada, Open-Meteo and MET Norway say true
//                     everywhere. A provider that does not cover a place is not
//                     in that place's chain — it is not tried and failed, it is
//                     absent.
//
//     priority        a number, given at registration. Lower goes first. A
//                     national service outranks a global one *where it applies*,
//                     which is exactly what the sketch's `if US → nws` means,
//                     without anybody writing "US" here.
//
// The `∅` case falls out too: a place where no provider covers the product has
// an empty chain, capabilitiesAt() reports nothing, and §4.4's rule — "a
// provider that returns ∅ must make the UI *hide* the feature, not show a
// broken one" — is the natural consequence rather than a thing to remember.
//
// Region() below exists for the providers to use in their own covers(), and for
// nothing else. It is a shared table of bounding boxes, not a router.
//
// ============================================================================
// ATTRIBUTION IS ENFORCED HERE, AT REGISTRATION
//
// docs/08-risks.md R12 is "a new provider gets added without its credit", and
// its mitigation is that the About screen is generated from this registry.
// Generation alone is not enough: a provider whose attribution() returns an
// empty struct generates an empty row.
//
// So add() returns a Status and REFUSES an incomplete Attribution, naming the
// missing field. The provider is not registered; not registered means never in
// a chain; never in a chain means its data cannot reach a screen. An
// uncredited provider does not render uncredited — it does not render.
//
// That is a deliberately harsh failure and it is the right one. The alternative
// failure is a licence breach that looks like a layout bug, discovered by the
// licensor.
//
// ============================================================================
// THE FALLBACK LOOP, AND WHICH ERROR SURVIVES IT
//
// fetchForecast() walks the chain: ask the first, and on a typed failure ask
// the next. Everything falls through except Cancelled, because a cancellation
// is the *caller* changing its mind and asking somebody else on its behalf
// would be wrong.
//
// A disabled provider costs nothing to skip. HttpClient answers a request for a
// provider it has disabled with an already-finished future carrying
// ErrorKind::ProviderDisabled — no socket, no event loop turn — so a chain
// whose primary has been 403'd falls through at roughly the speed of a function
// call. That is why the hard stop in HttpClient and the chain here are the same
// design and not two.
//
// When every provider fails, the error reported is the FIRST one — the
// primary's — with the rest appended to its message. The primary's failure is
// the news; the fallback failing too is corroboration. It also means a
// UserAgentRejected from the primary, which error.h says "should surface to a
// human" because it means our code is wrong, cannot be buried under a network
// timeout from a provider nobody was thinking about.
//
// Every attempt is recorded in the Answer either way, so a diagnostics panel
// can show the whole walk and the UI can say which source it is showing.

#pragma once

#include "libclima/core/result.h"
#include "libclima/providers/iforecastprovider.h"

#include <QList>
#include <QObject>
#include <QString>

namespace clima {

// ---- regions -------------------------------------------------------------------
//
// For a provider's own covers(), and for the alert and radar routing of §4.4
// when those providers land. Bounding boxes, which are wrong at every coastline
// and are the right tool anyway: the question they answer is "could this
// national service plausibly have data here", and the authoritative answer to
// that comes from the service, as a 404.
//
// Not used for pollen. That gate is derived from the payload — see
// libclima/providers/airquality/openmeteoairqualityprovider.h, which argues at
// length why a bounding box is the wrong instrument for a question the response
// already answers exactly.
enum class Region {
    Other,
    UnitedStates,   // CONUS, Alaska, Hawaii and the territories, loosely
    Canada,
    Europe,         // including the UK and Israel, which is MeteoAlarm's extent
};

// ---- there is no regionFor(), and that is the interesting part --------------
//
// The obvious signature is `Region regionFor(Coordinate)` — one place, one
// region — and it is wrong. Toronto is at 43.70 N, 79.42 W, which is inside the
// contiguous-United-States box AND inside the Canadian one, because the border
// between them runs 8,891 km through two Great Lakes and a river and no
// rectangle follows it. A single-winner function has to pick, and every rule it
// could pick by is wrong somewhere:
//
//     declaration order   makes Toronto American
//     smaller box wins    makes Seattle Canadian
//     centroid distance   makes Detroit and Windsor swap depending on rounding
//
// The question a provider actually needs answered is not "which country is
// this" — a bounding box cannot answer that and should not pretend to. It is
// "could this national service plausibly have data here", which is a per-region
// yes or no, and to which "both" is a legitimate answer. The service settles it
// authoritatively, with a 404, and the registry orders the attempts by priority.
//
// So: regionContains() for a provider's own covers(), and regionsFor() when
// something wants the whole plausible set. Neither of them can be wrong,
// because neither of them claims to know where the border is.
bool          regionContains(Region region, Coordinate coord);
QList<Region> regionsFor(Coordinate coord);
QString       regionName(Region region);

// ---- what a chain produced -------------------------------------------------------

struct ProviderFailure {
    QString providerId;
    Error   error;
};

// A value, plus the story of how it was obtained. The story is not diagnostics
// decoration: `servedBy` is what the UI shows when the fallback answered, and
// docs/06-roadmap.md's whole argument for building the fallback early is that a
// path nobody can see is a path nobody tests.
template <typename T>
struct Answer {
    T value;

    // The id of the provider that answered. Also present inside `value`; here
    // as well because a caller checking which source it got should not have to
    // know that the domain type happens to carry it.
    QString servedBy;

    // True when the provider that answered was not the first in the chain.
    // This is the flag behind a "showing MET Norway — Open-Meteo is
    // unavailable" line.
    bool fromFallback = false;

    // Every provider that was tried and did not serve, in the order tried.
    // Empty on the common path.
    QList<ProviderFailure> failures;
};

using ForecastAnswer   = Answer<Forecast>;
using AirQualityAnswer = Answer<AirQuality>;

// ---- the registry ------------------------------------------------------------------

class ProviderRegistry : public QObject
{
    Q_OBJECT

public:
    explicit ProviderRegistry(QObject *parent = nullptr);
    ~ProviderRegistry() override;

    // Providers are not owned. They outlive the registry or they are removed
    // from it; there is no shared ownership, because the thing that owns a
    // provider is the thing that gave it an HttpClient and a Clock, and
    // splitting that decision across two objects is how a dangling Clock
    // happens.
    //
    // `priority` orders the chain, lower first. The convention is 0 for a
    // national service, 100 for the global primary and 200 for a global
    // fallback, leaving room to slot one in without renumbering.
    //
    // Returns an error when the Attribution is incomplete — see the header —
    // and when a provider with the same id is already registered for the same
    // product, which is otherwise a silent double entry on the About screen.
    Status addForecastProvider(IForecastProvider *provider, int priority);
    Status addAirQualityProvider(IAirQualityProvider *provider, int priority);

    // The ordered chain for a place: covered providers, lowest priority first,
    // ties broken by registration order so that the answer is deterministic.
    [[nodiscard]] QList<IForecastProvider *>   forecastChain(Coordinate coord) const;
    [[nodiscard]] QList<IAirQualityProvider *> airQualityChain(Coordinate coord) const;

    // What the app can show here, for the tab bar.
    //
    // The SERVING provider's capabilities — the first in the chain — and
    // deliberately not the union across it. The union would be a promise the
    // app cannot keep: MET Norway has no UV index, so a UV tab built from a
    // union would be present while Open-Meteo is healthy and empty the moment
    // the fallback took over, which is a tab that breaks exactly when
    // everything else is already going wrong.
    //
    // An empty chain reports nothing available and nothing undetermined, which
    // §4.4 says must make the UI hide the feature.
    [[nodiscard]] Capabilities forecastCapabilitiesAt(Coordinate coord) const;
    [[nodiscard]] Capabilities airQualityCapabilitiesAt(Coordinate coord) const;

    // Every registered provider, deduplicated by id, in registration order.
    // This is what the About → Data sources screen is generated from — one
    // provider, one credit, no list maintained anywhere else. R12.
    [[nodiscard]] QList<const IProvider *> providers() const;
    [[nodiscard]] QList<Attribution>       attributions() const;

    // Try the chain. See the header for which errors fall through and which
    // error survives when none of them serve.
    QFuture<Result<ForecastAnswer>>   fetchForecast(const ForecastRequest &request);
    QFuture<Result<AirQualityAnswer>> fetchAirQuality(const ForecastRequest &request);

Q_SIGNALS:
    // Emitted when a request was served by something other than the first
    // provider in its chain. For the status line, and for a log that makes an
    // outage visible without a packet capture.
    void servedByFallback(const QString &productKind, const QString &providerId,
                          const QString &primaryProviderId);

private:
    template <typename ProviderT>
    struct Entry {
        ProviderT *provider = nullptr;
        int        priority = 0;
        int        sequence = 0;   // registration order, for a stable tie-break
    };

    // A member rather than a free function in the .cpp, because Entry is
    // private and a helper that has to be told about it is a helper that is
    // part of this class.
    template <typename ProviderT>
    static QList<ProviderT *> orderedChain(const QList<Entry<ProviderT>> &entries,
                                           Coordinate                    coord);

    Status validate(const IProvider *provider) const;

    QList<Entry<IForecastProvider>>   m_forecast;
    QList<Entry<IAirQualityProvider>> m_airQuality;

    int m_sequence = 0;
};

} // namespace clima
