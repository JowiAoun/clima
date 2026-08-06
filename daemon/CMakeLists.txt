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

# The object path is the app ID with its dots turned into slashes, computed
# here so that C++ never has to. Doing it at run time would make a mistake in
# it look like a service that registers and then answers nothing.
string(REPLACE "." "/" CLIMA_DAEMON_PATH "/${CLIMA_APP_ID}")
set(CLIMA_DAEMON_PATH "${CLIMA_DAEMON_PATH}/Daemon")

configure_file(daemonconfig.h.in daemonconfig.h @ONLY)

qt_add_executable(clima-daemon
    main.cpp
    daemonadaptor.h
    daemonadaptor.cpp
    snapshotservice.h
    snapshotservice.cpp
)

# The catalogue travels inside the binary rather than beside it. ListWidgets
# then answers the same bytes whether the daemon was started from a build
# directory, a .deb or inside a Flatpak — and there is exactly one copy of the
# file in the repository, which is the entire point of widgets/catalogue.json.
qt_add_resources(clima-daemon "clima_widget_catalogue"
    PREFIX "/clima"
    BASE ${CMAKE_SOURCE_DIR}/widgets
    FILES ${CMAKE_SOURCE_DIR}/widgets/catalogue.json
)

target_include_directories(clima-daemon PRIVATE
    ${CMAKE_CURRENT_BINARY_DIR}
    ${CMAKE_SOURCE_DIR}
)

target_link_libraries(clima-daemon PRIVATE
    libclima
    Qt6::Core
    Qt6::DBus
)

clima_guard_target(clima-daemon)
clima_forbid_gui(clima-daemon)

include(GNUInstallDirs)

install(TARGETS clima-daemon
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)
