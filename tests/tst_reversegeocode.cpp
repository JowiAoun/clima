// SPDX-FileCopyrightText: 2026 Jowi Aoun
// SPDX-License-Identifier: GPL-3.0-or-later
//
// The offline reverse geocoder, against the index that ships in the binary.
//
// Every assertion here is exact. There is no network, no clock and no
// randomness in the answer, which is the whole argument for the index being
// bundled rather than fetched — a golden image with a place name in the corner
// is only golden if that name cannot change under it.
//
// ---- the four coordinates, and why each one is here -------------------------
//
//   Toronto      the case that plain nearest-neighbour gets wrong. The nearest
//                row to Yonge and Queen is Moss Park, 880 m away — a
//                neighbourhood. This asserts that the answer is Toronto.
//
//   Reykjavík    a small capital with a larger suburb next door and a
//                non-ASCII name. Kópavogur is nearer to nothing in particular
//                and the accent has to survive the packer.
//
//   Singapore    a city-state whose GeoNames rows are almost all "New Towns"
//                with six-figure populations. Nearest gives Ang Mo Kio New
//                Town; the footprint rule gives Singapore.
//
//   Point Nemo   48.8767 S, 123.3933 W — the oceanic pole of inaccessibility,
//                2 690 km from the nearest land. This is the "nowhere" case,
//                and what it returns is a decision this file records:
//                ErrorKind::Unsupported, not a city on another continent.

#include "libclima/providers/geocoding/geocodingparser.h"
#include "libclima/providers/geocoding/geonamesindex.h"
#include "libclima/providers/geocoding/offlinereversegeocoder.h"

#include <QFile>
#include <QFileInfo>
#include <QTest>

using namespace clima;

namespace {

// The four decimals every request in this engine is quantised to. The index
// stores exactly this many, so a place found here and the same place found by
// searching Open-Meteo round to the identical double.
Coordinate at(double latitude, double longitude)
{
    return Coordinate{ latitude, longitude };
}

QByteArray fixture(const QString &name)
{
    QFile file(QStringLiteral(CLIMA_SOURCE_DIR) + QStringLiteral("/tests/fixtures/geocoding/")
               + name);
    if (!file.open(QIODevice::ReadOnly))
        return {};
    return file.readAll();
}

} // namespace

class TestReverseGeocode : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void initTestCase();

    void theBundledIndexIsPresentAndSmall();

    void torontoBeatsTheNeighbourhoodNextDoor();
    void reykjavikKeepsItsAccentAndBeatsItsSuburb();
    void singaporeBeatsItsNewTowns();
    void kigaliIsExactBecauseTheCityCentreIsRightThere();

    void aPointInTheOceanIsUnsupported();
    void theCutoffIsWhatMakesItUnsupported();

    void aDetectedPlaceAndASearchedPlaceAreTheSameEntity();

    void anInvalidCoordinateIsRefused();
    void anUnloadedIndexRefusesRatherThanReturningNothing();
    void truncatedBytesAreRejectedRatherThanRead();

private:
    OfflineReverseGeocoder m_geocoder;
};

void TestReverseGeocode::initTestCase()
{
    const Status status = m_geocoder.load();
    QVERIFY2(status.hasValue(), qPrintable(status.error().toString()));
    QVERIFY(m_geocoder.isLoaded());
}

void TestReverseGeocode::theBundledIndexIsPresentAndSmall()
{
    // The size is asserted, not merely reported. The packed index is committed
    // to the repository and compiled into every binary we ship, so a change
    // that doubles it should fail a test rather than turn up in a download
    // size six weeks later. The bound is deliberately loose — this is a
    // regression guard, not a budget — and docs/03-tech-stack.md §3.4 gives the
    // whole binary 15 MB.
    QFile file(GeonamesIndex::bundledResourcePath());
    QVERIFY2(file.open(QIODevice::ReadOnly), "the packed index is not in the binary");

    const qint64 bytes = file.size();
    QVERIFY2(bytes > 300 * 1024,
             qPrintable(QStringLiteral("the index is only %1 bytes — it is truncated").arg(bytes)));
    QVERIFY2(bytes < 512 * 1024,
             qPrintable(QStringLiteral("the index has grown to %1 bytes").arg(bytes)));

    // cities15000 minus the feature codes that are not a place you can stand
    // in: PPLX (a section of a city), PPLH, PPLQ, PPLW (historical, abandoned,
    // destroyed). See tools/geonames/pack.mjs.
    QCOMPARE(m_geocoder.cityCount(), 31673);
}

void TestReverseGeocode::torontoBeatsTheNeighbourhoodNextDoor()
{
    // Yonge and Queen. The nearest row in the dataset is Moss Park at 880 m,
    // then Bay Street Corridor, then Church-Yonge Corridor — all of them
    // dropped as PPLX — and after that Etobicoke at 15 km. Toronto's own row
    // is 6.4 km away at the city centroid, and it wins because the point is
    // deep inside its modelled footprint.
    const Result<ReverseMatch> found = m_geocoder.reverse(at(43.65, -79.38));
    QVERIFY2(found.hasValue(), qPrintable(found.error().toString()));

    const Place &place = found.value().place;
    QCOMPARE(place.name, QStringLiteral("Toronto"));
    QCOMPARE(place.admin1, QStringLiteral("Ontario"));
    QCOMPARE(place.countryCode, QStringLiteral("CA"));
    QCOMPARE(place.country, QStringLiteral("Canada"));
    QCOMPARE(place.timezone, QStringLiteral("America/Toronto"));
    QCOMPARE(place.geonamesId, Q_INT64_C(6167865));

    QCOMPARE(place.label(), QStringLiteral("Toronto, Ontario"));
    QCOMPARE(place.region(), QStringLiteral("Ontario, Canada"));

    QVERIFY(found.value().insideFootprint);
    QVERIFY(found.value().distanceKm > 6.0 && found.value().distanceKm < 7.0);
}

void TestReverseGeocode::reykjavikKeepsItsAccentAndBeatsItsSuburb()
{
    const Result<ReverseMatch> found = m_geocoder.reverse(at(64.1466, -21.9426));
    QVERIFY2(found.hasValue(), qPrintable(found.error().toString()));

    const Place &place = found.value().place;

    // With the accent. The packer takes GeoNames' `name` column and not its
    // `asciiname`, because a path that folds accents and a path that does not
    // produce two places as far as a string comparison is concerned.
    QCOMPARE(place.name, QString::fromUtf8("Reykjavík"));
    QCOMPARE(place.countryCode, QStringLiteral("IS"));
    QCOMPARE(place.country, QStringLiteral("Iceland"));
    QCOMPARE(place.admin1, QStringLiteral("Capital Region"));
    QCOMPARE(place.timezone, QStringLiteral("Atlantic/Reykjavik"));
    QCOMPARE(place.geonamesId, Q_INT64_C(3413829));

    // Kópavogur is 4.1 km away and Reykjavík is 2.6 km away, so nearest agrees
    // here — but the ratio is what decides, and Kópavogur's is 1.6 against
    // Reykjavík's 0.6. The two rules agreeing is worth asserting precisely
    // because in Toronto and Singapore they do not.
    QVERIFY(found.value().insideFootprint);
}

void TestReverseGeocode::singaporeBeatsItsNewTowns()
{
    const Result<ReverseMatch> found = m_geocoder.reverse(at(1.3521, 103.8198));
    QVERIFY2(found.hasValue(), qPrintable(found.error().toString()));

    const Place &place = found.value().place;

    // Ang Mo Kio New Town is 3.8 km away with 159 340 people; Singapore is
    // 7.7 km away with 5 638 700. Nearest picks the New Town, the footprint
    // rule picks Singapore, and Singapore is the answer to "where am I".
    QCOMPARE(place.name, QStringLiteral("Singapore"));
    QCOMPARE(place.countryCode, QStringLiteral("SG"));
    QCOMPARE(place.country, QStringLiteral("Singapore"));
    QCOMPARE(place.geonamesId, Q_INT64_C(1880252));

    // A country with no first-level divisions. The label must not end up as
    // "Singapore, " — Place::label() drops the comma, and the index stores the
    // empty admin1 as a real table entry rather than as a sentinel.
    QVERIFY(place.admin1.isEmpty());
    QCOMPARE(place.label(), QStringLiteral("Singapore"));
    QCOMPARE(place.region(), QStringLiteral("Singapore"));
}

void TestReverseGeocode::kigaliIsExactBecauseTheCityCentreIsRightThere()
{
    const Result<ReverseMatch> found = m_geocoder.reverse(at(-1.9441, 30.0619));
    QVERIFY2(found.hasValue(), qPrintable(found.error().toString()));

    QCOMPARE(found.value().place.name, QStringLiteral("Kigali"));
    QCOMPARE(found.value().place.admin1, QStringLiteral("Kigali"));
    QCOMPARE(found.value().place.country, QStringLiteral("Rwanda"));
    QCOMPARE(found.value().place.geonamesId, Q_INT64_C(202061));

    // The admin1 name equals the city name, which is the one case where
    // "Kigali, Kigali" would be the obvious label and the wrong one.
    QCOMPARE(found.value().place.label(), QStringLiteral("Kigali"));
    QCOMPARE(found.value().place.region(), QStringLiteral("Kigali, Rwanda"));

    // Southern hemisphere and a positive longitude: the quadrant that a
    // rounding mode which is not symmetric about the equator would get wrong.
    QVERIFY(found.value().place.coordinate.latitude < 0.0);
    QVERIFY(found.value().place.coordinate.longitude > 0.0);
}

void TestReverseGeocode::aPointInTheOceanIsUnsupported()
{
    // Point Nemo. 2 690 km from the nearest land, which is further than any
    // cutoff a weather app could justify.
    const Result<ReverseMatch> found = m_geocoder.reverse(at(-48.8767, -123.3933));

    QVERIFY2(!found.hasValue(), "the middle of the South Pacific resolved to a city");

    // Unsupported and not NotFound. docs/04-architecture.md §4.4 uses
    // Unsupported for "the provider does not cover this coordinate", and the
    // difference matters: NotFound invites the caller to try another provider,
    // and there is no provider for which the middle of the Pacific is a town.
    // The UI shows the coordinate instead — §4.4 again: a provider that
    // returns nothing must make the UI hide the feature, not show a broken one.
    QCOMPARE(found.errorKind(), ErrorKind::Unsupported);
    QVERIFY2(found.error().message().contains(QStringLiteral("250 km")),
             qPrintable(found.error().message()));
    QVERIFY2(found.error().message().contains(QStringLiteral("-48.8767")),
             qPrintable(found.error().message()));
}

void TestReverseGeocode::theCutoffIsWhatMakesItUnsupported()
{
    // The same point, with the cutoff opened wide enough to reach land. This
    // is what proves the previous test is asserting a policy rather than a
    // hole in the dataset: the index does know about places down there, and
    // the geocoder declines to name one because it is too far to mean
    // anything.
    OfflineReverseGeocoder wide;
    QVERIFY(wide.load().hasValue());
    wide.setMaximumDistanceKm(4000.0);

    const Result<ReverseMatch> found = wide.reverse(at(-48.8767, -123.3933));
    QVERIFY2(found.hasValue(), qPrintable(found.error().toString()));

    // Whatever it found is a long way away and the point is not inside it.
    QVERIFY(found.value().distanceKm > 1000.0);
    QVERIFY(!found.value().insideFootprint);
}

void TestReverseGeocode::aDetectedPlaceAndASearchedPlaceAreTheSameEntity()
{
    // The payoff, and the fourth reason the reverse index is GeoNames rather
    // than Nominatim.
    //
    // The fixture is a recorded response from Open-Meteo's geocoding API — a
    // different service, a different host, a different code path — and the
    // Place it parses to has to be the same place as the one the bundled index
    // returns for a coordinate downtown. Nominatim would have answered with an
    // OSM relation id, which has no correspondence to a GeoNames id at all,
    // and the app could never have known the two were one place.
    const Result<Place> searched = parseGeocodingPlace(fixture(QStringLiteral("get-toronto.json")));
    QVERIFY2(searched.hasValue(), qPrintable(searched.error().toString()));

    const Result<ReverseMatch> detected = m_geocoder.reverse(at(43.65, -79.38));
    QVERIFY(detected.hasValue());

    const Place &a = searched.value();
    const Place &b = detected.value().place;

    QCOMPARE(a.geonamesId, b.geonamesId);
    QCOMPARE(a.name, b.name);
    QCOMPARE(a.admin1, b.admin1);
    QCOMPARE(a.country, b.country);
    QCOMPARE(a.countryCode, b.countryCode);
    QCOMPARE(a.timezone, b.timezone);

    // The coordinates agree to the precision every request in this engine
    // uses. The index stores four decimals because that is
    // Coordinate::keyDecimals; the API sends five; both round to the same
    // double, so the two paths produce the same cache key and the same URL.
    QCOMPARE(a.coordinate.rounded(), b.coordinate.rounded());
    QCOMPARE(a.coordinate.toKeyString(), b.coordinate.toKeyString());

    // And the model that has to decide whether to insert a second row agrees.
    QVERIFY(a.isSameEntity(b));
}

void TestReverseGeocode::anInvalidCoordinateIsRefused()
{
    QCOMPARE(m_geocoder.reverse(at(91.0, 0.0)).errorKind(), ErrorKind::Unsupported);
    QCOMPARE(m_geocoder.reverse(at(0.0, 181.0)).errorKind(), ErrorKind::Unsupported);
}

void TestReverseGeocode::anUnloadedIndexRefusesRatherThanReturningNothing()
{
    // Storage and not Unsupported, because "nobody called load()" is a
    // programming error and "there is no city here" is an answer. Reporting
    // them the same way would make a forgotten load() look like the middle of
    // the Pacific.
    OfflineReverseGeocoder cold;
    QVERIFY(!cold.isLoaded());

    const Result<ReverseMatch> found = cold.reverse(at(43.65, -79.38));
    QVERIFY(!found.hasValue());
    QCOMPARE(found.errorKind(), ErrorKind::Storage);
}

void TestReverseGeocode::truncatedBytesAreRejectedRatherThanRead()
{
    QFile file(GeonamesIndex::bundledResourcePath());
    QVERIFY(file.open(QIODevice::ReadOnly));
    const QByteArray packed = file.readAll();

    GeonamesIndex index;

    // Empty, short, and half a file. Each one has to come back as a Parse
    // error rather than as a load that half-succeeds — the columns are
    // length-prefixed varints, and a reader that trusted them would walk off
    // the end of the payload.
    QCOMPARE(index.load(QByteArray()).errorKind(), ErrorKind::Parse);
    QCOMPARE(index.load(QByteArrayLiteral("CLGX")).errorKind(), ErrorKind::Parse);
    QCOMPARE(index.load(packed.left(packed.size() / 2)).errorKind(), ErrorKind::Parse);

    // A good magic number with a version we do not know is refused by version
    // rather than by accident, so that a future format can be introduced
    // without an old binary reading it as this one.
    QByteArray future = packed;
    future[4] = char(99);
    const Status status = index.load(future);
    QCOMPARE(status.errorKind(), ErrorKind::Parse);
    QVERIFY2(status.error().message().contains(QStringLiteral("version 99")),
             qPrintable(status.error().message()));

    QVERIFY(!index.isLoaded());
}

QTEST_MAIN(TestReverseGeocode)
#include "tst_reversegeocode.moc"
