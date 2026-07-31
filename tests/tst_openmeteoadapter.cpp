// SPDX-FileCopyrightText: 2026 Jowi Aoun
// SPDX-License-Identifier: GPL-3.0-or-later
//
// The Open-Meteo mapping, replayed against eight recorded responses.
//
// Golden-file tests in the sense docs/04-architecture.md §4.11 asks for: the
// fixtures in tests/fixtures/openmeteo/ are bytes the live service actually
// sent, recorded once with curl and committed, and nothing here touches a
// network. tests/fixtures/openmeteo/README.md records the exact URL and date
// behind each one.
//
// Recorded rather than hand-written because the three defects this file exists
// to catch are all defects of *belief* — about a unit, about an interval,
// about what a timestamp means — and a fixture somebody wrote by hand encodes
// the same beliefs as the parser and agrees with it happily.

#include "libclima/domain/hourconvention.h"
#include "libclima/domain/timeaxis.h"
#include "libclima/domain/weathercode.h"
#include "libclima/providers/openmeteo/openmeteoadapter.h"

#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QTest>
#include <QTimeZone>

using namespace clima;

namespace {

QByteArray fixture(const QString &name)
{
    QFile file(QStringLiteral(CLIMA_SOURCE_DIR "/tests/fixtures/openmeteo/") + name);
    if (!file.open(QIODevice::ReadOnly)) {
        qWarning("cannot open fixture %s", qPrintable(name));
        return {};
    }
    return file.readAll();
}

Forecast adapt(const QString &name)
{
    const Result<Forecast> result =
        openmeteo::adaptForecast(fixture(name), QStringLiteral("open-meteo"));
    if (!result)
        qWarning("fixture %s failed to parse: %s", qPrintable(name),
                 qPrintable(result.error().toString()));
    return result.hasValue() ? result.value() : Forecast{};
}

QDateTime utc(int year, int month, int day, int hour, int minute = 0)
{
    return QDateTime(QDate(year, month, day), QTime(hour, minute), QTimeZone::UTC);
}

// The local hour of an instant, as a chart axis would label it.
int localHour(const QDateTime &instant, const QTimeZone &zone)
{
    return instant.toTimeZone(zone).time().hour();
}

} // namespace

class TestOpenMeteoAdapter : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void fixturesAreReadable();

    void torontoParsesEveryBlock();
    void visibilityArrivesInMetresAndIsStoredInKilometres();
    void snowfallStaysInCentimetresBesidePrecipitationInMillimetres();
    void nullsBecomeAbsentReadingsRatherThanZero();
    void aMoonThatDoesNotRiseIsAbsentAndNotAnError();

    void theWmoCodeBecomesAPrecipitationTypeAndAGlyph();
    void everyCodeInTheRecordedResponsesIsRecognised();

    void precipitationIsShiftedOntoTheHourItFallsIn();
    void theUnshiftedSeriesWouldPutTheBandAnHourLate();
    void shiftingCostsExactlyTheLastHour();

    void openMeteoLabelsADstDayWithTwentyFourHours();
    void theLocalAxisHasTwentyFiveSlotsOnAFallBackDay();
    void theLocalAxisHasTwentyThreeSlotsOnASpringForwardDay();
    void sunTimesSurviveTheZoneCorrection();

    void midnightSunIsAFullArcRatherThanAnEmptyOne();
    void aModelWithoutUvOrVisibilityLeavesThoseColumnsEmpty();

    void malformedInputIsAParseErrorAndNotAnEmptyForecast();
};

// ---------------------------------------------------------------------------
// The fixtures themselves
// ---------------------------------------------------------------------------

void TestOpenMeteoAdapter::fixturesAreReadable()
{
    // A guard against the whole file passing because CLIMA_SOURCE_DIR is wrong
    // and every fixture is empty — which would make every assertion below
    // compare two default-constructed things and succeed.
    const QStringList names = {
        QStringLiteral("toronto-summer.json"),       QStringLiteral("kampala-precip-spike.json"),
        QStringLiteral("miami-thunder.json"),        QStringLiteral("andes-snow.json"),
        QStringLiteral("svalbard-midnight-sun.json"), QStringLiteral("toronto-ecmwf-gaps.json"),
        QStringLiteral("toronto-dst-fall.json"),     QStringLiteral("toronto-dst-spring.json"),
    };

    for (const QString &name : names) {
        QVERIFY2(fixture(name).size() > 1000, qPrintable(name));
        QVERIFY2(!adapt(name).hourly.isEmpty(), qPrintable(name));
    }
}

// ---------------------------------------------------------------------------
// Toronto, the canonical response
// ---------------------------------------------------------------------------

void TestOpenMeteoAdapter::torontoParsesEveryBlock()
{
    const Forecast forecast = adapt(QStringLiteral("toronto-summer.json"));

    QCOMPARE(forecast.providerId, QStringLiteral("open-meteo"));

    // forecast_days=16 and past_days=1: seventeen days, 408 hours.
    QCOMPARE(forecast.hourly.size(), 408);
    QCOMPARE(forecast.daily.size(), 17);

    // The grid cell, not the coordinate we asked for (43.6532, -79.3832).
    QVERIFY(qAbs(forecast.coordinate.latitude - 43.6466) < 0.01);
    QVERIFY(qAbs(forecast.coordinate.longitude - (-79.3827)) < 0.01);
    QVERIFY(forecast.elevation.has_value());
    QCOMPARE(*forecast.elevation, 99.0);

    QVERIFY(forecast.timeZone.isValid());
    QCOMPARE(forecast.timeZone.id(), QByteArrayLiteral("America/Toronto"));

    // The response labels its first hour "2026-07-30T00:00" with
    // utc_offset_seconds = -14400. That is 04:00 UTC, and it is the instant
    // that gets stored — see libclima/domain/timeaxis.h.
    QCOMPARE(forecast.hourly.first().time, utc(2026, 7, 30, 4));
    QCOMPARE(*forecast.hourly.first().temperature, 17.8);

    // `current` is quarter-hourly and lands between two hourly samples, which
    // is why it is a separate block rather than hour zero.
    QVERIFY(!forecast.current.isEmpty());
    QCOMPARE(forecast.current.time, utc(2026, 7, 31, 9, 15));   // 05:15 EDT
    QCOMPARE(*forecast.current.temperature, 15.5);
    QCOMPARE(*forecast.current.weatherCode, 0);
    QCOMPARE(*forecast.current.isDay, false);

    // Daily is keyed by local calendar date, with no offset arithmetic: a date
    // is not an instant.
    QCOMPARE(forecast.daily.first().date, QDate(2026, 7, 30));

    // 0.533 is just past full, so the disc is very nearly all lit. Reading the
    // phase as the illumination would have said 53%.
    const Reading phase = forecast.daily.first().moonPhase;
    QVERIFY(phase.has_value());
    QCOMPARE(*phase, 0.533);
    QVERIFY(qAbs(*moonIllumination(phase) - 0.989) < 0.01);
    QCOMPARE(moonPhaseName(phase), QStringLiteral("waning-gibbous"));
}

// ---------------------------------------------------------------------------
// Trap 2: units
// ---------------------------------------------------------------------------

void TestOpenMeteoAdapter::visibilityArrivesInMetresAndIsStoredInKilometres()
{
    const Forecast forecast = adapt(QStringLiteral("toronto-summer.json"));

    // The recorded first hour is 30100 in the payload. Left alone it would be
    // plotted against metrics.js's 0–25 km axis, where every hour of every day
    // pins to the top and the Visibility tab becomes a flat line that looks
    // like a working chart of a variable that never changes.
    const Reading visibility = forecast.hourly.first().visibility;
    QVERIFY(visibility.has_value());
    QCOMPARE(*visibility, 30.1);

    // And nothing in the whole series is left in metres. 400 km is well past
    // any real visibility and well under any value that is still in metres.
    for (const HourlyPoint &point : forecast.hourly) {
        if (point.visibility)
            QVERIFY2(*point.visibility < 400.0, qPrintable(point.time.toString(Qt::ISODate)));
    }

    QVERIFY(forecast.current.visibility.has_value());
    QCOMPARE(*forecast.current.visibility, 19.3);
}

void TestOpenMeteoAdapter::snowfallStaysInCentimetresBesidePrecipitationInMillimetres()
{
    // The two units live in the same JSON object — `precipitation` in mm,
    // `snowfall` in cm — and a reader who assumes one unit for the block is
    // out by a factor of ten on the field that is not it.
    const Forecast forecast = adapt(QStringLiteral("andes-snow.json"));

    const HourlyPoint &snowy = forecast.hourly.at(4);   // 2026-07-31T04:00 local
    QVERIFY(snowy.snowfall.has_value());
    QVERIFY(snowy.precipitation.has_value());

    QCOMPARE(*snowy.snowfall, 0.98);        // cm of snow
    QCOMPARE(*snowy.precipitation, 1.40);   // mm of water it came from

    // The point of the assertion: they are different numbers in different
    // units for the same weather, and a mapping that treated them as one would
    // have made them equal.
    QVERIFY(*snowy.snowfall != *snowy.precipitation);
}

// ---------------------------------------------------------------------------
// Absence
// ---------------------------------------------------------------------------

void TestOpenMeteoAdapter::nullsBecomeAbsentReadingsRatherThanZero()
{
    const Forecast forecast = adapt(QStringLiteral("toronto-summer.json"));

    // Index 405 is a null hour in the middle of an otherwise complete series —
    // the sixteenth day, where the model this blend uses runs out before the
    // others do. As zero it would draw a 0 °C spike into an August chart.
    const HourlyPoint &gap = forecast.hourly.at(405);
    QCOMPARE(gap.time, utc(2026, 8, 16, 1));   // 21:00 EDT on the 15th
    QVERIFY(!gap.temperature.has_value());
    QVERIFY(!gap.precipitation.has_value());

    // The hour before it is fine, which is what makes this a gap rather than
    // a truncation.
    QVERIFY(forecast.hourly.at(404).temperature.has_value());

    // The last daily row is null for most of its columns for the same reason.
    QVERIFY(!forecast.daily.at(16).temperatureMax.has_value());
    QVERIFY(forecast.daily.at(15).temperatureMax.has_value());
}

void TestOpenMeteoAdapter::aMoonThatDoesNotRiseIsAbsentAndNotAnError()
{
    const Forecast forecast = adapt(QStringLiteral("toronto-summer.json"));

    // The moon rises roughly fifty minutes later each day, so about once a
    // month it skips a calendar date entirely. Index 7 is that day here.
    QVERIFY(!forecast.daily.at(7).moonrise.isValid());

    // Its neighbours rise normally, and the day itself is otherwise complete:
    // this is one absent event, not a broken row.
    QVERIFY(forecast.daily.at(6).moonrise.isValid());
    QVERIFY(forecast.daily.at(8).moonrise.isValid());
    QVERIFY(forecast.daily.at(7).moonset.isValid());
    QVERIFY(forecast.daily.at(7).temperatureMax.has_value());
}

// ---------------------------------------------------------------------------
// Trap 3: the weather code
// ---------------------------------------------------------------------------

void TestOpenMeteoAdapter::theWmoCodeBecomesAPrecipitationTypeAndAGlyph()
{
    // The six types precip.js draws, from the codes the brief maps them from.
    QCOMPARE(precipitationTypeFor(51), PrecipitationType::Drizzle);
    QCOMPARE(precipitationTypeFor(57), PrecipitationType::Drizzle);
    QCOMPARE(precipitationTypeFor(61), PrecipitationType::Rain);
    QCOMPARE(precipitationTypeFor(82), PrecipitationType::Rain);
    QCOMPARE(precipitationTypeFor(66), PrecipitationType::Sleet);
    QCOMPARE(precipitationTypeFor(67), PrecipitationType::Sleet);
    QCOMPARE(precipitationTypeFor(71), PrecipitationType::Snow);
    QCOMPARE(precipitationTypeFor(86), PrecipitationType::Snow);
    QCOMPARE(precipitationTypeFor(95), PrecipitationType::Thunder);
    QCOMPARE(precipitationTypeFor(96), PrecipitationType::Hail);
    QCOMPARE(precipitationTypeFor(99), PrecipitationType::Hail);

    // Dry codes carry no type at all, so a cell is never built for them.
    QCOMPARE(precipitationTypeFor(0), PrecipitationType::None);
    QCOMPARE(precipitationTypeFor(3), PrecipitationType::None);
    QCOMPARE(precipitationTypeFor(45), PrecipitationType::None);

    // Recorded thunder and hail, from Miami. Passing 96 to precip.js as a
    // number would miss its STYLE table and silently draw ordinary rain.
    const Forecast miami = adapt(QStringLiteral("miami-thunder.json"));
    const HourlyPoint &hail = miami.hourly.at(85);
    QCOMPARE(*hail.weatherCode, 96);
    QCOMPARE(precipitationTypeFor(*hail.weatherCode), PrecipitationType::Hail);
    QCOMPARE(precipitationTypeName(precipitationTypeFor(*hail.weatherCode)),
             QStringLiteral("hail"));

    // Recorded snow showers, from the Andes in July.
    const Forecast andes = adapt(QStringLiteral("andes-snow.json"));
    QCOMPARE(*andes.hourly.at(4).weatherCode, 86);
    QCOMPARE(precipitationTypeFor(86), PrecipitationType::Snow);

    // The glyph is a different question with a different answer, because the
    // same code is a sun at noon and a moon at midnight.
    QCOMPARE(conditionKindName(conditionFor(0, true)), QStringLiteral("clear-day"));
    QCOMPARE(conditionKindName(conditionFor(0, false)), QStringLiteral("clear-night"));
    QCOMPARE(conditionKindName(conditionFor(61, false)), QStringLiteral("rain-night"));

    // And the seven names WeatherGlyph.qml can draw today are closed under
    // drawableToday(), so nothing renders as an empty item.
    const QStringList drawable = { QStringLiteral("clear-day"),   QStringLiteral("clear-night"),
                                   QStringLiteral("partly-day"),  QStringLiteral("partly-night"),
                                   QStringLiteral("cloudy"),      QStringLiteral("rain"),
                                   QStringLiteral("rain-night") };
    for (int code = 0; code <= 99; ++code) {
        for (bool day : { true, false }) {
            const QString name = conditionKindName(drawableToday(conditionFor(code, day)));
            QVERIFY2(drawable.contains(name), qPrintable(QStringLiteral("%1 -> %2")
                                                             .arg(code)
                                                             .arg(name)));
        }
    }
}

void TestOpenMeteoAdapter::everyCodeInTheRecordedResponsesIsRecognised()
{
    // A code with no wording is rendered "—" by the UI, which is honest but
    // useless. Every code the live service has actually been observed emitting
    // must have a phrase; this is the test that goes red when Open-Meteo starts
    // sending one we have not seen.
    const QStringList names = {
        QStringLiteral("toronto-summer.json"), QStringLiteral("miami-thunder.json"),
        QStringLiteral("andes-snow.json"),     QStringLiteral("svalbard-midnight-sun.json"),
        QStringLiteral("kampala-precip-spike.json"),
        QStringLiteral("toronto-dst-fall.json"), QStringLiteral("toronto-dst-spring.json"),
    };

    for (const QString &name : names) {
        const Forecast forecast = adapt(name);
        for (const HourlyPoint &point : forecast.hourly) {
            if (!point.weatherCode)
                continue;
            QVERIFY2(!conditionText(*point.weatherCode, true).isEmpty(),
                     qPrintable(QStringLiteral("%1: no wording for WMO code %2")
                                    .arg(name)
                                    .arg(*point.weatherCode)));
        }
    }
}

// ---------------------------------------------------------------------------
// Trap 1: the precipitation hour. The three tests this whole file is for.
// ---------------------------------------------------------------------------

void TestOpenMeteoAdapter::precipitationIsShiftedOntoTheHourItFallsIn()
{
    // Kampala, recorded because it had the one thing a search of a dozen
    // cities turned up: a single wet hour with dry hours on both sides. An
    // isolated spike is what makes an off-by-one unambiguous — inside a long
    // band, a shift of one hour looks like a band of the same length.
    const Forecast raw = adapt(QStringLiteral("kampala-precip-spike.json"));

    // What the provider sent. `precipitation[10]` is stamped 10:00 local and
    // it is the sum over the hour PRECEDING that stamp — so the rain fell
    // between 09:00 and 10:00.
    QCOMPARE(raw.hourly.size(), 48);
    QCOMPARE(*raw.hourly.at(9).precipitation, 0.00);
    QCOMPARE(*raw.hourly.at(10).precipitation, 0.40);
    QCOMPARE(*raw.hourly.at(11).precipitation, 0.00);
    QCOMPARE(localHour(raw.hourly.at(10).time, raw.timeZone), 10);

    // And what a chart gets: the same rain, on the hour it actually falls in.
    const QList<HourlyPoint> chart = asHourStarting(raw.hourly);

    QCOMPARE(*chart.at(9).precipitation, 0.40);
    QCOMPARE(*chart.at(8).precipitation, 0.00);
    QCOMPARE(*chart.at(10).precipitation, 0.00);

    // precip.js draws the wash for hour i across [i, i+1). So the band starts
    // at the timestamp of the slot that now holds the rain, and that timestamp
    // is 09:00 — the hour a reader deciding whether to go out is asking about.
    QCOMPARE(localHour(chart.at(9).time, raw.timeZone), 9);
    QCOMPARE(chart.at(9).time, utc(2026, 7, 31, 6));   // 09:00 EAT, UTC+3

    // The probability moves with the amount, because it is the probability of
    // that same hour's rain: 30 % was stamped 10:00 and belongs at 09:00.
    QCOMPARE(*chart.at(9).precipitationProbability, 30.0);

    // So does the code, and this is the pairing that matters: precip.js builds
    // one cell out of an amount and a type, and taking them from different
    // hours produces "0.0 mm of drizzle" at the edge of every spell.
    QCOMPARE(*chart.at(9).weatherCode, 51);
    QCOMPARE(precipitationTypeFor(*chart.at(9).weatherCode), PrecipitationType::Drizzle);

    // The instantaneous readings did NOT move. Temperature at 09:00 is still
    // the temperature that was measured at 09:00.
    QCOMPARE(*chart.at(9).temperature, *raw.hourly.at(9).temperature);
    QCOMPARE(chart.at(9).time, raw.hourly.at(9).time);
}

void TestOpenMeteoAdapter::theUnshiftedSeriesWouldPutTheBandAnHourLate()
{
    // The negative control, and the reason it is worth a test of its own: the
    // failure this guards against does not throw, empty anything, or move a
    // pixel that looks wrong. It draws the rain band one column to the right,
    // under a temperature curve that is correct, and the forecast says it
    // starts raining at ten when it starts raining at nine.
    const Forecast raw = adapt(QStringLiteral("kampala-precip-spike.json"));

    const auto firstWetHour = [](const QList<HourlyPoint> &series) {
        for (int i = 0; i < series.size(); ++i) {
            // 0.1 mm is precip.js's TRACE: below it the hour is dry.
            if (series.at(i).precipitation && *series.at(i).precipitation >= 0.1)
                return i;
        }
        return -1;
    };

    const int wrong = firstWetHour(raw.hourly);
    const int right = firstWetHour(asHourStarting(raw.hourly));

    QCOMPARE(wrong, 10);
    QCOMPARE(right, 9);
    QCOMPARE(right, wrong - 1);
}

void TestOpenMeteoAdapter::shiftingCostsExactlyTheLastHour()
{
    const Forecast raw = adapt(QStringLiteral("toronto-summer.json"));
    const QList<HourlyPoint> chart = asHourStarting(raw.hourly);

    // The final sample has no successor, so there is no measured accumulation
    // for the hour it starts. It is dropped rather than filled with an absent
    // Reading: a last column with a real temperature and no rain draws, looks
    // like data, and is the one column where "no rain" means "not asked".
    QCOMPARE(chart.size(), raw.hourly.size() - 1);
    QCOMPARE(chart.last().time, raw.hourly.at(raw.hourly.size() - 2).time);

    // Degenerate inputs do not produce a series of length -1.
    QVERIFY(asHourStarting(QList<HourlyPoint>{}).isEmpty());
    QVERIFY(asHourStarting(QList<HourlyPoint>{ HourlyPoint{} }).isEmpty());
}

// ---------------------------------------------------------------------------
// The fourth trap, which was not in the plan: timezone=auto is a constant
// ---------------------------------------------------------------------------

void TestOpenMeteoAdapter::openMeteoLabelsADstDayWithTwentyFourHours()
{
    // This is the test that documents the defect rather than the fix, and it
    // asserts on the raw bytes because the whole point is what the provider
    // said before we corrected it.
    //
    // Both recorded responses cross a real DST transition in Toronto. Both
    // carry utc_offset_seconds = -14400 for every row, and both label their
    // transition day with exactly 24 hours — 01:00 once on the day it happens
    // twice, and 02:00 present on the day it does not exist.
    // Counted out of `hourly.time` itself rather than out of the bytes: the
    // daily block stamps sunrise and sunset with the same date prefix, and a
    // byte count would quietly be measuring those too.
    const auto labelsOn = [](const QString &name, const QString &prefix) {
        const QJsonObject hourly = QJsonDocument::fromJson(fixture(name))
                                       .object()
                                       .value(QLatin1String("hourly"))
                                       .toObject();
        int count = 0;
        for (const QJsonValue &stamp : hourly.value(QLatin1String("time")).toArray()) {
            if (stamp.toString().startsWith(prefix))
                ++count;
        }
        return count;
    };

    for (const QString &name : { QStringLiteral("toronto-dst-fall.json"),
                                 QStringLiteral("toronto-dst-spring.json") }) {
        const QJsonObject root = QJsonDocument::fromJson(fixture(name)).object();

        // One offset, EDT, applied to every row — including the rows that are
        // in EST. That single number is the whole defect.
        QCOMPARE(root.value(QLatin1String("utc_offset_seconds")).toInt(), -14400);
    }

    QCOMPARE(labelsOn(QStringLiteral("toronto-dst-fall.json"), QStringLiteral("2025-11-02T")), 24);
    QCOMPARE(labelsOn(QStringLiteral("toronto-dst-spring.json"), QStringLiteral("2026-03-08T")), 24);

    // And the specific consequences: 01:00 appears once on the day it happens
    // twice, and 02:00 appears on the day it does not exist at all.
    const QByteArray fall = fixture(QStringLiteral("toronto-dst-fall.json"));
    QCOMPARE(fall.count("\"2025-11-02T01:00\""), 1);
    QVERIFY(fixture(QStringLiteral("toronto-dst-spring.json")).contains("\"2026-03-08T02:00\""));
}

void TestOpenMeteoAdapter::theLocalAxisHasTwentyFiveSlotsOnAFallBackDay()
{
    const Forecast forecast = adapt(QStringLiteral("toronto-dst-fall.json"));
    QCOMPARE(forecast.timeZone.id(), QByteArrayLiteral("America/Toronto"));

    QList<QDateTime> instants;
    for (const HourlyPoint &point : forecast.hourly)
        instants.append(point.time);

    // 2025-11-02: the clocks go back at 02:00 EDT, so the day is 25 hours long
    // and 01:00 happens twice. An axis built as `startHour + i` cannot produce
    // this, and an axis built from Open-Meteo's own labels gets 24.
    const QList<int> onTheDay =
        indicesOnLocalDate(instants, forecast.timeZone, QDate(2025, 11, 2));
    QCOMPARE(onTheDay.size(), 25);

    QList<int> hours;
    for (int index : onTheDay)
        hours.append(localHour(instants.at(index), forecast.timeZone));

    QCOMPARE(hours.first(), 0);
    QCOMPARE(hours.last(), 23);
    QCOMPARE(hours.count(1), 2);   // the repeated hour
    QCOMPARE(hours.count(2), 1);

    // The day before is an ordinary 24 hours, so this is a property of the
    // transition rather than of the parser.
    QCOMPARE(indicesOnLocalDate(instants, forecast.timeZone, QDate(2025, 11, 1)).size(), 24);
}

void TestOpenMeteoAdapter::theLocalAxisHasTwentyThreeSlotsOnASpringForwardDay()
{
    const Forecast forecast = adapt(QStringLiteral("toronto-dst-spring.json"));

    QList<QDateTime> instants;
    for (const HourlyPoint &point : forecast.hourly)
        instants.append(point.time);

    // 2026-03-08: the clocks go forward at 02:00 EST, so 02:00 never happens
    // and the day is 23 hours long.
    const QList<int> onTheDay =
        indicesOnLocalDate(instants, forecast.timeZone, QDate(2026, 3, 8));
    QCOMPARE(onTheDay.size(), 23);

    QList<int> hours;
    for (int index : onTheDay)
        hours.append(localHour(instants.at(index), forecast.timeZone));

    QCOMPARE(hours.count(1), 1);
    QCOMPARE(hours.count(2), 0);   // the hour that does not exist
    QCOMPARE(hours.count(3), 1);

    QCOMPARE(indicesOnLocalDate(instants, forecast.timeZone, QDate(2026, 3, 9)).size(), 24);
}

void TestOpenMeteoAdapter::sunTimesSurviveTheZoneCorrection()
{
    // Toronto in July: no transition in the window, so the provider's labels
    // and ours agree, and detaildata.js's minutes-past-midnight convention
    // survives untouched. 06:04 is 364 minutes.
    const Forecast summer = adapt(QStringLiteral("toronto-summer.json"));
    const DailyPoint &day = summer.daily.first();

    QCOMPARE(day.sunrise, utc(2026, 7, 30, 10, 4));    // 06:04 EDT
    QCOMPARE(day.sunset, utc(2026, 7, 31, 0, 42));     // 20:42 EDT
    QCOMPARE(minutesFromLocalMidnight(day.sunrise, summer.timeZone, day.date), 6 * 60 + 4);
    QCOMPARE(minutesFromLocalMidnight(day.sunset, summer.timeZone, day.date), 20 * 60 + 42);

    // And the correction the fourth trap needs. On 2025-11-02 the payload says
    // sunrise is at 07:55; Toronto was on EST that morning and the sun rose at
    // 06:55. Read as an instant and re-expressed in the real zone, we get the
    // right one — which is the difference between a Sun card that is right and
    // one that is an hour out for four months of the year.
    const Forecast fall = adapt(QStringLiteral("toronto-dst-fall.json"));
    const DailyPoint &transition = fall.daily.at(1);
    QCOMPARE(transition.date, QDate(2025, 11, 2));
    QCOMPARE(minutesFromLocalMidnight(transition.sunrise, fall.timeZone, transition.date),
             6 * 60 + 55);

    // The day before the transition is unaffected, so the correction is a
    // correction and not a blanket shift.
    const DailyPoint &before = fall.daily.at(0);
    QCOMPARE(minutesFromLocalMidnight(before.sunrise, fall.timeZone, before.date),
             7 * 60 + 54);
}

// ---------------------------------------------------------------------------
// The edges of the world
// ---------------------------------------------------------------------------

void TestOpenMeteoAdapter::midnightSunIsAFullArcRatherThanAnEmptyOne()
{
    const Forecast forecast = adapt(QStringLiteral("svalbard-midnight-sun.json"));
    const DailyPoint &day = forecast.daily.first();

    QCOMPARE(day.date, QDate(2026, 7, 31));

    // Open-Meteo answers a polar summer day with sunrise at that day's
    // midnight and sunset at the NEXT day's midnight. Both are valid
    // timestamps; asked for minutes past its own midnight, that sunset would
    // answer 0 and a Sun card would draw an arc of zero length on the one day
    // of the year the sun never sets. Measured from one reference it answers
    // 1440 and the arc is full.
    QVERIFY(day.sunrise.isValid());
    QVERIFY(day.sunset.isValid());
    QCOMPARE(minutesFromLocalMidnight(day.sunrise, forecast.timeZone, day.date), 0);
    QCOMPARE(minutesFromLocalMidnight(day.sunset, forecast.timeZone, day.date), 1440);

    // And the field that says which case you are in without any arithmetic.
    QVERIFY(day.daylightSeconds.has_value());
    QCOMPARE(*day.daylightSeconds, 86400.0);

    // Absent events are still absent up here.
    QVERIFY(!day.moonrise.isValid());
    QVERIFY(forecast.daily.at(1).moonrise.isValid());
}

void TestOpenMeteoAdapter::aModelWithoutUvOrVisibilityLeavesThoseColumnsEmpty()
{
    // The same endpoint, the same parameters, `models=ecmwf_ifs025`. IFS does
    // not carry UV or visibility, so both columns are null for all 72 hours —
    // which is a different fact from "no value this hour", and it is the fact
    // that decides whether a metric tab is drawn at all.
    const Forecast forecast = adapt(QStringLiteral("toronto-ecmwf-gaps.json"));

    QCOMPARE(forecast.hourly.size(), 72);

    int uv = 0;
    int visibility = 0;
    int temperature = 0;
    for (const HourlyPoint &point : forecast.hourly) {
        uv += point.uvIndex ? 1 : 0;
        visibility += point.visibility ? 1 : 0;
        temperature += point.temperature ? 1 : 0;
    }

    QCOMPARE(uv, 0);
    QCOMPARE(visibility, 0);
    QCOMPARE(temperature, 72);   // the model is fine, it simply lacks two fields

    // A whole column of nulls must not be mistaken for a failed parse.
    QVERIFY(!forecast.daily.isEmpty());
    QVERIFY(forecast.daily.first().temperatureMax.has_value());
    QVERIFY(!forecast.daily.first().uvIndexMax.has_value());
}

// ---------------------------------------------------------------------------
// Failure
// ---------------------------------------------------------------------------

void TestOpenMeteoAdapter::malformedInputIsAParseErrorAndNotAnEmptyForecast()
{
    const QString provider = QStringLiteral("open-meteo");

    const auto fails = [&provider](const QByteArray &body) {
        const Result<Forecast> result = openmeteo::adaptForecast(body, provider);
        QVERIFY(!result.hasValue());
        QCOMPARE(result.errorKind(), ErrorKind::Parse);
        QCOMPARE(result.error().providerId(), provider);
    };

    fails(QByteArray());
    fails(QByteArrayLiteral("not json at all"));
    fails(QByteArrayLiteral("[1, 2, 3]"));

    // Open-Meteo's own rejection envelope, which parses cleanly as JSON and
    // has none of the blocks we look for. Read first, so that a bad parameter
    // reports the reason the service gave rather than "no hourly series".
    const Result<Forecast> rejected = openmeteo::adaptForecast(
        QByteArrayLiteral(R"({"error":true,"reason":"Cannot initialize WeatherVariable"})"),
        provider);
    QVERIFY(!rejected.hasValue());
    QVERIFY(rejected.error().message().contains(QStringLiteral("WeatherVariable")));

    // A response with no hourly series at all.
    fails(QByteArrayLiteral(R"({"latitude":43.6,"longitude":-79.4,"utc_offset_seconds":0})"));

    // Ragged columns. Never observed from the live service, and rejected
    // rather than truncated: a series silently cut to its shortest column ends
    // at an hour that moves with whichever variable was short, which is
    // unreproducible by the time anyone reports it.
    fails(QByteArrayLiteral(
        R"({"utc_offset_seconds":0,"timezone":"UTC","hourly":{)"
        R"("time":["2026-07-30T00:00","2026-07-30T01:00","2026-07-30T02:00"],)"
        R"("temperature_2m":[17.8,18.0]}})"));

    // And the same payload with equal lengths parses, so the rejection above
    // is about the raggedness and not about the shape of the fragment.
    const Result<Forecast> tidy = openmeteo::adaptForecast(
        QByteArrayLiteral(
            R"({"utc_offset_seconds":0,"timezone":"UTC","hourly":{)"
            R"("time":["2026-07-30T00:00","2026-07-30T01:00","2026-07-30T02:00"],)"
            R"("temperature_2m":[17.8,18.0,18.4]}})"),
        provider);
    QVERIFY(tidy.hasValue());
    QCOMPARE(tidy.value().hourly.size(), 3);
}

QTEST_MAIN(TestOpenMeteoAdapter)
#include "tst_openmeteoadapter.moc"
