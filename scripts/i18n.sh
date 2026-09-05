#!/usr/bin/env bash
# SPDX-FileCopyrightText: 2026 Jowi Aoun
# SPDX-License-Identifier: GPL-3.0-or-later
#
# The translatable strings, and the catalogues made from them.
#
#   scripts/i18n.sh update    regenerate app/translations/clima.ts from the source
#   scripts/i18n.sh check     fail if it is out of date (CI runs this)
#
# ---- what the template is, and what it is not -------------------------------
#
# `app/translations/clima.ts` is the SOURCE catalogue: every string the app and
# the CLI mark for translation, with no translations in it. It is what a
# translation platform imports, and it is committed so that a pull request
# adding a user-facing sentence shows that sentence in its diff. Nothing loads
# it at run time.
#
# A LANGUAGE catalogue is `clima_<locale>.ts` beside it — `clima_fr.ts`,
# `clima_pt_BR.ts` — and there are none yet, which is stated in
# docs/known-gaps.md rather than papered over with a machine translation.
# Adding one is a file and a line in app/CMakeLists.txt; nothing else changes,
# because app/apptranslator.cpp already looks for whatever was compiled in.
#
# ---- two flags that are not incidental --------------------------------------
#
#   -I .            every include in this tree is written from the repository
#                   root — `#include "app/viewmodels/alertsdata.h"` — and
#                   lupdate resolves includes against the including file's own
#                   directory. Without this it never parses alertsdata.h, does
#                   not know the class, and reports "Qualifying with unknown
#                   namespace/class ::AlertsData" for two strings that then
#                   land in no context at all. A string in the wrong context is
#                   a string whose translation is never applied.
#
#   -locations none the template would otherwise carry a file and a LINE NUMBER
#                   for every one of its entries, so a commit that moved any
#                   line in any file would leave it stale and the check below
#                   would fail on unrelated work. Without locations it changes
#                   only when the strings change, which is the thing worth
#                   gating and the thing worth reading in a diff.

set -euo pipefail

root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
template="$root/app/translations/clima.ts"

mode="${1:-update}"

if ! command -v lupdate > /dev/null 2>&1; then
  echo "i18n: lupdate is not on PATH — it comes with Qt's LinguistTools (qt6-tools)." >&2
  exit 2
fi

generate() {
  # Into a file of the caller's choosing, so that `check` can compare rather
  # than overwrite the thing it is checking.
  # widgets/ as well as app/ and cli/. The tiles carry their own qsTr() calls —
  # a tile says "No weather service" and "Updated N min ago" — and leaving that
  # directory out meant this gate reported "ok" for strings no translator would
  # ever be offered, which is the failure it exists to prevent.
  lupdate -I "$root" -recursive "$root/app" "$root/cli" "$root/widgets" \
    -ts "$1" -no-obsolete -locations none -silent
}

case "$mode" in
  update)
    generate "$template"
    # Not `realpath --relative-to`, which is GNU coreutils only and aborts the
    # script under `set -e` on a BSD or a Mac after the template was written.
    echo "i18n: wrote ${template#"$root"/}"
    ;;

  check)
    if [ ! -r "$template" ]; then
      echo "i18n: no template at app/translations/clima.ts — run scripts/i18n.sh update" >&2
      exit 1
    fi

    scratch="$(mktemp -d)"
    trap 'rm -rf "$scratch"' EXIT
    fresh="$scratch/clima.ts"

    # lupdate merges into an existing file, so it has to start from the
    # committed one: generating into an empty file would mark every entry as
    # new and report a difference that is only about how the file was made.
    cp "$template" "$fresh"
    generate "$fresh"

    if diff -u "$template" "$fresh" > "$scratch/diff"; then
      echo "i18n: ok — the template matches the source"
      exit 0
    fi

    echo "i18n: app/translations/clima.ts is out of date. Run scripts/i18n.sh update." >&2
    echo >&2
    # Only the string lines, because the full diff is mostly XML scaffolding
    # and the question a reader has is "which sentence moved".
    grep -E '^[+-] *<source>' "$scratch/diff" | head -40 >&2
    exit 1
    ;;

  *)
    echo "usage: $0 [update|check]" >&2
    exit 2
    ;;
esac
