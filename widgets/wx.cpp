// SPDX-FileCopyrightText: 2026 Jowi Aoun
// SPDX-License-Identifier: GPL-3.0-or-later

#include "wx.h"

#include "widgetclock.h"

#include "libclima/domain/scales.h"
#include "libclima/domain/weathercode.h"

#include <QDateTime>
#include <QLocale>

#include <optional>

using namespace clima;

namespace {

// The one guard this whole file exists for. `undefined`, `null` and a string
// that is not a number all come back as nullopt; only an actual number gets
// through.
std::optional<double> numberOf(const QVariant &value)
{
    if (!value.isValid() || value.isNull())
        return std::nullopt;
    bool         ok = false;
    const double d  = value.toDouble(&ok);
    return ok ? std::optional<double>(d) : std::nullopt;
}

std::optional<int> codeOf(const QVariant &value)
{
    const std::optional<double> number = numberOf(value);
    return number ? std::optional<int>(int(*number)) : std::nullopt;
}

// isDay is `1`, `0` or null on the wire. A null is not "night": it is a
// provider that does not carry the flag, and the least wrong default for a
// glyph is day, because a moon over a sunny afternoon is a more obviously
// broken picture than a sun over a clear night.
bool dayOf(const QVariant &value)
{
    const std::optional<double> number = numberOf(value);
    return number ? (*number != 0.0) : true;
}

QDateTime instantOf(const QVariant &value)
{
    if (!value.isValid() || value.isNull())
        return {};
    return QDateTime::fromString(value.toString(), Qt::ISODate);
}

} // namespace

Wx::Wx(QObject *parent)
    : QObject(parent)
{
}

Wx *Wx::create(QQmlEngine *, QJSEngine *)
{
    static Wx *wx = new Wx;
    QQmlEngine::setObjectOwnership(wx, QQmlEngine::CppOwnership);
    return wx;
}

// ---- WMO codes --------------------------------------------------------------

QString Wx::glyphKind(const QVariant &code, const QVariant &isDay) const
{
    const std::optional<int> wmo = codeOf(code);
    if (!wmo)
        return {};
    return conditionKindName(drawableToday(conditionFor(*wmo, dayOf(isDay))));
}

QString Wx::conditionText(const QVariant &code, const QVariant &isDay) const
{
    const std::optional<int> wmo = codeOf(code);
    if (!wmo)
        return {};
    return clima::conditionText(*wmo, dayOf(isDay));
}

QString Wx::precipType(const QVariant &code) const
{
    const std::optional<int> wmo = codeOf(code);
    if (!wmo)
        return {};
    const PrecipitationType type = precipitationTypeFor(*wmo);
    return type == PrecipitationType::None ? QString() : precipitationTypeName(type);
}

// ---- published scales -------------------------------------------------------

QString Wx::uvBand(const QVariant &index) const
{
    const std::optional<double> value = numberOf(index);
    return value ? scales::uvBand(*value) : QString();
}

QString Wx::aqiBand(const QVariant &index) const
{
    const std::optional<double> value = numberOf(index);
    return value ? scales::aqiBand(*value) : QString();
}

QString Wx::compass(const QVariant &degrees) const
{
    const std::optional<double> value = numberOf(degrees);
    return value ? scales::compassPoint(*value) : QString();
}

QString Wx::beaufort(const QVariant &kmh) const
{
    const std::optional<double> value = numberOf(kmh);
    return value ? scales::beaufortName(scales::beaufortForce(*value)) : QString();
}

QString Wx::pollutant(const QVariant &id) const
{
    if (!id.isValid() || id.isNull())
        return {};
    const QString text = id.toString();
    return text.isEmpty() ? QString() : scales::pollutantLabel(text);
}

// ---- instants ---------------------------------------------------------------

QString Wx::clockTime(const QVariant &iso) const
{
    const QDateTime instant = instantOf(iso);
    if (!instant.isValid())
        return {};
    return QLocale().toString(instant.time(), QStringLiteral("h:mm AP"));
}

// "8:42", not "20:42".
//
// QLocale's "h" is a 24-hour hour unless the format string also carries AP, and
// the suffix is a separate field here — so asking for "h:mm" and appending "PM"
// produced "20:37 PM" under every sunset in the first render of the sun tile.
// app/viewmodels/conditionsdata.cpp hit this and wrote the same two lines; the
// arithmetic is shorter than the format string that gets it right.
QString Wx::clockLabel(const QVariant &iso) const
{
    const QDateTime instant = instantOf(iso);
    if (!instant.isValid())
        return {};
    const QTime time = instant.time();
    const int   hour = time.hour() % 12 == 0 ? 12 : time.hour() % 12;
    return QStringLiteral("%1:%2").arg(hour).arg(time.minute(), 2, 10, QLatin1Char('0'));
}

QString Wx::clockSuffix(const QVariant &iso) const
{
    const QDateTime instant = instantOf(iso);
    if (!instant.isValid())
        return {};
    return instant.time().hour() < 12 ? tr("AM") : tr("PM");
}

QString Wx::hourLabel(const QVariant &iso) const
{
    const QDateTime instant = instantOf(iso);
    if (!instant.isValid())
        return {};

    // "3 PM" and not "3:00 PM". An hourly strip gives each column about thirty
    // pixels, and ":00" is four of them spent restating that an hourly point
    // falls on the hour. The minutes come back the moment they are not always
    // zero, which for an hourly forecast is never.
    const QTime time = instant.time();
    const int   hour = time.hour() % 12 == 0 ? 12 : time.hour() % 12;
    return QStringLiteral("%1 %2").arg(hour).arg(time.hour() < 12 ? tr("AM") : tr("PM"));
}

QString Wx::spanBetween(const QVariant &fromIso, const QVariant &toIso) const
{
    const QDateTime from = instantOf(fromIso);
    const QDateTime to   = instantOf(toIso);
    if (!from.isValid() || !to.isValid())
        return {};

    qint64 minutes = from.secsTo(to) / 60;
    if (minutes < 0)
        minutes += 24 * 60; // a body that sets the next day
    return tr("%1 h %2 min").arg(minutes / 60).arg(minutes % 60);
}

int Wx::nowMinutesInZoneOf(const QVariant &iso) const
{
    const QDateTime reference = instantOf(iso);
    if (!reference.isValid())
        return -1;

    const QDateTime here = clima::widgets::now().toOffsetFromUtc(reference.offsetFromUtc());
    return here.time().hour() * 60 + here.time().minute();
}

QString Wx::shortDay(const QVariant &iso) const
{
    // A daily entry is a bare date ("2026-08-06"); an hourly one is a full
    // instant. QDateTime::fromString(Qt::ISODate) parses only the second, so
    // fall back to a date before giving up.
    const QDateTime instant = instantOf(iso);
    if (instant.isValid())
        return QLocale().toString(instant.date(), QStringLiteral("ddd"));

    if (!iso.isValid() || iso.isNull())
        return {};
    const QDate date = QDate::fromString(iso.toString(), Qt::ISODate);
    return date.isValid() ? QLocale().toString(date, QStringLiteral("ddd")) : QString();
}

int Wx::minutesFromMidnight(const QVariant &iso) const
{
    const QDateTime instant = instantOf(iso);
    if (!instant.isValid())
        return -1;

    // The wall clock in the string, not a conversion. The wire already moved
    // this into the place's zone; converting again here would move a Toronto
    // sunrise into the reader's afternoon.
    const QTime time = instant.time();
    return time.hour() * 60 + time.minute();
}

// ---- age --------------------------------------------------------------------

QString Wx::ago(int minutes) const
{
    if (minutes < 0)
        return {};
    if (minutes < 1)
        return tr("just now");
    if (minutes < 60)
        return tr("%n min ago", nullptr, minutes);

    const int hours = minutes / 60;
    if (hours < 24)
        return tr("%n h ago", nullptr, hours);

    const int days = hours / 24;
    if (days == 1)
        return tr("yesterday");

    // Past a couple of days the number stops being useful and starts being
    // alarming in the wrong way — the reading is not "a bit old", it is not
    // being refreshed at all, and that is what the tile should say.
    return tr("%n days ago", nullptr, days);
}
