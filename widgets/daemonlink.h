// SPDX-FileCopyrightText: 2026 Jowi Aoun
// SPDX-License-Identifier: GPL-3.0-or-later
//
// The widget host's end of the session bus. One connection, however many tiles.
//
// ============================================================================
// WHAT THIS IS FOR
//
// clima-daemon fetches the weather once and serves it to everything on the
// desktop (daemon/snapshotservice.h). This is the reader: it finds the daemon,
// re-finds it when it restarts, keeps one subscription per tile, and hands
// each tile its own snapshot. Nothing here draws and nothing here fetches.
//
// ============================================================================
// arg0 FILTERING IS THE WHOLE POINT OF THE TOKEN
//
// SnapshotChanged carries the subscription token as its *first* argument, and
// this class connects to it once per token with an argument match:
//
//     QDBusConnection::connect(service, path, interface, "SnapshotChanged",
//                              QStringList{token}, "ss", this, SLOT(...))
//
// so the bus daemon filters before delivery. A desktop with eight tiles on it
// therefore wakes one tile when one tile's data changes, not eight — which is
// what keeps the ~0 % idle CPU line in docs/03-tech-stack.md §3.4 true once
// somebody actually fills their desktop. Connecting without the match would
// work, would look identical on a two-tile test, and would quietly cost 8× the
// wakeups and 8× the JSON parsing in the case the design was written for.
//
// ============================================================================
// A DEAD DAEMON IS NOT A BLANK TILE
//
// The daemon can go away — an upgrade, a crash, a user logging into a session
// where it is not autostarted. When it does, every WidgetFeed keeps the last
// snapshot it was given and `available` goes false; the tiles keep drawing and
// start saying how old their reading is. This is non-negotiable 1 in
// docs/README.md, one process further out: the app never shows a number it
// cannot source, and it never shows nothing where it has something.
//
// When the name comes back, every attached feed is re-subscribed from here.
// Tokens are not stable across a daemon restart — the new daemon has never
// heard of the old ones — so the match rules are torn down and rebuilt.
//
// ============================================================================
// THE FILE SOURCE
//
// `--snapshot <file>` replaces the bus with one recorded JSON document, served
// to every feed regardless of its mask. That is deliberately not a fixture
// *provider*: this process must not link the engine's network or cache code
// (see widgets/CMakeLists.txt), and a recorded snapshot is the whole of what a
// widget ever sees anyway. It is what makes the tiles reviewable in the gallery
// and screenshot-able in CI without a session bus.

#pragma once

#include <QHash>
#include <QObject>
#include <QQmlEngine>
#include <QString>
#include <QStringList>
#include <QVariantList>
#include <QVariantMap>

class WidgetFeed;
class QDBusServiceWatcher;
class QTimer;

class DaemonLink : public QObject
{
    Q_OBJECT
    QML_ELEMENT
    QML_SINGLETON

    // Whether there is a daemon on the bus right now. False is not an error
    // state — it is the state a tile has to keep drawing through.
    Q_PROPERTY(bool available READ isAvailable NOTIFY availableChanged)

    // "bus" | "file" | "none". A tile does not branch on this; the footer does,
    // because "no daemon" and "reading a recorded file" are different things to
    // admit to.
    Q_PROPERTY(QString source READ source NOTIFY availableChanged)

    // The daemon's JSON schema version, or 0 when there is nobody to ask.
    // A reader checks this before trusting anything else it is given.
    Q_PROPERTY(int schemaVersion READ schemaVersion NOTIFY availableChanged)

    // Non-empty when the daemon speaks a schema this build does not understand.
    // The tiles show it instead of drawing, because a shape mismatch is the one
    // failure where last-known data is *not* the honest answer.
    Q_PROPERTY(QString incompatibility READ incompatibility NOTIFY availableChanged)

    // widgets/catalogue.json as a list of maps, from the daemon when there is
    // one and from this binary's own copy when there is not.
    Q_PROPERTY(QVariantList catalogue READ catalogue NOTIFY catalogueChanged)

public:
    ~DaemonLink() override;

    static DaemonLink *instance();
    static DaemonLink *create(QQmlEngine *qmlEngine, QJSEngine *jsEngine);

    // Called from main() before the QML is loaded. Serves `path`'s contents to
    // every feed and never touches the bus.
    void useSnapshotFile(const QString &path);

    // Called from main() when there is a bus to use. Safe to call when there is
    // not: it reports the failure and leaves `available` false, which is the
    // same state as a daemon that is simply not running.
    void connectToBus();

    [[nodiscard]] bool    isAvailable() const { return m_available; }
    [[nodiscard]] QString source() const;
    [[nodiscard]] int     schemaVersion() const { return m_schemaVersion; }
    [[nodiscard]] QString incompatibility() const { return m_incompatibility; }
    [[nodiscard]] QVariantList catalogue() const { return m_catalogue; }

    // The catalogue entry with this id, or an empty map. QML uses it to size a
    // tile and to know which fields to ask for, so that a widget's contract
    // with the daemon is read from the one file rather than repeated in QML.
    Q_INVOKABLE [[nodiscard]] QVariantMap widget(const QString &id) const;

    // Ask the daemon to refresh now. Whether a socket is opened is still its
    // cache policy's decision, so this is not a way to hammer a provider.
    Q_INVOKABLE void requestRefresh(const QString &placeId);

    // ---- what WidgetFeed calls --------------------------------------------
    void attach(WidgetFeed *feed);
    void detach(WidgetFeed *feed);
    void resubscribe(WidgetFeed *feed);

Q_SIGNALS:
    void availableChanged();
    void catalogueChanged();

    // Once a minute, for everything that displays an age. One timer for the
    // process rather than one per tile: the number it drives changes at most
    // once a minute anyway, and ten timers would be ten wakeups to compute the
    // same minute ten times.
    void minutePassed();

private Q_SLOTS:
    void onServiceRegistered(const QString &service);
    void onServiceUnregistered(const QString &service);
    void onSnapshotChanged(const QString &token, const QString &json);

private:
    // ---- why the constructor is private -----------------------------------
    //
    // Not tidiness. It is the difference between QML using create() below and
    // QML silently building a SECOND instance of this class.
    //
    // Qt decides how to construct a QML_SINGLETON in
    // QQmlPrivate::singletonConstructionMode(), and it checks in this order:
    //
    //     if constexpr (std::is_default_constructible<T>::value)
    //         return SingletonConstructionMode::Constructor;   // <- wins
    //     if constexpr (HasSingletonFactory<T>::value)
    //         return SingletonConstructionMode::Factory;
    //
    // So a public `Foo(QObject *parent = nullptr)` makes the type default
    // constructible, that branch is taken first, and `create()` is never called
    // no matter that it exists and compiles. The engine gets a fresh object;
    // C++ goes on holding the one it made; nothing warns.
    //
    // What that looked like here: the command line parsed correctly, the
    // snapshot loaded correctly, and the tiles came up empty — because QML's
    // WidgetOptions had an empty widget list and QML's DaemonLink had never
    // been told about the recorded snapshot. Two hours, and the fix is one
    // access specifier.
    //
    // A private constructor makes is_default_constructible false at the point
    // Qt asks, so the factory branch is reached. app/settings.h and
    // app/viewmodels/units.h have private constructors for the same reason.
    explicit DaemonLink(QObject *parent = nullptr);

    void handshake();
    void loadCatalogue();
    void loadEmbeddedCatalogue();
    void subscribeAll();
    void dropSubscription(WidgetFeed *feed);
    void deliver(WidgetFeed *feed, const QByteArray &json);

    bool         m_available     = false;
    bool         m_usingBus      = false;
    int          m_schemaVersion = 0;
    QString      m_incompatibility;
    QString      m_filePath;
    QByteArray   m_fileJson;
    QVariantList m_catalogue;

    QDBusServiceWatcher *m_watcher = nullptr;
    QTimer              *m_minute  = nullptr;

    // Both directions. Delivery arrives keyed by token; a re-subscribe has to
    // find the token a feed already holds so it can retire its match rule.
    QHash<QString, WidgetFeed *> m_byToken;
    QHash<WidgetFeed *, QString> m_tokens;
    QList<WidgetFeed *>          m_feeds;
};
