// SPDX-FileCopyrightText: 2026 Jowi Aoun
// SPDX-License-Identifier: GPL-3.0-or-later

#include "appoptions.h"

#include <QCommandLineOption>
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
// Everything else is CLIMA_DEV_TOOLS-only. --film, --poke, --walk and --scroll
// drive the capture harness; --sky, --metric, --day, --list and --tab put the
// app into a state a screenshot wants to catch; --gallery, --card and --details
// open one of the three preview modes, and those three are on their way out of
// this executable entirely — the component gallery becomes its own binary, at
// which point they leave with it.
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

// Every numeric flag in this parser wants the same three things: the value has
// to be a whole number, it has to clear a floor, and a failure has to name the
// flag rather than the number. Written once because it was written five times
// in the QML and two of the five got the NaN check subtly wrong — parseInt()
// coerces to 0 on the way into an int property, so a check made after the
// assignment was testing the coercion rather than the input.
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
    return { QStringLiteral("mobile"), QStringLiteral("tablet"), QStringLiteral("desktop") };
}

QStringList AppOptions::skyPhases()
{
    return { QStringLiteral("night"), QStringLiteral("dawn"),
             QStringLiteral("day"), QStringLiteral("dusk") };
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
                       "Targets: metric, day, list, feels, scroll, tab, flick, remount."),
        QStringLiteral("target=value"));
    parser.addOption(pokeOption);

    // ---- opening state -----------------------------------------------------
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

    // ---- previews ----------------------------------------------------------
    const QCommandLineOption galleryOption(
        QStringLiteral("gallery"),
        QStringLiteral("Open the component library instead of the app. A component name may "
                       "follow, unquoted."));
    parser.addOption(galleryOption);

    const QCommandLineOption walkOption(
        QStringLiteral("walk"),
        QStringLiteral("With --gallery: step <n> components on before grabbing, so a headless "
                       "check exercises navigation and not only first paint."),
        QStringLiteral("n"));
    parser.addOption(walkOption);

    const QCommandLineOption cardOption(
        QStringLiteral("card"),
        QStringLiteral("Render one detail card alone on the page gradient, e.g. Uv."),
        QStringLiteral("name"));
    parser.addOption(cardOption);

    const QCommandLineOption detailsOption(
        QStringLiteral("details"), QStringLiteral("Render the weather-details grid on its own."));
    parser.addOption(detailsOption);

    parser.addPositionalArgument(QStringLiteral("component"),
                                 QStringLiteral("With --gallery: which component to show. Every "
                                                "word up to the next flag, so `--gallery weather "
                                                "glyph` works without quotes."),
                                 QStringLiteral("[component…]"));
#endif // CLIMA_DEV_TOOLS

    // Handles --help and --version, and exits on an unknown flag. That last one
    // is the behaviour change worth naming: `--vieport mobile` used to open the
    // desktop shell and say nothing.
    parser.process(app);

    // ---- shipped -----------------------------------------------------------
    if (parser.isSet(grabOption))
        self->m_grab = parser.value(grabOption);

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
    if (parser.isSet(tabOption))
        self->m_tab = parser.value(tabOption);

    if (parser.isSet(skyOption)) {
        const QString phase = parser.value(skyOption);
        if (!skyPhases().contains(phase))
            fail(QStringLiteral("--sky: expected one of %1 — got \"%2\"")
                     .arg(skyPhases().join(QStringLiteral(", ")), phase));
        self->m_sky = phase;
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

    // ---- previews ----------------------------------------------------------
    if (parser.isSet(cardOption))
        self->m_card = parser.value(cardOption);
    self->m_details = parser.isSet(detailsOption);
    self->m_gallery = parser.isSet(galleryOption);

    if (parser.isSet(walkOption))
        self->m_walk = requireInt(parser, walkOption, 0, "a count >= 0");

    // The only positional arguments this program takes are the gallery's, so a
    // stray word without --gallery is a typo — most likely a flag that lost its
    // dashes. Saying so beats opening the forecast and ignoring it.
    const QStringList words = parser.positionalArguments();
    if (!words.isEmpty() && !self->m_gallery)
        fail(QStringLiteral("unexpected argument \"%1\" — a component name is only "
                            "meaningful with --gallery")
                 .arg(words.constFirst()));
    self->m_galleryPick = words.join(QLatin1Char(' '));
#endif // CLIMA_DEV_TOOLS
}
