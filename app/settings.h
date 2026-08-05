// SPDX-FileCopyrightText: 2026 Jowi Aoun
// SPDX-License-Identifier: GPL-3.0-or-later
//
// Preferences that survive a restart.
//
// A QSettings behind Q_PROPERTYs, exposed to QML as a singleton, so a control
// binds to `Settings.temperatureUnit` and the write to disk is somebody else's
// problem. Every property carries a NOTIFY signal even where nothing reads it
// yet: a CONSTANT property that later becomes writable is a source-compatible
// change that silently breaks every binding made against it, and the day that
// happens is the day the setting stops appearing to work.
//
// ---- INI, on every platform -------------------------------------------------
// main() calls QSettings::setDefaultFormat(QSettings::IniFormat) before any
// QSettings exists. Without it Windows gets the registry, and that is three
// separate problems: a user cannot back up their preferences by copying a file,
// a support answer cannot say "open this and read line 4", and a portable
// unzip-and-run build writes into a machine-global hive it does not own. One
// code path, one file format, greppable everywhere.
//
// ---- units are per-quantity -------------------------------------------------
// Not a metric/imperial switch. docs/04-architecture.md §4.10 is explicit about
// this and it is the single most repeated complaint under every weather app's
// reviews: people want °C with mph, or inHg with mm, and an app that offers two
// bundles cannot express either. So there are five unit properties and there
// will never be a sixth that overrides them.
//
// ---- why the migration helper is here on day one ----------------------------
// The application and organisation names are baked into the settings path, and
// docs/07-packaging.md §7.2 says plainly that changing the app ID later breaks
// users' saved data. It does — QSettings would simply start reading a directory
// that does not exist, and every preference would silently revert to its
// default with no error anywhere. Writing the copy-forward now, while the table
// of superseded identities is empty and nothing can go wrong, makes a future
// rename a one-line change to that table instead of a data-loss event and a
// migration written under pressure.
#pragma once

#include <QObject>
#include <QQmlEngine>
#include <QScopedPointer>
#include <QString>

class QSettings;

// An organisation/application pair, which together are all QSettings needs to
// find a file. A superseded one is a place preferences might still be sitting.
struct SettingsIdentity
{
    QString organization;
    QString application;
};

class Settings : public QObject
{
    Q_OBJECT
    QML_ELEMENT
    QML_SINGLETON

    // "system" | "light" | "dark". Nothing reads it yet — the theme work is
    // W3 — but the key exists so the preference survives that work landing.
    Q_PROPERTY(QString appearance READ appearance WRITE setAppearance NOTIFY appearanceChanged)

    // Window geometry. Position is stored as well as size, because a
    // multi-monitor user who always puts Clima on the left screen wants it
    // there again; whether it can be honoured is a platform question — see
    // Main.qml, which restores the size and deliberately does not restore the
    // position on Wayland.
    Q_PROPERTY(int  windowWidth  READ windowWidth  WRITE setWindowWidth  NOTIFY windowGeometryChanged)
    Q_PROPERTY(int  windowHeight READ windowHeight WRITE setWindowHeight NOTIFY windowGeometryChanged)
    Q_PROPERTY(int  windowX      READ windowX      WRITE setWindowX      NOTIFY windowGeometryChanged)
    Q_PROPERTY(int  windowY      READ windowY      WRITE setWindowY      NOTIFY windowGeometryChanged)
    Q_PROPERTY(bool hasWindowSize READ hasWindowSize NOTIFY windowGeometryChanged)

    // Per quantity, never bundled. Values are the spellings libclima's unit
    // conversion will take, lowercase and unpunctuated so they are safe in an
    // INI file and in a URL query string.
    Q_PROPERTY(QString temperatureUnit   READ temperatureUnit   WRITE setTemperatureUnit   NOTIFY temperatureUnitChanged)
    Q_PROPERTY(QString windUnit          READ windUnit          WRITE setWindUnit          NOTIFY windUnitChanged)
    Q_PROPERTY(QString pressureUnit      READ pressureUnit      WRITE setPressureUnit      NOTIFY pressureUnitChanged)
    Q_PROPERTY(QString visibilityUnit    READ visibilityUnit    WRITE setVisibilityUnit    NOTIFY visibilityUnitChanged)
    Q_PROPERTY(QString precipitationUnit READ precipitationUnit WRITE setPrecipitationUnit NOTIFY precipitationUnitChanged)

public:
    ~Settings() override;

    static Settings *instance();
    static Settings *create(QQmlEngine *qmlEngine, QJSEngine *jsEngine);

    // Call from main() before anything constructs a QSettings — including this
    // class. Sets the INI default and copies a superseded config directory
    // forward if there is one.
    static void prepareStorage();

    // Identities Clima has used and no longer writes to, newest first. Empty
    // today, because Clima has only ever had one. A rename adds a line here and
    // nothing else changes.
    static QList<SettingsIdentity> supersededIdentities();

    // Copies the first superseded config directory that exists into the current
    // one, if and only if the current one is absent. Returns true if it copied.
    //
    // Takes the table as an argument rather than reading supersededIdentities()
    // directly so that it is testable without a rename having happened: a test
    // hands it two identities it created itself and checks the file arrived.
    static bool migrateConfigDirectory(const QList<SettingsIdentity> &superseded);

    // Where the preferences actually are. Q_INVOKABLE because "which file?" is
    // the first question of every support conversation and an About box should
    // be able to answer it.
    Q_INVOKABLE QString filePath() const;

    QString appearance() const;
    void    setAppearance(const QString &value);

    int  windowWidth() const;
    void setWindowWidth(int value);
    int  windowHeight() const;
    void setWindowHeight(int value);
    int  windowX() const;
    void setWindowX(int value);
    int  windowY() const;
    void setWindowY(int value);
    bool hasWindowSize() const;

    // Size and position in one write, because they are one fact and a window
    // remembered half-moved is worse than one not remembered at all.
    Q_INVOKABLE void saveWindowGeometry(int x, int y, int width, int height);

    QString temperatureUnit() const;
    void    setTemperatureUnit(const QString &value);
    QString windUnit() const;
    void    setWindUnit(const QString &value);
    QString pressureUnit() const;
    void    setPressureUnit(const QString &value);
    QString visibilityUnit() const;
    void    setVisibilityUnit(const QString &value);
    QString precipitationUnit() const;
    void    setPrecipitationUnit(const QString &value);

    // Opaque to this class. app/viewmodels/alertsdata.cpp owns the format.
    QStringList acknowledgedAlerts() const;
    void        setAcknowledgedAlerts(const QStringList &value);

Q_SIGNALS:
    void appearanceChanged();
    void windowGeometryChanged();
    void temperatureUnitChanged();
    void windUnitChanged();
    void pressureUnitChanged();
    void visibilityUnitChanged();
    void precipitationUnitChanged();
    void acknowledgedAlertsChanged();

private:
    // Private for the same reason AppOptions' is: QML picks a default
    // constructor over a create() factory without saying so, and the result is
    // two Settings objects — one QML writes to and one nothing reads.
    Settings();

    // Writes only when the value actually changed, so a binding that reassigns
    // its own value does not dirty the file, and emits only then too.
    bool store(const QString &key, const QVariant &value);
    QVariant load(const QString &key, const QVariant &fallback) const;

    QScopedPointer<QSettings> m_settings;
};
