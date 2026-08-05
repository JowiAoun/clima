# SPDX-FileCopyrightText: 2026 Jowi Aoun
# SPDX-License-Identifier: GPL-3.0-or-later
#
# The application: one QML module, and one executable that loads it.
#
# The module is flat on purpose. Splitting it into views/, components/ and
# theme/ is a good idea and it is a separate commit — this one already moves 73
# files across a module boundary and proves it changed nothing by comparing
# screenshots, and a directory split in the same breath would mean two failure
# modes sharing one bisect.

configure_file(climaconfig.h.in climaconfig.h @ONLY)

# ---- the module's files -----------------------------------------------------
#
# Listed one by one. Never file(GLOB).
#
# This is not dogma about globbing, it is about the specific failure. A globbed
# QML module goes stale the moment someone adds a file without re-running
# CMake: the new file is not in the module, so the type it declares does not
# exist, so every *other* file that uses that type fails — and QML reports the
# failure at the point of use. You get "PrecipField is not a type" pointing at
# HourlyOverview.qml, a file nobody touched, in a build that succeeded. The
# list below turns that into a configure-time error naming the actual file.
#
# scripts/check-qml-files.sh diffs this block against what is on disk, so the
# list going stale is caught by a script rather than by an afternoon.
set(clima_qml_files
    qml/Clima/BottomNav.qml
    qml/Clima/ChevronGlyph.qml
    qml/Clima/Clock.qml
    qml/Clima/CurrentConditions.qml
    qml/Clima/DayIconBadge.qml
    qml/Clima/DayStrip.qml
    qml/Clima/DetailAirQualityCard.qml
    qml/Clima/DetailCard.qml
    qml/Clima/DetailCloudCoverCard.qml
    qml/Clima/DetailFeelsLikeCard.qml
    qml/Clima/DetailHumidityCard.qml
    qml/Clima/DetailMoonCard.qml
    qml/Clima/DetailPrecipitationCard.qml
    qml/Clima/DetailPressureCard.qml
    qml/Clima/DetailSunCard.qml
    qml/Clima/DetailTemperatureCard.qml
    qml/Clima/DetailUvCard.qml
    qml/Clima/DetailVisibilityCard.qml
    qml/Clima/DetailWindCard.qml
    qml/Clima/DropletGlyph.qml
    qml/Clima/FeelsLikeToggle.qml
    qml/Clima/HatchPattern.qml
    qml/Clima/HourlyList.qml
    qml/Clima/HourlyOverview.qml
    qml/Clima/LocationBar.qml
    qml/Clima/Main.qml
    qml/Clima/MapPlaceholder.qml
    qml/Clima/MetricTabBar.qml
    qml/Clima/MobileActivitiesCard.qml
    qml/Clima/MobileCalendar.qml
    qml/Clima/MobileCard.qml
    qml/Clima/MobileCurrentWeather.qml
    qml/Clima/MobileDailyStrip.qml
    qml/Clima/MobileHourStrip.qml
    qml/Clima/MobileHourlyPage.qml
    qml/Clima/MobileMapsPage.qml
    qml/Clima/MobileMePage.qml
    qml/Clima/MobileMetricPicker.qml
    qml/Clima/MobileMonthlyPage.qml
    qml/Clima/MobilePage.qml
    qml/Clima/MobilePollenCard.qml
    qml/Clima/MobileShell.qml
    qml/Clima/MobileSunMoonCard.qml
    qml/Clima/MobileTodayPage.qml
    qml/Clima/MobileWeekStrip.qml
    qml/Clima/MoonGlyph.qml
    qml/Clima/NavGlyph.qml
    qml/Clima/PageBackdrop.qml
    qml/Clima/PagerButton.qml
    qml/Clima/PlacePicker.qml
    qml/Clima/PrecipBands.qml
    qml/Clima/PrecipField.qml
    qml/Clima/PrecipitationStrip.qml
    qml/Clima/SectionHeader.qml
    qml/Clima/SeriesArea.qml
    qml/Clima/SeriesBars.qml
    qml/Clima/SkyArc.qml
    qml/Clima/SunEventGlyph.qml
    qml/Clima/TabFillet.qml
    qml/Clima/Theme.qml
    qml/Clima/TrendBadge.qml
    qml/Clima/Viewports.qml
    qml/Clima/WeatherDetails.qml
    qml/Clima/WeatherGlyph.qml
    qml/Clima/WeatherPage.qml
)

# The .js files are part of the module too, not incidental data beside it.
# Listing them in QML_FILES is what gets them through qmlcachegen — they are
# compiled to bytecode and embedded like the .qml, rather than parsed from a
# resource at first use — and it is what makes `.import "precip.js" as Precip`
# inside mockdata.js resolve, because the import is resolved against the
# importing file's own URL and both files are then at the same qrc directory.
# `mockdata.js`, `detaildata.js` and `metrics.js` used to be here. All three are
# app/viewmodels/ now — see forecastdata.h, conditionsdata.h and metrics.h, each
# of which quotes the header of the file it replaced. What is left in JavaScript
# is the four libraries that compute rather than describe: chart geometry, the
# precipitation model, the sky phase table and the colour ramps.
set(clima_js_files
    qml/Clima/chartmath.js
    qml/Clima/mobiletabs.js
    qml/Clima/precip.js
    qml/Clima/sky.js
    qml/Clima/theme.js
    qml/Clima/themelight.js
)

# ---- singletons -------------------------------------------------------------
#
# `pragma Singleton` in the file is necessary and not sufficient. Without this
# property the generated qmldir lists the type as an ordinary component —
# `Theme 1.0 Theme.qml` rather than `singleton Theme 1.0 Theme.qml` — and every
# use of it fails at runtime with `Value is undefined`, naming the file that
# read the token rather than the file that failed to become a singleton. There
# is no build-time complaint of any kind.
#
# So: a file with `pragma Singleton` at the top belongs here as well as in
# clima_qml_files. Check `build/dev/qml/Clima/qmldir` if a singleton comes back
# undefined; the word `singleton` is either on its line or it is not.
set_source_files_properties(
    qml/Clima/Clock.qml
    qml/Clima/Theme.qml
    qml/Clima/Viewports.qml
    PROPERTIES QT_QML_SINGLETON_TYPE TRUE
)

# ---- flattening the resource path -------------------------------------------
#
# qt_add_qml_module aliases each file by its path relative to *this*
# CMakeLists.txt, so `qml/Clima/Main.qml` would land at
# `qrc:/qt/qml/Clima/qml/Clima/Main.qml` — the directory name repeated, once
# from the URI and once from the source layout.
#
# It works, and it is wrong in two ways worth a loop to avoid. Every error
# message and every `component.url` in a log carries the doubled path, and
# QT_QML_OUTPUT_DIRECTORY reproduces the nesting on disk, so the module
# directory that qmllint and qmlls are pointed at has a `qml/Clima/` inside it
# and its qmldir entries all read `qml/Clima/Foo.qml`.
#
# Aliasing each file to its bare name puts the module where its URI says it is.
# gallery/CMakeLists.txt does the same thing for the same reason, and the two
# together are what make `../theme.js` mean what it says from inside
# Clima.Gallery.
foreach(clima_file IN LISTS clima_qml_files clima_js_files)
    get_filename_component(clima_file_name "${clima_file}" NAME)
    set_source_files_properties("${clima_file}"
        PROPERTIES QT_RESOURCE_ALIAS "${clima_file_name}")
endforeach()

# ---- the module is a library ------------------------------------------------
#
# Not a module baked straight into the executable, which is what this was until
# the gallery moved out. The reason is one directory over: `gallery/` builds a
# second executable whose entire job is to instantiate the types below, and a
# QML module that lives inside `clima` is a module only `clima` can import.
#
# STATIC rather than SHARED. There is one repository, one release, and two
# consumers that are always built together; a .so would buy nothing here but an
# RPATH to get wrong and a second file for a packager to place. Note that this
# is about *our* code — Qt itself is still linked dynamically, which is not a
# preference but LGPLv3 (risk R3, docs/08-risks.md).
qt_add_library(climaqml STATIC)

# ---- the C++ that QML can see -----------------------------------------------
#
# These go in SOURCES rather than on the library, and the difference is not
# cosmetic: qmltyperegistrar only scans what the *module* was told about, so a
# header carrying QML_ELEMENT that is listed as an ordinary source instead
# compiles fine, links fine, and then does not exist as a QML type. The failure
# names the QML file that used it.
#
# screenshotcontroller is here unconditionally even though most of what it does
# is CLIMA_DEV_TOOLS-only. `--grab` ships — the issue template says "attach
# `clima --grab bug.png`" — so the file that implements it has to ship too, and
# the rest of it is compiled out from the inside. See its header.
set(clima_cpp_sources
    appfont.h
    appfont.cpp
    appoptions.h
    appoptions.cpp
    settings.h
    settings.cpp
    devtools/screenshotcontroller.h
    devtools/screenshotcontroller.cpp

    # ---- the view models ----------------------------------------------------
    #
    # docs/04-architecture.md §4.2's middle layer, arrived. `Engine` owns
    # libclima and the offline-first loop; `Data` and `Detail` are the two
    # shapes the QML already reads, which is why almost no QML file changed
    # when live data landed; `Units` is the one place a conversion factor is
    # written down and `Metrics` is the registry that used to be metrics.js.
    #
    # All four are QML_SINGLETONs, so they are in SOURCES rather than on the
    # library for the reason the block above gives: qmltyperegistrar only scans
    # what the module was told about, and a QML_ELEMENT header listed as an
    # ordinary source compiles, links, and then does not exist as a QML type.
    viewmodels/appengine.h
    viewmodels/appengine.cpp
    viewmodels/conditionsdata.h
    viewmodels/conditionsdata.cpp
    viewmodels/forecastdata.h
    viewmodels/forecastdata.cpp
    viewmodels/metrics.h
    viewmodels/metrics.cpp
    viewmodels/units.h
    viewmodels/units.cpp

    # ---- the desktop, asked rather than assumed ------------------------------
    #
    # A QML_SINGLETON like the view models above, and here for the same reason.
    # It answers two questions the session owns — dark or light, and whether to
    # move — and it ships in every build: the D-Bus half is compiled out when
    # Qt6::DBus is absent, and QStyleHints still answers the colour question.
    platform/systemappearance.h
    platform/systemappearance.cpp
)

# ---- the typeface -----------------------------------------------------------
#
# Vendored, third-party, and the one thing in this repository that is not ours:
# Inter 4.1 by The Inter Project Authors, under the SIL Open Font License 1.1.
# REUSE.toml carries the attribution — it must say 2016 The Inter Project
# Authors and not 2026 Jowi Aoun — and LICENSES/OFL-1.1.txt carries the terms.
# The files are unmodified, which matters for more than tidiness: the OFL is
# satisfied by the copyright notice and licence travelling with the font, and
# each file's own `name` table already holds both (name ID 0 and name ID 13), so
# the notice survives into a stripped binary that ships no text files at all.
#
# RESOURCES rather than QML_FILES. QML_FILES runs its arguments through
# qmlcachegen, which has opinions about what a .qml or .js file contains and
# none about a TTF; RESOURCES embeds bytes. They land at
# `:/qt/qml/Clima/fonts/…` — the module prefix plus the path relative to this
# file — and app/appfont.cpp opens them there. Not flattened by the loop above,
# because that loop exists to make the module directory match its URI for
# qmllint's benefit, and a font is not a QML type.
#
# On climaqml, not on `clima`, so that the gallery gets the same bytes from the
# same place. Reviewing a component in a typeface the app does not use is
# reviewing something else.
set(clima_font_files
    fonts/Inter-Regular.ttf
    fonts/Inter-Bold.ttf
)

qt_add_qml_module(climaqml
    URI Clima
    VERSION 1.0
    QML_FILES
        ${clima_qml_files}
        ${clima_js_files}
    RESOURCES
        ${clima_font_files}
    SOURCES
        ${clima_cpp_sources}
)

# PUBLIC, all three, and each for its own reason.
#
# The binary dir is where configure_file put climaconfig.h, and both executables
# print the version it carries. The source dir is what lets the gallery reuse
# AppOptions' viewport and sky vocabularies rather than keeping a second copy of
# two lists that must not disagree. The devtools dir is for a file nobody
# writes: qmltyperegistrar generates clima_qmltyperegistrations.cpp with an
# `#include <screenshotcontroller.h>` — basename only, no directory — so every
# directory holding a QML_ELEMENT header has to be reachable that way or the
# generated file names a type that "was not declared in this scope".
target_include_directories(climaqml PUBLIC
    ${CMAKE_CURRENT_BINARY_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/devtools
    ${CMAKE_CURRENT_SOURCE_DIR}/viewmodels
    ${CMAKE_CURRENT_SOURCE_DIR}/platform
)

# The option has existed since the CMake skeleton landed; this is where it
# starts meaning something. OFF compiles out --film, --poke, --walk, --scroll,
# --sky, --metric, --day, --list and --tab, leaving --grab, --size and
# --viewport.
#
# PUBLIC because it changes the shape of ScreenshotController — a member the
# library was compiled with and a consumer was not is an ODR violation that
# links cleanly and corrupts memory at runtime.
if(CLIMA_DEV_TOOLS)
    target_compile_definitions(climaqml PUBLIC CLIMA_DEV_TOOLS)
endif()

# Qt6::Quick and nothing else, because QtQuick and QtQuick.Shapes are the only
# two modules the QML imports (QtQuick.Window ships inside Quick) and both come
# from this one target. Adding a component "in case" is how a banned module
# arrives — see cmake/ClimaLicenceGuard.cmake.
target_link_libraries(climaqml PUBLIC Qt6::Quick)

# The portal, when there is one to talk to. PRIVATE and guarded by a compile
# definition rather than by a stub file: SystemAppearance is one class whose
# D-Bus half is a handful of methods, and a second implementation of it kept in
# step by hand is a larger liability than four #ifdefs in the file that owns
# them. Without this the class still exists, still answers, and still floors at
# dark — it just stops hearing about changes.
if(TARGET Qt6::DBus)
    target_link_libraries(climaqml PRIVATE Qt6::DBus)
    target_compile_definitions(climaqml PRIVATE CLIMA_HAVE_DBUS)
endif()

# The engine. Nothing in app/ calls into it yet — the view models that will are
# W4 — and it is linked now anyway, for two reasons that are both about the
# link line rather than about the code.
#
# The first is that the direction of the dependency is the architecture. app/
# may include libclima's headers; libclima may not include app's, and the CMake
# graph is where that is stated. Declaring it on the day the library appears
# means the first view model is a `#include` rather than a build-system change
# reviewed at the same time as a feature.
#
# The second is that it costs nothing to be wrong about. A static archive
# contributes only the objects something references, so an unused libclima adds
# no bytes to `clima`; measured before and after this line, the stripped binary
# moved by 0 bytes against the <15 MB budget in docs/03-tech-stack.md §3.4. What
# it does add is libQt6Sql.so.6 to the app's DT_NEEDED list — Qt6::Network was
# already there, arriving through QtQml — which is a runtime dependency a
# packager has to satisfy, and which the app genuinely will need.
#
# PUBLIC so that both the QML module's C++ and main.cpp can reach it. It is not
# a QML-visible dependency: libclima declares no QML types and never will, which
# is the same GUI-free promise cmake/ClimaEngineGuard.cmake enforces.
target_link_libraries(climaqml PUBLIC libclima)

clima_guard_target(climaqml)

# ---- the executable ---------------------------------------------------------
#
# Both targets, and neither is redundant. qt_add_qml_module generates
# `climaqmlplugin` beside the backing library, and the plugin is what registers
# the module with the engine — link only `climaqml` and the app builds, links,
# and comes up against an engine that has never heard of `Clima`. But the plugin
# links its backing library privately, so its usage requirements — Qt6::Quick,
# and the three include directories below — stop there. Link only the plugin and
# main.cpp cannot find <QQmlEngine>.
qt_add_executable(clima
    main.cpp
)

target_link_libraries(clima PRIVATE climaqml climaqmlplugin)

clima_guard_target(clima)

set_target_properties(clima PROPERTIES
    WIN32_EXECUTABLE TRUE
    MACOSX_BUNDLE TRUE
)
