# SPDX-FileCopyrightText: 2026 Jowi Aoun
# SPDX-License-Identifier: GPL-3.0-or-later
#
# Reads the dynamic dependencies of a linked binary and fails if a Qt GUI
# library is among them. Run by ctest, not by the build:
#
#   cmake -DCLIMAT_BINARY=<path> -DCLIMAT_OBJDUMP=<objdump> -P ClimatNoGuiBinaryCheck.cmake
#
# This is the honest half of the no-GUI guarantee, and it is deliberately a
# separate mechanism from cmake/ClimatEngineGuard.cmake rather than a duplicate
# of it. The configure-time guard walks CMake's target graph, which is a model
# of the link; this reads DT_NEEDED, which is the link. A dependency that
# arrived outside the target graph - a bare `-lQt6Gui` on a flags variable, a
# transitively linked package that CMake never saw as a target - is invisible to
# the first check and unmissable to this one.
#
# It is pointed at a test executable rather than at libclimat itself, because a
# static archive records no dependencies at all: `libclimat.a` is a bag of object
# files and there is nothing in it to read. The test binaries link libclimat and
# Qt Test and nothing else, so their DT_NEEDED list *is* libclimat's closure plus
# QtCore.

if(NOT CLIMAT_BINARY)
    message(FATAL_ERROR "ClimatNoGuiBinaryCheck: pass -DCLIMAT_BINARY=<path>")
endif()

if(NOT EXISTS "${CLIMAT_BINARY}")
    message(FATAL_ERROR "ClimatNoGuiBinaryCheck: no such file: ${CLIMAT_BINARY}")
endif()

if(NOT CLIMAT_OBJDUMP OR NOT EXISTS "${CLIMAT_OBJDUMP}")
    # Reached only if objdump vanished between configure and test - the test is
    # not registered at all when it was missing at configure time, on the
    # grounds that a check which cannot run is not a check that passed and must
    # not be reported as one.
    message(FATAL_ERROR "ClimatNoGuiBinaryCheck: CLIMAT_OBJDUMP is not a usable program: "
                        "${CLIMAT_OBJDUMP}")
endif()

execute_process(
    COMMAND "${CLIMAT_OBJDUMP}" -p "${CLIMAT_BINARY}"
    OUTPUT_VARIABLE dump
    ERROR_VARIABLE dump_errors
    RESULT_VARIABLE dump_status
)

if(NOT dump_status EQUAL 0)
    message(FATAL_ERROR "ClimatNoGuiBinaryCheck: objdump failed: ${dump_errors}")
endif()

string(REGEX MATCHALL "NEEDED[ \t]+([^\n\r]+)" needed_lines "${dump}")

set(needed "")
foreach(line IN LISTS needed_lines)
    string(REGEX REPLACE "NEEDED[ \t]+" "" library "${line}")
    string(STRIP "${library}" library)
    list(APPEND needed "${library}")
endforeach()

if(NOT needed)
    message(FATAL_ERROR
        "ClimatNoGuiBinaryCheck: read no NEEDED entries from ${CLIMAT_BINARY}.\n"
        "  Either objdump's output format changed or this is not a dynamic executable.\n"
        "  A check that finds nothing must fail rather than pass.")
endif()

set(forbidden
    libQt6Gui
    libQt6Widgets
    libQt6Quick
    libQt6Qml
    libQt6OpenGL
)

set(offenders "")
foreach(library IN LISTS needed)
    foreach(banned IN LISTS forbidden)
        if(library MATCHES "^${banned}\\.")
            list(APPEND offenders "${library}")
        endif()
    endforeach()
endforeach()

if(offenders)
    string(REPLACE ";" "\n    " printable "${offenders}")
    string(REPLACE ";" "\n    " all_needed "${needed}")
    message(FATAL_ERROR
        "\n"
        "  ${CLIMAT_BINARY} links a Qt GUI library, and it must not.\n"
        "\n"
        "  Found:\n    ${printable}\n"
        "\n"
        "  Everything it needs:\n    ${all_needed}\n"
        "\n"
        "  This binary links libclimat and Qt Test and nothing else, so a GUI library\n"
        "  here means the engine reached one. docs/04-architecture.md §4.1 and §4.9:\n"
        "  a Plasma applet, a GNOME Shell extension and climat-cli all link libclimat,\n"
        "  and none of them can link a windowing toolkit.\n")
endif()

message(STATUS "ClimatNoGuiBinaryCheck: ${CLIMAT_BINARY} needs ${needed} - no GUI library")
