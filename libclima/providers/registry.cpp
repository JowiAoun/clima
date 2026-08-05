// SPDX-FileCopyrightText: 2026 Jowi Aoun
// SPDX-License-Identifier: MPL-2.0

#include "libclima/providers/registry.h"

#include <QPromise>

#include <algorithm>
#include <functional>
#include <memory>

namespace clima {

namespace {

struct Box {
    double south;
    double north;
    double west;
    double east;
};

// Loose, and loose on purpose. These decide whether a national service is worth
// *asking*, not whether it has data — the service answers that, with a 404, and
// a box tight enough to be a coastline would be a box that has to be maintained.
//
// The United States gets three: the contiguous states, Alaska, and a Pacific
// box wide enough for Hawaii and the territories. One box containing all three
// would contain most of the Pacific and half of Canada.
const QList<Box> &boxesFor(Region region)
{
    static const QList<Box> unitedStates = {
        { 24.0, 50.0, -125.0, -66.0 },     // contiguous
        { 51.0, 72.0, -170.0, -129.0 },    // Alaska
        { 13.0, 29.0, -179.0, -144.0 },    // Hawaii and the Pacific territories
    };
    static const QList<Box> canada = { { 41.0, 84.0, -142.0, -52.0 } };

    // MeteoAlarm's extent, which is what this box is for: continental Europe,
    // the UK, Iceland and Israel. It reaches further east and south than
    // "Europe" would to include Cyprus and Israel.
    static const QList<Box> europe = { { 29.0, 72.0, -25.0, 45.0 } };

    static const QList<Box> none;

    switch (region) {
    case Region::UnitedStates: return unitedStates;
    case Region::Canada:       return canada;
    case Region::Europe:       return europe;
    case Region::Other:        return none;
    }
    return none;
}

// The failure the whole chain reports, built from the walk. The first error is
// the one that survives — see registry.h — and the rest are appended so the
// message reads as a story rather than as one arbitrary sentence.
Error chainError(const QList<ProviderFailure> &failures, const QString &product)
{
    if (failures.isEmpty()) {
        return Error(ErrorKind::Unsupported,
                     QStringLiteral("no provider covers this location for %1").arg(product));
    }

    const Error &primary = failures.constFirst().error;

    QStringList tail;
    for (int i = 1; i < failures.size(); ++i) {
        tail.append(QStringLiteral("%1: %2")
                        .arg(failures.at(i).providerId, failures.at(i).error.toString()));
    }

    QString message = primary.message();
    if (!tail.isEmpty()) {
        message = QStringLiteral("%1 (fallbacks also failed — %2)")
                      .arg(message, tail.join(QStringLiteral("; ")));
    }

    // Rebuilt rather than copy-and-append, so that the kind, the provider id
    // and the HTTP status are carried over deliberately. An earlier version
    // copied the primary error, set its provider id, and then returned a *new*
    // error built from the kind and the message — dropping the id it had just
    // set. The kind survived, so nothing branched wrongly and no test that was
    // not asking about the id noticed.
    Error error(primary.kind(), message);
    error.setProviderId(failures.constFirst().providerId);
    error.setHttpStatus(primary.httpStatus());
    error.setRetryAfter(primary.retryAfter());
    return error;
}

// ---- the walk -------------------------------------------------------------------
//
// One shared implementation for both products, because the difference between
// them is the member function called and nothing else, and two copies of a
// retry loop is two places for the fall-through rule to drift.
//
// The state is a shared_ptr held by each continuation, which is what keeps it
// alive across the asynchronous hops without any of them owning it. The
// continuation's own future is stored in the state too: a QFuture returned by
// then() and immediately dropped is a continuation whose lifetime nobody is
// asserting anything about, and this code should not be the place that finds
// out whether it runs.
template <typename ProviderT, typename ValueT, typename FetchFn>
QFuture<Result<Answer<ValueT>>> walk(QObject *owner, QList<ProviderT *> chain,
                                     const ForecastRequest &request, const QString &product,
                                     FetchFn fetch,
                                     std::function<void(const QString &, const QString &)> onFallback)
{
    struct State {
        QPromise<Result<Answer<ValueT>>> promise;
        QList<ProviderT *>               chain;
        ForecastRequest                  request;
        QString                          product;
        QList<ProviderFailure>           failures;
        int                              index = 0;

        // Appended to rather than replaced. The step that starts attempt n+1
        // runs *inside* the continuation of attempt n, and overwriting the
        // handle there would destroy the QFuture whose continuation is on the
        // stack. Keeping all of them costs one handle per provider in a chain
        // that is two long.
        QList<QFuture<void>>             pending;
        std::function<void()>            step;
        std::function<void(const QString &, const QString &)> onFallback;
    };

    auto state       = std::make_shared<State>();
    state->chain     = std::move(chain);
    state->request   = request;
    state->product   = product;
    state->onFallback = std::move(onFallback);
    state->promise.start();

    QFuture<Result<Answer<ValueT>>> future = state->promise.future();

    state->step = [state, owner, fetch]() {
        if (state->index >= state->chain.size()) {
            state->promise.addResult(Result<Answer<ValueT>>(
                chainError(state->failures, state->product)));
            state->promise.finish();
            return;
        }

        ProviderT *provider = state->chain.at(state->index);

        state->pending.append(fetch(provider, state->request)
                                  .then(owner, [state, provider](const Result<ValueT> &result) {
            if (result.hasValue()) {
                Answer<ValueT> answer;
                answer.value        = result.value();
                answer.servedBy     = provider->id();
                answer.fromFallback = state->index > 0;
                answer.failures     = state->failures;

                if (answer.fromFallback && state->onFallback) {
                    state->onFallback(answer.servedBy,
                                      state->chain.constFirst()->id());
                }

                state->promise.addResult(Result<Answer<ValueT>>(answer));
                state->promise.finish();
                return;
            }

            const Error error = result.error();

            // Cancelled is the caller changing its mind. Asking somebody else
            // on their behalf would be answering a question that was withdrawn.
            if (error.kind() == ErrorKind::Cancelled) {
                state->promise.addResult(Result<Answer<ValueT>>(error));
                state->promise.finish();
                return;
            }

            state->failures.append({ provider->id(), error });
            ++state->index;
            state->step();
        }));
    };

    state->step();
    return future;
}

} // namespace

// ---- regions ----------------------------------------------------------------------

bool regionContains(Region region, Coordinate coord)
{
    if (!coord.isValid())
        return false;
    for (const Box &box : boxesFor(region)) {
        if (coord.latitude >= box.south && coord.latitude <= box.north
            && coord.longitude >= box.west && coord.longitude <= box.east)
            return true;
    }
    return false;
}

QList<Region> regionsFor(Coordinate coord)
{
    QList<Region> regions;
    for (const Region region : { Region::UnitedStates, Region::Canada, Region::Europe }) {
        if (regionContains(region, coord))
            regions.append(region);
    }
    return regions;
}

QString regionName(Region region)
{
    switch (region) {
    case Region::UnitedStates: return QStringLiteral("us");
    case Region::Canada:       return QStringLiteral("ca");
    case Region::Europe:       return QStringLiteral("eu");
    case Region::Other:        return QStringLiteral("other");
    }
    return QStringLiteral("other");
}

// ---- the registry --------------------------------------------------------------------

ProviderRegistry::ProviderRegistry(QObject *parent)
    : QObject(parent)
{
}

ProviderRegistry::~ProviderRegistry() = default;

Status ProviderRegistry::validate(const IProvider *provider) const
{
    if (provider == nullptr)
        return Error(ErrorKind::Unsupported, QStringLiteral("cannot register a null provider"));

    if (provider->id().trimmed().isEmpty()) {
        return Error(ErrorKind::Unsupported,
                     QStringLiteral("cannot register a provider with no id"));
    }

    // R12, enforced. The message names the field, because the author of a new
    // provider is the person reading it and "attribution is incomplete" would
    // send them to the interface rather than to their own file.
    const Attribution credit = provider->attribution();
    if (!credit.isComplete()) {
        return Error(ErrorKind::Unsupported,
                     QStringLiteral("provider '%1' has no %2 in its attribution; "
                                    "every source must carry its credit before it can be "
                                    "registered (docs/08-risks.md R12)")
                         .arg(provider->id(), credit.firstMissingField()));
    }

    return ok();
}

Status ProviderRegistry::addForecastProvider(IForecastProvider *provider, int priority)
{
    const Status valid = validate(provider);
    if (!valid)
        return valid;

    for (const Entry<IForecastProvider> &entry : std::as_const(m_forecast)) {
        if (entry.provider->id() == provider->id()) {
            return Error(ErrorKind::Unsupported,
                         QStringLiteral("a forecast provider with id '%1' is already registered")
                             .arg(provider->id()));
        }
    }

    m_forecast.append({ provider, priority, m_sequence++ });
    return ok();
}

Status ProviderRegistry::addAirQualityProvider(IAirQualityProvider *provider, int priority)
{
    const Status valid = validate(provider);
    if (!valid)
        return valid;

    for (const Entry<IAirQualityProvider> &entry : std::as_const(m_airQuality)) {
        if (entry.provider->id() == provider->id()) {
            return Error(
                ErrorKind::Unsupported,
                QStringLiteral("an air-quality provider with id '%1' is already registered")
                    .arg(provider->id()));
        }
    }

    m_airQuality.append({ provider, priority, m_sequence++ });
    return ok();
}

Status ProviderRegistry::addAlertProvider(IAlertProvider *provider, int priority)
{
    const Status valid = validate(provider);
    if (!valid)
        return valid;

    for (const Entry<IAlertProvider> &entry : std::as_const(m_alert)) {
        if (entry.provider->id() == provider->id()) {
            return Error(ErrorKind::Unsupported,
                         QStringLiteral("an alert provider with id '%1' is already registered")
                             .arg(provider->id()));
        }
    }

    m_alert.append({ provider, priority, m_sequence++ });
    return ok();
}

// Covered providers, lowest priority first, registration order breaking ties.
// A stable sort rather than a comparator that reaches for the sequence number,
// so the tie-break is a property of the algorithm rather than a thing the
// comparator has to remember.
template <typename ProviderT>
QList<ProviderT *> ProviderRegistry::orderedChain(const QList<Entry<ProviderT>> &entries,
                                                  Coordinate                     coord)
{
    QList<Entry<ProviderT>> covered;
    for (const Entry<ProviderT> &entry : entries) {
        if (entry.provider != nullptr && entry.provider->covers(coord))
            covered.append(entry);
    }

    std::stable_sort(covered.begin(), covered.end(),
                     [](const Entry<ProviderT> &a, const Entry<ProviderT> &b) {
                         return a.priority < b.priority;
                     });

    QList<ProviderT *> chain;
    chain.reserve(covered.size());
    for (const Entry<ProviderT> &entry : covered)
        chain.append(entry.provider);
    return chain;
}

QList<IForecastProvider *> ProviderRegistry::forecastChain(Coordinate coord) const
{
    return orderedChain<IForecastProvider>(m_forecast, coord);
}

QList<IAirQualityProvider *> ProviderRegistry::airQualityChain(Coordinate coord) const
{
    return orderedChain<IAirQualityProvider>(m_airQuality, coord);
}

Capabilities ProviderRegistry::forecastCapabilitiesAt(Coordinate coord) const
{
    const QList<IForecastProvider *> chain = forecastChain(coord);
    if (chain.isEmpty())
        return {};
    return chain.constFirst()->capabilitiesAt(coord);
}

Capabilities ProviderRegistry::airQualityCapabilitiesAt(Coordinate coord) const
{
    const QList<IAirQualityProvider *> chain = airQualityChain(coord);
    if (chain.isEmpty())
        return {};
    return chain.constFirst()->capabilitiesAt(coord);
}

QList<IAlertProvider *> ProviderRegistry::alertChain(Coordinate coord) const
{
    return orderedChain<IAlertProvider>(m_alert, coord);
}

Capabilities ProviderRegistry::alertCapabilitiesAt(Coordinate coord) const
{
    // The union, which is right here and wrong for the other two — see the
    // header. Undetermined is unioned too and then cleared where it overlaps
    // available, which Capabilities' constructor does: one provider knowing it
    // has alerts here settles the question for the place, whatever a second
    // provider has not learned yet.
    CapabilityFlags available;
    CapabilityFlags undetermined;
    for (IAlertProvider *provider : alertChain(coord)) {
        const Capabilities capabilities = provider->capabilitiesAt(coord);
        available |= capabilities.available();
        undetermined |= capabilities.undetermined();
    }
    return Capabilities(available, undetermined);
}

QList<const IProvider *> ProviderRegistry::providers() const
{
    // Deduplicated by id, because one source can serve two products —
    // Open-Meteo is the forecast provider and the air-quality provider, on two
    // hosts, under one id — and the About screen credits a source once.
    QList<const IProvider *> all;
    QStringList              seen;

    const auto append = [&all, &seen](const IProvider *provider) {
        if (provider == nullptr || seen.contains(provider->id()))
            return;
        seen.append(provider->id());
        all.append(provider);
    };

    for (const Entry<IForecastProvider> &entry : m_forecast)
        append(entry.provider);
    for (const Entry<IAirQualityProvider> &entry : m_airQuality)
        append(entry.provider);
    for (const Entry<IAlertProvider> &entry : m_alert)
        append(entry.provider);

    return all;
}

QList<Attribution> ProviderRegistry::attributions() const
{
    QList<Attribution> credits;
    const QList<const IProvider *> all = providers();
    credits.reserve(all.size());
    for (const IProvider *provider : all)
        credits.append(provider->attribution());
    return credits;
}

// ---- the fallback loop ------------------------------------------------------------------

QFuture<Result<ForecastAnswer>> ProviderRegistry::fetchForecast(const ForecastRequest &request)
{
    return walk<IForecastProvider, Forecast>(
        this, forecastChain(request.coord), request, QStringLiteral("forecast"),
        [](IForecastProvider *provider, const ForecastRequest &r) {
            return provider->fetchForecast(r);
        },
        [this](const QString &servedBy, const QString &primary) {
            Q_EMIT servedByFallback(QStringLiteral("forecast"), servedBy, primary);
        });
}

QFuture<Result<AirQualityAnswer>> ProviderRegistry::fetchAirQuality(const ForecastRequest &request)
{
    return walk<IAirQualityProvider, AirQuality>(
        this, airQualityChain(request.coord), request, QStringLiteral("air quality"),
        [](IAirQualityProvider *provider, const ForecastRequest &r) {
            return provider->fetchAirQuality(r);
        },
        [this](const QString &servedBy, const QString &primary) {
            Q_EMIT servedByFallback(QStringLiteral("air quality"), servedBy, primary);
        });
}

// ---- the fan-out ---------------------------------------------------------------------
//
// Not the walk above. Every covering provider is asked, concurrently, and the
// answers are merged. registry.h and libclima/providers/ialertprovider.h both
// carry the argument; what follows is the bookkeeping.
//
// The three outcomes, and they are genuinely three:
//
//   at least one set        success. `complete` is true only if every provider
//                           that was asked either produced a set or declined
//                           the question with Unsupported.
//   every provider declined Unsupported. Nobody covers this place after all,
//                           and §4.4 says the UI hides the feature.
//   everything else failed  the chain error, built the same way as the walk's.

QFuture<Result<AlertAnswer>> ProviderRegistry::fetchAlerts(const AlertRequest &request)
{
    const QList<IAlertProvider *> chain = alertChain(request.coord);

    if (chain.isEmpty()) {
        return QtFuture::makeReadyValueFuture(Result<AlertAnswer>(
            chainError({}, QStringLiteral("alerts"))));
    }

    struct State {
        QPromise<Result<AlertAnswer>> promise;
        QList<QFuture<void>>          pending;

        QList<Alert>           merged;
        QStringList            servedBy;
        QList<ProviderFailure> failures;

        // Providers that answered Unsupported: asked, declined, and not counted
        // against completeness. Held as a number rather than a list because
        // nothing reads which ones they were.
        int declined = 0;

        QDateTime fetchedAt;

        // The EARLIEST confirmation across contributing providers. A set is only
        // as confirmed as its least recently confirmed part, and taking the
        // latest here would let a healthy NWS poll vouch for an ECCC answer that
        // has been served from cache since breakfast.
        QDateTime confirmedAt;

        Coordinate coordinate;
        int        outstanding = 0;
    };

    auto state         = std::make_shared<State>();
    state->coordinate  = request.coord;
    state->outstanding = int(chain.size());
    state->promise.start();

    QFuture<Result<AlertAnswer>> future = state->promise.future();

    const auto settle = [state]() {
        if (--state->outstanding > 0)
            return;

        const bool nobodyServed = state->servedBy.isEmpty();

        if (nobodyServed && state->failures.isEmpty()) {
            // Everything that was asked declined. Not an error worth showing —
            // the same answer as an empty chain.
            Error error(ErrorKind::Unsupported,
                        QStringLiteral("no alert provider covers %1")
                            .arg(state->coordinate.toKeyString()));
            state->promise.addResult(Result<AlertAnswer>(error));
            state->promise.finish();
            return;
        }

        if (nobodyServed) {
            state->promise.addResult(Result<AlertAnswer>(
                chainError(state->failures, QStringLiteral("alerts"))));
            state->promise.finish();
            return;
        }

        AlertSet set;
        set.alerts      = state->merged;
        set.coordinate  = state->coordinate;
        set.fetchedAt   = state->fetchedAt;
        set.confirmedAt = state->confirmedAt;
        set.providerId  = state->servedBy.join(QStringLiteral(", "));

        // A provider that declined is not missing. A provider that failed is.
        set.complete = state->failures.isEmpty();

        AlertAnswer answer;
        answer.value    = set;
        answer.servedBy = set.providerId;
        answer.failures = state->failures;

        // Deliberately left false. There is no fallback here to have taken
        // over, and setting it would put "showing NWS — ECCC is unavailable" on
        // a screen where both were asked and one simply had nothing to say.
        answer.fromFallback = false;

        state->promise.addResult(Result<AlertAnswer>(answer));
        state->promise.finish();
    };

    for (IAlertProvider *provider : chain) {
        const QString providerId = provider->id();

        state->pending.append(provider->fetchAlerts(request).then(
            this, [state, settle, providerId](const Result<AlertSet> &result) {
                if (result.hasValue()) {
                    const AlertSet &part = result.value();

                    for (const Alert &alert : part.alerts) {
                        // Keys are provider-prefixed, so a duplicate across two
                        // services cannot happen — this is a guard against one
                        // provider repeating itself, which a merge of two
                        // paginated answers could one day do.
                        const bool known =
                            std::any_of(state->merged.cbegin(), state->merged.cend(),
                                        [&alert](const Alert &seen) {
                                            return seen.isSameHazard(alert);
                                        });
                        if (!known)
                            state->merged.append(alert);
                    }

                    state->servedBy.append(providerId);

                    if (part.fetchedAt.isValid()
                        && (!state->fetchedAt.isValid() || part.fetchedAt > state->fetchedAt))
                        state->fetchedAt = part.fetchedAt;

                    if (part.confirmedAt.isValid()
                        && (!state->confirmedAt.isValid()
                            || part.confirmedAt < state->confirmedAt))
                        state->confirmedAt = part.confirmedAt;

                } else if (result.errorKind() == ErrorKind::Unsupported) {
                    ++state->declined;
                } else {
                    state->failures.append({ providerId, result.error() });
                }

                settle();
            }));
    }

    return future;
}

} // namespace clima
