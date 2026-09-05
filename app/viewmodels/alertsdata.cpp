// SPDX-FileCopyrightText: 2026 Jowi Aoun
// SPDX-License-Identifier: GPL-3.0-or-later

#include "app/viewmodels/alertsdata.h"

#include "app/platform/notifier.h"
#include "app/settings.h"
#include "app/viewmodels/timeformat.h"
#include "libclima/core/clock.h"

#include <QLocale>
#include <QSet>

#if __has_include(<QNetworkInformation>)
#    include <QNetworkInformation>
#    define CLIMA_HAVE_NETWORK_INFORMATION 1
#endif

using namespace clima;

namespace {

// The tick that takes an alert off the screen when its hazard ends. One minute:
// finer than that buys nothing a reader could notice, and coarser means a
// warning that ended at 23:00 is still there at 23:04.
constexpr int kTickMs = 60 * 1000;

constexpr int kPollActiveMs  = 3 * 60 * 1000;
constexpr int kPollIdleMs    = 10 * 60 * 1000;
constexpr int kPollMeteredMs = 15 * 60 * 1000;

// A hidden window that somebody asked to be interrupted from. The same
// fifteen minutes a metered connection gets, because the two are the same
// judgement: keep the feature alive at the lowest rate that still makes it
// true. docs/04-architecture.md §4.5's exception, and alertsdata.h's schedule.
constexpr int kPollHiddenNotifyingMs = 15 * 60 * 1000;

// The separator inside one stored acknowledgement. A unit separator rather than
// a comma or a pipe, because the key it has to survive is an NWS URN — colons,
// dots and digits — and picking a printable delimiter is picking one that will
// eventually appear in the data.
const QChar kFieldSeparator = QChar(0x1f);

// "11:00 PM" today, "Thu 11:00 PM" any other day.
//
// QLocale::ShortFormat on a whole QDateTime was the obvious thing and it is
// wrong twice over. It prints the date every time, so a warning ending tonight
// read "Until 05/08/2026 23:00" — six characters of information wrapped in
// eleven of noise, on the one line of the banner that has to survive elision.
// And under LC_ALL=C.UTF-8, which is what every capture and every CI run uses,
// it renders as "7 08 2026 05:00": a date nobody can read at a glance and one
// that does not look like a date at all.
//
// A weekday rather than a date for the other case, because an alert's horizon is
// hours to days. "Thu" is what a reader needs; "06/08" is what a form needs.
//
// ---- the time itself is the reader's preference, not the locale's ------------
//
// It used to be QLocale::ShortFormat on the QTime, which is the half of the
// paragraph above that survived. That was the only clock in this application
// that followed the locale — every other one hardcoded a 12-hour spelling — so
// under a C locale the chart said "3 PM" and the banner underneath it said
// "23:00", in the same window, in the same second. Neither was a decision.
//
// TimeFormat is now the one answer, and the same one the hour labels use.
QString stamp(const QDateTime &instant, const QDateTime &now, const QLocale &locale)
{
    const QDateTime local = instant.toLocalTime();
    const QString   time  = TimeFormat::instance()->clock(local.time());

    if (local.date() == now.toLocalTime().date())
        return time;

    return locale.dayName(local.date().dayOfWeek(), QLocale::ShortFormat)
        + QLatin1Char(' ') + time;
}

bool metered()
{
#ifdef CLIMA_HAVE_NETWORK_INFORMATION
    // Best effort, and never loaded on our behalf. QNetworkInformation has no
    // backend on some platforms and none at all in a headless run, and the
    // honest answer there is "assume not metered": being wrong costs one extra
    // poll every fifteen minutes, and refusing to poll because we could not
    // tell would cost a warning.
    if (const QNetworkInformation *info = QNetworkInformation::instance())
        return info->isMetered();
#endif
    return false;
}

} // namespace

// ---- construction --------------------------------------------------------------

AlertsData::AlertsData()
{
    m_tick.setInterval(kTickMs);
    m_tick.setTimerType(Qt::VeryCoarseTimer);
    connect(&m_tick, &QTimer::timeout, this, &AlertsData::rebuild);

    m_poll.setTimerType(Qt::VeryCoarseTimer);
    connect(&m_poll, &QTimer::timeout, this, &AlertsData::refreshRequested);

    // Every "Until 11:00 PM" on the banner is a string built in rebuild(), so a
    // reader who switches to a 24-hour clock while a warning is up gets the
    // banner redrawn rather than the one line in the app that did not hear.
    connect(TimeFormat::instance(), &TimeFormat::changed, this, &AlertsData::rebuild);
}

AlertsData::~AlertsData() = default;

AlertsData *AlertsData::instance()
{
    static AlertsData data;
    return &data;
}

AlertsData *AlertsData::create(QQmlEngine *, QJSEngine *)
{
    AlertsData *data = instance();
    QQmlEngine::setObjectOwnership(data, QQmlEngine::CppOwnership);
    return data;
}

void AlertsData::setClock(Clock *clock)
{
    m_clock = clock;
}

void AlertsData::setNotifier(Notifier *notifier)
{
    m_notifier = notifier;
}

void AlertsData::setSettings(Settings *settings)
{
    m_settings = settings;
    loadAcknowledgements();

    // Switching notifications on is what starts a hidden window polling again,
    // and off is what stops it. Without this the change would take effect the
    // next time the window was shown and hidden, which is a preference that
    // appears not to work.
    if (m_settings != nullptr) {
        connect(m_settings, &Settings::alertNotificationsChanged, this,
                &AlertsData::reschedule, Qt::UniqueConnection);
    }
}

QDateTime AlertsData::now() const
{
    // The app's clock, which under `--fixture` is frozen at the recording's
    // instant. A view model reading QDateTime::currentDateTimeUtc() here would
    // make every recorded banner empty itself the day the recording aged out,
    // and take its golden images with it.
    //
    // AND THERE IS NO FALLBACK, deliberately. The first version of this ended
    // `: QDateTime::currentDateTimeUtc()`, and tests/tst_sourcerules.cpp
    // refused it — correctly. A fallback to the wall clock is not a safety net,
    // it is the exact failure the rule exists to prevent: a run that forgot to
    // set the clock would keep working, look right, and quietly judge a
    // recorded alert against today.
    //
    // An invalid instant instead. Alert::phaseAt() answers Ended for it, so a
    // clockless model shows nothing at all — which is wrong in the direction
    // that is visible, and says so on stderr.
    if (m_clock == nullptr) {
        qWarning("clima: the alert model has no clock; no alert can be displayed");
        return {};
    }
    return m_clock->now();
}

// ---- the set -------------------------------------------------------------------

void AlertsData::apply(const AlertSet &set)
{
    m_set       = set;
    m_available = true;
    rebuild();

    // Only start ticking once there is something to tick over. An app showing a
    // place with no alerts does not need a timer at all.
    if (!m_tick.isActive())
        m_tick.start();

    reschedule();
}

void AlertsData::clear(bool available)
{
    m_set       = {};
    m_available = available;
    m_tick.stop();
    rebuild();
    reschedule();
}

void AlertsData::rebuild()
{
    const QDateTime instant = now();

    // THE line. Everything the screen shows is filtered against the clock, so a
    // set that arrived from a cache written yesterday cannot put an ended
    // warning on the screen today — see the header.
    const QList<Alert> shown = m_set.displayableAt(instant);

    QVariantList built;
    built.reserve(shown.size());
    for (const Alert &alert : shown)
        built.append(toVariant(alert));

    // Republished unconditionally rather than diffed. The list is at most a
    // handful of maps and this runs once a minute; a diff here would be an
    // optimisation whose only measurable effect is a place for a bug.
    m_list = built;
    announce(shown);
    Q_EMIT changed();
}

// ---- what is worth interrupting somebody for ------------------------------------

void AlertsData::announce(const QList<Alert> &shown)
{
    // The preference first, and it gates the bookkeeping as well as the
    // posting. rebuild() runs every minute whether or not anything was
    // fetched — that is what makes an alert leave the screen when its hazard
    // ends — so a hazard reaching its onset under a hidden window would
    // otherwise announce itself with the switch turned off, having never been
    // polled for at all.
    //
    // Turning it off takes down what is already up, rather than leaving a
    // notification the reader can no longer explain.
    if (m_settings == nullptr || !m_settings->alertNotifications()) {
        takeDownEverythingPosted();
        m_announced.clear();
        return;
    }

    // The reader is looking at the banner, which says everything a
    // notification would and says it better. Take them down — but keep
    // m_announced, or hiding the window again would announce the same hazards
    // a second time.
    if (m_visible) {
        takeDownEverythingPosted();
        return;
    }

    // Everything still on the screen, so that what has gone can be withdrawn.
    QSet<QString> standing;

    for (const Alert &alert : shown) {
        // The hazard, not the message — and that means INTERSECTING the keys,
        // the way isAcknowledged() below does, rather than taking the first.
        //
        // NWS re-sends an alert in full under a new id on every update, and
        // nwsalertprovider.cpp builds identityKeys as the message's own id
        // followed by every id it references. So the first key changes on every
        // update of one hazard: keyed on it, the severity guard below would
        // never match, and the reader would be re-interrupted by a fresh
        // notification for each update — 24 of the 25 alerts in force in
        // California on the recording afternoon were updates.
        //
        // The key already announced wins, so notify() and withdraw() go on
        // addressing the notification that is actually on screen.
        if (alert.identityKeys.isEmpty())
            continue;

        QString key;
        for (const QString &candidate : alert.identityKeys) {
            if (m_announced.contains(candidate)) {
                key = candidate;
                break;
            }
        }
        if (key.isEmpty())
            key = alert.identityKeys.constFirst();

        standing.insert(key);

        const auto seen = m_announced.constFind(key);
        if (seen != m_announced.cend() && alert.severity <= seen.value())
            continue;

        m_announced.insert(key, alert.severity);
        m_posted.insert(key);
        Q_EMIT announced(key, alertSeverityKey(alert.severity));

        if (m_notifier == nullptr)
            continue;

        // The banner's own two lines: what it is, and until when. Not the
        // description — a notification is a summons to look, and CAP
        // descriptions run to paragraphs.
        const QVariantMap shape = toVariant(alert);
        m_notifier->notify(key, alert.event, shape.value(QStringLiteral("when")).toString(),
                           alert.severity == AlertSeverity::Extreme ? Notifier::Priority::Urgent
                           : alert.severity == AlertSeverity::Severe ? Notifier::Priority::High
                                                                     : Notifier::Priority::Normal);
    }

    // A hazard that has ended, or that this place no longer has, takes its
    // notification with it. The reader should not have to dismiss a warning
    // about weather that is over.
    for (auto it = m_announced.begin(); it != m_announced.end();) {
        if (standing.contains(it.key())) {
            ++it;
            continue;
        }
        withdraw(it.key());
        it = m_announced.erase(it);
    }
}

void AlertsData::withdraw(const QString &key)
{
    if (m_posted.remove(key) == 0)
        return;
    Q_EMIT withdrawn(key);
    if (m_notifier != nullptr)
        m_notifier->withdraw(key);
}

void AlertsData::takeDownEverythingPosted()
{
    const QSet<QString> posted = m_posted;
    for (const QString &key : posted)
        withdraw(key);
}

QVariantMap AlertsData::top() const
{
    if (m_list.isEmpty())
        return {};
    return m_list.constFirst().toMap();
}

int AlertsData::moreCount() const
{
    return m_list.isEmpty() ? 0 : int(m_list.size()) - 1;
}

QVariantMap AlertsData::toVariant(const Alert &alert) const
{
    const QDateTime instant = now();
    const QLocale   locale;

    QVariantMap map;
    map[QStringLiteral("id")]          = alert.id;
    map[QStringLiteral("event")]       = alert.event;
    map[QStringLiteral("headline")]    = alert.headline;
    map[QStringLiteral("description")] = alert.description;
    map[QStringLiteral("instruction")] = alert.instruction;
    map[QStringLiteral("area")]        = alert.areaDescription;
    map[QStringLiteral("sender")]      = alert.senderName;
    map[QStringLiteral("web")]         = alert.web.toString();

    // Both, always. `severityKey` indexes Theme.severity and `issuerLabel` is
    // what the reader recognises — "yellow warning" is what weather.gc.ca
    // showed them, and "Moderate" is a word they have never seen about weather.
    map[QStringLiteral("severityKey")]  = alertSeverityKey(alert.severity);
    map[QStringLiteral("severityName")] = alertSeverityName(alert.severity);
    map[QStringLiteral("issuerLabel")]  = alert.issuerLabel;

    const AlertPhase phase = alert.phaseAt(instant);
    map[QStringLiteral("phase")] = alertPhaseName(phase).toLower();

    // The sentence under the headline. Built here rather than in QML because it
    // is the one place that has both timestamps and the phase, and a view that
    // recomputed the phase to pick a format string would be a second copy of
    // the rule this whole feature turns on.
    QString when;
    if (phase == AlertPhase::Pending && alert.onset.isValid()) {
        //: %1 is a time, e.g. "12:00 PM" or "Thu 11:00 PM"
        when = tr("Begins %1").arg(stamp(alert.onset, instant, locale));
    } else if (alert.hazardEnd().isValid()) {
        when = tr("Until %1").arg(stamp(alert.hazardEnd(), instant, locale));
    }
    map[QStringLiteral("when")] = when;

    // Past the issuer's refresh deadline. Not a reason to hide anything — the
    // banner reads it together with `unconfirmed` below, which is the half that
    // knows whether the poll actually failed.
    map[QStringLiteral("pastDeadline")] = alert.isPastRefreshDeadline(instant);

    map[QStringLiteral("acknowledged")] = isAcknowledged(alert);

    return map;
}

// ---- confidence -------------------------------------------------------------------

bool AlertsData::isUnconfirmed() const
{
    if (m_list.isEmpty())
        return false;

    // BOTH conditions, and neither on its own.
    //
    // Past `expires` alone is routine: the issuer is simply due to speak again,
    // and they usually have. It only becomes something to tell the user about
    // when our last attempt to hear them FAILED — at which point we are holding
    // a message its author has already disowned, and saying nothing would be
    // silently keeping it.
    //
    // The failure is pushed in by AppEngine rather than inferred from the set's
    // own timestamps. An earlier version compared `confirmedAt` against
    // `fetchedAt` and could never fire: a provider serving a stale set on a
    // failed refresh parses the cached bytes, so both come out of the same cache
    // row and are equal by construction.
    if (!m_refreshFailed)
        return false;

    return m_list.constFirst().toMap().value(QStringLiteral("pastDeadline")).toBool();
}

void AlertsData::setRefreshFailed(bool failed)
{
    if (m_refreshFailed == failed)
        return;
    m_refreshFailed = failed;
    Q_EMIT changed();
}

QString AlertsData::confirmedLabel() const
{
    if (!m_set.confirmedAt.isValid())
        return {};
    //: %1 is a time of day. Shown when an alert could not be re-checked.
    return tr("Last confirmed %1").arg(stamp(m_set.confirmedAt, now(), QLocale()));
}

// ---- acknowledgement ----------------------------------------------------------------

bool AlertsData::isAcknowledged() const
{
    if (m_list.isEmpty())
        return false;
    return m_list.constFirst().toMap().value(QStringLiteral("acknowledged")).toBool();
}

bool AlertsData::isAcknowledged(const Alert &alert) const
{
    for (const Acknowledgement &entry : m_acknowledged) {
        if (!alert.identityKeys.contains(entry.key))
            continue;

        // A raise un-acknowledges. An update at the same grade or lower stays
        // collapsed, which is the common case: NWS re-sends an alert in full to
        // move its expiry, and re-raising the banner for that would teach people
        // to dismiss without reading.
        if (alert.severity > entry.severity)
            return false;
        return true;
    }
    return false;
}

void AlertsData::acknowledge()
{
    if (m_list.isEmpty())
        return;

    const QDateTime    instant = now();
    const QList<Alert> shown   = m_set.displayableAt(instant);
    if (shown.isEmpty())
        return;

    const Alert &alert = shown.constFirst();

    // Stored against EVERY key the alert answers to, which is what lets the
    // next message in an NWS update chain match on the reference it carries
    // without anything having to be migrated.
    for (const QString &key : alert.identityKeys) {
        bool replaced = false;
        for (Acknowledgement &entry : m_acknowledged) {
            if (entry.key != key)
                continue;
            entry.severity = alert.severity;
            entry.until    = alert.hazardEnd();
            replaced       = true;
            break;
        }
        if (!replaced)
            m_acknowledged.append({ key, alert.severity, alert.hazardEnd() });
    }

    saveAcknowledgements();
    rebuild();
}

void AlertsData::reveal()
{
    if (m_list.isEmpty())
        return;

    const QList<Alert> shown = m_set.displayableAt(now());
    if (shown.isEmpty())
        return;

    const QStringList keys = shown.constFirst().identityKeys;

    // Dropped rather than downgraded. "Show me this again" is not a statement
    // about a severity, and leaving a lower-graded entry behind would make the
    // banner collapse again by itself the next time the issuer said anything.
    auto it = m_acknowledged.begin();
    while (it != m_acknowledged.end()) {
        if (keys.contains(it->key))
            it = m_acknowledged.erase(it);
        else
            ++it;
    }

    saveAcknowledgements();
    rebuild();
}

void AlertsData::loadAcknowledgements()
{
    m_acknowledged.clear();
    if (m_settings == nullptr)
        return;

    const QDateTime instant = now();

    const QStringList stored = m_settings->acknowledgedAlerts();
    for (const QString &line : stored) {
        const QStringList fields = line.split(kFieldSeparator);
        if (fields.size() != 3)
            continue;

        Acknowledgement entry;
        entry.key      = fields.at(0);
        entry.severity = static_cast<AlertSeverity>(fields.at(1).toInt());
        entry.until    = QDateTime::fromString(fields.at(2), Qt::ISODate);

        // Pruned on load. An entry whose hazard is over is a trap the next time
        // the same county is warned about the same thing.
        if (entry.until.isValid() && instant.isValid() && instant >= entry.until)
            continue;

        m_acknowledged.append(entry);
    }
}

void AlertsData::saveAcknowledgements()
{
    if (m_settings == nullptr)
        return;

    QStringList stored;
    stored.reserve(m_acknowledged.size());
    for (const Acknowledgement &entry : m_acknowledged) {
        stored.append(entry.key + kFieldSeparator + QString::number(int(entry.severity))
                      + kFieldSeparator + entry.until.toUTC().toString(Qt::ISODate));
    }
    m_settings->setAcknowledgedAlerts(stored);
}

// ---- the schedule ---------------------------------------------------------------------

void AlertsData::setWindowState(bool visible, bool focused)
{
    if (m_visible == visible && m_focused == focused)
        return;

    const bool wasHidden      = !m_visible;
    const bool visibilityMoved = m_visible != visible;

    m_visible = visible;
    m_focused = focused;
    reschedule();

    // Showing or hiding the window changes what is worth interrupting somebody
    // for, so the announcement pass has to run again — opening the window is
    // what takes a notification down, and nothing else would do it until the
    // next minute tick. Only on the visibility change: focus moves several
    // times a minute and does not affect it.
    if (visibilityMoved)
        rebuild();

    // ---- and catch up on what was missed while nobody was looking ----------
    //
    // "Hidden means stopped" bounds the bandwidth, and on its own it also means
    // the set on screen is as old as the moment the window was hidden. A
    // warning issued while the app sat in the dock would then be absent from
    // the banner for a further three minutes after the reader brought it back
    // — which is the one moment the banner exists for.
    //
    // The minute tick does not cover this and cannot: it re-filters what is
    // already held against the clock, so it retires a hazard that has ENDED and
    // can never introduce one that has begun. Only a fetch does that.
    //
    // On the transition alone, so a window merely gaining or losing focus does
    // not fetch, and only where there is somebody to ask.
    if (wasHidden && m_visible && m_available)
        Q_EMIT refreshRequested();
}

int AlertsData::pollIntervalMs() const
{
    // Hidden means stopped, and this is the line that makes the bandwidth
    // arithmetic in the header come out. A window nobody is looking at does not
    // need a three-minute poll, and Canada's half of it cannot be revalidated.
    //
    // Unless the reader asked to be interrupted, which is §4.5's own exception
    // and the only thing that makes a notification worth having: a warning
    // that arrives only while the banner is already on screen is not a
    // warning. Fifteen minutes, the slowest rate that keeps it true.
    if (!m_visible) {
        const bool notifying = m_settings != nullptr && m_settings->alertNotifications();
        return notifying ? kPollHiddenNotifyingMs : 0;
    }

    if (metered())
        return kPollMeteredMs;

    return m_focused ? kPollActiveMs : kPollIdleMs;
}

void AlertsData::reschedule()
{
    const int interval = pollIntervalMs();

    if (interval <= 0 || !m_available) {
        m_poll.stop();
        return;
    }

    if (m_poll.interval() != interval)
        m_poll.setInterval(interval);
    if (!m_poll.isActive())
        m_poll.start();
}
