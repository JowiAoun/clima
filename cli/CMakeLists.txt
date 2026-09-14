# SPDX-FileCopyrightText: 2026 Jowi Aoun
# SPDX-License-Identifier: GPL-3.0-or-later
#
# climat-cli: the forecast for a status bar, a script or a terminal.
#
# docs/04-architecture.md §4.3 has had a `cli/` directory in its layout since
# the plan was written and docs/05-feature-parity.md §5.6 lists the tool as a
# differentiator the Linux audience will love; this is the first time either
# has been true. One file, because everything it does is done by libclimat -
# the providers, the cache, the places, the units - and what is left is
# parsing a command line and printing.
#
# ---- what this target may link ----------------------------------------------
#
# libclimat and Qt Core. climat_forbid_gui() makes that a configure-time error
# rather than an intention, and it is the whole reason the unit conversions
# moved into libclimat/domain/units.h: a status-bar tool that pulled in Qt Quick
# to print a temperature would start slower than the bar it sits in.

configure_file(cliconfig.h.in cliconfig.h @ONLY)

qt_add_executable(climat-cli
    main.cpp
)

target_include_directories(climat-cli PRIVATE
    ${CMAKE_CURRENT_BINARY_DIR}
    ${CMAKE_SOURCE_DIR}
)

target_link_libraries(climat-cli PRIVATE
    libclimat
    Qt6::Core
)

climat_guard_target(climat-cli)
climat_forbid_gui(climat-cli)

include(GNUInstallDirs)

install(TARGETS climat-cli
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)
