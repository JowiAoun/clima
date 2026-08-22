// SPDX-FileCopyrightText: 2026 Jowi Aoun
// SPDX-License-Identifier: GPL-3.0-or-later
//
// The variable lists, closed against the responses they produce.
//
// libclima/providers/openmeteo/openmeteovariables.h opens by naming the failure
// this file exists to catch:
//
//     "Every name below is asked for in a query string and read out of a JSON
//      object by the same spelling. Split across two files they drift the moment
//      somebody adds a variable to one of them, and the symptom is a column of
//      absent Readings for a variable the response does not contain — which
//      looks exactly like a provider that does not have it at that location,
//      which is a thing that genuinely happens."
//
// That last clause is the whole problem. `toronto-ecmwf-gaps.json` is a real
// recorded payload with 72 hours of null UV beside a complete temperature
// series, and the app is *required* to render that as "no UV tab here". So a
// misspelled request produces a screen the app also produces legitimately, and
// there is no assertion anywhere in the parser that could tell them apart.
//
// The only place they can be told apart is here, against a payload recorded
// from the live service with the current lists: every name we ask for came
// back. `tests/fixtures/openmeteo/record.sh` is what produced them, so a
// fixture and the query that made it are the same fact.
//
// ---- and one rule that is not about spelling --------------------------------
//
// `current=` may not name a variable Open-Meteo does not serve as a current
// value. That is not a missing field — it is a 400 for the entire request, so
// one wrong name in that list takes the forecast, the chart, the ten-day strip
// and every detail card down together. The subset check below is what stops a
// variable being added to `current` because it looked useful in `hourly`.

#include "libclima/providers/openmeteo/openmeteovariables.h"

#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSet>
#include <QTest>

using namespace clima::openmeteo;

namespace {

QJsonObject fixture(const QString &name)
{
    QFile file(QStringLiteral(CLIMA_SOURCE_DIR "/tests/fixtures/openmeteo/") + name);
    if (!file.open(QIODevice::ReadOnly))
        return {};
    return QJsonDocument::fromJson(file.readAll()).object();
}

QSet<QString> asSet(const QList<QLatin1String> &names)
{
    QSet<QString> out;
    for (const QLatin1String &name : names)
        out.insert(QString(name));
    return out;
}

// Every fixture recorded from /v1/forecast, and only those.
//
// `toronto-dst-fall.json` and `toronto-dst-spring.json` are deliberately not
// here. They come from archive-api.open-meteo.com — ERA5 reanalysis — because
// the forecast endpoint only reaches 92 days back and Toronto's DST
// transitions are outside that window. The archive serves a different and
// smaller set of variables: no `precipitation_probability`, no
// `precipitation_probability_max`, no UV, no moon, because a reanalysis of a
// day that has already happened has no probability to report.
//
// tests/fixtures/openmeteo/record.sh says so in its own two variable lists,
// `archive_hourly` and `archive_daily`, and those fixtures exist to test the
// hour arithmetic across a DST boundary rather than the variable lists. Asking
// them this file's question would be asking the archive to answer for the
// forecast.
//
// The remaining six ARE witnesses: record.sh builds their query strings from
// the same lists this file is testing, so each one is a recording of those
// names being accepted and answered by the live service. The fixtures contain
// MORE than we ask for — `snow_depth`, the three cloud-cover layers,
// `surface_pressure` — which is deliberate and is why the closure only runs in
// one direction: everything asked for came back, not everything that came back
// was asked for.
QStringList forecastFixtures()
{
    return {
        QStringLiteral("toronto-summer.json"),
        QStringLiteral("kampala-precip-spike.json"),
        QStringLiteral("miami-thunder.json"),
        QStringLiteral("andes-snow.json"),
        QStringLiteral("svalbard-midnight-sun.json"),
        // 72 hours of null UV and null visibility beside a complete temperature
        // series. The KEYS are all present, which is exactly the distinction
        // this file is drawing: "no such variable here" arrives as a column of
        // nulls, and a misspelled request arrives as no column at all.
        QStringLiteral("toronto-ecmwf-gaps.json"),
    };
}

} // namespace

class TestOpenMeteoVariables : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void everyHourlyVariableComesBackInARecordedResponse_data();
    void everyHourlyVariableComesBackInARecordedResponse();
    void everyDailyVariableComesBackInARecordedResponse_data();
    void everyDailyVariableComesBackInARecordedResponse();
    void everyCurrentVariableComesBackInARecordedResponse();

    void currentIsASubsetOfHourly();
    void currentDoesNotAskForPrecipitationProbability();

    void noListRepeatsAName();
    void noListIsEmpty();
    void theSpellingsAreTheOnesOpenMeteoUses_data();
    void theSpellingsAreTheOnesOpenMeteoUses();

    void theQueryParametersAreTheListsCommaJoined();
    void theQueryParametersCarryNothingAUrlWouldHaveToEscape();

    void theVariablesTheCardsDoNotReadAreStillAbsent();
    void theMoonVariablesAreAskedForRatherThanComputed();
};

// ============================================================================
// The closure: asked for, and came back.
// ============================================================================

void TestOpenMeteoVariables::everyHourlyVariableComesBackInARecordedResponse_data()
{
    QTest::addColumn<QString>("name");
    for (const QString &name : forecastFixtures())
        QTest::newRow(qPrintable(name)) << name;
}

void TestOpenMeteoVariables::everyHourlyVariableComesBackInARecordedResponse()
{
    QFETCH(QString, name);

    const QJsonObject root = fixture(name);
    QVERIFY2(!root.isEmpty(), qPrintable(name));

    const QJsonObject hourly = root.value(QStringLiteral("hourly")).toObject();
    QVERIFY2(!hourly.isEmpty(), qPrintable(name));

    for (const QLatin1String &variable : hourlyVariables()) {
        QVERIFY2(hourly.contains(QString(variable)),
                 qPrintable(QStringLiteral("%1: hourly has no \"%2\" — the request asks for a "
                                           "name the response does not use")
                                .arg(name, QString(variable))));
    }
}

void TestOpenMeteoVariables::everyDailyVariableComesBackInARecordedResponse_data()
{
    QTest::addColumn<QString>("name");
    for (const QString &name : forecastFixtures())
        QTest::newRow(qPrintable(name)) << name;
}

void TestOpenMeteoVariables::everyDailyVariableComesBackInARecordedResponse()
{
    QFETCH(QString, name);

    const QJsonObject daily = fixture(name).value(QStringLiteral("daily")).toObject();
    QVERIFY2(!daily.isEmpty(), qPrintable(name));

    for (const QLatin1String &variable : dailyVariables()) {
        QVERIFY2(daily.contains(QString(variable)),
                 qPrintable(QStringLiteral("%1: daily has no \"%2\"").arg(name, QString(variable))));
    }
}

void TestOpenMeteoVariables::everyCurrentVariableComesBackInARecordedResponse()
{
    // Only the fixtures that recorded a `current` block. A payload without one
    // is not a failure — the block is requested separately — but a payload WITH
    // one has to answer every name in the list, because a `current=` naming a
    // variable Open-Meteo does not serve is a 400 for the whole request.
    int checked = 0;
    for (const QString &name : forecastFixtures()) {
        const QJsonObject current = fixture(name).value(QStringLiteral("current")).toObject();
        if (current.isEmpty())
            continue;

        ++checked;
        for (const QLatin1String &variable : currentVariables()) {
            QVERIFY2(current.contains(QString(variable)),
                     qPrintable(QStringLiteral("%1: current has no \"%2\"")
                                    .arg(name, QString(variable))));
        }
    }

    QVERIFY2(checked > 0, "no recorded response carries a `current` block, so this test "
                          "asserted nothing at all");
}

// ============================================================================
// The rule that takes the whole request down when it is broken.
// ============================================================================

void TestOpenMeteoVariables::currentIsASubsetOfHourly()
{
    // "A subset of the hourly one — Open-Meteo does not offer every variable as
    // a current value, and asking for one it does not have fails the whole
    // request rather than omitting that field."
    //
    // The subset direction is the safe one to enforce mechanically: a name in
    // `current` that is not in `hourly` has not been through the closure test
    // above and is the shape of the failure that 400s.
    const QSet<QString> hourly  = asSet(hourlyVariables());
    const QSet<QString> current = asSet(currentVariables());

    for (const QString &name : current) {
        QVERIFY2(hourly.contains(name),
                 qPrintable(QStringLiteral("`current` asks for \"%1\", which `hourly` does not")
                                .arg(name)));
    }

    QVERIFY(current.size() < hourly.size());
}

void TestOpenMeteoVariables::currentDoesNotAskForPrecipitationProbability()
{
    // Named specifically because it is the one Open-Meteo has no current value
    // for, it is obviously useful, and the comment in openmeteovariables.cpp is
    // the only thing standing between it and somebody adding it. The cost of
    // being wrong is not a missing number — it is a 400 that empties every
    // screen in the app.
    QVERIFY(!asSet(currentVariables()).contains(QStringLiteral("precipitation_probability")));
    QVERIFY(asSet(hourlyVariables()).contains(QStringLiteral("precipitation_probability")));
}

// ============================================================================
// Shape of the lists themselves.
// ============================================================================

void TestOpenMeteoVariables::noListRepeatsAName()
{
    // A duplicate is free to write and costs a column in every request forever.
    // It also makes the parser's column-length check compare a series against
    // itself, which passes.
    const struct { const char *what; QList<QLatin1String> list; } lists[] = {
        { "hourly",  hourlyVariables()  },
        { "daily",   dailyVariables()   },
        { "current", currentVariables() },
    };

    for (const auto &entry : lists) {
        QCOMPARE(asSet(entry.list).size(), entry.list.size());
    }
}

void TestOpenMeteoVariables::noListIsEmpty()
{
    // The guard against a refactor that returns `{}` and makes every closure
    // test above pass by iterating nothing.
    QVERIFY(hourlyVariables().size() >= 15);
    QVERIFY(dailyVariables().size() >= 15);
    QVERIFY(currentVariables().size() >= 15);
}

void TestOpenMeteoVariables::theSpellingsAreTheOnesOpenMeteoUses_data()
{
    QTest::addColumn<QString>("name");

    // The handful whose spelling is easy to get wrong from memory, written out
    // so a typo fails here rather than as an empty tab. Every one of these is a
    // name where the obvious guess is different from the real thing.
    const char *const names[] = {
        "temperature_2m",              // not "temperature"
        "relative_humidity_2m",        // not "humidity"
        "dew_point_2m",                // not "dewpoint"
        "apparent_temperature",        // not "feels_like"
        "precipitation_probability",   // not "precipitation_chance"
        "weather_code",                // not "weathercode" — that is the v1 spelling
        "cloud_cover",                 // not "cloudcover"
        "wind_speed_10m",              // not "windspeed_10m"
        "wind_gusts_10m",              // not "wind_gust_10m"
        "wind_direction_10m",
        "pressure_msl",                // not "pressure"
        "uv_index",                    // not "uv"
        "is_day",
        "visibility",
    };

    for (const char *name : names)
        QTest::newRow(name) << QString::fromLatin1(name);
}

void TestOpenMeteoVariables::theSpellingsAreTheOnesOpenMeteoUses()
{
    QFETCH(QString, name);
    QVERIFY2(asSet(hourlyVariables()).contains(name), qPrintable(name));
}

// ============================================================================
// The query strings.
// ============================================================================

void TestOpenMeteoVariables::theQueryParametersAreTheListsCommaJoined()
{
    const auto joined = [](const QList<QLatin1String> &list) {
        QStringList parts;
        for (const QLatin1String &name : list)
            parts.append(QString(name));
        return parts.join(QLatin1Char(','));
    };

    QCOMPARE(hourlyParameter(), joined(hourlyVariables()));
    QCOMPARE(dailyParameter(), joined(dailyVariables()));
    QCOMPARE(currentParameter(), joined(currentVariables()));

    // "Order is the order they go in the URL, which is the order they come back
    // in, which makes a recorded response readable." So the parameter is not
    // sorted, deduplicated or otherwise tidied on the way out.
    QVERIFY(hourlyParameter().startsWith(QStringLiteral("temperature_2m,")));
    QVERIFY(hourlyParameter().endsWith(QStringLiteral(",is_day")));
}

void TestOpenMeteoVariables::theQueryParametersCarryNothingAUrlWouldHaveToEscape()
{
    // These go into a query string. A space or an ampersand in one would either
    // be percent-encoded into a name the service does not know, or split the
    // parameter — and both produce the same "variable not in the response"
    // symptom the whole file is about.
    for (const QString &parameter : { hourlyParameter(), dailyParameter(), currentParameter() }) {
        for (const QChar c : parameter) {
            const bool safe = c.isLower() || c.isDigit() || c == QLatin1Char('_')
                              || c == QLatin1Char(',');
            QVERIFY2(safe, qPrintable(QStringLiteral("\"%1\" contains %2")
                                          .arg(parameter).arg(c)));
        }
        QVERIFY(!parameter.startsWith(QLatin1Char(',')));
        QVERIFY(!parameter.endsWith(QLatin1Char(',')));
        QVERIFY(!parameter.contains(QStringLiteral(",,")));
    }
}

// ============================================================================
// Two decisions the header argues for, asserted so that reversing one is
// deliberate.
// ============================================================================

void TestOpenMeteoVariables::theVariablesTheCardsDoNotReadAreStillAbsent()
{
    // "`cloud_cover_low` / `_mid` / `_high` and `snow_depth`. Open-Meteo serves
    // all four and the plan called for them; nothing in app/qml/Clima/ reads
    // them … so asking for them would be four columns fetched, parsed, cached
    // and thrown away on every refresh, forever, against a free service's rate
    // limit."
    //
    // The recorded fixtures DO contain them, deliberately — so the day a card
    // wants one, only this list has to change. Which is also why their absence
    // here cannot be inferred from the fixtures and has to be asserted.
    const QSet<QString> asked = asSet(hourlyVariables()) + asSet(currentVariables());

    for (const QString &unused : { QStringLiteral("cloud_cover_low"),
                                   QStringLiteral("cloud_cover_mid"),
                                   QStringLiteral("cloud_cover_high"),
                                   QStringLiteral("snow_depth") }) {
        QVERIFY2(!asked.contains(unused),
                 qPrintable(QStringLiteral("\"%1\" is fetched and never read").arg(unused)));
    }

    // And `minutely_15` is not a forecast variable at all — it is a separate
    // request with its own cache row and its own region limits.
    for (const QLatin1String &name : hourlyVariables())
        QVERIFY(!QString(name).contains(QStringLiteral("minutely")));
}

void TestOpenMeteoVariables::theMoonVariablesAreAskedForRatherThanComputed()
{
    // "The three that delete a planned task. DetailMoonCard needs a phase, a
    // rise and a set, and the alternative to these was a local ephemeris —
    // Meeus' lunar terms, a few hundred lines of astronomy nobody here would be
    // qualified to review."
    //
    // Asserted because dropping them is a one-line edit that would silently
    // re-open that task: the card would go blank and the obvious repair is to
    // start writing the astronomy.
    const QSet<QString> daily = asSet(dailyVariables());
    QVERIFY(daily.contains(QStringLiteral("moon_phase")));
    QVERIFY(daily.contains(QStringLiteral("moonrise")));
    QVERIFY(daily.contains(QStringLiteral("moonset")));
}

QTEST_MAIN(TestOpenMeteoVariables)
#include "tst_openmeteovariables.moc"
