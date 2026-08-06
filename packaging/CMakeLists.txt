# SPDX-FileCopyrightText: 2026 Jowi Aoun
# SPDX-License-Identifier: GPL-3.0-or-later
#
# What a Linux install puts on disk beside the binary: the desktop entry, the
# AppStream component, and the icon at eight sizes.
#
# The whole directory is a no-op anywhere else. Windows has no hicolor theme and
# no .desktop, Android packs its own resources through androiddeployqt, and
# generating files for them would mean shipping three formats that only one
# platform reads. The Windows side of packaging is windeployqt and WiX, and it
# lives in packaging/windows/.

if(NOT CMAKE_SYSTEM_NAME STREQUAL "Linux")
    return()
endif()

include(GNUInstallDirs)

# ---- the version has to agree with the changelog ----------------------------
#
# `project(clima VERSION …)` is the one source of truth, and the AppStream
# <releases> block is the one place a human writes release notes. They are
# different kinds of thing — a number and a history — so neither can be
# generated from the other, which leaves them free to disagree.
#
# They disagree exactly once: on the release where somebody bumped the version
# and did not write the note. The symptom is a software centre showing "0.2.0"
# with 0.1.0's description under it, and nobody sees that until it is published.
# So it is a configure-time error here instead.
file(READ ${CMAKE_CURRENT_SOURCE_DIR}/linux/clima.metainfo.xml.in clima_metainfo_text)
string(REGEX MATCH "<release version=\"([0-9]+\\.[0-9]+\\.[0-9]+)\"" _match "${clima_metainfo_text}")

if(NOT CMAKE_MATCH_1)
    message(FATAL_ERROR
        "clima: packaging/linux/clima.metainfo.xml.in has no <release version=\"…\"> entry. "
        "Every release needs a note; see the file's own comment.")
endif()

if(NOT CMAKE_MATCH_1 VERSION_EQUAL PROJECT_VERSION)
    message(FATAL_ERROR
        "clima: version disagreement. project() says ${PROJECT_VERSION}, the newest "
        "<release> in packaging/linux/clima.metainfo.xml.in says ${CMAKE_MATCH_1}. "
        "release-please bumps the first; the release note is written by hand.")
endif()

# ---- the two generated files ------------------------------------------------
#
# @ONLY because both formats use ${…} for their own purposes — a desktop entry's
# Exec field takes field codes, and neither wants CMake expanding something it
# meant to keep.
#
# The output basenames are the app ID, and that is load-bearing rather than
# tidy. A desktop entry whose filename is not the app ID cannot be matched to
# our window by a Wayland compositor; an AppStream file whose basename is not
# the <id> is skipped outright by some catalogue builders. Both failures are
# silent and both look like "the icon is wrong".
configure_file(
    linux/clima.desktop.in
    ${CMAKE_CURRENT_BINARY_DIR}/${CLIMA_APP_ID}.desktop
    @ONLY
)

configure_file(
    linux/clima.metainfo.xml.in
    ${CMAKE_CURRENT_BINARY_DIR}/${CLIMA_APP_ID}.metainfo.xml
    @ONLY
)

install(
    FILES ${CMAKE_CURRENT_BINARY_DIR}/${CLIMA_APP_ID}.desktop
    DESTINATION ${CMAKE_INSTALL_DATADIR}/applications
)

install(
    FILES ${CMAKE_CURRENT_BINARY_DIR}/${CLIMA_APP_ID}.metainfo.xml
    DESTINATION ${CMAKE_INSTALL_DATADIR}/metainfo
)

# ---- the icon ---------------------------------------------------------------
#
# Renamed on the way in rather than stored under the app ID, so that a fork or a
# distro rebuild that overrides CLIMA_APP_ID gets icons matching its own name
# without renaming eight files in git. The sizes are committed PNGs; see
# scripts/icons.sh for why they are generated and committed rather than either
# one alone.
foreach(size IN ITEMS 16 24 32 48 64 128 256 512)
    install(
        FILES ${CMAKE_CURRENT_SOURCE_DIR}/icons/clima-${size}.png
        DESTINATION ${CMAKE_INSTALL_DATADIR}/icons/hicolor/${size}x${size}/apps
        RENAME ${CLIMA_APP_ID}.png
    )
endforeach()

# And the master, which is what a shell at 3x scaling actually wants.
install(
    FILES ${CMAKE_CURRENT_SOURCE_DIR}/icons/clima.svg
    DESTINATION ${CMAKE_INSTALL_DATADIR}/icons/hicolor/scalable/apps
    RENAME ${CLIMA_APP_ID}.svg
)

# ---- the copyright file -----------------------------------------------------
#
# Debian policy §12.5 requires every package to carry one, and this one is not
# a formality. `clima` is statically linked and carries Inter, the GeoNames
# place index and several recorded weather-service payloads inside the
# executable — five licences that are not ours, in a file that would otherwise
# claim the whole thing is GPL. See packaging/linux/copyright, which is the
# SPDX metadata in the source tree collapsed into the format Debian reads.
#
# Installed on every Linux target rather than only in the .deb: a Flatpak and
# an AppImage redistribute exactly the same bytes and incur exactly the same
# obligation.
install(
    FILES ${CMAKE_CURRENT_SOURCE_DIR}/linux/copyright
    DESTINATION ${CMAKE_INSTALL_DATADIR}/doc/clima
)
