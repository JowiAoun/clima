#!/usr/bin/env bash
# SPDX-FileCopyrightText: 2026 Jowi Aoun
# SPDX-License-Identifier: GPL-3.0-or-later
#
# Prove, on a real GNOME Shell, that a window belonging to a process we spawned
# can be adopted, re-typed as a dock and hidden from the window list.
#
#   scripts/shell-probe.sh flatpak     the Flatpak-installed app
#   scripts/shell-probe.sh host        build/dev/app/clima from this tree
#   scripts/shell-probe.sh -- CMD...   anything else
#
# This is the gate for the whole Ubuntu widget story. GNOME Shell cannot draw a
# QML surface, so a Clima widget is our own Qt process whose window the
# extension adopts — and every part of that rests on Meta.WaylandClient, whose
# notion of "our window" comes from a socket fd inherited at spawn time. See
# tests/shell/clima-window-probe@clima.invalid/extension.js for the mechanism
# and docs/widgets.md for what it measured.
#
# ---- why this is not in CI --------------------------------------------------
#
# It needs a GNOME Shell to nest inside, which a GitHub runner does not have.
# Making it work there would mean apt-installing gnome-shell and running it
# under a headless compositor, and the result would test that stack rather than
# the one users have. It is a manual acceptance test, run on a machine with a
# real session, and the answer is written into docs/widgets.md so that nobody
# has to re-run it to know what it said.
#
# ---- --nested, never --headless ---------------------------------------------
#
# A nested shell is an ordinary Wayland client of the session you are sitting
# in: it cannot take DRM master and it cannot open the libinput seat. gnome-
# shell --headless runs as a display server, and being wrong about whether that
# grabs the keyboard costs you the session you are testing from. The nested
# window appears for as long as the probe runs and then goes away.
set -euo pipefail

here="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
root="$(cd "$here/.." && pwd)"

app_id="io.github.JowiAoun.Clima"
uuid="clima-window-probe@clima.invalid"
mode="${CLIMA_PROBE_MODE:-dock}"

target="${1:-flatpak}"
shift || true

case "$target" in
  flatpak)
    if ! flatpak info "$app_id" >/dev/null 2>&1; then
      echo "shell-probe: $app_id is not installed. Run scripts/flatpak.sh build" >&2
      exit 1
    fi
    argv=(flatpak run --command=clima "$app_id")
    ;;
  host)
    binary="$root/build/dev/app/clima"
    if [ ! -x "$binary" ]; then
      echo "shell-probe: $binary does not exist. Build the dev preset first." >&2
      exit 1
    fi
    # The host binary needs the Qt from the flake on its library path, which is
    # what qt-env.sh works out. The Flatpak needs none of this — its Qt is the
    # runtime's — which is the one interesting difference between the two runs.
    argv=("$root/scripts/dev-run.sh")
    ;;
  --)
    argv=("$@")
    ;;
  *)
    echo "shell-probe: unknown target '$target' (flatpak | host | -- CMD...)" >&2
    exit 1
    ;;
esac

for tool in gnome-shell gjs /usr/bin/dbus-run-session; do
  command -v "$tool" >/dev/null 2>&1 || {
    echo "shell-probe: $tool is missing; this needs a machine with GNOME." >&2
    exit 1
  }
done

state="$(mktemp -d "${TMPDIR:-/tmp}/clima-shell-probe.XXXXXX")"
trap 'rm -rf "$state"' EXIT

# The nested shell gets its own everything. XDG_DATA_HOME is what makes it load
# our probe and not the user's extensions.
mkdir -p "$state/data/gnome-shell/extensions" "$state/config" "$state/cache" "$state/state"
cp -r "$root/tests/shell/$uuid" "$state/data/gnome-shell/extensions/"

export XDG_DATA_HOME="$state/data"
export XDG_CONFIG_HOME="$state/config"
export XDG_CACHE_HOME="$state/cache"
export XDG_STATE_HOME="$state/state"

# flatpak reads its user installation out of XDG_DATA_HOME too, and the line
# above just pointed that at an empty directory. Without this the probe reports
# that the app is not installed, on a machine where it is.
export FLATPAK_USER_DIR="${FLATPAK_USER_DIR:-$HOME/.local/share/flatpak}"

# dbus-run-session and dbus-daemon from a nix profile look for
# /etc/dbus-1/session.conf, which Debian and Ubuntu do not ship; the session
# bus then fails to start with a message about a missing file.
export PATH="/usr/bin:/bin:$PATH"

report="$state/report.txt"
: > "$report"
export CLIMA_PROBE_REPORT="$report"
export CLIMA_PROBE_MODE="$mode"

joined="$(printf '%s\x1f' "${argv[@]}")"
export CLIMA_PROBE_ARGV="${joined%$'\x1f'}"

echo "shell-probe: target  ${argv[*]}"
echo "shell-probe: mode    $mode"

# A private session bus. A second gnome-shell on the real one would contend for
# org.gnome.Shell and org.gnome.ScreenSaver with the shell the user is looking
# at. The cost is that there are no portals behind it, so a sandboxed Qt app
# spends ~25 s timing out on org.freedesktop.portal.Desktop before it paints —
# which is why the probe's deadline is 90 s and not 10.
timeout 180 /usr/bin/dbus-run-session -- bash -c '
    gsettings set org.gnome.shell enabled-extensions "[\"'"$uuid"'\"]"
    gsettings set org.gnome.shell disable-user-extensions false
    exec gnome-shell --nested --wayland
' > "$state/shell.log" 2>&1 || true

verdict="$(sed -n 's/^PROBE VERDICT //p' "$report" | tail -1)"

if [ -z "$verdict" ]; then
  echo "shell-probe: no verdict. The shell log follows." >&2
  sed -n 's/^/  | /p' "$report" >&2
  tail -40 "$state/shell.log" | sed -n 's/^/  # /p' >&2
  exit 1
fi

echo
sed -n 's/^PROBE /  /p' "$report"
echo
echo "shell-probe: verdict $verdict"

# The acceptance criteria, asserted rather than eyeballed. Window type 2 is
# Meta.WindowType.DOCK.
python3 - "$verdict" "$mode" <<'PY'
import json, sys

v = json.loads(sys.argv[1])
mode = sys.argv[2]

want = {
    "owns_window": True,
    "hide_from_window_list": True,
    "lower": True,
    "in_tab_list": False,
    "in_window_actors": True,
}
if mode == "dock":
    want["make_dock"] = True
    want["window_type"] = 2
elif mode == "desktop":
    want["make_desktop"] = True
    want["window_type"] = 1

bad = [(k, want[k], v.get(k, "<missing>")) for k in want if v.get(k, "<missing>") != want[k]]
for k, exp, got in bad:
    print(f"shell-probe: FAIL {k}: expected {exp!r}, got {got!r}", file=sys.stderr)
if bad:
    sys.exit(1)
print("shell-probe: ok — window adopted, re-typed, out of the window list, still composited")
PY
