// SPDX-FileCopyrightText: 2026 Jowi Aoun
// SPDX-License-Identifier: MPL-2.0
//
// The shape of a forecast on the wire, and the mask that trims it.
//
// ============================================================================
// WHY THERE IS A WIRE AT ALL
//
// Because SQLite has one writer, and a desktop with six widgets on it would
// otherwise have seven processes fetching and writing the same forecast. That
// is seven times the requests against a free non-commercial tier (R5), seven
// copies of the same rows, and a lock contention bug that only appears on the
// machines of people who like widgets.
//
// So exactly one process — clima-daemon — owns the network and the cache, and
// everything else subscribes. The daemon is the single writer; the app, the
// widget host and the tray are readers.
//
// ============================================================================
// WHY JSON AND NOT A TYPED D-BUS SIGNATURE
//
// A typed signature — a(sdd) and so on — is smaller, faster and checked by the
// bus. It is also the wrong trade here, for one reason: **the two ends ship on
// different clocks and from different places.**
//
// The GNOME extension is published to extensions.gnome.org and the app to
// Flathub, and gnome-shell will not load an extension from inside a Flatpak
// (docs/widgets.md), so they update independently and will routinely disagree
// by a version. With a typed signature, adding one field is an interface break
// that desynchronises them and produces an unmarshalling error rather than a
// missing number. With JSON, an older reader ignores a key it does not know
// and a newer reader finds it absent — which is the failure mode you want when
// you cannot make the two ends update together.
//
// The cost is a parse on each end. A masked snapshot is a few hundred bytes to
// a few kilobytes and arrives on a timer measured in minutes, so this does not
// matter, and `schema` below is what a reader checks before trusting any of it.
//
// ============================================================================
// THREE RULES THE ENCODER KEEPS, WHICH ARE EASY TO GET WRONG
//
//   1. **A series always carries its time axis.** Ask for `hourly.temperature`
//      and you also get `hourly.time`, whether or not you asked. An array of
//      twelve numbers with no axis looks perfectly usable and is silently
//      wrong the moment a slice starts anywhere other than the hour you
//      assumed — and it looks plausible, which is how it would ship. Same for
//      `daily.date`.
//
//   2. **Absent is null, and null is not zero.** `Reading` is
//      std::optional<double> and stays distinguishable on the wire. A widget
//      that draws 0 mm of rain where the provider said nothing at all is
//      reporting a dry hour it has no evidence for.
//
//   3. **A slice starts at now, not at the start of the array.** The forecast
//      is fetched with `past_days=1`, so index 0 is roughly a day behind. A
//      widget that asked for six hours means the next six.
//
// ============================================================================
// WHAT IS DELIBERATELY NOT HERE
//
// No colour, no band name, no formatted string, no unit conversion. The daemon
// answers what the weather is; how it reads is the reader's, and it is the
// app's own presentation layer that widgets share. Emitting an air-quality
// band here would put the thresholds in two places and let them drift, which
// is the specific failure docs/10-design-system.md §10.5 is about.

#pragma once

#include "libclima/domain/airquality.h"
#include "libclima/domain/alert.h"
#include "libclima/domain/forecast.h"
#include "libclima/domain/place.h"

#include <QDateTime>
#include <QJsonObject>
#include <QSet>
#include <QString>
#include <QStringList>

namespace clima::wire {

// Bumped only when a reader that understood the old shape would misread the
// new one. Adding a key is not that; renaming or re-meaning one is.
inline constexpr int kSchemaVersion = 1;

// Which branches of the snapshot a subscriber asked for.
//
// Paths are dotted and a branch selects everything under it: "current" implies
// "current.temperature", and "current.temperature" implies only itself. The
// empty mask means everything, because a reader that names no fields is asking
// for the whole thing rather than for nothing — the opposite convention would
// turn a forgotten argument into a silently empty widget.
class FieldMask
{
public:
    FieldMask() = default;

    static FieldMask everything();
    static FieldMask fromFields(const QStringList &fields);

    [[nodiscard]] bool isEverything() const { return m_everything; }

    // True if `path` was named, or if any prefix branch of it was.
    [[nodiscard]] bool wants(const QString &path) const;

    // True if anything at all under `branch` was named. Used to decide whether
    // a whole section is worth building.
    [[nodiscard]] bool wantsAnyUnder(const QString &branch) const;

    [[nodiscard]] QStringList fields() const;

private:
    bool          m_everything = false;
    QSet<QString> m_paths;
};

// Everything a snapshot is built from. The daemon fills this in; the encoder
// does no fetching, no clock reading and no I/O, which is what makes it
// testable against a recorded fixture.
struct SnapshotSource {
    QString placeId;
    Place   place;

    Forecast   forecast;
    AirQuality airQuality;
    AlertSet   alerts;

    // The instant the slices are taken from and alerts are filtered against.
    QDateTime now;

    // How much of each series to send. 0 sends none; a negative number sends
    // all of it. A widget declares these in widgets/catalogue.json.
    int hours = 0;
    int days  = 0;

    // Which provider answered, and whether this came off the network this
    // cycle or out of the cache. Both travel so that a widget can say "updated
    // 40 minutes ago" and mean it — the daemon going away must leave a stale
    // reading on screen, never a blank tile.
    QString servedBy;
    bool    fromCache = false;
};

// The snapshot, masked. Always carries `schema`, `placeId`, `generatedAt` and
// `state` regardless of the mask: a reader has to be able to tell how old this
// is and whether it understands it before it looks at anything else.
[[nodiscard]] QJsonObject buildSnapshot(const SnapshotSource &source, const FieldMask &mask);

// Compact, because this goes over a bus rather than into a file.
[[nodiscard]] QByteArray encode(const QJsonObject &snapshot);

} // namespace clima::wire
