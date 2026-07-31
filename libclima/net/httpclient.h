// SPDX-FileCopyrightText: 2026 Jowi Aoun
// SPDX-License-Identifier: MPL-2.0
//
// The only thing in Clima that talks to the internet.
//
// This is a compliance component wearing a convenience component's clothes.
// Every provider goes through it, and it goes through it because four of the
// promises Clima makes to the services it uses for free are enforced here and
// nowhere else. A provider that built its own QNetworkAccessManager would break
// all four without a compile error.
//
// ============================================================================
// 1. THE USER-AGENT IS NOT OPTIONAL
//
//   Clima/<version> (+<project url>; <contact>)
//
// docs/02-data-sources.md §2.9 records this as MET Norway's requirement and
// notes that a generic User-Agent gets 403 or blocked outright. It is not
// theoretical: api.weather.gov answers 403 to an empty User-Agent today,
// verified against the live service. The string is built from CMake variables
// in libclima/climaidentity.h so the version in it cannot drift from the
// release, and HttpRequest deliberately has no way to override it — a
// per-request header map that could set User-Agent is a per-request header map
// that eventually does.
//
// ============================================================================
// 2. A 403 IS A HARD STOP, FOR THE LIFE OF THE PROCESS
//
// A 403 answering an identifying User-Agent is a statement about our client.
// The identical request will earn the identical refusal, and the difference
// between a client that is refused and a client that is *banned* is exactly
// whether it kept asking. So one 403 marks the provider disabled, every later
// request for it fails immediately with ErrorKind::ProviderDisabled without
// touching the network, and `providerDisabled` is emitted so a human can be
// told. There is no backoff, no retry, and no automatic recovery: recovery is
// a new process, which in practice means a new build with the User-Agent
// fixed.
//
// The routing layer above is expected to fall through to the next provider in
// the chain — docs/04-architecture.md §4.4 — so a disabled provider degrades
// the app rather than breaking it.
//
// ============================================================================
// 3. ONE IN-FLIGHT REQUEST PER (PROVIDER, ENDPOINT, ROUNDED COORDINATE, PARAMS)
//
// Coalescing, keyed by RequestKey. Three view models asking for the same
// forecast during a warm start make one request and share one answer. A map
// drag makes one request, because the coordinate is rounded to four decimals
// before it is hashed and before it is sent — see libclima/net/requestkey.h,
// which is where that is done and why.
//
// The mechanism is QFuture: every caller for a key in flight gets a copy of
// the same future, and one QPromise fulfils all of them.
//
// ============================================================================
// 4. CONDITIONAL GET, BECAUSE WE AGREED TO
//
// If-None-Match and If-Modified-Since go out whenever a validator is on file,
// and a 304 comes back as a *successful* HttpResponse with `notModified` set
// rather than as an error — the data is confirmed current, which is a success
// however few bytes it took. MET Norway's terms require this. A response's
// Expires header is honoured when it is later than our own TTL, on the grounds
// that a provider knows more about the shelf life of its own data than §4.5's
// table does.
//
// ============================================================================
//
// ---- 429 and 5xx: exponential backoff with jitter, capped at 30 minutes -----
//
// See libclima/net/backoff.h for the schedule and for why the jitter is
// injected rather than drawn from the global generator. A server's Retry-After
// beats our schedule when it sends one.
//
// ---- threading ---------------------------------------------------------------
//
// One thread, the one that constructed it. QNetworkAccessManager is not
// thread-safe and must be used from the thread that owns it; this class holds
// one and inherits that rule. docs/04-architecture.md §4.8 puts parsing on a
// QThreadPool and leaves signal plumbing on the owning thread, which is exactly
// the split this supports: the bytes come back here, and whoever receives them
// hands them to a worker.
//
// ---- what it does not do -----------------------------------------------------
//
// It does not parse, it does not know what a forecast is, and it does not write
// to the cache. It reads validators through a narrow interface (see
// validatorstore.h) and hands raw bytes back. Storing them is the caller's
// decision, because the caller is the one who knows whether they parsed.

#pragma once

#include "libclima/core/result.h"
#include "libclima/net/backoff.h"
#include "libclima/net/httprequest.h"
#include "libclima/net/requestkey.h"

#include <QFuture>
#include <QHash>
#include <QObject>
#include <QSet>
#include <QString>

#include <chrono>
#include <memory>

class QNetworkAccessManager;
class QNetworkReply;

namespace clima {

class Clock;
class ValidatorStore;

class HttpClient : public QObject
{
    Q_OBJECT

public:
    // `clock` must outlive the client. Not owned, not optional: every timestamp
    // this class produces comes from it, so a null one would mean a cache whose
    // expiry times are all invalid and which therefore never hits.
    explicit HttpClient(Clock *clock, QObject *parent = nullptr);
    ~HttpClient() override;

    // The exact bytes sent as User-Agent on every request. Public because the
    // About → Data sources screen should be able to show it — a user who has
    // been rate-limited deserves to see what we told the server we were — and
    // because it is the one string a test can assert without a network.
    [[nodiscard]] static QByteArray userAgent();

    // Not owned. Null means no conditional requests are sent and no validators
    // are recorded, which is a legal but impolite mode we use only in tests
    // that are about something else.
    void setValidatorStore(ValidatorStore *store);

    // Replaces the retry schedule. `seed` fixes the jitter, so a test gets the
    // same delays every run. Production leaves both alone.
    void setBackoffPolicy(const BackoffPolicy &policy, quint32 seed);

    // How long a single attempt may take before it is abandoned as a timeout.
    // Applies per attempt, not to the retry sequence as a whole.
    void setTransferTimeout(std::chrono::milliseconds timeout);

    // The request, or the reason there isn't one. Never a partial success:
    // docs/04-architecture.md §4.4.
    //
    // The returned future is already finished when the provider is disabled —
    // no round trip, no event loop turn required before the caller can read it.
    QFuture<Result<HttpResponse>> send(const HttpRequest &request);

    [[nodiscard]] bool        isProviderDisabled(const QString &providerId) const;
    [[nodiscard]] QStringList disabledProviders() const;

    // Requests currently on the wire or waiting out a backoff. One per key,
    // however many callers are waiting on it — which is what makes this the
    // number a coalescing test asserts.
    [[nodiscard]] int inFlightCount() const;

    // Cumulative counters, for tests and for a diagnostics panel. `dispatched`
    // counts attempts actually put on the wire, so `requested - dispatched` is
    // the work coalescing and the disabled-provider gate saved.
    [[nodiscard]] int requestedCount() const;
    [[nodiscard]] int dispatchedCount() const;
    [[nodiscard]] int coalescedCount() const;

Q_SIGNALS:
    // Emitted once, the first time a provider earns a 403. The reason is
    // developer-facing English; it is meant for a log and for the diagnostics
    // panel, not for a user-facing string.
    void providerDisabled(const QString &providerId, const QString &reason);

    // Emitted for every retry that is scheduled, so an outage is visible
    // somewhere other than in a packet capture.
    void retryScheduled(const QString &providerId, const QString &key, int retry, int delayMs);

private:
    struct InFlight;

    void dispatch(const std::shared_ptr<InFlight> &flight);
    void onReplyFinished(const std::shared_ptr<InFlight> &flight, QNetworkReply *reply);
    void scheduleRetry(const std::shared_ptr<InFlight> &flight,
                       std::chrono::milliseconds delay);
    void finish(const std::shared_ptr<InFlight> &flight, Result<HttpResponse> result);
    void disableProvider(const QString &providerId, const QString &reason);

    Clock                 *m_clock = nullptr;
    ValidatorStore        *m_validators = nullptr;
    QNetworkAccessManager *m_network = nullptr;

    Backoff m_backoff;

    std::chrono::milliseconds m_transferTimeout{ 20000 };

    QHash<RequestKey, std::shared_ptr<InFlight>> m_inFlight;
    QSet<QString>                                m_disabled;

    int m_requested = 0;
    int m_dispatched = 0;
    int m_coalesced = 0;
};

} // namespace clima
