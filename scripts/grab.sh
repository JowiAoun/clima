#!/usr/bin/env bash
# SPDX-FileCopyrightText: 2026 Jowi Aoun
# SPDX-License-Identifier: GPL-3.0-or-later
#
# Renders one settled frame to a PNG, reproducibly.
#
# This is the single entry point for capture: CI calls it, the golden-image
# tests will call it, and the README's screenshots come out of it. Two runs an
# hour apart produce the same bytes, and that — not the picture — is the
# product. Everything below the usage block exists to keep that true.
#
#   scripts/grab.sh out.png                      the forecast, default window
#   scripts/grab.sh --size 1340x900 out.png      at a given size
#   scripts/grab.sh --viewport mobile out.png    the phone shell
#   scripts/grab.sh --viewport mobile --tab monthly out.png
#   scripts/grab.sh --scroll 900 out.png         below the fold
#   scripts/grab.sh --sky night out.png          a forced time of day
#   scripts/grab.sh --env -- <command> [args…]   run something else in here
#
# The convention is one sentence: THE LAST ARGUMENT IS THE FILE TO WRITE, and
# everything before it goes to the binary untouched. It has to end in .png,
# which is what turns `scripts/grab.sh --size 1340x900` — a flag whose value
# would otherwise become the filename — into an error rather than a surprise.
#
# `scripts/dev-run.sh --help` lists the flags. This script's own options are the
# two above and nothing else; the rest of its behaviour is environment, and it
# is dev-run.sh's environment because dev-run.sh is what ends up running:
#
#   CLIMA_PRESET=golden   capture from the RelWithDebInfo build
#   CLIMA_NO_BUILD=1      skip the build; CI has already done it
#   CLIMA_BINARY=<path>   capture a different executable out of the same build,
#                         the component gallery being the one that wants it
#
# --env runs any command under the same pinned environment. film.sh is the
# caller that needs it: a contact sheet has to come off the same renderer, the
# same scale factor and the same fonts as a still, or comparing the two is
# comparing two machines.

set -euo pipefail

here="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

if [[ $# -eq 0 || "$1" == "-h" || "$1" == "--help" ]]; then
    sed -n '5,37p' "${BASH_SOURCE[0]}" | sed 's/^# \?//'
    exit 1
fi

# ---- the platform -----------------------------------------------------------
#
# Assigned, not defaulted. Every other variable in this file is a knob somebody
# might have a reason to turn; this one is the difference between a capture and
# a screenshot of whatever window happened to be on top, and honouring an
# inherited value would mean a developer's stray `export QT_QPA_PLATFORM=xcb`
# quietly changing what CI compares against.
export QT_QPA_PLATFORM=offscreen

# ---- the renderer -----------------------------------------------------------
#
# llvmpipe, and deliberately not QT_QUICK_BACKEND=software.
#
# 29 files in the QML module set `Shape.preferredRendererType:
# Shape.CurveRenderer` — `grep -rl CurveRenderer app/qml` to recount after a
# refactor. The software backend has no curve renderer at all: it draws every
# Shape through QPainter and ignores the request without a word. A capture taken
# that way is a photograph of a renderer nobody runs, and it is not a small
# difference — the antialiasing on every glyph, badge, fillet and chart edge in
# this app comes off a different code path than the one a user sees. llvmpipe is
# a real GL implementation that happens to run on the CPU, so it takes the path
# the GPU takes and gets there without one.
#
# The measured caveat, which matters more than the intent: on Qt 6.11 the
# offscreen platform plugin advertises no OpenGL capability, so Qt Quick loads
# its software adapter before either of these variables is consulted. Today they
# change nothing — `QSG_INFO=1 scripts/grab.sh x.png` prints "Loading backend
# software" and the PNG is byte-identical with them set or unset. They stay
# because the day a headless capture does reach GL — xvfb behind the xcb
# platform, or an offscreen plugin that grows the capability — is the day this
# must be llvmpipe rather than whatever driver the host has, and a golden image
# that silently changed renderer between two commits is a long afternoon.
#
# Until that day: no headless capture exercises the curve renderer, here or in
# the prototype's six capture paths. Worth knowing before a golden image is
# taken as proof that a Shape draws correctly.
export LIBGL_ALWAYS_SOFTWARE=1
export GALLIUM_DRIVER=llvmpipe

# A stray QT_QUICK_BACKEND or QSG_RHI_BACKEND in the caller's environment would
# undo the paragraph above from outside. Unset rather than overwrite: there is
# no value for these that means "the default", so the only way to pin them is to
# make them absent.
unset QT_QUICK_BACKEND QSG_RHI_BACKEND QMLSCENE_DEVICE

# ---- scale ------------------------------------------------------------------
#
# All four, because Qt has four separate ways to arrive at a device pixel ratio
# and setting three of them leaves the fourth deciding. A capture on a HiDPI
# laptop is otherwise twice the size of the same capture in CI, which does not
# fail a byte comparison so much as make it meaningless.
export QT_SCALE_FACTOR=1
export QT_ENABLE_HIGHDPI_SCALING=0
export QT_SCREEN_SCALE_FACTORS=
export QT_FONT_DPI=96

# ---- text, locale and time --------------------------------------------------
#
# QLocale reads the locale for its decimal separator and its month names, and
# QDateTime reads the zone. The house rule is that nothing in this app reads the
# wall clock, so TZ should be inert — pinned anyway, because "should be inert"
# is a claim about today's code and this file is the thing that catches the
# commit that stops it being true.
#
# The empty platform theme is "no desktop theme": no GTK, no host font
# rendering settings, no dark-mode preference arriving from a session bus. Note
# that qt-env.sh upgrades an empty value to `generic` for a Nix-store Qt — the
# two mean the same thing here, since the offscreen integration advertises no
# theme of its own and Qt falls back to the generic one either way.
export QT_QPA_PLATFORMTHEME=
export LC_ALL=C.UTF-8
export LANG=C.UTF-8
export TZ=UTC

# Qt decides stderr has no console under `offscreen` and swallows QML warnings,
# which turns a broken binding into a picture that is quietly missing a card.
export QT_FORCE_STDERR_LOGGING=1

# ---- what this does NOT pin -------------------------------------------------
#
# The font *face* used to be the headline here: nothing in the QML named a
# family, so Qt rendered every string in whatever fontconfig picked and two
# machines with different font packages produced different pixels no matter how
# many variables this file exported. That is closed. The app ships Inter and
# installs it as the application font before the engine loads anything — see
# app/appfont.cpp — so the glyphs come out of the binary. Proof, if it is ever
# in doubt: run a capture with FONTCONFIG_FILE pointing at a config with no
# font directories in it, so the host has no fonts at all, and the page still
# renders in Inter.
#
# What is left is how those glyphs are *rasterised*. Hinting, antialiasing and
# subpixel order are fontconfig's to decide, per host, and Qt asks it — with no
# platform theme and no QFont::setHintingPreference from us, the answer arrives
# through QFontconfigDatabase all the same. Measured with the same binary and
# the same font, host fontconfig against an empty one:
#
#     Inter 12 px, one 60-character line
#       host   advance 341.266   line height 14.5156   ascent 11.625
#       empty  advance 336       line height 15        ascent 12
#
# Fractional against integer: one machine is positioning glyphs at subpixel
# offsets and the other is snapping them to the grid, which moves wrap points
# and shifts a whole page by a pixel or two. Same code, same face, different
# pixels.
#
# So golden images remain comparable only against captures from the same image,
# and the remaining fix is a fontconfig file in the repo with hintstyle,
# antialias and rgba stated outright, exported from here as FONTCONFIG_FILE.
# That is a change to what every recorded image looks like, so it belongs with
# the commit that records the first ones rather than ahead of it.
#
# And the clock. Measured on the plain `--size 1340x900` scene: 20 captures in a
# row are byte-identical on an idle machine, and roughly one in ten differs on a
# busy one. The difference is 35 pixels, one channel level each, in a 7x12 box
# around the pager chevron at (89, 852) — a Shape that settled half a pixel
# further along because the shutter landed one frame off.
#
# It is not the C++ port. Sixteen captures of each, interleaved, under twenty-two
# spinning processes: the app and prototype/hourly-overview/run.sh produced the
# same alternate image at the same rate. It is that "settled" is a number of
# milliseconds rather than a state — see app/devtools/screenshotcontroller.h,
# which says so — and a loaded machine fits a different number of frames into
# the same number of milliseconds.
#
# What that costs the golden-image work: a byte comparison is still the right
# test, and it will flake on a busy runner. Budget a retry, or compare with a
# tolerance of one level over a pixel count. Either way, do not spend an
# afternoon bisecting a thirty-five-pixel diff — it is this.

# --env: hand the environment to something else. Placed after the exports and
# before the argument handling, because a command run this way wants all of the
# above and none of the below.
if [[ "$1" == "--env" ]]; then
    shift
    [[ "${1:-}" == "--" ]] && shift
    if [[ $# -eq 0 ]]; then
        echo "grab.sh: --env needs a command to run" >&2
        exit 1
    fi
    exec "$@"
fi

# The last argument, and then everything but the last argument. Both spellings
# are load-bearing at $# == 1, where the second has to produce an empty list
# rather than an error.
out="${*: -1}"
set -- "${@:1:$#-1}"

case "$out" in
    -*)
        echo "grab.sh: the last argument is the file to write, and \"$out\" is a flag." >&2
        exit 1 ;;
    *.png | *.PNG) ;;
    *)
        echo "grab.sh: the last argument is the file to write and has to end in .png — got \"$out\"." >&2
        echo "         usage: scripts/grab.sh [clima options…] <out.png>" >&2
        exit 1 ;;
esac

# --grab goes last so that it wins: QCommandLineParser keeps the final value of
# a repeated option, and a caller who passes their own --grab has said where the
# file goes twice. The one they named as the last argument is the one this
# script promised to write.
exec "$here/dev-run.sh" "$@" --grab "$out"
