# SPDX-FileCopyrightText: 2026 Jowi Aoun
# SPDX-License-Identifier: GPL-3.0-or-later
#
# The engine. Qt Core, Qt Network, Qt Sql — and provably nothing else.
#
# Note the licence line above: this build file is GPL-3.0-or-later like the rest
# of the build, while every source file it compiles is MPL-2.0. That is not an
# oversight and it is the point of the split (D6). The MPL is file-scoped, so
# the licence that matters is the one in each source file's header; a build
# script is not part of the library it builds, and a downstream that vendors
# libclima's sources under the MPL writes its own build anyway.
#
# ---- what this target may link ----------------------------------------------
#
# Core, Network, Sql. Not Gui, not Qml, not Quick, and clima_forbid_gui() at the
# bottom of this file makes that a configure-time error rather than a rule in a
# document — see cmake/ClimaEngineGuard.cmake for why the promise is worth a
# hundred lines of CMake to keep.
#
# ---- STATIC, and the target name --------------------------------------------
#
# The target is `libclima` because that is what docs/04-architecture.md §4.3
# calls it and what every other document refers to, and OUTPUT_NAME is `clima`
# so that the archive is `libclima.a` rather than `liblibclima.a`. The
# executable in app/ is also called `clima`; the two do not collide because one
# is an archive with a `lib` prefix in build/dev/libclima and the other is an
# executable in build/dev/app.
#
# STATIC for the same reason app/climaqml is static: one repository, one
# release, consumers always built together. The applet and the extension that
# will one day want a shared object are separate builds in separate
# repositories, and they will make that decision for themselves.

add_library(libclima STATIC
    cache/cachepolicy.h
    cache/cachepolicy.cpp
    cache/cachestore.h
    cache/cachestore.cpp
    cache/migrations.h
    cache/migrations.cpp
    core/clock.h
    core/clock.cpp
    core/error.h
    core/error.cpp
    core/result.h
    domain/coordinate.h
    domain/coordinate.cpp
    net/backoff.h
    net/backoff.cpp
    net/httpclient.h
    net/httpclient.cpp
    net/httprequest.h
    net/requestkey.h
    net/requestkey.cpp
    net/validatorstore.h
    net/validatorstore.cpp
)

set_target_properties(libclima PROPERTIES
    OUTPUT_NAME clima
    AUTOMOC ON
)

# ---- who we say we are ------------------------------------------------------
#
# The User-Agent's version comes from PROJECT_VERSION, the same variable
# app/climaconfig.h.in reads, so the string a weather service sees and the
# string `clima --version` prints cannot disagree. See climaidentity.h.in — this
# is a compliance string, not a nicety.
configure_file(climaidentity.h.in climaidentity.h @ONLY)

# ---- include paths ----------------------------------------------------------
#
# The repository root, so that every include inside the engine reads
# `#include "libclima/net/httpclient.h"` — the path the file actually has. The
# alternative, relative includes, means `../../core/clock.h` in a header whose
# meaning then depends on where the *including* file sits, and a header that
# moves breaks its consumers rather than itself.
#
# PUBLIC on both counts: app/ and tests/ include the engine's headers by the
# same spelling, and the test that asserts the User-Agent needs the generated
# identity header to state what it expects.
target_include_directories(libclima PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/..
    ${CMAKE_CURRENT_BINARY_DIR}
)

target_compile_features(libclima PUBLIC cxx_std_17)

# -Wswitch is what makes cachepolicy.cpp's total switch over DataKind a
# guarantee rather than a habit: adding a kind without adding its TTL row stops
# the build instead of silently caching it for zero seconds.
target_compile_options(libclima PRIVATE
    $<$<CXX_COMPILER_ID:GNU,Clang,AppleClang>:-Wall;-Wextra;-Wswitch>
)

# QT_NO_CAST_FROM_ASCII would be the stricter setting and is not on, because the
# SQL in cache/ is genuinely more readable as QStringLiteral than as anything
# else and the casts that remain are all QLatin1String on compile-time
# constants. What is on is the one that catches a real bug class: an implicit
# conversion from a QUrl to a QString drops the difference between an encoded
# and a decoded URL, which is how a query parameter with a `+` in it becomes a
# space on one code path and not another.
#
# PRIVATE. Neither macro changes a type's layout, so there is no ODR hazard in
# one translation unit having them and another not — which means they can be
# this library's house rules rather than something imposed on every consumer.
# app/ and the QML module get to make their own decision.
target_compile_definitions(libclima PRIVATE
    QT_NO_CAST_FROM_BYTEARRAY
    QT_NO_URL_CAST_FROM_STRING
)

target_link_libraries(libclima PUBLIC
    Qt6::Core
    Qt6::Network
    Qt6::Sql
)

# The assertion. Everything above this line is a claim; this line is what makes
# it true. Try it: add Qt6::Gui to the list above and configure.
clima_forbid_gui(libclima)
