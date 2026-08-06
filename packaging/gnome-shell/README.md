<!-- SPDX-FileCopyrightText: 2026 Jowi Aoun -->
<!-- SPDX-License-Identifier: CC-BY-SA-4.0 -->

# The GNOME Shell extension

`clima@JowiAoun.github.io` puts Clima's tiles on the desktop and the current temperature in the
top bar. It is about 500 lines of GJS and **it draws no weather**.

## What it actually does

GNOME Shell cannot host a Qt Quick surface — an extension is GJS running inside gnome-shell's own
process, extensions.gnome.org forbids shipping binaries, and mutter does not implement
`wlr-layer-shell`. So the extension launches `clima-widget`, which is our own Qt process, adopts
the window that appears, types it as a dock and pins it below everything else.

That is the DING pattern. It was measured on GNOME Shell 46 before any of this was written, and
`docs/widgets.md` in the main repository has the verdict, the method and the four things the
measurement corrected.

```
extension  ──spawn──▶  clima-widget  ──D-Bus──▶  clima-daemon  ──HTTPS──▶  Open-Meteo
   (GJS)                  (Qt/QML)                (one fetch)
```

## It ships separately, and that is not an oversight

gnome-shell will not load an extension from inside a Flatpak: extensions live in
`~/.local/share/gnome-shell/extensions`, and the app has no `--filesystem=home` — deliberately.

So this directory is published to extensions.gnome.org on its own, with its own `shell-version`
list, and it updates on a different clock from the app. Two consequences:

- **the D-Bus interface between them is versioned.** `SchemaVersion()` is checked before anything
  else is trusted, and a mismatch shows a line of text rather than a guess drawn from a shape
  neither side understands.
- **it degrades to nothing when the app is absent.** No tiles, one line in the journal, and a
  preferences window that says why. A user who removes the Flatpak should see the widgets
  disappear, not a stack trace.

## The top bar will never look like the app

The indicator and its menu are drawn in St with the shell's own theme. They cannot use Clima's
typeface, its colour tokens or its charts, because none of that exists inside gnome-shell.

This is written down in three places — here, in `extension.js`, and in the preferences window —
because it is the first thing that looks like a bug and is not one. Making a St popup look like
Clima would mean reimplementing the design system in CSS and maintaining two of them.

## Wayland only

`Meta.WaylandClient` is what establishes that a window is ours: the shell makes a socketpair,
keeps one end, and hands the child the other as `WAYLAND_SOCKET`. `owns_window()` is then a
question about *that* `wl_client` — not about a title, and not about a sandbox id, which comes
back null for a client that connected on an inherited fd.

On X11 there is no such object. A window could be lowered but not verifiably adopted, and one the
user can raise by clicking is not a desktop widget. The extension says what it needs in the
journal and stops. GNOME has defaulted to Wayland since 3.34 and Ubuntu since 21.04.

## Installing it from this repository

```sh
ln -s "$PWD/packaging/gnome-shell/clima@JowiAoun.github.io" \
      ~/.local/share/gnome-shell/extensions/

glib-compile-schemas ~/.local/share/gnome-shell/extensions/clima@JowiAoun.github.io/schemas/

# Wayland cannot restart the shell in place, so: log out, log back in.
gnome-extensions enable clima@JowiAoun.github.io
```

For development against a build tree rather than an installed app:

```sh
CLIMA_WIDGET=$PWD/build/dev/widgets/clima-widget gnome-extensions enable clima@JowiAoun.github.io
```

`CLIMA_WIDGET` has to be in gnome-shell's own environment, not in your terminal's — the extension
runs inside the shell. `systemctl --user set-environment` before logging in, or use the nested
shell that `scripts/shell-probe.sh` stands up.

## What has been measured and what has not

| | |
|---|---|
| The adoption mechanism | Measured on GNOME Shell 46, Ubuntu, Wayland — `docs/widgets.md` |
| A Flatpak-installed target surviving `bwrap` | Measured, both halves separately |
| `make_dock`, `hide_from_window_list`, `lower` | Measured; window type 2, out of alt-tab, still composited |
| This extension end to end | **Not measured on a live session.** It is the probe's mechanism with placement, respawn and an indicator around it. |
| Shell versions 45, 47, 48 | **Declared, not tested.** Only 46 was run. |

The last two rows are why the `shell-version` list is a claim to re-check before the first upload
to extensions.gnome.org, and why nothing in CI reports on this directory: standing up a GNOME
Shell on a runner would test that stack rather than the one users have.
