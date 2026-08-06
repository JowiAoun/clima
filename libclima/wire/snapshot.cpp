// SPDX-FileCopyrightText: 2026 Jowi Aoun
// SPDX-License-Identifier: MPL-2.0

#include "libclima/wire/snapshot.h"

#include <QJsonArray>
#include <QJsonDocument>

namespace clima::wire {
namespace {

// A Reading on the wire. The whole point of the function is the else branch:
// QJsonValue() is null, and null is not 0.
QJsonValue number(Reading value)
{
    return value.has_value() ? QJsonValue(*value) : QJsonValue();
}

QJsonValue number(std::optional<int> value)
{
    return value.has_value() ? QJsonValue(*value) : QJsonValue();
}

// ISO 8601 with the offset the QDateTime carries. The forecast is fetched with
// timezone=auto, so these are the place's local times and the offset is what
// says so — a reader that formats them without one shows Toronto's afternoon
// in its own timezone.
QJsonValue instant(const QDateTime &value)
{
    return value.isValid() ? QJsonValue(value.toString(Qt::ISODate)) : QJsonValue();
}

QJsonValue day(const QDate &value)
{
    return value.isValid() ? QJsonValue(value.toString(Qt::ISODate)) : QJsonValue();
}

// Rule 3: a slice starts at `now`. The forecast carries a day of the past, so
// index 0 is behind us and a widget asking for six hours means the next six.
//
// The hour containing `now` is the first one included, not the one after it:
// an "hourly" point is the hour *ending* at its timestamp, so the point the
// user is living through is the first one they expect to see.
qsizetype firstHourAtOrAfter(const QList<HourlyPoint> &hourly, const QDateTime &now)
{
    for (qsizetype i = 0; i < hourly.size(); ++i) {
        if (hourly.at(i).time >= now)
            return i;
    }
    return hourly.size();
}

qsizetype firstDayAtOrAfter(const QList<DailyPoint> &daily, const QDate &today)
{
    for (qsizetype i = 0; i < daily.size(); ++i) {
        if (daily.at(i).date >= today)
            return i;
    }
    return daily.size();
}

// `count` < 0 means "all of it".
qsizetype clampCount(qsizetype available, int requested)
{
    if (requested < 0)
        return available;
    return std::min<qsizetype>(available, requested);
}

} // namespace

// ---- FieldMask --------------------------------------------------------------

FieldMask FieldMask::everything()
{
    FieldMask mask;
    mask.m_everything = true;
    return mask;
}

FieldMask FieldMask::fromFields(const QStringList &fields)
{
    // An empty list is "everything", not "nothing" — see the header. A reader
    // that forgot the argument gets a full snapshot rather than a blank tile
    // it will spend an afternoon debugging.
    if (fields.isEmpty())
        return everything();

    FieldMask mask;
    for (const QString &field : fields) {
        const QString trimmed = field.trimmed();
        if (!trimmed.isEmpty())
            mask.m_paths.insert(trimmed);
    }
    if (mask.m_paths.isEmpty())
        return everything();
    return mask;
}

bool FieldMask::wants(const QString &path) const
{
    if (m_everything)
        return true;
    if (m_paths.contains(path))
        return true;

    // A branch selects its leaves: "current" wants "current.temperature".
    for (qsizetype dot = path.indexOf(u'.'); dot > 0; dot = path.indexOf(u'.', dot + 1)) {
        if (m_paths.contains(path.left(dot)))
            return true;
    }
    return false;
}

bool FieldMask::wantsAnyUnder(const QString &branch) const
{
    if (m_everything)
        return true;
    if (m_paths.contains(branch))
        return true;

    const QString prefix = branch + u'.';
    for (const QString &path : m_paths) {
        if (path.startsWith(prefix))
            return true;
    }
    return false;
}

QStringList FieldMask::fields() const
{
    if (m_everything)
        return {};
    QStringList out(m_paths.cbegin(), m_paths.cend());
    out.sort();
    return out;
}

// ---- the snapshot -----------------------------------------------------------

QJsonObject buildSnapshot(const SnapshotSource &source, const FieldMask &mask)
{
    QJsonObject root;

    // These four are unconditional. A reader has to be able to work out how
    // old this is, and whether it understands the shape, before it reads a
    // single number out of it.
    root.insert(QStringLiteral("schema"), kSchemaVersion);
    root.insert(QStringLiteral("placeId"), source.placeId);
    root.insert(QStringLiteral("generatedAt"), instant(source.now));
    root.insert(QStringLiteral("state"),
                source.fromCache ? QStringLiteral("cached") : QStringLiteral("live"));

    if (!source.servedBy.isEmpty())
        root.insert(QStringLiteral("servedBy"), source.servedBy);

    // The moment the data itself was fetched, which is not the moment this
    // snapshot was built. It is what "updated 40 minutes ago" is computed
    // from, and it is the reason killing the daemon leaves a stale reading on
    // screen instead of a blank tile.
    if (source.forecast.fetchedAt.isValid())
        root.insert(QStringLiteral("fetchedAt"), instant(source.forecast.fetchedAt));

    if (mask.wantsAnyUnder(QStringLiteral("place"))) {
        QJsonObject place;
        place.insert(QStringLiteral("name"), source.place.name);
        place.insert(QStringLiteral("region"), source.place.region());
        place.insert(QStringLiteral("label"), source.place.label());
        place.insert(QStringLiteral("countryCode"), source.place.countryCode);
        place.insert(QStringLiteral("timezone"), source.place.timezone);
        place.insert(QStringLiteral("latitude"), source.place.coordinate.latitude);
        place.insert(QStringLiteral("longitude"), source.place.coordinate.longitude);
        root.insert(QStringLiteral("place"), place);
    }

    if (mask.wantsAnyUnder(QStringLiteral("current"))) {
        const CurrentConditions &c = source.forecast.current;
        QJsonObject current;

        const auto put = [&](const char *leaf, const QJsonValue &value) {
            const QString path = QLatin1String("current.") + QLatin1String(leaf);
            if (mask.wants(path))
                current.insert(QLatin1String(leaf), value);
        };

        put("time", instant(c.time));
        put("temperature", number(c.temperature));
        put("apparentTemperature", number(c.apparentTemperature));
        put("relativeHumidity", number(c.relativeHumidity));
        put("dewPoint", number(c.dewPoint));
        put("precipitation", number(c.precipitation));
        put("windSpeed", number(c.windSpeed));
        put("windGust", number(c.windGust));
        put("windDirection", number(c.windDirection));
        put("pressureMsl", number(c.pressureMsl));
        put("cloudCover", number(c.cloudCover));
        put("visibility", number(c.visibility));
        put("uvIndex", number(c.uvIndex));
        put("weatherCode", number(c.weatherCode));
        put("isDay", c.isDay.has_value() ? QJsonValue(*c.isDay) : QJsonValue());

        if (!current.isEmpty())
            root.insert(QStringLiteral("current"), current);
    }

    if (source.hours != 0 && mask.wantsAnyUnder(QStringLiteral("hourly"))) {
        const QList<HourlyPoint> &points = source.forecast.hourly;
        const qsizetype           from   = firstHourAtOrAfter(points, source.now);
        const qsizetype count = clampCount(points.size() - from, source.hours);

        QJsonObject hourly;

        // Rule 1: the axis travels whether or not it was asked for.
        QJsonArray time;
        for (qsizetype i = from; i < from + count; ++i)
            time.append(instant(points.at(i).time));
        hourly.insert(QStringLiteral("time"), time);

        const auto series = [&](const char *leaf, auto pick) {
            const QString path = QLatin1String("hourly.") + QLatin1String(leaf);
            if (!mask.wants(path))
                return;
            QJsonArray values;
            for (qsizetype i = from; i < from + count; ++i)
                values.append(pick(points.at(i)));
            hourly.insert(QLatin1String(leaf), values);
        };

        series("temperature", [](const HourlyPoint &p) { return number(p.temperature); });
        series("apparentTemperature",
               [](const HourlyPoint &p) { return number(p.apparentTemperature); });
        series("relativeHumidity", [](const HourlyPoint &p) { return number(p.relativeHumidity); });
        series("precipitation", [](const HourlyPoint &p) { return number(p.precipitation); });
        series("precipitationProbability",
               [](const HourlyPoint &p) { return number(p.precipitationProbability); });
        series("windSpeed", [](const HourlyPoint &p) { return number(p.windSpeed); });
        series("windGust", [](const HourlyPoint &p) { return number(p.windGust); });
        series("windDirection", [](const HourlyPoint &p) { return number(p.windDirection); });
        series("cloudCover", [](const HourlyPoint &p) { return number(p.cloudCover); });
        series("uvIndex", [](const HourlyPoint &p) { return number(p.uvIndex); });
        series("weatherCode", [](const HourlyPoint &p) { return number(p.weatherCode); });
        series("isDay", [](const HourlyPoint &p) {
            return p.isDay.has_value() ? QJsonValue(*p.isDay) : QJsonValue();
        });

        root.insert(QStringLiteral("hourly"), hourly);
    }

    if (source.days != 0 && mask.wantsAnyUnder(QStringLiteral("daily"))) {
        const QList<DailyPoint> &points = source.forecast.daily;
        const QDate              today  = source.now.date();
        const qsizetype          from   = firstDayAtOrAfter(points, today);
        const qsizetype count = clampCount(points.size() - from, source.days);

        QJsonObject daily;

        QJsonArray dates;
        for (qsizetype i = from; i < from + count; ++i)
            dates.append(day(points.at(i).date));
        daily.insert(QStringLiteral("date"), dates);

        const auto series = [&](const char *leaf, auto pick) {
            const QString path = QLatin1String("daily.") + QLatin1String(leaf);
            if (!mask.wants(path))
                return;
            QJsonArray values;
            for (qsizetype i = from; i < from + count; ++i)
                values.append(pick(points.at(i)));
            daily.insert(QLatin1String(leaf), values);
        };

        series("temperatureMax", [](const DailyPoint &p) { return number(p.temperatureMax); });
        series("temperatureMin", [](const DailyPoint &p) { return number(p.temperatureMin); });
        series("precipitationSum", [](const DailyPoint &p) { return number(p.precipitationSum); });
        series("precipitationProbabilityMax",
               [](const DailyPoint &p) { return number(p.precipitationProbabilityMax); });
        series("windSpeedMax", [](const DailyPoint &p) { return number(p.windSpeedMax); });
        series("uvIndexMax", [](const DailyPoint &p) { return number(p.uvIndexMax); });
        series("weatherCode", [](const DailyPoint &p) { return number(p.weatherCode); });
        series("sunrise", [](const DailyPoint &p) { return instant(p.sunrise); });
        series("sunset", [](const DailyPoint &p) { return instant(p.sunset); });
        series("moonPhase", [](const DailyPoint &p) { return number(p.moonPhase); });

        root.insert(QStringLiteral("daily"), daily);
    }

    if (mask.wantsAnyUnder(QStringLiteral("airquality"))) {
        const AirQualityPoint &now = source.airQuality.current;
        QJsonObject            air;

        // The European index, everywhere, because that is what the app shows
        // everywhere (app/viewmodels/conditionsdata.cpp). Two scales would let
        // a widget and the card behind it disagree about the same air.
        if (mask.wants(QStringLiteral("airquality.index")))
            air.insert(QStringLiteral("index"), number(now.europeanAqi));

        if (mask.wants(QStringLiteral("airquality.dominant"))) {
            const std::optional<Pollutant> dominant = now.dominantPollutant();
            air.insert(QStringLiteral("dominant"),
                       dominant ? QJsonValue(pollutantId(*dominant)) : QJsonValue());
        }

        if (!air.isEmpty()) {
            air.insert(QStringLiteral("time"), instant(now.time));
            root.insert(QStringLiteral("airquality"), air);
        }
    }

    if (mask.wantsAnyUnder(QStringLiteral("alerts"))) {
        // Filtered against the same clock everything else was sliced at, and
        // ranked, so a widget with room for one shows the one that matters.
        // `outranks` is the app's ordering, not a second one.
        QList<Alert> live = source.alerts.displayableAt(source.now);
        std::sort(live.begin(), live.end(),
                  [](const Alert &a, const Alert &b) { return a.outranks(b); });

        QJsonArray alerts;
        for (const Alert &alert : std::as_const(live)) {
            QJsonObject item;
            item.insert(QStringLiteral("id"), alert.id);
            item.insert(QStringLiteral("event"), alert.event);
            item.insert(QStringLiteral("headline"), alert.headline);
            item.insert(QStringLiteral("severity"), alertSeverityKey(alert.severity));
            item.insert(QStringLiteral("urgency"), alertUrgencyName(alert.urgency));
            item.insert(QStringLiteral("issuer"), alert.issuerLabel);
            item.insert(QStringLiteral("areaDescription"), alert.areaDescription);
            item.insert(QStringLiteral("onset"), instant(alert.onset));
            item.insert(QStringLiteral("expires"), instant(alert.expires));
            item.insert(QStringLiteral("ends"), instant(alert.ends));
            if (!alert.web.isEmpty())
                item.insert(QStringLiteral("web"), alert.web.toString());
            alerts.append(item);

            // No description and no instruction. A widget shows a banner and
            // the app shows the text; sending several kilobytes of prose to a
            // tile that has room for one line is what the field mask exists to
            // avoid.
        }
        root.insert(QStringLiteral("alerts"), alerts);

        // Distinct from an empty list: "we have not managed to ask yet" is not
        // "there is nothing in force", and a widget must not claim the second
        // when it means the first.
        root.insert(QStringLiteral("alertsKnown"), source.alerts.isValid());
    }

    return root;
}

QByteArray encode(const QJsonObject &snapshot)
{
    return QJsonDocument(snapshot).toJson(QJsonDocument::Compact);
}

} // namespace clima::wire
