// SPDX-FileCopyrightText: 2026 Jowi Aoun
// SPDX-License-Identifier: GPL-3.0-or-later

#include "appoptions.h"

#include "libclima/providers/fixture/fixtureprovider.h"

#include <QCommandLineOption>
#include <QProcessEnvironment>
#include <QCommandLineParser>
#include <QCoreApplication>
#include <QRegularExpression>

#include <cstdio>
#include <cstdlib>

namespace {

// ---- FLAG DISPOSITION -------------------------------------------------------
//
// Three flags ship in the packaged binary:
//
//   --grab --size --viewport
//
// They are small, they cost nothing to carry, and they are how a bug report
// gets a screenshot: the issue template says "attach `clima --grab bug.png`",
// and a user cannot do that with a flag that only exists in a developer build.
//
// Everything else is CLIMA_DEV_TOOLS-only. --film, --poke and --scroll drive
// the capture harness; --sky, --metric, --day, --list and --tab put the app
// into a state a screenshot wants to catch.
//
// --gallery, --card and --details are gone rather than conditional. They opened
// the three preview modes, the preview modes are `clima-gallery` now, and a
// weather app that still answered to `--gallery` by opening a component library
// would be the same mistake in a smaller font.
//
// The properties on AppOptions exist in every build regardless; see the header.

// A parse failure that is ours rather than QCommandLineParser's — a value with
// the wrong shape, as opposed to a flag that does not exist. Prints in the same
// voice the parser uses for its own errors and exits with the same code, so a
// script cannot tell the two apart and does not have to.
[[noreturn]] void fail(const QString &message)
{
    const QString name = QCoreApplication::applicationName();
    std::fprintf(stderr, "%s: %s\n", qPrintable(name), qPrintable(message));
    std::fprintf(stderr, "Try `%s --help` for the full list of options.\n", qPrintable(name));
    std::exit(EXIT_FAILURE);
}

#ifdef CLIMA_DEV_TOOLS
// Not an error: the run continues and does something reasonable. Said out loud
// anyway, because the thing it is reporting is a flag NOT taking effect, and a
// default that quietly declines to apply is indistinguishable from one that was
// never there.
//
// stderr rather than qWarning, to match fail() above: these two are the file's
// whole diagnostic vocabulary and they should look alike, and neither should be
// filterable by a logging category that nobody has configured.
//
// Guarded, because the only caller is the --place reconciliation and --place is
// a dev-tools flag; a packaged build compiles this to nothing and would
// otherwise carry an unused-function warning for it.
void note(const QString &message)
{
    std::fprintf(stderr, "%s: %s\n", qPrintable(QCoreApplication::applicationName()),
                 qPrintable(message));
}
#endif // CLIMA_DEV_TOOLS

#ifdef CLIMA_DEV_TOOLS
// Every numeric flag in this parser wants the same three things: the value has
// to be a whole number, it has to clear a floor, and a failure has to name the
// flag rather than the number. Written once because it was written five times
// in the QML and two of the five got the NaN check subtly wrong — parseInt()
// coerces to 0 on the way into an int property, so a check made after the
// assignment was testing the coercion rather than the input.
//
// Guarded, because every flag that takes a number — --frames, --every, --day —
// is a dev-tools flag, so a packaged build compiles this to nothing and would
// otherwise carry an unused-function warning for it.
int requireInt(const QCommandLineParser &parser, const QCommandLineOption &option,
               int minimum, const char *what)
{
    const QString raw = parser.value(option);
    bool ok = false;
    const int value = raw.toInt(&ok);
    if (!ok || value < minimum)
        fail(QStringLiteral("--%1: expected %2, got \"%3\"")
                 .arg(option.names().constFirst(), QString::fromUtf8(what), raw));
    return value;
}
#endif // CLIMA_DEV_TOOLS

AppOptions *g_instance = nullptr;

} // namespace

AppOptions::AppOptions() = default;

AppOptions *AppOptions::instance()
{
    // Leaked on purpose, in the way a singleton is: it outlives the QML engine
    // that reads it and there is nothing after main() that could observe the
    // difference. Deleting it at exit would only add an ordering problem
    // between this and the engine.
    if (g_instance == nullptr)
        g_instance = new AppOptions;
    return g_instance;
}

AppOptions *AppOptions::create(QQmlEngine *qmlEngine, QJSEngine *jsEngine)
{
    Q_UNUSED(qmlEngine)
    Q_UNUSED(jsEngine)
    AppOptions *options = instance();
    QJSEngine::setObjectOwnership(options, QJSEngine::CppOwnership);
    return options;
}

// The CLI's vocabulary, and only that. The pixel sizes behind these ids are in
// Viewports.qml and the gradients behind those phases are in Theme.qml; neither
// table is duplicated here, and neither is reachable from C++ before the engine
// exists — which is precisely when the command line has to be parsed.
QStringList AppOptions::viewportIds()
{
    return { QStringLiteral("mobile"), QStringLiteral("tablet"),
             QStringLiteral("tablet-landscape"), QStringLiteral("desktop") };
}

QStringList AppOptions::skyPhases()
{
    return { QStringLiteral("night"), QStringLiteral("dawn"),
             QStringLiteral("day"), QStringLiteral("dusk") };
}

QStringList AppOptions::schemes()
{
    return { QStringLiteral("dark"), QStringLiteral("light") };
}

void AppOptions::parseCommandLine(const QCoreApplication &app)
{
    AppOptions *self = instance();

    QCommandLineParser parser;
    parser.setApplicationDescription(
        QStringLiteral("Clima — a native Qt 6 weather app.\n"
                       "\n"
                       "With no options it opens the forecast. The window's width chooses the\n"
                       "layout: a phone gets five tabs under a nav bar, a desktop gets one\n"
                       "scrolling column."));
    parser.addHelpOption();
    parser.addVersionOption();

    // ---- shipped -----------------------------------------------------------
    const QCommandLineOption grabOption(
        QStringLiteral("grab"),
        QStringLiteral("Render one settled frame to <file> and exit. Attach the result to a "
                       "bug report."),
        QStringLiteral("file"));
    parser.addOption(grabOption);

    const QCommandLineOption fixtureOption(
        QStringLiteral("fixture"),
        QStringLiteral("Replay a recorded forecast at the instant it was recorded, instead of "
                       "fetching one: %1. Use \"off\" to force the live network. Defaults to "
                       "\"%2\" under --grab and --film, and to the live network otherwise; "
                       "CLIMA_FIXTURE sets it for a whole session.")
            .arg(clima::fixtures::names().join(QStringLiteral(", ")), clima::fixtures::defaultName()),
        QStringLiteral("name"));
    parser.addOption(fixtureOption);

    const QCommandLineOption sizeOption(
        QStringLiteral("size"),
        QStringLiteral("Open the window at <WxH> pixels, e.g. 1340x900."),
        QStringLiteral("WxH"));
    parser.addOption(sizeOption);

    const QCommandLineOption viewportOption(
        QStringLiteral("viewport"),
        QStringLiteral("Pin the layout to a device preset and size the window to match: %1.")
            .arg(viewportIds().join(QStringLiteral(", "))),
        QStringLiteral("id"));
    parser.addOption(viewportOption);

#ifdef CLIMA_DEV_TOOLS
    // ---- filming -----------------------------------------------------------
    const QCommandLineOption filmOption(
        QStringLiteral("film"),
        QStringLiteral("Write <prefix>-00.png, <prefix>-01.png … and exit. A still frame "
                       "cannot show a transition; this can."),
        QStringLiteral("prefix"));
    parser.addOption(filmOption);

    const QCommandLineOption framesOption(
        QStringLiteral("frames"), QStringLiteral("How many frames --film writes (default 8)."),
        QStringLiteral("count"));
    parser.addOption(framesOption);

    const QCommandLineOption everyOption(
        QStringLiteral("every"), QStringLiteral("Milliseconds between filmed frames (default 60)."),
        QStringLiteral("ms"));
    parser.addOption(everyOption);

    const QCommandLineOption pokeOption(
        QStringLiteral("poke"),
        QStringLiteral("Apply <target>=<value> once the scene has settled; repeatable. "
                       "Targets: metric, day, list, feels, scroll, tab, flick."),
        QStringLiteral("target=value"));
    parser.addOption(pokeOption);

    // ---- opening state -----------------------------------------------------
    const QCommandLineOption placeOption(
        QStringLiteral("place"),
        QStringLiteral("Look <query> up in the geocoder and open on the first match, e.g. "
                       "\"Kigali\". Saves it like the picker would."),
        QStringLiteral("query"));
    parser.addOption(placeOption);

    const QCommandLineOption tabOption(
        QStringLiteral("tab"), QStringLiteral("Open the mobile shell on a given tab."),
        QStringLiteral("id"));
    parser.addOption(tabOption);

    const QCommandLineOption skyOption(
        QStringLiteral("sky"),
        QStringLiteral("Force the time-of-day background and turn the star field on "
                       "anywhere: %1.")
            .arg(skyPhases().join(QStringLiteral(", "))),
        QStringLiteral("phase"));
    parser.addOption(skyOption);

    const QCommandLineOption schemeOption(
        QStringLiteral("scheme"),
        QStringLiteral("Force the colour scheme rather than following the desktop: %1. "
                       "Every capture needs this, because a screenshot that follows whichever "
                       "theme the machine happened to be in is not a screenshot of anything.")
            .arg(schemes().join(QStringLiteral(", "))),
        QStringLiteral("name"));
    parser.addOption(schemeOption);

    const QCommandLineOption metricOption(
        QStringLiteral("metric"), QStringLiteral("Select a chart metric, e.g. wind or uv."),
        QStringLiteral("id"));
    parser.addOption(metricOption);

    const QCommandLineOption dayOption(
        QStringLiteral("day"), QStringLiteral("Select a day in the strip, counting from 0."),
        QStringLiteral("index"));
    parser.addOption(dayOption);

    const QCommandLineOption listOption(
        QStringLiteral("list"), QStringLiteral("Open the hourly section as a list rather than a chart."));
    parser.addOption(listOption);

    const QCommandLineOption scrollOption(
        QStringLiteral("scroll"),
        QStringLiteral("Scroll the page down <px> before grabbing. The details grid is below "
                       "the fold at every size that fits on a laptop."),
        QStringLiteral("px"));
    parser.addOption(scrollOption);

#endif // CLIMA_DEV_TOOLS

    // Handles --help and --version, and exits on an unknown flag. That last one
    // is the behaviour change worth naming: `--vieport mobile` used to open the
    // desktop shell and say nothing.
    parser.process(app);

    // ---- shipped -----------------------------------------------------------
    if (parser.isSet(grabOption))
        self->m_grab = parser.value(grabOption);

    if (parser.isSet(fixtureOption)) {
        const QString name = parser.value(fixtureOption);
        if (name != QLatin1String("off") && !clima::fixtures::exists(name)) {
            fail(QStringLiteral("unknown fixture \"%1\" — try one of: %2, or \"off\"")
                     .arg(name, clima::fixtures::names().join(QStringLiteral(", "))));
        }
        self->m_fixture = name;
    }

    if (parser.isSet(sizeOption)) {
        // Anchored, so 1340x900x2 and 1340xfoo are rejected rather than
        // half-read. The QML this replaces used parseInt(), which stops at the
        // first non-digit and read "390abc" as 390.
        static const QRegularExpression shape(QStringLiteral("^([0-9]+)x([0-9]+)$"));
        const QRegularExpressionMatch match = shape.match(parser.value(sizeOption));
        const int w = match.hasMatch() ? match.captured(1).toInt() : 0;
        const int h = match.hasMatch() ? match.captured(2).toInt() : 0;
        if (w <= 0 || h <= 0)
            fail(QStringLiteral("--size: expected WxH in pixels, got \"%1\"")
                     .arg(parser.value(sizeOption)));
        self->m_sizeWidth  = w;
        self->m_sizeHeight = h;
    }

    if (parser.isSet(viewportOption)) {
        const QString id = parser.value(viewportOption);
        if (!viewportIds().contains(id))
            fail(QStringLiteral("--viewport: expected one of %1 — got \"%2\"")
                     .arg(viewportIds().join(QStringLiteral(", ")), id));
        self->m_viewport = id;
    }

#ifdef CLIMA_DEV_TOOLS
    // ---- filming -----------------------------------------------------------
    if (parser.isSet(framesOption))
        self->m_frames = requireInt(parser, framesOption, 1, "a count > 0");
    if (parser.isSet(everyOption))
        self->m_every = requireInt(parser, everyOption, 1, "milliseconds > 0");
    if (parser.isSet(filmOption))
        self->m_film = parser.value(filmOption);

    // Shape only. Whether `metric` is a target and whether `uv` is a metric are
    // questions for the moment the poke is applied, because that is the first
    // moment anything knows which shell is running — and under --gallery there
    // is no shell at all.
    for (const QString &poke : parser.values(pokeOption)) {
        if (!poke.contains(QLatin1Char('=')))
            fail(QStringLiteral("--poke: expected target=value, got \"%1\"").arg(poke));
        self->m_pokes.append(poke);
    }

    // ---- opening state -----------------------------------------------------
    if (parser.isSet(placeOption))
        self->m_place = parser.value(placeOption);
    if (parser.isSet(tabOption))
        self->m_tab = parser.value(tabOption);

    // ---- --place against the fixture rules ---------------------------------
    //
    // A fixture is one recorded place and answers with that place's weather
    // whatever coordinate it is handed. So a fixture and a named place are two
    // answers to the same question, and the header on fixture() sets out which
    // one wins where. Two of the three cases are settled here, at parse time,
    // because this is where both values are known and where saying something
    // still reaches a human.
    if (!self->m_place.isEmpty()) {
        // Both stated outright. There is no reading of this that gives the user
        // both things they asked for, and picking one silently is how the
        // original bug worked. Refused, with the two ways to say what was
        // probably meant.
        if (!self->m_fixture.isEmpty() && self->m_fixture != QLatin1String("off")) {
            fail(QStringLiteral("--place %1 and --fixture %2 ask for two different forecasts: "
                                "a fixture replays one recorded place and cannot answer for "
                                "another.\n"
                                "Drop --fixture to fetch %1 live, or drop --place to replay "
                                "the recording.")
                     .arg(self->m_place, self->m_fixture));
        }

        // Implied rather than stated, so the place wins — but the capture that
        // results is a live one, which means it is a picture of this afternoon
        // and will not compare against another taken tomorrow. That is worth a
        // line, because reproducibility is the entire reason the default exists.
        if (self->m_fixture.isEmpty()) {
            const QString session =
                QProcessEnvironment::systemEnvironment().value(QStringLiteral("CLIMA_FIXTURE"));
            const bool sessionFixture =
                !session.isEmpty() && session != QLatin1String("off")
                && clima::fixtures::exists(session);

            if (sessionFixture) {
                note(QStringLiteral("--place %1 overrides CLIMA_FIXTURE=%2 for this run; "
                                    "fetching live.")
                         .arg(self->m_place, session));
            } else if (self->capturing()) {
                note(QStringLiteral("--place %1 is a live fetch, so this capture is not "
                                    "reproducible. Drop --place for the recorded forecast "
                                    "that --grab and --film default to.")
                         .arg(self->m_place));
            }
        }
    }

    if (parser.isSet(skyOption)) {
        const QString phase = parser.value(skyOption);
        if (!skyPhases().contains(phase))
            fail(QStringLiteral("--sky: expected one of %1 — got \"%2\"")
                     .arg(skyPhases().join(QStringLiteral(", ")), phase));
        self->m_sky = phase;
    }

    if (parser.isSet(schemeOption)) {
        const QString name = parser.value(schemeOption);
        if (!schemes().contains(name))
            fail(QStringLiteral("--scheme: expected one of %1 — got \"%2\"")
                     .arg(schemes().join(QStringLiteral(", ")), name));
        self->m_scheme = name;
    }

    if (parser.isSet(metricOption))
        self->m_metric = parser.value(metricOption);
    if (parser.isSet(dayOption))
        self->m_day = requireInt(parser, dayOption, 0, "an index >= 0");
    self->m_list = parser.isSet(listOption);

    if (parser.isSet(scrollOption)) {
        const QString raw = parser.value(scrollOption);
        bool ok = false;
        const double distance = raw.toDouble(&ok);
        if (!ok || distance < 0)
            fail(QStringLiteral("--scroll: expected a distance >= 0, got \"%1\"").arg(raw));
        self->m_scroll = distance;
    }

#endif // CLIMA_DEV_TOOLS

    // This program takes no positional arguments at all — it took one, and that
    // one was the gallery's component name, which left with the gallery. A
    // stray word is therefore a typo, most likely a flag that lost its dashes,
    // and saying so beats opening the forecast and ignoring it.
    //
    // Outside the CLIMA_DEV_TOOLS block because it is not a dev-tools rule: a
    // packaged `clima Toronto` should be told that this is not how you pick a
    // location either.
    const QStringList words = parser.positionalArguments();
    if (!words.isEmpty())
        fail(QStringLiteral("unexpected argument \"%1\" — clima takes options, not arguments")
                 .arg(words.constFirst()));
}

// ---- which data this run uses ---------------------------------------------------
//
// The resolution order is in the header. It is three lines of code and the
// reason it is not inlined at the call site is that there are two call sites —
// app/main.cpp and gallery/main.cpp — and a gallery that resolved it differently
// would review components against data the app never shows.
QString AppOptions::fixture() const
{
    // Rule 1. Explicit, and it wins outright. A --place alongside it was
    // rejected at parse time, so there is no conflict left to resolve here.
    if (m_fixture == QLatin1String("off"))
        return {};
    if (!m_fixture.isEmpty())
        return m_fixture;

    // Rules 2 and 3 are both *implied* fixtures, and an implied fixture loses
    // to a named place — see the header. The place is the more specific
    // instruction and the only one of the two that a recording cannot honour.
    const bool named = !m_place.isEmpty();

    const QString fromEnvironment =
        QProcessEnvironment::systemEnvironment().value(QStringLiteral("CLIMA_FIXTURE"));
    if (fromEnvironment == QLatin1String("off"))
        return {};
    if (!fromEnvironment.isEmpty() && clima::fixtures::exists(fromEnvironment))
        return named ? QString() : fromEnvironment;

    // A capture defaults to the recording. Not because a capture is a test, but
    // because a capture is a picture that will be compared with another picture,
    // and two pictures of two different afternoons compare as a diff in every
    // pixel.
    if (capturing())
        return named ? QString() : clima::fixtures::defaultName();

    return {};
}
