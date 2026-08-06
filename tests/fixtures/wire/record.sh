#!/usr/bin/env bash
# SPDX-FileCopyrightText: 2026 Jowi Aoun
# SPDX-License-Identifier: GPL-3.0-or-later
#
# How the snapshots beside this file were captured. Provenance you can run
# rather than provenance you have to believe.
#
# Unlike tests/fixtures/openmeteo/record.sh, this one opens no socket: it drives
# clima-daemon in fixture mode, so the whole chain from a recorded provider
# response to the JSON a widget receives runs offline and reproducibly.
#
#   ./record.sh                       use build/dev, write into this directory
#   ./record.sh /path/to/build /tmp   somewhere else, to diff against what is here
#
# Re-record when the wire format changes — libclima/wire/snapshot.cpp — and
# expect tests/tst_widgets.cpp to need re-reading afterwards, because its
# assertions name specific keys.

set -euo pipefail

build="${1:-build/dev}"
out="${2:-$(dirname "$0")}"
daemon="$build/daemon/clima-daemon"

if [ ! -x "$daemon" ]; then
    echo "no clima-daemon at $daemon — build it first:" >&2
    echo "  nix develop -c cmake --build $build --target clima-daemon" >&2
    exit 1
fi

mkdir -p "$out"

# A scratch XDG_DATA_HOME per fixture, so the daemon's SQLite cache starts cold
# every time. Without it the second run of this script serves whatever the first
# one left behind, and the recordings stop being a function of the fixtures.
for name in berlin kampala seattle toronto; do
    scratch="$(mktemp -d)"
    XDG_DATA_HOME="$scratch/data" XDG_CACHE_HOME="$scratch/cache" \
        "$daemon" --fixture "$name" --dump-snapshot > "$out/$name.json"
    rm -rf "$scratch"
    printf '%8d B  %s\n' "$(stat -c%s "$out/$name.json")" "$name.json"
done

echo
echo "Recorded into $out."
