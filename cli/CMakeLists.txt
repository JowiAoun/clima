# SPDX-FileCopyrightText: 2026 Jowi Aoun
# SPDX-License-Identifier: GPL-3.0-or-later
#
# clima-cli: the forecast for a status bar, a script or a terminal.
#
# docs/04-architecture.md §4.3 has had a `cli/` directory in its layout since
# the plan was written and docs/05-feature-parity.md §5.6 lists the tool as a
# differentiator the Linux audience will love; this is the first time either
# has been true. One file, because everything it does is done by libclima —
# the providers, the cache, the places, the units — and what is left is
# parsing a command line and printing.
#
# ---- what this target may link ----------------------------------------------
#
# libclima and Qt Core. clima_forbid_gui() makes that a configure-time error
# rather than an intention, and it is the whole reason the unit conversions
# moved into libclima/domain/units.h: a status-bar tool that pulled in Qt Quick
# to print a temperature would start slower than the bar it sits in.

configure_file(cliconfig.h.in cliconfig.h @ONLY)

qt_add_executable(clima-cli
    main.cpp
)

target_include_directories(clima-cli PRIVATE
    ${CMAKE_CURRENT_BINARY_DIR}
    ${CMAKE_SOURCE_DIR}
)

target_link_libraries(clima-cli PRIVATE
    libclima
    Qt6::Core
)

clima_guard_target(clima-cli)
clima_forbid_gui(clima-cli)

include(GNUInstallDirs)

install(TARGETS clima-cli
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)
