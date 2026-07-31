# SPDX-FileCopyrightText: 2026 Jowi Aoun
# SPDX-License-Identifier: GPL-3.0-or-later
#
# The development shell, as a plain function of a nixpkgs.
#
# It lives here instead of inside flake.nix so that `nix develop` and
# `nix-shell` cannot drift apart: both call this file, so there is one list of
# tools and one shell hook, not two that agree until the day they do not.
# flake.nix chooses the nixpkgs; shell.nix reads that same choice back out of
# flake.lock.

{ pkgs }:

pkgs.mkShell {
  name = "clima-dev";

  # Grouped by what they are for. An alphabetical list would sort nicely and
  # stop telling you why anything is in it, and this list only grows.
  packages = with pkgs; [
    # Qt 6. qtshadertools is not optional even though nothing calls it directly:
    # qtdeclarative's own build needs it, and `qsb` is how any ShaderEffect we
    # write gets compiled ahead of time.
    qt6.qtbase
    qt6.qtdeclarative
    qt6.qtsvg
    qt6.qtshadertools

    # "Use my location", via GeoClue2 on Linux. Optional at configure time —
    # libclima/CMakeLists.txt compiles the feature out when this is absent, and
    # a packager is entitled to leave it out — but it is in the devshell so that
    # the code path is actually built here rather than only on somebody else's
    # machine. A feature nobody in CI compiles is a feature that stops
    # compiling.
    qt6.qtpositioning

    # The build. D8 in docs/03-tech-stack.md fixes the floor at CMake 3.21 and
    # the Qt6 CMake API; Ninja because qt_add_qml_module generates a lot of
    # small steps and make is slow at those.
    cmake
    ninja
    pkg-config
    gcc

    # What CI gates on, so it has to be here too — a lint you cannot run before
    # pushing is a lint that fails after pushing. reuse over every file,
    # clang-format and clang-tidy over C++, shellcheck and shfmt over scripts.
    clang-tools
    reuse
    shellcheck
    shfmt

    # tools/refcap is a Node/Playwright harness; film.sh tiles frames with
    # ffmpeg; jq reads the capture manifests.
    nodejs
    ffmpeg
    jq

    # Packaging metadata gets validated rather than eyeballed: appstreamcli
    # validate on the metainfo, desktop-file-validate on the .desktop. Both are
    # Flathub submission requirements — see docs/07-packaging.md.
    appstream
    desktop-file-utils
  ];

  shellHook = ''
    # On a GNOME session Qt picks its gtk3 platform theme, which initialises the
    # host GTK inside our process. A Nix-store Qt links its own GTK against a
    # different module path, so GNOME's gtk-modules (canberra) cannot be found
    # and GTK prints "Failed to load module canberra-gtk-module" on every
    # launch. The generic theme avoids loading host GTK at all.
    #
    # The rule is that this workaround applies to a Nix-store Qt only, never to
    # a distro one. In here that holds by construction: the only Qt on PATH is
    # the one this flake pins.
    export QT_QPA_PLATFORMTHEME=generic

    # Without this, Qt decides stderr has no console and silently swallows QML
    # errors — the app comes up blank and says nothing about why.
    export QT_FORCE_STDERR_LOGGING=1

    # Everything else about this Qt — QML_IMPORT_PATH, QT_PLUGIN_PATH, and the
    # CLIMA_QT_PREFIX that a CMake configure wants — comes from the same script
    # the prototype's run.sh uses, so the shell and a bare terminal cannot
    # disagree about which Qt is in play or how it is set up. Both exports above
    # survive it: every assignment in there honours a value already set.
    clima_root="$PWD"
    if command -v git > /dev/null 2>&1; then
      clima_root="$(git rev-parse --show-toplevel 2> /dev/null || printf '%s' "$PWD")"
    fi
    if [ -r "$clima_root/scripts/qt-env.sh" ]; then
      # shellcheck source=/dev/null
      . "$clima_root/scripts/qt-env.sh"
      clima_qt_env || echo "clima: scripts/qt-env.sh found no qml — the shell is still usable" >&2
    fi
    unset clima_root

    # Which Qt got picked matters only when something is wrong with the pick, so
    # saying so is opt-in — the same bargain run.sh makes with CLIMA_VERBOSE.
    if [ -n "''${CLIMA_VERBOSE:-}" ]; then
      echo "clima-dev: qml $CLIMA_QML_BIN" >&2
      echo "clima-dev: qt prefix $CLIMA_QT_PREFIX" >&2
    fi
  '';
}
