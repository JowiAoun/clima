#!/usr/bin/env bash
# SPDX-FileCopyrightText: 2026 Jowi Aoun
# SPDX-License-Identifier: GPL-3.0-or-later
#
# Render the screenshots a software centre shows, into a directory to publish.
#
#   scripts/store-shots.sh [dir]   defaults to build/store-shots
#
# CLIMAT_BUILD_DIR selects the build to photograph; it defaults to build/dev.
# The build needs CLIMAT_DEV_TOOLS=ON, because --grab is a development tool and
# a shipped binary does not have it.
#
# ---- the third capture profile ----------------------------------------------
#
# docs/screenshots.md names three and this is the one that had no script: raw
# grabs of the application's own window, no device bezel, no composition.
# Flathub's linter reads a marketing composite with a phone frame around it as
# excessive whitespace and rejects the submission, so the showcase sheets in
# docs/images cannot be used here and neither can the Play screenshots, which
# are 540 by 960 to Play's rules.
#
# These are not compared byte for byte against anything. They are the only
# capture profile in this repository that is not, and the reason is that nothing
# would be learned: every pixel in them is already vouched for by a golden image
# of the same scene. What this script guarantees instead is that the set is
# COMPLETE - that every <image> the AppStream component promises is a file
# somebody produced.
#
# ---- why it reads the names out of the metainfo -----------------------------
#
# Because the alternative has already failed. The metainfo has declared four
# screenshot URLs since it was written, docs/releasing.md and docs/screenshots.md
# both said the release workflow published them, and no workflow ever did - so
# all four have been dead links from the first day, which is a Flathub rejection
# and four broken images in GNOME Software.
#
# The list of names lives in exactly one place, and this script fails if it is
# handed a name it has no arguments for, or if it knows a name the component
# does not ask for. Adding a screenshot means editing the metainfo and this
# table, and forgetting either one is a red job rather than a broken store page.
set -euo pipefail

here="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
root="$(cd "$here/.." && pwd)"

build_dir="${CLIMAT_BUILD_DIR:-$root/build/dev}"
binary="$build_dir/app/climat"
metainfo="$root/packaging/linux/climat.metainfo.xml.in"
out="${1:-$root/build/store-shots}"

if [ ! -x "$binary" ]; then
  echo "store-shots: no app at $binary - build it first, with CLIMAT_DEV_TOOLS=ON" >&2
  exit 1
fi

# Name, then the app's arguments. The scenes are the ones the captions in the
# metainfo describe, and each is a scene a golden image already covers: see
# `desktop`, `desktop-light`, `alert-desktop` and `mobile-today` in
# tests/golden/cases.
#
# mobile-daily is the Today tab and not the Monthly one, which is what
# docs/screenshots.md used to suggest. The monthly calendar is correct and it is
# a bad photograph: a fixture is one day of forecast inside a whole month, so
# twenty-nine of its thirty-one cells are empty and the picture reads as an app
# with no data in it. The Today tab carries the ten-day card the caption is
# about, with every day in it filled.
recipes=(
  "desktop-dark|--viewport desktop --scheme dark"
  "desktop-light|--viewport desktop --scheme light"
  "alerts|--viewport desktop --scheme dark --fixture seattle"
  "mobile-daily|--viewport mobile --scheme dark --tab today"
)

# What the component actually asks for, in the order it asks for it.
mapfile -t wanted < <(
  sed -n 's#.*<image>.*/screenshots/\([A-Za-z0-9_-]*\)\.png</image>.*#\1#p' "$metainfo"
)

if [ "${#wanted[@]}" -eq 0 ]; then
  echo "store-shots: no <image> entries under /screenshots/ in $metainfo" >&2
  exit 1
fi

known=""
for recipe in "${recipes[@]}"; do
  known="$known ${recipe%%|*}"
done

status=0
for name in "${wanted[@]}"; do
  case " $known " in
    *" $name "*) ;;
    *)
      echo "store-shots: the metainfo asks for $name.png and this script has no arguments for it" >&2
      status=1
      ;;
  esac
done

for recipe in "${recipes[@]}"; do
  name="${recipe%%|*}"
  case " ${wanted[*]} " in
    *" $name "*) ;;
    *)
      echo "store-shots: this script renders $name.png and the metainfo does not ask for it" >&2
      status=1
      ;;
  esac
done

[ "$status" -eq 0 ] || exit 1

# shellcheck source=scripts/capture-env.sh
. "$here/capture-env.sh"

scratch="$(mktemp -d)"
trap 'rm -rf "$scratch"' EXIT
export XDG_CONFIG_HOME="$scratch/config"
export XDG_DATA_HOME="$scratch/data"
export XDG_CACHE_HOME="$scratch/cache"
mkdir -p "$XDG_CONFIG_HOME/Climat"
printf '[time]\nformat=12h\n' > "$XDG_CONFIG_HOME/Climat/climat.ini"

mkdir -p "$out"
for recipe in "${recipes[@]}"; do
  name="${recipe%%|*}"
  args="${recipe#*|}"
  # shellcheck disable=SC2086  # the recipe is a word list on purpose
  "$binary" $args --grab "$out/$name.png" > /dev/null
  printf 'store-shots: %-16s %s bytes\n' "$name.png" "$(wc -c < "$out/$name.png")"
done

echo "store-shots: ${#recipes[@]} screenshots in $out"
