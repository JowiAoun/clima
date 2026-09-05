// SPDX-FileCopyrightText: 2026 Jowi Aoun
// SPDX-License-Identifier: GPL-3.0-or-later
//
// The desktop tiles, as far as a headless test can reach them.
//
// ============================================================================
// WHAT IS TESTED HERE AND WHAT IS NOT
//
// Two kinds of thing, and it is worth being honest about the boundary.
//
// The first is `Wx`, which is where every number a tile prints goes through a
// function. Its whole job is to keep an absent reading absent — QVariant in,
// empty string out for anything that is not a number — and that is a pure
// function of its argument, so it is tested the ordinary way.
//
// The second is a set of THREE-WAY CONSISTENCY checks between files that have
// to agree and that nothing else would notice disagreeing:
//
//     widgets/catalogue.json      what a widget is, and what it asks the daemon for
//     WidgetTile.qml              which component draws it
//     widgets/CMakeLists.txt      which files are in the module
//
// A catalogue entry with no component is an empty tile and a line in the
// journal. A component that is not in CMakeLists is a QML type that does not
// exist, reported against the file that *used* it. Both are silent in every
// other test, and both are one line of JSON away at all times.
//
// What is NOT here is rendering. A tile is QML and its layout is a property of
// a running scene graph; the answer to "does the hourly strip fit in 360 px" is
// a screenshot, not an assertion. `clima-widget --snapshot … --grab` is how
// that is looked at, and tests/fixtures/wire/ is what it is looked at against.

#include "daemonlink.h"
#include "app/settings.h"
#include "widgetfeed.h"
#include "wx.h"

#include <QDir>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QRegularExpression>
#include <QSet>
#include <QTest>

namespace {

QString readFile(const QString &relative)
{
    QFile file(QStringLiteral(CLIMA_SOURCE_DIR "/") + relative);
    if (!file.open(QIODevice::ReadOnly))
        return {};
    return QString::fromUtf8(file.readAll());
}

QJsonObject readJson(const QString &relative)
{
    return QJsonDocument::fromJson(readFile(relative).toUtf8()).object();
}

// Source with its // comments removed, so that a check for what a file DOES is
// not answered by a paragraph about what it used to do. daemon/main.cpp quotes
// the wrong identity at length in the comment that explains the fix.
QString withoutComments(const QString &source)
{
    QStringList kept;
    const QStringList lines = source.split(u'\n');
    kept.reserve(lines.size());
    for (const QString &line : lines) {
        if (!line.trimmed().startsWith(QLatin1String("//")))
            kept.append(line);
    }
    return kept.join(u'\n');
}

// The argument to a QCoreApplication/QGuiApplication setter, verbatim — the
// text, not the value, because two of these are a macro and comparing what
// each file WROTE is the question. `QStringLiteral(CLIMA_APP_NAME)` in one
// file and `QStringLiteral("clima")` in another would be equal at run time on
// the day it was written and would drift the day the macro moved.
QString identitySetTo(const QString &source, const QString &setter)
{
    const QRegularExpression pattern(
        QStringLiteral("(?:QCoreApplication|QGuiApplication)::%1\\(([^;]*)\\);").arg(setter));
    const QRegularExpressionMatch match = pattern.match(source);
    return match.hasMatch() ? match.captured(1).simplified() : QString();
}

// Every dotted path the encoder produced in a recorded snapshot. Used to check
// a catalogue's declared fields against something real rather than against a
// list written from the same beliefs.
void flatten(const QJsonObject &object, const QString &prefix, QSet<QString> &out)
{
    for (auto it = object.constBegin(); it != object.constEnd(); ++it) {
        const QString path = prefix.isEmpty() ? it.key() : prefix + u'.' + it.key();
        out.insert(path);
        if (it.value().isObject())
            flatten(it.value().toObject(), path, out);
    }
}

} // namespace

class TestWidgets : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void initTestCase();

    // ---- the three files that have to agree --------------------------------
    void everyWidgetHasAComponent();
    void everyComponentHasAWidget();
    void everyComponentIsInTheModule();
    void everyDeclaredFieldIsOneTheDaemonSends();
    void allFourProcessesShareOneStorageIdentity();

    // ---- what a tile with no data says -------------------------------------
    //
    // Order matters between these three and it is not stylistic. DaemonLink is
    // a process-wide singleton by construction, so each of them leaves it
    // further along than it found it: no source at all, then a source that
    // failed, then one that worked. Qt runs private slots in declaration order.
    void aFeedWithNoSourceSaysThereIsNoService();
    void anUnreadableSnapshotNamesTheFile();
    void aReadableSnapshotLeavesNothingToExplain();

    // ---- Wx ----------------------------------------------------------------
    void absentReadingsStayAbsent();
    void publishedBandsMatchTheirTables();
    void glyphKindDegradesRatherThanVanishing();
    void clockIsTwelveHourWithASeparateSuffix();
    void instantsAreReadInThePlacesOwnZone();
    void ageNeverOverstatesHowFreshAReadingIs();

private:
    QStringList m_catalogueIds;
    QString     m_dispatch;
    QString     m_buildFile;
    Wx         *m_wx = nullptr;
};

void TestWidgets::initTestCase()
{
    const QJsonObject catalogue = readJson(QStringLiteral("widgets/catalogue.json"));
    QVERIFY2(!catalogue.isEmpty(), "widgets/catalogue.json did not parse");

    for (const QJsonValue &entry : catalogue.value(QStringLiteral("widgets")).toArray())
        m_catalogueIds.append(entry.toObject().value(QStringLiteral("id")).toString());
    QVERIFY(!m_catalogueIds.isEmpty());

    m_dispatch = readFile(QStringLiteral("widgets/qml/Clima/Widgets/WidgetTile.qml"));
    QVERIFY2(!m_dispatch.isEmpty(), "WidgetTile.qml is missing");

    m_buildFile = readFile(QStringLiteral("widgets/CMakeLists.txt"));
    QVERIFY2(!m_buildFile.isEmpty(), "widgets/CMakeLists.txt is missing");

    // Not a QML singleton here — there is no engine. create() is what QML would
    // call and it hands back the same object, so the behaviour under test is
    // the behaviour a tile gets.
    m_wx = Wx::create(nullptr, nullptr);
    QVERIFY(m_wx != nullptr);
}

// ---- consistency ------------------------------------------------------------

void TestWidgets::everyWidgetHasAComponent()
{
    for (const QString &id : std::as_const(m_catalogueIds)) {
        const QString label = QStringLiteral("case \"%1\":").arg(id);
        QVERIFY2(m_dispatch.contains(label),
                 qPrintable(QStringLiteral("widgets/catalogue.json declares \"%1\" and "
                                           "WidgetTile.qml has no case for it. The tile would "
                                           "be empty and the only report would be a console "
                                           "warning.")
                                .arg(id)));
    }
}

void TestWidgets::everyComponentHasAWidget()
{
    // The other direction, which catches the likelier mistake: a widget renamed
    // in the catalogue and not in the dispatch leaves a case nothing reaches.
    static const QRegularExpression caseLine(QStringLiteral("case \"([a-z0-9-]+)\":"));

    auto it = caseLine.globalMatch(m_dispatch);
    int  seen = 0;
    while (it.hasNext()) {
        const QString id = it.next().captured(1);
        ++seen;
        QVERIFY2(m_catalogueIds.contains(id),
                 qPrintable(QStringLiteral("WidgetTile.qml dispatches \"%1\", which is not in "
                                           "widgets/catalogue.json.")
                                .arg(id)));
    }
    QCOMPARE(seen, m_catalogueIds.size());
}

void TestWidgets::everyComponentIsInTheModule()
{
    // A .qml file that exists on disk and is not in QML_FILES compiles nothing,
    // registers no type, and produces "X is not a type" against whichever file
    // used it. scripts/check-qml-files.sh catches the general case; this is the
    // specific one, so a broken build says which widget.
    const QDir dir(QStringLiteral(CLIMA_SOURCE_DIR "/widgets/qml/Clima/Widgets"));
    const QStringList files = dir.entryList(QStringList{ QStringLiteral("*Widget.qml") },
                                            QDir::Files, QDir::Name);
    QVERIFY2(files.size() == m_catalogueIds.size(),
             qPrintable(QStringLiteral("%1 *Widget.qml files on disk for %2 catalogue entries")
                            .arg(files.size())
                            .arg(m_catalogueIds.size())));

    for (const QString &file : files) {
        QVERIFY2(m_buildFile.contains(QStringLiteral("qml/Clima/Widgets/") + file),
                 qPrintable(QStringLiteral("%1 is on disk and not in widgets/CMakeLists.txt")
                                .arg(file)));
    }
}

void TestWidgets::everyDeclaredFieldIsOneTheDaemonSends()
{
    // Checked against a RECORDING rather than against the encoder's source, so
    // the assertion is about what a widget will actually receive. A field name
    // that is right in the catalogue and wrong on the wire is a binding that
    // silently evaluates to undefined, which QML renders as nothing at all.
    QSet<QString> paths;
    flatten(readJson(QStringLiteral("tests/fixtures/wire/seattle.json")), QString(), paths);
    QVERIFY2(paths.contains(QStringLiteral("current.temperature")),
             "the Seattle recording is not a snapshot");

    const QJsonObject catalogue = readJson(QStringLiteral("widgets/catalogue.json"));
    for (const QJsonValue &entry : catalogue.value(QStringLiteral("widgets")).toArray()) {
        const QJsonObject widget = entry.toObject();
        const QString     id     = widget.value(QStringLiteral("id")).toString();

        for (const QJsonValue &field : widget.value(QStringLiteral("fields")).toArray()) {
            const QString path = field.toString();
            QVERIFY2(paths.contains(path),
                     qPrintable(QStringLiteral("\"%1\" asks for \"%2\", which is not in a "
                                               "recorded snapshot.")
                                    .arg(id, path)));
        }
    }
}

// ---- and the fourth file, which is where the data lives ---------------------

void TestWidgets::allFourProcessesShareOneStorageIdentity()
{
    // QStandardPaths::AppDataLocation is <organizationName>/<applicationName>,
    // and libclima/cache/cachestore.cpp puts the database under it. So these
    // two calls in three main() functions are not identity, they are an
    // address — and three processes that are supposed to share one database
    // agree about it in three separate files with nothing joining them up.
    //
    // The daemon disagreed. It set organizationName("clima") and
    // applicationName("clima-daemon"), opened
    // ~/.local/share/clima/clima-daemon/cache.sqlite, and found no places in
    // it — so every Subscribe answered "no such place" and every tile on every
    // desktop stayed empty, while both processes ran perfectly.
    //
    // Nothing caught it, and the reason is worth keeping: every automated test
    // and every screenshot of the tiles runs the daemon with --fixture, which
    // resolves its place out of a recorded file and never opens the places
    // table at all. The mode nobody automated was the only one a user runs.
    // clima-cli is the fourth, and it is here for exactly the reason the daemon
    // is: `clima-cli now` reads the places table to find out where "here" is
    // and writes what it fetched back into the same cache, so a status bar
    // polling every minute is one more client of the forecast service and not
    // a second one. An identity of its own would give it an empty places table
    // — every invocation answering "no saved place" on a machine with several.
    //
    // clima-gallery is deliberately absent. It sets the same organisation and a
    // name of its OWN, which is the opposite requirement: it is a developer
    // tool, it must read the reader's preferences so that a specimen is drawn
    // the way the product draws it, and it must never write to the file or the
    // database the product reads. Sharing the directory and not the name gets
    // both.
    const QStringList mains = {
        QStringLiteral("app/main.cpp"),
        QStringLiteral("widgets/main.cpp"),
        QStringLiteral("daemon/main.cpp"),
        QStringLiteral("cli/main.cpp"),
    };

    QString organisation;
    QString application;

    for (const QString &path : mains) {
        const QString source = withoutComments(readFile(path));
        QVERIFY2(!source.isEmpty(), qPrintable(path + QStringLiteral(" is missing")));

        const QString org = identitySetTo(source, QStringLiteral("setOrganizationName"));
        const QString app = identitySetTo(source, QStringLiteral("setApplicationName"));

        QVERIFY2(!org.isEmpty() && !app.isEmpty(),
                 qPrintable(QStringLiteral("%1 does not set both names. Qt defaults the missing "
                                           "one to the executable name, so this process would "
                                           "open a database of its own.")
                                .arg(path)));

        if (organisation.isEmpty()) {
            organisation = org;
            application  = app;
            continue;
        }

        QVERIFY2(org == organisation && app == application,
                 qPrintable(QStringLiteral("%1 sets (%2, %3); %4 sets (%5, %6). These name the "
                                           "AppDataLocation each process reads, so they resolve "
                                           "to different databases and the daemon serves a "
                                           "places table the app has never written to.")
                                .arg(mains.first(), organisation, application, path, org, app)));
    }
}

// ---- what a tile with no data says ------------------------------------------
//
// The bug these exist for: every tile drew a skeleton for as long as it had
// nothing, whether a snapshot was half a second away or was never coming. On a
// desktop with no daemon that is a permanent picture of loading, and the only
// other evidence was a warning that could not fire, since the one warning here
// was for a session bus that could not be reached.
//
// None of this touches D-Bus. That is not a limitation of a headless test — it
// is the point: DaemonLink has to be able to answer "why is there nothing" from
// what it knows, and the two states below are established before any bus is
// consulted. The bus paths were exercised by hand against a real daemon on a
// private bus; the sentences are what a test can hold still.

void TestWidgets::aFeedWithNoSourceSaysThereIsNoService()
{
    WidgetFeed feed;
    feed.classBegin();

    // Before completion a feed is not attached, and it must not pretend to know
    // anything. QML assigns properties between these two calls.
    QVERIFY(feed.waitingReason().isEmpty());

    feed.componentComplete();

    QVERIFY2(!feed.waitingReason().isEmpty(),
             "a tile with no daemon, no file and nothing on its way said nothing at all — "
             "which is the skeleton-forever bug this property exists to close");
    QVERIFY(feed.waitingReason().contains(QStringLiteral("not running")));
    QVERIFY(!feed.hasData());
}

void TestWidgets::anUnreadableSnapshotNamesTheFile()
{
    WidgetFeed feed;
    feed.classBegin();
    feed.componentComplete();

    const QString missing = QStringLiteral(CLIMA_SOURCE_DIR "/tests/fixtures/wire/nowhere.json");
    DaemonLink::instance()->useSnapshotFile(missing);

    // Attached BEFORE the change, so this also covers the push: a feed that is
    // already on a desktop has to be told when the answer changes under it, not
    // only when it asks.
    QVERIFY2(feed.waitingReason().contains(QStringLiteral("nowhere.json")),
             qPrintable(QStringLiteral("expected the file to be named, got: \"%1\"")
                            .arg(feed.waitingReason())));
    QVERIFY(!feed.hasData());
}

void TestWidgets::aReadableSnapshotLeavesNothingToExplain()
{
    DaemonLink::instance()->useSnapshotFile(
        QStringLiteral(CLIMA_SOURCE_DIR "/tests/fixtures/wire/toronto.json"));

    WidgetFeed feed;
    feed.classBegin();
    feed.componentComplete();

    QVERIFY(feed.hasData());

    // Data outranks every explanation of its absence. A tile that has a reading
    // shows the reading and its age — never a sentence about why it has none.
    QVERIFY(feed.waitingReason().isEmpty());
}

// ---- Wx ---------------------------------------------------------------------

void TestWidgets::absentReadingsStayAbsent()
{
    // The whole reason this class takes QVariant. Declared as double, QML would
    // coerce every one of these to 0 and the tiles would print "Low", "Good",
    // "N" and "Calm" for readings nobody took.
    const QList<QVariant> nothings{
        QVariant(),                                    // undefined
        QVariant::fromValue(nullptr),                  // JSON null
        QVariant(QStringLiteral("")),                  // an empty string
    };

    for (const QVariant &nothing : nothings) {
        QVERIFY(m_wx->uvBand(nothing).isEmpty());
        QVERIFY(m_wx->aqiBand(nothing).isEmpty());
        QVERIFY(m_wx->compass(nothing).isEmpty());
        QVERIFY(m_wx->beaufort(nothing).isEmpty());
        QVERIFY(m_wx->glyphKind(nothing, QVariant(1)).isEmpty());
        QVERIFY(m_wx->conditionText(nothing, QVariant(1)).isEmpty());
        QVERIFY(m_wx->precipType(nothing).isEmpty());
        QVERIFY(m_wx->clockTime(nothing).isEmpty());
        QVERIFY(m_wx->shortDay(nothing).isEmpty());
        QCOMPARE(m_wx->minutesFromMidnight(nothing), -1);
        QCOMPARE(m_wx->nowMinutesInZoneOf(nothing), -1);
    }

    // And zero is not absent. A UV index of 0 at midnight is a reading.
    QCOMPARE(m_wx->uvBand(QVariant(0.0)), QStringLiteral("Low"));
    QCOMPARE(m_wx->compass(QVariant(0.0)), QStringLiteral("N"));
}

void TestWidgets::publishedBandsMatchTheirTables()
{
    // The boundaries, because a band is a threshold and an off-by-one there is
    // a tile that says "High" where the WHO says "Moderate".
    QCOMPARE(m_wx->uvBand(QVariant(2.9)), QStringLiteral("Low"));
    QCOMPARE(m_wx->uvBand(QVariant(3.0)), QStringLiteral("Moderate"));
    QCOMPARE(m_wx->uvBand(QVariant(7.9)), QStringLiteral("High"));
    QCOMPARE(m_wx->uvBand(QVariant(8.0)), QStringLiteral("Very high"));
    QCOMPARE(m_wx->uvBand(QVariant(11.0)), QStringLiteral("Extreme"));

    QCOMPARE(m_wx->aqiBand(QVariant(20.0)), QStringLiteral("Good"));
    QCOMPARE(m_wx->aqiBand(QVariant(20.1)), QStringLiteral("Fair"));
    QCOMPARE(m_wx->aqiBand(QVariant(100.0)), QStringLiteral("Very poor"));
    QCOMPARE(m_wx->aqiBand(QVariant(100.1)), QStringLiteral("Extremely poor"));

    // Sixteen points, and 348.75° is the boundary that rounds back to north.
    QCOMPARE(m_wx->compass(QVariant(0.0)), QStringLiteral("N"));
    QCOMPARE(m_wx->compass(QVariant(90.0)), QStringLiteral("E"));
    QCOMPARE(m_wx->compass(QVariant(333.0)), QStringLiteral("NNW"));
    QCOMPARE(m_wx->compass(QVariant(359.9)), QStringLiteral("N"));

    QCOMPARE(m_wx->pollutant(QVariant(QStringLiteral("pm2_5"))), QStringLiteral("PM2.5"));
    QCOMPARE(m_wx->pollutant(QVariant(QStringLiteral("o3"))), QString::fromUtf8("O₃"));
}

void TestWidgets::glyphKindDegradesRatherThanVanishing()
{
    // WeatherGlyph draws nothing for a kind it does not know, and an empty
    // glyph reads as "no data" rather than as "snow". Every code the WMO tables
    // map has to come back as one of the thirteen the component can draw.
    //
    // It used to be seven, and a `drawableToday()` in the engine folded the
    // other six into them on the way here — so a widget showing a thunderstorm
    // drew an ordinary shower, and this test passed. Thirteen is the whole of
    // ConditionKind now, so the assertion is only that the widget host and the
    // app read the same table; tests/qml/tst_weatherglyph.qml is the one that
    // checks the pictures exist.
    static const QSet<QString> drawable{
        QStringLiteral("clear-day"),  QStringLiteral("clear-night"),
        QStringLiteral("partly-day"), QStringLiteral("partly-night"),
        QStringLiteral("cloudy"),     QStringLiteral("fog"),
        QStringLiteral("drizzle"),    QStringLiteral("rain"),
        QStringLiteral("rain-night"), QStringLiteral("sleet"),
        QStringLiteral("snow"),       QStringLiteral("thunder"),
        QStringLiteral("hail"),
    };

    for (int code = 0; code <= 99; ++code) {
        for (const QVariant day : { QVariant(1), QVariant(0) }) {
            const QString kind = m_wx->glyphKind(QVariant(code), day);
            QVERIFY2(drawable.contains(kind),
                     qPrintable(QStringLiteral("WMO %1 (isDay=%2) became \"%3\", which "
                                               "WeatherGlyph.qml cannot draw")
                                    .arg(code)
                                    .arg(day.toInt())
                                    .arg(kind)));
        }
    }

    // A null isDay is not night. The wire carries the flag as null for a
    // provider that does not report it, and a moon over a sunny afternoon is a
    // more obviously broken picture than a sun over a clear night.
    QCOMPARE(m_wx->glyphKind(QVariant(0), QVariant()), QStringLiteral("clear-day"));
    QCOMPARE(m_wx->glyphKind(QVariant(0), QVariant(0)), QStringLiteral("clear-night"));

    // And a storm reaches a tile as a storm. The widget host runs its own copy
    // of this lookup, so a fold reintroduced on one side of the D-Bus wire and
    // not the other is a real way for the tray to disagree with the window.
    QCOMPARE(m_wx->glyphKind(QVariant(95), QVariant(1)), QStringLiteral("thunder"));
    QCOMPARE(m_wx->glyphKind(QVariant(96), QVariant(1)), QStringLiteral("hail"));
    QCOMPARE(m_wx->glyphKind(QVariant(75), QVariant(0)), QStringLiteral("snow"));
}

void TestWidgets::clockIsTwelveHourWithASeparateSuffix()
{
    // The preference, stated rather than assumed. It used to be assumed, and
    // that held only while `Settings::clockFormat` defaulted to "12h" for
    // everybody: it now defaults to the reader's own locale, and this file runs
    // under LC_ALL=C.UTF-8, whose short format is 24-hour. The subject here is
    // the 12-hour SPELLING, so the 12-hour preference is part of the setup.
    Settings::instance()->setClockFormat(QStringLiteral("12h"));

    // The bug this exists for: QLocale's "h" is a 24-hour hour unless the format
    // also carries AP, so asking for "h:mm" and appending "PM" separately
    // produced "20:37 PM" under every sunset the first time the sun tile ran.
    const QVariant evening(QStringLiteral("2026-08-06T20:37:00-07:00"));
    QCOMPARE(m_wx->clockLabel(evening), QStringLiteral("8:37"));
    QCOMPARE(m_wx->clockSuffix(evening), QStringLiteral("PM"));
    QCOMPARE(m_wx->hourLabel(evening), QStringLiteral("8 PM"));

    const QVariant morning(QStringLiteral("2026-08-06T05:52:00-07:00"));
    QCOMPARE(m_wx->clockLabel(morning), QStringLiteral("5:52"));
    QCOMPARE(m_wx->clockSuffix(morning), QStringLiteral("AM"));

    // Midnight and noon are the two the modulo gets wrong if it is written the
    // obvious way: 0 % 12 is 0, and "0:15 AM" is not a time anybody writes.
    QCOMPARE(m_wx->clockLabel(QVariant(QStringLiteral("2026-08-06T00:15:00-07:00"))),
             QStringLiteral("12:15"));
    QCOMPARE(m_wx->clockSuffix(QVariant(QStringLiteral("2026-08-06T00:15:00-07:00"))),
             QStringLiteral("AM"));
    QCOMPARE(m_wx->clockLabel(QVariant(QStringLiteral("2026-08-06T12:15:00-07:00"))),
             QStringLiteral("12:15"));
    QCOMPARE(m_wx->clockSuffix(QVariant(QStringLiteral("2026-08-06T12:15:00-07:00"))),
             QStringLiteral("PM"));

    QCOMPARE(m_wx->spanBetween(morning, evening), QStringLiteral("14 h 45 min"));

    // A body that sets before it rises sets the NEXT day. The moon does this
    // most nights and a naive subtraction gives a negative span.
    QCOMPARE(m_wx->spanBetween(evening, morning), QStringLiteral("9 h 15 min"));
}

void TestWidgets::instantsAreReadInThePlacesOwnZone()
{
    // The wire has already moved every timestamp into the place's zone and left
    // the offset on it. These functions therefore read the wall clock out of the
    // string rather than converting: a second conversion here would put a
    // Toronto sunrise in the reader's afternoon.
    //
    // This test runs under TZ=UTC (tests/CMakeLists.txt), so a conversion would
    // be visible: 05:52-07:00 is 12:52 UTC.
    QCOMPARE(m_wx->minutesFromMidnight(QVariant(QStringLiteral("2026-08-06T05:52:00-07:00"))),
             5 * 60 + 52);
    QCOMPARE(m_wx->minutesFromMidnight(QVariant(QStringLiteral("2026-08-06T05:52:00+02:00"))),
             5 * 60 + 52);

    // And a bare date, which is what a daily entry is.
    QCOMPARE(m_wx->shortDay(QVariant(QStringLiteral("2026-08-06"))), QStringLiteral("Thu"));
    QCOMPARE(m_wx->shortDay(QVariant(QStringLiteral("2026-08-06T05:52:00-07:00"))),
             QStringLiteral("Thu"));

    // nowMinutesInZoneOf reads the *real* clock, so it cannot be compared with a
    // literal. What it can be compared with is itself, in two offsets an hour
    // apart — which is the property the sun mark depends on.
    const int here = m_wx->nowMinutesInZoneOf(QVariant(QStringLiteral("2026-01-01T00:00:00+00:00")));
    const int east = m_wx->nowMinutesInZoneOf(QVariant(QStringLiteral("2026-01-01T00:00:00+01:00")));
    QVERIFY(here >= 0 && east >= 0);
    QCOMPARE((east - here + 1440) % 1440, 60);
}

void TestWidgets::ageNeverOverstatesHowFreshAReadingIs()
{
    // Nothing to age is empty, not "just now": that would be a claim about a
    // reading that does not exist.
    QVERIFY(m_wx->ago(-1).isEmpty());

    QCOMPARE(m_wx->ago(0), QStringLiteral("just now"));
    QCOMPARE(m_wx->ago(1), QStringLiteral("1 min ago"));
    QCOMPARE(m_wx->ago(59), QStringLiteral("59 min ago"));
    QCOMPARE(m_wx->ago(60), QStringLiteral("1 h ago"));
    QCOMPARE(m_wx->ago(23 * 60 + 59), QStringLiteral("23 h ago"));
    QCOMPARE(m_wx->ago(24 * 60), QStringLiteral("yesterday"));
    QCOMPARE(m_wx->ago(48 * 60), QStringLiteral("2 days ago"));
}

QTEST_MAIN(TestWidgets)
#include "tst_widgets.moc"
