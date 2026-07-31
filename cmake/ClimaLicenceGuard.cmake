# SPDX-FileCopyrightText: 2026 Jowi Aoun
# SPDX-License-Identifier: GPL-3.0-or-later
#
# Refuses to configure a build that asks for a Qt module we cannot ship.
#
# The Qt Company dual-licenses Qt, and not every module gets the same second
# licence. Most of Qt is LGPLv3-or-commercial, which we can use in a
# dynamically linked GPL-3.0-or-later app. A handful of modules are
# GPLv3-or-commercial only — Qt Charts, Qt Graphs, Qt Lottie, Qt Quick 3D and
# Qt VirtualKeyboard among them. Linking one of those does not stop the app
# building and does not print a warning; it silently makes the whole binary
# GPLv3-only, which retroactively breaks the MPL-2.0 promise libclima makes to
# the Plasma applet and the GNOME extension that are supposed to reuse it.
# That is risk R2 in docs/08-risks.md, and the reason it is a risk at all is
# that nothing in the toolchain tells you it happened.
#
# So the toolchain tells you here. This is a source-level guard: it catches
# `find_package(Qt6 COMPONENTS Charts)` and a `target_link_libraries` naming a
# banned target. It cannot catch a module that arrives transitively through
# some other package, and it makes no claim to. The honest binary-level check —
# reading the actual DT_NEEDED list of the linked executable — belongs in CI,
# where there is a linked executable to read. Until then this covers the way
# the mistake actually gets made: someone adds a component because they want a
# chart, and it works.

set(CLIMA_BANNED_QT_MODULES
    Charts              # QtCharts          — GPLv3 / commercial
    Graphs              # QtGraphs          — GPLv3 / commercial (Charts' successor)
    Lottie              # QtLottieAnimation — GPLv3 / commercial
    Quick3D             # QtQuick3D         — GPLv3 / commercial
    VirtualKeyboard     # QtVirtualKeyboard — GPLv3 / commercial
    CACHE INTERNAL "Qt modules whose licence would relicense the whole app"
)

# Why each one is out, printed with the failure. A refusal that does not say
# what to do instead is a refusal someone works around.
function(_clima_banned_reason module out)
    set(${out} "GPLv3-or-commercial from The Qt Company; linking it relicenses \
the whole application and breaks libclima's MPL-2.0 promise (docs/08-risks.md, R2)"
        PARENT_SCOPE)
endfunction()

# Call before find_package(Qt6 ...) with the same component list.
function(clima_guard_qt_components)
    foreach(component IN LISTS ARGN)
        if(component IN_LIST CLIMA_BANNED_QT_MODULES)
            _clima_banned_reason(${component} reason)
            message(FATAL_ERROR
                "Qt6::${component} is not permitted in Clima.\n"
                "  ${reason}\n"
                "  If you need what it does, write it against the scene graph "
                "instead — that is what ClimaCharts is for (docs/04-architecture.md §4.6).")
        endif()
    endforeach()
endfunction()

# Call after the link line for a target is complete. Walks INTERFACE and
# private link libraries looking for a banned Qt target by name.
function(clima_guard_target target)
    get_target_property(libs ${target} LINK_LIBRARIES)
    if(NOT libs)
        return()
    endif()
    foreach(lib IN LISTS libs)
        foreach(banned IN LISTS CLIMA_BANNED_QT_MODULES)
            if(lib STREQUAL "Qt6::${banned}" OR lib STREQUAL "Qt::${banned}")
                _clima_banned_reason(${banned} reason)
                message(FATAL_ERROR
                    "Target '${target}' links ${lib}, which is not permitted in Clima.\n"
                    "  ${reason}")
            endif()
        endforeach()
    endforeach()
endfunction()
