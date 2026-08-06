// SPDX-FileCopyrightText: 2026 Jowi Aoun
// SPDX-License-Identifier: GPL-3.0-or-later
//
// The wire between the daemon and everything that reads from it, and the
// catalogue that declares what each widget asks for.
//
// ============================================================================
// WHY THE CATALOGUE IS TESTED HERE AND NOT BESIDE THE WIDGETS
//
// widgets/catalogue.json is consumed by five things — the widget host, the
// gallery group, the Plasma configuration page, the extension's menu and the
// documentation — and the whole reason it is one file is that five copies
// would disagree. The failure it is guarding against is not a crash: a widget
// whose `fields` contains a typo gets a snapshot with an axis in it and
// nothing else, renders an empty tile, and looks like a layout bug.
//
// So the test builds a full snapshot, flattens it to the set of paths the
// encoder can actually produce, and asserts every field in the catalogue is
// one of them. A renamed leaf in snapshot.cpp fails here, naming the widget.
//
// ============================================================================
// AND THE THREE RULES
//
// The header states three rules that are easy to break and silent when broken:
// a series carries its axis, absent is null rather than zero, and a slice
// starts at now rather than at index 0. Each has a test, and each was proved
// to fail by breaking the encoder before being trusted.

#include "libclima/wire/snapshot.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSet>
#include <QTest>
#include <QTimeZone>

using namespace clima;

namespace {

// Noon on a fixed day, in a fixed zone. Everything below is relative to it.
const QDateTime kNow = QDateTime(QDate(2026, 8, 6), QTime(12, 0), QTimeZone("America/Toronto"));

// A forecast with a day behind it and two days ahead, which is the shape the
// real one has: `past_days=1` means index 0 is yesterday, and that is the
// whole reason the slice rule exists.
//
// Temperature carries the hour offset from `kNow` so an assertion can say
// which hour it got rather than merely that it got something. Precipitation is
// deliberately absent at +2 h.
Forecast makeForecast()
{
    Forecast f;
    f.providerId = QStringLiteral("test");
    f.timeZone   = QTimeZone("America/Toronto");
    f.fetchedAt  = kNow.addSecs(-600);
    f.coordinate = {43.7, -79.4};

    f.current.time                = kNow;
    f.current.temperature         = 21.5;
    f.current.apparentTemperature = 20.0;
    f.current.windSpeed           = 12.0;
    f.current.weatherCode         = 3;
    f.current.isDay               = true;
    f.current.uvIndex             = 5.0;

    for (int h = -24; h <= 48; ++h) {
        HourlyPoint p;
        p.time        = kNow.addSecs(h * 3600);
        p.temperature = double(h);   // the hour offset, so a slice is identifiable
        p.weatherCode = 61;
        if (h != 2)
            p.precipitation = 0.5;   // …and absent at +2 h, on purpose
        f.hourly.append(p);
    }

    for (int d = -1; d <= 12; ++d) {
        DailyPoint p;
        p.date           = kNow.date().addDays(d);
        p.temperatureMax = double(d);
        p.temperatureMin = double(d) - 5.0;
        p.weatherCode    = 2;
        p.sunrise        = QDateTime(p.date, QTime(6, 12), f.timeZone);
        p.sunset         = QDateTime(p.date, QTime(20, 30), f.timeZone);
        p.uvIndexMax     = 6.0;
        f.daily.append(p);
    }
    return f;
}

Place makePlace()
{
    Place p;
    p.name        = QStringLiteral("Toronto");
    p.admin1      = QStringLiteral("Ontario");
    p.country     = QStringLiteral("Canada");
    p.countryCode = QStringLiteral("CA");
    p.timezone    = QStringLiteral("America/Toronto");
    p.coordinate  = {43.7, -79.4};
    return p;
}

wire::SnapshotSource makeSource()
{
    wire::SnapshotSource s;
    s.placeId   = QStringLiteral("toronto");
    s.place     = makePlace();
    s.forecast  = makeForecast();
    s.now       = kNow;
    s.hours     = 12;
    s.days      = 7;
    s.servedBy  = QStringLiteral("test");
    s.fromCache = false;
    return s;
}

// Every dotted path the encoder emitted: section names, and section.leaf for
// each leaf under an object. This is what the catalogue is checked against.
QSet<QString> flatten(const QJsonObject &root)
{
    QSet<QString> paths;
    for (auto it = root.begin(); it != root.end(); ++it) {
        paths.insert(it.key());
        if (it.value().isObject()) {
            const QJsonObject section = it.value().toObject();
            for (auto leaf = section.begin(); leaf != section.end(); ++leaf)
                paths.insert(it.key() + u'.' + leaf.key());
        }
    }
    return paths;
}

QJsonObject catalogue()
{
    QFile file(QStringLiteral(CLIMA_SOURCE_DIR "/widgets/catalogue.json"));
    if (!file.open(QIODevice::ReadOnly))
        return {};
    QJsonParseError error{};
    const QJsonDocument doc = QJsonDocument::fromJson(file.readAll(), &error);
    if (error.error != QJsonParseError::NoError)
        return {};
    return doc.object();
}

} // namespace

class TestWireSnapshot : public QObject
{
    Q_OBJECT

private slots:
    void emptyMaskMeansEverything();
    void branchSelectsItsLeaves();
    void maskTrimsWhatItDidNotAskFor();

    void seriesCarriesItsAxis();
    void absentReadingIsNullNotZero();
    void sliceStartsAtNow();
    void dailySliceStartsToday();

    void alwaysCarriesEnoughToBeJudged();
    void alertsKnownSeparatesEmptyFromUnasked();

    void catalogueIsWellFormed();
    void catalogueFieldsAreAllEncodable();
    void catalogueHorizonsMatchItsFields();
};

// ---- the mask ---------------------------------------------------------------

void TestWireSnapshot::emptyMaskMeansEverything()
{
    // A reader that names no fields is asking for the whole thing. The other
    // convention would turn a forgotten argument into a blank widget, which is
    // a bug you debug in the renderer for an hour.
    QVERIFY(wire::FieldMask::fromFields({}).isEverything());
    QVERIFY(wire::FieldMask::fromFields({QString(), QStringLiteral("  ")}).isEverything());

    const QJsonObject snap = wire::buildSnapshot(makeSource(), wire::FieldMask::fromFields({}));
    QVERIFY(snap.contains(QStringLiteral("place")));
    QVERIFY(snap.contains(QStringLiteral("current")));
    QVERIFY(snap.contains(QStringLiteral("hourly")));
    QVERIFY(snap.contains(QStringLiteral("daily")));
}

void TestWireSnapshot::branchSelectsItsLeaves()
{
    const wire::FieldMask mask = wire::FieldMask::fromFields({QStringLiteral("current")});
    QVERIFY(mask.wants(QStringLiteral("current.temperature")));
    QVERIFY(mask.wantsAnyUnder(QStringLiteral("current")));
    QVERIFY(!mask.wants(QStringLiteral("hourly.temperature")));

    const wire::FieldMask leaf =
        wire::FieldMask::fromFields({QStringLiteral("current.temperature")});
    QVERIFY(leaf.wants(QStringLiteral("current.temperature")));
    QVERIFY(!leaf.wants(QStringLiteral("current.windSpeed")));
    QVERIFY(leaf.wantsAnyUnder(QStringLiteral("current")));
}

void TestWireSnapshot::maskTrimsWhatItDidNotAskFor()
{
    // This is the entire justification for the mask existing: a wind rose must
    // not be sent 408 hourly points.
    const QJsonObject snap = wire::buildSnapshot(
        makeSource(), wire::FieldMask::fromFields({QStringLiteral("current.windSpeed")}));

    QVERIFY(!snap.contains(QStringLiteral("hourly")));
    QVERIFY(!snap.contains(QStringLiteral("daily")));
    QVERIFY(!snap.contains(QStringLiteral("place")));
    QVERIFY(!snap.contains(QStringLiteral("alerts")));

    const QJsonObject current = snap.value(QStringLiteral("current")).toObject();
    QCOMPARE(current.keys(), QStringList{QStringLiteral("windSpeed")});
}

// ---- the three rules --------------------------------------------------------

void TestWireSnapshot::seriesCarriesItsAxis()
{
    // Rule 1. Twelve numbers with no axis look perfectly usable and are wrong
    // the moment the slice starts anywhere other than where you assumed.
    const QJsonObject snap = wire::buildSnapshot(
        makeSource(), wire::FieldMask::fromFields({QStringLiteral("hourly.temperature")}));

    const QJsonObject hourly = snap.value(QStringLiteral("hourly")).toObject();
    QVERIFY2(hourly.contains(QStringLiteral("time")),
             "hourly.time must travel even when it was not asked for");

    const QJsonArray time = hourly.value(QStringLiteral("time")).toArray();
    const QJsonArray temp = hourly.value(QStringLiteral("temperature")).toArray();
    QCOMPARE(time.size(), temp.size());
    QCOMPARE(time.size(), 12);
}

void TestWireSnapshot::absentReadingIsNullNotZero()
{
    // Rule 2. Drawing 0 mm where the provider said nothing reports a dry hour
    // there is no evidence for.
    const QJsonObject snap = wire::buildSnapshot(
        makeSource(), wire::FieldMask::fromFields({QStringLiteral("hourly.precipitation")}));

    const QJsonArray precip =
        snap.value(QStringLiteral("hourly")).toObject().value(QStringLiteral("precipitation")).toArray();

    // The fixture omits +2 h, and the slice starts at +0 h, so index 2.
    QCOMPARE(precip.at(0).toDouble(), 0.5);
    QVERIFY2(precip.at(2).isNull(), "an absent reading must be null, never 0");
    QVERIFY(!precip.at(2).isDouble());
}

void TestWireSnapshot::sliceStartsAtNow()
{
    // Rule 3. The forecast carries a day of the past; a widget asking for six
    // hours means the next six.
    wire::SnapshotSource source = makeSource();
    source.hours                = 6;

    const QJsonObject snap =
        wire::buildSnapshot(source, wire::FieldMask::fromFields({QStringLiteral("hourly.temperature")}));
    const QJsonArray temp =
        snap.value(QStringLiteral("hourly")).toObject().value(QStringLiteral("temperature")).toArray();

    QCOMPARE(temp.size(), 6);
    // Temperature was seeded with the hour offset, so this says which hour.
    QCOMPARE(temp.at(0).toDouble(), 0.0);
    QCOMPARE(temp.at(5).toDouble(), 5.0);
}

void TestWireSnapshot::dailySliceStartsToday()
{
    wire::SnapshotSource source = makeSource();
    source.days                 = 3;

    const QJsonObject snap =
        wire::buildSnapshot(source, wire::FieldMask::fromFields({QStringLiteral("daily.temperatureMax")}));
    const QJsonObject daily = snap.value(QStringLiteral("daily")).toObject();

    QCOMPARE(daily.value(QStringLiteral("date")).toArray().size(), 3);
    QCOMPARE(daily.value(QStringLiteral("date")).toArray().at(0).toString(),
             kNow.date().toString(Qt::ISODate));
    // Seeded with the day offset: today is 0, not yesterday's -1.
    QCOMPARE(daily.value(QStringLiteral("temperatureMax")).toArray().at(0).toDouble(), 0.0);
}

// ---- what always travels ----------------------------------------------------

void TestWireSnapshot::alwaysCarriesEnoughToBeJudged()
{
    const QJsonObject snap = wire::buildSnapshot(
        makeSource(), wire::FieldMask::fromFields({QStringLiteral("current.temperature")}));

    // A reader has to know how old this is and whether it understands the
    // shape before it reads a number out of it.
    QCOMPARE(snap.value(QStringLiteral("schema")).toInt(), wire::kSchemaVersion);
    QCOMPARE(snap.value(QStringLiteral("placeId")).toString(), QStringLiteral("toronto"));
    QVERIFY(snap.contains(QStringLiteral("generatedAt")));
    QCOMPARE(snap.value(QStringLiteral("state")).toString(), QStringLiteral("live"));

    // fetchedAt is what "updated 40 minutes ago" is computed from, and it is
    // why a dead daemon leaves a stale reading rather than a blank tile.
    QVERIFY(snap.contains(QStringLiteral("fetchedAt")));

    wire::SnapshotSource cached = makeSource();
    cached.fromCache            = true;
    QCOMPARE(wire::buildSnapshot(cached, wire::FieldMask::everything())
                 .value(QStringLiteral("state"))
                 .toString(),
             QStringLiteral("cached"));
}

void TestWireSnapshot::alertsKnownSeparatesEmptyFromUnasked()
{
    // "Nothing is in force" and "we have not managed to ask" are different
    // claims, and a widget must not make the first when it means the second.
    const wire::FieldMask mask = wire::FieldMask::fromFields({QStringLiteral("alerts")});

    wire::SnapshotSource unasked = makeSource();
    const QJsonObject    a       = wire::buildSnapshot(unasked, mask);
    QCOMPARE(a.value(QStringLiteral("alerts")).toArray().size(), 0);
    QCOMPARE(a.value(QStringLiteral("alertsKnown")).toBool(), false);

    wire::SnapshotSource asked = makeSource();
    asked.alerts.fetchedAt     = kNow;   // asked, and the answer was none
    const QJsonObject b        = wire::buildSnapshot(asked, mask);
    QCOMPARE(b.value(QStringLiteral("alerts")).toArray().size(), 0);
    QCOMPARE(b.value(QStringLiteral("alertsKnown")).toBool(), true);
}

// ---- the catalogue ----------------------------------------------------------

void TestWireSnapshot::catalogueIsWellFormed()
{
    const QJsonObject root = catalogue();
    QVERIFY2(!root.isEmpty(), "widgets/catalogue.json is missing or not valid JSON");
    QCOMPARE(root.value(QStringLiteral("schema")).toInt(), wire::kSchemaVersion);

    const QJsonArray widgets = root.value(QStringLiteral("widgets")).toArray();
    QVERIFY(!widgets.isEmpty());

    QSet<QString> ids;
    for (const QJsonValue &value : widgets) {
        const QJsonObject w  = value.toObject();
        const QString     id = w.value(QStringLiteral("id")).toString();

        QVERIFY2(!id.isEmpty(), "every widget needs an id");
        QVERIFY2(!ids.contains(id), qPrintable(QStringLiteral("duplicate widget id: %1").arg(id)));
        ids.insert(id);

        QVERIFY2(!w.value(QStringLiteral("title")).toString().isEmpty(),
                 qPrintable(QStringLiteral("%1 has no title").arg(id)));
        QVERIFY2(!w.value(QStringLiteral("fields")).toArray().isEmpty(),
                 qPrintable(QStringLiteral("%1 asks for no fields").arg(id)));

        const QJsonArray size = w.value(QStringLiteral("size")).toArray();
        const QJsonArray min  = w.value(QStringLiteral("minSize")).toArray();
        QCOMPARE(size.size(), 2);
        QCOMPARE(min.size(), 2);
        QVERIFY2(size.at(0).toInt() >= min.at(0).toInt() && size.at(1).toInt() >= min.at(1).toInt(),
                 qPrintable(QStringLiteral("%1 has a default size below its minimum").arg(id)));
    }
}

void TestWireSnapshot::catalogueFieldsAreAllEncodable()
{
    // The point of the file. A typo in `fields` costs a widget its data and
    // looks like a rendering bug; here it is a failure that names the widget
    // and the field.
    wire::SnapshotSource full = makeSource();
    full.hours                = -1;
    full.days                 = -1;
    full.alerts.fetchedAt     = kNow;

    const QSet<QString> encodable =
        flatten(wire::buildSnapshot(full, wire::FieldMask::everything()));
    QVERIFY(!encodable.isEmpty());

    const QJsonArray widgets = catalogue().value(QStringLiteral("widgets")).toArray();
    for (const QJsonValue &value : widgets) {
        const QJsonObject w  = value.toObject();
        const QString     id = w.value(QStringLiteral("id")).toString();
        for (const QJsonValue &field : w.value(QStringLiteral("fields")).toArray()) {
            const QString path = field.toString();
            QVERIFY2(encodable.contains(path),
                     qPrintable(QStringLiteral("widget '%1' asks for '%2', which the encoder "
                                               "never emits")
                                    .arg(id, path)));
        }
    }
}

void TestWireSnapshot::catalogueHorizonsMatchItsFields()
{
    // Both directions, because both are silent. A widget asking for
    // hourly.temperature with hours=0 receives an axis and nothing else; one
    // declaring hours=24 with no hourly field receives an axis for nothing.
    const QJsonArray widgets = catalogue().value(QStringLiteral("widgets")).toArray();

    for (const QJsonValue &value : widgets) {
        const QJsonObject w  = value.toObject();
        const QString     id = w.value(QStringLiteral("id")).toString();

        bool wantsHourly = false;
        bool wantsDaily  = false;
        for (const QJsonValue &field : w.value(QStringLiteral("fields")).toArray()) {
            const QString path = field.toString();
            wantsHourly = wantsHourly || path.startsWith(QStringLiteral("hourly"));
            wantsDaily  = wantsDaily || path.startsWith(QStringLiteral("daily"));
        }

        const int hours = w.value(QStringLiteral("hours")).toInt();
        const int days  = w.value(QStringLiteral("days")).toInt();

        QVERIFY2(wantsHourly == (hours != 0),
                 qPrintable(QStringLiteral("widget '%1': hours=%2 but %3 hourly fields")
                                .arg(id)
                                .arg(hours)
                                .arg(wantsHourly ? "some" : "no")));
        QVERIFY2(wantsDaily == (days != 0),
                 qPrintable(QStringLiteral("widget '%1': days=%2 but %3 daily fields")
                                .arg(id)
                                .arg(days)
                                .arg(wantsDaily ? "some" : "no")));
    }
}

QTEST_MAIN(TestWireSnapshot)
#include "tst_wiresnapshot.moc"
