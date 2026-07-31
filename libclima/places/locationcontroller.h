// SPDX-FileCopyrightText: 2026 Jowi Aoun
// SPDX-License-Identifier: MPL-2.0
//
// The user's saved places, as a list model, and which one the app is showing.
//
// ---- the cold start is the whole design constraint --------------------------
//
// The app must come up on the home place before any network call completes.
// Not "quickly" — *before*, because the first frame is drawn from the cached
// forecast and a frame drawn before the place name is known has a hole in it
// where the location bar goes. So `load()` is synchronous: one SELECT over a
// table with a handful of rows, sub-millisecond against a cold page cache,
// against a cold-start budget of 400 ms.
//
// That is why this takes a CacheStore rather than talking to a geocoder. A
// saved place is a row, not a lookup. The geocoder is how a place *becomes* a
// row, once, and after that it is never consulted to open the app.
//
// ---- which place is current, and why it is remembered in the cache ----------
//
// The current place is persisted in the engine's own settings table
// (libclima/cache/migrations.cpp explains why that is not app/settings.h): the
// answer has to travel with the cache it describes, so that a Plasma applet and
// a CLI reusing libclima open on the same place the app did. A QSettings entry
// in the app's config would be invisible to both.
//
// Two keys, because they are two different facts:
//
//   places.current   the place last looked at. Restored on start.
//   the is_home flag on the row itself, which the places table enforces to at
//   most one with a partial unique index — see migration 2.
//
// A start with no remembered current falls back to home, and a start with
// neither falls back to the first row. A start with no rows at all leaves
// `currentIndex` at -1, which is the state the UI renders as "search for a
// place".
//
// ---- what the QML connects to -----------------------------------------------
//
// app/qml/Clima/LocationBar.qml already emits `changeRequested()` and
// `homeToggled()`, and both currently go nowhere. They should go here:
//
//   onChangeRequested  → open the place picker; the picker's list binds to
//                        this model, and choosing row N calls
//                        `LocationController.setCurrentIndex(N)`.
//   onHomeToggled      → `LocationController.toggleHome(LocationController
//                        .currentIndex)`
//
// and the bar's own two properties bind to `currentLabel` and `currentIsHome`.
// Wiring that up is a later agent's job; this class exists so that the wiring
// is three bindings and not a feature.
//
// ---- threading ---------------------------------------------------------------
//
// The GUI thread, like everything that touches a QAbstractItemModel. The
// CacheStore it holds belongs to the same thread, per that class's own rule.

#pragma once

#include "libclima/core/result.h"
#include "libclima/domain/place.h"

#include <QAbstractListModel>
#include <QList>
#include <QString>

namespace clima {

class CacheStore;

class LocationController : public QAbstractListModel
{
    Q_OBJECT

    Q_PROPERTY(int count READ count NOTIFY countChanged)
    Q_PROPERTY(int currentIndex READ currentIndex WRITE setCurrentIndex NOTIFY currentChanged)
    Q_PROPERTY(QString currentLabel READ currentLabel NOTIFY currentChanged)
    Q_PROPERTY(QString currentRegion READ currentRegion NOTIFY currentChanged)
    Q_PROPERTY(bool currentIsHome READ currentIsHome NOTIFY currentChanged)

public:
    // `cache` must outlive this and must already be open. Not owned.
    explicit LocationController(CacheStore *cache, QObject *parent = nullptr);
    ~LocationController() override;

    enum Role {
        PlaceIdRole = Qt::UserRole + 1,
        GeonamesIdRole,
        NameRole,
        Admin1Role,
        CountryRole,
        CountryCodeRole,
        TimezoneRole,
        LatitudeRole,
        LongitudeRole,
        IsHomeRole,
        LabelRole,     // "Toronto, Ontario" — what the location bar shows
        RegionRole,    // "Ontario, Canada" — the second line in a picker
    };
    Q_ENUM(Role)

    // Reads every saved place and restores the current selection. Synchronous,
    // on purpose — see the header comment. Safe to call again; it re-reads.
    Status load();

    [[nodiscard]] int      rowCount(const QModelIndex &parent = {}) const override;
    [[nodiscard]] QVariant data(const QModelIndex &index, int role) const override;
    [[nodiscard]] QHash<int, QByteArray> roleNames() const override;

    [[nodiscard]] int count() const { return int(m_places.size()); }

    // -1 when there are no places. Setting it out of range is ignored rather
    // than clamped: a caller asking for row 7 of a three-row model has a bug,
    // and silently showing row 2 hides it.
    [[nodiscard]] int currentIndex() const { return m_currentIndex; }
    Q_INVOKABLE bool  setCurrentIndex(int row);

    [[nodiscard]] QString currentLabel() const;
    [[nodiscard]] QString currentRegion() const;
    [[nodiscard]] bool    currentIsHome() const;

    // An invalid Place (id 0, empty name) when there is no current place.
    [[nodiscard]] Place currentPlace() const;
    [[nodiscard]] Place placeAt(int row) const;

    [[nodiscard]] QList<Place> places() const { return m_places; }

    // The row holding this GeoNames id, or -1. This is how "the place the user
    // just searched for is already saved" is answered, and it is why
    // `Place::geonamesId` is stored: the same question asked by name would say
    // no the day upstream changed a spelling.
    [[nodiscard]] Q_INVOKABLE int indexOfGeonamesId(qint64 geonamesId) const;

    // Adds, or returns the existing row when the place is already saved —
    // `Place::isSameEntity` decides, so a place found by searching and the
    // same place found by standing in it do not become two rows. Returns the
    // row, or -1 if the write failed.
    //
    // The first place ever added becomes home, because an app whose only saved
    // place is not its home place has a home nothing points at.
    int addPlace(const Place &place, bool makeCurrent = true);

    Q_INVOKABLE bool removeAt(int row);

    // Reorders. `to` is the destination row after the move, the way a drag
    // handle means it.
    Q_INVOKABLE bool move(int from, int to);

    // Exactly one place is home. Setting a new one clears the old, in one
    // transaction, because the partial unique index in migration 2 would
    // otherwise reject the second write.
    Q_INVOKABLE bool setHome(int row);

    // What LocationBar's home marker calls. Turning home *off* is not offered:
    // it would leave the app with nothing to open on. Tapping the marker on a
    // place that is already home is therefore a no-op that returns true, which
    // is what a toggle with one legal direction means.
    Q_INVOKABLE bool toggleHome(int row);

    [[nodiscard]] int homeIndex() const;

Q_SIGNALS:
    void countChanged();
    void currentChanged();
    void homeChanged();

    // A storage failure. Not fatal and not an exception: the model is still
    // consistent with what is in memory, and the caller decides whether a
    // failed write is worth telling the user about.
    void failed(const QString &message);

private:
    [[nodiscard]] bool isValidRow(int row) const;
    void               emitRowChanged(int row);
    void               rememberCurrent();
    bool               persist(Place &place);

    CacheStore  *m_cache = nullptr;
    QList<Place> m_places;
    int          m_currentIndex = -1;
};

} // namespace clima
