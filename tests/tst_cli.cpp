// SPDX-FileCopyrightText: 2026 Jowi Aoun
// SPDX-License-Identifier: GPL-3.0-or-later
//
// clima-cli, run as a script would run it.
//
// The binary, not its functions: every case here starts a process with an
// argv and reads what came out, because that is the interface. A script that
// parses `--json` today should parse the same shape after this file has been
// edited, and the way to promise that is to parse it here.
//
// Fixture mode throughout, so nothing opens a socket and every number is one
// tests/fixtures already carries. The reader's preferences come from an INI
// this test writes into a config directory of its own, because the whole
// point of the text output is that it follows them.

#include <QDir>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QProcess>
#include <QProcessEnvironment>
#include <QTemporaryDir>
#include <QtTest>

namespace {

struct Outcome {
    int        exitCode = -1;
    QByteArray stdOut;
    QByteArray stdErr;
};

} // namespace

class TestCli : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void initTestCase();

    void nowPrintsThePlaceAndAReading();
    void nowAsJsonIsCanonicalAndShaped();
    void hourlyCountsHours();
    void dailyCountsDaysAndStartsToday();
    void placesListsTheFixturePlace();
    void theReadersUnitsAreHonouredInTextAndNotInJson();
    void theOverrideBeatsThePreference();
    void anUnknownCommandIsAUsageError();
    void jsonAndCsvTogetherIsAUsageError();

private:
    Outcome run(const QStringList &arguments, const QByteArray &ini = {});

    QTemporaryDir m_config;
};

void TestCli::initTestCase()
{
    QVERIFY2(QFile::exists(QStringLiteral(CLIMA_CLI_BINARY)),
             "clima-cli was not built at " CLIMA_CLI_BINARY);
    QVERIFY(m_config.isValid());
}

Outcome TestCli::run(const QStringList &arguments, const QByteArray &ini)
{
    // The INI where QSettings will look for it under XDG_CONFIG_HOME: the
    // organisation's directory, the application's file. Written fresh — or
    // removed — for every run so that a case cannot inherit another's.
    const QString directory = m_config.path() + QStringLiteral("/Clima");
    const QString file      = directory + QStringLiteral("/clima.ini");
    QDir().mkpath(directory);
    QFile::remove(file);
    if (!ini.isEmpty()) {
        QFile out(file);
        if (out.open(QIODevice::WriteOnly))
            out.write(ini);
    }

    QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
    env.insert(QStringLiteral("XDG_CONFIG_HOME"), m_config.path());
    env.insert(QStringLiteral("LC_ALL"), QStringLiteral("C.UTF-8"));
    env.insert(QStringLiteral("TZ"), QStringLiteral("UTC"));

    QProcess process;
    process.setProcessEnvironment(env);
    process.start(QStringLiteral(CLIMA_CLI_BINARY), arguments);

    Outcome outcome;
    if (!process.waitForFinished(20000)) {
        outcome.stdErr = QByteArrayLiteral("clima-cli did not finish in 20 s");
        process.kill();
        return outcome;
    }
    outcome.exitCode = process.exitCode();
    outcome.stdOut   = process.readAllStandardOutput();
    outcome.stdErr   = process.readAllStandardError();
    return outcome;
}

void TestCli::nowPrintsThePlaceAndAReading()
{
    const Outcome got = run({ QStringLiteral("--fixture"), QStringLiteral("toronto"), QStringLiteral("now") });
    QCOMPARE(got.exitCode, 0);

    const QString text = QString::fromUtf8(got.stdOut);
    QVERIFY2(text.contains(QStringLiteral("Toronto")), qPrintable(text));
    QVERIFY2(text.contains(QStringLiteral("°C")), qPrintable(text));
    QVERIFY2(text.contains(QStringLiteral("Updated")), qPrintable(text));
    QVERIFY2(text.contains(QStringLiteral("fixture")), qPrintable(text));
}

void TestCli::nowAsJsonIsCanonicalAndShaped()
{
    const Outcome got = run({ QStringLiteral("--fixture"), QStringLiteral("toronto"), QStringLiteral("now"),
                              QStringLiteral("--json") });
    QCOMPARE(got.exitCode, 0);

    QJsonParseError error{};
    const QJsonDocument document = QJsonDocument::fromJson(got.stdOut, &error);
    QVERIFY2(error.error == QJsonParseError::NoError, qPrintable(error.errorString()));

    const QJsonObject root = document.object();
    QCOMPARE(root.value(QStringLiteral("place")).toObject().value(QStringLiteral("name")).toString(),
             QStringLiteral("Toronto"));
    QVERIFY(root.contains(QStringLiteral("servedBy")));
    QVERIFY(root.contains(QStringLiteral("fetchedAt")));
    QVERIFY(root.value(QStringLiteral("alerts")).isArray());

    const QJsonObject current = root.value(QStringLiteral("current")).toObject();
    QVERIFY(current.value(QStringLiteral("temperature")).isDouble());
    // Canonical: a Toronto summer afternoon is tens of degrees, not seventies.
    const double temperature = current.value(QStringLiteral("temperature")).toDouble();
    QVERIFY2(temperature > -40 && temperature < 45, qPrintable(QString::number(temperature)));
    QVERIFY(current.value(QStringLiteral("time")).toString().contains(QLatin1Char('T')));
    QVERIFY(!current.value(QStringLiteral("condition")).toString().isEmpty());
}

void TestCli::hourlyCountsHours()
{
    const Outcome got = run({ QStringLiteral("--fixture"), QStringLiteral("toronto"),
                              QStringLiteral("hourly"), QStringLiteral("3"), QStringLiteral("--csv") });
    QCOMPARE(got.exitCode, 0);

    const QList<QByteArray> lines = got.stdOut.trimmed().split('\n');
    QCOMPARE(lines.size(), 4); // a header and three hours
    QVERIFY(lines.constFirst().startsWith("time,temperature,"));

    const Outcome text = run({ QStringLiteral("--fixture"), QStringLiteral("toronto"),
                               QStringLiteral("hourly"), QStringLiteral("5") });
    QCOMPARE(text.exitCode, 0);
    QCOMPARE(text.stdOut.trimmed().split('\n').size(), 5);
}

void TestCli::dailyCountsDaysAndStartsToday()
{
    const Outcome got = run({ QStringLiteral("--fixture"), QStringLiteral("toronto"),
                              QStringLiteral("daily"), QStringLiteral("2"), QStringLiteral("--json") });
    QCOMPARE(got.exitCode, 0);

    const QJsonArray days = QJsonDocument::fromJson(got.stdOut).object().value(QStringLiteral("daily")).toArray();
    QCOMPARE(days.size(), 2);

    // The fixture's clock is the recording instant; the first day is that day
    // and not the `past_days` day the series starts on.
    const Outcome text = run({ QStringLiteral("--fixture"), QStringLiteral("toronto"),
                               QStringLiteral("daily"), QStringLiteral("1") });
    QVERIFY2(text.stdOut.startsWith("Today"), text.stdOut.constData());
}

void TestCli::placesListsTheFixturePlace()
{
    const Outcome got = run({ QStringLiteral("--fixture"), QStringLiteral("toronto"), QStringLiteral("places") });
    QCOMPARE(got.exitCode, 0);
    QVERIFY(got.stdOut.contains("Toronto"));
}

void TestCli::theReadersUnitsAreHonouredInTextAndNotInJson()
{
    const QByteArray fahrenheit = QByteArrayLiteral("[units]\ntemperature=fahrenheit\nwind=mph\n");

    const Outcome text = run({ QStringLiteral("--fixture"), QStringLiteral("toronto"), QStringLiteral("now") },
                             fahrenheit);
    QCOMPARE(text.exitCode, 0);
    QVERIFY2(text.stdOut.contains("\xC2\xB0" "F"), text.stdOut.constData());
    QVERIFY2(text.stdOut.contains("mph"), text.stdOut.constData());

    const Outcome json = run({ QStringLiteral("--fixture"), QStringLiteral("toronto"), QStringLiteral("now"),
                               QStringLiteral("--json") },
                             fahrenheit);
    const double temperature = QJsonDocument::fromJson(json.stdOut).object()
                                   .value(QStringLiteral("current")).toObject()
                                   .value(QStringLiteral("temperature")).toDouble();
    // Still Celsius: a preference in a dialog must not move a number a script
    // reads.
    QVERIFY2(temperature < 45, qPrintable(QString::number(temperature)));
}

void TestCli::theOverrideBeatsThePreference()
{
    const Outcome got = run({ QStringLiteral("--fixture"), QStringLiteral("toronto"), QStringLiteral("now"),
                              QStringLiteral("--units"), QStringLiteral("metric") },
                            QByteArrayLiteral("[units]\ntemperature=fahrenheit\n"));
    QCOMPARE(got.exitCode, 0);
    QVERIFY2(got.stdOut.contains("\xC2\xB0" "C"), got.stdOut.constData());
    QVERIFY(!got.stdOut.contains("\xC2\xB0" "F"));
}

void TestCli::anUnknownCommandIsAUsageError()
{
    const Outcome got = run({ QStringLiteral("--fixture"), QStringLiteral("toronto"), QStringLiteral("tomorrow") });
    QCOMPARE(got.exitCode, 2);
    QVERIFY(got.stdErr.contains("not a command"));

    const Outcome none = run({});
    QCOMPARE(none.exitCode, 2);
}

void TestCli::jsonAndCsvTogetherIsAUsageError()
{
    const Outcome got = run({ QStringLiteral("--fixture"), QStringLiteral("toronto"), QStringLiteral("now"),
                              QStringLiteral("--json"), QStringLiteral("--csv") });
    QCOMPARE(got.exitCode, 2);
}

QTEST_GUILESS_MAIN(TestCli)
#include "tst_cli.moc"
