# SPDX-FileCopyrightText: 2026 Jowi Aoun
# SPDX-License-Identifier: GPL-3.0-or-later
#
# The QML test runner. See main.cpp for why these tests are written in QML.

qt_add_executable(tst_qml
    main.cpp
    qmlwarnings.h
    qmlwarnings.cpp
)

# QmlWarnings carries QML_ELEMENT and QML_SINGLETON, so it needs a module to be
# registered into. A module of its own, `Clima.Test`, rather than adding it to
# `Clima`: the app's module is what ships, and a type whose only purpose is to
# let a test assert about stderr has no business being importable from it.
qt_add_qml_module(tst_qml
    URI Clima.Test
    VERSION 1.0
)

# QUICK_TEST_SOURCE_DIR is how QtQuickTest finds the .qml files, and it points
# at the source tree rather than at a copy in the build directory on purpose:
# editing a test and re-running ctest should not need a build step, and a
# QtQuickTest .qml is data, not something that gets compiled.
target_compile_definitions(tst_qml PRIVATE
    QUICK_TEST_SOURCE_DIR="${CMAKE_CURRENT_SOURCE_DIR}"
)

target_link_libraries(tst_qml PRIVATE
    Qt6::QuickTest
    climaqml           climaqmlplugin
    climagalleryqml    climagalleryqmlplugin
    clima_testsupport
)

add_test(NAME tst_qml COMMAND tst_qml)

# The same pinned environment scripts/grab.sh exports, and for the same reasons
# — with one addition. `QT_QPA_PLATFORM=offscreen` is not a convenience here:
# QtQuickTest creates a real QQuickView per test file, so on a headless runner
# without it this does not fail, it hangs.
#
# The locale and zone are pinned because a component that formats a time or a
# number reads both, and `tst_specimen` builds every component there is.
#
# XDG_CONFIG_HOME and XDG_DATA_HOME point into the build tree because the setup
# object runs the app's real startup — Settings::prepareStorage() and
# AppEngine::configure() — which writes an INI file and opens a SQLite cache.
# A test suite that leaves things in the developer's ~/.config is a test suite
# whose second run is not the same as its first.
set_tests_properties(tst_qml PROPERTIES
    ENVIRONMENT "QT_QPA_PLATFORM=offscreen;QT_QPA_PLATFORMTHEME=;QT_FORCE_STDERR_LOGGING=1;QT_SCALE_FACTOR=1;QT_ENABLE_HIGHDPI_SCALING=0;QT_FONT_DPI=96;LC_ALL=C.UTF-8;LANG=C.UTF-8;TZ=UTC;XDG_CONFIG_HOME=${CMAKE_CURRENT_BINARY_DIR}/xdg/config;XDG_DATA_HOME=${CMAKE_CURRENT_BINARY_DIR}/xdg/data;XDG_CACHE_HOME=${CMAKE_CURRENT_BINARY_DIR}/xdg/cache"

    # Higher than the 60 s the engine tests get. This one instantiates every
    # component in the tree, and the bound failure is still a hang rather than a
    # slow machine.
    TIMEOUT 300
)
