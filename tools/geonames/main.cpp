// SPDX-FileCopyrightText: 2026 Jowi Aoun
// SPDX-License-Identifier: GPL-3.0-or-later
//
// clima-geocode — look at what geocoding actually returns.
//
//   clima-geocode Kigali                  forward search, over the network
//   clima-geocode "New York" --count 3    ditto, fewer rows
//   clima-geocode 43.65,-79.38            reverse, from the bundled index
//   clima-geocode --id 6167865            resolve one place by GeoNames id
//
// An argument that parses as "lat,lon" is a reverse lookup and anything else
// is a search, because that is the distinction a person holds in their head
// when they run this and a --mode flag would be one more thing to remember.
//
// ---- what this is for, given that the suite already passes -------------------
//
// Two things a test cannot do.
//
// The first is the live half. tests/tst_geocoding.cpp runs against four
// recorded responses and a loopback stub, which proves the parser is
// consistent with what Open-Meteo sent on 2026-07-31 and will go on proving it
// after Open-Meteo renames a field. A recorded fixture guards against
// regression in us, not against drift in them.
//
// The second is the eyeball half of reverse geocoding. "Toronto" is assertable;
// "does this answer feel right for where I am standing" is not, and the
// footprint rule in geonamesindex.h is exactly the kind of heuristic that
// passes four test coordinates and says something silly about the fifth. This
// prints the distance and whether the point was inside the settlement's
// modelled reach, so a wrong answer is legible rather than merely wrong.

#include "libclima/cache/cachestore.h"
#include "libclima/core/clock.h"
#include "libclima/net/httpclient.h"
#include "libclima/providers/geocoding/offlinereversegeocoder.h"
#include "libclima/providers/geocoding/openmeteogeocoder.h"

#include <QCoreApplication>
#include <QFutureWatcher>
#include <QRegularExpression>
#include <QTextStream>
#include <QTimer>

using namespace clima;

namespace {

QTextStream &out()
{
    static QTextStream stream(stdout);
    return stream;
}

void printPlace(const Place &place, const QString &prefix = QStringLiteral("  "))
{
    out() << prefix << place.label() << Qt::endl;
    out() << prefix << "  geonames id  " << place.geonamesId << Qt::endl;
    out() << prefix << "  coordinate   " << place.coordinate.toKeyString()
          << QStringLiteral("  (") << QString::number(place.coordinate.latitude, 'f', 5)
          << QStringLiteral(", ") << QString::number(place.coordinate.longitude, 'f', 5)
          << QStringLiteral(" as given)") << Qt::endl;
    out() << prefix << "  region       " << place.region() << Qt::endl;
    out() << prefix << "  country code " << place.countryCode << Qt::endl;
    out() << prefix << "  timezone     " << place.timezone << Qt::endl;
    if (place.elevationMetres)
        out() << prefix << "  elevation    " << *place.elevationMetres << " m" << Qt::endl;
}

// "43.65,-79.38" and nothing looser. A place called "1,2" is not a place, and
// a search string that happened to contain a comma should not silently become
// a coordinate.
bool parseCoordinate(const QString &text, Coordinate *coordinate)
{
    static const QRegularExpression pattern(
        QStringLiteral("^\\s*(-?\\d+(?:\\.\\d+)?)\\s*,\\s*(-?\\d+(?:\\.\\d+)?)\\s*$"));

    const QRegularExpressionMatch match = pattern.match(text);
    if (!match.hasMatch())
        return false;

    *coordinate = Coordinate{ match.captured(1).toDouble(), match.captured(2).toDouble() };
    return true;
}

int reverse(const Coordinate &at)
{
    OfflineReverseGeocoder geocoder;

    const Status loaded = geocoder.load();
    if (!loaded) {
        out() << "could not load the bundled index: " << loaded.error().toString() << Qt::endl;
        return 1;
    }

    out() << "reverse " << at.toKeyString() << QStringLiteral("  (")
          << geocoder.cityCount() << QStringLiteral(" places, offline, no request sent)")
          << Qt::endl;

    const Result<ReverseMatch> found = geocoder.reverse(at);
    if (!found) {
        // The interesting failure. A point in the ocean is Unsupported rather
        // than an error, and the app is expected to show the coordinate.
        out() << "  " << found.error().toString() << Qt::endl;
        return found.errorKind() == ErrorKind::Unsupported ? 0 : 1;
    }

    printPlace(found.value().place);
    out() << QStringLiteral("    distance     ")
          << QString::number(found.value().distanceKm, 'f', 2) << QStringLiteral(" km")
          << Qt::endl;
    out() << QStringLiteral("    inside it    ")
          << (found.value().insideFootprint ? QStringLiteral("yes")
                                            : QStringLiteral("no — nearest, not enclosing"))
          << Qt::endl;
    return 0;
}

} // namespace

int main(int argc, char **argv)
{
    QCoreApplication app(argc, argv);

    QStringList arguments = QCoreApplication::arguments();
    arguments.removeFirst();

    if (arguments.isEmpty()) {
        out() << "usage: clima-geocode <name> [--count N] [--language xx]" << Qt::endl;
        out() << "       clima-geocode <lat>,<lon>" << Qt::endl;
        out() << "       clima-geocode --id <geonames id>" << Qt::endl;
        return 2;
    }

    int     count = 10;
    QString language = QStringLiteral("en");
    qint64  identifier = 0;
    QString name;

    for (int i = 0; i < arguments.size(); ++i) {
        const QString &argument = arguments.at(i);
        if (argument == QStringLiteral("--count") && i + 1 < arguments.size())
            count = arguments.at(++i).toInt();
        else if (argument == QStringLiteral("--language") && i + 1 < arguments.size())
            language = arguments.at(++i);
        else if (argument == QStringLiteral("--id") && i + 1 < arguments.size())
            identifier = arguments.at(++i).toLongLong();
        else if (name.isEmpty())
            name = argument;
    }

    // Reverse first, and without an event loop: it is a table lookup, it
    // cannot fail slowly, and treating it like a network call would be the
    // whole reason igeocoder.h has two interfaces instead of one.
    Coordinate coordinate;
    if (identifier == 0 && parseCoordinate(name, &coordinate))
        return reverse(coordinate);

    // Everything below reaches the internet. That is the point of this
    // executable and the reason it is not a test.
    SystemClock clock;
    HttpClient  http(&clock);

    CacheStore cache(&clock);
    const Status opened = cache.open(CacheStore::defaultDatabasePath());
    if (opened) {
        http.setValidatorStore(&cache);
    } else {
        out() << "note: running without a cache (" << opened.error().message() << ")" << Qt::endl;
    }

    OpenMeteoGeocoder geocoder(&http, opened ? &cache : nullptr, &clock);

    out() << "user-agent   " << QString::fromLatin1(HttpClient::userAgent()) << Qt::endl;
    for (const QString &credit : geocoder.attribution())
        out() << "attribution  " << credit << Qt::endl;
    out() << Qt::endl;

    int status = 0;

    if (identifier != 0) {
        out() << "get id=" << identifier << Qt::endl;

        auto *watcher = new QFutureWatcher<Result<Place>>(&app);
        QObject::connect(watcher, &QFutureWatcher<Result<Place>>::finished, &app, [&] {
            const Result<Place> found = watcher->result();
            if (!found) {
                out() << "  " << found.error().toString() << Qt::endl;
                status = 1;
            } else {
                printPlace(found.value());
            }
            QCoreApplication::quit();
        });
        watcher->setFuture(geocoder.resolve(identifier));
    } else {
        out() << "search " << name << QStringLiteral("  (count ") << count
              << QStringLiteral(", language ") << language << QStringLiteral(")") << Qt::endl;

        GeocodeQuery query;
        query.name = name;
        query.count = count;
        query.language = language;

        auto *watcher = new QFutureWatcher<Result<QList<Place>>>(&app);
        QObject::connect(watcher, &QFutureWatcher<Result<QList<Place>>>::finished, &app, [&] {
            const Result<QList<Place>> found = watcher->result();
            if (!found) {
                out() << "  " << found.error().toString() << Qt::endl;
                status = 1;
            } else if (found.value().isEmpty()) {
                out() << "  no matches" << Qt::endl;
            } else {
                for (const Place &place : found.value()) {
                    printPlace(place);
                    out() << Qt::endl;
                }
            }
            QCoreApplication::quit();
        });
        watcher->setFuture(geocoder.search(query));
    }

    // A hard stop, so that a hung request ends the process rather than the
    // person running it.
    QTimer::singleShot(30000, &app, [&] {
        out() << "  timed out after 30 s" << Qt::endl;
        status = 1;
        QCoreApplication::quit();
    });

    QCoreApplication::exec();
    out().flush();
    return status;
}
