# SPDX-FileCopyrightText: 2026 Jowi Aoun
# SPDX-License-Identifier: GPL-3.0-or-later
#
# Reads the dynamic dependencies of a linked binary and fails if a Qt GUI
# library is among them. Run by ctest, not by the build:
#
#   cmake -DCLIMA_BINARY=<path> -DCLIMA_OBJDUMP=<objdump> -P ClimaNoGuiBinaryCheck.cmake
#
# This is the honest half of the no-GUI guarantee, and it is deliberately a
# separate mechanism from cmake/ClimaEngineGuard.cmake rather than a duplicate
# of it. The configure-time guard walks CMake's target graph, which is a model
# of the link; this reads DT_NEEDED, which is the link. A dependency that
# arrived outside the target graph — a bare `-lQt6Gui` on a flags variable, a
# transitively linked package that CMake never saw as a target — is invisible to
# the first check and unmissable to this one.
#
# It is pointed at a test executable rather than at libclima itself, because a
# static archive records no dependencies at all: `libclima.a` is a bag of object
# files and there is nothing in it to read. The test binaries link libclima and
# Qt Test and nothing else, so their DT_NEEDED list *is* libclima's closure plus
# QtCore.

if(NOT CLIMA_BINARY)
    message(FATAL_ERROR "ClimaNoGuiBinaryCheck: pass -DCLIMA_BINARY=<path>")
endif()

if(NOT EXISTS "${CLIMA_BINARY}")
    message(FATAL_ERROR "ClimaNoGuiBinaryCheck: no such file: ${CLIMA_BINARY}")
endif()

if(NOT CLIMA_OBJDUMP OR NOT EXISTS "${CLIMA_OBJDUMP}")
    # Reached only if objdump vanished between configure and test — the test is
    # not registered at all when it was missing at configure time, on the
    # grounds that a check which cannot run is not a check that passed and must
    # not be reported as one.
    message(FATAL_ERROR "ClimaNoGuiBinaryCheck: CLIMA_OBJDUMP is not a usable program: "
                        "${CLIMA_OBJDUMP}")
endif()

execute_process(
    COMMAND "${CLIMA_OBJDUMP}" -p "${CLIMA_BINARY}"
    OUTPUT_VARIABLE dump
    ERROR_VARIABLE dump_errors
    RESULT_VARIABLE dump_status
)

if(NOT dump_status EQUAL 0)
    message(FATAL_ERROR "ClimaNoGuiBinaryCheck: objdump failed: ${dump_errors}")
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
        "ClimaNoGuiBinaryCheck: read no NEEDED entries from ${CLIMA_BINARY}.\n"
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
        "  ${CLIMA_BINARY} links a Qt GUI library, and it must not.\n"
        "\n"
        "  Found:\n    ${printable}\n"
        "\n"
        "  Everything it needs:\n    ${all_needed}\n"
        "\n"
        "  This binary links libclima and Qt Test and nothing else, so a GUI library\n"
        "  here means the engine reached one. docs/04-architecture.md §4.1 and §4.9:\n"
        "  a Plasma applet, a GNOME Shell extension and clima-cli all link libclima,\n"
        "  and none of them can link a windowing toolkit.\n")
endif()

message(STATUS "ClimaNoGuiBinaryCheck: ${CLIMA_BINARY} needs ${needed} — no GUI library")
