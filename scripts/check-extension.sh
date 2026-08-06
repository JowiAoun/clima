#!/usr/bin/env bash
# SPDX-FileCopyrightText: 2026 Jowi Aoun
# SPDX-License-Identifier: GPL-3.0-or-later
#
# What can be checked about the GNOME Shell extension without a GNOME Shell.
#
#   scripts/check-extension.sh
#
# Four things, and it is worth being precise about which four, because the
# gap between them and "the extension works" is where the manual acceptance
# test in scripts/shell-probe.sh lives:
#
#   1. Both JS files PARSE as ES modules. gjs resolves imports after parsing, so
#      a module that fails only on `resource:///org/gnome/shell/...` is a module
#      whose own syntax is fine — and that is exactly what a checkout has to be
#      able to establish, because those resources only exist inside gnome-shell.
#
#   2. The D-Bus introspection XML the extension carries is VALID and describes
#      the same methods clima-daemon exports. This is a hand-maintained copy of
#      daemon/daemonadaptor.h — it has to be, since the extension ships from
#      extensions.gnome.org and the daemon from Flathub — so the one thing that
#      can be checked here is that it parses and that its names match.
#
#   3. Every Meta.WaylandClient method the extension calls EXISTS on this
#      machine's mutter. This is the check that matters most. The spike this
#      extension grew out of called `query_window_belongs_to`, which is DING's
#      own wrapper name and not a method on the object at all; the failure was a
#      TypeError at map time, an hour into a nested-shell run.
#
#   4. metadata.json parses and the GSettings schema compiles --strict.
#
# NOT run by CI: it needs gjs and a mutter typelib, and installing a GNOME on a
# runner would test that stack rather than the one users have. It is a developer
# action, like scripts/shell-probe.sh, and it is much cheaper than that one.

set -euo pipefail

here="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
repo="$(cd "$here/.." && pwd)"
ext="$repo/packaging/gnome-shell/clima@JowiAoun.github.io"

fail() { printf 'check-extension: %s\n' "$1" >&2; exit 1; }

command -v gjs >/dev/null || fail "no gjs. On Debian and Ubuntu: apt install gjs"

[ -d "$ext" ] || fail "no extension at $ext"

# ---- 3's prerequisite: find mutter's typelibs --------------------------------
#
# They are not on the default GI search path because they are private to
# gnome-shell: /usr/lib/<triplet>/mutter-<abi>/Meta-<abi>.typelib. The ABI number
# tracks the GNOME release (14 is GNOME 46), so the glob is over versions rather
# than a fixed path.
# A plain glob rather than `compgen -G`: compgen is a bash builtin that is not
# available in every bash this repository's devshell might hand us, and it
# failed silently here — reporting "no mutter typelib" on a machine that has one,
# which is the worst kind of skip.
mutter_dir=""
for typelib in /usr/lib/*/mutter-*/Meta-*.typelib /usr/lib64/mutter-*/Meta-*.typelib; do
    if [ -e "$typelib" ]; then
        mutter_dir="$(dirname "$typelib")"
        break
    fi
done

if [ -n "$mutter_dir" ]; then
    export GI_TYPELIB_PATH="$mutter_dir${GI_TYPELIB_PATH:+:$GI_TYPELIB_PATH}"
    echo "mutter typelibs: $mutter_dir"
else
    # Reported, not fatal. A machine with no GNOME installed can still check
    # 1, 2 and 4, and saying which check was skipped is the difference between
    # a partial pass and a pass that is quietly missing its best assertion.
    echo "check-extension: no mutter typelib found; the WaylandClient check is SKIPPED." >&2
fi

# ---- 1: do the modules parse? ------------------------------------------------
#
# gjs exits non-zero either way, so the discrimination is in the message: a
# SyntaxError is ours and an ImportError is the shell's resources being absent,
# which is expected outside gnome-shell.
for file in extension.js prefs.js; do
    output="$(gjs -m "$ext/$file" 2>&1 || true)"
    if grep -q 'SyntaxError' <<<"$output"; then
        printf '%s\n' "$output" >&2
        fail "$file does not parse"
    fi
    if ! grep -qE 'ImportError|does not exist' <<<"$output"; then
        # It ran to completion, which for these two files means the shell's
        # resources were somehow present. Not a failure — just not the outcome
        # this check was written around, and worth saying so.
        echo "note: $file evaluated without an ImportError"
    fi
    echo "parse: $file ok"
done

# ---- 2, 3, 4 ----------------------------------------------------------------

gjs -m "$here/check-extension.js" "$ext" || fail "the JS checks failed"

# The schema, with glib's own validator. --strict turns a warning into an exit
# code; --dry-run keeps a compiled gschemas.compiled out of the source tree.
if command -v glib-compile-schemas >/dev/null; then
    glib-compile-schemas --strict --dry-run "$ext/schemas"
    echo "schema: ok"
else
    echo "check-extension: no glib-compile-schemas; the schema check is SKIPPED." >&2
fi

echo "check-extension: ok"
