// SPDX-FileCopyrightText: 2026 Jowi Aoun
// SPDX-License-Identifier: GPL-3.0-or-later
//
// The alert domain type: when an alert is on the screen, which one wins the
// banner, and when two messages are the same hazard.
//
// This file is about arithmetic on four timestamps, and it exists because
// getting that arithmetic wrong is invisible. An app that hides an alert at
// `expires` looks completely correct — the banner appears, the sheet works, the
// severity colour is right — and takes a Heat Advisory off the screen at five
// in the morning on the day of the heat. There is no screenshot in which that
// is visible, so it has to be a test.
//
// The values below are not invented. They are the instants in
// tests/fixtures/alerts/nws/siskiyou-heat-advisory.json, which is a real alert
// recorded from the live service, and the shape they make — expires eighteen
// hours before ends — is the shape 19 of the 25 alerts in force in California
// that afternoon had.

#include "libclima/domain/alert.h"

#include <QTest>

using namespace clima;

namespace {

// The recorded Heat Advisory, to the minute, in UTC.
//
//     onset      2026-08-05T14:15-07:00   =  21:15Z
//     expires    2026-08-06T05:00-07:00   =  12:00Z on the 6th
//     ends       2026-08-06T23:00-07:00   =  06:00Z on the 7th
const QDateTime kEffective{ QDate(2026, 8, 5), QTime(21, 15), QTimeZone::UTC };
const QDateTime kOnset{ QDate(2026, 8, 5), QTime(21, 15), QTimeZone::UTC };
const QDateTime kExpires{ QDate(2026, 8, 6), QTime(12, 0), QTimeZone::UTC };
const QDateTime kEnds{ QDate(2026, 8, 7), QTime(6, 0), QTimeZone::UTC };

QDateTime at(int day, int hour, int minute = 0)
{
    return QDateTime(QDate(2026, 8, day), QTime(hour, minute), QTimeZone::UTC);
}

Alert heatAdvisory()
{
    Alert alert;
    alert.id           = QStringLiteral("urn:oid:siskiyou.001.1");
    alert.providerId   = QStringLiteral("nws");
    alert.event        = QStringLiteral("Heat Advisory");
    alert.severity     = AlertSeverity::Moderate;
    alert.urgency      = AlertUrgency::Expected;
    alert.certainty    = AlertCertainty::Likely;
    alert.effective    = kEffective;
    alert.onset        = kOnset;
    alert.expires      = kExpires;
    alert.ends         = kEnds;
    alert.identityKeys = { QStringLiteral("nws:urn:oid:siskiyou.001.1") };
    return alert;
}

Alert graded(AlertSeverity severity, const QString &id)
{
    Alert alert;
    alert.id           = id;
    alert.event        = QStringLiteral("Test Event");
    alert.severity     = severity;
    alert.effective    = at(5, 0);
    alert.ends         = at(9, 0);
    alert.identityKeys = { id };
    return alert;
}

} // namespace

class TestAlerts : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void theHazardEndsAtEndsAndNotAtExpires();
    void whereThereIsNoEndsExpiresStandsIn();
    void theWholeExpiryTable_data();
    void theWholeExpiryTable();
    void anAlertWithNoOnsetIsActiveRatherThanPending();
    void pastTheRefreshDeadlineIsNotPastTheHazard();
    void unknownSeveritySortsBelowMinorAndStillDisplays();
    void rankingFallsThroughSeverityUrgencyCertaintyThenOnset();
    void rankingIsTotalSoASortIsStable();
    void twoMessagesSharingOneKeyAreOneHazard();
    void twoAlertsSharingEveryFieldButKeysAreTwoHazards();
    void displayableAtDropsEndedAndSortsTheRest();
};

// ---- the file's reason for existing ------------------------------------------------

void TestAlerts::theHazardEndsAtEndsAndNotAtExpires()
{
    const Alert alert = heatAdvisory();

    QCOMPARE(alert.hazardEnd(), kEnds);
    QVERIFY(alert.hazardEnd() != kExpires);

    // 13:00Z on the 6th: one hour past `expires`, seventeen hours before `ends`.
    // This is the moment an implementation that keys visibility off `expires`
    // takes the advisory down, and it is the middle of the afternoon it is
    // warning about.
    QVERIFY(alert.isDisplayableAt(at(6, 13)));
    QCOMPARE(alert.phaseAt(at(6, 13)), AlertPhase::Active);
}

void TestAlerts::whereThereIsNoEndsExpiresStandsIn()
{
    // All three Seattle Air Quality Alerts arrive this way. Without the
    // fallback they would have no end at all and would never come off screen.
    Alert alert = heatAdvisory();
    alert.ends  = QDateTime();

    QCOMPARE(alert.hazardEnd(), kExpires);
    QVERIFY(alert.isDisplayableAt(at(6, 11, 59)));
    QVERIFY(!alert.isDisplayableAt(at(6, 12, 1)));
}

void TestAlerts::theWholeExpiryTable_data()
{
    QTest::addColumn<QDateTime>("now");
    QTest::addColumn<int>("phase");

    QTest::newRow("before effective")
        << at(5, 20) << int(AlertPhase::NotYet);
    QTest::newRow("at effective, which is also onset")
        << at(5, 21, 15) << int(AlertPhase::Active);
    QTest::newRow("well inside")
        << at(6, 3) << int(AlertPhase::Active);
    QTest::newRow("one minute before expires")
        << at(6, 11, 59) << int(AlertPhase::Active);
    QTest::newRow("one minute after expires — still active")
        << at(6, 12, 1) << int(AlertPhase::Active);
    QTest::newRow("one minute before ends")
        << at(7, 5, 59) << int(AlertPhase::Active);
    QTest::newRow("at ends")
        << at(7, 6) << int(AlertPhase::Ended);
    QTest::newRow("after ends")
        << at(8, 0) << int(AlertPhase::Ended);
}

void TestAlerts::theWholeExpiryTable()
{
    QFETCH(QDateTime, now);
    QFETCH(int, phase);

    QCOMPARE(int(heatAdvisory().phaseAt(now)), phase);
}

void TestAlerts::anAlertWithNoOnsetIsActiveRatherThanPending()
{
    // ECCC states when a message takes effect and when the event ends, and never
    // states when the weather starts. Aliasing onset to effective would be a
    // guess; leaving it invalid means the alert is Active from the moment it is
    // effective, which is what the issuer actually said.
    Alert alert = heatAdvisory();
    alert.onset = QDateTime();

    QCOMPARE(alert.phaseAt(at(5, 22)), AlertPhase::Active);
    QCOMPARE(alert.phaseAt(at(5, 20)), AlertPhase::NotYet);
}

void TestAlerts::pastTheRefreshDeadlineIsNotPastTheHazard()
{
    const Alert alert = heatAdvisory();

    QVERIFY(!alert.isPastRefreshDeadline(at(6, 11)));
    QVERIFY(alert.isPastRefreshDeadline(at(6, 13)));

    // The two questions must not answer together. Past the deadline and still
    // displayable is the "last confirmed 14:05" state, and it is a state, not a
    // contradiction.
    QVERIFY(alert.isPastRefreshDeadline(at(6, 13)));
    QVERIFY(alert.isDisplayableAt(at(6, 13)));
}

// ---- ranking -------------------------------------------------------------------------

void TestAlerts::unknownSeveritySortsBelowMinorAndStillDisplays()
{
    const Alert unknown = graded(AlertSeverity::Unknown, QStringLiteral("u"));
    const Alert minor   = graded(AlertSeverity::Minor, QStringLiteral("m"));

    QVERIFY(minor.outranks(unknown));
    QVERIFY(!unknown.outranks(minor));

    // Ungraded is not hidden. Six of the nine recorded alerts are ungraded and
    // every one of them is a real warning somebody issued.
    QVERIFY(unknown.isDisplayableAt(at(6, 0)));
}

void TestAlerts::rankingFallsThroughSeverityUrgencyCertaintyThenOnset()
{
    Alert severe   = graded(AlertSeverity::Severe, QStringLiteral("a"));
    Alert moderate = graded(AlertSeverity::Moderate, QStringLiteral("b"));
    QVERIFY(severe.outranks(moderate));

    // Same severity: urgency decides.
    Alert immediate = graded(AlertSeverity::Severe, QStringLiteral("c"));
    immediate.urgency = AlertUrgency::Immediate;
    severe.urgency    = AlertUrgency::Future;
    QVERIFY(immediate.outranks(severe));

    // Same severity and urgency: certainty decides.
    Alert observed = graded(AlertSeverity::Severe, QStringLiteral("d"));
    observed.urgency   = AlertUrgency::Immediate;
    observed.certainty = AlertCertainty::Observed;
    immediate.certainty = AlertCertainty::Possible;
    QVERIFY(observed.outranks(immediate));

    // All three equal: the earlier onset wins.
    Alert early = graded(AlertSeverity::Minor, QStringLiteral("e"));
    Alert late  = graded(AlertSeverity::Minor, QStringLiteral("f"));
    early.onset = at(5, 6);
    late.onset  = at(5, 9);
    QVERIFY(early.outranks(late));

    // A missing onset is not evidence of imminence: it sorts last.
    Alert undated = graded(AlertSeverity::Minor, QStringLiteral("g"));
    QVERIFY(early.outranks(undated));
    QVERIFY(!undated.outranks(early));

    // An Extreme starting tomorrow still outranks a Severe running now, because
    // the banner shows one alert and the one it must not omit is the worst.
    Alert extremeTomorrow = graded(AlertSeverity::Extreme, QStringLiteral("h"));
    extremeTomorrow.onset = at(6, 12);
    Alert severeNow       = graded(AlertSeverity::Severe, QStringLiteral("i"));
    severeNow.onset       = at(5, 1);
    QVERIFY(extremeTomorrow.outranks(severeNow));
}

void TestAlerts::rankingIsTotalSoASortIsStable()
{
    // Two alerts equal on every graded axis and on onset. Without the final
    // tie-break on id, std::stable_sort would leave their order to whatever the
    // provider happened to emit — and a golden image of a two-alert banner would
    // alternate between runs.
    const Alert first  = graded(AlertSeverity::Moderate, QStringLiteral("aaa"));
    const Alert second = graded(AlertSeverity::Moderate, QStringLiteral("bbb"));

    QVERIFY(first.outranks(second));
    QVERIFY(!second.outranks(first));
}

// ---- identity --------------------------------------------------------------------------

void TestAlerts::twoMessagesSharingOneKeyAreOneHazard()
{
    // The NWS update case: a new message with a new id, referencing the one it
    // supersedes. The reference is what makes a dismissal survive the update.
    Alert original = graded(AlertSeverity::Moderate, QStringLiteral("first"));
    original.identityKeys = { QStringLiteral("nws:first") };

    Alert update = graded(AlertSeverity::Moderate, QStringLiteral("second"));
    update.identityKeys = { QStringLiteral("nws:second"), QStringLiteral("nws:first") };

    QVERIFY(update.isSameHazard(original));
    QVERIFY(original.isSameHazard(update));
}

void TestAlerts::twoAlertsSharingEveryFieldButKeysAreTwoHazards()
{
    // Seattle's two Air Quality Alerts: same event, same sender, same geocodes,
    // no references, different ids. This is the case that rules out an identity
    // built from what they have in common — the obvious design merges them and
    // hides one.
    Alert one = graded(AlertSeverity::Unknown, QStringLiteral("x"));
    Alert two = graded(AlertSeverity::Unknown, QStringLiteral("y"));

    one.event = two.event = QStringLiteral("Air Quality Alert");
    one.senderName = two.senderName = QStringLiteral("NWS Seattle WA");
    one.areaDescription = two.areaDescription = QStringLiteral("King; Pierce; Snohomish");
    one.identityKeys = { QStringLiteral("nws:x") };
    two.identityKeys = { QStringLiteral("nws:y") };

    QVERIFY(!one.isSameHazard(two));
}

// ---- the set -----------------------------------------------------------------------------

void TestAlerts::displayableAtDropsEndedAndSortsTheRest()
{
    AlertSet set;
    set.fetchedAt = at(6, 0);

    Alert ended = graded(AlertSeverity::Extreme, QStringLiteral("ended"));
    ended.ends  = at(5, 12);

    Alert notYet     = graded(AlertSeverity::Extreme, QStringLiteral("notyet"));
    notYet.effective = at(8, 0);

    set.alerts = { graded(AlertSeverity::Minor, QStringLiteral("minor")),
                   ended,
                   graded(AlertSeverity::Severe, QStringLiteral("severe")),
                   notYet,
                   graded(AlertSeverity::Unknown, QStringLiteral("unknown")) };

    const QList<Alert> shown = set.displayableAt(at(6, 0));

    QCOMPARE(shown.size(), 3);
    QCOMPARE(shown.at(0).id, QStringLiteral("severe"));
    QCOMPARE(shown.at(1).id, QStringLiteral("minor"));
    QCOMPARE(shown.at(2).id, QStringLiteral("unknown"));

    // An Extreme that has ended does not lead the banner, and an Extreme that
    // is not effective yet does not either. Both would, under a sort that ran
    // before the filter.
}

QTEST_MAIN(TestAlerts)
#include "tst_alerts.moc"
