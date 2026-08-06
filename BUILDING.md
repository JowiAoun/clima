<!-- SPDX-FileCopyrightText: 2026 Clima contributors -->
<!-- SPDX-License-Identifier: CC-BY-SA-4.0 -->

# Building Clima

Two supported routes: the Nix devshell, which pins everything, and your
distribution's Qt, which is what a packager uses.

## With Nix (recommended)

```sh
nix develop --command cmake --preset dev
nix develop --command cmake --build build/dev
nix develop --command ctest --test-dir build/dev --output-on-failure
```

Or skip the ceremony — this builds if needed, finds Qt itself, and works from a
plain shell:

```sh
scripts/dev-run.sh
```

The flake pins nixpkgs by revision, so the Qt, compiler, FreeType and fontconfig
you get are the ones every golden image was recorded against, down to the store
hash. That is why golden images reproduce here and not in a container tagged
`latest`.

> **Running the binary directly needs the devshell.** `./build/dev/app/clima`
> from a plain shell exits 255 with no output: Qt is only in the Nix store, so
> the binary cannot find its QML imports. Either use `scripts/dev-run.sh` or
> prefix with `nix develop --command`.

## With your distribution's Qt

The floor is **Qt 6.8**. No dependency is fetched at configure time — decision
D8 rules out Conan and vcpkg precisely so this path works.

**Debian 13 / Ubuntu 26.04+**

```sh
sudo apt install build-essential cmake ninja-build pkg-config \
  qt6-base-dev qt6-declarative-dev qt6-declarative-dev-tools \
  libqt6sql6-sqlite libgl1-mesa-dev
```

**Fedora 40+**

```sh
sudo dnf install gcc-c++ cmake ninja-build \
  qt6-qtbase-devel qt6-qtdeclarative-devel qt6-qtpositioning-devel
```

**Arch**

```sh
sudo pacman -S base-devel cmake ninja qt6-base qt6-declarative qt6-positioning
```

Then:

```sh
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build
sudo cmake --install build
```

**Ubuntu 24.04 cannot build this.** It ships Qt 6.4.2. Use the Flatpak, which
carries its own Qt — see [`docs/known-gaps.md`](docs/known-gaps.md).

## Options

| Option | Default | What it does |
|---|---|---|
| `CLIMA_BUILD_TESTS` | `ON` | The test suite. `OFF` for a package build. |
| `CLIMA_BUILD_GALLERY` | `ON` | The `clima-gallery` component browser, a second binary. |
| `CLIMA_DEV_TOOLS` | `ON` | `--grab`, `--film`, `--poke` and the probe harnesses. `OFF` in shipped builds. |
| `CLIMA_APP_ID` | `io.github.JowiAoun.Clima` | Reverse-DNS id: desktop file, AppStream, icon name, settings path. |
| `CLIMA_CONTACT` | the issue tracker | Goes in the outbound User-Agent. **Packagers should override this** — a rate-limit complaint about your rebuild should reach you. |
| `CLIMA_MAINTAINER` | a noreply address | The Debian `Maintainer` field. |

Two optional Qt modules are found if present and compiled out if not:
**Qt Positioning** (the "use my location" button; without it the user searches
by name) and **Qt D-Bus** (reads the desktop's colour scheme over the XDG
portal; without it `QStyleHints` answers instead).

## Packaging

```sh
scripts/deb.sh inspect     # a .deb, built in debian:trixie through docker
scripts/flatpak.sh deps    # once: the KDE runtime and SDK
scripts/flatpak.sh build   # build and install it into your user flatpak
scripts/flatpak.sh run
```

The `.deb` is built in a container on purpose. `dpkg-shlibdeps` derives the
`Depends` field from the binary's `DT_NEEDED` entries, so building it against a
Nix Qt produces a package that depends on nothing, installs on a machine with no
Qt, and then does not start — a failure invisible on the machine that made it.

`flatpak-builder` needs `appstreamcli compose`, which lives in a separate binary
many distributions do not ship. Without installing anything:

```sh
nix shell nixpkgs#flatpak-builder nixpkgs#appstream -c scripts/flatpak.sh build
```

[`docs/releasing.md`](docs/releasing.md) covers the rest of the pipeline.

## Troubleshooting

**"Type X is not a type" naming a file you did not touch.** The QML module lists
its files explicitly and never globs, so a new `.qml` is missing from
`app/CMakeLists.txt`. Run `scripts/check-qml-files.sh`, which says which one.

**A golden image fails on a fresh machine.** Look at `tst_environment` first. It
is a canary that asserts font advance widths, so rasterisation drift is reported
as one sentence about the machine instead of forty picture diffs. Golden images
are only meaningful under `nix develop`; `ctest -LE golden` skips them.

**The app starts and shows nothing.** Check stderr. If it is empty, you are
probably outside the devshell — see the note at the top.
