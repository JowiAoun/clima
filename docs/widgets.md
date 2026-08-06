<!--
SPDX-FileCopyrightText: 2026 Jowi Aoun
SPDX-License-Identifier: CC-BY-SA-4.0
-->

# Desktop widgets

How a Clima widget reaches a desktop, and what had to be measured before any of
it could be designed.

Nothing in this document is a plan for a feature that might work. The
mechanism below was run on a real GNOME Shell before the first line of widget
code was written, because the whole Ubuntu story rests on one API behaving in a
way that is not obvious, and finding out afterwards would have been expensive.

---

## The constraint everything follows from

**GNOME Shell cannot draw a QML surface.** An extension is GJS running inside
gnome-shell's own process; extensions.gnome.org forbids shipping binaries; and
mutter still does not implement `wlr-layer-shell`, so there is no protocol by
which an outside process can ask to be a desktop layer.

So a Clima widget is **not drawn by the extension**. It is our own Qt process,
whose window the extension adopts and pins. That is the DING pattern — the same
one Ubuntu's own desktop-icons extension uses, which is running on the machine
these measurements were taken on.

The extension draws no weather at all. It spawns, adopts, stacks, and persists
geometry. Everything a user looks at is QML, which is why the widgets can share
components with the app instead of being a second implementation.

---

## One process fetches; the rest draw

`clima-daemon` owns the network, the cache and the clock. Everything else —
widgets, the tray, eventually the app — reads from it over the session bus.

Three reasons, in the order they bite. **SQLite has one writer**, and a desktop
with six widgets is eight processes writing one database. **The free tier is
per-client**, so eight processes each honouring a 15-minute TTL is eight times
the requests for one desktop's worth of weather, which is the difference
between a good citizen and a scraper (R5). And **the alert poll has to be in
one place** — `docs/04-architecture.md` §4.5 budgets it at ~264 KB a day on the
assumption that there is one of it, and eight independent pollers is eight
tombstone state machines that can disagree about whether a warning was
cancelled.

```
io.github.JowiAoun.Clima.Daemon   /io/github/JowiAoun/Clima/Daemon
io.github.JowiAoun.Clima.Daemon1
```

| Call | Does |
|---|---|
| `SchemaVersion() → i` | The version of the JSON. Check it before trusting anything else. |
| `GetSnapshot(placeId, fields, hours, days) → s` | One masked snapshot, now |
| `Subscribe(placeId, fields, hours, days) → s` | The same, kept. Returns a token |
| `Unsubscribe(token) → b` | |
| `RequestRefresh(placeId)` | Ask now rather than at the next poll |
| `ListWidgets() → s` | `widgets/catalogue.json`, verbatim |
| `ListPlaces() → as` | Canonical place ids |
| `SnapshotChanged(token, json)` | signal |

**The token is the signal's first argument on purpose.** A D-Bus signal is a
broadcast, so without it every widget is woken — and made to parse a snapshot —
every time any other widget refreshes. A reader adds a match rule with
`arg0='<its token>'` and the bus filters before delivery. That is what keeps
the ~0 % idle CPU line in `docs/03-tech-stack.md` §3.4 true on a desktop full
of tiles.

The payload is JSON rather than a typed D-Bus signature, for the version-skew
reason in the section below: the two ends ship from different places on
different clocks, and an unknown key has to be ignorable rather than an
unmarshalling error. `libclima/wire/snapshot.h` argues it at length.

The field mask is what makes that affordable. A wind rose asks for three
current readings; it is not sent 408 hourly points.

```sh
clima-daemon --print-address
clima-daemon --fixture toronto     # recorded data at a frozen clock
```

## What was measured, and on what

| | |
|---|---|
| GNOME Shell | 46.0 |
| mutter typelib | `Meta-14` |
| gjs | 1.80.2 |
| Session | Ubuntu, Wayland |
| Harness | `scripts/shell-probe.sh` + `tests/shell/clima-window-probe@clima.invalid/` |

The probe spawns a target through `Meta.WaylandClient`, waits for a window, and
reports whether it can be adopted. It runs a **nested** shell — an ordinary
Wayland client of the live session — rather than `--headless`, which runs as a
display server and could take the seat out from under the session you are
testing from.

### The verdict, for the Flatpak-installed app

```json
{"owns_window": true, "make_dock": true, "hide_from_window_list": true,
 "lower": true, "window_type": 2, "in_tab_list": false,
 "in_window_actors": true, "on_all_workspaces": true, "rect": "734x568+66+32"}
```

Window type 2 is `Meta.WindowType.DOCK`. The window is out of alt-tab
(`in_tab_list: false`) and still composited (`in_window_actors: true`) — hidden
from the window list rather than from the user.

The run carries its own control: DING's window, mapped in the same shell by a
*different* `MetaWaylandClient`, reported `owns_window: false`. The call
discriminates; it is not returning true for everything on screen.

---

## Four things this corrected

### 1. D-Bus activation is the wrong mechanism, not a workable one

The question this spike was written to answer was *"can a host-side GNOME
extension D-Bus-activate a name owned by a Flatpak-installed app?"* It can —
and it must not.

`MetaWaylandClient` identity is established by an inherited socket fd:
`meta_wayland_client_spawnv()` makes a socketpair, keeps the server end, and
hands the child the other end as `WAYLAND_SOCKET`. A **bus-activated process is
spawned by `dbus-daemon`**, so it connects to the compositor the ordinary way
and no `MetaWaylandClient` ever owns it. `owns_window()` would return false and
the window could never be adopted, re-typed or hidden.

**The extension must spawn the widget host itself.** D-Bus is how the widget
host then talks to the daemon — it is not how the widget host gets started.

### 2. A Flatpak install survives the spawn, which was the real risk

`flatpak run` execs `bwrap`. If bwrap had closed the inherited fd or filtered
the environment, the DING pattern would have been available to the `.deb` and
not to the Flatpak — and the Flatpak is the Ubuntu 24.04 story, so that would
have taken the whole plan with it.

Both halves were measured separately before the full run:

- an fd opened by the parent is still open inside the sandbox, at the same
  number;
- `WAYLAND_SOCKET` passes through flatpak's environment filter unchanged.

And `wl_display_connect()` prefers `WAYLAND_SOCKET` over `WAYLAND_DISPLAY`, so
the app uses the compositor's fd rather than the socket flatpak bind-mounts.

The host binary case is strictly easier — no bwrap, no env filter — so it is
covered a fortiori and was not run separately.

### 3. `make_dock()` replaces DING's title-parsing hack

DING encodes its flags **in the window title** — `@!B` for bottom, `D` for all
desktops, `H` to hide from the window list — and re-parses the title on every
change. That is not how it would be written today; it predates the API.

mutter 14 exposes exactly six methods on `MetaWaylandClient`:

```
hide_from_window_list  make_desktop  make_dock  owns_window  show_in_window_list  spawnv
```

`make_dock()` gets `on_all_workspaces` and exclusion from the overview by
construction, which is most of what the title flags were emulating. Clima's
extension will use it when it is written, and nothing we send a widget will
travel through a window title.

### 4. `get_sandboxed_app_id()` returns null here, so it cannot identify us

A window from a Flatpak app is normally identifiable by its sandbox id. For a
process spawned through `MetaWaylandClient` it came back **null** — the client
connected on the inherited fd, so no security context was attached to it.

This costs nothing, because `owns_window()` is the right question anyway and is
the one we ask. It is written down because "match on the sandboxed app id" is a
reasonable-looking idea that does not work, and someone will otherwise spend an
afternoon finding that out.

---

## What still ships separately

**gnome-shell will not load an extension from inside a Flatpak.** Extensions
live in `~/.local/share/gnome-shell/extensions`, and the app has no
`--filesystem=home` — deliberately.

So the extension is published to extensions.gnome.org on its own, with its own
`shell-version` compatibility list, and updates on a different clock from the
app. Two consequences, both of which are design constraints rather than
annoyances:

- **the D-Bus interface between them is versioned**, and the daemon keeps
  answering an older field mask than the one it would choose today;
- **the extension degrades to nothing** when the app is absent. It must not
  error, block the shell, or leave a broken tile — a user who removes the
  Flatpak should see the widgets disappear, not a stack trace in their journal.

---

## Running the probe

```sh
scripts/shell-probe.sh flatpak      # the Flatpak-installed app
scripts/shell-probe.sh host         # build/dev/app/clima from this tree
scripts/shell-probe.sh -- CMD...    # anything else
```

It is **not in CI**, and cannot usefully be: it needs a GNOME Shell to nest
inside, and standing one up on a runner would test that stack rather than the
one users have. It is a manual acceptance test whose answer is recorded above,
so that nobody has to re-run it to know what it said.

The assertion block is exercised in the ordinary way — against the verdict the
real run produced, and against three injected defects (`make_dock` silently
doing nothing, the window still in alt-tab, the window no longer composited),
each of which fails it. The script end-to-end has been run in the form
described here; the committed copy is the same harness with its output
normalised to booleans.

---

## What exists today

**Built and verified.** The mechanism above, measured on a real shell. The wire
format and its field mask, with the three encoder rules under test. The widget
catalogue, with a test that asserts every field a widget declares is one the
encoder actually emits. `clima-daemon`, exercised end to end on a private
session bus against a fixture — introspection, a masked `GetSnapshot`, a
`Subscribe` that delivers its own token, `Unsubscribe`.

**Not built.** Everything that draws:

- the widget host and the ten widgets in the catalogue — the catalogue
  describes them, and nothing renders them yet;
- the GNOME extension that spawns the host and adopts its window;
- the Plasma 6 applet, which is the cheapest of the three because a Plasma
  applet *is* QML and the widget files go into `contents/ui/` nearly verbatim;
- the SNI tray;
- the link-line guard that keeps a widget process from linking the providers.
  libclima is a static archive, so the check that matters is a symbol check on
  the built binary rather than `ldd` — a widget that never calls `HttpClient`
  does not carry it, and asserting that is stronger than asserting a shared
  object is absent.

The daemon is additive. The app does not link it, does not know about it, and
behaves exactly as it did before this existed when nothing is running.
