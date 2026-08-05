// SPDX-FileCopyrightText: 2026 Jowi Aoun
// SPDX-License-Identifier: GPL-3.0-or-later
//
// The canary for the golden images.
//
// A golden image is a claim that a scene renders to exact bytes. When that
// claim fails there are two possible causes and they want opposite responses:
// somebody changed the drawing, or somebody changed the *machine*. Told apart,
// the first is a bug or an intended edit and the second is a re-baseline. Not
// told apart, a FreeType upgrade produces thirty image diffs and a morning
// spent staring at pictures that differ by one pixel each.
//
// So this test asserts the environment before any image is compared, and it is
// ordered first for that reason. It measures the handful of numbers that decide
// where a glyph lands, and if any of them has moved it says so — once, in
// words, naming the number.
//
// What it deliberately does not do is assert a *good* value. There is no
// correct advance width; there is only the one the recorded images were taken
// at. The expected numbers live in tests/golden/environment.json beside the
// PNGs they belong to, and both are regenerated together.
#include <QFont>
#include <QFontDatabase>
#include <QFontMetricsF>
#include <QGuiApplication>
#include <QJsonDocument>
#include <QJsonObject>
#include <QTest>

#include "appfont.h"

namespace {

// One line of the app's own text, in the app's own sizes. A pangram would
// exercise more glyphs; this exercises the ones that are actually on screen,
// including the degree sign and the digits that every reading is made of.
constexpr auto sampleText = "Partly cloudy, 27° · Feels like 24° · 9 km/h";

// The type sizes theme.js declares, from the smallest label to the hero. A
// rasteriser change rarely moves every size by the same amount, so measuring
// several is what turns "something moved" into "the small sizes moved".
const QList<int> sampleSizes = { 11, 12, 13, 15, 17, 22, 44, 72 };

QString fingerprintPath()
{
    return QStringLiteral(CLIMA_SOURCE_DIR "/tests/golden/environment.json");
}

} // namespace

class TestEnvironment : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void initTestCase();
    void theFontIsOurs();
    void theMetricsAreWhatTheGoldensWereTakenAt();
    void thereIsNoDevicePixelRatio();

private:
    QJsonObject measure() const;

    QString m_family;
};

void TestEnvironment::initTestCase()
{
    m_family = AppFont::install();
}

// If this fails nothing below it means anything: every measurement would be of
// whatever face fontconfig picked instead.
void TestEnvironment::theFontIsOurs()
{
    QVERIFY2(!m_family.isEmpty(), "the bundled font did not install");
    QCOMPARE(QGuiApplication::font().family(), m_family);

    // The name comes out of the font files rather than a string typed here, so
    // this asserts the relationship rather than the spelling: whatever the
    // files declare is what the application is set to.
    QVERIFY2(QFontDatabase::families().contains(m_family),
             qPrintable(QStringLiteral("%1 is not in the font database").arg(m_family)));
}

QJsonObject TestEnvironment::measure() const
{
    QJsonObject sizes;
    for (int pixelSize : sampleSizes) {
        QFont font(m_family);
        font.setPixelSize(pixelSize);

        const QFontMetricsF metrics(font);
        QJsonObject row;
        // Six decimal places: the difference between a hinted and an unhinted
        // rasteriser showed up in the third (341.266 against 336), and rounding
        // to integers here would hide exactly the drift this exists to catch.
        row["advance"] = QString::number(metrics.horizontalAdvance(QString::fromUtf8(sampleText)), 'f', 6);
        row["height"]  = QString::number(metrics.height(), 'f', 6);
        row["ascent"]  = QString::number(metrics.ascent(), 'f', 6);
        sizes[QString::number(pixelSize)] = row;
    }

    QJsonObject out;
    out["family"] = m_family;
    out["sizes"]  = sizes;
    return out;
}

void TestEnvironment::theMetricsAreWhatTheGoldensWereTakenAt()
{
    const QJsonObject actual = measure();

    // The bootstrap path, and the only way this file is ever written. Running
    // the test with CLIMA_WRITE_FINGERPRINT=1 records the current machine as
    // the reference — which is a thing to do deliberately, inside the pinned
    // container, at the same time as re-recording the images, and never as a
    // way of making a red test go green.
    if (!qEnvironmentVariableIsEmpty("CLIMA_WRITE_FINGERPRINT")) {
        QFile file(fingerprintPath());
        QVERIFY2(file.open(QIODevice::WriteOnly | QIODevice::Text),
                 qPrintable(fingerprintPath()));
        file.write(QJsonDocument(actual).toJson(QJsonDocument::Indented));
        QSKIP("wrote tests/golden/environment.json; re-run without CLIMA_WRITE_FINGERPRINT");
    }

    QFile file(fingerprintPath());
    QVERIFY2(file.open(QIODevice::ReadOnly),
             qPrintable(QStringLiteral("cannot read %1 — run once with "
                                       "CLIMA_WRITE_FINGERPRINT=1 to record it")
                            .arg(fingerprintPath())));

    const QJsonObject expected = QJsonDocument::fromJson(file.readAll()).object();
    QVERIFY2(!expected.isEmpty(), "tests/golden/environment.json is empty or not JSON");

    if (expected["family"].toString() != actual["family"].toString()) {
        QFAIL(qPrintable(QStringLiteral(
            "the application font is \"%1\" but the golden images were taken in \"%2\".")
                             .arg(actual["family"].toString(), expected["family"].toString())));
    }

    const QJsonObject expectedSizes = expected["sizes"].toObject();
    const QJsonObject actualSizes   = actual["sizes"].toObject();
    QStringList drift;

    for (const QString &size : expectedSizes.keys()) {
        const QJsonObject want = expectedSizes[size].toObject();
        const QJsonObject got  = actualSizes[size].toObject();
        for (const QString &metric : want.keys()) {
            if (want[metric].toString() != got[metric].toString()) {
                drift.append(QStringLiteral("  %1px %2: recorded %3, measured %4")
                                 .arg(size, metric, want[metric].toString(),
                                      got[metric].toString()));
            }
        }
    }

    if (!drift.isEmpty()) {
        QFAIL(qPrintable(
            QStringLiteral(
                "text is rasterising differently than when the golden images were recorded.\n"
                "%1\n"
                "Every golden image will differ, and none of those differences is a bug in\n"
                "this application. The usual cause is a FreeType or fontconfig change, or a\n"
                "capture run without FONTCONFIG_FILE=tests/golden/fontconfig.conf. Re-record\n"
                "inside the pinned container rather than accepting the diffs one by one.")
                .arg(drift.join(QLatin1Char('\n')))));
    }
}

// QT_SCALE_FACTOR and its three siblings are exported by scripts/grab.sh, and a
// capture at ratio 2 is not a failed comparison so much as a meaningless one:
// every image is twice the size and nothing lines up. Cheap to assert, and it
// names the cause rather than leaving thirty dimension mismatches.
void TestEnvironment::thereIsNoDevicePixelRatio()
{
    QCOMPARE(qGuiApp->devicePixelRatio(), 1.0);
}

QTEST_MAIN(TestEnvironment)

#include "tst_environment.moc"
