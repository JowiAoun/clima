# SPDX-FileCopyrightText: 2026 Jowi Aoun
# SPDX-License-Identifier: GPL-3.0-or-later
#
# The widget host: one process, every tile, and no engine underneath it.
#
# ---- what this target may not link ------------------------------------------
#
# The providers, the HTTP client and the cache. A widget draws what
# clima-daemon fetched; a desktop with six tiles on it must not be six things
# opening sockets and writing one SQLite file.
#
# That is enforced twice, and neither half is a comment:
#
#   * `clima-widget` does not link libclima at all. It compiles two files out of
#     it — domain/weathercode.cpp and domain/scales.cpp, which are pure tables
#     over QString — so the WMO codes and the published bands are not copied,
#     and nothing else from the engine is reachable to be called.
#
#   * `widget_has_no_engine` reads the symbol table of the built binary. libclima
#     is a STATIC archive, so `ldd` proves nothing here — an archive contributes
#     only what is referenced and a linked-but-unused provider leaves no trace in
#     DT_NEEDED. The symbol check is the one that would actually notice.
#
# The second exists because the first is easy to undo. `target_link_libraries(…
# libclima)` is a plausible-looking line that would compile, link, run, and
# quietly put a second fetcher on the desktop.
#
# ---- why the app's QML is listed here rather than imported -------------------
#
# `import Clima` is not available: app/'s QML module is a static library whose
# types are registered by `climaqmlplugin`, and linking that would drag in
# `AppEngine` — and with it the providers this directory exists to keep out.
#
# So the shared files are listed below and compiled into a second module. One
# copy of each *file* in the repository, two copies of the bytecode in two
# binaries that were never going to share an address space anyway. A cloud that
# gains a highlight in the app gains it on the tiles in the same commit, which
# is the property that matters.

configure_file(widgetconfig.h.in widgetconfig.h @ONLY)

# The daemon's name, path and interface. Configured from the same template the
# daemon uses, out of the same CLIMA_DAEMON_PATH the top-level CMakeLists
# computes, because a client and a server that disagree about an object path
# produce a service that registers and then answers nothing.
configure_file(${CMAKE_SOURCE_DIR}/daemon/daemonconfig.h.in daemonconfig.h @ONLY)

# ---- the presentation files shared with the app ------------------------------
#
# Deliberately short, and it is meant to stay short. Every file here is one a
# tile actually instantiates today; nothing is listed "because a widget might
# want it". A tile is 160 px square and the components that suit it are the
# small ones.
set(clima_widget_shared_qml
    ${CMAKE_SOURCE_DIR}/app/qml/Clima/Theme.qml
    ${CMAKE_SOURCE_DIR}/app/qml/Clima/WeatherGlyph.qml
    ${CMAKE_SOURCE_DIR}/app/qml/Clima/SeriesArea.qml
    ${CMAKE_SOURCE_DIR}/app/qml/Clima/SeverityGlyph.qml
    ${CMAKE_SOURCE_DIR}/app/qml/Clima/SkyArc.qml
)

set(clima_widget_shared_js
    ${CMAKE_SOURCE_DIR}/app/qml/Clima/theme.js
    ${CMAKE_SOURCE_DIR}/app/qml/Clima/themelight.js
    ${CMAKE_SOURCE_DIR}/app/qml/Clima/chartmath.js
)

# ---- this module's own files -------------------------------------------------
#
# Listed one by one, never file(GLOB), for the reason app/CMakeLists.txt spends
# a paragraph on: a globbed module goes stale the moment somebody adds a file
# without re-running CMake, and QML reports the resulting failure at the point
# of *use* rather than at the file that is missing.
set(clima_widget_qml
    qml/Clima/Widgets/WidgetWindow.qml
    qml/Clima/Widgets/WidgetTile.qml
    qml/Clima/Widgets/WidgetSurface.qml
    qml/Clima/Widgets/DialGauge.qml
    qml/Clima/Widgets/CurrentConditionsWidget.qml
    qml/Clima/Widgets/TemperatureSparklineWidget.qml
    qml/Clima/Widgets/HourlyStripWidget.qml
    qml/Clima/Widgets/PrecipitationWidget.qml
    qml/Clima/Widgets/WindRoseWidget.qml
    qml/Clima/Widgets/UvDialWidget.qml
    qml/Clima/Widgets/AqiDialWidget.qml
    qml/Clima/Widgets/SunArcWidget.qml
    qml/Clima/Widgets/DailyStripWidget.qml
    qml/Clima/Widgets/AlertsWidget.qml
    qml/Clima/Widgets/wire.js
)

# Theme.qml carries `pragma Singleton`, which is necessary and not sufficient:
# without this property the generated qmldir lists it as an ordinary component
# and every token read fails at run time with "Value is undefined", naming the
# file that read the token rather than the file that failed to become a
# singleton. There is no build-time complaint of any kind.
set_source_files_properties(
    ${CMAKE_SOURCE_DIR}/app/qml/Clima/Theme.qml
    PROPERTIES QT_QML_SINGLETON_TYPE TRUE
)

# Every file aliased to its bare name, which does two things at once. It puts
# the module where its URI says it is — `qrc:/qt/qml/Clima/Widgets/Theme.qml`
# rather than a path with the whole of app/ in it — and it is what makes
# `import "theme.js" as Tokens` inside Theme.qml resolve, because a JS import is
# resolved against the importing file's own URL and the alias is what puts the
# two at the same qrc directory.
foreach(clima_file IN LISTS clima_widget_shared_qml clima_widget_shared_js clima_widget_qml)
    get_filename_component(clima_file_name "${clima_file}" NAME)
    set_source_files_properties("${clima_file}"
        PROPERTIES QT_RESOURCE_ALIAS "${clima_file_name}")
endforeach()

# ---- the C++ the module registers --------------------------------------------
#
# Four of these are app/'s own files, compiled a second time rather than linked.
#
# `Settings`, `Units` and `TimeFormat` because a tile has to show the units and
# the clock the reader chose in the app, and they choose them by writing an INI
# that both processes read — a second implementation of either mapping is a
# second place for °F, or for a 24-hour clock, to be wrong.
# `SystemAppearance` because a tile follows the desktop's light/dark preference
# like everything else on it. `ScreenshotController` because `--grab` is what
# makes a tile reviewable in CI without a session bus.
#
# None of the four reaches libclima. That is checked, not assumed:
# `widget_has_no_engine` in tests/CMakeLists.txt reads this binary's symbols.
set(clima_widget_sources
    daemonlink.h
    daemonlink.cpp
    layershell.h
    layershell.cpp
    widgetclock.h
    widgetclock.cpp
    widgetfeed.h
    widgetfeed.cpp
    widgetoptions.h
    widgetoptions.cpp
    wx.h
    wx.cpp

    ${CMAKE_SOURCE_DIR}/app/settings.h
    ${CMAKE_SOURCE_DIR}/app/settings.cpp
    ${CMAKE_SOURCE_DIR}/app/viewmodels/timeformat.h
    ${CMAKE_SOURCE_DIR}/app/viewmodels/timeformat.cpp
    ${CMAKE_SOURCE_DIR}/app/viewmodels/units.h
    ${CMAKE_SOURCE_DIR}/app/viewmodels/units.cpp
    ${CMAKE_SOURCE_DIR}/app/platform/systemappearance.h
    ${CMAKE_SOURCE_DIR}/app/platform/systemappearance.cpp
    ${CMAKE_SOURCE_DIR}/app/devtools/screenshotcontroller.h
    ${CMAKE_SOURCE_DIR}/app/devtools/screenshotcontroller.cpp

    # Two files out of libclima rather than the whole archive. Both are pure
    # functions over QString with no I/O, no cache and no provider — see the
    # header of each. This is what keeps the WMO tables and the published bands
    # single-sourced without linking the engine.
    ${CMAKE_SOURCE_DIR}/libclima/domain/weathercode.h
    ${CMAKE_SOURCE_DIR}/libclima/domain/weathercode.cpp
    ${CMAKE_SOURCE_DIR}/libclima/domain/scales.h
    ${CMAKE_SOURCE_DIR}/libclima/domain/scales.cpp
)

qt_add_library(climawidgetsqml STATIC)

qt_add_qml_module(climawidgetsqml
    URI Clima.Widgets
    VERSION 1.0
    QML_FILES
        ${clima_widget_shared_qml}
        ${clima_widget_shared_js}
        ${clima_widget_qml}
    SOURCES
        ${clima_widget_sources}
)

target_include_directories(climawidgetsqml PUBLIC
    ${CMAKE_CURRENT_BINARY_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/app
    ${CMAKE_SOURCE_DIR}/app/viewmodels
    ${CMAKE_SOURCE_DIR}/app/platform
    ${CMAKE_SOURCE_DIR}/app/devtools
)

# ScreenshotController changes shape under this definition — a member the
# library was compiled with and a consumer was not is an ODR violation that
# links cleanly and corrupts memory at run time. PUBLIC, exactly as app/ has it.
if(CLIMA_DEV_TOOLS)
    target_compile_definitions(climawidgetsqml PUBLIC CLIMA_DEV_TOOLS)
endif()

target_link_libraries(climawidgetsqml PUBLIC Qt6::Quick Qt6::DBus)

# SystemAppearance's portal half, on the same terms app/ links it. Here it is
# not optional in practice — this whole directory is a D-Bus client — but the
# definition is what the shared source file branches on and it has to be spelled
# the same way in both modules.
target_compile_definitions(climawidgetsqml PRIVATE CLIMA_HAVE_DBUS)

# ---- pinning the tiles to the desktop, where the compositor allows it ---------
#
# `zwlr_layer_shell_v1` is what KWin, Sway, Hyprland, Wayfire, river and labwc
# implement and mutter does not. Where it exists, clima-widget asks for a
# desktop-layer surface itself and there is no shell extension, no applet and no
# plugin — see widgets/layershell.h for why that is the whole of the KDE story.
#
# Optional, on the same terms qtpositioning is optional in libclima: a packager
# on a GNOME-only distribution may leave layer-shell-qt out, and then --pin
# reports that as its reason instead of failing to build.
#
# PUBLIC on the definition, not PRIVATE. widgets/layershell.h is included by
# widgetoptions.h, which the executable compiles against; a definition that
# changed the header's meaning between the library and its consumer is the ODR
# violation that links cleanly and misbehaves at run time — the same trap
# CLIMA_DEV_TOOLS is PUBLIC for, twenty lines up.
find_package(LayerShellQt QUIET)
find_package(PkgConfig QUIET)
if(PkgConfig_FOUND)
    pkg_check_modules(WaylandClient QUIET IMPORTED_TARGET wayland-client)
endif()

if(LayerShellQt_FOUND AND WaylandClient_FOUND)
    target_compile_definitions(climawidgetsqml PUBLIC CLIMA_HAS_LAYER_SHELL)
    # PkgConfig::WaylandClient is for the one registry roundtrip in
    # layershell.cpp that asks the compositor what it implements. Qt will not
    # answer that question — its Wayland globals are private API.
    target_link_libraries(climawidgetsqml PUBLIC LayerShellQt::Interface PkgConfig::WaylandClient)
    message(STATUS "clima: layer-shell-qt found — clima-widget can pin itself to the desktop")
else()
    message(STATUS
        "clima: layer-shell-qt or wayland-client not found, so clima-widget cannot "
        "pin itself to the desktop on KDE or a wlroots compositor. --pin says so at "
        "run time. GNOME is unaffected: its shell extension does the pinning.")
endif()

clima_guard_target(climawidgetsqml)

# ---- the executable ----------------------------------------------------------

qt_add_executable(clima-widget main.cpp ${CMAKE_SOURCE_DIR}/app/appfont.cpp)

# Both targets. qt_add_qml_module generates `climawidgetsqmlplugin` beside the
# backing library and the plugin is what registers the module with the engine;
# link only the library and the process comes up against an engine that has
# never heard of `Clima.Widgets`. But the plugin links its backing library
# privately, so link only the plugin and main.cpp cannot find <QQmlEngine>.
target_link_libraries(clima-widget PRIVATE climawidgetsqml climawidgetsqmlplugin)

# The same catalogue file the daemon embeds, embedded here too. `--list` works
# with no daemon running, and so does the first paint before the handshake
# lands; the daemon's answer wins whenever there is one, because it is the copy
# that will have been upgraded alongside the data.
qt_add_resources(clima-widget "clima_widget_catalogue"
    PREFIX "/clima"
    BASE ${CMAKE_CURRENT_SOURCE_DIR}
    FILES ${CMAKE_CURRENT_SOURCE_DIR}/catalogue.json
)

# The bundled faces, at the path app/appfont.cpp already looks for them at.
# Reviewing a tile in a typeface the app does not use is reviewing something
# else — and on a desktop the two are side by side.
qt_add_resources(clima-widget "clima_widget_fonts"
    PREFIX "/qt/qml/Clima"
    BASE ${CMAKE_SOURCE_DIR}/app
    FILES
        ${CMAKE_SOURCE_DIR}/app/fonts/Inter-Regular.ttf
        ${CMAKE_SOURCE_DIR}/app/fonts/Inter-Bold.ttf
)

clima_guard_target(clima-widget)

# ---- the check that keeps the engine out -------------------------------------
#
# Registered in tests/CMakeLists.txt beside `engine_has_no_gui`, not here.
# enable_testing() is called from the top-level directory *after* this one is
# entered, so an add_test() in this file silently registers nothing — which is
# the worst possible outcome for a guard: a green ctest run with the check
# missing from it. See cmake/ClimaNoEngineSymbolCheck.cmake for what it asks.

include(GNUInstallDirs)

install(TARGETS clima-widget
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)
