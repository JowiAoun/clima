// SPDX-FileCopyrightText: 2026 Jowi Aoun
// SPDX-License-Identifier: GPL-3.0-or-later

#include "daemonlink.h"

#include "widgetfeed.h"

#include "daemonconfig.h"

#include <QDBusConnection>
#include <QDBusConnectionInterface>
#include <QDBusInterface>
#include <QDBusReply>
#include <QDBusServiceWatcher>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLoggingCategory>
#include <QTimer>

namespace {

// The schema *this build* knows how to read.
//
// Deliberately a separate number from clima::wire::kSchemaVersion rather than
// an include of it, and that is the point rather than an oversight. The widget
// host and the daemon ship from different places on different clocks — the
// GNOME extension from extensions.gnome.org, the app and its daemon from
// Flathub (docs/widgets.md) — so a build of this file will routinely meet a
// daemon that was compiled from a different commit. Tying the two numbers
// together in CMake would make them agree on the developer's machine and
// nowhere else, which is the one place the disagreement does not matter.
//
// Bump it when this directory learns a new shape, and not when the daemon does.
constexpr int kUnderstoodSchema = 1;

// Local calls to a process that is already running. Three seconds is generous
// for that and short enough that a daemon which registered its name and then
// wedged does not hold the first paint of the desktop for the default 25.
constexpr int kCallTimeoutMs = 3000;

constexpr auto kSignature = "ss";

// Cut every array under `branch` down to `count` entries.
//
// Only the file source needs this. A subscription over the bus is answered by
// libclima/wire/snapshot.cpp, which slices to the horizon the feed asked for;
// a recorded file is one full snapshot served to every tile, so without this a
// six-hour rain tile would sum three hundred and sixty-nine hours and a
// twenty-four-hour sparkline would draw a fortnight as a sawtooth.
//
// From the front, because rule 3 of the wire format says a slice starts at now
// — index 0 of a recording is already the current hour.
//
// `count` < 0 means all of it, and 0 means the branch was not asked for at all
// and goes away entirely.
void trimSeries(QJsonObject &root, const QString &branch, int count)
{
    if (count < 0)
        return;
    if (!root.contains(branch))
        return;
    if (count == 0) {
        root.remove(branch);
        return;
    }

    QJsonObject section = root.value(branch).toObject();
    for (const QString &key : section.keys()) {
        const QJsonValue value = section.value(key);
        if (!value.isArray())
            continue;
        QJsonArray whole = value.toArray();
        if (whole.size() <= count)
            continue;
        QJsonArray cut;
        for (int i = 0; i < count; ++i)
            cut.append(whole.at(i));
        section.insert(key, cut);
    }
    root.insert(branch, section);
}

QVariantList catalogueFrom(const QByteArray &json, QString *error)
{
    QJsonParseError parse{};
    const QJsonDocument doc = QJsonDocument::fromJson(json, &parse);
    if (parse.error != QJsonParseError::NoError) {
        if (error)
            *error = parse.errorString();
        return {};
    }
    return doc.object().value(QStringLiteral("widgets")).toArray().toVariantList();
}

} // namespace

Q_LOGGING_CATEGORY(lcWidgets, "clima.widgets")

// ---- the singleton ----------------------------------------------------------

DaemonLink::DaemonLink(QObject *parent)
    : QObject(parent)
{
    // Started unconditionally, including in file mode, because "how old is
    // this" is a question a recorded snapshot has to answer honestly too.
    m_minute = new QTimer(this);
    m_minute->setInterval(60 * 1000);
    m_minute->setTimerType(Qt::VeryCoarseTimer);
    connect(m_minute, &QTimer::timeout, this, &DaemonLink::minutePassed);
    m_minute->start();

    loadEmbeddedCatalogue();
}

DaemonLink::~DaemonLink() = default;

DaemonLink *DaemonLink::instance()
{
    static DaemonLink *link = new DaemonLink;
    return link;
}

DaemonLink *DaemonLink::create(QQmlEngine *, QJSEngine *)
{
    DaemonLink *link = instance();
    QQmlEngine::setObjectOwnership(link, QQmlEngine::CppOwnership);
    return link;
}

QString DaemonLink::source() const
{
    if (!m_filePath.isEmpty())
        return QStringLiteral("file");
    return m_available ? QStringLiteral("bus") : QStringLiteral("none");
}

QVariantMap DaemonLink::widget(const QString &id) const
{
    for (const QVariant &entry : m_catalogue) {
        const QVariantMap map = entry.toMap();
        if (map.value(QStringLiteral("id")).toString() == id)
            return map;
    }
    return {};
}

// ---- the two sources --------------------------------------------------------

void DaemonLink::useSnapshotFile(const QString &path)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        qCWarning(lcWidgets, "could not read %s: %s", qPrintable(path),
                  qPrintable(file.errorString()));
        return;
    }

    m_filePath = path;
    m_fileJson = file.readAll();
    m_available = true;
    m_schemaVersion = QJsonDocument::fromJson(m_fileJson)
                          .object()
                          .value(QStringLiteral("schema"))
                          .toInt();

    // The same check the bus path makes. A recording is a snapshot like any
    // other and a stale one can be the wrong shape.
    if (m_schemaVersion != kUnderstoodSchema) {
        m_incompatibility = tr("%1 carries schema %2; this build reads %3.")
                                .arg(path)
                                .arg(m_schemaVersion)
                                .arg(kUnderstoodSchema);
    }

    Q_EMIT availableChanged();

    for (WidgetFeed *feed : std::as_const(m_feeds))
        deliver(feed, m_fileJson);
}

void DaemonLink::connectToBus()
{
    QDBusConnection bus = QDBusConnection::sessionBus();
    if (!bus.isConnected()) {
        qCWarning(lcWidgets, "no session bus: %s", qPrintable(bus.lastError().message()));
        return;
    }

    m_usingBus = true;

    // WatchForOwnerChange rather than ForRegistration alone, so that
    // `clima-daemon --replace` — one daemon handing the name to another — is a
    // re-subscribe rather than a permanent disconnection.
    m_watcher = new QDBusServiceWatcher(QStringLiteral(CLIMA_DAEMON_SERVICE), bus,
                                        QDBusServiceWatcher::WatchForOwnerChange, this);
    connect(m_watcher, &QDBusServiceWatcher::serviceRegistered, this,
            &DaemonLink::onServiceRegistered);
    connect(m_watcher, &QDBusServiceWatcher::serviceUnregistered, this,
            &DaemonLink::onServiceUnregistered);

    if (bus.interface()->isServiceRegistered(QStringLiteral(CLIMA_DAEMON_SERVICE)))
        handshake();
}

void DaemonLink::onServiceRegistered(const QString &)
{
    handshake();
}

void DaemonLink::onServiceUnregistered(const QString &)
{
    // Every token belonged to a daemon that no longer exists. Retire the match
    // rules so a *different* daemon reusing the same token strings cannot
    // deliver somebody else's place to a tile.
    for (WidgetFeed *feed : std::as_const(m_feeds))
        dropSubscription(feed);

    m_available = false;
    m_schemaVersion = 0;
    m_incompatibility.clear();
    Q_EMIT availableChanged();

    // Note what does *not* happen here: no feed is cleared. Every tile goes on
    // drawing its last snapshot and starts counting minutes. See the header.
}

// ---- handshake --------------------------------------------------------------

void DaemonLink::handshake()
{
    QDBusInterface daemon(QStringLiteral(CLIMA_DAEMON_SERVICE), QStringLiteral(CLIMA_DAEMON_PATH),
                          QStringLiteral(CLIMA_DAEMON_INTERFACE), QDBusConnection::sessionBus());
    daemon.setTimeout(kCallTimeoutMs);

    const QDBusReply<int> version = daemon.call(QStringLiteral("SchemaVersion"));
    if (!version.isValid()) {
        qCWarning(lcWidgets, "daemon did not answer SchemaVersion: %s",
                  qPrintable(version.error().message()));
        return;
    }

    m_schemaVersion = version.value();
    m_available     = true;

    // Both directions are a refusal, and that is the honest reading of the rule
    // in libclima/wire/snapshot.h: the number is bumped exactly when a reader
    // that understood the old shape would *misread* the new one. An older
    // widget against a newer daemon misreads; a newer widget against an older
    // daemon is reading a shape that was replaced for a reason. Drawing
    // something plausible from either is worse than saying so.
    if (m_schemaVersion != kUnderstoodSchema) {
        m_incompatibility = tr("The Clima daemon speaks schema %1; this build reads %2. "
                               "Update both to the same release.")
                                .arg(m_schemaVersion)
                                .arg(kUnderstoodSchema);
        qCWarning(lcWidgets, "%s", qPrintable(m_incompatibility));
        Q_EMIT availableChanged();
        return;
    }

    m_incompatibility.clear();
    loadCatalogue();
    subscribeAll();
    Q_EMIT availableChanged();
}

void DaemonLink::loadCatalogue()
{
    QDBusInterface daemon(QStringLiteral(CLIMA_DAEMON_SERVICE), QStringLiteral(CLIMA_DAEMON_PATH),
                          QStringLiteral(CLIMA_DAEMON_INTERFACE), QDBusConnection::sessionBus());
    daemon.setTimeout(kCallTimeoutMs);

    const QDBusReply<QString> reply = daemon.call(QStringLiteral("ListWidgets"));
    if (!reply.isValid()) {
        qCWarning(lcWidgets, "daemon did not answer ListWidgets: %s",
                  qPrintable(reply.error().message()));
        return;
    }

    QString      error;
    QVariantList list = catalogueFrom(reply.value().toUtf8(), &error);
    if (list.isEmpty()) {
        qCWarning(lcWidgets, "the daemon's catalogue did not parse (%s); using the built-in copy.",
                  qPrintable(error));
        return;
    }

    m_catalogue = list;
    Q_EMIT catalogueChanged();
}

void DaemonLink::loadEmbeddedCatalogue()
{
    // The same file the daemon serves, compiled into this binary as well.
    //
    // Two copies of the bytes, one copy of the file — widgets/catalogue.json is
    // listed in exactly one place in the repository and both targets embed it.
    // The daemon's answer wins when there is a daemon, because it is the one
    // that will have been upgraded alongside the data; this is what makes
    // `--list`, `--snapshot` and a first paint before the handshake possible at
    // all.
    QFile file(QStringLiteral(":/clima/catalogue.json"));
    if (!file.open(QIODevice::ReadOnly)) {
        qCWarning(lcWidgets, "the built-in widget catalogue is missing from this binary.");
        return;
    }

    QString error;
    m_catalogue = catalogueFrom(file.readAll(), &error);
    if (m_catalogue.isEmpty())
        qCWarning(lcWidgets, "the built-in widget catalogue did not parse: %s", qPrintable(error));
}

// ---- subscriptions ----------------------------------------------------------

void DaemonLink::attach(WidgetFeed *feed)
{
    if (m_feeds.contains(feed))
        return;
    m_feeds.append(feed);

    if (!m_fileJson.isEmpty()) {
        deliver(feed, m_fileJson);
        return;
    }
    if (m_available && m_incompatibility.isEmpty())
        resubscribe(feed);
}

void DaemonLink::detach(WidgetFeed *feed)
{
    dropSubscription(feed);
    m_feeds.removeAll(feed);
}

void DaemonLink::resubscribe(WidgetFeed *feed)
{
    if (!m_usingBus || !m_available || !m_incompatibility.isEmpty())
        return;

    dropSubscription(feed);

    QDBusConnection bus = QDBusConnection::sessionBus();
    QDBusInterface  daemon(QStringLiteral(CLIMA_DAEMON_SERVICE), QStringLiteral(CLIMA_DAEMON_PATH),
                          QStringLiteral(CLIMA_DAEMON_INTERFACE), bus);
    daemon.setTimeout(kCallTimeoutMs);

    const QDBusReply<QString> reply =
        daemon.call(QStringLiteral("Subscribe"), feed->place(), feed->fields(), feed->hours(),
                    feed->days());

    if (!reply.isValid() || reply.value().isEmpty()) {
        qCWarning(lcWidgets, "Subscribe(%s) failed: %s", qPrintable(feed->place()),
                  qPrintable(reply.isValid() ? QStringLiteral("no token returned")
                                             : reply.error().message()));
        return;
    }

    const QString token = reply.value();

    // The argument match is the reason the token is the signal's first
    // argument. Without the QStringList{token} below this connection would
    // receive every subscriber's snapshot and throw away all but its own,
    // which on a desktop with eight tiles is 8x the wakeups and 8x the parsing
    // for the same pixels. See the header.
    const bool matched = bus.connect(QStringLiteral(CLIMA_DAEMON_SERVICE),
                                     QStringLiteral(CLIMA_DAEMON_PATH),
                                     QStringLiteral(CLIMA_DAEMON_INTERFACE),
                                     QStringLiteral("SnapshotChanged"), QStringList{ token },
                                     QString::fromLatin1(kSignature), this,
                                     SLOT(onSnapshotChanged(QString, QString)));
    if (!matched) {
        qCWarning(lcWidgets, "could not add a match rule for token %s", qPrintable(token));
        daemon.call(QStringLiteral("Unsubscribe"), token);
        return;
    }

    m_byToken.insert(token, feed);
    m_tokens.insert(feed, token);

    // ---- and now the first snapshot, pulled rather than waited for ----------
    //
    // The match rule is in place, so this is the first moment anything the
    // daemon emits could reach us. The daemon used to push one snapshot on
    // subscribe and it could not work: this process was blocked in the
    // Subscribe call while that push went out, and AddMatch is itself a round
    // trip. Every tile came up blank against a live daemon while `gdbus
    // monitor` showed the signals going past.
    //
    // One extra call at startup, once per tile, against a local process. That
    // buys a deterministic first paint, which is worth more than the round trip
    // costs — and it is the same arguments, so the daemon answers it out of the
    // memory it already filled for the subscription.
    const QDBusReply<QString> first =
        daemon.call(QStringLiteral("GetSnapshot"), feed->place(), feed->fields(), feed->hours(),
                    feed->days());
    if (first.isValid() && !first.value().isEmpty())
        deliver(feed, first.value().toUtf8());
}

void DaemonLink::dropSubscription(WidgetFeed *feed)
{
    const QString token = m_tokens.take(feed);
    if (token.isEmpty())
        return;
    m_byToken.remove(token);

    if (!m_usingBus)
        return;

    QDBusConnection bus = QDBusConnection::sessionBus();
    bus.disconnect(QStringLiteral(CLIMA_DAEMON_SERVICE), QStringLiteral(CLIMA_DAEMON_PATH),
                   QStringLiteral(CLIMA_DAEMON_INTERFACE), QStringLiteral("SnapshotChanged"),
                   QStringList{ token }, QString::fromLatin1(kSignature), this,
                   SLOT(onSnapshotChanged(QString, QString)));

    // Best effort, and deliberately not checked: if the daemon has gone away
    // there is nobody to tell, and if it has not it drops the subscription on
    // the next publish anyway.
    QDBusInterface daemon(QStringLiteral(CLIMA_DAEMON_SERVICE), QStringLiteral(CLIMA_DAEMON_PATH),
                          QStringLiteral(CLIMA_DAEMON_INTERFACE), bus);
    daemon.setTimeout(kCallTimeoutMs);
    daemon.asyncCall(QStringLiteral("Unsubscribe"), token);
}

void DaemonLink::subscribeAll()
{
    for (WidgetFeed *feed : std::as_const(m_feeds))
        resubscribe(feed);
}

void DaemonLink::onSnapshotChanged(const QString &token, const QString &json)
{
    WidgetFeed *feed = m_byToken.value(token);
    if (!feed)
        return;
    deliver(feed, json.toUtf8());
}

void DaemonLink::deliver(WidgetFeed *feed, const QByteArray &json)
{
    QJsonParseError parse{};
    const QJsonDocument doc = QJsonDocument::fromJson(json, &parse);
    if (parse.error != QJsonParseError::NoError) {
        qCWarning(lcWidgets, "a snapshot did not parse: %s", qPrintable(parse.errorString()));
        return;
    }

    QJsonObject snapshot = doc.object();

    // Only in file mode. Over the bus the daemon has already sliced to this
    // feed's horizon; here one recording serves every tile and each has to be
    // given its own.
    if (!m_filePath.isEmpty()) {
        trimSeries(snapshot, QStringLiteral("hourly"), feed->hours());
        trimSeries(snapshot, QStringLiteral("daily"), feed->days());
    }

    // toVariantMap, not a hand-written walk, because it is what keeps a JSON
    // null a null QVariant all the way into QML. Rule 2 of the wire format
    // survives this line and nothing downstream has to re-establish it.
    feed->deliver(snapshot.toVariantMap());
}

void DaemonLink::requestRefresh(const QString &placeId)
{
    if (!m_usingBus || !m_available)
        return;

    QDBusInterface daemon(QStringLiteral(CLIMA_DAEMON_SERVICE), QStringLiteral(CLIMA_DAEMON_PATH),
                          QStringLiteral(CLIMA_DAEMON_INTERFACE), QDBusConnection::sessionBus());
    daemon.setTimeout(kCallTimeoutMs);
    daemon.asyncCall(QStringLiteral("RequestRefresh"), placeId);
}
