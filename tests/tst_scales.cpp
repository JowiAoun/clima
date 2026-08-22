// SPDX-FileCopyrightText: 2026 Jowi Aoun
// SPDX-License-Identifier: GPL-3.0-or-later
//
// Somebody else's tables, checked against the authority that published them.
//
// libclima/domain/scales.h is a transcription: the WHO's UV bands, the European
// AQI's own bands, the Beaufort scale, the sixteen-point compass. Nothing in it
// is a judgement of ours, which is exactly why it needs a test — a transcription
// error is invisible. Every function returns a plausible word for every input,
// so a band boundary off by one grades a UV index of 6 as "Moderate" for the
// rest of the app's life and no screenshot shows it.
//
// Three kinds of assertion here, and the second is the one that earns its keep:
//
//   the centres      one value comfortably inside each band. Catches a row
//                    deleted or two rows swapped.
//
//   the edges        the value ON the boundary and the value one step below it.
//                    This is where a `<` written as a `<=` lives, and the three
//                    string tables in this file deliberately do not agree about
//                    which comparison they use — uvBand is `<`, aqiBand is `<=`,
//                    visibilityBand is `>=`. A tidy-up that unified them would
//                    silently move two of the three.
//
//   the closure      the label table against the id table it claims to read.
//                    Two enums in two files that have to spell things the same
//                    way, with no compiler between them.
//
// ---- NaN is a case, not an oversight ----------------------------------------
//
// scales.h opens with a paragraph about it: a caller with no reading must be
// handed an empty string rather than "Low", because "being handed 'Low' for a
// UV index nobody measured is the null-drawn-as-zero mistake with a word on it."
// Every string function here is asked that question.

#include "libclima/domain/airquality.h"
#include "libclima/domain/scales.h"

#include <QSet>
#include <QTest>

#include <cmath>
#include <limits>

using namespace clima;
using namespace clima::scales;

namespace {

const double kNaN = std::numeric_limits<double>::quiet_NaN();
const double kInf = std::numeric_limits<double>::infinity();

// One step below a boundary, in the units the band is expressed in. Small
// enough to be inside the last representable slice of the lower band and large
// enough that a double can tell the two apart.
constexpr double kEpsilon = 0.001;

// What a chemist writes, for every species the engine can name. Closed on
// purpose, in the way tst_weathercode.cpp's emittedCodes() is: adding a
// pollutant to the enum without adding it here fails this file, which is the
// only moment anybody is going to remember that the label table needs a row too.
//
// Not derived from pollutantLabel() — that is the thing under test, and a table
// checked against itself checks nothing.
QString expectedLabel(Pollutant pollutant)
{
    switch (pollutant) {
    case Pollutant::Pm2_5:           return QStringLiteral("PM2.5");
    case Pollutant::Pm10:            return QStringLiteral("PM10");
    case Pollutant::Ozone:           return QStringLiteral("O\u2083");
    case Pollutant::NitrogenDioxide: return QStringLiteral("NO\u2082");
    case Pollutant::SulphurDioxide:  return QStringLiteral("SO\u2082");
    case Pollutant::CarbonMonoxide:  return QStringLiteral("CO");
    case Pollutant::Count:           break;
    }
    return {};
}

} // namespace

class TestScales : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    // ---- UV ----------------------------------------------------------------
    void uvBandsMatchTheTableInTheHeader_data();
    void uvBandsMatchTheTableInTheHeader();
    void uvBandsBreakAtTheBoundaryAndNotBeforeIt_data();
    void uvBandsBreakAtTheBoundaryAndNotBeforeIt();

    // ---- AQI ---------------------------------------------------------------
    void aqiBandsMatchTheEuropeanIndex_data();
    void aqiBandsMatchTheEuropeanIndex();
    void theAqiBoundaryBelongsToTheLowerBand();

    // ---- visibility --------------------------------------------------------
    void visibilityBandsDescendInKilometres_data();
    void visibilityBandsDescendInKilometres();
    void theVisibilityBoundaryBelongsToTheHigherBand();

    // ---- Beaufort ----------------------------------------------------------
    void everyPublishedBeaufortBandInvertsBackToItsForce_data();
    void everyPublishedBeaufortBandInvertsBackToItsForce();
    void theOneKilometreDisagreementIsTheFormulaAndNotABug();
    void beaufortIsBoundedAtBothEndsOfTheScale();
    void aWindSpeedThatIsNotAFiniteNumberIsNotAHurricane();
    void everyForceHasItsOwnName();

    // ---- compass -----------------------------------------------------------
    void theSixteenPointsSitAtTheirOwnCentres_data();
    void theSixteenPointsSitAtTheirOwnCentres();
    void northWrapsRatherThanRunningOffTheEndOfTheTable();
    void aBearingMeansTheSameThingEveryTimeRoundTheCircle();
    void aBearingOutsideZeroToThreeSixtyStillNamesAPoint();

    // ---- pollutants --------------------------------------------------------
    void everyPollutantIdTheEngineEmitsHasAChemicalName();
    void theSubscriptsAreRealCharactersAndNotMarkup();
    void anUnknownPollutantIsUppercasedRatherThanDropped();

    // ---- the shared rule ---------------------------------------------------
    void noBandNamesAReadingThatDoesNotExist();
};

// ============================================================================
// UV — the WHO's five bands. scales.h states the table outright: "low 0–2,
// moderate 3–5, high 6–7, very high 8–10, extreme 11+", so that is what is
// asserted, integer by integer, rather than a handful of values chosen to pass.
// ============================================================================

void TestScales::uvBandsMatchTheTableInTheHeader_data()
{
    QTest::addColumn<double>("index");
    QTest::addColumn<QString>("band");

    // Every whole index from 0 to 15, because the published table is stated in
    // whole numbers and a reader comparing this file with the WHO's page should
    // be able to do it line by line.
    const struct { int from; int to; const char *band; } rows[] = {
        { 0,  2,  "Low"       },
        { 3,  5,  "Moderate"  },
        { 6,  7,  "High"      },
        { 8,  10, "Very high" },
        { 11, 15, "Extreme"   },
    };

    for (const auto &row : rows) {
        for (int i = row.from; i <= row.to; ++i) {
            QTest::newRow(qPrintable(QStringLiteral("uv %1").arg(i)))
                << double(i) << QString::fromLatin1(row.band);
        }
    }
}

void TestScales::uvBandsMatchTheTableInTheHeader()
{
    QFETCH(double, index);
    QFETCH(QString, band);
    QCOMPARE(uvBand(index), band);
}

void TestScales::uvBandsBreakAtTheBoundaryAndNotBeforeIt_data()
{
    QTest::addColumn<double>("boundary");
    QTest::addColumn<QString>("below");
    QTest::addColumn<QString>("atOrAbove");

    // uvBand uses `<`, so the boundary value belongs to the band ABOVE it.
    const struct { double at; const char *below; const char *above; } edges[] = {
        { 3.0,  "Low",       "Moderate"  },
        { 6.0,  "Moderate",  "High"      },
        { 8.0,  "High",      "Very high" },
        { 11.0, "Very high", "Extreme"   },
    };

    for (const auto &edge : edges) {
        QTest::newRow(qPrintable(QStringLiteral("%1 opens %2").arg(edge.at).arg(edge.above)))
            << edge.at << QString::fromLatin1(edge.below) << QString::fromLatin1(edge.above);
    }
}

void TestScales::uvBandsBreakAtTheBoundaryAndNotBeforeIt()
{
    QFETCH(double, boundary);
    QFETCH(QString, below);
    QFETCH(QString, atOrAbove);

    QCOMPARE(uvBand(boundary - kEpsilon), below);
    QCOMPARE(uvBand(boundary), atOrAbove);
}

// ============================================================================
// AQI — the European index, and the one table in this file whose comparison is
// `<=`. 20 is Good and 20.001 is Fair, which is the opposite convention to the
// UV table three functions away.
// ============================================================================

void TestScales::aqiBandsMatchTheEuropeanIndex_data()
{
    QTest::addColumn<double>("index");
    QTest::addColumn<QString>("band");

    QTest::newRow("0 is the floor")    << 0.0   << QStringLiteral("Good");
    QTest::newRow("10 mid Good")        << 10.0  << QStringLiteral("Good");
    QTest::newRow("30 mid Fair")        << 30.0  << QStringLiteral("Fair");
    QTest::newRow("50 mid Moderate")    << 50.0  << QStringLiteral("Moderate");
    QTest::newRow("70 mid Poor")        << 70.0  << QStringLiteral("Poor");
    QTest::newRow("90 mid Very poor")   << 90.0  << QStringLiteral("Very poor");
    QTest::newRow("140 off the scale")  << 140.0 << QStringLiteral("Extremely poor");

    // The index has no negative half. A caller handing one over has a defect of
    // its own; grading it Good rather than returning nothing is the behaviour
    // every band table here shares for an out-of-range low value, and it is
    // recorded so that a change to it is a deliberate one.
    QTest::newRow("below the floor")    << -5.0  << QStringLiteral("Good");
}

void TestScales::aqiBandsMatchTheEuropeanIndex()
{
    QFETCH(double, index);
    QFETCH(QString, band);
    QCOMPARE(aqiBand(index), band);
}

void TestScales::theAqiBoundaryBelongsToTheLowerBand()
{
    // The whole point of this test is the contrast with uvBand above: there the
    // boundary opens the higher band, here it closes the lower one. Both are
    // right — they are two authorities' tables — and a refactor that made them
    // agree would be wrong twice.
    QCOMPARE(aqiBand(20.0), QStringLiteral("Good"));
    QCOMPARE(aqiBand(20.0 + kEpsilon), QStringLiteral("Fair"));

    QCOMPARE(aqiBand(40.0), QStringLiteral("Fair"));
    QCOMPARE(aqiBand(40.0 + kEpsilon), QStringLiteral("Moderate"));

    QCOMPARE(aqiBand(60.0), QStringLiteral("Moderate"));
    QCOMPARE(aqiBand(60.0 + kEpsilon), QStringLiteral("Poor"));

    QCOMPARE(aqiBand(80.0), QStringLiteral("Poor"));
    QCOMPARE(aqiBand(80.0 + kEpsilon), QStringLiteral("Very poor"));

    QCOMPARE(aqiBand(100.0), QStringLiteral("Very poor"));
    QCOMPARE(aqiBand(100.0 + kEpsilon), QStringLiteral("Extremely poor"));
}

// ============================================================================
// Visibility — kilometres, and the only table here that descends. `>=`, so a
// boundary belongs to the band above it.
//
// The units are the hazard. Open-Meteo serves `visibility` in METRES —
// libclima/providers/openmeteo/openmeteovariables.cpp says so on the line that
// asks for it — and nothing in this function can tell 10 km from 10 m. The
// conversion is the adapter's job and tst_openmeteoadapter.cpp is where it is
// checked; all this file can do is pin the scale these numbers are on.
// ============================================================================

void TestScales::visibilityBandsDescendInKilometres_data()
{
    QTest::addColumn<double>("km");
    QTest::addColumn<QString>("band");

    QTest::newRow("24 km clear day")   << 24.0  << QStringLiteral("Excellent");
    QTest::newRow("12 km")             << 12.0  << QStringLiteral("Good");
    QTest::newRow("6 km haze")         << 6.0   << QStringLiteral("Moderate");
    QTest::newRow("2 km mist")         << 2.0   << QStringLiteral("Poor");
    QTest::newRow("0.3 km fog")        << 0.3   << QStringLiteral("Very poor");
    QTest::newRow("0 km")              << 0.0   << QStringLiteral("Very poor");
}

void TestScales::visibilityBandsDescendInKilometres()
{
    QFETCH(double, km);
    QFETCH(QString, band);
    QCOMPARE(visibilityBand(km), band);
}

void TestScales::theVisibilityBoundaryBelongsToTheHigherBand()
{
    QCOMPARE(visibilityBand(16.0), QStringLiteral("Excellent"));
    QCOMPARE(visibilityBand(16.0 - kEpsilon), QStringLiteral("Good"));

    QCOMPARE(visibilityBand(10.0), QStringLiteral("Good"));
    QCOMPARE(visibilityBand(10.0 - kEpsilon), QStringLiteral("Moderate"));

    QCOMPARE(visibilityBand(4.0), QStringLiteral("Moderate"));
    QCOMPARE(visibilityBand(4.0 - kEpsilon), QStringLiteral("Poor"));

    QCOMPARE(visibilityBand(1.0), QStringLiteral("Poor"));
    QCOMPARE(visibilityBand(1.0 - kEpsilon), QStringLiteral("Very poor"));
}

// ============================================================================
// Beaufort. scales.h says it inverts v = 0.836·B^1.5, and the interesting
// question is whether that inversion reproduces the km/h table the WMO
// publishes — because the app shows the WORD, and the word is what a reader
// checks against a forecast on television.
// ============================================================================

void TestScales::everyPublishedBeaufortBandInvertsBackToItsForce_data()
{
    QTest::addColumn<int>("lower");
    QTest::addColumn<int>("upper");
    QTest::addColumn<int>("force");

    // The WMO's own km/h ranges, both edges of each. Force 1 starts at 2 rather
    // than at the published 1 — see the test below it, which is about that one
    // kilometre and nothing else.
    const struct { int lower; int upper; int force; } bands[] = {
        {   0,   0,  0 },
        {   2,   5,  1 },
        {   6,  11,  2 },
        {  12,  19,  3 },
        {  20,  28,  4 },
        {  29,  38,  5 },
        {  39,  49,  6 },
        {  50,  61,  7 },
        {  62,  74,  8 },
        {  75,  88,  9 },
        {  89, 102, 10 },
        { 103, 117, 11 },
        { 118, 200, 12 },
    };

    for (const auto &band : bands) {
        QTest::newRow(qPrintable(QStringLiteral("F%1 %2-%3 km/h")
                                     .arg(band.force).arg(band.lower).arg(band.upper)))
            << band.lower << band.upper << band.force;
    }
}

void TestScales::everyPublishedBeaufortBandInvertsBackToItsForce()
{
    QFETCH(int, lower);
    QFETCH(int, upper);
    QFETCH(int, force);

    // Both edges AND everything between them, because a band that is right at
    // its ends and wrong in the middle is a band whose formula has been
    // replaced by a lookup with a hole in it.
    for (int kmh = lower; kmh <= upper; ++kmh) {
        QCOMPARE(beaufortForce(double(kmh)), force);
    }
}

void TestScales::theOneKilometreDisagreementIsTheFormulaAndNotABug()
{
    // The WMO prints "1–5 km/h" for Force 1. The formula puts the 0/1 crossover
    // at 0.836·0.5^1.5 m/s = 1.064 km/h, so 1 km/h is Force 0 and 1.1 is
    // Force 1.
    //
    // The formula is right and the table is a rounding of it: the scale is
    // DEFINED in metres per second, and 1 km/h is 0.278 m/s against a boundary
    // at 0.296 m/s. Rounded to whole km/h there is nowhere for that to go.
    //
    // Recorded here so that the next person to compare this code with a
    // published table finds the answer instead of "fixing" it — a special case
    // for 1 km/h would put a discontinuity in a continuous function to make one
    // integer match a lossy printing of it.
    QCOMPARE(beaufortForce(1.0), 0);
    QCOMPARE(beaufortForce(1.064), 0);
    QCOMPARE(beaufortForce(1.1), 1);

    QCOMPARE(beaufortName(beaufortForce(1.0)), QStringLiteral("Calm"));
}

void TestScales::beaufortIsBoundedAtBothEndsOfTheScale()
{
    // "The scale has no 13, and extrapolating one would be inventing a
    // category" — scales.h. A category-5 hurricane is around 280 km/h and the
    // strongest surface wind ever recorded is 408; all of it is Force 12.
    QCOMPARE(beaufortForce(280.0), 12);
    QCOMPARE(beaufortForce(408.0), 12);
    QCOMPARE(beaufortForce(100000.0), 12);

    QCOMPARE(beaufortForce(0.0), 0);
}

void TestScales::aWindSpeedThatIsNotAFiniteNumberIsNotAHurricane()
{
    // Three inputs that are not a wind speed, and the direction each one has to
    // fail in.
    //
    // NaN is "no reading", and 0 is the only int that can carry that: the
    // caller has to check the reading rather than the force. Every other
    // function in scales.h answers an empty STRING for NaN; this one returns an
    // int and has nowhere to put the same answer, which is why
    // ConditionsData::buildWind has to test the reading itself before it prints
    // a name — see tst_conditionsdata.cpp.
    QCOMPARE(beaufortForce(kNaN), 0);

    // Infinity is the one that used to be wrong, and wrong in the worse
    // direction. `int(std::pow(inf, 2.0/3.0) + 0.5)` is undefined behaviour;
    // on x86-64 it produces INT_MIN, which qBound clamps to 0 — so an infinite
    // wind speed reported "Calm". A guard on finiteness saturates it at the top
    // of the scale instead, which is the only end of the scale an unbounded
    // number can honestly be at.
    QCOMPARE(beaufortForce(kInf), 12);

    // A negative wind speed is a caller's defect rather than weather, and 0 is
    // the floor of a scale that has no negative half.
    QCOMPARE(beaufortForce(-kInf), 0);
    QCOMPARE(beaufortForce(-10.0), 0);
}

void TestScales::everyForceHasItsOwnName()
{
    // Thirteen forces, thirteen names, no two the same. A switch with a fallen
    // -through case reads perfectly and puts "Gale" on two different winds.
    QSet<QString> seen;
    for (int force = 0; force <= 12; ++force) {
        const QString name = beaufortName(force);
        QVERIFY2(!name.isEmpty(), qPrintable(QStringLiteral("force %1 has no name").arg(force)));
        QVERIFY2(!seen.contains(name),
                 qPrintable(QStringLiteral("force %1 reuses the name \"%2\"")
                                .arg(force).arg(name)));
        seen.insert(name);
    }
    QCOMPARE(seen.size(), 13);

    // The two ends, by name, because those are the two a reader notices.
    QCOMPARE(beaufortName(0), QStringLiteral("Calm"));
    QCOMPARE(beaufortName(12), QStringLiteral("Hurricane force"));

    // Off the end of the scale in either direction. beaufortName's `default`
    // catches both, and the top of the scale is the safe place for an unknown
    // force to land.
    QCOMPARE(beaufortName(13), QStringLiteral("Hurricane force"));
    QCOMPARE(beaufortName(-1), QStringLiteral("Hurricane force"));
}

// ============================================================================
// The compass. Sixteen points, and the only function here that has to wrap.
// ============================================================================

void TestScales::theSixteenPointsSitAtTheirOwnCentres_data()
{
    QTest::addColumn<double>("degrees");
    QTest::addColumn<QString>("point");

    const char *const points[] = { "N",  "NNE", "NE", "ENE", "E",  "ESE", "SE", "SSE",
                                   "S",  "SSW", "SW", "WSW", "W",  "WNW", "NW", "NNW" };

    for (int i = 0; i < 16; ++i) {
        const double centre = i * 22.5;
        QTest::newRow(qPrintable(QStringLiteral("%1 deg is %2")
                                     .arg(centre).arg(QString::fromLatin1(points[i]))))
            << centre << QString::fromLatin1(points[i]);
    }
}

void TestScales::theSixteenPointsSitAtTheirOwnCentres()
{
    QFETCH(double, degrees);
    QFETCH(QString, point);

    QCOMPARE(compassPoint(degrees), point);

    // And the whole sector around that centre, not only its middle. Each point
    // owns 22.5°, so ±11° either side has to give the same answer.
    QCOMPARE(compassPoint(degrees + 11.0), point);
    QCOMPARE(compassPoint(degrees - 11.0 + 360.0), point);
}

void TestScales::northWrapsRatherThanRunningOffTheEndOfTheTable()
{
    // North is the point that straddles zero, so it is the only one whose
    // sector is not contiguous in the input. `& 15` is what makes index 16
    // come back to 0; without it this is a read one past the end of a
    // sixteen-element array.
    QCOMPARE(compassPoint(0.0), QStringLiteral("N"));
    QCOMPARE(compassPoint(11.0), QStringLiteral("N"));
    QCOMPARE(compassPoint(349.0), QStringLiteral("N"));
    QCOMPARE(compassPoint(359.9), QStringLiteral("N"));
    QCOMPARE(compassPoint(360.0), QStringLiteral("N"));

    // And the two values that decide where north stops. The halfway point
    // rounds outward, so 11.25 has already left N.
    QCOMPARE(compassPoint(11.24), QStringLiteral("N"));
    QCOMPARE(compassPoint(11.25), QStringLiteral("NNE"));
    QCOMPARE(compassPoint(348.74), QStringLiteral("NNW"));
    QCOMPARE(compassPoint(348.75), QStringLiteral("N"));
}

void TestScales::aBearingMeansTheSameThingEveryTimeRoundTheCircle()
{
    // A property rather than a table: a bearing and the same bearing a full
    // turn later are the same direction. Swept finely enough to cross every one
    // of the sixteen boundaries from both sides.
    for (double degrees = 0.0; degrees < 360.0; degrees += 0.25) {
        const QString once = compassPoint(degrees);
        QVERIFY2(!once.isEmpty(), qPrintable(QStringLiteral("%1 deg named nothing").arg(degrees)));
        QCOMPARE(compassPoint(degrees + 360.0), once);
        QCOMPARE(compassPoint(degrees + 720.0), once);
    }
}

void TestScales::aBearingOutsideZeroToThreeSixtyStillNamesAPoint()
{
    static const QSet<QString> valid = {
        QStringLiteral("N"),   QStringLiteral("NNE"), QStringLiteral("NE"),  QStringLiteral("ENE"),
        QStringLiteral("E"),   QStringLiteral("ESE"), QStringLiteral("SE"),  QStringLiteral("SSE"),
        QStringLiteral("S"),   QStringLiteral("SSW"), QStringLiteral("SW"),  QStringLiteral("WSW"),
        QStringLiteral("W"),   QStringLiteral("WNW"), QStringLiteral("NW"),  QStringLiteral("NNW"),
    };

    // A negative bearing is not something a forecast produces — every provider
    // sends 0–360 — but `& 15` on a negative index is the kind of arithmetic
    // that either works everywhere or reads one before the start of an array,
    // and "works everywhere" should be asserted rather than assumed.
    //
    // Not compared against the positive bearing it is congruent to: std::lround
    // rounds half AWAY from zero, so -11.25 and 348.75 land on opposite sides
    // of the same boundary. That asymmetry is unreachable from real data and
    // naming a point for every input is the property that actually matters.
    for (double degrees = -720.0; degrees <= 1080.0; degrees += 3.75) {
        QVERIFY2(valid.contains(compassPoint(degrees)),
                 qPrintable(QStringLiteral("%1 deg gave \"%2\"")
                                .arg(degrees).arg(compassPoint(degrees))));
    }
}

// ============================================================================
// Pollutants — the closure test, and the reason this file exists at all.
//
// pollutantLabel() takes "a pollutant's machine id" and its header names
// clima::pollutantId() as where those come from. Those are two tables in two
// files with no compiler between them, and the app calls one on the output of
// the other: app/viewmodels/conditionsdata.cpp does
// `scales::pollutantLabel(pollutantId(*worst))` and widgets/wx.cpp does the
// same to the id that arrives over the wire.
//
// So every id the enum can produce has to be a key the table knows. Nothing
// fails when it is not — the table's fallback uppercases the id — which is how
// four of the six pollutants came to print OZONE, NITROGEN_DIOXIDE,
// SULPHUR_DIOXIDE and CARBON_MONOXIDE on a card whose whole reason for existing
// was that the fifth was printing PM2_5.
// ============================================================================

void TestScales::everyPollutantIdTheEngineEmitsHasAChemicalName()
{
    for (int i = 0; i < int(Pollutant::Count); ++i) {
        const Pollutant pollutant = static_cast<Pollutant>(i);
        const QString   id        = pollutantId(pollutant);

        QVERIFY2(!id.isEmpty(), qPrintable(QStringLiteral("pollutant %1 has no id").arg(i)));

        // The closure. `pollutantLabel` has a fallback that uppercases anything
        // it does not recognise, so a missing row produces a plausible string
        // rather than an empty one — which is why this compares against a name
        // written down independently instead of merely checking for non-empty.
        QVERIFY2(pollutantLabel(id) == expectedLabel(pollutant),
                 qPrintable(QStringLiteral("pollutantLabel(\"%1\") is \"%2\", expected \"%3\"")
                                .arg(id, pollutantLabel(id), expectedLabel(pollutant))));

        // And no machine spelling survives into anything a reader sees. The
        // original defect was one character wide: "PM2_5" for "PM2.5".
        QVERIFY2(!pollutantLabel(id).contains(QLatin1Char('_')),
                 qPrintable(QStringLiteral("\"%1\" reached the screen for %2")
                                .arg(pollutantLabel(id), id)));
    }
}

void TestScales::theSubscriptsAreRealCharactersAndNotMarkup()
{
    // "Subscripts as real characters rather than as rich text, because these go
    // into a QML Text with no styled-text parsing" — scales.h. A `<sub>` here
    // renders literally.
    for (int i = 0; i < int(Pollutant::Count); ++i) {
        const QString label = pollutantLabel(pollutantId(static_cast<Pollutant>(i)));
        QVERIFY2(!label.contains(QLatin1Char('<')),
                 qPrintable(QStringLiteral("\"%1\" carries markup").arg(label)));
    }

    // The two that carry a subscript, by codepoint. U+2082 and U+2083, not a
    // digit and not a `<sub>`.
    QCOMPARE(pollutantLabel(pollutantId(Pollutant::Ozone)), QStringLiteral("O\u2083"));
    QCOMPARE(pollutantLabel(pollutantId(Pollutant::NitrogenDioxide)),
             QStringLiteral("NO\u2082"));

    // The formula spellings are accepted too. The wire sends whatever
    // pollutantId() produced, but a widget reading a snapshot written by an
    // older or newer Clima should still get a chemist's name rather than a
    // shout, and both spellings costing one row each is cheaper than a
    // migration.
    QCOMPARE(pollutantLabel(QStringLiteral("o3")), QStringLiteral("O\u2083"));
    QCOMPARE(pollutantLabel(QStringLiteral("no2")), QStringLiteral("NO\u2082"));
    QCOMPARE(pollutantLabel(QStringLiteral("so2")), QStringLiteral("SO\u2082"));
    QCOMPARE(pollutantLabel(QStringLiteral("co")), QStringLiteral("CO"));
}

void TestScales::anUnknownPollutantIsUppercasedRatherThanDropped()
{
    // "Unknown ids come back uppercased, which is the old behaviour and the
    // only honest answer for a species we have no name for" — scales.h. A
    // species this build has never heard of still has to appear on the card;
    // an empty label would read as "no dominant pollutant", which is a
    // different and untrue statement.
    QCOMPARE(pollutantLabel(QStringLiteral("nh3")), QStringLiteral("NH3"));
    QCOMPARE(pollutantLabel(QStringLiteral("benzene")), QStringLiteral("BENZENE"));

    // Case-insensitive on the way in, because the id may have been through a
    // JSON round trip or a settings file.
    QCOMPARE(pollutantLabel(QStringLiteral("PM2_5")), QStringLiteral("PM2.5"));
    QCOMPARE(pollutantLabel(QStringLiteral("Ozone")), QStringLiteral("O\u2083"));

    // Empty in, empty out. `QString().toUpper()` is empty, so this falls out of
    // the fallback rather than being handled — asserted so it stays that way,
    // because the caller uses an empty label to mean "no breakdown here".
    QCOMPARE(pollutantLabel(QString()), QString());
}

// ============================================================================
// And the rule all four string tables share.
// ============================================================================

void TestScales::noBandNamesAReadingThatDoesNotExist()
{
    // scales.h's opening paragraph, asserted. Every one of these is reachable:
    // `value(reading)` hands NaN straight through for an absent Reading, and
    // ECMWF genuinely omits variables at some coordinates — see
    // tests/fixtures/openmeteo/toronto-ecmwf-gaps.json.
    QVERIFY(uvBand(kNaN).isEmpty());
    QVERIFY(aqiBand(kNaN).isEmpty());
    QVERIFY(visibilityBand(kNaN).isEmpty());
    QVERIFY(compassPoint(kNaN).isEmpty());
}

QTEST_MAIN(TestScales)
#include "tst_scales.moc"
