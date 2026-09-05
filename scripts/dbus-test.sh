#!/usr/bin/env bash
# SPDX-FileCopyrightText: 2026 Jowi Aoun
# SPDX-License-Identifier: GPL-3.0-or-later
#
# Runs a test on a session bus of its very own.
#
#   scripts/dbus-test.sh build/dev/tests/tst_portallocator [args…]
#
# Two tests need this — tst_portallocator and tst_notifier — because both OWN
# a well-known name that belongs to the desktop: org.freedesktop.portal.Desktop
# and org.freedesktop.Notifications. A test that took either of those on the
# developer's real bus would be taking it from the desktop, and a test that
# failed to take it would be reporting on the runner rather than on the code.
#
# Three things have to be arranged, and each of them was a failure first.
#
# ---- 1. the bus must not be able to start the real thing --------------------
#
# `dbus-run-session` starts a private bus, but a private bus still reads
# service files out of XDG_DATA_HOME and XDG_DATA_DIRS. On any desktop machine
# that means /usr/share/dbus-1/services, which contains
# org.freedesktop.portal.Desktop.service — so the moment the test asked for
# that name, the bus activated the host's actual xdg-desktop-portal to provide
# it, and the test found itself talking to the real portal on a bus it thought
# was empty. Both variables are pointed at a scratch directory with nothing in
# it, which leaves only the daemon's own service directory in the Nix store.
#
# ---- 2. LD_LIBRARY_PATH must not shadow the tool's own libraries ------------
#
# `dbus-run-session` is linked against the libdbus that shipped with it, by
# RUNPATH — and LD_LIBRARY_PATH beats RUNPATH. A desktop session that exports
# one (this developer's does, by way of an unrelated application) hands the
# 1.16 tool a 1.14 library and it dies with `version LIBDBUS_PRIVATE_1.16.2
# not found`. Nix binaries carry their dependencies in RUNPATH and need no
# LD_LIBRARY_PATH, so it is cleared rather than repaired.
#
# ---- 3. the daemon needs a config file it can find --------------------------
#
# A Nix-built dbus looks for /etc/dbus-1/session.conf, which exists on a NixOS
# machine and not on this one. The config that belongs to the daemon we are
# actually running is beside it in the store, so it is found from the binary's
# own path rather than assumed.

set -euo pipefail

if [ $# -lt 1 ]; then
  echo "usage: $0 <program> [args…]" >&2
  exit 2
fi

if ! command -v dbus-run-session > /dev/null 2>&1; then
  echo "$0: dbus-run-session is not on PATH" >&2
  exit 2
fi

# The daemon's own prefix, and therefore its own session.conf. `readlink -f`
# because PATH holds a symlink farm and the config is beside the real file.
tool="$(command -v dbus-run-session)"
prefix="$(dirname "$(dirname "$(readlink -f "$tool")")")"
config="$prefix/share/dbus-1/session.conf"

if [ ! -r "$config" ]; then
  echo "$0: no session.conf at $config" >&2
  exit 2
fi

scratch="$(mktemp -d)"
trap 'rm -rf "$scratch"' EXIT

# Empty, and existing: a directory the bus can scan and find nothing in.
mkdir -p "$scratch/data/dbus-1/services" "$scratch/config" "$scratch/cache" "$scratch/runtime"
chmod 700 "$scratch/runtime"

export XDG_DATA_HOME="$scratch/data"
export XDG_DATA_DIRS="$scratch/data"
export XDG_CONFIG_HOME="$scratch/config"
export XDG_CACHE_HOME="$scratch/cache"
export XDG_RUNTIME_DIR="$scratch/runtime"

# See 2 above.
unset LD_LIBRARY_PATH

exec dbus-run-session --config-file="$config" -- "$@"
