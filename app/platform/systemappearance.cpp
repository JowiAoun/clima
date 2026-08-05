// SPDX-FileCopyrightText: 2026 Jowi Aoun
// SPDX-License-Identifier: GPL-3.0-or-later
#include "systemappearance.h"

#include <QGuiApplication>
#include <QStyleHints>

#ifdef CLIMA_HAVE_DBUS
#include <QDBusConnection>
#include <QDBusMessage>
#include <QDBusVariant>
#include <QVariant>
#endif

namespace {

#ifdef CLIMA_HAVE_DBUS
constexpr auto portalService   = "org.freedesktop.portal.Desktop";
constexpr auto portalPath      = "/org/freedesktop/portal/desktop";
constexpr auto portalInterface = "org.freedesktop.portal.Settings";

// The two namespaces we read, and they are not the same kind of thing.
// `org.freedesktop.appearance` is cross-desktop and standardised. Reduced
// motion has no such key: the closest thing anyone implements is GNOME's
// `enable-animations`, which the portal exposes verbatim, so on KDE or a bare
// wlroots session this simply goes unanswered and animations stay on. That is
// the honest outcome — better than inventing a default from a key nobody set.
constexpr auto appearanceNamespace = "org.freedesktop.appearance";
constexpr auto colorSchemeKey      = "color-scheme";
constexpr auto gnomeNamespace      = "org.gnome.desktop.interface";
constexpr auto animationsKey       = "enable-animations";

// 0 = no preference, 1 = prefer dark, 2 = prefer light. Anything else is a
// portal newer than this code, and the honest reading of an unknown value is
// "no preference" rather than a guess.
QString schemeFromPortalValue(uint value)
{
    if (value == 2)
        return QStringLiteral("light");
    if (value == 1)
        return QStringLiteral("dark");
    return {};
}

// ReadOne landed in portal Settings v2 and hands back the value directly.
// Read is the v1 spelling and wraps it twice — a QDBusVariant inside a
// QDBusVariant — which is the detail that makes a naive port silently read an
// empty QVariant and conclude the desktop is dark.
QVariant readPortalSetting(const QString &nameSpace, const QString &key, bool *ok)
{
    *ok = false;

    auto call = [&](const char *method) {
        QDBusMessage request = QDBusMessage::createMethodCall(
            QLatin1String(portalService), QLatin1String(portalPath),
            QLatin1String(portalInterface), QLatin1String(method));
        request << nameSpace << key;
        return QDBusConnection::sessionBus().call(request, QDBus::Block, 500);
    };

    QDBusMessage reply = call("ReadOne");
    if (reply.type() == QDBusMessage::ReplyMessage && !reply.arguments().isEmpty()) {
        *ok = true;
        return reply.arguments().constFirst().value<QDBusVariant>().variant();
    }

    reply = call("Read");
    if (reply.type() == QDBusMessage::ReplyMessage && !reply.arguments().isEmpty()) {
        *ok = true;
        // Unwrap twice; see above.
        QVariant outer = reply.arguments().constFirst().value<QDBusVariant>().variant();
        return outer.value<QDBusVariant>().variant();
    }

    return {};
}
#endif // CLIMA_HAVE_DBUS

} // namespace

SystemAppearance::SystemAppearance(QObject *parent)
    : QObject(parent)
{
    // Style hints first, portal second, so that the portal's answer overwrites
    // the weaker source rather than racing it. Both are synchronous here: this
    // is constructed before the QML engine loads, and a window that paints one
    // scheme and then repaints in another is a window a screenshot can catch
    // halfway.
    readFromStyleHints();
    connectToPortal();
}

QString SystemAppearance::colorScheme() const { return m_colorScheme; }
bool    SystemAppearance::reduceMotion() const { return m_reduceMotion; }
bool    SystemAppearance::available() const { return m_available; }

void SystemAppearance::readFromStyleHints()
{
    const QStyleHints *hints = QGuiApplication::styleHints();
    if (hints == nullptr)
        return;

    // Qt::ColorScheme::Unknown means the platform did not say, which is not the
    // same as saying dark. Leave the floor in place and leave `available`
    // false, so the Me page can be honest about following nothing.
    switch (hints->colorScheme()) {
    case Qt::ColorScheme::Dark:
        applyColorScheme(QStringLiteral("dark"), false);
        break;
    case Qt::ColorScheme::Light:
        applyColorScheme(QStringLiteral("light"), false);
        break;
    default:
        break;
    }

    connect(hints, &QStyleHints::colorSchemeChanged, this, [this](Qt::ColorScheme scheme) {
        // The portal outranks this. Once it has answered, a style-hint change
        // is Qt reporting the same news through a second channel, and letting
        // it through would make the scheme flap between two spellings of it.
        if (m_portalAnswered)
            return;
        if (scheme == Qt::ColorScheme::Dark)
            applyColorScheme(QStringLiteral("dark"), false);
        else if (scheme == Qt::ColorScheme::Light)
            applyColorScheme(QStringLiteral("light"), false);
    });
}

void SystemAppearance::connectToPortal()
{
#ifdef CLIMA_HAVE_DBUS
    if (!QDBusConnection::sessionBus().isConnected())
        return;

    bool ok = false;
    const QVariant scheme = readPortalSetting(QLatin1String(appearanceNamespace),
                                              QLatin1String(colorSchemeKey), &ok);
    if (ok) {
        const QString resolved = schemeFromPortalValue(scheme.toUInt());
        if (!resolved.isEmpty())
            applyColorScheme(resolved, true);
    }

    bool animationsOk = false;
    const QVariant animations = readPortalSetting(QLatin1String(gnomeNamespace),
                                                  QLatin1String(animationsKey), &animationsOk);
    if (animationsOk)
        applyReduceMotion(!animations.toBool());

    // Subscribed whether or not either read succeeded. A portal that starts
    // after the app does — or a key that only exists once somebody changes it
    // — still arrives here, and that is the whole reason to prefer this source
    // over polling one.
    QDBusConnection::sessionBus().connect(
        QLatin1String(portalService), QLatin1String(portalPath),
        QLatin1String(portalInterface), QStringLiteral("SettingChanged"),
        this, SLOT(onSettingChanged(QString, QString, QDBusVariant)));
#endif
}

#ifdef CLIMA_HAVE_DBUS
void SystemAppearance::onSettingChanged(const QString &nameSpace, const QString &key,
                                        const QDBusVariant &value)
{
    if (nameSpace == QLatin1String(appearanceNamespace)
        && key == QLatin1String(colorSchemeKey)) {
        const QString resolved = schemeFromPortalValue(value.variant().toUInt());
        if (!resolved.isEmpty())
            applyColorScheme(resolved, true);
        return;
    }

    if (nameSpace == QLatin1String(gnomeNamespace) && key == QLatin1String(animationsKey))
        applyReduceMotion(!value.variant().toBool());
}
#endif

void SystemAppearance::applyColorScheme(const QString &scheme, bool fromPortal)
{
    if (fromPortal)
        m_portalAnswered = true;

    if (!m_available) {
        m_available = true;
        Q_EMIT availableChanged();
    }

    if (m_colorScheme == scheme)
        return;

    m_colorScheme = scheme;
    Q_EMIT colorSchemeChanged();
}

void SystemAppearance::applyReduceMotion(bool value)
{
    if (m_reduceMotion == value)
        return;

    m_reduceMotion = value;
    Q_EMIT reduceMotionChanged();
}
