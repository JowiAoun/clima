# SPDX-FileCopyrightText: 2026 Jowi Aoun
# SPDX-License-Identifier: GPL-3.0-or-later
#
# The single writer: one process that fetches and one that writes the cache.
#
# ---- what this target may link ----------------------------------------------
#
# libclima and Qt DBus, and clima_forbid_gui() below makes that enforceable
# rather than aspirational. A daemon that draws nothing must not carry Qt Quick:
# it costs startup time on a process that exists to be running when the app is
# not, and it would make a headless server install pull in a GL stack.
#
# The 25 MB headless budget in docs/03-tech-stack.md §3.4 is only reachable
# because of this line.
#
# ---- two targets, and the split is for the tests ----------------------------
#
# `clima_daemon_core` is the service — everything that decides what a snapshot
# says — and it links no D-Bus. `clima-daemon` is the executable that puts it on
# the bus. The split exists so that tests/tst_snapshotservice.cpp can construct
# the service and call GetSnapshot's implementation without a session bus,
# which a runner does not have and a test must not need.

# CLIMA_DAEMON_PATH comes from the top-level CMakeLists, because widgets/
# configures this same template to call what this directory exports. The name,
# path and interface are a contract between two executables, so they are one
# variable and one template rather than two copies that agree today.
configure_file(daemonconfig.h.in daemonconfig.h @ONLY)

add_library(clima_daemon_core STATIC
    snapshotservice.h
    snapshotservice.cpp
)

# The catalogue travels inside the binary rather than beside it. ListWidgets
# then answers the same bytes whether the daemon was started from a build
# directory, a .deb or inside a Flatpak — and there is exactly one copy of the
# file in the repository, which is the entire point of widgets/catalogue.json.
#
# On the core library, because catalogue() is the core's method; Qt carries a
# static library's resources into whichever executable links it.
qt_add_resources(clima_daemon_core "clima_widget_catalogue"
    PREFIX "/clima"
    BASE ${CMAKE_SOURCE_DIR}/widgets
    FILES ${CMAKE_SOURCE_DIR}/widgets/catalogue.json
)

target_include_directories(clima_daemon_core PUBLIC
    ${CMAKE_CURRENT_BINARY_DIR}
    ${CMAKE_SOURCE_DIR}
)

target_link_libraries(clima_daemon_core PUBLIC
    libclima
    Qt6::Core
)

clima_guard_target(clima_daemon_core)
clima_forbid_gui(clima_daemon_core)

qt_add_executable(clima-daemon
    main.cpp
    daemonadaptor.h
    daemonadaptor.cpp
)

target_link_libraries(clima-daemon PRIVATE
    clima_daemon_core
    Qt6::Core
    Qt6::DBus
)

clima_guard_target(clima-daemon)
clima_forbid_gui(clima-daemon)

include(GNUInstallDirs)

install(TARGETS clima-daemon
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)
