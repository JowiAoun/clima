#!/usr/bin/env bash
# SPDX-FileCopyrightText: 2026 Jowi Aoun
# SPDX-License-Identifier: GPL-3.0-or-later
#
# Builds the app if it needs building, then runs it.
#
# The successor to prototype/hourly-overview/run.sh: same flags, same habits,
# one extra step in the middle that the prototype never needed because pure QML
# has nothing to compile. Everything after the script name is handed to the
# binary untouched, so `--help` is the authority on the flag surface and the
# list below is only a reminder.
#
#   scripts/dev-run.sh                        open the forecast
#   scripts/dev-run.sh --viewport mobile      …as a phone: five tabs under a nav bar
#   scripts/dev-run.sh --viewport tablet      …at 834x1112, same shell
#   scripts/dev-run.sh --tab monthly [...]    open the mobile shell on a given tab
#   scripts/dev-run.sh --sky night [...]      force the time-of-day background
#   scripts/dev-run.sh --metric wind [...]    select a chart metric
#   scripts/dev-run.sh --grab shot.png [...]  render one frame to a PNG and exit
#   scripts/dev-run.sh --scroll 900 [...]     scroll the page down before grabbing
#   scripts/dev-run.sh --size 1500x950 [...]  set the window size
#   scripts/dev-run.sh --help                 the binary's own list, always current
#
# Knobs, all environment because they are not the app's business and the app's
# parser rejects flags it does not know:
#
#   CLIMA_PRESET=golden     which CMake preset to build and run (default: dev)
#   CLIMA_NO_BUILD=1        skip the build; CI has already done it
#   CLIMA_BINARY=<path>     run a different executable out of the same build —
#                           the component gallery is its own binary, and this is
#                           how it and anything after it get the same launcher
#                           rather than a second copy of this file
#   CLIMA_VERBOSE=1         say which Qt and which binary
#   CLIMA_QML=/path/to/qml  pin the Qt, as in the prototype
#   QT_QPA_PLATFORM=xcb     force X11 if Wayland misbehaves
#
# Do NOT call this directly for a capture that has to be reproducible. Call
# scripts/grab.sh, which pins the dozen environment variables that decide what
# the pixels look like and then calls this.

set -euo pipefail

here="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
repo="$(cd "$here/.." && pwd)"

preset="${CLIMA_PRESET:-dev}"
build_dir="$repo/build/$preset"
binary="${CLIMA_BINARY:-$build_dir/app/clima}"

# ---- reaching the toolchain -------------------------------------------------
#
# cmake and ninja come from the Nix devshell and are on PATH only inside it. Two
# ways to be inside it — `nix develop` then this script, or this script from a
# bare terminal — and both have to work, because the first is what a developer
# does all afternoon and the second is what someone does on their first day.
#
# So: use the cmake we can see, and reach for `nix develop` only when we cannot.
# The check is `command -v` rather than a $IN_NIX_SHELL test on purpose, since a
# machine with its own cmake and Qt is a supported way to build this and should
# not be dragged through Nix for it.
clima_cmake() {
    if command -v cmake >/dev/null 2>&1; then
        cmake "$@"
    elif command -v nix >/dev/null 2>&1; then
        nix develop "$repo" --command cmake "$@"
    else
        cat >&2 <<'EOF'
error: no cmake, and no nix to borrow one from.

  Nix     : nix develop            (this repo's flake), then re-run
  Debian  : sudo apt install cmake ninja-build qt6-declarative-dev
  Fedora  : sudo dnf install cmake ninja-build qt6-qtdeclarative-devel
  Arch    : sudo pacman -S cmake ninja qt6-declarative
EOF
        return 1
    fi
}

# `cmake --build --preset` reads CMakePresets.json from the working directory
# and, unlike a configure, has no -S to point it somewhere else. So the build
# runs from the repo root — in a subshell, because the app inherits our working
# directory and `--grab shot.png` has to mean the directory the user typed it
# in, not this one.
if [[ -z "${CLIMA_NO_BUILD:-}" ]]; then
    (
        cd "$repo"
        [[ -f "$build_dir/CMakeCache.txt" ]] || clima_cmake --preset "$preset"
        clima_cmake --build --preset "$preset"
    )
fi

if [[ ! -x "$binary" ]]; then
    echo "error: no executable at $binary" >&2
    if [[ -n "${CLIMA_BINARY:-}" ]]; then
        echo "       CLIMA_BINARY names it — check the path, or unset it for the app." >&2
    elif [[ -n "${CLIMA_NO_BUILD:-}" ]]; then
        echo "       CLIMA_NO_BUILD is set — unset it, or build the '$preset' preset first." >&2
    else
        echo "       the build reported success and produced nothing; check the preset name." >&2
    fi
    exit 1
fi

# ---- the Qt this binary was linked against ----------------------------------
#
# Finding Qt is not this script's business either — scripts/qt-env.sh answers it
# once, for the prototype, the build and this. What matters here is that the
# answer is not optional. A binary built against a Nix-store Qt is not
# env-wrapped: without the QML_IMPORT_PATH and QT_PLUGIN_PATH that qt-env.sh
# exports it cannot find QtQuick or a platform plugin, and the way it says so is
# to exit 255 having printed nothing at all. Failing here, with the hint, beats
# handing that back to whoever ran this.
# shellcheck source-path=SCRIPTDIR source=qt-env.sh
source "$repo/scripts/qt-env.sh"

if ! clima_qt_env; then
    clima_qt_env_hint >&2
    exit 1
fi

# Any headless capture wants the offscreen platform, and --grab is not always
# the first argument — scan for either. Kept as a default rather than an
# assignment so that grab.sh, which has already pinned this and eleven other
# variables, is not overruled by the script it called.
for _a in "$@"; do
    if [[ "$_a" == "--grab" || "$_a" == "--film" ]]; then
        export QT_QPA_PLATFORM="${QT_QPA_PLATFORM:-offscreen}"
        break
    fi
done

# Which Qt got picked matters only when something is wrong with the pick, so it
# is opt-in. CLIMA_VERBOSE=1 scripts/dev-run.sh to see it.
if [[ -n "${CLIMA_VERBOSE:-}" ]]; then
    echo "preset:   $preset" >&2
    echo "binary:   $binary" >&2
    echo "qt:       ${CLIMA_QT_PREFIX:-<unknown prefix>}" >&2
    echo "platform: ${QT_QPA_PLATFORM:-<default>}" >&2
fi

exec "$binary" "$@"
