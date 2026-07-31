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
| **Linux** | **Flathub** | **P0** | `org.kde.Platform` runtime (Qt ≥ 6.8) | Primary channel. We control the Qt version, so users get modern Qt regardless of distro. `org.kde.Sdk` to build. |
| Linux | AppImage | P1 | Bundled, dynamic, with relink info | For distros/users that avoid Flatpak; must bundle Qt + relink instructions |
| Linux | AUR (`clima`, `clima-git`) | P1 | System Qt | Arch users are early adopters and vocal |
| Linux | Fedora COPR → Fedora | P2 | System Qt | Needs a `.spec`; system-library build (D8) makes this feasible |
| Linux | Debian/Ubuntu PPA → Debian | P2 | System Qt | Debian 13 ships Qt 6.8, which is why 6.8 is our floor |
| Linux | openSUSE OBS, nixpkgs | P3 | System Qt | Community-driven |
| Linux | Snap | P3 | `qt6` content snap | Only if demand appears; Flatpak first |
| **Windows** | **winget** + signed MSIX | **P0** | Bundled via `windeployqt` | `windeployqt` copies compiler runtime by default |
| Windows | Portable ZIP | P1 | Bundled | For no-installer users |
| Windows | Microsoft Store | P3 | Bundled | Only worth it if GPLv3 terms are compatible — verify |
| Windows | Chocolatey / Scoop | P3 | — | Community |
| **macOS** | **Notarised DMG** | **P0** | Bundled via `macdeployqt` | Needs Apple Developer ID ($99/yr) — see Q4 |
| macOS | Homebrew cask | P1 | Bundled | |
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
  sandbox is `org.freedesktop.portal.Location`, so the manifest needs
  `--talk-name=org.freedesktop.portal.Desktop` and the user gets a portal prompt on first
  use. Until that is done, `clima-geocode`'s locator reports `Failure::Unavailable` under
  Flatpak and the app falls back to manual search, which is a degraded feature and not a
  broken one — see `libclima/places/devicelocator.h`. Nothing else needs location:
  *reverse* geocoding is offline and bundled, so turning a coordinate into "Toronto,
  Ontario" never leaves the sandbox.

## 7.3 CI matrix

| Job | Runner | Qt | Purpose |
|---|---|---|---|
| `linux-system-qt` | ubuntu-24.04 | distro Qt 6 | Proves the packager build path works |
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

## Sources

- [Flatpak: Qt](https://docs.flatpak.org/en/latest/qt.html) · [Available runtimes](https://docs.flatpak.org/en/latest/available-runtimes.html) · [KDE flatpak runtime](https://github.com/KDE/flatpak-kde-runtime) · [KDE: Your first Flatpak](https://develop.kde.org/docs/flatpak/packaging/) · [KDE Flatpak integration](https://develop.kde.org/docs/packaging/flatpak/integration/)
- [Qt for Windows — Deployment](https://doc.qt.io/qt-6/windows-deployment.html)
- [install-qt-action](https://github.com/marketplace/actions/install-qt) · [aqtinstall](https://github.com/miurahr/aqtinstall) · [macdeployqt fork with nested-framework signing fix](https://github.com/GDATASoftwareAG/macdeployqt)
- [Qt LGPL obligations](https://www.qt.io/development/open-source-lgpl-obligations)
