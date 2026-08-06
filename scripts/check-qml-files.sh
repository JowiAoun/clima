#!/usr/bin/env bash
# SPDX-FileCopyrightText: 2026 Jowi Aoun
# SPDX-License-Identifier: GPL-3.0-or-later
#
# Checks that each CMakeLists.txt with a QML module in it lists every QML and JS
# file that is on disk beside it, and that every file it lists is on disk.
#
#   scripts/check-qml-files.sh          report, exit 1 on a mismatch
#
# Why this exists, given that the alternative is one line of file(GLOB):
#
# A globbed QML module is correct until someone adds a file and builds without
# re-running CMake. The new file is then simply not in the module, so the type
# it declares does not exist — and QML reports that at the point of *use*. The
# error names HourlyOverview.qml, which nobody touched, and says a type is not a
# type; the file that is actually missing is not mentioned. It is a genuinely
# expensive twenty minutes, and it is the same twenty minutes every time.
#
# So the list is explicit, and this script is what keeps the explicit list from
# going stale. Run it in CI and, if you like, from a pre-commit hook.

set -euo pipefail

here="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
repo="$(cd "$here/.." && pwd)"

# Every QML module in the tree: the source-relative path its files live at, and
# the CMakeLists.txt that has to name them. The path is what appears in the
# build file, so it is also what this greps for — see below.
#
# Three, because the component gallery and the desktop widget host are each
# their own executable with their own QML module. Adding a fourth means adding a
# line here; a module that is not listed is a module this script silently
# approves of.
#
# widgets/ is checked for the files it OWNS. Its CMakeLists.txt also lists a
# handful of app/qml/Clima/ files by absolute path — the presentation components
# a tile shares with the app — and those are deliberately outside this check:
# they are already covered by the app's own entry, and a second claim on them
# here would report every one of them as "on disk but not listed" the moment the
# widget host stopped using one.
modules=(
    "app:qml/Clima"
    "gallery:qml/Clima/Gallery"
    "widgets:qml/Clima/Widgets"
)

# Reported indented, because a bare column of filenames under an error line
# reads as part of the sentence above it.
indent() {
    while IFS= read -r line; do
        printf '    %s\n' "$line"
    done
}

on_disk="$(mktemp)"
in_cmake="$(mktemp)"
trap 'rm -f "$on_disk" "$in_cmake"' EXIT

status=0
counted=0

for entry in "${modules[@]}"; do
    dir="${entry%%:*}"
    prefix="${entry#*:}"

    module_dir="$repo/$dir/$prefix"
    cmake_file="$repo/$dir/CMakeLists.txt"

    for path in "$module_dir" "$cmake_file"; do
        if [[ ! -e "$path" ]]; then
            echo "check-qml-files: no such path: $path" >&2
            exit 2
        fi
    done

    # What is on disk. LC_ALL=C so the ordering is the byte ordering everywhere
    # and not whatever the caller's locale thinks about case — this list is
    # compared with `comm`, which requires both sides sorted the same way.
    #
    # A glob and not `find`, so it stays in this directory. A submodule's files
    # are the submodule's own entry in the table above, and a recursive listing
    # would report every one of them as missing from its parent's list.
    #
    # nullglob so an empty module directory produces nothing rather than the two
    # literal patterns, which would then be reported as two missing files and
    # send the reader looking for a file called `*.qml`.
    (
        shopt -s nullglob
        cd "$module_dir" || exit 1
        printf '%s\n' *.qml *.js
    ) | LC_ALL=C sort >"$on_disk"

    # What the build lists. Comments are stripped first and then every
    # `<prefix>/<name>` left standing is taken as an entry.
    #
    # Stripping the comments is not optional: these files explain their own
    # resource layout in prose and name example paths while doing it, and a path
    # inside a `#` comment is not a file in the module. Matching a path prefix
    # rather than parsing the set() blocks is deliberate though — it survives the
    # list being reordered, renamed or split into more variables, which a block
    # parser would not.
    #
    # The absent `/` in the character class is what keeps `qml/Clima` from
    # claiming `qml/Clima/Gallery`'s files — one is a path prefix of the other,
    # and a name that may not contain a slash is the whole of the distinction.
    LC_ALL=C sed 's/#.*$//' "$cmake_file" \
        | LC_ALL=C grep -oE "$prefix/[A-Za-z0-9_.-]+\.(qml|js)" \
        | sed "s#^$prefix/##" \
        | LC_ALL=C sort -u >"$in_cmake"

    missing="$(LC_ALL=C comm -23 "$on_disk" "$in_cmake")"
    extra="$(LC_ALL=C comm -13 "$on_disk" "$in_cmake")"

    if [[ -n "$missing" ]]; then
        status=1
        echo "error: on disk but not in $dir/CMakeLists.txt QML_FILES:" >&2
        printf '%s\n' "$missing" | indent >&2
        echo >&2
        echo "  Add them to that file's QML or JS list. Until you do, the type each" >&2
        echo "  one declares does not exist, and the error you get will name a" >&2
        echo "  different file." >&2
    fi

    if [[ -n "$extra" ]]; then
        status=1
        echo "error: listed in $dir/CMakeLists.txt but not on disk:" >&2
        printf '%s\n' "$extra" | indent >&2
        echo >&2
        echo "  Remove them, or restore the files. CMake fails to configure with a" >&2
        echo "  missing QML_FILES entry, so this is a build breakage waiting for the" >&2
        echo "  next clean checkout." >&2
    fi

    counted=$((counted + $(wc -l <"$on_disk")))
done

if [[ $status -eq 0 ]]; then
    echo "check-qml-files: ok — $counted files across ${#modules[@]} modules, lists match disk"
fi

exit "$status"
