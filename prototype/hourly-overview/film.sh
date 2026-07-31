#!/usr/bin/env bash
# SPDX-FileCopyrightText: 2026 Jowi Aoun
# SPDX-License-Identifier: GPL-3.0-or-later
#
# Films a transition and tiles the frames into one contact sheet.
#
# A still frame cannot show motion, and `--grab` lands wherever the animation
# happened to be — usually after it finished. So an animation that is wrong, or
# missing altogether, grabs identically to one that is right. This is the tool
# for looking at motion; `--grab` is the tool for looking at layout.
#
#   ./film.sh out.png -- --size 1000x700 --scroll 420 --poke feels=true
#   ./film.sh out.png --frames 12 --every 45 -- --gallery UV --poke remount=1
#   ./film.sh out.png -- --poke metric=uv
#
# Frames read left to right, top to bottom. Frame 00 is the state *before* the
# poke; every frame after it is the transition. A contact sheet whose frames all
# look identical means either nothing is animating or the whole thing finished
# inside one interval — turn `--every` down and look again.
set -euo pipefail

here="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

if [[ $# -lt 1 || "$1" == "-h" || "$1" == "--help" ]]; then
    sed -n '3,20p' "${BASH_SOURCE[0]}" | sed 's/^# \?//'
    exit 1
fi

out="$1"; shift
frames=8
every=60
width=440

while [[ $# -gt 0 && "$1" != "--" ]]; do
    case "$1" in
        --frames) frames="$2"; shift 2 ;;
        --every)  every="$2";  shift 2 ;;
        --width)  width="$2";  shift 2 ;;
        *)
            echo "film.sh: unknown option '$1' — run.sh options go after a '--' separator" >&2
            exit 1 ;;
    esac
done
[[ "${1:-}" == "--" ]] && shift

tmp="$(mktemp -d)"
trap 'rm -rf "$tmp"' EXIT

"$here/run.sh" --film "$tmp/f" --frames "$frames" --every "$every" "$@"

count=$(ls "$tmp"/f-*.png 2>/dev/null | wc -l)
if [[ "$count" -eq 0 ]]; then
    echo "film.sh: no frames were written — did the app fail to start?" >&2
    exit 1
fi
if [[ "$count" -ne "$frames" ]]; then
    echo "film.sh: warning — asked for $frames frames, got $count" >&2
fi

cols=$(( count < 4 ? count : 4 ))
rows=$(( (count + cols - 1) / cols ))

ffmpeg -v error -y -framerate 1 -i "$tmp/f-%02d.png" \
    -vf "scale=${width}:-1,tile=${cols}x${rows}:padding=6:margin=6:color=0x2a2f57" \
    -frames:v 1 "$out"

echo "film: $count frames, ${cols}x${rows} -> $out"
