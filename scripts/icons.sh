#!/usr/bin/env bash
# SPDX-FileCopyrightText: 2026 Jowi Aoun
# SPDX-License-Identifier: GPL-3.0-or-later
#
# The hicolor icon sizes, cut from one SVG.
#
#   scripts/icons.sh render     re-render packaging/icons/clima-<n>.png
#   scripts/icons.sh check      re-render to a temp dir and diff (CI gate)
#
# Why the PNGs are committed at all, when there is a generator right here:
#
# A packager building from a source tarball has librsvg or does not, and an
# icon that fails to appear is not a build failure — it is an app with a grey
# square in the launcher and a Flathub submission that gets bounced. So the
# rendered sizes are artefacts of the repository, and this script's real job is
# `check`: proving the committed bytes are still what the master renders to, so
# that editing the SVG and forgetting the PNGs is a red CI job rather than an
# icon that quietly disagrees with itself at four sizes out of eight.
#
# Run it under `nix develop`. The flake pins librsvg by store hash, which is
# what makes "the same bytes on every machine" true rather than hopeful — the
# same argument scripts/golden.sh makes at greater length.
set -euo pipefail

here="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
root="$(cd "$here/.." && pwd)"

icons_dir="$root/packaging/icons"
master="$icons_dir/clima.svg"

# The hicolor sizes that matter, and why this list stops where it does.
#
#   16 24 32   menus, window title bars, the tray
#   48 64      the classic launcher grid
#   128 256    GNOME Software and Discover list at 128, detail at 256
#   512        Flathub's store banner, and the source for anything larger
#
# 22 and 96 exist in the spec and no shipping shell asks for them any more.
sizes=(16 24 32 48 64 128 256 512)

command="${1:-render}"

if ! command -v rsvg-convert > /dev/null 2>&1; then
  echo "icons: rsvg-convert not found — run this inside \`nix develop\`" >&2
  exit 1
fi

# One size. Kept separate because `check` and `render` differ only in where the
# bytes land, and a second copy of the rsvg-convert invocation is a second
# chance to pass different flags to it.
render_one() {
  local size="$1" dest="$2"
  rsvg-convert --width "$size" --height "$size" --format png \
    --output "$dest/clima-$size.png" "$master"
}

case "$command" in
  render)
    for size in "${sizes[@]}"; do
      render_one "$size" "$icons_dir"
      echo "icons: clima-$size.png"
    done
    # And the Windows container, which is those same PNGs in one file. It is
    # built from the rendered sizes rather than from the SVG, so it cannot
    # describe a drawing the PNGs do not.
    python3 "$here/make-ico.py" "$icons_dir" "$icons_dir/clima.ico"
    ;;

  check)
    tmp="$(mktemp -d)"
    trap 'rm -rf "$tmp"' EXIT

    drift=0
    for size in "${sizes[@]}"; do
      render_one "$size" "$tmp"
      committed="$icons_dir/clima-$size.png"

      if [ ! -f "$committed" ]; then
        echo "icons: clima-$size.png is missing" >&2
        drift=1
      elif ! cmp -s "$tmp/clima-$size.png" "$committed"; then
        echo "icons: clima-$size.png does not match what clima.svg renders to" >&2
        drift=1
      fi
    done

    python3 "$here/make-ico.py" "$tmp" "$tmp/clima.ico" > /dev/null
    if ! cmp -s "$tmp/clima.ico" "$icons_dir/clima.ico"; then
      echo "icons: clima.ico does not match the rendered sizes" >&2
      drift=1
    fi

    if [ "$drift" -ne 0 ]; then
      echo "icons: run \`scripts/icons.sh render\` and commit the result" >&2
      exit 1
    fi
    echo "icons: ${#sizes[@]} sizes and the .ico match the master"
    ;;

  *)
    echo "usage: icons.sh [render|check]" >&2
    exit 2
    ;;
esac
