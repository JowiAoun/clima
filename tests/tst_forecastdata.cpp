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
#include <QSignalSpy>
#include <QStandardPaths>
#include <QTest>

using namespace clima;

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

QTEST_MAIN(TestForecastData)

#include "tst_forecastdata.moc"
