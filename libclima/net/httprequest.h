// SPDX-FileCopyrightText: 2026 Jowi Aoun
// SPDX-License-Identifier: MPL-2.0
//
// What a provider asks for, and what comes back.
//
// ---- the URL arrives in pieces, on purpose ----------------------------------
//
// A provider hands over a base URL with no query string, a list of parameters,
// and — separately — a coordinate. It does not hand over a finished URL, and
// the reason is the one rule this whole layer is built around:
//
//     the coordinate is rounded BEFORE it is hashed, and before it is sent.
//
// If providers built their own URLs, each of them would have to remember to
// round, and the first one that forgot would issue a fresh request for every
// frame of a map drag — a hundred identical forecasts, a hundred rows in
// somebody's rate-limit ledger, and no symptom on our side except a slow tab.
// Composing the URL in one place means it is impossible to get wrong once, let
// alone once per provider. See libclima/domain/coordinate.h for why four
// decimals is the right number and not merely a round one.
//
// The parameter *names* differ — Open-Meteo says `latitude`/`longitude`, MET
// Norway says `lat`/`lon` — so those are per-request strings rather than a
// constant. A request with no coordinate at all (a radar timeline manifest, a
// tile) leaves the optional empty and the two names are ignored.

#pragma once

#include "libclima/cache/cachepolicy.h"
#include "libclima/domain/coordinate.h"
#include "libclima/net/validatorstore.h"

#include <QByteArray>
#include <QDateTime>
#include <QList>
#include <QMap>
#include <QPair>
#include <QString>
#include <QUrl>

#include <optional>

namespace clima {

// How the coordinate is spelled in the query string.
//
// Three shapes rather than one because the services genuinely disagree, and the
// alternative — a provider composing its own URL — would put the rounding rule
// above back in every provider's hands. Everything here still goes through the
// single `rounded()` call in composeUrl(), which is the property this enum
// exists to preserve.
enum class CoordinateForm {
    // latitude=43.6532&longitude=-79.3832. Open-Meteo, MET Norway.
    LatitudeLongitudePair,

    // point=43.6532,-79.3832 — latitude first. api.weather.gov.
    LatitudeCommaLongitude,

    // bbox=-79.3832,43.6532,-79.3832,43.6532 — LONGITUDE first, and the same
    // point twice. A zero-area bounding box is how OGC API — Features Part 1
    // asks "what covers this point", and it is what api.weather.gc.ca is asked
    // with. See libclima/providers/eccc/ecccalertprovider.h for why that rather
    // than the CQL2 spatial filter the plan called for.
    DegenerateBoundingBox,
};

struct HttpRequest {
    // Which provider is asking. This is the unit a 403 disables, so it has to
    // be the provider's stable id — "open-meteo", "met-no", "nws" — and not a
    // per-request label.
    QString providerId;

    // A stable name for *what* is being asked for, used in the cache key and
    // in diagnostics: "forecast", "air-quality", "alerts/active". Not the URL
    // path, because two providers' paths for the same product differ and the
    // key is meant to read the same across both.
    QString endpoint;

    // Scheme, host and path. No query — see the header comment.
    QUrl url;

    // Which row of the TTL table this answer belongs in.
    DataKind kind = DataKind::Forecast;

    // Empty for an endpoint that is not about a point on the earth.
    std::optional<Coordinate> coordinate;

    CoordinateForm coordinateForm = CoordinateForm::LatitudeLongitudePair;

    // Read only by CoordinateForm::LatitudeLongitudePair.
    QString latitudeParameter  = QStringLiteral("latitude");
    QString longitudeParameter = QStringLiteral("longitude");

    // Read by the other two forms: the one parameter the whole coordinate goes
    // into. "point", "bbox".
    QString coordinateParameter = QStringLiteral("point");

    // Everything else that goes in the query string, in the order a provider
    // wrote it. Order is preserved in the URL because a readable URL in a log
    // is worth something, and ignored in the cache key because two orderings
    // of the same parameters are the same request.
    QList<QPair<QString, QString>> parameters;

    // Provider-specific request headers. User-Agent is *not* one of these —
    // HttpClient sets it and refuses to let a caller override it, because it
    // is the one header a compliance obligation hangs off.
    QMap<QByteArray, QByteArray> headers;

    // Send If-None-Match / If-Modified-Since when a validator is on file.
    // False only for the rare endpoint where a 304 would leave us with nothing
    // to show, which today is none of them.
    bool conditional = true;
};

struct HttpResponse {
    int status = 0;

    // Exactly the bytes the server sent, unparsed. Empty on a 304.
    QByteArray body;
    QByteArray contentType;

    // What this response told us to remember for next time.
    Validators validators;

    // When the answer arrived, from the injected Clock — never from the wall
    // clock directly, so a fixture run's cache expires exactly when the
    // fixture says it does.
    QDateTime fetchedAt;

    // The later of (fetchedAt + our TTL) and the server's own Expires. See
    // HttpClient: a provider is allowed a longer opinion about its own data
    // than our table has, and honouring it saves a request that would have
    // been answered with the same bytes.
    QDateTime expiresAt;

    // True when the server said 304 and the body is deliberately empty. The
    // caller reads its cached payload; the entry's expiry moves forward.
    //
    // This flag is why 304 is not modelled as an Error. A conditional request
    // that gets 304 has *succeeded* — the data is confirmed current — and
    // reporting it as a failure would make every caller special-case a
    // successful outcome.
    bool notModified = false;

    // How many times the request had to be retried before this answer. Zero on
    // the common path; non-zero is worth a line in a diagnostic panel, because
    // "it worked, on the fourth try" is a different story from "it worked".
    int retries = 0;

    QUrl url;
};

// The final URL: base, then the rounded coordinate under the provider's own
// parameter names, then the rest of the parameters in the order given.
//
// Free function rather than a method so that a test can assert what would be
// sent without a client, a network stack or an event loop.
QUrl composeUrl(const HttpRequest &request);

} // namespace clima
