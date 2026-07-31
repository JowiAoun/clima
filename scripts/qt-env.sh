# SPDX-FileCopyrightText: 2026 Jowi Aoun
# SPDX-License-Identifier: GPL-3.0-or-later
#
# shellcheck shell=bash
#
# Finds a Qt 6 and describes it to whoever asked. Source this; do not run it.
# There is no shebang and no execute bit on purpose — the whole point is to put
# variables into the caller's environment, which a subprocess cannot do.
#
#   source scripts/qt-env.sh
#   clima_qt_env || { clima_qt_env_hint >&2; exit 1; }
#
# It reports a failure by returning non-zero rather than exiting, and it sets no
# shell options of its own. A library that called `set -e` would silently change
# how the script that sourced it handles every later error, and one that called
# `exit` would take an interactive shell down with it.
#
# After a successful call:
#
#   CLIMA_QML_BIN            the `qml` runtime, absolute
#   CLIMA_QT_PREFIX          qtbase's prefix — the first CMAKE_PREFIX_PATH entry
#   CLIMA_QT_QML_PREFIX      qtdeclarative's prefix, which on Nix is a different
#                            store path and therefore a second prefix entry
#   QML_IMPORT_PATH          \
#   QML2_IMPORT_PATH          }  set for a Nix-store Qt only, see below
#   QT_PLUGIN_PATH           /
#   QT_QPA_PLATFORMTHEME     /
#   QT_FORCE_STDERR_LOGGING  always
#
# Every export honours a value that was already set, so any one of them can be
# overridden from outside without editing anything here. CLIMA_QML picks the
# runtime; CLIMA_QT_PREFIX pins the prefix when the guess is wrong.
#
# This logic used to live in prototype/hourly-overview/run.sh, which is still
# its main caller. It moved because the CMake build needs the same answer, and
# two copies of "which Qt is this machine using" is one copy too many.

# What to tell a user who has no Qt 6. The caller prints it, because only the
# caller knows whether a missing Qt is fatal — inside `nix develop` it is a bug,
# in a bare terminal it is a Tuesday.
clima_qt_env_hint() {
    cat <<'EOF'
error: could not find Qt 6's `qml` runtime.

  Debian/Ubuntu : sudo apt install qml6-module-qtquick-shapes qml6-module-qtquick-window
  Fedora        : sudo dnf install qt6-qtdeclarative
  Arch          : sudo pacman -S qt6-declarative
  Nix           : nix develop            (this repo's flake), or
                  nix shell nixpkgs#qt6.qtdeclarative
  macOS         : brew install qt

Or point CLIMA_QML at a `qml` binary.
EOF
}

_clima_find_qml() {
    if [[ -n "${CLIMA_QML:-}" ]]; then
        printf '%s\n' "$CLIMA_QML"
        return 0
    fi
    local c
    for c in qml6 qml; do
        if command -v "$c" >/dev/null 2>&1; then
            command -v "$c"
            return 0
        fi
    done
    # Nix store: pick the highest qtdeclarative version present.
    # shellcheck disable=SC2012  # store paths are [a-z0-9-.]; `find` buys nothing here
    c=$(ls -d /nix/store/*-qtdeclarative-6*/bin/qml 2>/dev/null \
        | sed -E 's#.*-qtdeclarative-([0-9.]+)/bin/qml#\1 &#' \
        | sort -V | tail -n1 | cut -d' ' -f2 || true)
    if [[ -n "$c" && -x "$c" ]]; then
        printf '%s\n' "$c"
        return 0
    fi
    return 1
}

# A distro Qt knows its own prefix and will tell you if asked. Insist on 6.x:
# on a machine with both Qt versions installed, plain `qmake` is usually Qt 5
# and would hand back a prefix that has no Qt6Config.cmake under it at all.
_clima_qt_prefix_from_qmake() {
    local q v p
    for q in qmake6 qmake; do
        command -v "$q" >/dev/null 2>&1 || continue
        v="$("$q" -query QT_VERSION 2>/dev/null)" || continue
        [[ "$v" == 6.* ]] || continue
        p="$("$q" -query QT_INSTALL_PREFIX 2>/dev/null)" || continue
        [[ -n "$p" ]] || continue
        printf '%s\n' "$p"
        return 0
    done
    return 1
}

# No qmake6 — so work backwards from the answer. "The prefix" is not a property
# of the filesystem layout, it is whatever directory CMake can find
# Qt6Config.cmake beneath, so look for that file rather than guessing at how
# many `lib/x86_64-linux-gnu` levels to strip off.
_clima_qt_prefix_by_search() {
    local d
    d="$(cd "$1" 2>/dev/null && pwd)" || return 1
    while [[ -n "$d" && "$d" != "/" ]]; do
        if compgen -G "$d/lib*/cmake/Qt6/Qt6Config.cmake" >/dev/null 2>&1; then
            printf '%s\n' "$d"
            return 0
        fi
        d="$(dirname "$d")"
    done
    return 1
}

clima_qt_env() {
    local qml_bin qtd qtbase qroot guess

    if ! qml_bin=$(_clima_find_qml); then
        return 1
    fi
    export CLIMA_QML_BIN="$qml_bin"

    if [[ "$qml_bin" == /nix/store/* ]]; then
        # A raw binary out of the Nix store is not env-wrapped, so it cannot find
        # its own QML modules or platform plugins. Derive both from the store
        # paths.
        qtd="$(cd "$(dirname "$qml_bin")/.." && pwd)"
        qtbase="$(ldd "$qml_bin" 2>/dev/null \
            | sed -nE 's#.*=> (/nix/store/[^/]*qtbase[^/]*)/lib/libQt6Gui.*#\1#p' | head -n1)"

        export QML_IMPORT_PATH="${QML_IMPORT_PATH:-$qtd/lib/qt-6/qml}"
        export QML2_IMPORT_PATH="$QML_IMPORT_PATH"
        if [[ -n "$qtbase" ]]; then
            export QT_PLUGIN_PATH="${QT_PLUGIN_PATH:-$qtbase/lib/qt-6/plugins:$qtd/lib/qt-6/plugins}"
        else
            export QT_PLUGIN_PATH="${QT_PLUGIN_PATH:-$qtd/lib/qt-6/plugins}"
        fi

        # On a GNOME session Qt picks its gtk3 platform theme, which initialises
        # the host GTK inside our process. A Nix-store Qt links its own GTK
        # against a different module path, so GNOME's gtk-modules (canberra)
        # cannot be found and GTK prints "Failed to load module
        # canberra-gtk-module" on every launch. Harmless, but noisy. The generic
        # theme avoids loading host GTK at all.
        #
        # Scoped to Nix-store Qt deliberately: a distro or Flatpak Qt has a
        # consistent GTK stack, so it should keep gtk3 and the desktop
        # integration that comes with it. The real app gets font/dark-mode/dialog
        # integration through portals instead — see docs/04-architecture.md §4.9.
        export QT_QPA_PLATFORMTHEME="${QT_QPA_PLATFORMTHEME:-generic}"

        # Nix splits Qt across one derivation per module, so there is no single
        # prefix with all of Qt6Config.cmake, Qt6QmlConfig.cmake and friends
        # under it. A CMake configure needs both of these on CMAKE_PREFIX_PATH.
        # Empty when ldd told us nothing — an empty prefix is honest, a wrong one
        # sends CMake somewhere plausible and wrong.
        export CLIMA_QT_PREFIX="${CLIMA_QT_PREFIX:-$qtbase}"
        export CLIMA_QT_QML_PREFIX="${CLIMA_QT_QML_PREFIX:-$qtd}"
    else
        # A distro or Flatpak Qt is already wrapped and knows where its modules
        # and plugins live. Setting QML_IMPORT_PATH or QT_PLUGIN_PATH here would
        # add nothing but an opportunity to get them wrong, and overriding
        # QT_QPA_PLATFORMTHEME would cost the user their desktop integration.
        # So: read the prefix, touch nothing else.
        qroot="$(cd "$(dirname "$qml_bin")/.." 2>/dev/null && pwd)" || qroot=""
        guess=""
        if ! guess=$(_clima_qt_prefix_from_qmake); then
            if [[ -n "$qroot" ]]; then
                guess=$(_clima_qt_prefix_by_search "$qroot") || guess=""
            fi
        fi
        export CLIMA_QT_PREFIX="${CLIMA_QT_PREFIX:-$guess}"
        export CLIMA_QT_QML_PREFIX="${CLIMA_QT_QML_PREFIX:-$CLIMA_QT_PREFIX}"
    fi

    # Without this, Qt decides stderr has no console and silently swallows QML
    # errors — the app comes up blank and says nothing about why.
    export QT_FORCE_STDERR_LOGGING="${QT_FORCE_STDERR_LOGGING:-1}"

    return 0
}
