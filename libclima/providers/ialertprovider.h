// SPDX-FileCopyrightText: 2026 Jowi Aoun
// SPDX-License-Identifier: MPL-2.0
//
// What an alert provider is.
//
// Its own header rather than a third interface in iforecastprovider.h, because
// what it asks for is genuinely different: a forecast request carries days, a
// resolution, a model list and a time zone, and an alert request carries a
// point and a language. Reusing ForecastRequest would mean four fields that are
// ignored, and an ignored field is one somebody eventually sets.
//
// ============================================================================
// ALERTS FAN OUT. THEY DO NOT FALL BACK.
//
// Everywhere else in this codebase a chain means "ask the first, and if it
// fails ask the next" — libclima/providers/registry.h. For alerts that is
// wrong, and wrong in a way that hides warnings.
//
// The bug: a fallback chain stops at the first provider that SUCCEEDS. An alert
// provider succeeds by answering `{"features": []}` — HTTP 200, a well-formed
// empty collection — which is what ECCC returns for any coordinate south of the
// border, verified: tests/fixtures/alerts/eccc/toronto-clear.json is 850 bytes
// of exactly that. Detroit is inside the Canadian bounding box, because the box
// has to contain the Great Lakes. So a fallback chain in Detroit asks ECCC,
// gets a valid empty answer, stops, and never asks the National Weather Service
// about the tornado.
//
// The bounding boxes cannot be tightened out of this. registry.h already argues
// at length that no rectangle follows an 8,891 km border and that the honest
// answer for a border city is "both" — the forecast chain can live with that
// because a forecast from either service is a forecast. An alert set from one
// of two services is HALF THE WARNINGS.
//
// So ProviderRegistry::fetchAlerts() queries every covering provider
// concurrently and merges. The merge is well defined because the sets are
// disjoint by construction — ECCC and NWS issue about their own territory, and
// Alert::identityKeys are provider-prefixed, so nothing can be double-counted
// even if they were not.
//
// The cost of that decision is stated in AlertSet::complete: when one provider
// of two answers, the result is not a complete answer, and saying "no alerts"
// when what happened is "could not reach the service that would know" is the
// failure this whole file exists to avoid.
//
// ============================================================================
// A REGIONAL SERVICE ASKED OUTSIDE ITS REGION RETURNS Unsupported
//
// Not NotFound, not HttpStatus. api.weather.gov answers a Canadian coordinate
// with HTTP 400 and `"Parameter \"point\" is invalid: out of bounds"` — verified
// live, recorded in tests/fixtures/alerts/nws/out-of-bounds.json. That is a
// well-formed statement that the question was not for them, and it must not
// reach the user as a failure or make AlertSet::complete false. covers() should
// have kept us from asking; when the boxes are loose enough that we ask anyway,
// this is the answer that costs nothing.

#pragma once

#include "libclima/domain/alert.h"
#include "libclima/providers/iforecastprovider.h"

#include <QFuture>
#include <QString>

namespace clima {

struct AlertRequest {
    Coordinate coord;

    // Answer from the cache or not at all. Same flag, same meaning and same
    // reason as ForecastRequest::cachedOnly — docs/04-architecture.md §4.1's
    // "render from cache, then reconcile", which for alerts is the difference
    // between a tornado warning on the first frame and one two seconds later.
    bool cachedOnly = false;

    // An ISO 639-1 code. ECCC is bilingual by FIELD SELECTION — one feature
    // carries `alert_text_en` and `alert_text_fr` together — rather than by
    // serving different documents, so this picks which field is read and does
    // not change the request. NWS is English only and ignores it.
    //
    // Two decimals of a consequence: because it selects a field rather than a
    // document, the cached payload is language-independent and switching
    // language costs no request.
    QString language = QStringLiteral("en");
};

class IAlertProvider : public IProvider
{
public:
    ~IAlertProvider() override;

    // Either an AlertSet — possibly empty, which is the commonest answer and is
    // a success — or a typed Error. An empty set and a failure are the two
    // things this interface exists to keep apart.
    virtual QFuture<Result<AlertSet>> fetchAlerts(const AlertRequest &request) = 0;
};

} // namespace clima
