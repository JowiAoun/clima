// SPDX-FileCopyrightText: 2026 Jowi Aoun
// SPDX-License-Identifier: GPL-3.0-or-later
//
// The shape the twelve detail cards read, before there is anything to put in it.
//
// ============================================================================
// WHAT THIS TEST IS ABOUT
//
// `Detail` is a QML singleton whose fifteen blocks are QVariantMaps, and QML
// binds `Detail.wind.gust` while the component holding that binding is being
// constructed. On any start that has to fetch — a first run with a cold cache,
// an expired forecast, a socket that is still opening — construction happens
// before the first snapshot, so those bindings are evaluated against whatever
// the maps hold at that moment.
//
// They used to hold nothing, and an empty map answers `undefined` to every key
// in it. Every launch of both binaries therefore printed several hundred lines
// of
//
//     Unable to assign [undefined] to QString
//     TypeError: Cannot read property 'length' of undefined
//
// before the data landed and it all quietly resolved. The rendering was never
// wrong; the model was simply not well formed until something filled it in.
//
// ============================================================================
// WHY IT IS A KEY-SET COMPARISON AND NOT A LIST OF EXPECTED KEYS
//
// The fix is a neutral shape per block — the same keys the build function
// produces, with every value at its nothing — and its failure mode is drift:
// somebody adds `d.gustDirection` to buildWind() and does not add it to
// neutralWind(). That is invisible. It compiles, the card draws correctly the
// moment data arrives, and the only symptom is one more line of console on
// startup, in a stream that (before this was fixed) nobody was reading anyway.
//
// A test that listed the keys it expected would have to be edited by the same
// person in the same commit, which is exactly the step being missed. So it
// asserts the RELATIONSHIP instead: the key set a block has at construction is
// the key set it has after a real fixture snapshot. Add a key to one side only
// and this fails, naming the block and the key.
//
// ============================================================================
// AND WHY THIS IS THE ONE TEST THAT LINKS THE APP
//
// tests/CMakeLists.txt says the tests link libclima and Qt Test and that is the
// whole list, because `engine_has_no_gui` reads a test binary's DT_NEEDED
// entries to prove the engine drags in no windowing toolkit. That guarantee is
// asked of tst_httpclient specifically and it is unaffected by this file: what
// is under test here is a VIEW MODEL, which lives in app/, is GPL-3.0-or-later
// rather than MPL-2.0, and is allowed a QML dependency because being read by
// QML is its entire job. It is registered by hand below the loop for that
// reason, without clima_forbid_gui.

#include "conditionsdata.h"

#include "libclima/providers/fixture/fixtureprovider.h"

#include <QSet>
#include <QStandardPaths>
#include <QStringList>
#include <QTest>

using namespace clima;

class TestConditionsData : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void initTestCase();

    void everyBlockCarriesItsKeysBeforeTheFirstSnapshot();
    void nothingInTheNeutralShapeIsUndefined();
    void theNeutralShapeInventsNoWeather();

private:
    // The blocks, as (name, getter) so a failure names the one that drifted.
    // `activities` is deliberately absent: it is a QVariantList, and an empty
    // list is already well formed — it answers `.length` with 0 and a Repeater
    // bound to it draws nothing. Only the maps had holes.
    using Block = std::pair<const char *, QVariantMap (ConditionsData::*)() const>;
    static QList<Block> blocks();

    static QStringList keysOf(const QVariantMap &map);
};

QList<TestConditionsData::Block> TestConditionsData::blocks()
{
    return {
        { "current",       &ConditionsData::current },
        { "temperature",   &ConditionsData::temperature },
        { "feelsLike",     &ConditionsData::feelsLike },
        { "cloudCover",    &ConditionsData::cloudCover },
        { "precipitation", &ConditionsData::precipitation },
        { "wind",          &ConditionsData::wind },
        { "humidity",      &ConditionsData::humidity },
        { "uv",            &ConditionsData::uv },
        { "airQuality",    &ConditionsData::airQuality },
        { "visibility",    &ConditionsData::visibility },
        { "pressure",      &ConditionsData::pressure },
        { "sun",           &ConditionsData::sun },
        { "moon",          &ConditionsData::moon },
        { "pollen",        &ConditionsData::pollen },
        { "location",      &ConditionsData::location },
    };
}

QStringList TestConditionsData::keysOf(const QVariantMap &map)
{
    QStringList names = map.keys();
    names.sort();
    return names;
}

void TestConditionsData::initTestCase()
{
    // Before anything constructs a Settings, which Units does on first use and
    // ConditionsData's constructor triggers. Without this the test writes to
    // the developer's real preferences directory.
    QStandardPaths::setTestModeEnabled(true);
}

// Berlin, because it is the fixture with pollen in it: outside the CAMS
// European domain buildPollen() gates the block off and fills only `available`,
// and a comparison against that would prove nothing about the other five keys.
void TestConditionsData::everyBlockCarriesItsKeysBeforeTheFirstSnapshot()
{
    ConditionsData data(nullptr);

    QHash<QString, QStringList> before;
    for (const Block &block : blocks())
        before.insert(QString::fromLatin1(block.first), keysOf((data.*block.second)()));

    const Fixture fixture = fixtures::load(QStringLiteral("berlin"));
    QVERIFY(fixture.isValid());

    ForecastRequest request;
    request.coord = fixture.place.coordinate;

    FixtureForecastProvider   forecasts(fixture);
    FixtureAirQualityProvider air(fixture);

    const QFuture<Result<Forecast>>   forecastFuture = forecasts.fetchForecast(request);
    const QFuture<Result<AirQuality>> airFuture      = air.fetchAirQuality(request);

    // Finished inside the call, which is what lets the app publish a fixture
    // before its QML engine loads. See AppEngine::fetch().
    QVERIFY(forecastFuture.isFinished());
    QVERIFY(airFuture.isFinished());

    const Result<Forecast>   forecast = forecastFuture.result();
    const Result<AirQuality> quality  = airFuture.result();
    QVERIFY(forecast.hasValue());
    QVERIFY(quality.hasValue());

    data.setSnapshot(forecast.value(), quality.value(), fixture.recordedAt, fixture.place,
                     /*hasPollen=*/true);

    for (const Block &block : blocks()) {
        const QString     name  = QString::fromLatin1(block.first);
        const QStringList after = keysOf((data.*block.second)());

        const QSet<QString> missing = QSet<QString>(after.cbegin(), after.cend())
                                    - QSet<QString>(before.value(name).cbegin(),
                                                    before.value(name).cend());
        const QSet<QString> extra = QSet<QString>(before.value(name).cbegin(),
                                                  before.value(name).cend())
                                  - QSet<QString>(after.cbegin(), after.cend());

        QVERIFY2(missing.isEmpty(),
                 qPrintable(QStringLiteral(
                     "Detail.%1 gains %2 only once a snapshot lands. Add it to the matching "
                     "neutral*() in app/viewmodels/conditionsdata.cpp, or QML reads it as "
                     "undefined on every start that has to fetch.")
                                .arg(name, QStringList(missing.cbegin(), missing.cend())
                                               .join(QStringLiteral(", ")))));

        QVERIFY2(extra.isEmpty(),
                 qPrintable(QStringLiteral(
                     "Detail.%1 carries %2 before a snapshot and never again. A key nothing "
                     "fills is a key nothing should promise.")
                                .arg(name, QStringList(extra.cbegin(), extra.cend())
                                               .join(QStringLiteral(", ")))));
    }
}

// A key that is present but holds an invalid QVariant is the same `undefined`
// with extra steps, so the shape is only well formed if every value in it is a
// value.
void TestConditionsData::nothingInTheNeutralShapeIsUndefined()
{
    ConditionsData data(nullptr);

    for (const Block &block : blocks()) {
        const QVariantMap map = (data.*block.second)();
        QVERIFY2(!map.isEmpty(), block.first);

        for (auto it = map.cbegin(); it != map.cend(); ++it) {
            QVERIFY2(it.value().isValid(),
                     qPrintable(QStringLiteral("Detail.%1.%2 is an invalid QVariant, which "
                                               "reaches QML as undefined.")
                                    .arg(QString::fromLatin1(block.first), it.key())));
        }
    }
}

// docs/README.md's first non-negotiable, asked of the one frame that has no
// data behind it: a card may say nothing, and may not say something.
//
// Every string is empty except `trend`, which is "none" — the word TrendBadge
// hides itself on, and the model's own vocabulary for a reading that is not
// doing anything. Nothing in the shape is a formatted reading: no em dash
// either, because "we asked and there is no value" is a claim that has not been
// earned before anybody has asked.
void TestConditionsData::theNeutralShapeInventsNoWeather()
{
    ConditionsData data(nullptr);

    for (const Block &block : blocks()) {
        const QVariantMap map = (data.*block.second)();

        for (auto it = map.cbegin(); it != map.cend(); ++it) {
            if (it.value().typeId() != QMetaType::QString)
                continue;

            const QString text     = it.value().toString();
            const QString expected = it.key() == QLatin1String("trend")
                                         ? QStringLiteral("none")
                                         : QString();

            QVERIFY2(text == expected,
                     qPrintable(QStringLiteral("Detail.%1.%2 says \"%3\" before there is any "
                                               "weather. It should say nothing.")
                                    .arg(QString::fromLatin1(block.first), it.key(), text)));
        }
    }

    // And the two sentence fields on the block that has to exist before the
    // first fetch even resolves a place.
    QCOMPARE(data.observedAt(), QString());
    QCOMPARE(data.observedOn(), QString());
    QVERIFY(data.activities().isEmpty());
}

// Guiless on purpose. The target links the QML module because that is where the
// view model lives, but nothing under test needs a window or a platform plugin,
// and a test that opens neither cannot hang waiting for one.
QTEST_GUILESS_MAIN(TestConditionsData)
#include "tst_conditionsdata.moc"
