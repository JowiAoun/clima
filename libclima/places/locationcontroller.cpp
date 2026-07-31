// SPDX-FileCopyrightText: 2026 Jowi Aoun
// SPDX-License-Identifier: MPL-2.0

#include "locationcontroller.h"

#include "libclima/cache/cachestore.h"

#include <algorithm>

namespace clima {

namespace {

// The engine settings key holding the place last looked at. Prefixed, because
// the settings table is shared with everything else in libclima and a bare
// "current" would be the first collision.
QString currentPlaceKey()
{
    return QStringLiteral("places.current");
}

} // namespace

LocationController::LocationController(CacheStore *cache, QObject *parent)
    : QAbstractListModel(parent)
    , m_cache(cache)
{
}

LocationController::~LocationController() = default;

Status LocationController::load()
{
    if (m_cache == nullptr)
        return Error(ErrorKind::Storage, QStringLiteral("no cache store"));

    const Result<QList<Place>> stored = m_cache->places();
    if (!stored)
        return stored.error();

    beginResetModel();
    m_places = stored.value();
    m_currentIndex = -1;
    endResetModel();

    // Which row to open on, in order of preference: the one last looked at,
    // then home, then the first. Each fallback is a real case — a fresh
    // install has no remembered current, and a database restored from a backup
    // may have a remembered id that no longer exists.
    int wanted = -1;
    if (const Result<QString> remembered = m_cache->setting(currentPlaceKey())) {
        bool         parsed = false;
        const qint64 id = remembered.value().toLongLong(&parsed);
        if (parsed) {
            for (int row = 0; row < m_places.size(); ++row) {
                if (m_places.at(row).id == id) {
                    wanted = row;
                    break;
                }
            }
        }
    }
    if (wanted < 0)
        wanted = homeIndex();
    if (wanted < 0 && !m_places.isEmpty())
        wanted = 0;

    m_currentIndex = wanted;

    Q_EMIT countChanged();
    Q_EMIT currentChanged();
    return ok();
}

int LocationController::rowCount(const QModelIndex &parent) const
{
    return parent.isValid() ? 0 : int(m_places.size());
}

QHash<int, QByteArray> LocationController::roleNames() const
{
    return {
        { PlaceIdRole,     QByteArrayLiteral("placeId") },
        { GeonamesIdRole,  QByteArrayLiteral("geonamesId") },
        { NameRole,        QByteArrayLiteral("name") },
        { Admin1Role,      QByteArrayLiteral("admin1") },
        { CountryRole,     QByteArrayLiteral("country") },
        { CountryCodeRole, QByteArrayLiteral("countryCode") },
        { TimezoneRole,    QByteArrayLiteral("timezone") },
        { LatitudeRole,    QByteArrayLiteral("latitude") },
        { LongitudeRole,   QByteArrayLiteral("longitude") },
        { IsHomeRole,      QByteArrayLiteral("isHome") },
        { LabelRole,       QByteArrayLiteral("label") },
        { RegionRole,      QByteArrayLiteral("region") },
    };
}

QVariant LocationController::data(const QModelIndex &index, int role) const
{
    if (!isValidRow(index.row()))
        return {};

    const Place &place = m_places.at(index.row());
    switch (role) {
    case PlaceIdRole:     return QVariant::fromValue(place.id);
    case GeonamesIdRole:  return QVariant::fromValue(place.geonamesId);
    case NameRole:        return place.name;
    case Admin1Role:      return place.admin1;
    case CountryRole:     return place.country;
    case CountryCodeRole: return place.countryCode;
    case TimezoneRole:    return place.timezone;
    case LatitudeRole:    return place.coordinate.latitude;
    case LongitudeRole:   return place.coordinate.longitude;
    case IsHomeRole:      return place.isHome;
    case LabelRole:       return place.label();
    case RegionRole:      return place.region();

    // Qt::DisplayRole so that a plain view, a QCompleter or a debugging
    // dump shows something readable instead of an empty column.
    case Qt::DisplayRole: return place.label();
    default:              return {};
    }
}

bool LocationController::isValidRow(int row) const
{
    return row >= 0 && row < m_places.size();
}

void LocationController::emitRowChanged(int row)
{
    const QModelIndex at = index(row, 0);
    Q_EMIT dataChanged(at, at);
}

Place LocationController::placeAt(int row) const
{
    return isValidRow(row) ? m_places.at(row) : Place{};
}

Place LocationController::currentPlace() const
{
    return placeAt(m_currentIndex);
}

QString LocationController::currentLabel() const
{
    return currentPlace().label();
}

QString LocationController::currentRegion() const
{
    return currentPlace().region();
}

bool LocationController::currentIsHome() const
{
    return currentPlace().isHome;
}

int LocationController::homeIndex() const
{
    for (int row = 0; row < m_places.size(); ++row) {
        if (m_places.at(row).isHome)
            return row;
    }
    return -1;
}

int LocationController::indexOfGeonamesId(qint64 geonamesId) const
{
    if (geonamesId == 0)
        return -1;
    for (int row = 0; row < m_places.size(); ++row) {
        if (m_places.at(row).geonamesId == geonamesId)
            return row;
    }
    return -1;
}

void LocationController::rememberCurrent()
{
    if (m_cache == nullptr)
        return;

    const Place place = currentPlace();
    const Status status = m_cache->setSetting(
        currentPlaceKey(), place.id != 0 ? QString::number(place.id) : QString());
    if (!status)
        Q_EMIT failed(status.error().message());
}

bool LocationController::persist(Place &place)
{
    if (m_cache == nullptr)
        return false;

    const Status status = m_cache->savePlace(place);
    if (!status) {
        Q_EMIT failed(status.error().message());
        return false;
    }
    return true;
}

bool LocationController::setCurrentIndex(int row)
{
    // -1 is legal and means "nothing selected", which is the state an empty
    // model is in. Anything else out of range is a caller bug and is refused
    // rather than clamped.
    if (row != -1 && !isValidRow(row))
        return false;
    if (row == m_currentIndex)
        return true;

    m_currentIndex = row;
    rememberCurrent();
    Q_EMIT currentChanged();
    return true;
}

int LocationController::addPlace(const Place &place, bool makeCurrent)
{
    // Already saved? Then this is a selection, not an insertion. isSameEntity
    // compares GeoNames ids when both have one, so a place searched for and
    // the same place reverse-geocoded from a GPS fix collapse onto one row.
    for (int row = 0; row < m_places.size(); ++row) {
        if (m_places.at(row).isSameEntity(place)) {
            if (makeCurrent)
                setCurrentIndex(row);
            return row;
        }
    }

    Place saved = place;
    saved.id = 0;
    saved.sortOrder = m_places.isEmpty() ? 0 : m_places.last().sortOrder + 1;

    // The first place is home. An app whose only saved place is not its home
    // has a home nothing points at, and the next start would fall back to row
    // 0 anyway — this makes the fallback a stored fact instead of an accident.
    saved.isHome = m_places.isEmpty();

    if (!persist(saved))
        return -1;

    const int row = int(m_places.size());
    beginInsertRows({}, row, row);
    m_places.append(saved);
    endInsertRows();

    Q_EMIT countChanged();
    if (saved.isHome)
        Q_EMIT homeChanged();
    if (makeCurrent)
        setCurrentIndex(row);

    return row;
}

bool LocationController::removeAt(int row)
{
    if (!isValidRow(row))
        return false;

    const Place going = m_places.at(row);
    if (m_cache != nullptr) {
        const Status status = m_cache->removePlace(going.id);
        if (!status) {
            Q_EMIT failed(status.error().message());
            return false;
        }
    }

    beginRemoveRows({}, row, row);
    m_places.removeAt(row);
    endRemoveRows();

    // Removing home hands it to whatever is left, so that the invariant "there
    // is a home while there are places" survives. First row, because any other
    // choice would need a reason and there is not one.
    if (going.isHome && !m_places.isEmpty()) {
        Place &next = m_places.first();
        next.isHome = true;
        if (persist(next))
            emitRowChanged(0);
        Q_EMIT homeChanged();
    }

    // The current row moves with the list: the row after the removed one takes
    // its index, so staying put shows the next place rather than following the
    // deleted one into nothing.
    int current = m_currentIndex;
    if (current == row)
        current = std::min(row, int(m_places.size()) - 1);
    else if (current > row)
        current -= 1;
    if (m_places.isEmpty())
        current = -1;

    // Assigned rather than routed through setCurrentIndex, which would decline
    // to notify when the index happens to be unchanged. After a removal the
    // index can stay the same while the place under it does not, and the
    // location bar binds to currentLabel.
    m_currentIndex = current;
    rememberCurrent();
    Q_EMIT currentChanged();

    Q_EMIT countChanged();
    return true;
}

bool LocationController::move(int from, int to)
{
    if (!isValidRow(from) || !isValidRow(to) || from == to)
        return false;

    const qint64 currentId = currentPlace().id;

    // beginMoveRows counts the destination in the *pre-move* indexing, where
    // moving down by one is a no-op and moving down by two lands at to + 1.
    // Getting this wrong does not crash; it desynchronises every attached view
    // from the model, which then paints the wrong rows until something resets
    // it.
    const int destination = to > from ? to + 1 : to;
    beginMoveRows({}, from, from, {}, destination);
    m_places.move(from, to);
    endMoveRows();

    // sort_order is rewritten for every row rather than for the two that
    // moved, because the column is what `ORDER BY sort_order, id` reads on the
    // next start and a gap-based scheme would eventually run out of gaps.
    // Handfuls of rows; this is not a loop worth optimising.
    for (int row = 0; row < m_places.size(); ++row) {
        Place &place = m_places[row];
        if (place.sortOrder == row)
            continue;
        place.sortOrder = row;
        if (!persist(place))
            return false;
    }

    if (currentId != 0) {
        for (int row = 0; row < m_places.size(); ++row) {
            if (m_places.at(row).id == currentId) {
                if (row != m_currentIndex) {
                    m_currentIndex = row;
                    Q_EMIT currentChanged();
                }
                break;
            }
        }
    }

    return true;
}

bool LocationController::setHome(int row)
{
    if (!isValidRow(row))
        return false;
    if (m_places.at(row).isHome)
        return true;

    // The old home is cleared *before* the new one is written. The places table
    // carries a partial unique index over is_home, so the other order is a
    // constraint failure rather than an overwrite — which is exactly what the
    // index is for, and why this order is not an accident.
    const int previous = homeIndex();
    if (previous >= 0) {
        Place &old = m_places[previous];
        old.isHome = false;
        if (!persist(old)) {
            old.isHome = true;
            return false;
        }
        emitRowChanged(previous);
    }

    Place &next = m_places[row];
    next.isHome = true;
    if (!persist(next)) {
        next.isHome = false;
        return false;
    }
    emitRowChanged(row);

    Q_EMIT homeChanged();
    if (row == m_currentIndex || previous == m_currentIndex)
        Q_EMIT currentChanged();

    return true;
}

bool LocationController::toggleHome(int row)
{
    return setHome(row);
}

} // namespace clima
