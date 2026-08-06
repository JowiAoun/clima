// SPDX-FileCopyrightText: 2026 Jowi Aoun
// SPDX-License-Identifier: GPL-3.0-or-later
//
// The tables a tile needs, as something QML can call.
//
//     Text { text: Wx.uvBand(feed.snapshot.current.uvIndex) }
//     WeatherGlyph { kind: Wx.glyphKind(code, isDay) }
//
// Every function here forwards to libclima — the WMO code tables in
// domain/weathercode.h and the published bands in domain/scales.h — and adds
// exactly one thing of its own: it takes a QVariant rather than a double, so
// that a null on the wire stays a null instead of becoming a plausible zero.
//
// ============================================================================
// WHY THIS IS NOT `Metrics` OR `ConditionsData`
//
// Those are in app/, they are shaped around the app's cards, and one of them
// (`Metrics`) reaches the engine. A widget host must not link the engine's
// network or cache code — widgets/CMakeLists.txt makes that a check on the
// built binary rather than a rule in a document — so the tile side gets its
// own thin object over the same domain functions. The tables themselves are
// not copied, and that is the whole reason libclima/domain/scales.h exists.
//
// ============================================================================
// NULL, AND WHY EVERY ARGUMENT IS A QVariant
//
// `feed.snapshot.current.uvIndex` is a JavaScript value that may be a number,
// `null` (the provider carries no reading), or `undefined` (the field mask
// never asked for it). Declaring these as `double` would let QML coerce all
// three, and two of them would arrive as 0 — which `uvBand` would dutifully
// call "Low". A widget saying "UV Low" for a place with no UV product is the
// null-drawn-as-zero mistake with a reassuring word on it.
//
// So: QVariant in, and an empty string out for anything that is not a number.
// `wire.js` is the QML-side half of the same rule.

#pragma once

#include <QObject>
#include <QQmlEngine>
#include <QString>
#include <QVariant>

class Wx : public QObject
{
    Q_OBJECT
    QML_ELEMENT
    QML_SINGLETON

public:
    static Wx *create(QQmlEngine *qmlEngine, QJSEngine *jsEngine);

    // ---- WMO codes ---------------------------------------------------------

    // The string WeatherGlyph.qml switches on, already degraded to something it
    // can actually draw. Empty when there is no code, which the glyph renders
    // as nothing — correct, because "we do not know the sky" is not "clear".
    Q_INVOKABLE [[nodiscard]] QString glyphKind(const QVariant &code, const QVariant &isDay) const;

    // One short localised phrase: "Light rain", "Partly cloudy".
    Q_INVOKABLE [[nodiscard]] QString conditionText(const QVariant &code,
                                                    const QVariant &isDay) const;

    // One of precip.js's six names, or "" for an hour with no precipitation.
    Q_INVOKABLE [[nodiscard]] QString precipType(const QVariant &code) const;

    // ---- published scales --------------------------------------------------

    Q_INVOKABLE [[nodiscard]] QString uvBand(const QVariant &index) const;
    Q_INVOKABLE [[nodiscard]] QString aqiBand(const QVariant &index) const;
    Q_INVOKABLE [[nodiscard]] QString compass(const QVariant &degrees) const;
    Q_INVOKABLE [[nodiscard]] QString beaufort(const QVariant &kmh) const;
    Q_INVOKABLE [[nodiscard]] QString pollutant(const QVariant &id) const;

    // ---- instants ----------------------------------------------------------
    //
    // Every timestamp on the wire is ISO 8601 *already moved into the place's
    // own zone*, offset and all (libclima/wire/snapshot.cpp). So these read the
    // wall clock out of the string rather than converting anything: a tile
    // showing Toronto shows Toronto's afternoon whatever zone the desktop is
    // in, and there is no second conversion here to get backwards.

    // "3:00 PM", in the reader's locale.
    Q_INVOKABLE [[nodiscard]] QString clockTime(const QVariant &iso) const;

    // The same instant split where SkyArc.qml wants it: "3:00" and "PM". Two
    // calls rather than one string, because the component sets them in
    // different type sizes and a widget must not be the one place that glues
    // them back together with a space.
    Q_INVOKABLE [[nodiscard]] QString clockLabel(const QVariant &iso) const;
    Q_INVOKABLE [[nodiscard]] QString clockSuffix(const QVariant &iso) const;

    // "3 PM" — the whole instant in the width an hourly column has.
    Q_INVOKABLE [[nodiscard]] QString hourLabel(const QVariant &iso) const;

    // "13 h 42 min" between two instants, or empty if either is absent.
    Q_INVOKABLE [[nodiscard]] QString spanBetween(const QVariant &fromIso,
                                                  const QVariant &toIso) const;

    // Minutes past midnight *right now*, in the zone the given instant carries.
    //
    // Not `minutesFromMidnight(snapshot.generatedAt)`: that is the daemon's
    // clock at the last publish, so a sun mark would step forward every five
    // minutes and freeze completely when the daemon stopped. This reads the
    // reader's own clock and moves it into the place's offset, so the mark is
    // live even when the data behind it is an hour old — which is exactly the
    // state the tiles are designed to survive.
    //
    // One caveat, written down because it is invisible: the offset comes from
    // the snapshot, so during the hour after a DST change in the *place* — but
    // before the next snapshot arrives — this is out by an hour. It corrects
    // itself on the next publish, and the alternative is carrying a full zone
    // database into a process whose job is to draw four hundred pixels.
    Q_INVOKABLE [[nodiscard]] int nowMinutesInZoneOf(const QVariant &iso) const;

    // "Mon", "Tue" — the short day name, for a daily strip.
    Q_INVOKABLE [[nodiscard]] QString shortDay(const QVariant &iso) const;

    // Minutes from that day's local midnight, which is what SkyArc.qml's
    // riseMin/setMin/nowMin want. -1 when there is no instant.
    Q_INVOKABLE [[nodiscard]] int minutesFromMidnight(const QVariant &iso) const;

    // ---- age ---------------------------------------------------------------

    // "just now" / "12 min ago" / "3 h ago" / "yesterday". The one string every
    // tile footer prints, so there is one wording of it.
    //
    // `minutes` < 0 means there is nothing to age, and the answer is empty —
    // not "just now", which would be a claim about a reading that does not
    // exist.
    Q_INVOKABLE [[nodiscard]] QString ago(int minutes) const;

private:
    // Private, and that is load-bearing rather than tidy: a public
    // `Wx(QObject *parent = nullptr)` makes this type default-constructible,
    // and Qt's singletonConstructionMode() checks default-constructible BEFORE
    // it looks for create() — so QML would build a second instance and never
    // call the factory. widgets/daemonlink.h has the full argument and what it
    // looked like when it happened.
    explicit Wx(QObject *parent = nullptr);
};
