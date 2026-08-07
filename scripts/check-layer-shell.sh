#!/usr/bin/env bash
# SPDX-FileCopyrightText: 2026 Jowi Aoun
# SPDX-License-Identifier: GPL-3.0-or-later
#
# Proves that clima-widget pins itself to the desktop, against a real compositor.
#
#   scripts/check-layer-shell.sh [path-to-clima-widget]
#
# ============================================================================
# WHY THIS SCRIPT EXISTS AT ALL
#
# packaging/plasma/README.md used to say the layer-shell path was the right
# answer for KDE and was not built, and gave one reason: on the machine this
# project is developed on, mutter does not implement `wlr-layer-shell`, so the
# code could have been compiled and never once run. docs/widgets.md exists
# because the GNOME adoption mechanism was measured before anything was built on
# it, and shipping an unmeasured second mechanism would have been the opposite.
#
# This is the measurement. `WLR_BACKENDS=headless sway` stands up a wlroots
# compositor with a virtual output, no GPU and no seat — and wlroots is the
# reference implementation of the protocol KWin also speaks, so a layer surface
# sway accepts is one Plasma, Hyprland, Wayfire, river and labwc accept too.
#
# ============================================================================
# THE FIVE ASSERTIONS
#
#   1. PINNED       sway logs a layer surface with our namespace, on the layer
#                   we asked for. The namespace and the layer are arguments to
#                   `get_layer_surface`, so they are in the log line; the anchor
#                   and margins are sent afterwards and are NOT, which is why 2
#                   exists rather than a longer grep.
#
#   2. PLACED       the tiles are in the corner they were anchored to and the
#                   opposite corner is empty. Read off a photograph of the
#                   output, because that is the only place the anchor and the
#                   margin can actually be observed.
#
#   3. NOT A WINDOW `swaymsg -t get_tree` does not know about it. A layer
#                   surface is not in the window tree — no alt-tab, no tiling,
#                   nothing moves when it appears.
#
#   4. FALSIFIABLE  the same binary with `--pin off` IS in that tree and logs no
#                   layer surface. Every check in this repository is verified by
#                   injecting the defect it exists for; this one carries its own
#                   injection, because a check that passes when the feature is
#                   switched off is not checking the feature.
#
#   5. REFUSES      `--pin on` where a layer surface is impossible exits non-zero
#                   and says why, rather than putting a floating window on
#                   somebody's desktop and calling it done.
#
# The monitor-hotplug case — unplug the output a pinned surface lives on and it
# comes back on another one — is assertion 6, and it is the reason
# widgets/layershell.cpp has a Remap in it.

set -euo pipefail

here="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
repo="$(cd "$here/.." && pwd)"

widget="${1:-$repo/build/dev/widgets/clima-widget}"
fixture="$repo/tests/fixtures/wire/seattle.json"

fail() { printf 'check-layer-shell: %s\n' "$1" >&2; exit 1; }
skip() { printf 'check-layer-shell: %s SKIPPED.\n' "$1" >&2; exit 0; }

[ -x "$widget" ] || fail "no clima-widget at $widget — build it first, or pass a path"
[ -r "$fixture" ] || fail "no fixture at $fixture"

command -v sway >/dev/null || skip "no sway, so there is no compositor to test against —"
command -v swaymsg >/dev/null || skip "no swaymsg —"
command -v grim >/dev/null || skip "no grim, so the placement cannot be photographed —"
command -v ffmpeg >/dev/null || skip "no ffmpeg, so the photograph cannot be measured —"

# ---- a short runtime directory ----------------------------------------------
#
# Not the scratch directory somebody handed us, and not $TMPDIR. A Wayland
# socket is a AF_UNIX path and `sun_path` is 108 bytes; a session directory a
# few levels deep overruns it and sway fails with "Unable to open wayland
# socket", which reads like a permissions problem and is a length problem.
run="${CLIMA_LAYER_SHELL_RUNTIME_DIR:-/tmp/clima-ls-$$}"
rm -rf "$run"
mkdir -p "$run"
chmod 700 "$run"
trap 'rm -rf "$run"' EXIT

# ---- 5, first, because it needs no compositor -------------------------------
#
# The offscreen platform plugin cannot produce a Wayland surface of any kind, so
# this is the same refusal a GNOME user would get, reached without a GNOME.
echo "== refusal =="
set +e
QT_QPA_PLATFORM=offscreen "$widget" --pin on --snapshot "$fixture" \
    --widget current-conditions > "$run/refuse.log" 2>&1
refused=$?
set -e
[ "$refused" -eq 3 ] || fail "--pin on under the offscreen platform exited $refused, wanted 3"
grep -q "not \"wayland\"" "$run/refuse.log" \
    || fail "--pin on refused without saying the platform was the reason"
echo "refuses when it cannot pin: ok"

# ---- the compositor ----------------------------------------------------------
#
# One sway run per case rather than one for all of them, because each case wants
# its own log and its own photograph and a shared compositor would make the
# assertions depend on the order they run in.
#
# `sway -d` is what puts the layer-surface line in the log at all; without it the
# only evidence is the picture.
#
# WLR_RENDERER=pixman keeps this off the GPU. wlroots would otherwise want a DRM
# render node for its GLES2 renderer, and a CI runner has none — so the software
# renderer is what makes this the same check everywhere rather than one that only
# a workstation can run. Overridable, because on a machine with a GPU the GL path
# is the one users are actually on.
run_sway() {
    local name="$1" delay="$2"
    shift 2
    {
        echo "output HEADLESS-1 resolution 1200x800"
        for line in "$@"; do echo "$line"; done
        echo "exec \"sleep $delay; swaymsg -t get_tree > $run/$name.tree.json; grim $run/$name.png; swaymsg exit\""
    } > "$run/$name.conf"

    XDG_RUNTIME_DIR="$run" \
    QT_QPA_PLATFORM=wayland \
    WLR_BACKENDS=headless \
    WLR_LIBINPUT_NO_DEVICES=1 \
    WLR_RENDERER="${WLR_RENDERER:-pixman}" \
    LIBGL_ALWAYS_SOFTWARE="${LIBGL_ALWAYS_SOFTWARE:-1}" \
        timeout 120 sway -d -c "$run/$name.conf" > "$run/$name.log" 2>&1 \
        || fail "sway exited non-zero for case '$name'"
}

# ---- can this machine run a compositor at all? -------------------------------
#
# Asked separately, and before anything is asserted, because the two answers are
# different kinds of thing. A machine with no way to stand up wlroots — no
# renderer it can use, no seat, a $XDG_RUNTIME_DIR it cannot bind in — has told
# us nothing about clima-widget, and reporting that as a failure would put a red
# cross on a change that is fine. Every failure after this line is about the
# code.
{
    echo "output HEADLESS-1 resolution 640x480"
    echo 'exec "swaymsg exit"'
} > "$run/probe.conf"
if ! XDG_RUNTIME_DIR="$run" WLR_BACKENDS=headless WLR_LIBINPUT_NO_DEVICES=1 \
     WLR_RENDERER="${WLR_RENDERER:-pixman}" \
     timeout 60 sway -c "$run/probe.conf" > "$run/probe.log" 2>&1; then
    sed 's/^/  /' "$run/probe.log" >&2
    skip "sway could not start here, so there is no compositor to measure against —"
fi
echo "compositor: sway starts headless here"

# The tiles, frozen at a known instant so the picture is the same every run, and
# fed a recorded snapshot so nothing opens a socket.
tiles="$widget --snapshot $fixture --widget current-conditions --widget uv-dial \
--now 2026-07-31T15:20:00-07:00 --columns 1"

# Mean luminance of a crop of the photograph, as an integer. This is how the
# anchor and the margin get asserted: they are sent to the compositor after the
# surface exists, so they are not in any log line — the only place they can be
# observed is where the pixels ended up.
#
# `file=-` rather than reading the log, because `metadata=print` writes at info
# level and -v error swallows it — which produces an empty measurement and an
# assertion that cannot fail.
#
# The scale is broadcast YUV, so black is 16 and white is 235, not 0 and 255.
# An empty corner of this compositor measures exactly 16.
luma() {
    local image="$1" geometry="$2"
    ffmpeg -v error -i "$image" \
        -vf "crop=$geometry,signalstats,metadata=print:key=lavfi.signalstats.YAVG:file=-" \
        -f null - 2>/dev/null \
        | sed -nE 's/.*YAVG=([0-9]+).*/\1/p' | head -n1
}

# ---- 1, 2, 3: pinned --------------------------------------------------------

echo
echo "== pinned =="
run_sway pinned 6 "exec \"$tiles --pin on --anchor top-right --margin 24 > $run/pinned.widget.log 2>&1\""

surface="$(grep -o 'new layer surface: namespace [^ ]* layer [0-9]*' "$run/pinned.log" | head -n1 || true)"
[ -n "$surface" ] || fail "no layer surface was created — sway logged none"
echo "$surface"

grep -q 'namespace clima-widgets' <<<"$surface" \
    || fail "the layer surface is not ours: $surface"
# 0 background, 1 bottom, 2 top, 3 overlay. Bottom is what a desktop widget is:
# above the wallpaper, below every window.
grep -q 'layer 1$' <<<"$surface" \
    || fail "the layer surface is not on the bottom layer: $surface"
echo "namespace and layer: ok"

# The tile block is 240 px wide against a 1200 px output, 24 px in from the top
# and the right. So: bright in the top-right, dark in its mirror image on the
# left. Two crops rather than one, because "there are pixels somewhere" would
# pass with the anchor ignored and the surface centred.
right="$(luma "$run/pinned.png" "260:200:920:20")"
left="$(luma "$run/pinned.png" "260:200:20:20")"
[ -n "$right" ] && [ -n "$left" ] || fail "ffmpeg measured nothing from the photograph"
echo "top-right luminance $right, top-left luminance $left"
[ "$right" -gt 30 ] || fail "nothing was drawn in the corner the tiles were anchored to"
[ "$left" -lt 20 ] || fail "something was drawn in the corner the tiles were anchored away from"
echo "anchor and margin: ok"

grep -q '"app_id": *"' "$run/pinned.tree.json" \
    && fail "a pinned surface turned up in the window tree; it should not be a window at all"
echo "absent from the window tree: ok"

# ---- 4: the same binary, not pinned -----------------------------------------
#
# The injection. If this case also produced a layer surface, or also stayed out
# of the window tree, then everything above would pass with `--pin` doing
# nothing at all.

echo
echo "== not pinned (the control) =="
run_sway plain 6 "exec \"$tiles --pin off > $run/plain.widget.log 2>&1\""

grep -q 'new layer surface' "$run/plain.log" \
    && fail "--pin off still created a layer surface"
echo "no layer surface: ok"

grep -q '"app_id": *"[^"]' "$run/plain.tree.json" \
    || fail "--pin off produced no ordinary window either — the widget did not start"
echo "an ordinary window in the tree: ok"

# ---- 6: the monitor goes away -----------------------------------------------
#
# A layer surface belongs to an output. Unplug that output and the compositor
# dismisses the surface, layer-shell-qt closes the window, and a host whose only
# window that is would exit — a two-screen desktop would lose its tiles the
# first time somebody undocked. widgets/layershell.cpp puts them back; this is
# what says so.
#
# sway will not unplug the last output, hence the second one.

echo
echo "== monitor unplugged =="
run_sway hotplug 12 \
    "exec \"$tiles --pin on > $run/hotplug.widget.log 2>&1\"" \
    "exec \"sleep 5; swaymsg create_output; sleep 2; swaymsg output HEADLESS-1 unplug\""

surfaces="$(grep -c 'namespace clima-widgets' "$run/hotplug.log" || true)"
echo "layer surfaces created across the unplug: $surfaces"
[ "$surfaces" -ge 2 ] \
    || fail "the tiles did not come back after their output went away"
grep -q 'giving up' "$run/hotplug.widget.log" \
    && fail "the host gave up rather than remapping"
echo "survives its output being unplugged: ok"

echo
echo "check-layer-shell: ok"
