// SPDX-FileCopyrightText: 2026 Jowi Aoun
// SPDX-License-Identifier: GPL-3.0-or-later
//
// Which hours the chart is looking at, and which hour is "now".
//
// ============================================================================
// WHAT THIS TEST IS ABOUT
//
// The day strip was wired to nothing. Every card selected, the selected card
// grew into the panel below it and merged with it, and the panel went on
// drawing today — because `ForecastData` published one window, fixed around the
// present, and nothing could ask it for another. Picking Friday changed the
// picture and not the data, which is the worst kind of broken control: it
// answers.
//
// So the window moved, and the properties that describe it changed meaning
// with it. This asserts the new meanings, because they are not obvious and two
// of them are deliberately allowed to look wrong:
//
//   * `nowIndex` may fall OUTSIDE [0, count). It is an offset to the present,
//     not an index into the window, and that is what makes the chart's past
//     veil correct on every day with no branch in it — `xForIndex(nowIndex)`
//     wide, so a day still ahead veils nothing and a day gone veils everything.
//
//   * `ahead()` indexes the whole series from the present and is unaffected by
//     any of it. The phone's Today screen and the Hourly screen's reading both
//     mean "right now", and both said `nowIndex` and meant it until the window
//     could be Friday's.
//
// ============================================================================
// WHY IT LINKS app/
//
// The same reason tst_conditionsdata does, and tests/CMakeLists.txt has the
// argument at length: the subject is a view model, which lives in app/, is
// GPL-3.0-or-later rather than MPL-2.0, and is allowed to depend on QML because
// being read by QML is its whole job. `clima_forbid_gui()` is not applied here
// and libclima's no-GUI promise is still read off tst_httpclient.

#include "forecastdata.h"

#include "libclima/providers/fixture/fixtureprovider.h"

#include <QDate>
#include <QSet>
#include <QSignalSpy>
#include <QStandardPaths>
#include <QStringList>
#include <QTest>
#include <QTime>
#include <QTimeZone>

using namespace clima;

namespace {

// The six glyph names that are only a statement about how much sky is showing.
// Everything else in `ConditionKind` is a thing happening — fog, something
// falling, lightning — and the difference is the line
// libclima/domain/weathercode.h folds a labelled column across.
bool isSky(const QString &kind)
{
    static const QSet<QString> sky = {
        QStringLiteral("clear-day"),  QStringLiteral("clear-night"),
        QStringLiteral("partly-day"), QStringLiteral("partly-night"),
        QStringLiteral("cloudy"),
    };
    return sky.contains(kind);
}

// Four days of overcast with one thunderstorm in it, stamped at `stormHour` on
// the third day. UTC throughout, so a local hour and an index are the same
// number and the test can say which column it means.
//
// Four days and not three because libclima/domain/hourconvention.h's shift
// costs the series its last hour: day 2 keeps its 11 p.m. only if day 3 exists
// to supply it.
Forecast oneStormyHour(int stormHour)
{
    const QDate first(2026, 8, 20);

    Forecast forecast;
    forecast.timeZone   = QTimeZone::UTC;
    forecast.providerId = QStringLiteral("test");

    for (int day = 0; day < 4; ++day) {
        const QDate date = first.addDays(day);

        DailyPoint daily;
        daily.date           = date;
        daily.temperatureMax = 20.0;
        daily.temperatureMin = 10.0;
        daily.weatherCode    = day == 2 ? 95 : 3;
        forecast.daily.append(daily);

        for (int hour = 0; hour < 24; ++hour) {
            HourlyPoint point;
            point.time        = QDateTime(date, QTime(hour, 0), QTimeZone::UTC);
            point.temperature = 15.0;
            point.isDay       = hour >= 6 && hour < 20;
            point.weatherCode = (day == 2 && hour == stormHour) ? 95 : 3;
            forecast.hourly.append(point);
        }
    }

    return forecast;
}

} // namespace

class TestForecastData : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void initTestCase();

    void aFreshSnapshotOpensOnToday();
    void todaysWindowIsTheOneAroundNow();
    void anotherDayIsThatDayFromMidnight();
    void everyDayInTheStripSelectsRealHours();

    void nowIsBehindAWindowThatHasNotStartedYet();
    void nowIsAheadOfAWindowThatIsOver();
    void onlyTodayHasAColumnCalledNow();

    void aheadIsCountedFromThePresentAndNotFromTheWindow();
    void aheadRunsOutRatherThanWrappingRound();

    void anOutOfRangeSelectionIsClampedRatherThanObeyed();
    void reselectingTheSameDayRebuildsNothing();

    void everyLabelledColumnHasAGlyph();
    void aColumnNeverDrawsPlainSkyOverAnHourWithWeatherInIt();
    void aThunderstormOnAColumnTheBandSkipsIsStillDrawn();
    void theLastLabelDoesNotReadPastItsOwnDay();

private:
    // One fixture, loaded once: Toronto is the one every capture uses and its
    // recorded instant is midday, which is the only interesting case — a
    // fixture recorded at 00:30 would make "today's window" and "today from
    // midnight" almost the same window and prove nothing.
    Fixture   m_fixture;
    Forecast  m_forecast;
    AirQuality m_air;

    // The zone the fixture's own timestamps are in, which is not the machine's.
    [[nodiscard]] QTimeZone zone() const;

    void load(ForecastData &data);
};

QTimeZone TestForecastData::zone() const
{
    if (m_forecast.timeZone.isValid())
        return m_forecast.timeZone;
    return QTimeZone(m_fixture.place.timezone.toUtf8());
}

void TestForecastData::initTestCase()
{
    // ForecastData reaches Units, which is a QSettings away from the
    // developer's own preferences. Same guard tst_conditionsdata sets.
    QStandardPaths::setTestModeEnabled(true);

    m_fixture = fixtures::load(QStringLiteral("toronto"));
    QVERIFY(m_fixture.isValid());

    ForecastRequest request;
    request.coord = m_fixture.place.coordinate;

    FixtureForecastProvider   forecasts(m_fixture);
    FixtureAirQualityProvider air(m_fixture);

    const QFuture<Result<Forecast>>   forecastFuture = forecasts.fetchForecast(request);
    const QFuture<Result<AirQuality>> airFuture      = air.fetchAirQuality(request);
    QVERIFY(forecastFuture.isFinished());
    QVERIFY(airFuture.isFinished());

    QVERIFY(forecastFuture.result().hasValue());
    QVERIFY(airFuture.result().hasValue());
    m_forecast = forecastFuture.result().value();
    m_air      = airFuture.result().value();

    QVERIFY(!m_forecast.hourly.isEmpty());
    QVERIFY(!m_forecast.daily.isEmpty());
}

void TestForecastData::load(ForecastData &data)
{
    data.setSnapshot(m_forecast, m_air, m_fixture.recordedAt, m_fixture.place);
    QVERIFY(data.count() > 0);
    QVERIFY(!data.days().isEmpty());
}

// ---- what the window is -----------------------------------------------------

void TestForecastData::aFreshSnapshotOpensOnToday()
{
    ForecastData data(nullptr);
    load(data);

    QCOMPARE(data.selectedDay(), data.todayIndex());
    QVERIFY(data.nowInWindow());
}

void TestForecastData::todaysWindowIsTheOneAroundNow()
{
    ForecastData data(nullptr);
    load(data);

    // 48 hours with 15 of them behind — the window this class has always drawn,
    // asserted here so that the day-aware branch cannot quietly change it. A
    // provider with less than 15 hours of past would land on fewer, which is
    // why this is a bound rather than an equality.
    QCOMPARE(data.count(), 48);
    QVERIFY(data.nowIndex() > 0);
    QVERIFY(data.nowIndex() <= 15);
    QVERIFY(data.nowInWindow());
}

void TestForecastData::anotherDayIsThatDayFromMidnight()
{
    ForecastData data(nullptr);
    load(data);

    const int tomorrow = data.todayIndex() + 1;
    QVERIFY(tomorrow < data.days().size());

    data.setSelectedDay(tomorrow);

    // A day, and a whole one. 24 rather than 48: a chart of Friday that opens
    // on Thursday evening is a chart of Friday you have to go and find.
    QCOMPARE(data.count(), 24);
    QCOMPARE(data.startHour(), 0);

    // The first label is one column in, not on the edge. The header band
    // centres a two-column entry on each label, and a day window opens at column
    // 0 with nowhere further left to go — so a label in column 0 is a label
    // sliced down the middle by the plot's clip. Today's window puts its first
    // one column inside the edge for the same reason.
    QCOMPARE(data.firstLabelIndex(), 1);
    QVERIFY(!data.labelIndices().isEmpty());
    QCOMPARE(data.labelIndices().first().toInt(), 1);

    // And it is the RIGHT twenty-four hours, checked against the window it
    // came out of rather than against this class's own arithmetic repeated.
    //
    // Today's window runs from fifteen hours before the fixture's midday to
    // thirty-two after it, so it already contains most of tomorrow. The hours
    // tomorrow's window publishes have to be the same readings, at the offset
    // where tomorrow's midnight falls in today's. If the day lookup were off by
    // one — a UTC date compared against a local one is the obvious way — this
    // is the assertion that says so.
    const QDateTime nowLocal = m_fixture.recordedAt.toTimeZone(zone());
    const int       toMidnight = 24 - nowLocal.time().hour();

    data.setSelectedDay(data.todayIndex());
    const QVariantList todays = data.temperature();
    const int          offset = data.nowIndex() + toMidnight;

    data.setSelectedDay(tomorrow);
    const QVariantList tomorrows = data.temperature();

    const int overlap = qMin(int(tomorrows.size()), int(todays.size()) - offset);
    QVERIFY(overlap > 12);
    for (int i = 0; i < overlap; ++i)
        QCOMPARE(tomorrows.at(i), todays.at(offset + i));
}

void TestForecastData::everyDayInTheStripSelectsRealHours()
{
    ForecastData data(nullptr);
    load(data);

    // The strip draws a card per row of `days`, and every one of them is
    // tappable. A row whose date is past the hourly horizon has to fall back to
    // something rather than to an empty chart — MET Norway's daily series
    // outruns its hourly one by days, and a blank panel under a card that
    // lights up is the same defect this whole change is about.
    const int rows = int(data.days().size());
    QVERIFY(rows > 2);

    for (int i = 0; i < rows; ++i) {
        data.setSelectedDay(i);
        QVERIFY2(data.count() > 0,
                 qPrintable(QStringLiteral("day %1 selects an empty window").arg(i)));
        QVERIFY2(!data.temperature().isEmpty(),
                 qPrintable(QStringLiteral("day %1 selects hours with no series").arg(i)));
        QCOMPARE(data.temperature().size(), data.count());
        QCOMPARE(data.precipProb().size(), data.count());
        QCOMPARE(data.precipTypes().size(), data.count());
    }
}

// ---- where "now" is ---------------------------------------------------------

void TestForecastData::nowIsBehindAWindowThatHasNotStartedYet()
{
    ForecastData data(nullptr);
    load(data);

    const int ahead = data.todayIndex() + 2;
    QVERIFY(ahead < data.days().size());
    data.setSelectedDay(ahead);

    // Negative, and that is the answer rather than an error. The chart's past
    // veil is this many columns wide, so a day that has not happened is a day
    // with nothing veiled.
    QVERIFY2(data.nowIndex() < 0,
             qPrintable(QStringLiteral("nowIndex is %1 on a day two days ahead")
                            .arg(data.nowIndex())));
    QVERIFY(!data.nowInWindow());
}

void TestForecastData::nowIsAheadOfAWindowThatIsOver()
{
    ForecastData data(nullptr);
    load(data);

    // Yesterday, which the strip carries as its first card.
    QVERIFY(data.todayIndex() > 0);
    data.setSelectedDay(data.todayIndex() - 1);

    QVERIFY2(data.nowIndex() >= data.count(),
             qPrintable(QStringLiteral("nowIndex is %1 in a window of %2 on yesterday")
                            .arg(data.nowIndex())
                            .arg(data.count())));
    QVERIFY(!data.nowInWindow());
}

void TestForecastData::onlyTodayHasAColumnCalledNow()
{
    ForecastData data(nullptr);
    load(data);

    const QString now = data.hourLabel(data.nowIndex());
    QVERIFY(!now.isEmpty());

    // The label is the one thing that must not fall out of a comparison of two
    // integers: on a day window `nowIndex` is an offset that can be any number,
    // and 0 == 0 would print "Now" over Friday midnight on the afternoon the
    // arithmetic happened to agree.
    for (int day = 0; day < data.days().size(); ++day) {
        data.setSelectedDay(day);
        if (data.nowInWindow())
            continue;
        for (int i = 0; i < data.count(); ++i)
            QVERIFY2(data.hourLabel(i) != now,
                     qPrintable(QStringLiteral("day %1 column %2 is labelled \"%3\"")
                                    .arg(day)
                                    .arg(i)
                                    .arg(now)));
    }
}

// ---- and what is not affected by any of it ----------------------------------

void TestForecastData::aheadIsCountedFromThePresentAndNotFromTheWindow()
{
    ForecastData data(nullptr);
    load(data);

    // Recorded while the window is today's…
    QVERIFY(data.nowInWindow());
    QVariantList expected;
    for (int i = 0; i < 12; ++i)
        expected.append(data.ahead(i));
    QCOMPARE(data.ahead(0).value(QStringLiteral("label")).toString(), QStringLiteral("Now"));

    // …and identical after the chart has been sent three days away. This is
    // the phone's Today screen: it shows the next twelve hours and it must go
    // on showing the next twelve hours while the Hourly tab is looking at
    // Friday.
    data.setSelectedDay(data.todayIndex() + 3 < data.days().size() ? data.todayIndex() + 3
                                                                  : data.todayIndex() + 1);
    QVERIFY(!data.nowInWindow());

    for (int i = 0; i < 12; ++i)
        QCOMPARE(data.ahead(i), expected.at(i).toMap());
}

void TestForecastData::aheadRunsOutRatherThanWrappingRound()
{
    ForecastData data(nullptr);
    load(data);

    QVERIFY(data.aheadCount() > 0);
    QVERIFY(!data.ahead(data.aheadCount() - 1).isEmpty());

    // Past the end is an empty map, which QML reads as `undefined` per key —
    // a column with nothing in it, rather than the first hour of the series
    // wearing tomorrow's label.
    QVERIFY(data.ahead(data.aheadCount()).isEmpty());
    QVERIFY(data.ahead(-1000).isEmpty());
}

// ---- the setter -------------------------------------------------------------

void TestForecastData::anOutOfRangeSelectionIsClampedRatherThanObeyed()
{
    ForecastData data(nullptr);
    load(data);

    const int last = int(data.days().size()) - 1;

    data.setSelectedDay(9999);
    QCOMPARE(data.selectedDay(), last);
    QVERIFY(data.count() > 0);

    data.setSelectedDay(-4);
    QCOMPARE(data.selectedDay(), 0);
    QVERIFY(data.count() > 0);
}

void TestForecastData::reselectingTheSameDayRebuildsNothing()
{
    ForecastData data(nullptr);
    load(data);

    QSignalSpy spy(&data, &ForecastData::selectedDayChanged);
    data.setSelectedDay(data.selectedDay());
    QCOMPARE(spy.count(), 0);

    // And a rebuild really is a rebuild rather than an append — every builder
    // downstream of the window appends to a list, so selecting a day twice over
    // is where a missing clear shows up as a doubled series.
    const int day = data.todayIndex() + 1 < data.days().size() ? data.todayIndex() + 1 : 0;
    data.setSelectedDay(day);
    const int count = data.count();
    const int marks = int(data.sunEvents().size());
    const int buckets = int(data.precipBuckets().size());

    data.setSelectedDay(data.todayIndex());
    data.setSelectedDay(day);

    QCOMPARE(data.count(), count);
    QCOMPARE(data.temperature().size(), count);
    QCOMPARE(data.sunEvents().size(), marks);
    QCOMPARE(data.precipBuckets().size(), buckets);
}

// ---- the header band's glyphs -----------------------------------------------
//
// The band draws one icon per *label*, and it labels every second column,
// because two dozen 27 px glyphs will not fit across a plot. For as long as
// each label asked `conditionFor` about the single hour it happened to land on,
// half of every day had no icon anywhere — and which half was arbitrary, since
// a day window's labels start at column 1 while today's take their phase from
// where the present fell. The visible result was a ten-day card that said
// thunderstorm above an hourly row that said rain all evening.

void TestForecastData::everyLabelledColumnHasAGlyph()
{
    ForecastData data(nullptr);
    load(data);

    for (int day = 0; day < data.days().size(); ++day) {
        data.setSelectedDay(day);
        const QVariantList labels = data.labelIndices();
        QVERIFY(!labels.isEmpty());

        for (const QVariant &label : labels) {
            QVERIFY2(!data.conditionForLabel(label.toInt()).isEmpty(),
                     qPrintable(QStringLiteral("day %1 column %2 has no glyph")
                                    .arg(day).arg(label.toInt())));
        }
    }
}

void TestForecastData::aColumnNeverDrawsPlainSkyOverAnHourWithWeatherInIt()
{
    // The property behind the fix, stated as a property. A column may show a
    // different glyph from one of its hours — two events in one span, and the
    // louder wins — but it may never show *sky* over a span with weather in it,
    // because that is the case where a reader is told nothing is happening.
    ForecastData data(nullptr);
    load(data);

    for (int day = 0; day < data.days().size(); ++day) {
        data.setSelectedDay(day);
        const QVariantList labels = data.labelIndices();

        for (int n = 0; n < labels.size(); ++n) {
            const int from = labels.at(n).toInt();
            const int to   = n + 1 < labels.size() ? labels.at(n + 1).toInt() : data.count();

            const QString drawn = data.conditionForLabel(from);
            if (!isSky(drawn))
                continue;

            for (int hour = from; hour < to; ++hour) {
                const QString covered = data.conditionFor(hour);
                QVERIFY2(covered.isEmpty() || isSky(covered),
                         qPrintable(QStringLiteral("day %1 column %2 draws %3 over hour %4, "
                                                   "which is %5")
                                        .arg(day).arg(from).arg(drawn).arg(hour).arg(covered)));
            }
        }
    }
}

void TestForecastData::aThunderstormOnAColumnTheBandSkipsIsStillDrawn()
{
    // The reported bug, built rather than borrowed. The Toronto fixture cannot
    // catch this: its thunderstorm runs 3 p.m. to 6 p.m., four consecutive
    // hours, so it lands on a labelled column whatever the phase and the band
    // drew it even when the band was wrong.
    //
    // One hour is what it takes. A day window labels columns 1, 3, 5 … so the
    // even columns are the ones nothing asks about, and column 12 is the hour
    // starting at noon — WMO 95 stamped 1 p.m. under Open-Meteo's convention.
    // Before this fix that storm had no glyph anywhere in its own day.
    const Forecast forecast = oneStormyHour(13);

    ForecastData data(nullptr);
    data.setSnapshot(forecast, AirQuality(), forecast.hourly.at(36).time, Place());

    // Yesterday, today, tomorrow, and the spare day the shift needs. Tomorrow
    // is the day the storm is on.
    QCOMPARE(data.days().size(), 4);
    data.setSelectedDay(2);
    QCOMPARE(data.days().at(2).toMap().value(QStringLiteral("label")).toString(),
             QStringLiteral("Tomorrow"));
    QCOMPARE(data.count(), 24);

    QVERIFY2(!data.labelIndices().contains(12),
             "column 12 is labelled after all — this test no longer tests anything");
    QCOMPARE(data.conditionFor(12), QStringLiteral("thunder"));

    bool drawn = false;
    for (const QVariant &label : data.labelIndices()) {
        if (data.conditionForLabel(label.toInt()) == QLatin1String("thunder"))
            drawn = true;
    }
    QVERIFY2(drawn, "the day's only thunderstorm has no glyph in the day's own band");
}

void TestForecastData::theLastLabelDoesNotReadPastItsOwnDay()
{
    // The final column spans fewer hours than the step, and reading the hour
    // after it would put tomorrow morning on tonight's last glyph.
    ForecastData data(nullptr);
    load(data);

    for (int day = 0; day < data.days().size(); ++day) {
        data.setSelectedDay(day);
        const QVariantList labels = data.labelIndices();
        QVERIFY(!labels.isEmpty());

        const int last = labels.constLast().toInt();
        QVERIFY(last < data.count());

        QSet<QString> mine;
        for (int i = last; i < data.count(); ++i)
            mine.insert(data.conditionFor(i));

        const QString drawn = data.conditionForLabel(last);
        QVERIFY2(drawn.isEmpty() || mine.contains(drawn),
                 qPrintable(QStringLiteral("day %1 last column draws %2; its own hours are %3")
                                .arg(day)
                                .arg(drawn, QStringList(mine.values()).join(QLatin1Char(',')))));
    }
}

QTEST_MAIN(TestForecastData)

#include "tst_forecastdata.moc"
