// SPDX-FileCopyrightText: 2026 Jowi Aoun
// SPDX-License-Identifier: GPL-3.0-or-later
//
// The WMO tables: total, closed, and agreeing with each other.
//
// Three properties, and each of them is a way the tables can be wrong without
// anything failing to compile:
//
//   * a code that has a precipitation type but no glyph, or the reverse
//   * a glyph name QML does not switch on, which renders as an empty item
//   * a code Open-Meteo emits that nothing here has heard of

#include "libclima/domain/weathercode.h"

#include <QSet>
#include <QTest>

using namespace clima;

namespace {

// Every code Open-Meteo documents for `weather_code`. The list is closed —
// WMO 4677 has ninety-nine entries and a forecast model emits these.
QList<int> emittedCodes()
{
    return { 0,  1,  2,  3,  45, 48, 51, 53, 55, 56, 57, 61, 63, 65, 66,
             67, 71, 73, 75, 77, 80, 81, 82, 85, 86, 95, 96, 99 };
}

} // namespace

class TestWeatherCode : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void everyEmittedCodeHasAPhrase();
    void everyEmittedCodeHasAGlyph();
    void typeAndGlyphAgreeAboutWhetherItIsRaining();
    void theSixTypesAreSpelledThePrecipJsWay();
    void dayAndNightDifferOnlyWhereTheSkyIsVisible();
    void anUnknownCodeIsCloudyAndSilent();
    void drawableTodayIsClosedOverWhatTheGlyphCanDraw();
};

void TestWeatherCode::everyEmittedCodeHasAPhrase()
{
    for (int code : emittedCodes()) {
        QVERIFY2(!conditionText(code, true).isEmpty(), qPrintable(QString::number(code)));
        QVERIFY2(!conditionText(code, false).isEmpty(), qPrintable(QString::number(code)));
    }
}

void TestWeatherCode::everyEmittedCodeHasAGlyph()
{
    // Not "returns something" — `conditionFor` has a default — but "returns
    // something other than the default", which is the only way to catch a code
    // that was never given a row.
    for (int code : emittedCodes()) {
        if (code == 3)
            continue;   // overcast genuinely is Cloudy
        QVERIFY2(conditionFor(code, true) != ConditionKind::Cloudy,
                 qPrintable(QStringLiteral("WMO %1 has no glyph of its own").arg(code)));
    }
}

void TestWeatherCode::typeAndGlyphAgreeAboutWhetherItIsRaining()
{
    // The two tables are written separately and are read together. A code that
    // is Drizzle to one and ClearDay to the other draws a sun over a wash.
    const QSet<ConditionKind> wetKinds = {
        ConditionKind::Drizzle, ConditionKind::Rain,    ConditionKind::RainNight,
        ConditionKind::Sleet,   ConditionKind::Snow,    ConditionKind::Thunder,
        ConditionKind::Hail,
    };

    for (int code = 0; code <= 99; ++code) {
        const bool wetType  = precipitationTypeFor(code) != PrecipitationType::None;
        const bool wetGlyph = wetKinds.contains(conditionFor(code, true));
        QVERIFY2(wetType == wetGlyph,
                 qPrintable(QStringLiteral("WMO %1: type says %2, glyph says %3")
                                .arg(code)
                                .arg(wetType)
                                .arg(wetGlyph)));
    }
}

void TestWeatherCode::theSixTypesAreSpelledThePrecipJsWay()
{
    // These strings are a contract with app/qml/Clima/precip.js's TYPES array
    // and its STYLE table. A typo here does not fail: `STYLE[c.type]` misses,
    // `_styleOf` falls back to rain, and a snowstorm draws as rain.
    QCOMPARE(precipitationTypeName(PrecipitationType::Drizzle), QStringLiteral("drizzle"));
    QCOMPARE(precipitationTypeName(PrecipitationType::Rain), QStringLiteral("rain"));
    QCOMPARE(precipitationTypeName(PrecipitationType::Sleet), QStringLiteral("sleet"));
    QCOMPARE(precipitationTypeName(PrecipitationType::Snow), QStringLiteral("snow"));
    QCOMPARE(precipitationTypeName(PrecipitationType::Hail), QStringLiteral("hail"));
    QCOMPARE(precipitationTypeName(PrecipitationType::Thunder), QStringLiteral("thunder"));

    // And None is the empty string, which is what `typeFor` treats as "dry".
    QCOMPARE(precipitationTypeName(PrecipitationType::None), QString());
}

void TestWeatherCode::dayAndNightDifferOnlyWhereTheSkyIsVisible()
{
    // Clear, mainly clear, partly cloudy and rain have a night form. Fog,
    // snow, sleet, drizzle, thunder and hail do not — there is no moon to draw
    // behind any of them, and inventing one would mean six more glyphs for a
    // difference nobody can see at 26 px.
    for (int code : { 0, 1, 2, 61, 63, 65, 80, 81, 82 })
        QVERIFY2(conditionFor(code, true) != conditionFor(code, false),
                 qPrintable(QString::number(code)));

    for (int code : { 3, 45, 48, 51, 53, 55, 71, 73, 75, 95, 96, 99 })
        QVERIFY2(conditionFor(code, true) == conditionFor(code, false),
                 qPrintable(QString::number(code)));
}

void TestWeatherCode::anUnknownCodeIsCloudyAndSilent()
{
    // -1 is how the adapter spells "this hour carried no code". WMO defines
    // codes a forecast model will never emit — duststorms, funnel clouds —
    // and inventing a picture for one would have the app claim something
    // nobody forecast.
    for (int code : { -1, 4, 20, 39, 98, 1000 }) {
        QCOMPARE(precipitationTypeFor(code), PrecipitationType::None);
        QCOMPARE(conditionFor(code, true), ConditionKind::Cloudy);
        QCOMPARE(conditionText(code, true), QString());
    }
}

void TestWeatherCode::drawableTodayIsClosedOverWhatTheGlyphCanDraw()
{
    // WeatherGlyph.qml switches on seven strings and renders an empty item for
    // anything else — every one of its hasSun/hasCloud/hasRain booleans is
    // false. An empty glyph reads as "no data", which is a worse lie than a
    // slightly wrong picture, so everything degrades into the seven.
    const QSet<QString> drawable = {
        QStringLiteral("clear-day"),   QStringLiteral("clear-night"),
        QStringLiteral("partly-day"),  QStringLiteral("partly-night"),
        QStringLiteral("cloudy"),      QStringLiteral("rain"),
        QStringLiteral("rain-night"),
    };

    for (int code = 0; code <= 99; ++code) {
        for (bool day : { true, false }) {
            const ConditionKind kind = drawableToday(conditionFor(code, day));
            QVERIFY2(drawable.contains(conditionKindName(kind)),
                     qPrintable(QStringLiteral("WMO %1 (%2) degrades to %3")
                                    .arg(code)
                                    .arg(day ? QStringLiteral("day") : QStringLiteral("night"))
                                    .arg(conditionKindName(kind))));
        }
    }

    // Idempotent: a kind that is already drawable is left alone, so a caller
    // that pipes everything through it does not slowly turn the whole forecast
    // into rain.
    for (int code = 0; code <= 99; ++code) {
        const ConditionKind once = drawableToday(conditionFor(code, true));
        QCOMPARE(drawableToday(once), once);
    }

    // And it degrades by one step rather than to a single value: snow becomes
    // cloudy, not rain, because a sky you cannot see through is nearer to
    // snow than falling water is.
    QCOMPARE(drawableToday(ConditionKind::Snow), ConditionKind::Cloudy);
    QCOMPARE(drawableToday(ConditionKind::Fog), ConditionKind::Cloudy);
    QCOMPARE(drawableToday(ConditionKind::Thunder), ConditionKind::Rain);
    QCOMPARE(drawableToday(ConditionKind::Hail), ConditionKind::Rain);
}

QTEST_MAIN(TestWeatherCode)
#include "tst_weathercode.moc"
