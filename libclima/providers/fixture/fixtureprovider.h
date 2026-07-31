// SPDX-FileCopyrightText: 2026 Jowi Aoun
// SPDX-License-Identifier: MPL-2.0
//
// A provider that answers from a recording, at the instant the recording was
// made.
//
// ============================================================================
// WHY THIS IS A PROVIDER AND NOT A MODE
//
// The alternative — a `bool fixtureMode` threaded through the app — is the
// design libclima/core/clock.h already argues against at length, and every word
// of that argument applies here. Five flags in, "fixture mode" is a
// cross-cutting concern living in the cache, the network layer and the view
// models, and no single file describes what it does.
//
// So there is no fixture mode. There is a provider that happens to read its
// bytes from a resource instead of from a socket, registered into the same
// ProviderRegistry as any other, and a FrozenClock set to the moment those
// bytes were captured. Everything downstream — the TTL table, the backoff, the
// "now" marker on the hourly strip, the past veil, the sky phase, the "updated
// N minutes ago" line — behaves exactly as it did on the afternoon of the
// recording, and not one of them knows why.
//
// That is also what makes a golden image worth comparing: two runs a week apart
// produce the same bytes, because there is nothing in the pipeline that can
// tell them apart.
//
// ============================================================================
// IT PARSES. IT DOES NOT SHORT-CIRCUIT.
//
// The recorded payload goes through `openmeteo::adaptForecast()` — the same
// function the live provider calls on the same bytes. A fixture provider that
// handed back a pre-built Forecast would be a fixture of the adapter's output
// rather than of the service's, and the first thing it would stop testing is
// the adapter: the hour shift, the unit division, the time-axis
// reconstruction. Those are exactly the three things that are invisible on
// screen when they are wrong, which is why the review workflow this class
// exists to serve has to exercise them.
//
// ============================================================================
// THE ATTRIBUTION IS OPEN-METEO'S, AND SAYS SO
//
// The bytes are Open-Meteo's, recorded under CC BY 4.0, so the credit line is
// Open-Meteo's credit line. What is ours is the `note`, which says the data is
// a recording and when it was taken — because a screenshot of this app taken in
// fixture mode is a screenshot of a July afternoon whatever the calendar says,
// and a reader has a right to know that.
//
// ProviderRegistry::add() would refuse an incomplete Attribution anyway; this
// paragraph is about the fields it cannot check.

#pragma once

#include "libclima/core/result.h"
#include "libclima/domain/place.h"
#include "libclima/providers/iforecastprovider.h"

#include <QByteArray>
#include <QDateTime>
#include <QObject>
#include <QString>
#include <QStringList>

namespace clima {

// ---- what a fixture is -------------------------------------------------------
//
// One recorded place, its two payloads, and the instant the recording was made.
// A payload may be empty, which means "this fixture does not carry that
// product" rather than "the product failed" — kampala carries a forecast and no
// air quality, because it exists to prove one thing about one hour of rain.
struct Fixture {
    QString    name;
    QDateTime  recordedAt;   // UTC. What the FrozenClock is set to.
    Place      place;
    QByteArray forecast;     // Open-Meteo /v1/forecast, verbatim
    QByteArray airQuality;   // Open-Meteo /v1/air-quality, verbatim; may be empty

    // A sentence for the About screen and for `--fixture list`.
    QString description;

    [[nodiscard]] bool isValid() const { return !name.isEmpty() && recordedAt.isValid(); }
};

// ---- the catalogue -------------------------------------------------------------
//
// Fixtures are compiled in, under `:/clima/fixtures/<name>/`. Compiled in and
// not read from tests/, because `--fixture` has to work from an installed
// binary on a machine that has no source tree — that is the difference between
// a reviewer's tool and a developer's habit.
namespace fixtures {

// Sorted, for a stable `--help` and a stable error message.
QStringList names();

bool exists(const QString &name);

// An invalid Fixture when the name is unknown or the manifest will not parse.
// Not a Result, because the only two callers are a command-line parser that has
// already validated the name against names() and a test.
Fixture load(const QString &name);

// The name a build defaults to when something asks for a fixture without saying
// which. One place, so that the app, the gallery and CI cannot drift.
QString defaultName();

} // namespace fixtures

// ---- the providers -------------------------------------------------------------

class FixtureForecastProvider final : public QObject, public IForecastProvider
{
    Q_OBJECT

public:
    // Takes the fixture by value: it is a handful of kilobytes and this object
    // outlives whatever loaded it. Copy-cheap is not the same as free, and this
    // is one of the places where owning the bytes outright is worth more than
    // saving them.
    explicit FixtureForecastProvider(Fixture fixture, QObject *parent = nullptr);
    ~FixtureForecastProvider() override;

    static QString providerId();

    [[nodiscard]] QString      id() const override;
    [[nodiscard]] QString      displayName() const override;
    [[nodiscard]] Attribution  attribution() const override;
    [[nodiscard]] bool         covers(Coordinate coord) const override;
    [[nodiscard]] Capabilities capabilitiesAt(Coordinate coord) const override;

    // Already finished when it returns. No event loop turn, no thread, no
    // socket: a caller that awaits this future gets its answer inside the same
    // call stack, which is what lets the first frame be drawn from a fixture
    // with no "loading" state to photograph.
    QFuture<Result<Forecast>> fetchForecast(const ForecastRequest &request) override;

    [[nodiscard]] const Fixture &fixture() const { return m_fixture; }

private:
    Fixture      m_fixture;
    Capabilities m_capabilities;
};

class FixtureAirQualityProvider final : public QObject, public IAirQualityProvider
{
    Q_OBJECT

public:
    explicit FixtureAirQualityProvider(Fixture fixture, QObject *parent = nullptr);
    ~FixtureAirQualityProvider() override;

    static QString providerId();

    [[nodiscard]] QString      id() const override;
    [[nodiscard]] QString      displayName() const override;
    [[nodiscard]] Attribution  attribution() const override;
    [[nodiscard]] bool         covers(Coordinate coord) const override;
    [[nodiscard]] Capabilities capabilitiesAt(Coordinate coord) const override;

    QFuture<Result<AirQuality>> fetchAirQuality(const ForecastRequest &request) override;

private:
    Fixture      m_fixture;
    Capabilities m_capabilities;
};

} // namespace clima
