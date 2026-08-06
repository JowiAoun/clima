#!/usr/bin/env bash
# SPDX-FileCopyrightText: 2026 Jowi Aoun
# SPDX-License-Identifier: GPL-3.0-or-later
#
# Build, install and run the Flatpak from the working tree.
#
#   scripts/flatpak.sh build      build and install it into the user's flatpak
#   scripts/flatpak.sh run [...]  run it, passing anything else through
#   scripts/flatpak.sh deps       install the runtime and SDK it needs
#   scripts/flatpak.sh clean      uninstall it and delete the build state
#
# The manifest builds a `dir` source pointing at this tree, so what you get is
# the branch you are on and not a tag. That is the point: it is the only way to
# find out before pushing that a change works inside the sandbox, where the
# cache is in ~/.var/app, there is no host font, and the only route to the
# desktop's colour scheme is the portal.
#
# ---- this is NOT run under `nix develop` ------------------------------------
#
# flatpak-builder drives the host's flatpak installation — its repo, its
# bubblewrap, its runtimes — so it has to be the host's flatpak-builder or one
# that agrees with it. The Qt in the flake is irrelevant here; the build inside
# the sandbox uses the SDK's Qt, which is the entire reason a Flatpak solves
# Ubuntu 24.04. Run this from a normal shell.
set -euo pipefail

here="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
root="$(cd "$here/.." && pwd)"

manifest="$root/packaging/flatpak/io.github.JowiAoun.Clima.yml"
app_id="io.github.JowiAoun.Clima"
state_dir="$root/build/flatpak"

# Read the runtime version out of the manifest rather than repeating it, so
# `deps` cannot install a runtime the manifest does not ask for. That mistake
# produces a build failure whose message is about a missing SDK, which sends
# you to install the version you already have.
runtime_version="$(sed -n "s/^runtime-version: *'\{0,1\}\([^']*\)'\{0,1\}.*/\1/p" "$manifest")"

if [ -z "$runtime_version" ]; then
  echo "flatpak: could not read runtime-version out of $manifest" >&2
  exit 1
fi

command="${1:-build}"
shift || true

case "$command" in
  deps)
    # --user, because a system install needs polkit and this is a development
    # dependency rather than something the machine should carry.
    flatpak remote-add --user --if-not-exists \
      flathub https://dl.flathub.org/repo/flathub.flatpakrepo
    flatpak install --user --noninteractive --or-update flathub \
      "org.kde.Platform//$runtime_version" "org.kde.Sdk//$runtime_version"
    ;;

  build)
    if ! command -v flatpak-builder > /dev/null 2>&1; then
      echo "flatpak: flatpak-builder not found." >&2
      echo "  Debian/Ubuntu: apt install flatpak-builder" >&2
      echo "  Fedora:        dnf install flatpak-builder" >&2
      echo >&2
      echo "  Or, without installing anything:" >&2
      echo "    nix shell nixpkgs#flatpak-builder nixpkgs#appstream -c scripts/flatpak.sh build" >&2
      echo >&2
      echo "  The second package is not optional. flatpak-builder finishes by" >&2
      echo "  running \`appstreamcli compose\`, which lives in a separate binary" >&2
      echo "  at libexec/appstreamcli-compose that many distributions do not" >&2
      echo "  ship. Without it the whole build fails at the last step, after" >&2
      echo "  compiling everything, with a message about an addon." >&2
      exit 1
    fi

    # --force-clean because an incremental flatpak-builder build reuses the
    # previous export, and a file that stopped being installed stays in the
    # result. That turns "I removed the gallery from the manifest" into a
    # Flatpak that still contains it.
    flatpak-builder --user --install --force-clean \
      --state-dir "$state_dir/state" \
      "$state_dir/build" "$manifest"

    echo
    echo "flatpak: installed $app_id — run it with"
    echo "  scripts/flatpak.sh run"
    ;;

  run)
    exec flatpak run "$app_id" "$@"
    ;;

  clean)
    flatpak uninstall --user --noninteractive "$app_id" || true
    rm -rf "$state_dir"
    echo "flatpak: uninstalled and cleaned"
    ;;

  *)
    echo "usage: flatpak.sh [deps|build|run|clean]" >&2
    exit 2
    ;;
esac
