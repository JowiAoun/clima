# SPDX-FileCopyrightText: 2026 Jowi Aoun
# SPDX-License-Identifier: GPL-3.0-or-later
#
# The application: one executable, one QML module.
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
    qml/Clima/Gallery.qml
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
    qml/Clima/PrecipBands.qml
    qml/Clima/PrecipField.qml
    qml/Clima/PrecipitationStrip.qml
    qml/Clima/SectionHeader.qml
    qml/Clima/SeriesArea.qml
    qml/Clima/SeriesBars.qml
    qml/Clima/SkyArc.qml
    qml/Clima/Specimen.qml
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
set(clima_js_files
    qml/Clima/chartmath.js
    qml/Clima/detaildata.js
    qml/Clima/gallery.js
    qml/Clima/metrics.js
    qml/Clima/mobiletabs.js
    qml/Clima/mockdata.js
    qml/Clima/precip.js
    qml/Clima/sky.js
    qml/Clima/theme.js
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
foreach(clima_file IN LISTS clima_qml_files clima_js_files)
    get_filename_component(clima_file_name "${clima_file}" NAME)
    set_source_files_properties("${clima_file}"
        PROPERTIES QT_RESOURCE_ALIAS "${clima_file_name}")
endforeach()

qt_add_executable(clima
    main.cpp
)

qt_add_qml_module(clima
    URI Clima
    VERSION 1.0
    QML_FILES
        ${clima_qml_files}
        ${clima_js_files}
)

target_include_directories(clima PRIVATE ${CMAKE_CURRENT_BINARY_DIR})

# Qt6::Quick and nothing else, because QtQuick and QtQuick.Shapes are the only
# two modules the QML imports (QtQuick.Window ships inside Quick) and both come
# from this one target. Adding a component "in case" is how a banned module
# arrives — see cmake/ClimaLicenceGuard.cmake.
target_link_libraries(clima PRIVATE Qt6::Quick)

clima_guard_target(clima)

set_target_properties(clima PROPERTIES
    WIN32_EXECUTABLE TRUE
    MACOSX_BUNDLE TRUE
)
