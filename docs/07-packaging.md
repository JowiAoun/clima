<!-- SPDX-FileCopyrightText: 2026 Jowi Aoun -->
<!-- SPDX-License-Identifier: CC-BY-SA-4.0 -->

# 07 — Packaging, Distribution and CI

Distribution *is* a feature. An app that only exists as a `git clone` will never displace
MSN Weather, and on Linux specifically the install story determines whether people ever try
it. Two rules follow from LGPLv3 (§3.1): **link Qt dynamically everywhere**, and **ship the
corresponding Qt source or a written offer from infrastructure we control**.

## 7.1 Channel matrix

| Platform | Channel | Priority | Qt source | Notes |
|---|---|---|---|---|
| **Linux** | **Flathub** | **P0** | `org.kde.Platform` runtime, branch **6.11** | Primary channel. We control the Qt version, so users get modern Qt regardless of distro. `org.kde.Sdk` to build. **There is no 6.8 branch** — see §7.6. |
| Linux | AppImage | P1 | Bundled, dynamic, with relink info | For distros/users that avoid Flatpak; must bundle Qt + relink instructions. **Never built** — `docs/known-gaps.md`. |
| Linux | AUR (`clima`, `clima-git`) | P1 | System Qt | Arch users are early adopters and vocal |
| Linux | Fedora COPR → Fedora | P2 | System Qt | Needs a `.spec`; system-library build (D8) makes this feasible |
| Linux | Debian/Ubuntu PPA → Debian | P2 | System Qt | Debian 13 ships Qt 6.8, which is why 6.8 is our floor |
| Linux | openSUSE OBS, nixpkgs | P3 | System Qt | Community-driven |
| Linux | Snap | P3 | `qt6` content snap | Only if demand appears; Flatpak first |
| **Windows** | **winget** + **unsigned WiX v5 MSI** | **P0** | Bundled via `windeployqt` | `windeployqt` copies compiler runtime by default. **Not MSIX** — see §7.6. Per-user install, no admin. SmartScreen warns; `docs/known-gaps.md` has the mitigations. |
| Windows | Portable ZIP | P1 | Bundled | For no-installer users. Built from the same staged tree as the MSI, so both carry the source offer. |
| Windows | Microsoft Store | P3 | Bundled | Only worth it if GPLv3 terms are compatible — verify |
| Windows | Chocolatey / Scoop | P3 | — | Community |
| **macOS** | **Notarised DMG** | **not shipped** | Bundled via `macdeployqt` | Needs an Apple Developer ID at $99/yr, which we do not have. Without notarisation Gatekeeper *refuses* rather than warns, so a DMG would be undeliverable. `docs/known-gaps.md`. |
| macOS | Homebrew cask | — | Bundled | Follows the DMG, so also not shipped. |
| macOS | Mac App Store | ❌ | — | GPLv3 conflicts with App Store terms (D6) |
| Android / iOS | — | post-1.0 | — | iOS blocked for a GPLv3 build; MPL-2.0 engine keeps a path open |

## 7.2 Linux specifics

- **App ID** must be a domain or forge namespace we control, and it is baked into the
  desktop file, D-Bus name, AppStream ID, Flatpak ID and settings path. Changing it later
  breaks users' saved data. See [`08-risks.md`](08-risks.md) Q3.
- **AppStream metainfo** (`<id>`, screenshots, `<content_rating>`, release notes) is required
  by Flathub and used by every software centre. Write it in M1, not M7.
- **Wayland-first**: test on Wayland (KWin + Mutter) *and* X11 every milestone. Client-side
  decorations, fractional scaling, and the screencast/portal paths are where Qt apps
  typically break.
- **Portals**, not raw APIs: `xdg-desktop-portal` for notifications, background running,
  autostart, location, and file save (export). Flatpak sandbox stays tight —
  `--share=network`, `--socket=wayland`, `--socket=fallback-x11`, and *no* `--filesystem=host`.
- **Tray**: StatusNotifierItem is the only thing that works across Plasma and
  appindicator-based shells; plain XEmbed trays are dead.
- **Location needs the portal, and Qt Positioning does not use it.** ⚠️ "Use my location"
  goes through `QGeoPositionInfoSource`, whose Linux backend talks to GeoClue2 on the
  session bus — which is not reachable from inside a Flatpak sandbox. The route in a
  sandbox is `org.freedesktop.portal.Location`, and the user gets a portal prompt on first
  use. ⚠️ **Correction:** this used to say the manifest needs
  `--talk-name=org.freedesktop.portal.Desktop`. It does not. Flatpak's default policy
  already permits `org.freedesktop.portal.*`, so declaring it states a permission that was
  granted anyway — verified by `SystemAppearance` reading the colour scheme over the
  portal inside the sandbox today with nothing declared. `packaging/flatpak/` carries no
  `--talk-name` at all. Until that is done, `clima-geocode`'s locator reports `Failure::Unavailable` under
  Flatpak and the app falls back to manual search, which is a degraded feature and not a
  broken one — see `libclima/places/devicelocator.h`. Nothing else needs location:
  *reverse* geocoding is offline and bundled, so turning a coordinate into "Toronto,
  Ontario" never leaves the sandbox.

## 7.3 CI matrix

| Job | Runner | Qt | Purpose |
|---|---|---|---|
| `linux-system-qt` | **debian:trixie** | distro Qt 6.8 | Proves the packager build path works. Not `ubuntu-24.04`, which ships Qt 6.4.2 and cannot satisfy the floor — see §7.6. |
| `linux-aqt` | ubuntu-24.04 | aqtinstall, min (6.8) + latest | Version-floor and version-ceiling coverage |
| `linux-flatpak` | ubuntu-24.04 | `org.kde.Sdk` | Builds the actual shipped artefact |
| `windows-msvc` | windows-latest | aqtinstall | MSVC + `windeployqt` |
| `macos-arm64` | macos-latest | aqtinstall | `macdeployqt` + codesign |
| `tests` | linux | min Qt | `ctest`, golden-file providers, golden-image charts, **no network** |
| `qml-tests` | linux (offscreen) | min Qt | `qmltestrunner` |
| `perf-budget` | linux | min Qt | Asserts startup / RSS / frame-time budgets from §3.4 |
| `lint` | linux | — | `clang-format`, `clang-tidy`, `reuse lint`, AppStream validate, `qmllint` |
| `release` | all three | — | Tag-triggered; builds every artefact, generates the third-party licence bundle, attaches the Qt source offer |

Tooling: `jurplel/install-qt-action` / `aqtinstall` for Qt; `flatpak-builder` in a container;
`macdeployqt` with the code-signing fix for nested frameworks (the upstream tool has known
gaps with nested framework signing — use a patched fork or post-process with
`macdeployqtfix`).

## 7.4 Release engineering

- **SemVer**, tags `vX.Y.Z`, releases cut from `main` only.
- Conventional-commit history → generated changelog and AppStream release notes.
- Every release ships: source tarball, SBOM, third-party licence bundle, and the
  **written offer for Qt's corresponding source** (LGPLv3 obligation — a link to qt.io is
  explicitly *not* sufficient).
- Reproducible-build check on the Linux artefact.
- Update checking: **off by default**; when enabled it queries a static JSON on our own
  domain and never sends identifying information.

## 7.5 Launch checklist

- [ ] Flathub submission accepted (needs verified app ID, AppStream, screenshots)
- [ ] `winget` manifest merged; MSIX signed
- [ ] DMG notarised; Homebrew cask merged
- [ ] AUR package + `-git` variant
- [ ] Docs site with an honest "known gaps" page (satellite, pollen outside Europe,
      alerts coverage outside US/EU/CA)
- [ ] `About → Data sources` verified against every provider's attribution requirement
- [ ] Screenshots and a 30-second screen recording per platform, light and dark
- [ ] Posts prepared: r/linux, r/kde, r/gnome, Hacker News, OMG!Ubuntu, LinuxLinks,
      Phoronix, KDE and GNOME planet blogs
- [ ] Issue templates, `CONTRIBUTING.md`, DCO bot, translation project live

## 7.6 Corrections, measured 2026-08-05

This section is the plan meeting the tools. Each entry is something §7.1–§7.3
asserted that turned out not to hold, with what was measured rather than what
was reasoned.

**`org.kde.Platform//6.8` does not exist.** §7.1 said "Qt ≥ 6.8" and the build
plan said that branch outright. Flathub publishes `5.15-25.08`, `6.9`, `6.10`
and `6.11`; KDE retires runtime branches as they age, so a manifest pinned to
our *compile* floor would not build at all. The manifest uses **6.11**, and not
because newest is best — it is the Qt the flake pins, so the Flatpak is built
against the same Qt every test and all 46 golden images were produced against.
This number needs looking at on each KDE runtime release. When it goes stale
the Flathub build stops, which is the good kind of failure.

**A scalable icon cannot go in a Flatpak.** `flatpak-builder` validates every
icon it exports through `gdk-pixbuf`, and librsvg removed its `gdk-pixbuf`
loader in 2.58 — so an SVG comes back "Format not recognized" and the export
fails *after everything has compiled*. This is not a toolchain quirk on one
machine: the host's own `/usr/libexec/flatpak-validate-icon` rejects the file
and accepts the 512 px PNG beside it. The manifest cleans
`/share/icons/hicolor/scalable` out. The `.deb` still ships it.

**Signed MSIX is not achievable, and MSI is better anyway.** §7.1's P0 is
corrected to an unsigned WiX v5 MSI. The reasoning is in
`docs/known-gaps.md` — the short version is that an unsigned MSIX cannot be
side-loaded without the user importing a certificate into their trusted root
store, and MSI additionally gets native winget validation, per-user install
without admin, and a real upgrade code.

**`linux-system-qt` cannot run on `ubuntu-24.04`.** §7.3's matrix put the
packager-build job there; 24.04 ships Qt 6.4.2 against a 6.8 floor, so the job
as written could never have passed. It runs in `debian:trixie`, which ships
6.8 — which is also why 6.8 is the floor.

**The portals need no `--talk-name`.** See §7.2.

**Reproducibility comes from `flake.lock`, not a container.** §7.3 and the
build plan both assumed a digest-pinned `debian:trixie` image for golden
images. The repository had something stronger already: a flake revision pins
Qt, FreeType and fontconfig down to the store hash, where a `trixie` tag is a
moving target. See `scripts/golden.sh`, which explains it at length.

## Sources

- [Flatpak: Qt](https://docs.flatpak.org/en/latest/qt.html) · [Available runtimes](https://docs.flatpak.org/en/latest/available-runtimes.html) · [KDE flatpak runtime](https://github.com/KDE/flatpak-kde-runtime) · [KDE: Your first Flatpak](https://develop.kde.org/docs/flatpak/packaging/) · [KDE Flatpak integration](https://develop.kde.org/docs/packaging/flatpak/integration/)
- [Qt for Windows — Deployment](https://doc.qt.io/qt-6/windows-deployment.html)
- [install-qt-action](https://github.com/marketplace/actions/install-qt) · [aqtinstall](https://github.com/miurahr/aqtinstall) · [macdeployqt fork with nested-framework signing fix](https://github.com/GDATASoftwareAG/macdeployqt)
- [Qt LGPL obligations](https://www.qt.io/development/open-source-lgpl-obligations)
