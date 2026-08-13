#!/usr/bin/env bash
# SPDX-FileCopyrightText: 2026 Jowi Aoun
# SPDX-License-Identifier: GPL-3.0-or-later
#
# Puts the desktop tiles on screen, with something for them to read.
#
#   scripts/widgets-run.sh                          live weather
#   CLIMA_FIXTURE=toronto scripts/widgets-run.sh    recorded, at a frozen clock
#   scripts/widgets-run.sh --columns 2 --widget uv-dial --widget wind-rose
#   scripts/widgets-run.sh --grab tiles.png
#
# Everything after the script name goes to clima-widget untouched, so its
# `--help` is the authority on the flag surface. Which fixture the daemon serves
# is environment rather than a flag, because it is the *other* process's
# business — the same split scripts/dev-run.sh makes for CLIMA_PRESET.
#
# ---- why this exists at all -------------------------------------------------
#
# A tile draws what clima-daemon gives it and nothing else, so a widget host
# with no daemon on the bus has nothing to show and says so. On an installed
# system that never happens: packaging/linux/clima-daemon.service.in makes the
# daemon D-Bus-activatable and the bus starts one the moment the host looks for
# it.
#
# A build tree installs nothing, so there is nothing for the bus to activate,
# and the tiles come up correctly reporting that the weather service is not
# running. Correct is not the same as useful when what you wanted was to look at
# a widget. So this starts one beside them.
#
# ---- and why it does not check whether one is already running ---------------
#
# Because the daemon answers that question better than a probe would. It refuses
# to take a name somebody else owns and exits 5 immediately (daemon/main.cpp),
# so starting a second one costs a process that is gone before this script
# reaches the next line — and the alternative, asking the bus first, is a race
# with a window in it and a dependency on whichever of busctl, gdbus or
# dbus-send this machine happens to have.
set -euo pipefail

here="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
repo="$(cd "$here/.." && pwd)"

preset="${CLIMA_PRESET:-dev}"
build_dir="$repo/build/$preset"

# The host is built through dev-run.sh below, which builds the whole preset —
# so by the time the daemon is needed it exists. Building here as well would be
# a second ninja run to discover that there is nothing to do.
daemon="$build_dir/daemon/clima-daemon"

# `--snapshot` reads a recorded file and never touches the bus, which is what
# CI and the gallery use. Starting a daemon for one would be starting a process
# nothing will talk to.
for arg in "$@"; do
    if [[ "$arg" == "--snapshot" ]]; then
        exec env CLIMA_BINARY="$build_dir/widgets/clima-widget" "$here/dev-run.sh" "$@"
    fi
done

daemon_pid=""

# Only ever our own child, and only if it is still alive — which it will not be
# when the name was already owned. A daemon somebody else started is a shared
# service and stopping it would take the weather away from everything else on
# the desktop, which is the same reason the GNOME extension leaves it running.
cleanup() {
    if [[ -n "$daemon_pid" ]] && kill -0 "$daemon_pid" 2>/dev/null; then
        kill "$daemon_pid" 2>/dev/null || true
    fi
}
trap cleanup EXIT

if [[ -x "$daemon" ]]; then
    daemon_args=()
    [[ -n "${CLIMA_FIXTURE:-}" ]] && daemon_args=(--fixture "${CLIMA_FIXTURE}")

    # Its output belongs on this terminal — a daemon that cannot reach the
    # network says so there, and that is the answer to a tile drawing nothing.
    "$daemon" "${daemon_args[@]}" &
    daemon_pid=$!
fi

# Started and not waited for. The host watches the bus name and subscribes the
# moment it appears — the same path that handles a daemon restarted by hand — so
# a tile that comes up before the daemon has registered fills in on its own
# rather than needing this script to synchronise anything.
CLIMA_BINARY="$build_dir/widgets/clima-widget" "$here/dev-run.sh" "$@"
