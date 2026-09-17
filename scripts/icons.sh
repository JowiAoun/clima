#!/usr/bin/env bash
# SPDX-FileCopyrightText: 2026 Jowi Aoun
# SPDX-License-Identifier: GPL-3.0-or-later
#
# The hicolor icon sizes, cut from one SVG.
#
#   scripts/icons.sh render     re-render packaging/icons/climat-<n>.png
#   scripts/icons.sh check      re-render to a temp dir and diff (CI gate)
#
# Why the PNGs are committed at all, when there is a generator right here:
#
# A packager building from a source tarball has librsvg or does not, and an
# icon that fails to appear is not a build failure - it is an app with a grey
# square in the launcher and a Flathub submission that gets bounced. So the
# rendered sizes are artefacts of the repository, and this script's real job is
# `check`: proving the committed bytes are still what the master renders to, so
# that editing the SVG and forgetting the PNGs is a red CI job rather than an
# icon that quietly disagrees with itself at four sizes out of eight.
#
# Run it under `nix develop`. The flake pins librsvg by store hash, which is
# what makes "the same bytes on every machine" true rather than hopeful - the
# same argument scripts/golden.sh makes at greater length.
set -euo pipefail

here="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
root="$(cd "$here/.." && pwd)"

icons_dir="$root/packaging/icons"
master="$icons_dir/climat.svg"

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
  echo "icons: rsvg-convert not found - run this inside \`nix develop\`" >&2
  exit 1
fi

# One size. Kept separate because `check` and `render` differ only in where the
# bytes land, and a second copy of the rsvg-convert invocation is a second
# chance to pass different flags to it.
render_one() {
  local size="$1" dest="$2"
  rsvg-convert --width "$size" --height "$size" --format png \
    --output "$dest/climat-$size.png" "$master"
}

# ---- Android ----------------------------------------------------------------
#
# Three images per density: the legacy square launcher icon, and the two
# layers of the adaptive icon that Android 8 and later compose and mask
# themselves. scripts/android-icon-layers.py cuts the layers out of the same
# master, so all of this is still climat.svg and nothing else.
#
# Densities and their scale: 48 dp for the launcher icon, 108 dp for a layer.
android_dir="$root/packaging/android/res"
android_densities=(mdpi:48:108 hdpi:72:162 xhdpi:96:216 xxhdpi:144:324 xxxhdpi:192:432)

render_android() {
  local dest="$1" layers
  layers="$(mktemp -d)"
  python3 "$here/android-icon-layers.py" "$master" "$layers"
  local entry density launcher layer dir
  for entry in "${android_densities[@]}"; do
    IFS=: read -r density launcher layer <<< "$entry"
    dir="$dest/mipmap-$density"
    mkdir -p "$dir"
    rsvg-convert --width "$launcher" --height "$launcher" --format png \
      --output "$dir/ic_launcher.png" "$master"
    rsvg-convert --width "$layer" --height "$layer" --format png \
      --output "$dir/ic_launcher_background.png" "$layers/background.svg"
    rsvg-convert --width "$layer" --height "$layer" --format png \
      --output "$dir/ic_launcher_foreground.png" "$layers/foreground.svg"
  done
  # And the Play Store feature graphic, which is the same drawing on a wide
  # sky. 1024 by 500 is the one size Play accepts.
  mkdir -p "$dest/../play"
  rsvg-convert --width 1024 --height 500 --format png \
    --output "$dest/../play/feature-graphic.png" "$layers/feature.svg"
  rm -rf "$layers"
}

android_files() {
  local entry density
  for entry in "${android_densities[@]}"; do
    IFS=: read -r density _ _ <<< "$entry"
    echo "mipmap-$density/ic_launcher.png"
    echo "mipmap-$density/ic_launcher_background.png"
    echo "mipmap-$density/ic_launcher_foreground.png"
  done
  echo "../play/feature-graphic.png"
}

case "$command" in
  render)
    for size in "${sizes[@]}"; do
      render_one "$size" "$icons_dir"
      echo "icons: climat-$size.png"
    done
    # And the Windows container, which is those same PNGs in one file. It is
    # built from the rendered sizes rather than from the SVG, so it cannot
    # describe a drawing the PNGs do not.
    python3 "$here/make-ico.py" "$icons_dir" "$icons_dir/climat.ico"
    render_android "$android_dir"
    echo "icons: packaging/android/res, 5 densities"
    ;;

  check)
    tmp="$(mktemp -d)"
    trap 'rm -rf "$tmp"' EXIT

    drift=0
    for size in "${sizes[@]}"; do
      render_one "$size" "$tmp"
      committed="$icons_dir/climat-$size.png"

      if [ ! -f "$committed" ]; then
        echo "icons: climat-$size.png is missing" >&2
        drift=1
      elif ! cmp -s "$tmp/climat-$size.png" "$committed"; then
        echo "icons: climat-$size.png does not match what climat.svg renders to" >&2
        drift=1
      fi
    done

    python3 "$here/make-ico.py" "$tmp" "$tmp/climat.ico" > /dev/null
    if ! cmp -s "$tmp/climat.ico" "$icons_dir/climat.ico"; then
      echo "icons: climat.ico does not match the rendered sizes" >&2
      drift=1
    fi

    render_android "$tmp/android"
    while read -r file; do
      if [ ! -f "$android_dir/$file" ]; then
        echo "icons: packaging/android/res/$file is missing" >&2
        drift=1
      elif ! cmp -s "$tmp/android/$file" "$android_dir/$file"; then
        echo "icons: packaging/android/res/$file does not match what climat.svg renders to" >&2
        drift=1
      fi
    done < <(android_files)
    if [ "$drift" -ne 0 ]; then
      echo "icons: run \`scripts/icons.sh render\` and commit the result" >&2
      exit 1
    fi
    echo "icons: ${#sizes[@]} sizes, the .ico and the Android set match the master"
    ;;

  *)
    echo "usage: icons.sh [render|check]" >&2
    exit 2
    ;;
esac
