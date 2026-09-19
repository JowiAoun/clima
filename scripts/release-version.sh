#!/usr/bin/env bash
# SPDX-FileCopyrightText: 2026 Jowi Aoun
# SPDX-License-Identifier: GPL-3.0-or-later
#
# The version a release artefact is named after, as a line for $GITHUB_ENV.
#
#   bash scripts/release-version.sh >> "$GITHUB_ENV"
#   bash scripts/release-version.sh          just prints it, anywhere
#
# From the tag when the workflow was triggered by one, because that is the
# authority: release-please pushed it and it matches what the release is called.
#
# From CMakeLists.txt otherwise, with `-dev` after it. `workflow_dispatch` is
# how this workflow gets rehearsed, and a dispatch off a branch has no tag - so
# `${GITHUB_REF_NAME#v}` gave "main", and the rehearsal produced
# `climat-main-windows-x64.msi`. The suffix is there so that a rehearsed
# artefact and a released one can never be confused for one another.
set -euo pipefail

here="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
root="$(cd "$here/.." && pwd)"

case "${GITHUB_REF:-}" in
  refs/tags/*)
    version="${GITHUB_REF_NAME#v}"
    ;;
  *)
    # The same sed scripts/licence-bundle.sh and scripts/flatpak.sh use, against
    # the one line release-please rewrites.
    version="$(sed -n 's/^ *VERSION \([0-9][0-9.]*\).*/\1/p' "$root/CMakeLists.txt" | head -1)-dev"
    ;;
esac

if [ -z "$version" ] || [ "$version" = "-dev" ]; then
  echo "release-version: no tag and no VERSION in CMakeLists.txt" >&2
  exit 1
fi

echo "CLIMAT_RELEASE_VERSION=$version"
