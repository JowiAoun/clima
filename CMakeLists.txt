# SPDX-FileCopyrightText: 2026 Jowi Aoun
# SPDX-License-Identifier: GPL-3.0-or-later
#
# The top of the build. Everything here is policy — versions, identity, the
# options that switch pieces of the tree on and off — and nothing here compiles
# anything. The targets live one directory down, in app/.
#
#   nix develop -c cmake --preset dev
#   nix develop -c cmake --build build/dev
#   nix develop -c ./build/dev/app/clima
#
# 3.21 is the floor and it is not arbitrary: it is the first release with the
# `qt_standard_project_setup` story we depend on and the first that ships
# `CMAKE_<LANG>_LINKER_LAUNCHER` for the whole toolchain. The devshell pins
# CMake 4.x, which has dropped compatibility with `cmake_minimum_required`
# below 3.5 — well under us, so this floor is safe. If a dependency ever trips
# that, the answer is to fix the dependency, not to lower this number.
cmake_minimum_required(VERSION 3.21)

project(clima
    VERSION 0.1.0
    DESCRIPTION "A native Qt 6 weather app"
    HOMEPAGE_URL "https://github.com/JowiAoun/clima"
    LANGUAGES CXX
)

list(APPEND CMAKE_MODULE_PATH ${CMAKE_CURRENT_SOURCE_DIR}/cmake)

# ---- identity ---------------------------------------------------------------
# One variable, one spelling, everywhere. The reverse-DNS app ID is the name the
# desktop file, the AppStream metainfo, the D-Bus service, the icon theme entry
# and QStandardPaths all have to agree on, and every one of those is a separate
# file that would otherwise carry its own copy of the string. A cache variable
# rather than a plain `set` so a downstream packager can rename the app without
# patching sources — a fork, a nightly channel, a distro rebrand.
set(CLIMA_APP_ID "io.github.JowiAoun.Clima" CACHE STRING
    "Reverse-DNS application ID: desktop file, AppStream, D-Bus, icon name")

# Where a weather service can reach a human about our traffic.
#
# This goes in the User-Agent every request carries, and it is an obligation
# rather than a courtesy: docs/02-data-sources.md §2.9 records MET Norway's
# requirement for an identifying User-Agent with a contact, and notes that a
# generic one gets 403. api.weather.gov enforces the same thing today — an empty
# User-Agent is refused outright.
#
# A cache variable, and this is the one that a downstream packager genuinely
# should override. A distribution shipping a rebuilt Clima will be the source of
# that traffic, and a rate-limit complaint about it should arrive in an inbox
# they read rather than in ours.
set(CLIMA_CONTACT "https://github.com/JowiAoun/clima/issues" CACHE STRING
    "Contact channel in the outbound User-Agent. Packagers: point this at yours")

# ---- options ----------------------------------------------------------------
# Declared now, wired up as each lands. An option that appears the same day as
# the feature it guards is an option nobody remembered to check for, so they go
# in first and are honoured as the code arrives.
option(CLIMA_DEV_TOOLS    "Build development-only tooling (gallery, harnesses)" ON)
option(CLIMA_BUILD_GALLERY "Build the component gallery"                        ON)
option(CLIMA_BUILD_TESTS  "Build the test suite"                                ON)

# ---- Qt ---------------------------------------------------------------------
# 6.8 is the LTS floor from D2. Ask for exactly the components the code uses:
# an unused `find_package` component is a dependency the packager has to satisfy
# for nothing, and — more to the point here — a chance to pull in a module whose
# licence we cannot accept. See cmake/ClimaLicenceGuard.cmake.
include(ClimaLicenceGuard)
include(ClimaEngineGuard)

# Network and Sql are libclima's, not the app's: the engine fetches and the
# engine caches, and app/ reaches both through it. They are asked for here
# rather than in libclima/CMakeLists.txt because find_package is a global
# operation and one call is easier to audit than two.
clima_guard_qt_components(Core Gui Quick Network Sql)

find_package(Qt6 6.8 REQUIRED COMPONENTS Core Gui Quick Network Sql)
qt_standard_project_setup(REQUIRES 6.8)

# Where qt_add_qml_module lays out its module directories. This is not a tidiness
# preference — it is what makes the build's own QML modules discoverable as
# modules rather than as resources baked into one executable:
#
#   * `all_qmllint` can resolve `import Clima`
#   * qmlls (the language server) resolves it too, so an editor stops
#     underlining every type in the module
#   * QML_IMPORT_PATH=build/dev/qml qml … loads our types without a build of the
#     app around them
#
# Without it those three fail in three different and individually confusing
# ways. Do not remove it.
set(QT_QML_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/qml)

# The engine first, because app/ links it. It is also the only directory here
# whose sources are MPL-2.0 rather than GPL-3.0-or-later — see D6, and
# libclima/CMakeLists.txt, which explains why the build file itself is not.
add_subdirectory(libclima)

add_subdirectory(app)

# The component gallery is a separate executable and not a mode of the app, so
# `CLIMA_BUILD_GALLERY=OFF` is a directory that is never entered rather than a
# preprocessor symbol threaded through the product. That is the whole point of
# it having moved: a storybook shipping inside a weather app is wrong on
# principle, and it also costs about eleven hundred lines against the <15 MB
# budget in docs/03-tech-stack.md §3.4.
#
# It depends on `climaqml`, which app/ defines, so it is entered second.
if(CLIMA_BUILD_GALLERY)
    add_subdirectory(gallery)
endif()
