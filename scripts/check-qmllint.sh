#!/usr/bin/env bash
# SPDX-FileCopyrightText: 2026 Jowi Aoun
# SPDX-License-Identifier: GPL-3.0-or-later
#
# qmllint, as a ratchet rather than a gate.
#
#   scripts/check-qmllint.sh            fail if any category got worse
#   scripts/check-qmllint.sh --accept   record the current counts
#
# CLIMA_BUILD_DIR selects the build; it defaults to build/dev.
#
# ---- why a ratchet ----------------------------------------------------------
#
# There are 459 warnings in this tree and 443 of them are `unqualified`. That is
# not neglect: the prototype was written as one document where every id was in
# scope, and qualifying all of them is a 13,400-line edit that would touch every
# file and make one commit out of a hundred unrelated changes. Demanding zero
# today means either that edit or a blanket suppression, and the blanket is what
# actually happens.
#
# So the count is recorded and may only go down. A file that is being changed
# anyway gets qualified as it is touched, the number falls, and the baseline
# falls with it. What this stops is the thing worth stopping: a new file
# arriving with fifty more.
#
# ---- why the count is per category ------------------------------------------
#
# A single total is a ratchet with a hole in it. Fixing one unqualified access
# while introducing one `missing-property` leaves the total unchanged, and those
# two are not the same kind of thing at all — an unqualified access costs
# `qmlcachegen` an AOT compilation, which is a first-paint cost, while a missing
# property is a binding that will resolve to `undefined` and paint nothing.
#
# ---- what these warnings actually cost --------------------------------------
#
# Unqualified access is the reason to care beyond tidiness: qmlcachegen cannot
# ahead-of-time compile a binding whose lookups it cannot resolve, so every one
# of them is a binding interpreted at run time. docs/03-tech-stack.md §3.4 wants
# first paint under 400 ms, and this is where a good deal of it goes.
set -euo pipefail

here="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
root="$(cd "$here/.." && pwd)"

build_dir="${CLIMA_BUILD_DIR:-$root/build/dev}"
baseline="$root/tests/qmllint-baseline"
accept=0

if [[ "${1:-}" == "--accept" ]]; then
    accept=1
elif [[ $# -gt 0 ]]; then
    echo "check-qmllint: unknown argument \"$1\"" >&2
    exit 2
fi

if [[ ! -d "$build_dir" ]]; then
    echo "check-qmllint: $build_dir is not there. Configure first, or set CLIMA_BUILD_DIR." >&2
    exit 2
fi

log="$(mktemp)"
trap 'rm -f "$log"' EXIT

# The target qt_add_qml_module generates. It reports and exits 0 by design,
# which is exactly why this script exists rather than a CMake option.
cmake --build "$build_dir" --target all_qmllint > "$log" 2>&1 || {
    echo "check-qmllint: the all_qmllint target failed to run" >&2
    tail -20 "$log" >&2
    exit 2
}

# One line per category, "count name", sorted by name so the file is stable
# under a diff and a new category lands in an obvious place.
current="$(grep '^Warning:' "$log" \
    | grep -oE '\[[a-z-]+\]$' \
    | tr -d '[]' \
    | sort | uniq -c \
    | awk '{ print $2, $1 }' \
    | sort)"

if [[ $accept -eq 1 ]]; then
    printf '%s\n' "$current" > "$baseline"
    echo "check-qmllint: recorded"
    sed 's/^/  /' "$baseline"
    exit 0
fi

if [[ ! -f "$baseline" ]]; then
    echo "check-qmllint: no baseline at $baseline — run with --accept once to record one." >&2
    exit 2
fi

status=0
improved=0

while read -r category count; do
    [[ -z "$category" ]] && continue
    was="$(awk -v c="$category" '$1 == c { print $2 }' "$baseline")"

    if [[ -z "$was" ]]; then
        echo "check-qmllint: new warning category \"$category\" ($count)" >&2
        status=1
    elif (( count > was )); then
        echo "check-qmllint: $category went from $was to $count" >&2
        status=1
    elif (( count < was )); then
        echo "check-qmllint: $category improved, $was down to $count"
        improved=1
    fi
done <<< "$current"

# A category that vanished entirely is an improvement the loop above cannot
# see, because it iterates over what is there now.
while read -r category was; do
    [[ -z "$category" ]] && continue
    if ! grep -qE "^$category " <<< "$current"; then
        echo "check-qmllint: $category is gone, was $was"
        improved=1
    fi
done < "$baseline"

if [[ $status -ne 0 ]]; then
    echo "check-qmllint: qmllint got worse. Fix the new warnings, or say here why" >&2
    echo "check-qmllint: they are acceptable and run --accept." >&2
    exit 1
fi

if [[ $improved -eq 1 ]]; then
    echo "check-qmllint: run scripts/check-qmllint.sh --accept to lower the baseline."
else
    echo "check-qmllint: ok — no category got worse"
fi
