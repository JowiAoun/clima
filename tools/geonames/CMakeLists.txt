# SPDX-FileCopyrightText: 2026 Jowi Aoun
# SPDX-License-Identifier: GPL-3.0-or-later
#
# `clima-geocode` — the manual harness for both halves of geocoding.
#
# The suite proves the parser against four recorded responses and the reverse
# index against four coordinates. Neither of those notices the day Open-Meteo
# renames a field, and neither of them shows a human what "search Kigali"
# actually looks like. This does:
#
#   nix develop -c ./build/dev/tools/geonames/clima-geocode Kigali
#   nix develop -c ./build/dev/tools/geonames/clima-geocode 43.65,-79.38
#
# It sits in tools/geonames/ next to pack.mjs, which builds the index it reads,
# so that the tool that makes the data and the tool that looks at it are one
# directory rather than two.
#
# Guarded by CLIMA_DEV_TOOLS, whose help text already promises "gallery,
# harnesses". A packaging build never enters this directory.
#
# Qt6::Core only, and clima_forbid_gui() is what keeps it that way: a harness
# that pulled in QtGui could not be run from the headless machine anybody would
# want to run it from.

add_executable(clima-geocode main.cpp)

target_link_libraries(clima-geocode PRIVATE libclima Qt6::Core)
target_compile_features(clima-geocode PRIVATE cxx_std_17)

target_compile_options(clima-geocode PRIVATE
    $<$<CXX_COMPILER_ID:GNU,Clang,AppleClang>:-Wall;-Wextra>
)

clima_forbid_gui(clima-geocode)
