// SPDX-FileCopyrightText: 2026 Jowi Aoun
// SPDX-License-Identifier: GPL-3.0-or-later
//
// Coordinate rounding and the request key, as arithmetic and string handling.
// No client, no socket, no event loop — this is the layer everything above it
// inherits its notion of "the same request" from, and it is worth asserting on
// its own before it is asserted through three other classes.

#include "libclima/domain/coordinate.h"
#include "libclima/net/requestkey.h"

#include <QTest>

using namespace clima;

class TestRequestKey : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void roundingIsHalfAwayFromZeroAndSymmetric();
    void formattingIsCLocaleAndFixedWidth();
    void validityRejectsWhatIsNotOnEarth();

    void oneKeyPerRoundedCell();
    void adjacentCellsAreDifferentKeys();
    void parameterOrderDoesNotMatter();
    void providerAndEndpointSeparateTheNamespaces();
    void theHostIsPartOfTheKey();
    void theKeyStaysReadable();

    void composedUrlCarriesTheRoundedCoordinate();
    void composedUrlUsesTheProvidersParameterNames();
    void aRequestWithNoCoordinateGetsNoCoordinateParameters();
};

void TestRequestKey::roundingIsHalfAwayFromZeroAndSymmetric()
{
    QCOMPARE(Coordinate({ 52.520008, 13.404954 }).rounded().latitudeString(),
             QStringLiteral("52.5200"));
    QCOMPARE(Coordinate({ 52.520008, 13.404954 }).rounded().longitudeString(),
             QStringLiteral("13.4050"));

    // Symmetric about zero. Banker's rounding would send +0.00005 and -0.00005
    // to cells that are not mirror images, which is a cache miss whose cause
    // depends on which side of the equator you are standing on.
    QCOMPARE(Coordinate({ 0.00005, -0.00005 }).rounded().toKeyString(),
             QStringLiteral("0.0001,-0.0001"));

    QCOMPARE(Coordinate({ -33.86882, 151.20930 }).rounded().toKeyString(),
             QStringLiteral("-33.8688,151.2093"));
}

void TestRequestKey::formattingIsCLocaleAndFixedWidth()
{
    // Fixed width, so 13.405 is "13.4050" and not "13.405". Two spellings of
    // one coordinate are two cache keys.
    QCOMPARE(Coordinate({ 52.52, 13.405 }).toKeyString(), QStringLiteral("52.5200,13.4050"));
    QCOMPARE(Coordinate({ 0.0, 0.0 }).toKeyString(), QStringLiteral("0.0000,0.0000"));

    // C locale, always. A comma decimal separator in a URL query is a
    // different request on a French machine than on an English one, and it is
    // the kind of bug that only reproduces on somebody else's laptop.
    QVERIFY(!Coordinate({ 52.52, 13.405 }).toKeyString().contains(QStringLiteral("52,52")));
}

void TestRequestKey::validityRejectsWhatIsNotOnEarth()
{
    QVERIFY(Coordinate({ 52.52, 13.405 }).isValid());
    QVERIFY(Coordinate({ -90.0, -180.0 }).isValid());
    QVERIFY(Coordinate({ 90.0, 180.0 }).isValid());

    QVERIFY(!Coordinate({ 90.1, 0.0 }).isValid());
    QVERIFY(!Coordinate({ 0.0, 180.1 }).isValid());
    QVERIFY(!Coordinate({ qQNaN(), 0.0 }).isValid());
    QVERIFY(!Coordinate({ qInf(), 0.0 }).isValid());
}

namespace {

HttpRequest berlinForecast()
{
    HttpRequest request;
    request.providerId = QStringLiteral("open-meteo");
    request.endpoint = QStringLiteral("forecast");
    request.url = QUrl(QStringLiteral("http://127.0.0.1:8080/v1/forecast"));
    request.coordinate = Coordinate{ 52.520008, 13.404954 };
    request.parameters = { { QStringLiteral("hourly"), QStringLiteral("temperature_2m") } };
    return request;
}

} // namespace

void TestRequestKey::oneKeyPerRoundedCell()
{
    // The map drag, at the level where it is decided. Six coordinates inside
    // two metres of each other are one key, and therefore one in-flight
    // request, one cache row and one entry in somebody's rate-limit ledger.
    const QList<Coordinate> drag = {
        { 52.520008, 13.404954 }, { 52.5200081, 13.4049541 }, { 52.52001, 13.40495 },
        { 52.5199951, 13.4050449 },
    };

    const QString expected = [&]() {
        HttpRequest request = berlinForecast();
        request.coordinate = drag.at(0);
        return RequestKey::forRequest(request).toString();
    }();

    for (const Coordinate &coordinate : drag) {
        HttpRequest request = berlinForecast();
        request.coordinate = coordinate;
        QCOMPARE(RequestKey::forRequest(request).toString(), expected);
    }
}

void TestRequestKey::adjacentCellsAreDifferentKeys()
{
    // Rounding must not collapse everything. A move of 0.001° — about 110 m,
    // and enough to change grid cell for a 2 km convection-allowing model — is
    // a different request.
    HttpRequest here = berlinForecast();
    HttpRequest there = berlinForecast();
    there.coordinate = Coordinate{ 52.521008, 13.404954 };

    QVERIFY(RequestKey::forRequest(here) != RequestKey::forRequest(there));
}

void TestRequestKey::parameterOrderDoesNotMatter()
{
    HttpRequest a = berlinForecast();
    a.parameters = { { QStringLiteral("hourly"), QStringLiteral("temperature_2m") },
                     { QStringLiteral("daily"), QStringLiteral("sunrise") },
                     { QStringLiteral("timezone"), QStringLiteral("auto") } };

    HttpRequest b = berlinForecast();
    b.parameters = { { QStringLiteral("timezone"), QStringLiteral("auto") },
                     { QStringLiteral("hourly"), QStringLiteral("temperature_2m") },
                     { QStringLiteral("daily"), QStringLiteral("sunrise") } };

    QCOMPARE(RequestKey::forRequest(a), RequestKey::forRequest(b));

    // But a different parameter set is a different request.
    b.parameters.append({ QStringLiteral("models"), QStringLiteral("icon_d2") });
    QVERIFY(RequestKey::forRequest(a) != RequestKey::forRequest(b));
}

void TestRequestKey::providerAndEndpointSeparateTheNamespaces()
{
    HttpRequest openMeteo = berlinForecast();
    HttpRequest metNo = berlinForecast();
    metNo.providerId = QStringLiteral("met-no");

    QVERIFY(RequestKey::forRequest(openMeteo) != RequestKey::forRequest(metNo));

    HttpRequest airQuality = berlinForecast();
    airQuality.endpoint = QStringLiteral("air-quality");
    QVERIFY(RequestKey::forRequest(openMeteo) != RequestKey::forRequest(airQuality));
}

void TestRequestKey::theHostIsPartOfTheKey()
{
    // A self-hosted Open-Meteo (documented and realistic — §2.8) answers the
    // same endpoint at a different host, and it is not the same answer.
    HttpRequest hosted = berlinForecast();
    hosted.url = QUrl(QStringLiteral("http://127.0.0.1:9090/v1/forecast"));

    QVERIFY(RequestKey::forRequest(berlinForecast()) != RequestKey::forRequest(hosted));
}

void TestRequestKey::theKeyStaysReadable()
{
    // A key is read by a human about as often as by a machine: it goes in log
    // lines and it is what you would sort a cache file by. The parts a human
    // recognises stay legible; only the parameter tail is a digest.
    const QString key = RequestKey::forRequest(berlinForecast()).toString();

    QVERIFY2(key.startsWith(QStringLiteral("open-meteo/forecast@52.5200,13.4050#")),
             qPrintable(key));
    QCOMPARE(key.section(QLatin1Char('#'), 1).size(), 16);
}

void TestRequestKey::composedUrlCarriesTheRoundedCoordinate()
{
    const QUrl url = composeUrl(berlinForecast());

    QCOMPARE(url.host(), QStringLiteral("127.0.0.1"));
    QCOMPARE(url.path(), QStringLiteral("/v1/forecast"));

    const QString query = url.query();
    QVERIFY2(query.contains(QStringLiteral("latitude=52.5200")), qPrintable(query));
    QVERIFY2(query.contains(QStringLiteral("longitude=13.4050")), qPrintable(query));

    // The precision the caller had is not what goes on the wire. MET Norway's
    // terms ask for four decimals by name.
    QVERIFY2(!query.contains(QStringLiteral("52.520008")), qPrintable(query));
}

void TestRequestKey::composedUrlUsesTheProvidersParameterNames()
{
    HttpRequest metNo = berlinForecast();
    metNo.latitudeParameter = QStringLiteral("lat");
    metNo.longitudeParameter = QStringLiteral("lon");

    const QString query = composeUrl(metNo).query();
    QVERIFY2(query.contains(QStringLiteral("lat=52.5200")), qPrintable(query));
    QVERIFY2(query.contains(QStringLiteral("lon=13.4050")), qPrintable(query));
}

void TestRequestKey::aRequestWithNoCoordinateGetsNoCoordinateParameters()
{
    // A radar timeline manifest, a basemap tile: not everything is about a
    // point on the earth.
    HttpRequest manifest;
    manifest.providerId = QStringLiteral("librewxr");
    manifest.endpoint = QStringLiteral("timeline");
    manifest.url = QUrl(QStringLiteral("http://127.0.0.1:8080/public/weather-maps.json"));

    const QUrl url = composeUrl(manifest);
    QVERIFY(!url.hasQuery());
    QVERIFY(RequestKey::forRequest(manifest).toString().contains(QStringLiteral("@-#")));
}

QTEST_MAIN(TestRequestKey)
#include "tst_requestkey.moc"
