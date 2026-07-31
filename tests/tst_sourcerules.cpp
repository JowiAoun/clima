// SPDX-FileCopyrightText: 2026 Jowi Aoun
// SPDX-License-Identifier: GPL-3.0-or-later
//
// Two rules about the source tree, enforced by reading it.
//
// Both are rules that a comment in a header states and that nothing else would
// notice being broken. A comment is a request; a failing test is a rule.
//
//   1. NOTHING READS THE WALL CLOCK except libclima/core/clock.cpp.
//      The whole determinism story rests on this — fixture mode, golden
//      images, every TTL test in tst_cachestore.cpp — and the way it breaks is
//      not a bad commit but an ordinary one: somebody needs "now", writes
//      QDateTime::currentDateTime(), and it works. Nothing fails. The golden
//      image that starts flaking is six weeks away and names a chart.
//
//   2. NO TEST NAMES AN EXTERNAL HOST, except the one whose subject is that
//      it must not be reachable. This is the static half of the network
//      isolation rule; the runtime half is tests/support/networkguard.h.
//
// ---- how the scan handles comments ------------------------------------------
//
// Both rules are *about* strings that this codebase writes down in prose all
// the time — clock.h's header lists every banned spelling, and
// tst_networkisolation.cpp names two real weather APIs on purpose. So the
// scanner strips comments before matching: a line whose first non-space
// characters are `//`, `/*` or `*` is dropped, and anything from a `//` to the
// end of a line is dropped.
//
// That is a line-based approximation rather than a parser, and its one blind
// spot is a statement beginning with a dereference — `*out = ...`. It is
// documented rather than fixed because the alternative is a C++ lexer in a
// test, and because a real violation on such a line would still be caught by
// review, which is more than the rule had before.

#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QRegularExpression>
#include <QTest>
#include <QTextStream>

namespace {

struct Violation {
    QString file;
    int     line = 0;
    QString text;
    QString matched;
};

QString stripComments(const QString &line)
{
    const QString trimmed = line.trimmed();
    if (trimmed.startsWith(QLatin1String("//")) || trimmed.startsWith(QLatin1String("/*"))
        || trimmed.startsWith(QLatin1String("*")) || trimmed.startsWith(QLatin1String("#")))
        return {};

    const int comment = line.indexOf(QLatin1String("//"));
    if (comment >= 0)
        return line.left(comment);
    return line;
}

QStringList sourceFiles(const QString &root, const QStringList &subdirectories,
                        const QStringList &suffixes)
{
    QStringList found;
    for (const QString &subdirectory : subdirectories) {
        const QString path = root + QLatin1Char('/') + subdirectory;
        if (!QDir(path).exists())
            continue;
        QDirIterator it(path, suffixes, QDir::Files, QDirIterator::Subdirectories);
        while (it.hasNext())
            found.append(it.next());
    }
    found.sort();
    return found;
}

QList<Violation> scan(const QStringList &files, const QList<QRegularExpression> &patterns,
                      const QStringList &exemptBasenames)
{
    QList<Violation> violations;
    for (const QString &path : files) {
        if (exemptBasenames.contains(QFileInfo(path).fileName()))
            continue;

        QFile file(path);
        if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
            continue;

        QTextStream stream(&file);
        int         lineNumber = 0;
        while (!stream.atEnd()) {
            ++lineNumber;
            const QString code = stripComments(stream.readLine());
            if (code.trimmed().isEmpty())
                continue;

            for (const QRegularExpression &pattern : patterns) {
                const QRegularExpressionMatch match = pattern.match(code);
                if (match.hasMatch()) {
                    violations.append({ path, lineNumber, code.trimmed(), match.captured(0) });
                    break;
                }
            }
        }
    }
    return violations;
}

QString describe(const QList<Violation> &violations)
{
    QString text;
    for (const Violation &violation : violations) {
        text += QStringLiteral("\n    %1:%2  %3\n        %4")
                    .arg(violation.file)
                    .arg(violation.line)
                    .arg(violation.matched, violation.text);
    }
    return text;
}

} // namespace

class TestSourceRules : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void nothingReadsTheWallClock();
    void noTestNamesAnExternalHost();
    void theScannerActuallyFindsThings();
};

void TestSourceRules::nothingReadsTheWallClock()
{
    const QList<QRegularExpression> patterns = {
        QRegularExpression(QStringLiteral(R"(QDateTime::currentDateTime\b)")),
        QRegularExpression(QStringLiteral(R"(QDateTime::currentDateTimeUtc\b)")),
        QRegularExpression(QStringLiteral(R"(QDateTime::currentSecsSinceEpoch\b)")),
        QRegularExpression(QStringLiteral(R"(QDateTime::currentMSecsSinceEpoch\b)")),
        QRegularExpression(QStringLiteral(R"(QDate::currentDate\b)")),
        QRegularExpression(QStringLiteral(R"(QTime::currentTime\b)")),
        QRegularExpression(QStringLiteral(R"(\bDate\.now\s*\()")),
        QRegularExpression(QStringLiteral(R"(\bnew\s+Date\s*\()")),
    };

    const QStringList files =
        sourceFiles(QStringLiteral(CLIMA_SOURCE_DIR), { QStringLiteral("libclima"),
                                                        QStringLiteral("app"),
                                                        QStringLiteral("gallery") },
                    { QStringLiteral("*.cpp"), QStringLiteral("*.h"), QStringLiteral("*.qml"),
                      QStringLiteral("*.js") });

    QVERIFY2(files.size() > 50, "the source scan found almost nothing — CLIMA_SOURCE_DIR is wrong");

    // clock.cpp is the mechanism. It is the one file allowed to ask the
    // operating system what time it is, and everything else asks it.
    const QList<Violation> violations = scan(files, patterns, { QStringLiteral("clock.cpp") });

    QVERIFY2(violations.isEmpty(),
             qPrintable(QStringLiteral(
                            "something reads the wall clock directly.%1\n\n"
                            "  Inject a clima::Clock and call now() instead. libclima/core/clock.h "
                            "explains why:\n"
                            "  fixture mode, golden images and every TTL test depend on time being "
                            "asked for\n"
                            "  rather than assumed, and there is no `if (testing)` anywhere "
                            "downstream because of it.")
                            .arg(describe(violations))));
}

void TestSourceRules::noTestNamesAnExternalHost()
{
    // Any scheme-and-host that is not loopback. The runtime guard blocks these
    // anyway; this catches the fixture URL that got left in a test and is
    // failing for a reason nobody can see from the assertion.
    const QList<QRegularExpression> patterns = {
        QRegularExpression(QStringLiteral(R"(https?://(?!127\.0\.0\.1|localhost|\[::1\])[\w.-]+)")),
    };

    const QStringList files = sourceFiles(QStringLiteral(CLIMA_SOURCE_DIR),
                                          { QStringLiteral("tests") },
                                          { QStringLiteral("*.cpp"), QStringLiteral("*.h") });

    QVERIFY(!files.isEmpty());

    // The one file whose subject is that these hosts must not be reachable.
    const QList<Violation> violations =
        scan(files, patterns, { QStringLiteral("tst_networkisolation.cpp") });

    QVERIFY2(violations.isEmpty(),
             qPrintable(QStringLiteral("a test names a host outside this machine.%1\n\n"
                                       "  docs/04-architecture.md §4.11: no network in CI. Use "
                                       "tests/support/httpstub.h,\n"
                                       "  which serves canned responses from loopback.")
                            .arg(describe(violations))));
}

void TestSourceRules::theScannerActuallyFindsThings()
{
    // A scan that matches nothing passes both tests above whether or not the
    // tree is clean. This is the control: the same machinery, pointed at a
    // pattern that is definitely present, has to come back non-empty.
    const QStringList files = sourceFiles(QStringLiteral(CLIMA_SOURCE_DIR),
                                          { QStringLiteral("libclima") },
                                          { QStringLiteral("*.cpp") });
    QVERIFY(!files.isEmpty());

    const QList<Violation> found =
        scan(files, { QRegularExpression(QStringLiteral(R"(namespace clima)")) }, {});
    QVERIFY2(!found.isEmpty(), "the source scanner matched nothing at all — it is broken");

    // And comment stripping works, which is what keeps the two rules above from
    // failing on the paragraphs that describe them.
    QCOMPARE(stripComments(QStringLiteral("    // QDateTime::currentDateTime()")), QString());
    QCOMPARE(stripComments(QStringLiteral("     * Date.now() is banned")), QString());
    QCOMPARE(stripComments(QStringLiteral("int x = 1; // Date.now()")),
             QStringLiteral("int x = 1; "));
}

QTEST_MAIN(TestSourceRules)
#include "tst_sourcerules.moc"
