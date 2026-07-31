// SPDX-FileCopyrightText: 2026 Jowi Aoun
// SPDX-License-Identifier: MPL-2.0

#include "placesearchmodel.h"

#include "libclima/providers/geocoding/igeocoder.h"

#include <QFutureWatcher>

#include <algorithm>

namespace clima {

PlaceSearchModel::PlaceSearchModel(IForwardGeocoder *geocoder, QObject *parent)
    : QAbstractListModel(parent)
    , m_geocoder(geocoder)
{
    m_debounce.setSingleShot(true);
    m_debounce.setInterval(defaultDebounceMs);
    connect(&m_debounce, &QTimer::timeout, this, &PlaceSearchModel::dispatch);
}

PlaceSearchModel::~PlaceSearchModel() = default;

int PlaceSearchModel::rowCount(const QModelIndex &parent) const
{
    return parent.isValid() ? 0 : int(m_results.size());
}

QHash<int, QByteArray> PlaceSearchModel::roleNames() const
{
    return {
        { GeonamesIdRole,  QByteArrayLiteral("geonamesId") },
        { NameRole,        QByteArrayLiteral("name") },
        { Admin1Role,      QByteArrayLiteral("admin1") },
        { CountryRole,     QByteArrayLiteral("country") },
        { CountryCodeRole, QByteArrayLiteral("countryCode") },
        { TimezoneRole,    QByteArrayLiteral("timezone") },
        { LatitudeRole,    QByteArrayLiteral("latitude") },
        { LongitudeRole,   QByteArrayLiteral("longitude") },
        { LabelRole,       QByteArrayLiteral("label") },
        { RegionRole,      QByteArrayLiteral("region") },
    };
}

QVariant PlaceSearchModel::data(const QModelIndex &index, int role) const
{
    if (index.row() < 0 || index.row() >= m_results.size())
        return {};

    const Place &place = m_results.at(index.row());
    switch (role) {
    case GeonamesIdRole:  return QVariant::fromValue(place.geonamesId);
    case NameRole:        return place.name;
    case Admin1Role:      return place.admin1;
    case CountryRole:     return place.country;
    case CountryCodeRole: return place.countryCode;
    case TimezoneRole:    return place.timezone;
    case LatitudeRole:    return place.coordinate.latitude;
    case LongitudeRole:   return place.coordinate.longitude;
    case LabelRole:       return place.label();
    case RegionRole:      return place.region();
    case Qt::DisplayRole: return place.label();
    default:              return {};
    }
}

Place PlaceSearchModel::resultAt(int row) const
{
    return (row >= 0 && row < m_results.size()) ? m_results.at(row) : Place{};
}

void PlaceSearchModel::setLanguage(const QString &language)
{
    if (language.isEmpty() || language == m_language)
        return;
    m_language = language;

    // The language is part of the question, so changing it re-asks it. Not
    // immediately — through the same debounce, because a language picker bound
    // to a combo box can emit twice while the user scrolls it.
    if (!m_query.isEmpty())
        m_debounce.start();
}

void PlaceSearchModel::setMaximumResults(int count)
{
    m_maximumResults = std::max(1, count);
}

void PlaceSearchModel::setDebounceInterval(int milliseconds)
{
    m_debounce.setInterval(std::max(0, milliseconds));
}

void PlaceSearchModel::setQuery(const QString &query)
{
    if (query == m_query)
        return;

    m_query = query;
    Q_EMIT queryChanged();

    // Below the minimum the list empties at once rather than after the
    // debounce. A search field that has just been cleared should not go on
    // showing yesterday's results for a quarter of a second.
    if (m_query.simplified().size() < 2) {
        m_debounce.stop();
        ++m_generation;
        setSearching(false);
        setErrorMessage(QString());
        if (!m_results.isEmpty()) {
            beginResetModel();
            m_results.clear();
            endResetModel();
            Q_EMIT countChanged();
        }
        return;
    }

    m_debounce.start();
}

void PlaceSearchModel::searchNow()
{
    m_debounce.stop();
    dispatch();
}

void PlaceSearchModel::clear()
{
    setQuery(QString());
}

void PlaceSearchModel::dispatch()
{
    if (m_geocoder == nullptr)
        return;

    const QString name = m_query.simplified();
    if (name.size() < 2)
        return;

    const quint64 generation = ++m_generation;

    GeocodeQuery query;
    query.name = name;
    query.count = m_maximumResults;
    query.language = m_language;

    setSearching(true);
    setErrorMessage(QString());

    // A watcher per request, carrying the generation it belongs to, deleting
    // itself when it is done. One reused watcher would be cheaper and would
    // lose the generation: `setFuture` detaches from the old future, but the
    // handler then has no way to say which future it is reporting, and the
    // whole point of the number is to answer that.
    auto *watcher = new QFutureWatcher<Result<QList<Place>>>(this);
    connect(watcher, &QFutureWatcher<Result<QList<Place>>>::finished, this,
            [this, watcher, generation] {
                watcher->deleteLater();

                // A cancelled future has no result to read — QFuture::result()
                // on one asserts. It happens when the geocoder's QObject
                // context goes away mid-flight.
                if (watcher->isCanceled()) {
                    if (generation == m_generation)
                        setSearching(false);
                    return;
                }
                applyResults(watcher->result(), generation);
            });
    watcher->setFuture(m_geocoder->search(query));
}

void PlaceSearchModel::applyResults(const Result<QList<Place>> &result, quint64 generation)
{
    if (generation != m_generation)
        return;

    setSearching(false);

    if (!result) {
        setErrorMessage(result.error().toString());
        return;
    }

    beginResetModel();
    m_results = result.value();
    endResetModel();
    Q_EMIT countChanged();
}

void PlaceSearchModel::setSearching(bool searching)
{
    if (m_searching == searching)
        return;
    m_searching = searching;
    Q_EMIT searchingChanged();
}

void PlaceSearchModel::setErrorMessage(const QString &message)
{
    if (m_errorMessage == message)
        return;
    m_errorMessage = message;
    Q_EMIT errorMessageChanged();
}

} // namespace clima
