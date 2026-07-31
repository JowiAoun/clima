#!/usr/bin/env bash
# SPDX-FileCopyrightText: 2026 Jowi Aoun
# SPDX-License-Identifier: GPL-3.0-or-later
#
# Runs the hourly-overview prototype. There is no build step: it is pure QML,
# executed by Qt 6's `qml` runtime.
#
#   ./run.sh                          open the page
#   ./run.sh --viewport mobile        …as a phone: five tabs under a nav bar
#   ./run.sh --viewport tablet        …at 834x1112, same shell
#   ./run.sh --tab monthly [...]      open the mobile shell on a given tab
#   ./run.sh --sky night [...]        force the time-of-day background
#   ./run.sh --gallery                open the component library
#   ./run.sh --gallery uv             …on a particular component
#   ./run.sh --gallery x --walk 3     …then step 3 components on
#   ./run.sh --gallery --viewport mobile   …with every specimen in a phone frame
#   ./run.sh --details                the weather-details grid on its own
#   ./run.sh --card Uv                one detail card, alone on the gradient
#   ./run.sh --grab shot.png [...]    render one frame to a PNG and exit
#   ./run.sh --scroll 900 [...]       scroll the page down before grabbing
#   ./run.sh --size 1500x950 [...]    set the window size (headless review)
#   CLIMA_QML=/path/to/qml ./run.sh   use a specific Qt
#   QT_QPA_PLATFORM=xcb ./run.sh      force X11 if Wayland misbehaves
#
# Which shell runs is a function of the window width alone — see viewports.js.
# --viewport pins it and resizes to match; --size on its own works too, so
# `--size 400x800` gets you the phone layout.

set -euo pipefail

here="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
repo="$(cd "$here/../.." && pwd)"

# Finding Qt is no longer this script's business. The CMake build has to reach
# the same conclusion about which Qt this machine is using, so the discovery —
# and the Nix-store environment it has to reconstruct by hand — lives in one
# sourceable place. Read scripts/qt-env.sh for what it sets and why.
# shellcheck source-path=SCRIPTDIR source=../../scripts/qt-env.sh
source "$repo/scripts/qt-env.sh"

if ! clima_qt_env; then
    clima_qt_env_hint >&2
    exit 1
fi
qml_bin="$CLIMA_QML_BIN"

# Any headless capture wants the offscreen platform, and --grab is not always
# the first argument — scan for either.
for _a in "$@"; do
    if [[ "$_a" == "--grab" || "$_a" == "--film" ]]; then
        export QT_QPA_PLATFORM="${QT_QPA_PLATFORM:-offscreen}"
        break
    fi
done

# Which Qt got picked matters only when something is wrong with the pick, so
# it is opt-in. CLIMA_VERBOSE=1 ./run.sh to see it.
if [[ -n "${CLIMA_VERBOSE:-}" ]]; then
    echo "qml runtime: $qml_bin ($("$qml_bin" --version 2>&1 | head -n1))" >&2
fi

exec "$qml_bin" "$here/Main.qml" -- "$@"
