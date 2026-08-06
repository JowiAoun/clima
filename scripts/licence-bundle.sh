#!/usr/bin/env bash
# SPDX-FileCopyrightText: 2026 Jowi Aoun
# SPDX-License-Identifier: GPL-3.0-or-later
#
# Assemble the third-party licence bundle that ships with every release.
#
#   scripts/licence-bundle.sh [output]     defaults to THIRD-PARTY-LICENCES.txt
#
# Generated rather than written, and assembled from the two files that are
# already authoritative — packaging/linux/copyright for what covers what, and
# LICENSES/ for the texts themselves. A hand-maintained bundle is a third copy
# of the licensing story, and a third copy is the one that goes stale: it stays
# correct until somebody adds a dependency and updates the other two.
#
# `reuse lint` is what proves the inputs are complete. If a file in the tree
# carried a licence that LICENSES/ had no text for, reuse would fail before
# this script ever ran — which is why this does no checking of its own.
set -euo pipefail

here="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
root="$(cd "$here/.." && pwd)"

output="${1:-$root/THIRD-PARTY-LICENCES.txt}"
version="$(sed -n 's/^ *VERSION \([0-9][0-9.]*\).*/\1/p' "$root/CMakeLists.txt" | head -1)"

rule() { printf '%.0s=' {1..78}; printf '\n'; }

{
  rule
  echo "  CLIMA ${version} — THIRD-PARTY LICENCES"
  rule
  echo
  cat <<'PREAMBLE'
Clima is GNU GPL v3 or later. This file is about everything else it contains.

The clima executable is statically linked and carries, inside the binary, a
typeface, a place-name database and several recorded weather-service payloads.
Each arrives under its own licence, and none of them is ours. Qt is linked
dynamically and is covered separately by the written offer in
QT-SOURCE-OFFER.txt.

Part one below says what covers what. Part two is the full text of every
licence named in part one. Both are generated from the repository's own SPDX
metadata, which `reuse lint` gates on every commit — so this file cannot
describe a set of components that is not the set actually shipped.
PREAMBLE
  echo
  echo
  rule
  echo "  PART ONE — WHAT COVERS WHAT"
  rule
  echo
  cat "$root/packaging/linux/copyright"
  echo
  echo
  rule
  echo "  PART TWO — THE LICENCE TEXTS"
  rule

  for licence in "$root"/LICENSES/*.txt; do
    name="$(basename "$licence" .txt)"
    echo
    echo
    rule
    echo "  $name"
    rule
    echo
    cat "$licence"
  done
} > "$output"

echo "licences: $output ($(wc -l < "$output") lines, $(wc -c < "$output") bytes)"
