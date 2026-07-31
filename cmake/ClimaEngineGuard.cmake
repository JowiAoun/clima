# SPDX-FileCopyrightText: 2026 Jowi Aoun
# SPDX-License-Identifier: GPL-3.0-or-later
#
# Refuses to configure a build in which libclima can see a GUI.
#
# docs/04-architecture.md §4.1, design principle 3: "The engine has no GUI
# dependency. `libclima` links Qt Core/Network only, so it can be reused by a
# CLI, a Plasma applet, a GNOME extension, or a widget." §4.9 says the same
# thing from the other end: "The Plasma applet and GNOME extension are exactly
# why `libclima` is MPL-2.0 and GUI-free."
#
# The promise is easy to make and easy to break, and breaking it produces no
# error of any kind. Somebody wants a colour, writes QColor, adds Qt6::Gui to
# the link line because the compiler asked for it, and the build succeeds. The
# damage is not visible until months later, when the GNOME extension — which
# runs inside gnome-shell and cannot link a windowing toolkit — turns out to be
# unbuildable against the engine it was supposed to reuse, and the fix is not a
# link line but every type that leaked through the API in between.
#
# So the build refuses. This is not a style rule with a lint attached; it is the
# architectural boundary that three future frontends depend on, checked at the
# only moment it is cheap to fix.
#
# ---- what it actually checks -------------------------------------------------
#
# The *transitive* link closure, walked over real CMake targets. Qt6::Quick does
# not name Qt6::Gui in a way a grep of one CMakeLists.txt would find — it
# arrives through Qt6::Quick's own INTERFACE_LINK_LIBRARIES — so a one-level
# check would pass a target that links QtQuick. This one follows every edge.
#
# It cannot see a dependency that arrives outside CMake's target graph: a bare
# `-lQt6Gui` on a flags variable, or a header included from a directory nobody
# declared. That is what the binary-level test in tests/CMakeLists.txt is for —
# it reads the actual dynamic dependencies of a linked executable, which is the
# honest check, and which needs something linked before it can run.

# Anything that is, or drags in, a windowing system. Qt6::Gui is the one that
# matters; the rest are listed because linking any of them means Qt6::Gui
# arrived and a message naming the module somebody actually added is worth more
# than one naming its dependency.
set(CLIMA_GUI_QT_TARGETS
    Gui
    Widgets
    Quick
    QuickControls2
    Qml            # not a windowing dependency, but the engine must not need a
                   # QML engine either: a CLI and a Plasma applet each bring
                   # their own, and a libclima that registers QML types has
                   # decided for them.
    OpenGL
    OpenGLWidgets
    PrintSupport
    Svg
    SvgWidgets
    CACHE INTERNAL "Qt targets libclima may never reach, directly or transitively"
)

# Resolves aliases and strips the generator expressions Qt puts in its interface
# link lists. `$<LINK_ONLY:Qt6::Foo>` is a real edge and has to be followed;
# anything else with a `$<` in it is a conditional this function does not try to
# evaluate, and is skipped — which is a deliberate false-negative rather than a
# guess.
function(_clima_normalise_link_entry entry out)
    set(value "${entry}")

    if(value MATCHES "^\\$<LINK_ONLY:(.+)>$")
        set(value "${CMAKE_MATCH_1}")
    endif()

    if(value MATCHES "\\$<")
        set(${out} "" PARENT_SCOPE)
        return()
    endif()

    if(TARGET ${value})
        get_target_property(aliased ${value} ALIASED_TARGET)
        if(aliased)
            set(value "${aliased}")
        endif()
    endif()

    set(${out} "${value}" PARENT_SCOPE)
endfunction()

# Breadth-first over LINK_LIBRARIES and INTERFACE_LINK_LIBRARIES. Returns the
# full closure including the starting target, and the path by which each banned
# target was reached — the path is the whole value of the message: "libclima →
# Qt6::Quick → Qt6::Gui" tells you which line to delete, and "Qt6::Gui is
# linked" does not.
function(_clima_walk_link_closure target out_visited out_offenders out_paths)
    set(visited "")
    set(offenders "")
    set(paths "")

    # Two parallel lists used as one queue of (target, path-to-it) pairs. CMake
    # has no structures, and encoding the pair in one string would break on the
    # separator the day a target name contains it.
    set(queue "${target}")
    set(queue_paths "${target}")

    while(queue)
        list(POP_FRONT queue current)
        list(POP_FRONT queue_paths current_path)

        if(current IN_LIST visited)
            continue()
        endif()
        list(APPEND visited "${current}")

        foreach(banned IN LISTS CLIMA_GUI_QT_TARGETS)
            if(current STREQUAL "Qt6::${banned}" OR current STREQUAL "Qt::${banned}")
                list(APPEND offenders "${current}")
                list(APPEND paths "${current_path}")
            endif()
        endforeach()

        # A non-target entry is a bare library name or a file path. There is
        # nothing to walk into, and no Qt module arrives that way in this build.
        if(NOT TARGET ${current})
            continue()
        endif()

        get_target_property(target_type ${current} TYPE)
        set(link_properties INTERFACE_LINK_LIBRARIES)
        if(NOT target_type STREQUAL "INTERFACE_LIBRARY")
            list(APPEND link_properties LINK_LIBRARIES)
        endif()

        foreach(property IN LISTS link_properties)
            get_target_property(dependencies ${current} ${property})
            if(NOT dependencies)
                continue()
            endif()
            foreach(dependency IN LISTS dependencies)
                _clima_normalise_link_entry("${dependency}" normalised)
                if(normalised STREQUAL "")
                    continue()
                endif()
                list(APPEND queue "${normalised}")
                list(APPEND queue_paths "${current_path} -> ${normalised}")
            endforeach()
        endforeach()
    endwhile()

    set(${out_visited} "${visited}" PARENT_SCOPE)
    set(${out_offenders} "${offenders}" PARENT_SCOPE)
    set(${out_paths} "${paths}" PARENT_SCOPE)
endfunction()

# Call after the target's link line is complete.
function(clima_forbid_gui target)
    _clima_walk_link_closure(${target} visited offenders paths)

    if(NOT offenders)
        return()
    endif()

    list(REMOVE_DUPLICATES offenders)
    string(REPLACE ";" "\n    " printable_paths "${paths}")

    message(FATAL_ERROR
        "\n"
        "  ${target} reaches a Qt GUI module, and it must not.\n"
        "\n"
        "  How it gets there:\n"
        "    ${printable_paths}\n"
        "\n"
        "  libclima is the GUI-free engine. docs/04-architecture.md §4.1 and §4.9:\n"
        "  a Plasma 6 applet, a GNOME Shell extension and clima-cli are all meant to\n"
        "  link it, and none of them can link a windowing toolkit. A GUI dependency\n"
        "  here does not fail to build — it fails to be reusable, months later, in a\n"
        "  repository that is not this one.\n"
        "\n"
        "  If you needed a type from QtGui, you almost certainly wanted one of:\n"
        "    * a colour        — the design tokens live in app/qml/Clima/Theme.qml;\n"
        "                        the engine has no opinion about colour.\n"
        "    * an image        — hand the caller the bytes and let it decode.\n"
        "    * a QPointF       — use libclima's own value types, or std::pair.\n"
        "\n"
        "  If the dependency is genuinely necessary, the piece that needs it belongs\n"
        "  in app/, not in the engine.\n")
endfunction()
