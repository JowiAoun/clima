// SPDX-FileCopyrightText: 2026 Jowi Aoun
// SPDX-License-Identifier: GPL-3.0-or-later

#include "daemonlink.h"

#include "widgetfeed.h"

#include "daemonconfig.h"

#include <QDBusConnection>
#include <QDBusConnectionInterface>
#include <QDBusInterface>
#include <QDBusPendingCallWatcher>
#include <QDBusPendingReply>
#include <QDBusReply>
#include <QDBusServiceWatcher>
#include <QDir>
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

// ---- the sentences a tile shows instead of a skeleton ------------------------
//
// Short, because the smallest tile in the catalogue is 240 px wide and has about
// two lines under its title. Each one names the thing that is wrong rather than
// the symptom: "not running" and "not answering" send somebody to different
// places, and a tile that said "no weather" for both would send them to neither.
//
// They are sentences and not codes because the alternative is a mapping in QML,
// which puts the words a long way from the code that knows which is true — and
// tr() here is the same choice DaemonLink::incompatibility already made.
QString notRunningText()
{
    return DaemonLink::tr("The Clima weather service is not running.");
}

QString notAnsweringText()
{
    return DaemonLink::tr("The Clima weather service is not answering.");
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
    // Until something establishes otherwise there is no weather service, and
    // saying so is the honest starting state rather than a pessimistic one:
    // this object is built before either source has been chosen, and every path
    // that finds one clears this. Setting it here rather than in connectToBus()
    // also means a future caller that forgets to choose a source at all gets a
    // tile that says what is wrong instead of one that loads forever.
    , m_reason(notRunningText())
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

        // Named, unlike the sentences above, because `--snapshot` is typed by
        // somebody who is looking at the terminal they typed it in and the file
        // they meant is the whole of what went wrong. This process never
        // touches the bus once the flag is given, so there is nothing else this
        // tile could be waiting for.
        setReason(tr("%1 could not be read.").arg(QDir::toNativeSeparators(path)));
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
    // The reason starts as "not running" (see the constructor) and every return
    // below leaves it that way. Only two things clear it: a handshake that
    // worked, and an activation request still in flight — which are exactly the
    // two cases where a snapshot really is on its way.
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

    // ---- the places moving under us -----------------------------------------
    //
    // Connected once, here, rather than per subscription: the match rule is on
    // the well-known name, so it survives the daemon exiting and coming back
    // and there is no token to key it on anyway.
    //
    // What it is for is the tile that has no subscription. A widget host that
    // started before the user had chosen a place got an empty token from
    // Subscribe, said so on the tile, and had nothing left that would ever make
    // it ask again — no timer, no retry, and the daemon has no way to push to a
    // subscription that was never created. This is that missing edge.
    bus.connect(QStringLiteral(CLIMA_DAEMON_SERVICE), QStringLiteral(CLIMA_DAEMON_PATH),
                QStringLiteral(CLIMA_DAEMON_INTERFACE), QStringLiteral("PlacesChanged"), this,
                SLOT(onPlacesChanged()));

    if (bus.interface()->isServiceRegistered(QStringLiteral(CLIMA_DAEMON_SERVICE))) {
        handshake();
        return;
    }

    startDaemon();
}

// ---- asking the bus to start one --------------------------------------------
//
// packaging/linux/clima-daemon.service.in makes the daemon activatable; this is
// the request that uses it. Without both halves a desktop that is not GNOME has
// nothing that starts the service between one login and the next — the
// extension is the only thing that ever did, and `--pin` put tiles on four other
// compositors where there is no extension.
//
// Asynchronous, and deliberately. Activation is a fork, an exec and a name
// registration, and the daemon opens its cache on the way up; a blocking call
// would hold this process before its first frame for as long as all of that
// takes. Nothing is waiting for the answer anyway — the service watcher above
// is already armed, so a daemon that appears is picked up by the same path that
// handles one which was restarted by hand.
void DaemonLink::startDaemon()
{
    QDBusConnectionInterface *bus = QDBusConnection::sessionBus().interface();
    if (bus == nullptr)
        return;

    // The reason is cleared for the length of the attempt, so the tiles show
    // their skeleton rather than a sentence they would have to take back a
    // moment later. This is the one moment where "a snapshot is on its way" is
    // true with no daemon on the bus.
    setReason({});

    auto *watcher = new QDBusPendingCallWatcher(
        bus->asyncCall(QStringLiteral("StartServiceByName"),
                       QString::fromLatin1(CLIMA_DAEMON_SERVICE), quint32(0)),
        this);

    connect(watcher, &QDBusPendingCallWatcher::finished, this,
            [this](QDBusPendingCallWatcher *call) {
                call->deleteLater();

                const QDBusPendingReply<uint> reply = *call;
                if (!reply.isError())
                    return; // The service watcher takes it from here.

                // Two ordinary ways to arrive here and they are not worth
                // separate messages: nothing installed a .service file (a build
                // tree, or a package that skipped it), or one is installed and
                // the binary it names is gone. Both mean the same thing to
                // whoever is reading — there is no weather service and nothing
                // is going to produce one.
                qCWarning(lcWidgets,
                          "no clima-daemon on the session bus, and the bus could not start one: "
                          "%s. The tiles will say so rather than sit on a skeleton. Start one "
                          "with `clima-daemon`, or `clima-daemon --fixture toronto` for recorded "
                          "data.",
                          qPrintable(reply.error().message()));

                setReason(notRunningText());
            });
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
    //
    // The reason below reaches those tiles too and is ignored by them, because
    // a feed with data has no waiting state left to explain. It is here for the
    // tile that never got its first snapshot — a desktop where the daemon died
    // during the handshake has both kinds on it at once.
    setReason(notRunningText());
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

        // A different sentence from "not running", and the difference is the
        // whole reason there are two: the name is owned, so starting another
        // daemon is not the answer and would fail. Something has the name and
        // is wedged, and that is what the reader needs to be told.
        setReason(notAnsweringText());
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

        // Cleared rather than set. Incompatibility is state 1 and outranks
        // everything, so the tile draws the sentence above; leaving a second
        // one on the feed would mean two explanations existing for one tile and
        // the surface choosing between them.
        setReason({});
        Q_EMIT availableChanged();
        return;
    }

    m_incompatibility.clear();
    setReason({});
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
    if (m_available && m_incompatibility.isEmpty()) {
        resubscribe(feed);
        return;
    }

    // Every feed attaches after main() has already decided what this process is
    // reading from — QML is loaded last — so whatever went wrong is known by
    // now and this tile can be told immediately rather than after a timeout.
    feed->setWaitingReason(m_reason);
}

void DaemonLink::detach(WidgetFeed *feed)
{
    dropSubscription(feed);
    m_feeds.removeAll(feed);
}

void DaemonLink::resubscribe(WidgetFeed *feed)
{
    if (!m_usingBus || !m_available || !m_incompatibility.isEmpty()) {
        feed->setWaitingReason(m_reason);
        return;
    }

    dropSubscription(feed);

    QDBusConnection bus = QDBusConnection::sessionBus();
    QDBusInterface  daemon(QStringLiteral(CLIMA_DAEMON_SERVICE), QStringLiteral(CLIMA_DAEMON_PATH),
                          QStringLiteral(CLIMA_DAEMON_INTERFACE), bus);
    daemon.setTimeout(kCallTimeoutMs);

    const QDBusReply<QString> reply =
        daemon.call(QStringLiteral("Subscribe"), feed->place(), feed->fields(), feed->hours(),
                    feed->days());

    if (!reply.isValid()) {
        qCWarning(lcWidgets, "Subscribe(%s) failed: %s", qPrintable(feed->place()),
                  qPrintable(reply.error().message()));
        feed->setWaitingReason(notAnsweringText());
        return;
    }

    // ---- a token that came back empty ---------------------------------------
    //
    // Not a failure of the call: the daemon answered, and the answer is that it
    // has no place by that id (daemon/snapshotservice.cpp, canonical()). Which
    // is a first run, and a common one — a package installs the widgets and the
    // autostart entry together, so the tiles can reach a working daemon on a
    // desktop where nobody has opened Clima yet and chosen anywhere.
    //
    // That used to render as a skeleton, which is the least informative
    // possible answer to a question the reader can settle in ten seconds.
    if (reply.value().isEmpty()) {
        qCWarning(lcWidgets, "Subscribe(%s): the daemon has no such place.",
                  qPrintable(feed->place()));

        const bool home = feed->place().isEmpty() || feed->place() == QLatin1String("home");
        feed->setWaitingReason(home ? tr("No place yet. Open Clima and choose one.")
                                    : tr("That place is not in Clima any more."));
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
        feed->setWaitingReason(notAnsweringText());
        return;
    }

    m_byToken.insert(token, feed);
    m_tokens.insert(feed, token);

    // Subscribed. Anything this tile was told about why it had nothing is now
    // out of date, and the skeleton is the honest picture again for the one
    // call it takes to fill it.
    feed->setWaitingReason({});

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

void DaemonLink::setReason(const QString &reason)
{
    m_reason = reason;

    // Pushed to every feed, including the ones that have data and will ignore
    // it. Filtering here would put the state order in two files — this one and
    // WidgetSurface.qml — and they would disagree the first time either moved.
    for (WidgetFeed *feed : std::as_const(m_feeds))
        feed->setWaitingReason(reason);
}

void DaemonLink::onPlacesChanged()
{
    // Every feed, not only the ones that failed. A subscription made against
    // "home" was resolved to a row id when it was made, and the daemon has
    // already re-pointed it — but a tile naming a specific place that has just
    // been deleted, or one whose subscription predates the place it wants,
    // is only put right by asking again. Four tiles is four round trips to a
    // local process, on an event that happens when somebody edits their places.
    subscribeAll();
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
