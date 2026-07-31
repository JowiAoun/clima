# SPDX-FileCopyrightText: 2026 Jowi Aoun
# SPDX-License-Identifier: GPL-3.0-or-later
#
# A development harness that prints what the provider adapters make of a
# recorded fixture. Ships nowhere: it is entered only when CLIMA_DEV_TOOLS is
# on, and it is not installed.
#
# It links libclima and Qt Core and nothing else, so clima_forbid_gui() applies
# to it exactly as it does to the tests — a harness that dragged in Qt Gui would
# be a second place the engine's strongest guarantee could be quietly lost.

add_executable(clima-provider-probe main.cpp)

target_link_libraries(clima-provider-probe PRIVATE libclima Qt6::Core)

# The same define the tests take, for the same reason: the fixtures live in the
# source tree, and a path computed from the binary's own location breaks the day
# somebody builds out of a different directory.
target_compile_definitions(clima-provider-probe PRIVATE
    CLIMA_SOURCE_DIR="${CMAKE_SOURCE_DIR}"
)

clima_forbid_gui(clima-provider-probe)
