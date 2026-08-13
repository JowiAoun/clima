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

option(CLIMA_INSTALL_AUTOSTART
    "Install the daemon's XDG autostart entry into /etc/xdg/autostart" ON)

# ---- the two entries the desktop widgets need -------------------------------
#
# Only where there is a session bus to serve on, which is the same gate the
# top-level CMakeLists puts on building the daemon at all. Installing an
# autostart entry for a binary that was not built is how a package ends up with
# a startup application that fails every login.
#
# The daemon's goes to /etc/xdg/autostart and not to applications/: it is a
# background service, and the whole reason it exists is the days nobody opens
# the weather app. The widget host's goes to applications/ and is a launcher —
# on GNOME the shell extension spawns it and this entry is not how anyone gets
# the tiles, but on every other desktop it is.
if(CMAKE_SYSTEM_NAME STREQUAL "Linux" AND TARGET clima-daemon)
    configure_file(
        linux/clima-daemon.desktop.in
        ${CMAKE_CURRENT_BINARY_DIR}/${CLIMA_APP_ID}.Daemon.desktop
        @ONLY
    )

    configure_file(
        linux/clima-widget.desktop.in
        ${CMAKE_CURRENT_BINARY_DIR}/${CLIMA_APP_ID}.Widget.desktop
        @ONLY
    )

    # ---- and the one that starts the daemon on demand ------------------------
    #
    # Under the prefix, unlike the autostart entry, so it stages, relocates and
    # exports like everything else — and it is therefore NOT behind
    # CLIMA_INSTALL_AUTOSTART. The Flatpak that has to switch that option off is
    # the install that needs this file most: it is the only mechanism by which a
    # sandboxed daemon ever starts on a desktop with no GNOME extension on it.
    #
    # The file name has to BE the bus name. The bus looks up an activation
    # request by matching it against the file's own name, and flatpak's export
    # check requires the same thing before it will publish it — a service file
    # named anything else is silently ignored by both.
    configure_file(
        linux/clima-daemon.service.in
        ${CMAKE_CURRENT_BINARY_DIR}/${CLIMA_APP_ID}.Daemon.service
        @ONLY
    )

    install(
        FILES ${CMAKE_CURRENT_BINARY_DIR}/${CLIMA_APP_ID}.Daemon.service
        DESTINATION ${CMAKE_INSTALL_DATADIR}/dbus-1/services
    )

    # /etc/xdg, absolute, and deliberately not under CMAKE_INSTALL_PREFIX:
    # the XDG autostart search path is a fixed list and a file in
    # /usr/local/etc/xdg/autostart is read by nothing. CMAKE_INSTALL_SYSCONFDIR
    # is "etc" under a prefixed install, so this is spelled out.
    #
    # Which is exactly why it has an off switch. An absolute destination ignores
    # DESTDIR-free staged installs and every read-only /etc there is, and one of
    # those is the Flatpak build sandbox: `cmake --install` there failed with
    # "Read-only file system" *after* compiling the whole application, which is
    # the most expensive possible place to discover a packaging rule.
    #
    # A Flatpak could not use this file anyway — nothing outside the sandbox
    # reads /app, and autostart inside one is the Background portal's job, which
    # is a permission prompt and belongs with the notifications work. So the
    # manifest passes -DCLIMA_INSTALL_AUTOSTART=OFF and the daemon is started by
    # whatever opens it: the app, the GNOME extension, or a widget host.
    #
    # The option itself is declared above this `if`, not inside it. A packager
    # who passes -DCLIMA_INSTALL_AUTOSTART=OFF to a build with no Qt D-Bus would
    # otherwise get "Manually-specified variables were not used by the project"
    # and no way to tell whether it had been honoured or misspelled.
    if(CLIMA_INSTALL_AUTOSTART)
        install(
            FILES ${CMAKE_CURRENT_BINARY_DIR}/${CLIMA_APP_ID}.Daemon.desktop
            DESTINATION /etc/xdg/autostart
        )
    endif()

    install(
        FILES ${CMAKE_CURRENT_BINARY_DIR}/${CLIMA_APP_ID}.Widget.desktop
        DESTINATION ${CMAKE_INSTALL_DATADIR}/applications
    )
endif()

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
