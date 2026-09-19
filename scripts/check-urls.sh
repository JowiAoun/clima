#!/usr/bin/env bash
# SPDX-FileCopyrightText: 2026 Jowi Aoun
# SPDX-License-Identifier: GPL-3.0-or-later
#
# Checks that every link back to this project names the same repository - and,
# inside GitHub Actions, that it is the repository the workflow is running in.
#
#   scripts/check-urls.sh            report; fail only on internal disagreement
#   scripts/check-urls.sh --strict   fail on a mismatch with GITHUB_REPOSITORY
#
# ---- what this is for -------------------------------------------------------
#
# The project URL is written down in sixteen files: the AppStream component, the
# desktop entry's origin, the Play listing, the privacy policy, the MSI, the
# Debian copyright file, the written offer for Qt's source, three issue
# templates, the README and the User-Agent every outbound request carries. Every
# one of them is a separate copy of one string.
#
# Two things then go wrong, and neither is visible from inside a diff:
#
#   * One copy is edited and fifteen are not. The store listing links to a page
#     that is not there, which is the one kind of broken link a user cannot
#     route around and a Flathub reviewer will reject.
#
#   * All sixteen agree with each other and none of them agrees with the
#     repository. That is what a rename does, and it is silent: the tree is
#     self-consistent, every test passes, and every published link 404s.
#
# So the release workflow runs this with --strict, and a release stops rather
# than shipping a store page that points nowhere. The lint job runs it without,
# because a pull request should not go red over a repository setting nobody in
# the diff can change.
set -euo pipefail

here="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
root="$(cd "$here/.." && pwd)"
cd "$root"

strict=0
case "${1:-}" in
  --strict) strict=1 ;;
  "") ;;
  -h|--help) sed -n '5,8p' "${BASH_SOURCE[0]}" | sed 's/^# \?//'; exit 0 ;;
  *) echo "usage: check-urls.sh [--strict]" >&2; exit 2 ;;
esac

# The one copy that is the source of truth, because it is the one CMake hands to
# everything it generates.
canonical="$(sed -n 's#^ *HOMEPAGE_URL "https://github.com/\([^"]*\)".*#\1#p' CMakeLists.txt)"

if [ -z "$canonical" ]; then
  echo "urls: CMakeLists.txt has no HOMEPAGE_URL of the form https://github.com/<owner>/<repo>" >&2
  exit 1
fi

owner="${canonical%%/*}"
repo="${canonical#*/}"
echo "urls: CMakeLists.txt says $canonical"

status=0

# ---- every other copy, against that one -------------------------------------
#
# Only this owner's repositories. A link to KDE, Open-Meteo or linuxdeploy is a
# link to somebody else and has nothing to do with this check. -I skips the
# binaries.
#
# Two spellings, because half of them are not written as a URL at all: the
# README tells a reader to run `gh attestation verify --repo <owner>/<repo>`,
# and a Flathub submission is a repository name on its own. They are the same
# string and they go stale the same way.
#
# The second pattern has to say what may come before the name, and that is not
# fussiness: `/io/github/JowiAoun/Climat/Daemon` is the daemon's D-Bus object
# path, it contains the owner followed by a slash, and it is not a repository.
# A rename must not touch it.
url_names="$(git grep -hoI -E "github\.com/${owner}/[A-Za-z0-9_.-]+" | sed 's#.*/##' || true)"
bare_names="$(git grep -hoI -E "(^|[[:space:]\`'\"(])${owner}/[A-Za-z0-9_.-]+" \
  | sed "s#.*${owner}/##" || true)"

mismatched="$(printf '%s\n%s\n' "$url_names" "$bare_names" \
  | sed 's/[^A-Za-z0-9_-].*$//' \
  | grep -v '^$' \
  | sort -u \
  | grep -v -x "$repo" || true)"

if [ -n "$mismatched" ]; then
  echo "urls: these name a different repository of $owner's:" >&2
  while read -r other; do
    [ -z "$other" ] && continue
    echo "  $other, in:" >&2
    git grep -lI -E "(github\.com/|[[:space:]\`'\"(])${owner}/${other}([^A-Za-z0-9_-]|\$)" \
      | sed 's/^/    /' >&2
  done <<< "$mismatched"
  status=1
fi

# ---- the Pages host, which is where the store screenshots live ---------------
#
# https://<owner>.github.io/<repo>/ - lower case, because that is what GitHub
# serves. Not to be confused with the GNOME extension's UUID, which is
# climat@JowiAoun.github.io and is a name rather than an address, which is why
# this matches only after a scheme.
pages_expected="$(printf '%s' "$owner" | tr '[:upper:]' '[:lower:]').github.io/$repo"
pages_wrong="$(git grep -hoI -E "https://$(printf '%s' "$owner" | tr '[:upper:]' '[:lower:]')\.github\.io/[A-Za-z0-9_.-]+" \
  | sed 's#^https://##' \
  | sort -u \
  | grep -v -x "$pages_expected" || true)"

if [ -n "$pages_wrong" ]; then
  echo "urls: these Pages addresses do not match $pages_expected:" >&2
  printf '  %s\n' "$pages_wrong" >&2
  status=1
fi

if [ "$status" -eq 0 ]; then
  echo "urls: every link to this project names $canonical"
fi

# ---- and the repository this is actually running in --------------------------
if [ -n "${GITHUB_REPOSITORY:-}" ] && [ "${GITHUB_REPOSITORY}" != "$canonical" ]; then
  echo >&2
  echo "urls: the tree says $canonical and this is ${GITHUB_REPOSITORY}." >&2
  echo "urls: every published link - the AppStream homepage a software centre" >&2
  echo "urls:   shows, the bug tracker in the issue templates, the Play listing," >&2
  echo "urls:   the privacy policy, the Pages screenshots - points at a" >&2
  echo "urls:   repository that is not there." >&2
  echo "urls: fix it by renaming the repository to ${repo}, or by changing" >&2
  echo "urls:   HOMEPAGE_URL in CMakeLists.txt and the files beside it." >&2
  if [ "$strict" -eq 1 ]; then
    exit 1
  fi
  echo "::warning title=Project URLs::The tree links to $canonical; this is ${GITHUB_REPOSITORY}. A release will refuse to publish."
fi

exit "$status"
