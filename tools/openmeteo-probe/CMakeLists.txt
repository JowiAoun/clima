# SPDX-FileCopyrightText: 2026 Jowi Aoun
# SPDX-License-Identifier: GPL-3.0-or-later
#
# The one thing in this repository that talks to a real weather service on
# purpose, and the reason it is a separate executable rather than a test.
#
# The suite proves the adapter is consistent with eight recorded responses. It
# would go on proving that after Open-Meteo renamed a field or changed a unit,
# because a recorded fixture is a guard against regression in us and not
# against drift in them. This is where you look at the live answer and check it
# by eye — see main.cpp.
#
# Guarded by CLIMA_DEV_TOOLS, whose help text already promises "gallery,
# harnesses". A packaging build asks for neither and never enters this
# directory, so nothing that ships can accidentally depend on it.
#
# Qt6::Core only. It links libclima, which is Core, Network and Sql, and
# clima_forbid_gui() below is what keeps that true through this target as well:
# a harness that pulled in QtGui would be a harness that could not be built on
# the headless machine anybody would want to run it from.

add_executable(clima-openmeteo-probe main.cpp)

target_link_libraries(clima-openmeteo-probe PRIVATE libclima Qt6::Core)
target_compile_features(clima-openmeteo-probe PRIVATE cxx_std_17)

target_compile_options(clima-openmeteo-probe PRIVATE
    $<$<CXX_COMPILER_ID:GNU,Clang,AppleClang>:-Wall;-Wextra>
)

clima_forbid_gui(clima-openmeteo-probe)
