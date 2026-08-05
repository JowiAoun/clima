// SPDX-FileCopyrightText: 2026 Jowi Aoun
// SPDX-License-Identifier: GPL-3.0-or-later
//
// What the desktop would like the app to look like.
//
// Two questions, asked of the session rather than of the user: is this a dark
// desktop or a light one, and has the person asked for less movement. Neither
// is a setting this app owns — they are the desktop's, and an app that ignores
// them is the one window on the screen that did not get the message when
// somebody turned the lights off.
//
// ---- where the answer comes from --------------------------------------------
//
// Three sources, tried in order, and the order is the design:
//
//   1. The XDG desktop portal, `org.freedesktop.portal.Settings`. This is the
//      only one that works from inside a Flatpak, which is the primary Linux
//      channel (docs/07 §7.2), and it is also the only one that pushes: it
//      emits SettingChanged, so turning the desktop dark repaints this app
//      without it having to poll or to be restarted.
//
//   2. QStyleHints::colorScheme(), Qt 6.5+ and so inside our 6.8 floor. This
//      covers Windows, and Linux sessions with no portal running. It answers
//      the colour question only; Qt has no reduced-motion hint on any platform.
//
//   3. Dark. Not "no preference" — dark. It is where this design system
//      started, it is what every committed screenshot shows, and a fallback
//      that means "I could not tell" still has to paint something.
//
// The portal's `color-scheme` is a uint with three values, and the middle one
// is the trap: 0 is *no preference*, 1 is prefer-dark, 2 is prefer-light. A
// reader that treats it as a boolean gets light desktops right and hands "no
// preference" back as dark by accident rather than by decision.
//
// ---- what this deliberately does not read -----------------------------------
//
// `accent-color`, which the same portal namespace offers. Accent in this
// palette is structure — the selected pill, the nav pill, the wash behind the
// now row — and `accent.ink` is tuned against it to a measured contrast ratio.
// Adopting a system accent would break that pair on whichever desktop chose an
// unlucky hue, and the light theme has already shown that the fill and the ink
// on it cannot be chosen independently. See the note in themelight.js.
#pragma once

#include <QObject>
#include <QQmlEngine>
#include <QString>

#ifdef CLIMA_HAVE_DBUS
// The whole header, not a forward declaration. QDBusVariant appears in a slot
// signature below, and moc emits a metatype for every parameter of a slot — so
// an incomplete type here fails the build inside qmetatype.h with "Meta Types
// must be fully defined", a long way from the line that caused it.
#include <QDBusVariant>
#endif

class SystemAppearance : public QObject
{
    Q_OBJECT
    QML_ELEMENT
    QML_SINGLETON

    // "dark" or "light", never empty. The same two strings Theme.scheme takes,
    // so the binding that joins them is an assignment and not a translation.
    Q_PROPERTY(QString colorScheme READ colorScheme NOTIFY colorSchemeChanged)

    // True when the desktop has asked for less movement. Defaults false: no
    // answer means no request, and an app that stopped animating because it
    // could not reach a settings daemon would be a puzzling thing to debug.
    Q_PROPERTY(bool reduceMotion READ reduceMotion NOTIFY reduceMotionChanged)

    // Whether anything actually answered. The Me page uses it to say "System"
    // rather than pretending to follow a preference it never received.
    Q_PROPERTY(bool available READ available NOTIFY availableChanged)

public:
    explicit SystemAppearance(QObject *parent = nullptr);

    QString colorScheme() const;
    bool    reduceMotion() const;
    bool    available() const;

Q_SIGNALS:
    void colorSchemeChanged();
    void reduceMotionChanged();
    void availableChanged();

#ifdef CLIMA_HAVE_DBUS
    // A real slot, declared to moc, and it has to be. QDBusConnection::connect
    // takes a signature through the old SLOT() macro and resolves it at run
    // time against the meta-object — so an ordinary private method compiles,
    // links, and then fails at startup with
    //
    //   qt.dbus.integration: Could not connect "org.freedesktop.portal.Settings"
    //   to onSettingChanged(QString,QString,QDBusVariant)
    //
    // on stderr and nothing else. The initial read still works, so the app
    // starts in the right scheme and simply never hears about a change: a bug
    // that looks exactly like the feature working until somebody toggles their
    // desktop and waits.
private Q_SLOTS:
    void onSettingChanged(const QString &nameSpace, const QString &key,
                          const QDBusVariant &value);
#endif

private:
    void connectToPortal();
    void readFromStyleHints();
    void applyColorScheme(const QString &scheme, bool fromPortal);
    void applyReduceMotion(bool value);

    QString m_colorScheme = QStringLiteral("dark");
    bool    m_reduceMotion = false;
    bool    m_available = false;
    bool    m_portalAnswered = false;
};
