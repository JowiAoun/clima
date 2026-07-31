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
clima_add_test(tst_airquality)
clima_add_test(tst_metno)
clima_add_test(tst_providerregistry)

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
