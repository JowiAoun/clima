# SPDX-FileCopyrightText: 2026 Jowi Aoun
# SPDX-License-Identifier: GPL-3.0-or-later
#
# The test suite. Qt Test, ctest, and nothing fetched from the internet at
# configure time or at run time.
#
# ---- why Qt Test and not Catch2, doctest or GoogleTest ----------------------
#
# D8 in docs/03-tech-stack.md rules out Conan and vcpkg so that a distribution
# packager can build Clima from what their distribution already ships. A test
# framework pulled in by FetchContent is exactly the dependency that decision
# excludes: it turns `cmake --build` into an operation that needs a network,
# which is the one thing a Debian or Fedora build environment does not have.
#
# Qt Test arrives with the Qt that is already required, and it happens to be the
# right shape anyway. Everything below is asynchronous — a network client, a
# signal, a future — and QSignalSpy and QTest::qWaitFor are what those are
# tested with.
#
# ---- the tests link libclima and Qt Test, and that is the whole list --------
#
# Not Qt6::Gui, not Qt6::Quick. That is not tidiness: `clima_no_gui_test` below
# reads the linked binary's DT_NEEDED entries and fails if a GUI library is
# there, which is only a meaningful check because these executables link the
# engine and nothing else. Adding Qt6::Gui to a test target here would silently
# defeat the strongest guarantee in the build.

# ---- shared test support ----------------------------------------------------
#
# A loopback HTTP server and the proxy factory that makes reaching anything else
# impossible. Both are GPL-3.0-or-later like everything outside libclima/ —
# tests are not part of the reusable engine.
add_library(clima_testsupport STATIC
    support/httpstub.h
    support/httpstub.cpp
    support/networkguard.h
    support/networkguard.cpp
)

target_include_directories(clima_testsupport PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(clima_testsupport PUBLIC Qt6::Core Qt6::Network)
target_compile_features(clima_testsupport PUBLIC cxx_std_17)

clima_forbid_gui(clima_testsupport)

# ---- one function, because every test wants the same five lines --------------
#
# CLIMA_SOURCE_DIR is the repository root, handed to the tests that read the
# tree — tst_sourcerules.cpp scans for wall-clock reads and for hostnames. A
# test that computed the path from its own binary location would be a test that
# breaks the day the build directory moves.
function(clima_add_test name)
    add_executable(${name} ${name}.cpp)

    target_link_libraries(${name} PRIVATE libclima clima_testsupport Qt6::Test)
    target_compile_definitions(${name} PRIVATE
        CLIMA_SOURCE_DIR="${CMAKE_SOURCE_DIR}"
    )

    clima_forbid_gui(${name})

    add_test(NAME ${name} COMMAND ${name})

    set_tests_properties(${name} PROPERTIES
        # offscreen, in case a future test links Gui: a test that tries to open
        # a window on a headless runner hangs rather than fails, and a hung test
        # is a timeout nobody can read.
        ENVIRONMENT "QT_QPA_PLATFORM=offscreen;QT_FORCE_STDERR_LOGGING=1;LC_ALL=C.UTF-8;TZ=UTC"

        # Generous, because the failure this bounds is a deadlock rather than a
        # slow machine. Nothing here should take more than a second.
        TIMEOUT 60
    )
endfunction()

clima_add_test(tst_clock)
clima_add_test(tst_backoff)
clima_add_test(tst_requestkey)
clima_add_test(tst_httpclient)
clima_add_test(tst_cachestore)
clima_add_test(tst_networkisolation)
clima_add_test(tst_sourcerules)
clima_add_test(tst_alerts)
clima_add_test(tst_alertproviders)
clima_add_test(tst_weathercode)
clima_add_test(tst_timeaxis)
clima_add_test(tst_openmeteoadapter)
clima_add_test(tst_airquality)
clima_add_test(tst_metno)
clima_add_test(tst_fixtureprovider)
clima_add_test(tst_providerregistry)
clima_add_test(tst_geocoding)
clima_add_test(tst_reversegeocode)
clima_add_test(tst_places)
clima_add_test(tst_wiresnapshot)

# ---- the tiles ---------------------------------------------------------------
#
# Registered by hand rather than through clima_add_test(), for two reasons that
# both come from the subject. It links `climawidgetsqml` — the widget host's QML
# module — rather than libclima, because `Wx` is what every number a tile prints
# goes through. And clima_forbid_gui() is deliberately not applied: that guard
# enforces libclima's own promise, and this is a test of a thing that draws.
#
# Guarded on the target, because widgets/ is only entered on Linux with Qt DBus.
if(TARGET climawidgetsqml)
    add_executable(tst_widgets tst_widgets.cpp)

    target_link_libraries(tst_widgets PRIVATE climawidgetsqml Qt6::Test)
    target_compile_definitions(tst_widgets PRIVATE
        CLIMA_SOURCE_DIR="${CMAKE_SOURCE_DIR}"
    )

    add_test(NAME tst_widgets COMMAND tst_widgets)

    # TZ=UTC is load-bearing here rather than hygienic. Half of what this file
    # asserts is that an instant is read in the zone its own offset names and
    # NOT converted into the reader's — which is invisible on a machine whose
    # clock happens to agree with the fixture.
    set_tests_properties(tst_widgets PROPERTIES
        ENVIRONMENT "QT_QPA_PLATFORM=offscreen;QT_FORCE_STDERR_LOGGING=1;LC_ALL=C.UTF-8;TZ=UTC"
        TIMEOUT 60
    )
endif()

# ---- the one test that links app/ -------------------------------------------
#
# Registered by hand rather than through clima_add_test(), because it is the one
# test whose subject is a VIEW MODEL and not the engine. `Detail` is
# app/viewmodels/conditionsdata.h: GPL-3.0-or-later, not MPL-2.0, read by QML and
# therefore allowed to depend on it. clima_forbid_gui() is deliberately not
# applied — it would refuse the link, and refusing it here would be enforcing
# libclima's promise against something that never made it.
#
# The promise itself is untouched. `engine_has_no_gui` below asks the question of
# tst_httpclient, which links the engine and nothing else, and that is still the
# binary the guarantee is read off.
#
# What it is for: the neutral shape those maps are born with has to stay in step
# with what the build functions produce, and nothing else would notice when it
# does not. The file's own header has the argument.
add_executable(tst_conditionsdata tst_conditionsdata.cpp)

target_link_libraries(tst_conditionsdata PRIVATE climaqml clima_testsupport Qt6::Test)
target_compile_definitions(tst_conditionsdata PRIVATE
    CLIMA_SOURCE_DIR="${CMAKE_SOURCE_DIR}"
)

add_test(NAME tst_conditionsdata COMMAND tst_conditionsdata)

set_tests_properties(tst_conditionsdata PROPERTIES
    ENVIRONMENT "QT_QPA_PLATFORM=offscreen;QT_FORCE_STDERR_LOGGING=1;LC_ALL=C.UTF-8;TZ=UTC"
    TIMEOUT 60
)

# ---- the golden images' canary ----------------------------------------------
#
# Links climaqml for AppFont and therefore links Qt6::Gui, which is why it is
# registered here rather than through clima_add_test() — that function applies
# clima_forbid_gui() and would refuse the link. Measuring text without a GUI is
# not a thing that can be arranged.
#
# FONTCONFIG_FILE is the whole reason this is meaningful: it measures the font
# environment the golden images are *taken* in, not the one the developer's
# desktop happens to have. Without it the numbers are about the host and the
# test would fail on every machine but one.
add_executable(tst_environment tst_environment.cpp)

target_link_libraries(tst_environment PRIVATE climaqml Qt6::Test)
target_compile_definitions(tst_environment PRIVATE
    CLIMA_SOURCE_DIR="${CMAKE_SOURCE_DIR}"
)

add_test(NAME tst_environment COMMAND tst_environment)

# Labelled `golden`, so that `ctest -LE golden` drops the canary along with the
# images it guards. That is not tidiness — it was a real failure. Built against
# Debian 13's Qt 6.8.2 the suite is 21/22, and the one failure is this test
# correctly reporting that FreeType there rounds line heights differently:
#
#   11px height: recorded 13.296875, measured 13.312500
#
# One sixty-fourth of a pixel, at five of the eight sizes, with every advance
# identical. Every golden image would differ and not one of those differences
# would be a defect — which is exactly what this test exists to say. But the
# packager's build deliberately runs a *different* Qt, so asking it whether the
# machine matches the one the images were recorded on is asking a question with
# a known answer and calling it a failure.
set_tests_properties(tst_environment PROPERTIES
    ENVIRONMENT "QT_QPA_PLATFORM=offscreen;QT_QPA_PLATFORMTHEME=;QT_FORCE_STDERR_LOGGING=1;QT_SCALE_FACTOR=1;QT_ENABLE_HIGHDPI_SCALING=0;QT_SCREEN_SCALE_FACTORS=;QT_FONT_DPI=96;LC_ALL=C.UTF-8;LANG=C.UTF-8;TZ=UTC;FONTCONFIG_FILE=${CMAKE_CURRENT_SOURCE_DIR}/golden/fontconfig.conf"
    LABELS "golden"
    TIMEOUT 60
)

# ---- the QML suite ----------------------------------------------------------
#
# One executable running every tests/qml/tst_*.qml, and the first test coverage
# this repository has ever had of the part of it that users look at.
#
# It links both QML modules and both plugins. `Clima` for the components and the
# Theme singleton; `Clima.Gallery` for gallery.js — the catalogue of everything
# in the tree, which tst_specimen walks — and contrast.js, which is where WCAG's
# arithmetic lives. Linking the gallery module is why gallery/ builds a static
# library with a thin executable on top rather than baking its module into the
# binary: a module inside an executable is a module only that executable can
# import.
#
# clima_forbid_gui() is deliberately absent, as it is for tst_conditionsdata.
# This one is *all* GUI.
add_subdirectory(qml)

# ---- the binary-level no-GUI check ------------------------------------------
#
# cmake/ClimaEngineGuard.cmake refuses at configure time if libclima's target
# graph reaches Qt6::Gui. This reads the actual dynamic dependencies of a linked
# executable, which is the same question asked of the artefact rather than of
# the model — see the script for why both exist.
#
# It is pointed at tst_httpclient because that binary links the widest part of
# the engine: the network client, the cache store and the clock, plus Qt Test.
# If any of them dragged a GUI library in, it is in this executable's DT_NEEDED
# list.
# ---- the golden images ------------------------------------------------------
#
# Registered only when there is something to photograph with. `--grab` lives
# behind CLIMA_DEV_TOOLS and half the cases are gallery pages, so without both
# this would be a test that fails for want of a flag rather than for want of a
# pixel.
#
# DEPENDS tst_environment so that ctest runs the canary first. When the machine's
# text rasterisation has moved, every image differs and none of those
# differences is a defect — having the one legible failure appear above thirty
# image diffs is the entire reason the canary exists.
#
# LABELS so that `ctest -LE slow` skips it: a minute of rendering is the right
# price in CI and the wrong one for a developer re-running a unit test.
if(CLIMA_DEV_TOOLS AND CLIMA_BUILD_GALLERY AND UNIX AND NOT APPLE)
    add_test(NAME golden_images
        COMMAND ${CMAKE_SOURCE_DIR}/scripts/golden.sh check
    )

    set_tests_properties(golden_images PROPERTIES
        ENVIRONMENT "CLIMA_BUILD_DIR=${CMAKE_BINARY_DIR}"
        DEPENDS tst_environment
        LABELS "slow;golden"
        TIMEOUT 600
    )
endif()

# ---- the binary-level licence check -----------------------------------------
#
# R2 and R3 from docs/08-risks.md, asked of the artefact. cmake/
# ClimaLicenceGuard.cmake asks the same two questions of the build's own model
# at configure time and cannot see a module that arrives without being named —
# through a QML import, or through some future component's dependency graph.
#
# Registered as a ctest rather than only as a CI job because it is a property of
# what was just built: a developer who adds `find_package(Qt6 COMPONENTS
# Charts)` should learn about it from `ctest`, not from a red pull request.
if(UNIX AND NOT APPLE)
    add_test(NAME licences_are_compatible
        COMMAND ${CMAKE_SOURCE_DIR}/scripts/check-licences.sh
            $<TARGET_FILE:clima>
            $<$<BOOL:${CLIMA_BUILD_GALLERY}>:$<TARGET_FILE:clima-gallery>>
            # And the two that ship beside it on Linux. The widget host is the
            # one that matters here: it is a second Qt Quick binary, built from
            # a second QML module, and a banned module added to that module
            # would be invisible to a check that only ever looked at `clima`.
            $<$<TARGET_EXISTS:clima-widget>:$<TARGET_FILE:clima-widget>>
            $<$<TARGET_EXISTS:clima-daemon>:$<TARGET_FILE:clima-daemon>>
    )
endif()

find_program(CLIMA_OBJDUMP NAMES objdump llvm-objdump)

# Registered only when it can actually run. A test that reports "passed"
# because the tool it needed was missing is worse than one that is absent: the
# first is a guarantee nobody is checking, and the second is visible in the
# ctest output as a test that is not there.
if(CLIMA_OBJDUMP)
    add_test(NAME engine_has_no_gui
        COMMAND ${CMAKE_COMMAND}
            -DCLIMA_BINARY=$<TARGET_FILE:tst_httpclient>
            -DCLIMA_OBJDUMP=${CLIMA_OBJDUMP}
            -P ${CMAKE_SOURCE_DIR}/cmake/ClimaNoGuiBinaryCheck.cmake
    )
else()
    message(STATUS
        "clima: no objdump found, so the binary-level no-GUI test is not registered. "
        "The configure-time guard in cmake/ClimaEngineGuard.cmake still applies.")
endif()

# ---- and the opposite question, asked of the widget host ---------------------
#
# `engine_has_no_gui` above asks whether the engine reached a windowing toolkit.
# This asks whether a *drawing* process reached the engine's fetching half, which
# is the same architectural rule from the other end: one process fetches and
# every other one reads from it over the bus.
#
# It is a symbol check and not an `ldd`, and cmake/ClimaNoEngineSymbolCheck.cmake
# opens with why: libclima is a static archive, so there is no shared object to
# look for and DT_NEEDED records nothing about which members were pulled in. What
# a static link leaves behind is a symbol.
#
# Registered here rather than in widgets/CMakeLists.txt because enable_testing()
# runs from the top-level directory after that one is entered, so an add_test()
# there registers nothing at all — a guard that is silently absent from a green
# ctest run.
if(TARGET clima-widget)
    find_program(CLIMA_NM NAMES nm llvm-nm)
    if(CLIMA_NM)
        add_test(NAME widget_has_no_engine
            COMMAND ${CMAKE_COMMAND}
                -DCLIMA_BINARY=$<TARGET_FILE:clima-widget>
                -DCLIMA_NM=${CLIMA_NM}
                -P ${CMAKE_SOURCE_DIR}/cmake/ClimaNoEngineSymbolCheck.cmake
        )
    else()
        message(STATUS
            "clima: no nm found, so the widget host's link line is unchecked in this build.")
    endif()
endif()
