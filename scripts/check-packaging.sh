#!/usr/bin/env bash
# SPDX-FileCopyrightText: 2026 Jowi Aoun
# SPDX-License-Identifier: GPL-3.0-or-later
#
# Validates the Linux packaging metadata — against a real staged install, not
# against the templates.
#
#   scripts/check-packaging.sh          exit 1 on anything a store would reject
#
# CLIMA_BUILD_DIR selects the build to install from; it defaults to build/dev.
# The build has to exist and be built, because `cmake --install` needs the
# binary it was told to install.
#
# ---- why it stages an install rather than linting the sources ---------------
#
# Because half of what can go wrong here is in the install rules, not in the
# files. The desktop entry can be perfect and land in the wrong directory; the
# icons can render and be installed under the source basename instead of the
# app ID; the metainfo can validate and never be installed at all. Every one of
# those produces exactly the same symptom — a working app with a generic icon
# that no software centre lists — and none of them is visible in a template.
#
# So: install to a temporary prefix, then check what actually arrived, at the
# paths a packager will actually ship. That is also the shape of the thing
# Flathub and GNOME Software run on submission.
set -euo pipefail

here="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
root="$(cd "$here/.." && pwd)"

build_dir="${CLIMA_BUILD_DIR:-$root/build/dev}"
app_id="io.github.JowiAoun.Clima"

for tool in desktop-file-validate appstreamcli; do
  if ! command -v "$tool" > /dev/null 2>&1; then
    echo "packaging: $tool not found — run this inside \`nix develop\`" >&2
    exit 1
  fi
done

if [ ! -f "$build_dir/CMakeCache.txt" ]; then
  echo "packaging: no build at $build_dir — configure and build it first" >&2
  exit 1
fi

# The app ID is a build variable, so read it back rather than assuming the
# default. A fork that renamed the app should still be able to run this.
cached_id="$(sed -n 's/^CLIMA_APP_ID:STRING=//p' "$build_dir/CMakeCache.txt" || true)"
[ -n "$cached_id" ] && app_id="$cached_id"

root="$(mktemp -d)"
trap 'rm -rf "$root"' EXIT

echo "packaging: staging an install of $app_id"

# DESTDIR and not only --prefix, because one install rule in this project has an
# absolute destination and has to: the daemon's XDG autostart entry goes to
# /etc/xdg/autostart, since the autostart search path is a fixed list and
# nothing reads /usr/local/etc/xdg/autostart. `--prefix` does not redirect an
# absolute DESTINATION — it is not for staging — so this script tried to write
# into the real /etc and stopped with "Permission denied", which is a check
# that fails on the machine rather than on the tree.
#
# DESTDIR is the mechanism that exists for this and every packager already uses
# it. It also means the autostart entry is now *covered* here, rather than
# switched off to make the check pass.
DESTDIR="$root" cmake --install "$build_dir" --prefix /usr > /dev/null
stage="$root/usr"

fail=0
note() {
  echo "packaging: $1" >&2
  fail=1
}

# ---- the daemon's autostart entry -------------------------------------------
#
# Outside $stage on purpose: it is the one thing here that does not live under
# the prefix. Checked only when the daemon was built, which is the same gate
# packaging/CMakeLists.txt puts on installing it — a build without Qt D-Bus has
# no daemon and correctly installs no entry for one.
if [ -x "$build_dir/daemon/clima-daemon" ]; then
  autostart="$root/etc/xdg/autostart/$app_id.Daemon.desktop"
  if [ ! -f "$autostart" ]; then
    note "the daemon was built but installed no autostart entry at etc/xdg/autostart/$app_id.Daemon.desktop"
  elif ! command -v desktop-file-validate > /dev/null; then
    echo "packaging: no desktop-file-validate; the autostart entry is not validated." >&2
  elif ! desktop-file-validate "$autostart"; then
    # `if !`, not `cmd && echo`. This script runs under `set -e`, where a bare
    # `cmd && echo ok` aborts on failure instead of reaching note() — so the one
    # thing it would have to say is the one thing it would not print.
    note "the daemon's autostart entry is not a valid desktop file"
  else
    echo "autostart entry: valid"
  fi
fi

# ---- the desktop entry ------------------------------------------------------
desktop="$stage/share/applications/$app_id.desktop"
if [ ! -f "$desktop" ]; then
  note "no desktop entry at share/applications/$app_id.desktop"
else
  desktop-file-validate "$desktop" || note "desktop-file-validate rejected the entry"

  # The Icon= key has to name the app ID, not a file path and not `clima`.
  # An icon name that does not match an installed hicolor entry falls back to
  # a generic square, and nothing reports it.
  if ! grep -qx "Icon=$app_id" "$desktop"; then
    note "Icon= in the desktop entry is not $app_id"
  fi
fi

# ---- the AppStream component ------------------------------------------------
metainfo="$stage/share/metainfo/$app_id.metainfo.xml"
if [ ! -f "$metainfo" ]; then
  note "no AppStream component at share/metainfo/$app_id.metainfo.xml"
else
  # --no-net because CI has no network by policy — see the `test` job in
  # .github/workflows/ci.yml, which asserts that nothing opens a socket. The
  # cost is that a broken screenshot URL is not caught here; the release
  # workflow publishes those and is where reaching them is checked.
  appstreamcli validate --strict --no-net "$metainfo" \
    || note "appstreamcli rejected the component"
fi

# ---- the icons --------------------------------------------------------------
for size in 16 24 32 48 64 128 256 512; do
  icon="$stage/share/icons/hicolor/${size}x${size}/apps/$app_id.png"
  [ -f "$icon" ] || note "no ${size}x${size} icon at ${icon#"$stage"/}"
done

svg="$stage/share/icons/hicolor/scalable/apps/$app_id.svg"
[ -f "$svg" ] || note "no scalable icon at ${svg#"$stage"/}"

# And that the committed PNGs are still what the master renders to. Different
# question from "did they install": this one catches a hand-edited PNG.
"$here/icons.sh" check || fail=1

# ---- the binary -------------------------------------------------------------
[ -x "$stage/bin/clima" ] || note "no executable at bin/clima"

if [ "$fail" -ne 0 ]; then
  echo "packaging: FAILED" >&2
  exit 1
fi

echo "packaging: desktop entry, AppStream component and 9 icons are installable and valid"
