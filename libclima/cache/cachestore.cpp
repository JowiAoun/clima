// SPDX-FileCopyrightText: 2026 Jowi Aoun
// SPDX-License-Identifier: MPL-2.0

#include "cachestore.h"

#include "libclima/core/clock.h"

#include <QAtomicInteger>
#include <QDir>
#include <QFileInfo>
#include <QSqlError>
#include <QSqlQuery>
#include <QStandardPaths>
#include <QVariant>

namespace clima {

namespace {

// Every CacheStore gets its own named QSqlDatabase connection. The default
// connection is a process-wide global, and two stores sharing it — a test
// opening a second store while the first is alive, or a future tile cache
// beside the forecast cache — would silently reconfigure each other's PRAGMAs
// and close each other's file.
QString nextConnectionName()
{
    static QAtomicInteger<quint64> counter{ 0 };
    return QStringLiteral("clima-cache-%1").arg(counter.fetchAndAddRelaxed(1));
}

// QDateTime ⇄ column. NULL is an invalid QDateTime and means "never" for an
// expiry; making that the same value in both directions is what lets the
// immutable row of §4.5 be expressed without a sentinel.
QVariant toColumn(const QDateTime &when)
{
    if (!when.isValid())
        return {};
    return when.toUTC().toMSecsSinceEpoch();
}

QDateTime fromColumn(const QVariant &value)
{
    if (value.isNull() || !value.isValid())
        return {};
    return QDateTime::fromMSecsSinceEpoch(value.toLongLong(), QTimeZone::UTC);
}

QVariant toBlob(const QByteArray &bytes)
{
    if (bytes.isEmpty())
        return {};
    return bytes;
}

Error storageError(const QSqlQuery &query, const QString &what)
{
    return { ErrorKind::Storage,
             QStringLiteral("%1: %2").arg(what, query.lastError().text()) };
}

} // namespace

CacheStore::CacheStore(Clock *clock)
    : m_clock(clock)
    , m_connectionName(nextConnectionName())
{
}

CacheStore::~CacheStore()
{
    close();
}

QString CacheStore::defaultDatabasePath()
{
    // AppDataLocation and not CacheLocation, deliberately. The file holds the
    // user's saved places and the engine's settings alongside the forecasts,
    // and CacheLocation is a directory the platform is entitled to empty
    // without asking — on a Flatpak upgrade, or when a disk-cleaner runs. §4.5
    // names AppDataLocation and this is why.
    const QString directory = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    return directory + QStringLiteral("/cache.sqlite");
}

Status CacheStore::open(const QString &databasePath)
{
    return open(databasePath, defaultMigrations());
}

Status CacheStore::open(const QString &databasePath, const QList<Migration> &migrations)
{
    close();

    const bool inMemory = databasePath == QLatin1String(":memory:");
    if (!inMemory) {
        const QString directory = QFileInfo(databasePath).absolutePath();
        if (!QDir().mkpath(directory)) {
            return Error(ErrorKind::Storage,
                         QStringLiteral("could not create the data directory %1").arg(directory));
        }
    }

    m_database = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), m_connectionName);
    m_database.setDatabaseName(databasePath);

    if (!m_database.open()) {
        const QString message = m_database.lastError().text();
        m_database = QSqlDatabase();
        QSqlDatabase::removeDatabase(m_connectionName);
        return Error(ErrorKind::Storage,
                     QStringLiteral("could not open %1: %2").arg(databasePath, message));
    }

    m_path = databasePath;

    Status status = configureConnection();
    if (!status) {
        close();
        return status;
    }

    status = runMigrations(m_database, migrations, m_clock);
    if (!status) {
        close();
        return status;
    }

    m_schemaVersion = currentVersion(m_database);
    return ok();
}

Status CacheStore::configureConnection()
{
    // Three PRAGMAs, each buying something specific.
    //
    //   foreign_keys   SQLite defaults it OFF for backward compatibility, so a
    //                  REFERENCES clause is decoration until this is set. There
    //                  are none in v1; there will be when the alert table lands
    //                  and points at places, and a constraint that silently
    //                  does nothing is worse than no constraint.
    //
    //   journal_mode   WAL lets a read proceed while a write is in flight,
    //                  which is the whole shape of this app: the UI reads the
    //                  cache to paint while a fetch writes the next payload.
    //                  In rollback-journal mode that read blocks, on the GUI
    //                  thread, for the length of a disk write.
    //
    //   synchronous    NORMAL rather than FULL. FULL fsyncs on every commit,
    //                  which is the right trade for a ledger and the wrong one
    //                  for a cache: the failure it protects against is losing
    //                  the last write to a power cut, and the last write here
    //                  is a weather forecast we can fetch again.
    static const char *pragmas[] = {
        "PRAGMA foreign_keys = ON",
        "PRAGMA journal_mode = WAL",
        "PRAGMA synchronous = NORMAL",
    };

    for (const char *pragma : pragmas) {
        QSqlQuery query(m_database);
        if (!query.exec(QLatin1String(pragma)))
            return storageError(query, QStringLiteral("could not apply %1").arg(QLatin1String(pragma)));
    }
    return ok();
}

void CacheStore::close()
{
    if (m_database.isValid()) {
        if (m_database.isOpen())
            m_database.close();
        m_database = QSqlDatabase();
        QSqlDatabase::removeDatabase(m_connectionName);
    }
    m_path.clear();
    m_schemaVersion = 0;
}

bool CacheStore::isOpen() const
{
    return m_database.isValid() && m_database.isOpen();
}

int CacheStore::schemaVersion() const
{
    return m_schemaVersion;
}

// ---- payloads ---------------------------------------------------------------

Status CacheStore::put(const CacheEntry &entry)
{
    if (!isOpen())
        return Error(ErrorKind::Storage, QStringLiteral("the cache database is not open"));

    QSqlQuery query(m_database);
    query.prepare(QStringLiteral(R"sql(
        INSERT OR REPLACE INTO forecast_blob
            (key, provider_id, endpoint, kind, latitude, longitude,
             payload, content_type, etag, last_modified, server_expires,
             fetched_at, expires_at)
        VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)
    )sql"));
    query.addBindValue(entry.key);
    query.addBindValue(entry.providerId);
    query.addBindValue(entry.endpoint);
    query.addBindValue(dataKindName(entry.kind));
    query.addBindValue(entry.coordinate ? QVariant(entry.coordinate->latitude) : QVariant());
    query.addBindValue(entry.coordinate ? QVariant(entry.coordinate->longitude) : QVariant());
    query.addBindValue(entry.payload);
    query.addBindValue(QString::fromLatin1(entry.contentType));
    query.addBindValue(toBlob(entry.validators.entityTag));
    query.addBindValue(toBlob(entry.validators.lastModified));
    query.addBindValue(toColumn(entry.validators.expires));
    query.addBindValue(toColumn(entry.fetchedAt));
    query.addBindValue(toColumn(entry.expiresAt));

    if (!query.exec())
        return storageError(query, QStringLiteral("could not write cache entry %1").arg(entry.key));
    return ok();
}

Result<CacheEntry> CacheStore::get(const QString &key) const
{
    if (!isOpen())
        return Error(ErrorKind::Storage, QStringLiteral("the cache database is not open"));

    QSqlQuery query(m_database);
    query.prepare(QStringLiteral(R"sql(
        SELECT provider_id, endpoint, kind, latitude, longitude, payload, content_type,
               etag, last_modified, server_expires, fetched_at, expires_at
        FROM forecast_blob WHERE key = ?
    )sql"));
    query.addBindValue(key);

    if (!query.exec())
        return storageError(query, QStringLiteral("could not read cache entry %1").arg(key));

    if (!query.next())
        return Error(ErrorKind::NotFound, QStringLiteral("no cache entry for %1").arg(key));

    // A row with no payload is a validator record, not a cache entry — see
    // storeValidators() below and the forecast_blob comment in migrations.cpp.
    // Reporting it as a hit would hand the caller zero bytes to parse.
    const QVariant payload = query.value(5);
    if (payload.isNull()) {
        return Error(ErrorKind::NotFound,
                     QStringLiteral("cache entry %1 holds validators but no payload").arg(key));
    }

    CacheEntry entry;
    entry.key = key;
    entry.providerId = query.value(0).toString();
    entry.endpoint = query.value(1).toString();
    entry.kind = dataKindFromName(query.value(2).toString());
    if (!query.value(3).isNull() && !query.value(4).isNull())
        entry.coordinate = Coordinate{ query.value(3).toDouble(), query.value(4).toDouble() };
    entry.payload = payload.toByteArray();
    entry.contentType = query.value(6).toString().toLatin1();
    entry.validators.entityTag = query.value(7).toByteArray();
    entry.validators.lastModified = query.value(8).toByteArray();
    entry.validators.expires = fromColumn(query.value(9));
    entry.fetchedAt = fromColumn(query.value(10));
    entry.expiresAt = fromColumn(query.value(11));
    return entry;
}

Status CacheStore::remove(const QString &key)
{
    if (!isOpen())
        return Error(ErrorKind::Storage, QStringLiteral("the cache database is not open"));

    QSqlQuery query(m_database);
    query.prepare(QStringLiteral("DELETE FROM forecast_blob WHERE key = ?"));
    query.addBindValue(key);
    if (!query.exec())
        return storageError(query, QStringLiteral("could not delete cache entry %1").arg(key));
    return ok();
}

Status CacheStore::touch(const QString &key, const QDateTime &fetchedAt,
                         const QDateTime &expiresAt)
{
    if (!isOpen())
        return Error(ErrorKind::Storage, QStringLiteral("the cache database is not open"));

    QSqlQuery query(m_database);
    query.prepare(QStringLiteral(
        "UPDATE forecast_blob SET fetched_at = ?, expires_at = ? WHERE key = ?"));
    query.addBindValue(toColumn(fetchedAt));
    query.addBindValue(toColumn(expiresAt));
    query.addBindValue(key);
    if (!query.exec())
        return storageError(query, QStringLiteral("could not touch cache entry %1").arg(key));
    return ok();
}

Result<int> CacheStore::pruneUnusable()
{
    if (!isOpen())
        return Error(ErrorKind::Storage, QStringLiteral("the cache database is not open"));

    // Only the kinds that may not be shown stale. Everything else is kept past
    // its TTL on purpose: an hour-old forecast is the best thing to show on a
    // train, and deleting it to be tidy is how the app ends up with the empty
    // screen §4.1 exists to prevent.
    QStringList unusableKinds;
    for (const DataKind kind : { DataKind::CurrentConditions, DataKind::Forecast,
                                 DataKind::Nowcast, DataKind::Ensemble, DataKind::AirQuality,
                                 DataKind::Alerts, DataKind::RadarFrame, DataKind::BasemapTile,
                                 DataKind::HistoricalArchive, DataKind::Geocoding }) {
        if (!policyFor(kind).staleWhileRevalidate)
            unusableKinds.append(dataKindName(kind));
    }

    if (unusableKinds.isEmpty())
        return 0;

    QStringList placeholders;
    placeholders.reserve(unusableKinds.size());
    for (int i = 0; i < unusableKinds.size(); ++i)
        placeholders.append(QStringLiteral("?"));

    QSqlQuery query(m_database);
    query.prepare(QStringLiteral("DELETE FROM forecast_blob "
                                 "WHERE expires_at IS NOT NULL AND expires_at <= ? "
                                 "AND kind IN (%1)")
                      .arg(placeholders.join(QLatin1String(", "))));
    query.addBindValue(m_clock->now().toMSecsSinceEpoch());
    for (const QString &kind : std::as_const(unusableKinds))
        query.addBindValue(kind);

    if (!query.exec())
        return storageError(query, QStringLiteral("could not prune the cache"));
    return int(query.numRowsAffected());
}

bool CacheStore::isFresh(const CacheEntry &entry) const
{
    // An invalid expiry is the immutable row: ERA5 reanalysis for a day that
    // has already happened does not go stale.
    if (!entry.expiresAt.isValid())
        return true;
    return m_clock->now() < entry.expiresAt;
}

bool CacheStore::isUsable(const CacheEntry &entry) const
{
    if (isFresh(entry))
        return true;
    return policyFor(entry.kind).staleWhileRevalidate;
}

// ---- places -----------------------------------------------------------------

Status CacheStore::savePlace(Place &place)
{
    if (!isOpen())
        return Error(ErrorKind::Storage, QStringLiteral("the cache database is not open"));

    QSqlQuery query(m_database);
    if (place.id == 0) {
        query.prepare(QStringLiteral(R"sql(
            INSERT INTO places
                (name, admin1, country, country_code, timezone,
                 latitude, longitude, elevation_m, is_home, sort_order, added_at,
                 geonames_id)
            VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)
        )sql"));
    } else {
        query.prepare(QStringLiteral(R"sql(
            UPDATE places SET
                name = ?, admin1 = ?, country = ?, country_code = ?, timezone = ?,
                latitude = ?, longitude = ?, elevation_m = ?, is_home = ?,
                sort_order = ?, added_at = ?, geonames_id = ?
            WHERE id = ?
        )sql"));
    }

    query.addBindValue(place.name);
    query.addBindValue(place.admin1);
    query.addBindValue(place.country);
    query.addBindValue(place.countryCode);
    query.addBindValue(place.timezone);
    query.addBindValue(place.coordinate.latitude);
    query.addBindValue(place.coordinate.longitude);
    query.addBindValue(place.elevationMetres ? QVariant(*place.elevationMetres) : QVariant());

    // NULL rather than 0 when the place is not home, and the partial unique
    // index in migration 2 is why: `WHERE is_home = 1` over a column full of
    // NULLs indexes exactly the one home row, and SQLite lets any number of
    // NULLs coexist under a unique index. A 0 would be a value, and the second
    // non-home place would collide with the first.
    query.addBindValue(place.isHome ? QVariant(1) : QVariant());

    query.addBindValue(place.sortOrder);
    query.addBindValue(toColumn(place.addedAt.isValid() ? place.addedAt : m_clock->now()));
    query.addBindValue(place.geonamesId != 0 ? QVariant(place.geonamesId) : QVariant());
    if (place.id != 0)
        query.addBindValue(place.id);

    if (!query.exec())
        return storageError(query, QStringLiteral("could not save the place %1").arg(place.name));

    if (place.id == 0)
        place.id = query.lastInsertId().toLongLong();
    if (!place.addedAt.isValid())
        place.addedAt = m_clock->now();
    return ok();
}

Result<QList<Place>> CacheStore::places() const
{
    if (!isOpen())
        return Error(ErrorKind::Storage, QStringLiteral("the cache database is not open"));

    QSqlQuery query(m_database);
    if (!query.exec(QStringLiteral(R"sql(
            SELECT id, name, admin1, country, country_code, timezone,
                   latitude, longitude, elevation_m, is_home, sort_order, added_at,
                   geonames_id
            FROM places ORDER BY sort_order, id
        )sql"))) {
        return storageError(query, QStringLiteral("could not read the saved places"));
    }

    QList<Place> found;
    while (query.next()) {
        Place place;
        place.id = query.value(0).toLongLong();
        place.name = query.value(1).toString();
        place.admin1 = query.value(2).toString();
        place.country = query.value(3).toString();
        place.countryCode = query.value(4).toString();
        place.timezone = query.value(5).toString();
        place.coordinate = Coordinate{ query.value(6).toDouble(), query.value(7).toDouble() };
        if (!query.value(8).isNull())
            place.elevationMetres = query.value(8).toDouble();
        place.isHome = query.value(9).toInt() != 0;
        place.sortOrder = query.value(10).toInt();
        place.addedAt = fromColumn(query.value(11));
        place.geonamesId = query.value(12).toLongLong();
        found.append(place);
    }
    return found;
}

Status CacheStore::removePlace(qint64 id)
{
    if (!isOpen())
        return Error(ErrorKind::Storage, QStringLiteral("the cache database is not open"));

    QSqlQuery query(m_database);
    query.prepare(QStringLiteral("DELETE FROM places WHERE id = ?"));
    query.addBindValue(id);
    if (!query.exec())
        return storageError(query, QStringLiteral("could not remove place %1").arg(id));
    return ok();
}

// ---- engine settings --------------------------------------------------------

Status CacheStore::setSetting(const QString &key, const QString &value)
{
    if (!isOpen())
        return Error(ErrorKind::Storage, QStringLiteral("the cache database is not open"));

    QSqlQuery query(m_database);
    query.prepare(QStringLiteral("INSERT OR REPLACE INTO settings (key, value) VALUES (?, ?)"));
    query.addBindValue(key);
    query.addBindValue(value);
    if (!query.exec())
        return storageError(query, QStringLiteral("could not write the setting %1").arg(key));
    return ok();
}

Result<QString> CacheStore::setting(const QString &key) const
{
    if (!isOpen())
        return Error(ErrorKind::Storage, QStringLiteral("the cache database is not open"));

    QSqlQuery query(m_database);
    query.prepare(QStringLiteral("SELECT value FROM settings WHERE key = ?"));
    query.addBindValue(key);
    if (!query.exec())
        return storageError(query, QStringLiteral("could not read the setting %1").arg(key));
    if (!query.next())
        return Error(ErrorKind::NotFound, QStringLiteral("no setting named %1").arg(key));
    return query.value(0).toString();
}

// ---- ValidatorStore ---------------------------------------------------------

std::optional<Validators> CacheStore::validatorsFor(const QString &key) const
{
    if (!isOpen())
        return std::nullopt;

    QSqlQuery query(m_database);
    query.prepare(QStringLiteral(
        "SELECT etag, last_modified, server_expires FROM forecast_blob WHERE key = ?"));
    query.addBindValue(key);
    if (!query.exec() || !query.next())
        return std::nullopt;

    Validators validators;
    validators.entityTag = query.value(0).toByteArray();
    validators.lastModified = query.value(1).toByteArray();
    validators.expires = fromColumn(query.value(2));
    if (validators.isEmpty())
        return std::nullopt;
    return validators;
}

void CacheStore::storeValidators(const QString &key, const Validators &validators)
{
    if (!isOpen())
        return;

    // An UPSERT that touches only the validator columns. It has to work both
    // ways round: HttpClient records an ETag the moment a 200 arrives, which is
    // *before* the caller has parsed the body and decided to keep it, so the
    // row may not exist yet. When it does not, this leaves a row with a NULL
    // payload — a validator record, which get() reports as a miss and a later
    // put() fills in.
    //
    // The alternative, a separate validators table, means two rows per request
    // that must be deleted together and a join to read them. One row with a
    // nullable payload is the smaller thing to get wrong.
    QSqlQuery query(m_database);
    query.prepare(QStringLiteral(R"sql(
        INSERT INTO forecast_blob (key, etag, last_modified, server_expires)
        VALUES (:key, :etag, :last_modified, :server_expires)
        ON CONFLICT(key) DO UPDATE SET
            etag = :etag2,
            last_modified = :last_modified2,
            server_expires = :server_expires2
    )sql"));
    query.bindValue(QStringLiteral(":key"), key);
    query.bindValue(QStringLiteral(":etag"), toBlob(validators.entityTag));
    query.bindValue(QStringLiteral(":last_modified"), toBlob(validators.lastModified));
    query.bindValue(QStringLiteral(":server_expires"), toColumn(validators.expires));
    query.bindValue(QStringLiteral(":etag2"), toBlob(validators.entityTag));
    query.bindValue(QStringLiteral(":last_modified2"), toBlob(validators.lastModified));
    query.bindValue(QStringLiteral(":server_expires2"), toColumn(validators.expires));
    query.exec();
}

} // namespace clima
