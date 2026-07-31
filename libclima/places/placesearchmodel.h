// SPDX-FileCopyrightText: 2026 Jowi Aoun
// SPDX-License-Identifier: MPL-2.0
//
// The search box, as a model: a string in, a list of places out, one request
// per pause rather than one per keystroke.
//
// ---- the 250 ms is the whole class ------------------------------------------
//
// Search-as-you-type is the feature docs/06-roadmap.md asks for and it is also
// the fastest way to send sixty requests a minute to a service that costs
// nothing and asked for nothing in return. "Toronto" typed at a normal pace is
// seven requests, six of which are answers to prefixes the user never wanted
// and all of which are already obsolete by the time they arrive.
//
// So the query is debounced: a keystroke restarts a 250 ms timer and only its
// expiry sends anything. A quarter of a second is the number every search field
// converges on — long enough that a fluent typist makes one request for a word,
// short enough that a hunt-and-peck typist does not notice it — and it is
// settable so that a test does not have to wait for it.
//
// The minimum length is the second half of the same restraint. Two characters
// is what Open-Meteo supports for an exact match (docs/02-data-sources.md
// §2.7); one character would match a large fraction of the planet and is a
// request nobody wanted to make. Below the minimum, the model empties and
// sends nothing — it does not error, because a person who has typed one letter
// has not made a mistake.
//
// ---- why the debounce is here and not in the provider -----------------------
//
// OpenMeteoGeocoder answers every call it is given, because its other callers
// are not typing: resolving a saved place by id, refreshing a stale entry, a
// CLI. A timer inside the provider would make all of them wait for a pause
// that never comes. The thing that knows the user stopped typing is the thing
// bound to the text field, which is this.
//
// ---- out-of-order answers ---------------------------------------------------
//
// The debounce makes overlapping requests rare and not impossible: a slow
// answer to "Tor" can arrive after a fast one to "Toron". Every search carries
// a generation number and an answer from an old generation is dropped, so the
// list can never end up showing results for a query the box no longer holds.

#pragma once

#include "libclima/core/result.h"
#include "libclima/domain/place.h"

#include <QAbstractListModel>
#include <QList>
#include <QString>
#include <QTimer>

namespace clima {

class IForwardGeocoder;

class PlaceSearchModel : public QAbstractListModel
{
    Q_OBJECT

    Q_PROPERTY(QString query READ query WRITE setQuery NOTIFY queryChanged)
    Q_PROPERTY(int count READ count NOTIFY countChanged)
    Q_PROPERTY(bool searching READ isSearching NOTIFY searchingChanged)
    Q_PROPERTY(QString errorMessage READ errorMessage NOTIFY errorMessageChanged)

public:
    // `geocoder` must outlive this. Not owned.
    explicit PlaceSearchModel(IForwardGeocoder *geocoder, QObject *parent = nullptr);
    ~PlaceSearchModel() override;

    static constexpr int defaultDebounceMs = 250;

    enum Role {
        GeonamesIdRole = Qt::UserRole + 1,
        NameRole,
        Admin1Role,
        CountryRole,
        CountryCodeRole,
        TimezoneRole,
        LatitudeRole,
        LongitudeRole,
        LabelRole,     // "Toronto, Ontario"
        RegionRole,    // "Ontario, Canada"
    };
    Q_ENUM(Role)

    [[nodiscard]] int      rowCount(const QModelIndex &parent = {}) const override;
    [[nodiscard]] QVariant data(const QModelIndex &index, int role) const override;
    [[nodiscard]] QHash<int, QByteArray> roleNames() const override;

    [[nodiscard]] QString query() const { return m_query; }
    void                  setQuery(const QString &query);

    [[nodiscard]] int  count() const { return int(m_results.size()); }
    [[nodiscard]] bool isSearching() const { return m_searching; }

    // Empty unless the last search failed. Developer-facing English; a
    // user-facing string is the app's job and goes through Qt Linguist.
    [[nodiscard]] QString errorMessage() const { return m_errorMessage; }

    [[nodiscard]] Place resultAt(int row) const;

    // ISO 639-1. Part of the cache key, because "Paris" in French and "Paris"
    // in Japanese are two answers to two questions (§4.5).
    void                  setLanguage(const QString &language);
    [[nodiscard]] QString language() const { return m_language; }

    void                 setMaximumResults(int count);
    [[nodiscard]] int    maximumResults() const { return m_maximumResults; }

    // Zero sends immediately, which is what a test wants and what nothing else
    // does.
    void              setDebounceInterval(int milliseconds);
    [[nodiscard]] int debounceInterval() const { return m_debounce.interval(); }

    // Sends now, without waiting out the debounce. For a search field's return
    // key, where the user has said they are finished.
    //
    // Not `submit()`. QAbstractItemModel already has one, it returns bool, and
    // an override that differs only in return type is a compile error that
    // names moc's generated file rather than this line.
    Q_INVOKABLE void searchNow();

    Q_INVOKABLE void clear();

Q_SIGNALS:
    void queryChanged();
    void countChanged();
    void searchingChanged();
    void errorMessageChanged();

private:
    void dispatch();
    void applyResults(const Result<QList<Place>> &result, quint64 generation);
    void setSearching(bool searching);
    void setErrorMessage(const QString &message);

    IForwardGeocoder *m_geocoder = nullptr;

    QString m_query;
    QString m_language = QStringLiteral("en");
    int     m_maximumResults = 10;

    QTimer  m_debounce;

    // Bumped by every dispatch. An answer that arrives carrying an older
    // number is an answer to a question the box no longer asks, and is
    // dropped — see the header comment on out-of-order answers.
    quint64 m_generation = 0;

    QList<Place> m_results;
    bool         m_searching = false;
    QString      m_errorMessage;
};

} // namespace clima
