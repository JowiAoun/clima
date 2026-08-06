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

stage="$(mktemp -d)"
trap 'rm -rf "$stage"' EXIT

echo "packaging: staging an install of $app_id"
cmake --install "$build_dir" --prefix "$stage" > /dev/null

fail=0
note() {
  echo "packaging: $1" >&2
  fail=1
}

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
