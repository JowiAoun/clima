// SPDX-FileCopyrightText: 2026 Jowi Aoun
// SPDX-License-Identifier: GPL-3.0-or-later

#include "settings.h"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QSettings>
#include <QVariant>

namespace {

// Keys, in one place. A key spelled twice is a key that will eventually be
// spelled two ways, and the failure mode of that is a preference that saves and
// never loads — no error, no warning, just a setting that does not stick.
namespace key {
const auto appearance        = QStringLiteral("appearance");
// A group of its own rather than `appearance/dynamic`. QSettings will happily
// write a scalar `appearance` and a section `[appearance]` into the same INI,
// and the file that comes out is one nobody can read at a glance — which is the
// whole reason this app forces INI in the first place.
const auto dynamicBackground = QStringLiteral("background/dynamic");
const auto clockFormat       = QStringLiteral("time/format");
const auto windowWidth       = QStringLiteral("window/width");
const auto windowHeight      = QStringLiteral("window/height");
const auto windowX           = QStringLiteral("window/x");
const auto windowY           = QStringLiteral("window/y");
const auto temperatureUnit   = QStringLiteral("units/temperature");
const auto windUnit          = QStringLiteral("units/wind");
const auto pressureUnit      = QStringLiteral("units/pressure");
const auto visibilityUnit    = QStringLiteral("units/visibility");
const auto precipitationUnit = QStringLiteral("units/precipitation");
const auto acknowledgedAlerts = QStringLiteral("alerts/acknowledged");
} // namespace key

// The file QSettings would use for a given identity. Constructing a QSettings
// does not create anything on disk — it only computes a path — so this is safe
// to call for an identity that has never existed.
QString configFileFor(const SettingsIdentity &identity)
{
    const QSettings probe(QSettings::IniFormat, QSettings::UserScope,
                          identity.organization, identity.application);
    return probe.fileName();
}

// And the directory that file sits in.
//
// ---- these are two questions, not one ---------------------------------------
//
// The path QSettings computes is <config>/<organisation>/<application>.ini, so
// the DIRECTORY is named after the organisation alone and every application
// under one organisation shares it. That is the whole reason the migration
// below asks about the file as well:
//
//   * a directory that exists proves nothing about whether THIS application has
//     ever been run, only that something under the same organisation has
//
//   * two identities differing only in application name resolve to the same
//     directory, so a migration keyed on the directory sees no move to make and
//     skips a rename that renamed the file out from under the reader
//
// Both were live: this function used to be the only one, and an application
// rename — the likelier half of a rebrand, since the organisation is a domain —
// silently reverted every preference to its default with the old file sitting
// unread in the same directory.
QString configDirectoryFor(const SettingsIdentity &identity)
{
    return QFileInfo(configFileFor(identity)).absolutePath();
}

// Recursive copy that refuses to overwrite. The migration only ever runs into
// an absent destination, so an existing file means something raced us and the
// right answer is to leave it alone rather than to win.
bool copyTree(const QString &from, const QString &to)
{
    QDir source(from);
    if (!source.exists())
        return false;
    if (!QDir().mkpath(to))
        return false;

    bool ok = true;
    const QFileInfoList entries =
        source.entryInfoList(QDir::Files | QDir::Dirs | QDir::NoDotAndDotDot | QDir::Hidden);
    for (const QFileInfo &entry : entries) {
        const QString target = to + QLatin1Char('/') + entry.fileName();
        if (entry.isDir())
            ok = copyTree(entry.absoluteFilePath(), target) && ok;
        else if (!QFile::exists(target))
            ok = QFile::copy(entry.absoluteFilePath(), target) && ok;
    }
    return ok;
}

Settings *g_instance = nullptr;

} // namespace

Settings::Settings()
    : m_settings(new QSettings)
{
}

Settings::~Settings() = default;

Settings *Settings::instance()
{
    if (g_instance == nullptr)
        g_instance = new Settings;
    return g_instance;
}

Settings *Settings::create(QQmlEngine *qmlEngine, QJSEngine *jsEngine)
{
    Q_UNUSED(qmlEngine)
    Q_UNUSED(jsEngine)
    Settings *settings = instance();
    QJSEngine::setObjectOwnership(settings, QJSEngine::CppOwnership);
    return settings;
}

void Settings::prepareStorage()
{
    // Before any QSettings is constructed anywhere, this one included. A
    // QSettings built before this line has already chosen its format and will
    // keep it — on Windows that is the registry, and the app then reads one
    // store and writes another.
    QSettings::setDefaultFormat(QSettings::IniFormat);

    migrateConfigDirectory(supersededIdentities());
}

QList<SettingsIdentity> Settings::supersededIdentities()
{
    // Empty, and correct: Clima has written preferences under exactly one
    // organisation and application name, the one main() sets today. This is the
    // list a rename appends to — `{ QStringLiteral("Clima"), QStringLiteral("clima") }`
    // would be the entry if the identity moved tomorrow — and the reason the
    // machinery below exists before there is anything for it to do is that a
    // migration written after the rename has already lost the data it was
    // supposed to carry.
    return {};
}

bool Settings::migrateConfigDirectory(const QList<SettingsIdentity> &superseded)
{
    if (superseded.isEmpty())
        return false;

    const SettingsIdentity self{ QCoreApplication::organizationName(),
                                 QCoreApplication::applicationName() };
    const QString currentFile = configFileFor(self);
    const QString currentDir  = configDirectoryFor(self);

    // A settings FILE that exists is a user who has already run this version.
    // Never copy over it: their current preferences win over an older copy,
    // always, and getting this branch backwards would revert a user's settings
    // on every single launch.
    //
    // The file and not the directory, because the directory is named after the
    // organisation and is shared — see configDirectoryFor(). Asking about the
    // directory meant a rename within one organisation was refused as "already
    // migrated" on the strength of a file belonging to something else.
    if (QFile::exists(currentFile))
        return false;

    for (const SettingsIdentity &identity : superseded) {
        const QString legacyFile = configFileFor(identity);
        const QString legacyDir  = configDirectoryFor(identity);

        if (legacyFile == currentFile || !QFile::exists(legacyFile))
            continue;

        // Everything beside the settings file comes forward too — a per-widget
        // layout, a cached place list — but only when it is somewhere else. An
        // application rename leaves the directory where it was, and copying a
        // directory onto itself is not a thing to attempt.
        bool ok = true;
        if (legacyDir != currentDir)
            ok = copyTree(legacyDir, currentDir);
        else
            ok = QDir().mkpath(currentDir);

        // And then the settings file itself, under the name THIS build will look
        // for. This is the half a directory copy cannot do: after a rename the
        // bytes are already in the right directory under the wrong filename, and
        // QSettings reads a filename.
        if (ok && !QFile::exists(currentFile))
            ok = QFile::copy(legacyFile, currentFile);

        if (ok) {
            qInfo("settings: carried preferences forward from %s", qPrintable(legacyFile));
            return true;
        }
        qWarning("settings: could not copy %s to %s", qPrintable(legacyFile),
                 qPrintable(currentFile));
        return false;
    }
    return false;
}

QString Settings::filePath() const
{
    return m_settings->fileName();
}

bool Settings::store(const QString &key, const QVariant &value)
{
    if (m_settings->value(key) == value)
        return false;
    m_settings->setValue(key, value);
    return true;
}

QVariant Settings::load(const QString &key, const QVariant &fallback) const
{
    return m_settings->value(key, fallback);
}

// ---- appearance -------------------------------------------------------------

QString Settings::appearance() const
{
    return load(key::appearance, QStringLiteral("system")).toString();
}

void Settings::setAppearance(const QString &value)
{
    if (store(key::appearance, value))
        Q_EMIT appearanceChanged();
}

bool Settings::dynamicBackground() const
{
    return load(key::dynamicBackground, true).toBool();
}

void Settings::setDynamicBackground(bool value)
{
    if (store(key::dynamicBackground, value))
        Q_EMIT dynamicBackgroundChanged();
}

// ---- the clock --------------------------------------------------------------

QString Settings::clockFormat() const
{
    // Anything that is not "24h" reads as "12h", including a hand-edited value
    // and a key written by a future version that grew a third spelling. A clock
    // is not a setting that may fail closed: an unrecognised value has to still
    // put a time on the screen.
    return load(key::clockFormat, QStringLiteral("12h")).toString() == QLatin1String("24h")
               ? QStringLiteral("24h")
               : QStringLiteral("12h");
}

void Settings::setClockFormat(const QString &value)
{
    if (store(key::clockFormat, value == QLatin1String("24h") ? QStringLiteral("24h")
                                                              : QStringLiteral("12h")))
        Q_EMIT clockFormatChanged();
}

// ---- window geometry --------------------------------------------------------

int Settings::windowWidth() const  { return load(key::windowWidth, 0).toInt(); }
int Settings::windowHeight() const { return load(key::windowHeight, 0).toInt(); }
int Settings::windowX() const      { return load(key::windowX, 0).toInt(); }
int Settings::windowY() const      { return load(key::windowY, 0).toInt(); }

// Zero is not a window, so it doubles as "never saved" and there is no second
// key to keep in step with the first four.
bool Settings::hasWindowSize() const
{
    return windowWidth() > 0 && windowHeight() > 0;
}

void Settings::setWindowWidth(int value)
{
    if (store(key::windowWidth, value))
        Q_EMIT windowGeometryChanged();
}

void Settings::setWindowHeight(int value)
{
    if (store(key::windowHeight, value))
        Q_EMIT windowGeometryChanged();
}

void Settings::setWindowX(int value)
{
    if (store(key::windowX, value))
        Q_EMIT windowGeometryChanged();
}

void Settings::setWindowY(int value)
{
    if (store(key::windowY, value))
        Q_EMIT windowGeometryChanged();
}

void Settings::saveWindowGeometry(int x, int y, int width, int height)
{
    // A window that is not on screen yet, or is minimised, reports a size that
    // would come back as a window the user cannot see. Refuse it rather than
    // remember it.
    if (width <= 0 || height <= 0)
        return;

    bool changed = store(key::windowX, x);
    changed = store(key::windowY, y) || changed;
    changed = store(key::windowWidth, width) || changed;
    changed = store(key::windowHeight, height) || changed;

    // Explicitly, because this is usually the last thing that happens before
    // the process exits and QSettings' own flush is tied to its destructor
    // running — which a crash, a SIGTERM or a compositor logout does not
    // guarantee.
    if (changed) {
        m_settings->sync();
        Q_EMIT windowGeometryChanged();
    }
}

// ---- units ------------------------------------------------------------------
//
// The defaults are SI where SI is unambiguous and the locale-independent
// spelling everywhere. They are not a "metric preset": each one is chosen on
// its own, which is the whole point of storing five keys.

QString Settings::temperatureUnit() const
{
    return load(key::temperatureUnit, QStringLiteral("celsius")).toString();
}

void Settings::setTemperatureUnit(const QString &value)
{
    if (store(key::temperatureUnit, value))
        Q_EMIT temperatureUnitChanged();
}

QString Settings::windUnit() const
{
    return load(key::windUnit, QStringLiteral("kmh")).toString();
}

void Settings::setWindUnit(const QString &value)
{
    if (store(key::windUnit, value))
        Q_EMIT windUnitChanged();
}

QString Settings::pressureUnit() const
{
    return load(key::pressureUnit, QStringLiteral("hpa")).toString();
}

void Settings::setPressureUnit(const QString &value)
{
    if (store(key::pressureUnit, value))
        Q_EMIT pressureUnitChanged();
}

QString Settings::visibilityUnit() const
{
    return load(key::visibilityUnit, QStringLiteral("km")).toString();
}

void Settings::setVisibilityUnit(const QString &value)
{
    if (store(key::visibilityUnit, value))
        Q_EMIT visibilityUnitChanged();
}

QString Settings::precipitationUnit() const
{
    return load(key::precipitationUnit, QStringLiteral("mm")).toString();
}

void Settings::setPrecipitationUnit(const QString &value)
{
    if (store(key::precipitationUnit, value))
        Q_EMIT precipitationUnitChanged();
}

// ---- the dismissed alerts --------------------------------------------------
//
// The one setting this class stores without understanding. Each entry is a
// hazard key, the severity it was dismissed at and when it stops mattering,
// packed by app/viewmodels/alertsdata.cpp — which owns the format, prunes the
// expired entries and is the only thing that reads them back.
//
// Kept here anyway rather than in a file of its own, because the reason it
// persists at all is that it is a *preference*: without it, a multi-day heat
// warning re-raises its banner on every launch, which is how a person learns to
// dismiss a banner without reading it.
QStringList Settings::acknowledgedAlerts() const
{
    return load(key::acknowledgedAlerts, QStringList()).toStringList();
}

void Settings::setAcknowledgedAlerts(const QStringList &value)
{
    if (store(key::acknowledgedAlerts, value))
        Q_EMIT acknowledgedAlertsChanged();
}
