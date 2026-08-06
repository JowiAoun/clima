# SPDX-FileCopyrightText: 2026 Jowi Aoun
# SPDX-License-Identifier: GPL-3.0-or-later
#
# The component gallery: a second executable, built from the same components the
# app is built from and shipping in nothing.
#
#   nix develop -c cmake --build build/dev
#   ./build/dev/gallery/clima-gallery
#   ./build/dev/gallery/clima-gallery --grab g.png --viewport mobile
#
# ---- why it is a binary and not a flag ---------------------------------------
#
# It was a flag: `clima --gallery` opened a component library inside the weather
# app. That is a storybook shipping to end users — about eleven hundred lines of
# rail, filter, specimen and catalogue in a product with a <15 MB budget
# (docs/03-tech-stack.md §3.4) — and the argument against it is not really the
# kilobytes. A developer tool reachable from the product's own binary is a
# developer tool that has to keep working in the product's own release build,
# and every flag it adds is a flag someone can reach by accident.
#
# So: `CLIMA_BUILD_GALLERY=OFF` and this directory is never entered. Nothing in
# app/ mentions it, and `clima --gallery` is now an unknown option with a
# message saying so.
#
# ---- what it borrows ---------------------------------------------------------
#
# Everything. `climaqml` is the Clima QML module built as a static library
# precisely so this can link it: the specimens on the stage are the same
# DetailUvCard.qml the app draws, out of the same resource, or the gallery would
# be reviewing a copy. See the note on Specimen.qml for how a bare file name in
# the catalogue finds its way into a module that is no longer its own.

# ---- the module's files -----------------------------------------------------
#
# Listed one by one, and checked against the disk by scripts/check-qml-files.sh,
# for the reason app/CMakeLists.txt gives at length: a globbed QML module goes
# stale silently and reports the failure at some other file's point of use.
set(clima_gallery_qml_files
    qml/Clima/Gallery/DeviceFrame.qml
    qml/Clima/Gallery/Gallery.qml
    qml/Clima/Gallery/HitTargets.qml
    qml/Clima/Gallery/Main.qml
    qml/Clima/Gallery/ShotSheet.qml
    qml/Clima/Gallery/Specimen.qml
)

set(clima_gallery_js_files
    qml/Clima/Gallery/gallery.js
    qml/Clima/Gallery/contrast.js
    qml/Clima/Gallery/shots.js
)

# Same flattening as app/, and here it is load-bearing rather than tidy.
# `Clima.Gallery` maps to `qrc:/qt/qml/Clima/Gallery/`, exactly one directory
# below `qrc:/qt/qml/Clima/`, which is what lets gallery.js reach the colour
# ramps with `.import "../theme.js"` — the one thing in here that cannot come
# across as a QML type, because a `.pragma library` cannot import a singleton.
# Leave the source layout doubled into the alias and that resolves one directory
# too deep, at runtime, as an import that silently imports nothing.
foreach(gallery_file IN LISTS clima_gallery_qml_files clima_gallery_js_files)
    get_filename_component(gallery_file_name "${gallery_file}" NAME)
    set_source_files_properties("${gallery_file}"
        PROPERTIES QT_RESOURCE_ALIAS "${gallery_file_name}")
endforeach()

# A static library plus a thin executable, exactly the shape app/ uses one
# directory over, and for the same reason turned around: a QML module baked
# straight into an executable is a module only that executable can import.
#
# Here the second consumer is the test suite. `tests/qml/` runs the catalogue —
# instantiating all sixty-odd components and asserting none of them errors is
# the broadest smoke test this tree can have, and it costs almost nothing
# because Gallery.qml and Specimen.qml already know how to do it. That test has
# to be able to `import Clima.Gallery`, and it could not while the module lived
# inside a binary.
#
# STATIC, again for app/CMakeLists.txt's reasons: one repository, one release,
# consumers always built together. Nothing here ships.
qt_add_library(climagalleryqml STATIC)

qt_add_qml_module(climagalleryqml
    URI Clima.Gallery
    VERSION 1.0
    QML_FILES
        ${clima_gallery_qml_files}
        ${clima_gallery_js_files}
    SOURCES
        galleryoptions.h
        galleryoptions.cpp
)

# PUBLIC rather than PRIVATE now that something links this library: the
# generated climagalleryqml_qmltyperegistrations.cpp includes
# `<galleryoptions.h>` by basename with no directory, and so does anything else
# that ends up compiling against this module.
target_include_directories(climagalleryqml PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

# Both, for the reason app/CMakeLists.txt gives at the same line: the plugin
# registers the module with the engine, the backing library carries the usage
# requirements — Qt6::Quick, and the include directory that makes
# `#include "appoptions.h"` in galleryoptions.cpp resolve. Neither one alone
# gets a build that both compiles and runs.
#
# PUBLIC so that the executable and the test suite each get `Clima` as well as
# `Clima.Gallery`; a gallery that could not import the module it exists to show
# would be a short gallery.
target_link_libraries(climagalleryqml PUBLIC climaqml climaqmlplugin)

clima_guard_target(climagalleryqml)

qt_add_executable(clima-gallery
    main.cpp
)

target_link_libraries(clima-gallery PRIVATE climagalleryqml climagalleryqmlplugin)

clima_guard_target(clima-gallery)

# No WIN32_EXECUTABLE and no MACOSX_BUNDLE, unlike the app. This is a console
# program that happens to open a window: it is run from a terminal, it prints
# what it grabbed, and on Windows a GUI subsystem binary would detach from the
# console it was started in and swallow every one of those messages.
