// SPDX-FileCopyrightText: 2026 Jowi Aoun
// SPDX-License-Identifier: MPL-2.0

#include "geonamesindex.h"

#include <QFile>

#include <algorithm>
#include <cmath>
#include <limits>

namespace clima {

namespace {

constexpr char     magic[] = { 'C', 'L', 'G', 'X' };
constexpr quint32  supportedFormatVersion = 1;
constexpr int      sectionCount = 12;
constexpr qsizetype headerBytes = 52 + 4 * sectionCount;

// The mean radius of the earth, IUGG. Not the equatorial radius: every
// distance here is a comparison between two candidate cities and a spherical
// earth is the right amount of model for that — the error against a proper
// geodesic is a few parts in a thousand, which is metres at the distances that
// decide anything, and a WGS84 inverse solution would be a hundred lines to
// change no answer.
constexpr double earthRadiusKm = 6371.0088;

// Spelled out rather than M_PI, which is a POSIX extension the C++ standard
// does not require and which MSVC hides behind _USE_MATH_DEFINES.
constexpr double pi = 3.14159265358979323846;
constexpr double degreesToRadians = pi / 180.0;

enum Section {
    CellStarts = 0,
    Latitudes,
    Longitudes,
    GeonamesIds,
    Reach,
    CountryIndex,
    Admin1Index,
    TimezoneIndex,
    CountryCodes,
    Admin1Names,
    TimezoneNames,
    CityNames,
};

Error malformed(const QString &what)
{
    return Error(ErrorKind::Parse,
                 QStringLiteral("the bundled GeoNames index is malformed: %1").arg(what));
}

// A cursor over one section. Everything below reads through this rather than
// indexing the payload directly, so that a truncated file is caught by the
// first read past the end instead of by a segfault three sections later.
class Cursor
{
public:
    Cursor(const char *begin, const char *end)
        : m_at(begin)
        , m_end(end)
    {
    }

    [[nodiscard]] bool atEnd() const { return m_at >= m_end; }

    bool readByte(quint8 *out)
    {
        if (m_at >= m_end)
            return false;
        *out = quint8(*m_at++);
        return true;
    }

    // LEB128. Bounded at ten septets, which is every value a 64-bit integer
    // can hold — an unbounded loop on corrupt input reads to the end of the
    // section and calls it a number.
    bool readVarint(quint64 *out)
    {
        quint64 value = 0;
        int     shift = 0;
        for (int septet = 0; septet < 10; ++septet) {
            quint8 byte = 0;
            if (!readByte(&byte))
                return false;
            value |= quint64(byte & 0x7f) << shift;
            if ((byte & 0x80) == 0) {
                *out = value;
                return true;
            }
            shift += 7;
        }
        return false;
    }

    bool readZigzag(qint64 *out)
    {
        quint64 raw = 0;
        if (!readVarint(&raw))
            return false;
        *out = qint64(raw >> 1) ^ -qint64(raw & 1);
        return true;
    }

private:
    const char *m_at;
    const char *m_end;
};

template <typename T>
bool readDeltaColumn(Cursor &cursor, QVector<T> &out, int count)
{
    out.resize(count);
    qint64 running = 0;
    for (int i = 0; i < count; ++i) {
        qint64 delta = 0;
        if (!cursor.readZigzag(&delta))
            return false;
        running += delta;
        out[i] = T(running);
    }
    return true;
}

bool readPlainColumn(Cursor &cursor, QVector<quint16> &out, int count, int ceiling)
{
    out.resize(count);
    for (int i = 0; i < count; ++i) {
        quint64 value = 0;
        if (!cursor.readVarint(&value) || value >= quint64(ceiling))
            return false;
        out[i] = quint16(value);
    }
    return true;
}

// Newline-separated UTF-8. QByteArray::split on an empty section yields one
// empty entry, which is the right answer for a table whose only member is the
// empty string, and the wrong one for a table that should have had none — so
// the caller checks the count it was promised.
QStringList splitNames(const QByteArray &section)
{
    QStringList names;
    for (const QByteArray &line : section.split('\n'))
        names.append(QString::fromUtf8(line));
    return names;
}

} // namespace

GeonamesIndex::GeonamesIndex() = default;
GeonamesIndex::~GeonamesIndex() = default;

QString GeonamesIndex::bundledResourcePath()
{
    return QStringLiteral(":/clima/cities15000.cgx");
}

Status GeonamesIndex::loadBundled()
{
    if (m_loaded)
        return ok();

    QFile file(bundledResourcePath());
    if (!file.open(QIODevice::ReadOnly)) {
        return Error(ErrorKind::Storage,
                     QStringLiteral("the bundled GeoNames index is missing from the binary at %1. "
                                    "It is a Qt resource added by libclima/CMakeLists.txt; a build "
                                    "that lost it has a build problem, not a data problem.")
                         .arg(bundledResourcePath()));
    }

    return load(file.readAll());
}

Status GeonamesIndex::load(const QByteArray &packed)
{
    if (packed.size() < headerBytes)
        return malformed(QStringLiteral("it is shorter than its own header"));

    if (!packed.startsWith(QByteArray::fromRawData(magic, 4)))
        return malformed(QStringLiteral("it does not start with CLGX"));

    // Little-endian, read a field at a time rather than by casting the header
    // onto a struct: a struct read here would be correct on x86 and wrong on
    // anything that pads or byte-swaps differently, and the failure would be a
    // row count of four billion.
    const auto u32 = [&packed](qsizetype at) {
        return quint32(quint8(packed[at]))
            | (quint32(quint8(packed[at + 1])) << 8)
            | (quint32(quint8(packed[at + 2])) << 16)
            | (quint32(quint8(packed[at + 3])) << 24);
    };

    const quint32 version = u32(4);
    if (version != supportedFormatVersion) {
        return malformed(QStringLiteral("it is format version %1 and this build reads version %2")
                             .arg(version)
                             .arg(supportedFormatVersion));
    }

    const int rowCount        = int(u32(8));
    const int cellDegrees     = int(u32(12));
    const int latitudeCells   = int(u32(16));
    const int longitudeCells  = int(u32(20));
    const int countryCount    = int(u32(24));
    const int admin1Count     = int(u32(28));
    const int timezoneCount   = int(u32(32));
    const int coordinateScale = int(u32(36));
    const int reachStep       = int(u32(40));
    const int sections        = int(u32(44));
    const int payloadLength   = int(u32(48));

    if (sections != sectionCount)
        return malformed(QStringLiteral("it declares %1 sections and not %2")
                             .arg(sections)
                             .arg(sectionCount));
    if (rowCount <= 0 || cellDegrees <= 0 || latitudeCells <= 0 || longitudeCells <= 0
        || coordinateScale <= 0 || reachStep <= 0 || countryCount <= 0 || admin1Count <= 0
        || timezoneCount <= 0) {
        return malformed(QStringLiteral("its header has a non-positive dimension"));
    }
    if (countryCount > 256 || admin1Count > 65536 || timezoneCount > 65536) {
        return malformed(QStringLiteral("one of its lookup tables is wider than the column that "
                                        "indexes it"));
    }

    QVector<qsizetype> lengths(sectionCount);
    qsizetype          declared = 0;
    for (int i = 0; i < sectionCount; ++i) {
        lengths[i] = qsizetype(u32(52 + 4 * i));
        declared += lengths[i];
    }
    if (declared != qsizetype(payloadLength))
        return malformed(QStringLiteral("its section lengths do not add up to its payload length"));

    // qUncompress refuses anything it did not recognise by returning an empty
    // array, which is indistinguishable from a payload that was legitimately
    // empty — so the length is checked rather than the emptiness.
    const QByteArray payload = qUncompress(packed.mid(headerBytes));
    if (payload.size() != qsizetype(payloadLength)) {
        return malformed(QStringLiteral("its payload decompressed to %1 bytes and not the %2 its "
                                        "header promised")
                             .arg(payload.size())
                             .arg(payloadLength));
    }

    QVector<qsizetype> starts(sectionCount);
    qsizetype          at = 0;
    for (int i = 0; i < sectionCount; ++i) {
        starts[i] = at;
        at += lengths[i];
    }

    const auto section = [&payload, &starts, &lengths](int index) {
        return QByteArray::fromRawData(payload.constData() + starts[index], lengths[index]);
    };
    const auto cursorFor = [&payload, &starts, &lengths](int index) {
        const char *begin = payload.constData() + starts[index];
        return Cursor(begin, begin + lengths[index]);
    };

    const int cells = latitudeCells * longitudeCells;

    QVector<quint32> cellStarts;
    {
        Cursor cursor = cursorFor(CellStarts);
        if (!readDeltaColumn(cursor, cellStarts, cells + 1))
            return malformed(QStringLiteral("its cell offset table is truncated"));
        if (cellStarts.first() != 0 || int(cellStarts.last()) != rowCount)
            return malformed(QStringLiteral("its cell offset table does not span the rows"));
    }

    QVector<qint32> latitudes;
    QVector<qint32> longitudes;
    QVector<quint32> geonamesIds;
    {
        Cursor cursor = cursorFor(Latitudes);
        if (!readDeltaColumn(cursor, latitudes, rowCount))
            return malformed(QStringLiteral("its latitude column is truncated"));
    }
    {
        Cursor cursor = cursorFor(Longitudes);
        if (!readDeltaColumn(cursor, longitudes, rowCount))
            return malformed(QStringLiteral("its longitude column is truncated"));
    }
    {
        Cursor cursor = cursorFor(GeonamesIds);
        if (!readDeltaColumn(cursor, geonamesIds, rowCount))
            return malformed(QStringLiteral("its geonameid column is truncated"));
    }

    const QByteArray reach = section(Reach);
    const QByteArray countryIndex = section(CountryIndex);
    if (reach.size() != rowCount || countryIndex.size() != rowCount)
        return malformed(QStringLiteral("its one-byte columns are not one byte a row"));

    QVector<quint16> admin1Index;
    QVector<quint16> timezoneIndex;
    {
        Cursor cursor = cursorFor(Admin1Index);
        if (!readPlainColumn(cursor, admin1Index, rowCount, admin1Count))
            return malformed(QStringLiteral("its admin1 column is truncated or out of range"));
    }
    {
        Cursor cursor = cursorFor(TimezoneIndex);
        if (!readPlainColumn(cursor, timezoneIndex, rowCount, timezoneCount))
            return malformed(QStringLiteral("its timezone column is truncated or out of range"));
    }

    const QByteArray countryBytes = section(CountryCodes);
    if (countryBytes.size() != qsizetype(countryCount) * 2)
        return malformed(QStringLiteral("its country table is not two bytes a country"));

    QStringList countryCodes;
    countryCodes.reserve(countryCount);
    for (int i = 0; i < countryCount; ++i)
        countryCodes.append(QString::fromLatin1(countryBytes.mid(i * 2, 2)));

    const QStringList admin1Names = splitNames(section(Admin1Names));
    const QStringList timezoneNames = splitNames(section(TimezoneNames));
    if (admin1Names.size() != admin1Count || timezoneNames.size() != timezoneCount)
        return malformed(QStringLiteral("its name tables do not hold the counts it declared"));

    // The city names are indexed rather than split: rowCount QStrings would be
    // roughly two megabytes of heap for a table from which one row is read.
    const QByteArray nameView = section(CityNames);
    const QByteArray nameBlob(nameView.constData(), nameView.size());
    QVector<quint32> nameOffsets;
    nameOffsets.reserve(rowCount + 1);
    nameOffsets.append(0);
    for (qsizetype i = 0; i < nameBlob.size(); ++i) {
        if (nameBlob[i] == '\n')
            nameOffsets.append(quint32(i + 1));
    }
    nameOffsets.append(quint32(nameBlob.size()) + 1);
    if (nameOffsets.size() != rowCount + 1)
        return malformed(QStringLiteral("its city-name blob does not hold one name a row"));

    m_rowCount = rowCount;
    m_cellDegrees = cellDegrees;
    m_latitudeCells = latitudeCells;
    m_longitudeCells = longitudeCells;
    m_coordinateScale = coordinateScale;
    m_reachStepMetres = reachStep;

    m_cellStarts = std::move(cellStarts);
    m_latitudes = std::move(latitudes);
    m_longitudes = std::move(longitudes);
    m_geonamesIds = std::move(geonamesIds);

    // Deep copies: `section()` handed out views onto `payload`, which is about
    // to go out of scope.
    m_reach = QByteArray(reach.constData(), reach.size());
    m_countryIndex = QByteArray(countryIndex.constData(), countryIndex.size());
    m_admin1Index = std::move(admin1Index);
    m_timezoneIndex = std::move(timezoneIndex);
    m_countryCodes = countryCodes;
    m_admin1Names = admin1Names;
    m_timezoneNames = timezoneNames;
    m_nameBlob = nameBlob;
    m_nameOffsets = std::move(nameOffsets);

    m_loaded = true;
    return ok();
}

GeonamesCity GeonamesIndex::cityAt(int row) const
{
    GeonamesCity city;
    if (row < 0 || row >= m_rowCount)
        return city;

    const double scale = double(m_coordinateScale);
    city.geonamesId = m_geonamesIds[row];
    city.coordinate = Coordinate{ double(m_latitudes[row]) / scale,
                                  double(m_longitudes[row]) / scale };
    city.countryCode = m_countryCodes.value(quint8(m_countryIndex[row]));
    city.admin1 = m_admin1Names.value(m_admin1Index[row]);
    city.timezone = m_timezoneNames.value(m_timezoneIndex[row]);
    city.reachKm = double(quint8(m_reach[row])) * double(m_reachStepMetres) / 1000.0;

    // The offsets point at the first byte of each name and at one past each
    // separator, so the length is the gap minus the newline that ended it.
    const qsizetype begin = m_nameOffsets[row];
    const qsizetype end = m_nameOffsets[row + 1] - 1;
    city.name = QString::fromUtf8(m_nameBlob.constData() + begin, end - begin);

    return city;
}

double GeonamesIndex::distanceKm(const Coordinate &a, const Coordinate &b)
{
    // Haversine. The half-versed-sine form rather than the spherical law of
    // cosines because the latter loses all its precision at small distances —
    // which is every distance that decides anything here.
    const double lat1 = a.latitude * degreesToRadians;
    const double lat2 = b.latitude * degreesToRadians;
    const double dLat = (b.latitude - a.latitude) * degreesToRadians;
    const double dLon = (b.longitude - a.longitude) * degreesToRadians;

    const double h = std::sin(dLat / 2) * std::sin(dLat / 2)
        + std::cos(lat1) * std::cos(lat2) * std::sin(dLon / 2) * std::sin(dLon / 2);

    return 2.0 * earthRadiusKm * std::asin(std::sqrt(std::min(1.0, h)));
}

int GeonamesIndex::cellIndex(int latitudeCell, int longitudeCell) const
{
    return latitudeCell * m_longitudeCells + longitudeCell;
}

std::optional<GeonamesIndex::Match> GeonamesIndex::nearest(const Coordinate &at,
                                                           double maxDistanceKm) const
{
    if (!m_loaded || !at.isValid() || maxDistanceKm <= 0.0)
        return std::nullopt;

    // The two candidates the two-step rule chooses between. `bestRatio` is the
    // settlement the point is furthest inside; `bestDistance` is the nearest
    // one however small it is. They are usually the same row and the cases
    // where they are not are the whole reason both are tracked.
    int    ratioRow = -1;
    double ratioValue = std::numeric_limits<double>::max();
    double ratioDistance = 0.0;

    int    nearestRow = -1;
    double nearestDistance = std::numeric_limits<double>::max();

    const double latitudeWindow = maxDistanceKm / kilometresPerDegree;
    const double latitudeMin = std::max(-90.0, at.latitude - latitudeWindow);
    const double latitudeMax = std::min(90.0, at.latitude + latitudeWindow);

    // A degree of longitude is kilometresPerDegree * cos(latitude), so the box
    // is widest at whichever of its two latitude edges is furthest from the
    // equator. Taking the cosine at the query's own latitude instead would
    // make the box too narrow at its poleward edge and lose a city that is
    // inside the radius.
    const double extremeLatitude = std::max(std::abs(latitudeMin), std::abs(latitudeMax));
    const double cosine = std::cos(extremeLatitude * degreesToRadians);

    bool   allLongitudes = true;
    double longitudeMin = -180.0;
    double longitudeMax = 180.0;
    if (cosine > 1e-9) {
        const double longitudeWindow = maxDistanceKm / (kilometresPerDegree * cosine);
        if (longitudeWindow < 180.0) {
            allLongitudes = false;
            longitudeMin = at.longitude - longitudeWindow;
            longitudeMax = at.longitude + longitudeWindow;
        }
    }

    const auto latitudeCellOf = [this](double latitude) {
        const int cell = int(std::floor((latitude + 90.0) / double(m_cellDegrees)));
        return std::clamp(cell, 0, m_latitudeCells - 1);
    };

    // The longitude cell, wrapped rather than clamped: a box that crosses the
    // antimeridian is a box whose cell indices run off one end and continue at
    // the other, and clamping there would silently drop half of it.
    const auto longitudeCellOf = [this](double longitude) {
        const int raw = int(std::floor((longitude + 180.0) / double(m_cellDegrees)));
        const int wrapped = raw % m_longitudeCells;
        return wrapped < 0 ? wrapped + m_longitudeCells : wrapped;
    };

    const int firstLatitudeCell = latitudeCellOf(latitudeMin);
    const int lastLatitudeCell = latitudeCellOf(latitudeMax);

    QVector<int> longitudeCells;
    if (allLongitudes) {
        longitudeCells.reserve(m_longitudeCells);
        for (int i = 0; i < m_longitudeCells; ++i)
            longitudeCells.append(i);
    } else {
        const int first = longitudeCellOf(longitudeMin);
        const int span = int(std::floor((longitudeMax - longitudeMin) / double(m_cellDegrees))) + 2;
        const int count = std::min(span, m_longitudeCells);
        longitudeCells.reserve(count);
        for (int i = 0; i < count; ++i)
            longitudeCells.append((first + i) % m_longitudeCells);
    }

    const double scale = double(m_coordinateScale);
    const double reachStepKm = double(m_reachStepMetres) / 1000.0;

    for (int latitudeCell = firstLatitudeCell; latitudeCell <= lastLatitudeCell; ++latitudeCell) {
        for (const int longitudeCell : longitudeCells) {
            const int cell = cellIndex(latitudeCell, longitudeCell);
            const int begin = int(m_cellStarts[cell]);
            const int end = int(m_cellStarts[cell + 1]);

            for (int row = begin; row < end; ++row) {
                const Coordinate candidate{ double(m_latitudes[row]) / scale,
                                            double(m_longitudes[row]) / scale };
                const double distance = distanceKm(at, candidate);
                if (distance > maxDistanceKm)
                    continue;

                // Strictly less than, so that two rows at the same distance
                // resolve to the lower row index. The row order is fixed by
                // the packer, so the tie-break is deterministic rather than
                // dependent on which cell was walked first.
                if (distance < nearestDistance) {
                    nearestDistance = distance;
                    nearestRow = row;
                }

                const double reach = double(quint8(m_reach[row])) * reachStepKm;
                const double ratio = reach > 0.0 ? distance / reach
                                                 : std::numeric_limits<double>::max();
                if (ratio < ratioValue) {
                    ratioValue = ratio;
                    ratioRow = row;
                    ratioDistance = distance;
                }
            }
        }
    }

    if (nearestRow < 0)
        return std::nullopt;

    // Step one: the point is inside somebody's footprint. Step two: it is not,
    // and the answer reverts to plain distance — see the header comment for
    // why a big city must not win from 60 km away when a village is at 30.
    if (ratioRow >= 0 && ratioValue <= 1.0) {
        Match match;
        match.row = ratioRow;
        match.distanceKm = ratioDistance;
        match.reachKm = double(quint8(m_reach[ratioRow])) * reachStepKm;
        match.insideFootprint = true;
        return match;
    }

    Match match;
    match.row = nearestRow;
    match.distanceKm = nearestDistance;
    match.reachKm = double(quint8(m_reach[nearestRow])) * reachStepKm;
    match.insideFootprint = false;
    return match;
}

} // namespace clima
