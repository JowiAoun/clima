// SPDX-FileCopyrightText: 2026 Jowi Aoun
// SPDX-License-Identifier: GPL-3.0-or-later

#include "galleryoptions.h"

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
// Nothing here ships, because this binary does not ship. The CLIMA_DEV_TOOLS
// split is honoured anyway, and for one reason worth stating: a gallery built
// with the dev tools compiled out must fail the same way the app does rather
// than a new way. `--walk` and `--poke` reach into ScreenshotController's
// dev-tools half; offering the flags in a build where that half is not there
// would mean a flag that parses, validates, and does nothing.
//
// So: --grab, --size, --viewport, --sky, --card, --details and the component
// name are always available; --film, --frames, --every, --poke and --walk go
// with CLIMA_DEV_TOOLS.

[[noreturn]] void fail(const QString &message)
{
    const QString name = QCoreApplication::applicationName();
    std::fprintf(stderr, "%s: %s\n", qPrintable(name), qPrintable(message));
    std::fprintf(stderr, "Try `%s --help` for the full list of options.\n", qPrintable(name));
    std::exit(EXIT_FAILURE);
}

// Guarded for the same reason appoptions.cpp guards its twin: --frames, --every
// and --walk are the only flags here that take a number and all three are
// dev-tools flags, so without this a CLIMA_DEV_TOOLS=OFF build carries an
// unused-function warning.
#ifdef CLIMA_DEV_TOOLS
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

GalleryOptions *g_instance = nullptr;

} // namespace

GalleryOptions::GalleryOptions() = default;

GalleryOptions *GalleryOptions::instance()
{
    if (g_instance == nullptr)
        g_instance = new GalleryOptions;
    return g_instance;
}

GalleryOptions *GalleryOptions::create(QQmlEngine *qmlEngine, QJSEngine *jsEngine)
{
    Q_UNUSED(qmlEngine)
    Q_UNUSED(jsEngine)
    GalleryOptions *options = instance();
    QJSEngine::setObjectOwnership(options, QJSEngine::CppOwnership);
    return options;
}

void GalleryOptions::parseCommandLine(const QCoreApplication &app)
{
    GalleryOptions *self = instance();

    QCommandLineParser parser;
    parser.setApplicationDescription(
        QStringLiteral("Clima's component gallery — every component in the app, on one\n"
                       "screen, on the gradient it is actually composited over.\n"
                       "\n"
                       "It exists because almost every defect found in this design so far was\n"
                       "invisible in the code and obvious in a render, and because a component\n"
                       "is easiest to get wrong in the states no current screen happens to use."));
    parser.addHelpOption();
    parser.addVersionOption();

    const QCommandLineOption grabOption(
        QStringLiteral("grab"),
        QStringLiteral("Render one settled frame to <file> and exit."),
        QStringLiteral("file"));
    parser.addOption(grabOption);

    const QCommandLineOption sizeOption(
        QStringLiteral("size"),
        QStringLiteral("Open the window at <WxH> pixels, e.g. 1500x950."),
        QStringLiteral("WxH"));
    parser.addOption(sizeOption);

    // The one flag whose meaning differs from the app's, and the help text says
    // so rather than leaving the reader to find out from a screenshot.
    const QCommandLineOption viewportOption(
        QStringLiteral("viewport"),
        QStringLiteral("Stage every specimen inside a device frame of this size, with the "
                       "page gradient painted inside it: %1. The window is not resized — the "
                       "rail alone is 232 px wide.")
            .arg(AppOptions::viewportIds().join(QStringLiteral(", "))),
        QStringLiteral("id"));
    parser.addOption(viewportOption);

    const QCommandLineOption skyOption(
        QStringLiteral("sky"),
        QStringLiteral("Force the sky a phone frame is painted with, rather than reading it "
                       "off the mock clock: %1.")
            .arg(AppOptions::skyPhases().join(QStringLiteral(", "))),
        QStringLiteral("phase"));
    parser.addOption(skyOption);

    const QCommandLineOption cardOption(
        QStringLiteral("card"),
        QStringLiteral("Render one detail card alone on the page gradient, e.g. Uv. No rail, "
                       "no catalogue."),
        QStringLiteral("name"));
    parser.addOption(cardOption);

    const QCommandLineOption detailsOption(
        QStringLiteral("details"),
        QStringLiteral("Render the weather-details grid on its own, all twelve cards."));
    parser.addOption(detailsOption);

#ifdef CLIMA_DEV_TOOLS
    const QCommandLineOption filmOption(
        QStringLiteral("film"),
        QStringLiteral("Write <prefix>-00.png, <prefix>-01.png … and exit. A still frame "
                       "cannot show a component's mount animation; this can."),
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
        QStringLiteral("Apply <target>=<value> once the stage has settled; repeatable. The "
                       "only target that means anything here is remount, which rebuilds every "
                       "specimen and replays whatever it does on mount."),
        QStringLiteral("target=value"));
    parser.addOption(pokeOption);

    const QCommandLineOption walkOption(
        QStringLiteral("walk"),
        QStringLiteral("Step <n> components on before grabbing, so a headless check exercises "
                       "navigation and not only first paint."),
        QStringLiteral("n"));
    parser.addOption(walkOption);
#endif // CLIMA_DEV_TOOLS

    parser.addPositionalArgument(
        QStringLiteral("component"),
        QStringLiteral("Which component to open on, matched as a substring against the "
                       "catalogue's names and file names. Every word up to the next flag, so "
                       "`clima-gallery weather glyph` works without quotes."),
        QStringLiteral("[component…]"));

    parser.process(app);

    if (parser.isSet(grabOption))
        self->m_grab = parser.value(grabOption);

    if (parser.isSet(sizeOption)) {
        // Anchored, so 1500x950x2 and 1500xfoo are rejected rather than
        // half-read.
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
        if (!AppOptions::viewportIds().contains(id))
            fail(QStringLiteral("--viewport: expected one of %1 — got \"%2\"")
                     .arg(AppOptions::viewportIds().join(QStringLiteral(", ")), id));
        self->m_viewport = id;
    }

    if (parser.isSet(skyOption)) {
        const QString phase = parser.value(skyOption);
        if (!AppOptions::skyPhases().contains(phase))
            fail(QStringLiteral("--sky: expected one of %1 — got \"%2\"")
                     .arg(AppOptions::skyPhases().join(QStringLiteral(", ")), phase));
        self->m_sky = phase;
    }

    if (parser.isSet(cardOption))
        self->m_card = parser.value(cardOption);
    self->m_details = parser.isSet(detailsOption);

    // Both at once is a contradiction rather than a precedence question, and
    // guessing which one was meant is how a screenshot ends up being of the
    // wrong thing without anybody noticing.
    if (!self->m_card.isEmpty() && self->m_details)
        fail(QStringLiteral("--card and --details each render one thing on its own; "
                            "pick one"));

#ifdef CLIMA_DEV_TOOLS
    if (parser.isSet(framesOption))
        self->m_frames = requireInt(parser, framesOption, 1, "a count > 0");
    if (parser.isSet(everyOption))
        self->m_every = requireInt(parser, everyOption, 1, "milliseconds > 0");
    if (parser.isSet(filmOption))
        self->m_film = parser.value(filmOption);
    if (parser.isSet(walkOption))
        self->m_walk = requireInt(parser, walkOption, 0, "a count >= 0");

    // Shape only. Whether `remount` is a target is a question for the moment the
    // poke is applied, which is the first moment anything knows what is on the
    // stage.
    for (const QString &poke : parser.values(pokeOption)) {
        if (!poke.contains(QLatin1Char('=')))
            fail(QStringLiteral("--poke: expected target=value, got \"%1\"").arg(poke));
        self->m_pokes.append(poke);
    }
#endif // CLIMA_DEV_TOOLS

    // Joined with spaces rather than taken as the first word, so an unquoted
    // `weather glyph` is one query and not a query plus a stray argument. The
    // catalogue matches on substrings, so a name with a space in it works.
    self->m_pick = parser.positionalArguments().join(QLatin1Char(' '));

    // A component name with --card or --details is a request the window cannot
    // honour: neither of those modes has a catalogue to look a name up in.
    if (!self->m_pick.isEmpty() && (!self->m_card.isEmpty() || self->m_details))
        fail(QStringLiteral("\"%1\": a component name selects from the catalogue, and "
                            "--card and --details do not show one")
                 .arg(self->m_pick));
}
