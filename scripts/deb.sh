#!/usr/bin/env bash
# SPDX-FileCopyrightText: 2026 Jowi Aoun
# SPDX-License-Identifier: GPL-3.0-or-later
#
# Build the .deb, in the distribution it is for.
#
#   scripts/deb.sh              build it into build/deb/
#   scripts/deb.sh inspect      build it and print the control file and contents
#
# CLIMA_DEB_IMAGE overrides the container image; it defaults to debian:trixie,
# which is what .github/workflows/ci.yml builds in.
#
# ---- why this is a container and not just `cpack` ---------------------------
#
# Because the dependency list is derived, not written. dpkg-shlibdeps reads the
# binary's DT_NEEDED entries and asks dpkg which package provides each one, so
# the .deb's Depends field is only correct if the Qt that was linked is the Qt
# that Debian ships. Build it in the Nix devshell instead and shlibdeps looks up
# a /nix/store path, finds no package owning it, and produces a .deb that
# depends on essentially nothing — which installs cleanly on a machine with no
# Qt and then does not start.
#
# That failure is invisible locally, because the machine that built it has Qt.
# It appears on someone else's. So the build happens in trixie or it does not
# happen.
set -euo pipefail

here="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
root="$(cd "$here/.." && pwd)"

image="${CLIMA_DEB_IMAGE:-debian:trixie}"
mode="${1:-build}"

if ! command -v docker > /dev/null 2>&1; then
  echo "deb: docker not found — it is how this reaches a Debian userland" >&2
  exit 1
fi

echo "deb: building in $image"

# Runs as root so apt works, then hands the output back. Without the chown the
# build directory becomes root-owned and the next ordinary `cmake --build`
# fails with a permission error that says nothing about containers.
docker run --rm \
  -v "$root:/src" \
  -w /src \
  -e DEBIAN_FRONTEND=noninteractive \
  "$image" \
  bash -euc '
    apt-get update -qq
    apt-get install --no-install-recommends -y -qq \
      build-essential cmake ninja-build pkg-config file \
      qt6-base-dev qt6-declarative-dev qt6-declarative-dev-tools \
      libqt6sql6-sqlite libgl1-mesa-dev > /dev/null

    # Tests and the gallery are off: a package build should build the product.
    # Dev tools too, which is what drops --grab and --film from a shipped
    # binary.
    cmake -S /src -B /src/build/deb -G Ninja \
      -DCMAKE_BUILD_TYPE=Release \
      -DCLIMA_BUILD_TESTS=OFF \
      -DCLIMA_BUILD_GALLERY=OFF \
      -DCLIMA_DEV_TOOLS=OFF

    cmake --build /src/build/deb
    cd /src/build/deb && cpack -G DEB

    chown -R '"$(id -u):$(id -g)"' /src/build/deb
  '

deb="$(find "$root/build/deb" -maxdepth 1 -name '*.deb' -print -quit)"

if [ -z "$deb" ]; then
  echo "deb: cpack produced no .deb" >&2
  exit 1
fi

echo
echo "deb: $deb"

if [ "$mode" = "inspect" ]; then
  echo
  echo "==== control ===================================================="
  dpkg-deb --info "$deb"
  echo
  echo "==== contents ==================================================="
  dpkg-deb --contents "$deb"
fi
