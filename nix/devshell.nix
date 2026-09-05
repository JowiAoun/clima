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

let
  # The Qt derivations, named once. Nix splits Qt across one store path per
  # module, so "where are Qt's plugins" has as many answers as there are
  # modules here — and the shell hook below turns this list into that answer
  # rather than letting scripts/qt-env.sh guess it back out of `ldd`, which can
  # only ever find the two modules a binary happens to link.
  qtModules = with pkgs; [
    qt6.qtbase
    qt6.qtdeclarative
    qt6.qtsvg
    qt6.qtshadertools
    qt6.qtpositioning

    # The Wayland platform plugin. Not needed to build anything and needed to
    # run one thing: `clima-widget --pin`, which asks a compositor for a
    # desktop-layer surface and can only do that over Wayland. Without this
    # module Qt has no `wayland` platform plugin at all and the layer-shell
    # path could be compiled here and never once executed — which is the exact
    # reason packaging/plasma/README.md gave for not building it.
    qt6.qtwayland
  ];
in

pkgs.mkShell {
  name = "clima-dev";

  # Grouped by what they are for. An alphabetical list would sort nicely and
  # stop telling you why anything is in it, and this list only grows.
  packages = with pkgs; qtModules ++ [
    # Qt 6 is in qtModules above, because the shell hook needs the list to
    # build QT_PLUGIN_PATH. qtshadertools is not optional even though nothing
    # calls it directly: qtdeclarative's own build needs it, and `qsb` is how
    # any ShaderEffect we write gets compiled ahead of time. qtpositioning is
    # "use my location" via GeoClue2 — optional at configure time, since
    # libclima/CMakeLists.txt compiles the feature out when it is absent and a
    # packager is entitled to leave it out, but present here so the code path
    # is actually built somewhere. A feature nobody in CI compiles is a feature
    # that stops compiling.

    # Desktop widgets on KDE and on every wlroots compositor. This is the
    # `zwlr_layer_shell_v1` client half: it turns clima-widget's window into a
    # surface the compositor pins to a layer of the desktop, which is what
    # GNOME needs a whole shell extension to do. Optional at configure time in
    # exactly the same way — widgets/CMakeLists.txt compiles --pin out when it
    # is missing — and, like qtpositioning, present here so that "optional"
    # does not mean "never built".
    kdePackages.layer-shell-qt

    # wayland-client, for the one question that has to be answered before
    # anything else: does the compositor we are talking to implement
    # zwlr_layer_shell_v1 at all? See widgets/layershell.cpp.
    wayland

    # dbus-run-session, for the tests that own a well-known name — the
    # location portal's, in tst_portallocator — and must therefore not run on
    # the developer's own bus. tests/CMakeLists.txt registers those tests only
    # where this is found.
    dbus

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

    # The workflows, which are the one part of CI that CI cannot check before
    # it runs. actionlint parses them, verifies every `uses:` and `runs-on:`
    # against the real schema, and pipes each `run:` block through shellcheck —
    # so a typo in the release workflow is caught here rather than by tagging
    # a release and watching it fail.
    actionlint

    # A compositor to prove the desktop-layer path against, because "it
    # compiles" is not evidence that a surface was ever created.
    #
    # sway is here rather than KWin for one reason: it runs headless.
    # `WLR_BACKENDS=headless sway` stands up a wlroots compositor with a
    # virtual output, no GPU and no seat, and wlroots is the reference
    # implementation of the protocol KWin also speaks — so a layer surface that
    # sway accepts is one Plasma, Hyprland, Wayfire and river accept too.
    # scripts/check-layer-shell.sh drives it; `sway -d` logs each layer surface
    # with its namespace, layer, anchor and margins, which is the assertion.
    #
    # grim photographs the result and wayland-info lists what the compositor
    # advertises — the two things that turn "it did not crash" into a check.
    sway
    grim
    wayland-utils

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

    # The icon rasteriser. The master is one SVG and the hicolor theme wants
    # eight PNG sizes, so the sizes are generated rather than drawn — see
    # scripts/icons.sh, which also re-renders and diffs them in CI so a hand-
    # edited PNG cannot survive. librsvg rather than ImageMagick because the
    # output has to be byte-reproducible: a flake-pinned librsvg renders the
    # same bytes on every machine, and `convert` delegates to whatever it found
    # at build time.
    librsvg
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

    # Where Qt's plugins and QML modules are, said by the thing that knows.
    #
    # scripts/qt-env.sh can work this out on its own and does, for a bare
    # terminal — but only from `ldd qml`, which finds qtbase and qtdeclarative
    # because those are the two a `qml` binary links. Every other Nix Qt module
    # is a separate store path that nothing links, so nothing points at it:
    # qtsvg's image formats and qtwayland's *platform plugin* were both absent
    # from QT_PLUGIN_PATH here, and a missing platform plugin is not a
    # degraded run, it is `could not load the Qt platform plugin "wayland"`.
    #
    # Set before qt-env.sh is sourced, because every export in there honours a
    # value that is already set. So this wins where it knows better and that
    # file still answers for everyone outside this shell.
    export QT_PLUGIN_PATH="${
      pkgs.lib.makeSearchPath "lib/qt-6/plugins" qtModules
    }''${QT_PLUGIN_PATH:+:$QT_PLUGIN_PATH}"
    export QML_IMPORT_PATH="${
      pkgs.lib.makeSearchPath "lib/qt-6/qml" qtModules
    }''${QML_IMPORT_PATH:+:$QML_IMPORT_PATH}"
    export QML2_IMPORT_PATH="$QML_IMPORT_PATH"

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
