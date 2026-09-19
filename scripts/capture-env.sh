# SPDX-FileCopyrightText: 2026 Jowi Aoun
# SPDX-License-Identifier: GPL-3.0-or-later
#
# shellcheck shell=bash
#
# The pinned environment every headless capture runs under. Source this; do not
# run it. There is no shebang and no execute bit for the reason scripts/qt-env.sh
# gives - the whole point is to put variables into the caller's environment,
# which a subprocess cannot do.
#
#   root=/path/to/repo
#   . "$here/capture-env.sh"
#
# ---- why this is one file rather than a list in each script ------------------
#
# Because three scripts compare images byte for byte against images somebody
# else recorded, and they were only comparable while all three pinned the same
# things. They did not. scripts/golden.sh pinned fourteen variables;
# scripts/shots.sh and scripts/play-shots.sh pinned six and said in a comment
# that they were "the same pinned environment", which had stopped being true.
#
# What that cost: the README device images passed on a laptop and failed on
# every CI run from 2026-09-14 on, on docs/images/widgets.png - the sheet with
# the wind rose, the sun arc and the UV dial on it, which is to say the one made
# almost entirely of Shapes. QSG_RENDER_LOOP was the variable missing, and a
# missing Shape is exactly what the threaded render loop produces here. So the
# list lives in one place now, and a script that captures gets all of it or
# none of it.
#
# The caller still owns its scratch XDG directories and the preferences it
# writes into them, because what a capture pins there is a property of what it
# is photographing rather than of the renderer.

# Software rasterisation, so the picture does not depend on whose GPU took it.
# `unset` rather than a value for the three backend switches: any of them
# arriving from the environment would override the platform chosen here.
export QT_QPA_PLATFORM=offscreen
export LIBGL_ALWAYS_SOFTWARE=1
export GALLIUM_DRIVER=llvmpipe
unset QT_QUICK_BACKEND QSG_RHI_BACKEND QMLSCENE_DEVICE

# The single-threaded render loop. With the default threaded one, grabToImage()
# completes on the render thread and the capture races the scene - which is how
# one run in five came back with the preferences gear missing from two pages,
# and how the widget sheet came back wrong on a runner. scripts/grab.sh carries
# the evidence and its limits, and docs/screenshots.md has the one-level
# difference this does not fix.
export QSG_RENDER_LOOP=basic

# One device pixel per logical pixel, whatever the machine thinks its display
# is. An empty QT_SCREEN_SCALE_FACTORS is not the same as an unset one: it is
# what stops a per-screen factor inherited from a desktop session applying.
export QT_SCALE_FACTOR=1
export QT_ENABLE_HIGHDPI_SCALING=0
export QT_SCREEN_SCALE_FACTORS=
export QT_FONT_DPI=96

# No host platform theme, so no host palette, no host font and no host icon
# theme reaches the scene. The devshell sets `generic` for a GTK reason of its
# own; a capture wants neither.
export QT_QPA_PLATFORMTHEME=

# One locale and one clock. Both decide glyphs and both decide strings.
export LC_ALL=C.UTF-8
export LANG=C.UTF-8
export TZ=UTC

export QT_FORCE_STDERR_LOGGING=1

# The font configuration that names no font directory at all - the typeface is
# inside the binary - which is what stops a host font being substituted. This is
# the line that makes these images comparable across machines rather than merely
# repeatable on one.
#
# $root is the caller's repository root, and the `:?` is the whole of this
# file's error handling: a caller that forgot to set it gets a named failure
# here instead of a capture against the host's fonts, which would look like a
# rendering bug in the app.
# shellcheck disable=SC2154  # assigned by the caller, by contract - see above
export FONTCONFIG_FILE="${root:?capture-env.sh: set \$root to the repository root first}/tests/golden/fontconfig.conf"
