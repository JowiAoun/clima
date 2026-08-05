#!/usr/bin/env bash
# SPDX-FileCopyrightText: 2026 Jowi Aoun
# SPDX-License-Identifier: GPL-3.0-or-later
#
# Asks the *built binary* the two licence questions the build system can only
# ask of its own model.
#
#   scripts/check-licences.sh <binary> [binary…]
#
# ---- why this exists alongside cmake/ClimaLicenceGuard.cmake ----------------
#
# That file refuses at configure time if a banned Qt module is named in a
# find_package component list or a target_link_libraries call. It is the right
# place for the check and it catches the way this mistake is normally made.
#
# What it cannot see is a module that arrives without being named. Qt modules
# pull in other Qt modules: a QML import in a .qml file resolves a plugin at run
# time, `qt_add_qml_module` scans imports and adds dependencies, and a future
# `find_package(Qt6 COMPONENTS Multimedia)` would bring a graph nobody wrote
# down. None of those appear in CMake as a banned name; all of them appear in
# the linked binary as a DT_NEEDED entry.
#
# So this reads the artefact. The two are not redundant — one checks the recipe
# and one checks the cake, and R2 in docs/08-risks.md is a risk precisely
# because the two can differ.
#
# ---- the two questions ------------------------------------------------------
#
# R2. No GPLv3-or-commercial Qt module. Qt Charts, Graphs, Lottie, Quick3D and
#     VirtualKeyboard are licensed GPLv3-or-commercial rather than
#     LGPLv3 — so linking one does not merely add a dependency, it relicenses
#     the result. The app is GPL-3.0-or-later and would survive that; libclima
#     is MPL-2.0 and would not, and neither would any downstream reuse of it,
#     which is the entire point of the split in D6.
#
# R3. Qt must be linked dynamically. LGPLv3 §4 lets a proprietary-or-differently
#     licensed work link an LGPL library only if the user can relink against a
#     modified version. Static linking removes that, and the obligation it
#     replaces it with is to ship the object files. A statically linked Qt is
#     therefore not a build-size decision — it is a licence change made by
#     accident, and it is invisible in the source.
#
# Both are checked with objdump because that is what a distribution's own
# licence audit would use. Missing objdump is a hard error rather than a skip:
# a licence check that passes because a tool was absent is worse than no check.
set -euo pipefail

if [[ $# -eq 0 ]]; then
    sed -n '5,8p' "${BASH_SOURCE[0]}" | sed 's/^# \?//'
    exit 2
fi

objdump=""
for candidate in objdump llvm-objdump; do
    if command -v "$candidate" >/dev/null 2>&1; then
        objdump="$candidate"
        break
    fi
done

if [[ -z "$objdump" ]]; then
    echo "check-licences: no objdump or llvm-objdump on PATH." >&2
    echo "check-licences: refusing to report success without having looked." >&2
    exit 2
fi

# The same five as cmake/ClimaLicenceGuard.cmake, spelled as the shared-object
# names a linker actually records. Kept in step by hand, and the comment there
# says so too — two spellings of one list is the price of asking the question in
# two places, and the list changes about once a Qt major.
banned_libs=(
    libQt6Charts
    libQt6Graphs
    libQt6Lottie
    libQt6Quick3D
    libQt6VirtualKeyboard
)

status=0

for binary in "$@"; do
    if [[ ! -f "$binary" ]]; then
        echo "check-licences: $binary does not exist" >&2
        status=1
        continue
    fi

    # DT_NEEDED only. Not the full dynamic-section dump and not `ldd`: ldd
    # resolves the whole transitive closure through the loader, which would
    # report a library that Qt itself pulls in on this machine as though we had
    # linked it, and — more to the point — ldd runs the binary's loader, which
    # is not a thing to do to an artefact you are auditing.
    needed="$($objdump -p "$binary" 2>/dev/null | awk '$1 == "NEEDED" { print $2 }')"

    if [[ -z "$needed" ]]; then
        echo "check-licences: $binary lists no shared libraries at all." >&2
        echo "check-licences: that is either a static link (R3) or not an ELF file." >&2
        status=1
        continue
    fi

    for lib in "${banned_libs[@]}"; do
        if grep -q "^${lib}" <<<"$needed"; then
            echo "check-licences: $binary links $lib" >&2
            echo "  That module is GPLv3-or-commercial, not LGPLv3. See R2 in" >&2
            echo "  docs/08-risks.md; draw the chart with QQuickItem and QSGNode" >&2
            echo "  instead — that is what ClimaCharts is for." >&2
            status=1
        fi
    done

    # R3, asked positively. "No banned module" is satisfied by a binary with no
    # Qt in it at all, which is exactly what a static link looks like from here,
    # so the check that matters is that Qt Core is present as a shared object.
    if ! grep -q "^libQt6Core" <<<"$needed"; then
        echo "check-licences: $binary does not link libQt6Core dynamically." >&2
        echo "  A statically linked Qt is a licence change, not a size choice:" >&2
        echo "  LGPLv3 §4 requires that the user can relink against a modified" >&2
        echo "  Qt. See R3 in docs/08-risks.md." >&2
        status=1
    fi
done

if [[ $status -eq 0 ]]; then
    echo "check-licences: ok — $# binary/binaries, no GPLv3-only Qt module, Qt linked dynamically"
fi

exit $status
