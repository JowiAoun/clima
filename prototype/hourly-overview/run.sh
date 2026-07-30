#!/usr/bin/env bash
# SPDX-License-Identifier: GPL-3.0-or-later
#
# Runs the hourly-overview prototype. There is no build step: it is pure QML,
# executed by Qt 6's `qml` runtime.
#
#   ./run.sh                          open the window
#   ./run.sh --grab shot.png          render one frame to a PNG and exit
#   CLIMA_QML=/path/to/qml ./run.sh   use a specific Qt
#   QT_QPA_PLATFORM=xcb ./run.sh      force X11 if Wayland misbehaves

set -euo pipefail

here="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

find_qml() {
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
    c=$(ls -d /nix/store/*-qtdeclarative-6*/bin/qml 2>/dev/null \
        | sed -E 's#.*-qtdeclarative-([0-9.]+)/bin/qml#\1 &#' \
        | sort -V | tail -n1 | cut -d' ' -f2 || true)
    if [[ -n "$c" && -x "$c" ]]; then
        printf '%s\n' "$c"
        return 0
    fi
    return 1
}

if ! qml_bin=$(find_qml); then
    cat >&2 <<'EOF'
error: could not find Qt 6's `qml` runtime.

  Debian/Ubuntu : sudo apt install qml6-module-qtquick-shapes qml6-module-qtquick-window
  Fedora        : sudo dnf install qt6-qtdeclarative
  Arch          : sudo pacman -S qt6-declarative
  Nix           : nix shell nixpkgs#qt6.qtdeclarative
  macOS         : brew install qt

Or point CLIMA_QML at a `qml` binary.
EOF
    exit 1
fi

# A raw binary out of the Nix store is not env-wrapped, so it cannot find its own
# QML modules or platform plugins. Derive both from the store paths.
if [[ "$qml_bin" == /nix/store/* ]]; then
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
fi

# Without this, Qt decides stderr has no console and silently swallows QML errors.
export QT_FORCE_STDERR_LOGGING="${QT_FORCE_STDERR_LOGGING:-1}"

if [[ "${1:-}" == "--grab" ]]; then
    export QT_QPA_PLATFORM="${QT_QPA_PLATFORM:-offscreen}"
fi

echo "qml runtime: $qml_bin ($("$qml_bin" --version 2>&1 | head -n1))" >&2

exec "$qml_bin" "$here/Main.qml" -- "$@"
