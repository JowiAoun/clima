#!/usr/bin/env bash
# SPDX-FileCopyrightText: 2026 Jowi Aoun
# SPDX-License-Identifier: GPL-3.0-or-later
#
# Render the composed device images the README and the docs use.
#
#   scripts/shots.sh              write docs/images/*.png
#   scripts/shots.sh check        re-render to a temp dir and diff (CI gate)
#
# CLIMA_BUILD_DIR selects the build to photograph; it defaults to build/dev.
#
# ---- the two capture profiles, and why they are different -------------------
#
# This is the SHOWCASE profile: device bezels, the app's own page gradient
# behind them, several form factors in one frame. It exists to be looked at.
#
# It is deliberately not the profile AppStream screenshots use. Flathub's
# linter reads a marketing composite with a phone frame around it as excessive
# whitespace and rejects it, so store screenshots are raw un-bezelled grabs
# straight out of `clima --grab`. Two profiles, two purposes, and neither is a
# worse version of the other.
#
# ---- why these are still deterministic --------------------------------------
#
# Same reason the golden images are: a fixture at a frozen clock, Theme
# stillness collapsing every animation duration to zero, and a flake-pinned Qt.
# So `check` is a real gate rather than a formality — it can tell the difference
# between "the README images are stale" and "somebody edited a PNG".
set -euo pipefail

here="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
root="$(cd "$here/.." && pwd)"

build_dir="${CLIMA_BUILD_DIR:-$root/build/dev}"
binary="$build_dir/gallery/clima-gallery"
widget_binary="$build_dir/widgets/clima-widget"
images_dir="$root/docs/images"
sheets_js="$root/gallery/qml/Clima/Gallery/shots.js"

# The shot ids, read out of the catalogue rather than listed here. A third copy
# of this list would be the one that goes stale — the C++ already keeps a second
# for --help, and tests/qml/tst_shots.qml is what holds those two together.
mapfile -t shots < <(sed -n 's/^ *id: "\([a-z-]*\)",$/\1/p' "$sheets_js")

if [ "${#shots[@]}" -eq 0 ]; then
  echo "shots: found no sheets in $sheets_js" >&2
  exit 1
fi

if [ ! -x "$binary" ]; then
  echo "shots: no gallery at $binary — build it first" >&2
  exit 1
fi

if [ ! -x "$widget_binary" ]; then
  echo "shots: no widget host at $widget_binary — build it first" >&2
  exit 1
fi

# The same pinned environment scripts/golden.sh captures under, and for the same
# reason: fontconfig decides where glyphs land, and the host's would put them
# somewhere else. See that script's header, which explains it at length.
export QT_QPA_PLATFORM=offscreen
export QT_SCALE_FACTOR=1
export QT_FONT_DPI=96
export TZ=UTC
export FONTCONFIG_FILE="$root/tests/golden/fontconfig.conf"
export LC_ALL=C.UTF-8

# And the developer's own settings kept out of it, which scripts/golden.sh has
# always done and this script did not. Units are a QSettings value: somebody who
# chose Fahrenheit rendered a README with Fahrenheit in it, and `check` then
# failed on their machine for a reason nothing in the diff would show.
scratch="$(mktemp -d)"
trap 'rm -rf "$scratch"' EXIT
export XDG_CONFIG_HOME="$scratch/config"
export XDG_DATA_HOME="$scratch/data"
export XDG_CACHE_HOME="$scratch/cache"

# ---- the desktop tiles ------------------------------------------------------
#
# Not a gallery sheet, because the tiles are a different binary with a different
# QML module — see widgets/CMakeLists.txt for why they cannot be imported into
# one. Rendered here anyway so that the README's widget image is gated by the
# same `check` as every other one.
#
# `--now` is what makes it deterministic. Two things on a tile move without new
# data arriving — the sun mark and the age footer — and both are correct
# behaviour that would otherwise produce a different PNG every run.
# widgets/widgetclock.h has the argument. The instant is inside the Seattle
# fixture's own afternoon, so the sun is up and the arc has somewhere to put its
# mark.
widget_now="2026-07-31T15:20:00-07:00"
widget_snapshot="$root/tests/fixtures/wire/seattle.json"
widget_args=(
  --snapshot "$widget_snapshot"
  --now "$widget_now"
  --scheme dark
  --columns 2
  --widget current-conditions
  --widget wind-rose
  --widget hourly-strip
  --widget uv-dial
  --widget sun-arc
  --widget alerts
)

render_all() {
  local dest="$1"
  for shot in "${shots[@]}"; do
    "$binary" --shot "$shot" --grab "$dest/$shot.png" > /dev/null
    printf 'shots: %-18s %s bytes\n' "$shot.png" "$(wc -c < "$dest/$shot.png")"
  done

  "$widget_binary" "${widget_args[@]}" --grab "$dest/widgets.png" > /dev/null
  printf 'shots: %-18s %s bytes\n' "widgets.png" "$(wc -c < "$dest/widgets.png")"
}

case "${1:-render}" in
  render)
    mkdir -p "$images_dir"
    render_all "$images_dir"
    ;;

  check)
    tmp="$(mktemp -d)"
    trap 'rm -rf "$tmp"' EXIT
    render_all "$tmp" > /dev/null

    drift=0
    for shot in "${shots[@]}" widgets; do
      if [ ! -f "$images_dir/$shot.png" ]; then
        echo "shots: docs/images/$shot.png is missing" >&2
        drift=1
      elif ! cmp -s "$tmp/$shot.png" "$images_dir/$shot.png"; then
        echo "shots: docs/images/$shot.png is not what the app renders today" >&2
        drift=1
      fi
    done

    if [ "$drift" -ne 0 ]; then
      echo "shots: run \`scripts/shots.sh\` and commit the result" >&2
      exit 1
    fi
    echo "shots: $(( ${#shots[@]} + 1 )) images match the app"
    ;;

  *)
    echo "usage: shots.sh [render|check]" >&2
    exit 2
    ;;
esac
