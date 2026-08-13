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

There are two readers today and they exercise the interface differently, which
is worth more than one of them exercising it twice. `clima-widget` holds one
subscription per tile and adds an **arg0 match rule** for each token, so the bus
filters before delivery and one tile's refresh does not wake the other seven.
The GNOME extension's panel indicator holds a single subscription and filters in
its callback, because GJS's proxy wrapper exposes no argument matching — which
costs one wakeup per other subscriber and, for one indicator, is nothing.

```sh
clima-daemon --print-address
clima-daemon --fixture toronto           # recorded data at a frozen clock
clima-daemon --fixture toronto --dump-snapshot   # one snapshot, no bus at all
```

### What starts it

Three things, and the order is how little each asks of the user.

| | |
|---|---|
| **The bus** | `packaging/linux/clima-daemon.service.in` makes it activatable, so the first widget host or extension that looks for the name gets one started for it. Works from a Flatpak, on any desktop, without a login. |
| **An autostart entry** | `/etc/xdg/autostart`, where a package can write one. Login-time, which is what a pinned tile wants: a reading that is already current when the desktop appears. |
| **By hand** | `clima-daemon`. |

All three are idempotent — whichever loses the race finds the name owned and
exits 5 — and none of them is a fallback for the others.

**Activation is new, and it reverses a decision this document made.** Finding 1
below rules out D-Bus activation for the *widget host*, because a bus-activated
process is spawned by `dbus-daemon` and gnome-shell can therefore never own its
Wayland client. That was measured and it stands. What went wrong is that the
same conclusion was written into the daemon, which has no window, no Wayland
connection and nothing for a shell to adopt — and the cost only appeared once
`--pin` put tiles on compositors where there is no extension to start anything.
On a Flatpak install on KDE, nothing started the daemon, ever.

That last one is how `tests/fixtures/wire/` is recorded, and it goes through the
same encoder the bus does — a recorded fixture produced any other way would
drift from what the daemon actually sends, which is the whole of what makes the
recording worth having.

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
extension uses it — `packaging/gnome-shell/clima@JowiAoun.github.io/extension.js`
— and nothing we send a widget travels through a window title.

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

## Four things the tiles corrected once they ran

The mechanism above was measured before anything was built on it. What follows
was not measurable in advance — it only appears when a widget is on screen —
and each of the four produced a tile that looked plausible and was wrong.

### 5. QML builds a second singleton when the type is default-constructible

`QML_SINGLETON` plus a `static T *create(QQmlEngine *, QJSEngine *)` looks like
it settles how the object is made. It does not. `QQmlPrivate::singletonConstructionMode()`
checks in this order:

```cpp
if constexpr (std::is_default_constructible<T>::value)
    return SingletonConstructionMode::Constructor;   // <- wins
if constexpr (HasSingletonFactory<T>::value)
    return SingletonConstructionMode::Factory;
```

So a public `Foo(QObject *parent = nullptr)` makes the type default
constructible, that branch is taken first, and `create()` is never called
however correct it is. The engine gets a fresh object; C++ goes on holding the
one it made; nothing warns at build time or at run time.

Here that meant the command line parsed correctly, the recorded snapshot loaded
correctly, and every tile came up empty — because QML's `WidgetOptions` had no
widget list and QML's `DaemonLink` had never been told about the file. The fix
is one access specifier: the constructors of `DaemonLink`, `WidgetOptions` and
`Wx` are private, which is what `app/settings.h` and `app/viewmodels/units.h`
already did without saying why.

### 6. A JSON array is not a JavaScript array

`QJsonObject::toVariantMap()` is what keeps a JSON `null` a null QVariant all
the way into QML — rule 2 of the wire format survives the trip because of it.
What it also does is turn every array into a **QVariantList**, which QML hands
to JavaScript as a sequence wrapper: it has a `length`, it indexes, and
`Array.isArray()` returns **false** for it.

A guard written as `Array.isArray(v) ? v : []` therefore turns every series in
the snapshot into an empty one. The tiles lay out correctly, draw nothing, and
look exactly like a tile waiting for its first snapshot. `wire.js` copies into a
real array instead, because the wrapper also has none of `Array`'s methods and
`.concat()` on one throws inside a binding — where a throw is an expression that
silently evaluates to `undefined`.

### 7. A tile has no page behind it

`Theme.surface.base` is a 7 % white wash. It is a *lift* off the page, not a
colour, and that works everywhere in the app because there is always a page.
On a desktop there is not: there is a wallpaper the user chose and this process
knows nothing about.

Rendered as-is, a dark-mode tile was 7 % white over a photograph with white text
on it — legible over some wallpapers and invisible over the rest, and
unfixable from the theme, because the token means what it says. So a tile paints
`Theme.page.bg` at 92 % and carries its own page. The same argument makes the
hairline card edge unconditional here rather than a light-mode exception:
`docs/10-design-system.md` §10.1 bans borders because contrast against the page
defines a card, which is exactly the premise a wallpaper removes.

### 8. A loading skeleton is a claim, and it was making a false one

Every tile drew three grey bars while it had no snapshot. Correct for the
fraction of a second before the first one arrives — and the same picture,
indefinitely, when no snapshot was ever coming.

That state was not rare. It is what a desktop looks like whenever the daemon is
not running, which until D-Bus activation was every Flatpak install on a
compositor that is not GNOME, every first run before the next login, and every
build tree. Four tiles animating nothing, no message on screen, and nothing in
the journal either: the only warning on that path was for a session bus that
could not be reached, which is not the case that happens.

`docs/README.md` ranks not fabricating a reading above everything else. This was
the same lie told with a picture instead of a digit — and worse than a wrong
number, because a wrong number is at least reported. A skeleton is read as *the
software is working on it*, and that is what sends somebody to look at the
widget code rather than at the service that is not running.

So `WidgetFeed::waitingReason` splits the state in two. The bars now mean "a
snapshot is on its way" and are shown only when one is; everything else gets a
sentence that names what is wrong, because "not running", "not answering" and
"there is no place set" send a reader to three different places:

| What the tile shows | What is actually true |
|---|---|
| a skeleton | subscribed, or an activation request is in flight |
| The Clima weather service is not running. | nothing owns the name and the bus could not start one |
| The Clima weather service is not answering. | something owns the name and did not reply |
| No place yet. Open Clima and choose one. | a working daemon with an empty place database — a first run |

The third one is the interesting one, and it was a second silent failure hiding
behind the first: a package installs the widgets and the daemon together, so the
tiles can reach a healthy daemon on a machine where nobody has opened Clima and
chosen anywhere. `Subscribe` answers with an empty token, which is the daemon
saying *I have no place by that id* — and that answer had been on the wire,
unread, since the day the interface was written.

---

## What exists today

**Built and verified.**

| | |
|---|---|
| The adoption mechanism | Measured on GNOME Shell 46, Wayland — the verdict above |
| The wire format and its field mask | `tst_wiresnapshot`, 17 assertions, three encoder rules |
| `clima-daemon` | Exercised end to end on a private session bus: introspection, a masked `GetSnapshot`, `Subscribe` delivering its own token, `Unsubscribe` |
| `clima-widget` and the ten tiles | Rendered against four recorded snapshots in both schemes; `tst_widgets` asserts the catalogue, the dispatch and the module list agree, plus every `Wx` boundary |
| Starting the daemon | D-Bus activation, run against a private bus with its own service directory: no daemon, no autostart, and `clima-widget` alone brought one up and filled its tiles. The three states a tile can be empty in were each photographed. |
| The link-line guard | `widget_has_no_engine`, verified by injecting the defect |
| The GNOME extension | `scripts/check-extension.sh`: both modules parse, the introspection XML matches what it calls, and every `Meta.WaylandClient` method it calls exists on this machine's mutter. Verified by injecting both defects. |
| Pinning on KDE and wlroots | `scripts/check-layer-shell.sh`, in CI: a real headless wlroots compositor, six assertions, one of which is the same binary with `--pin off` failing them |
| The reader's units and clock | `app/settings.cpp`, `app/viewmodels/units.cpp` and `app/viewmodels/timeformat.cpp` are compiled into the widget host, so a tile prints °F and a 24-hour clock because the app's own preference says so — one mapping, two processes |

```sh
clima-widget --list
clima-widget --snapshot tests/fixtures/wire/seattle.json --columns 2 \
             --widget current-conditions --widget alerts --grab tiles.png
clima-widget --pin on --anchor bottom-right --margin 16
```

In a build tree there is nothing installed for the bus to activate, so the tiles
will say the weather service is not running — correctly — until one is started
beside them:

```sh
build/dev/daemon/clima-daemon --fixture toronto &
build/dev/widgets/clima-widget
```

`--snapshot` is the other way, and the one CI uses: it reads a recorded snapshot
and never touches the bus at all.

### Preferences arrive at start, not while running

A tile shows the units and the clock format the reader chose in the app, because
both processes read the same INI and share the code that interprets it. What
neither shares is a *change*: the widget host reads the file when it starts.
Switch to a 24-hour clock in the app and the tiles keep the old spelling until
the host is restarted.

Nothing pushes a settings change across processes today — the daemon's bus
interface carries the forecast, not the reader's preferences. `docs/known-gaps.md`
has the entry, including the cheapest way to close it.

### The second mechanism

The tiles reach a desktop two ways, and which one is used is a property of the
compositor rather than a build option.

| | GNOME | KDE, Sway, Hyprland, Wayfire, river, labwc |
|---|---|---|
| Mechanism | the shell adopts our window | we ask for a layer surface |
| Protocol | none — mutter exposes no such thing | `zwlr_layer_shell_v1` |
| What ships | ~600 lines of GJS, separately, from extensions.gnome.org | nothing extra |
| Identity | an inherited socket fd, so we must be *spawned* | an ordinary Wayland client |
| Placement | `make_dock()` + `lower()` + saved geometry | `--anchor`, `--margin`, `--layer` |
| Measured on | GNOME Shell 46, by hand | headless wlroots, in CI |

Both draw the same tiles from the same binary. The GNOME column is the
expensive one and it is expensive because of the first row: where a protocol
exists, none of the rest of that column is needed.

**A guard that matters more than it looks.** The availability probe in
`widgets/layershell.cpp` refuses to run when `WAYLAND_SOCKET` is set, and that
is not tidiness. `wl_display_connect(nullptr)` reads that variable, takes
ownership of the descriptor and unsets it — and that descriptor is precisely
the one the GNOME extension handed us to establish who we are. Probing there
would consume the handshake, leave Qt with no socket to connect to, and produce
tiles that never appear under the one shell that spawns us.

**Not built, and each for a stated reason.**

- **The extension has not been run on a live session.** It is the probe's
  measured mechanism with placement, respawn, a daemon starter and an indicator
  around it, and `scripts/check-extension.sh` covers what can be asserted
  without a shell. The `shell-version` list declares 45 to 48 and only 46 was
  measured; that is a claim to re-check before the first upload.

- **The Plasma applet.** Not built and not going to be. The plan called it the
  cheapest of the three because "a Plasma applet *is* QML"; that is wrong, and
  `packaging/plasma/README.md` has the correction. Every tile reads
  `WidgetFeed`, `DaemonLink`, `Wx` and `Units`, which are C++ types a plasmoid
  cannot import unless the module is installed as a shared QML plugin — and
  Plasma 6 ships no generic D-Bus binding for QML, so a pure-QML second
  implementation is not available either. **`--pin` is what KDE gets instead**,
  and it is a better outcome than an applet: the same binary, the same tiles,
  and every wlroots compositor for free. It is in the table above rather than
  here.

- **The SNI tray.** Dropped rather than deferred. It was in the plan as the
  cheap validator for the wire schema, and the schema now has two independent
  readers — a Qt host and a GJS indicator — which is a stronger check than one
  more Qt process would have been. As a *feature* it earns less than it costs:
  GNOME hides SNI icons entirely without a third-party extension, KDE gets a
  better answer from layer-shell, and Windows — where a tray genuinely is the
  right shape — has no session bus and therefore no daemon to read from.

- **The Background portal.** A Flatpak cannot write to `/etc/xdg/autostart`, so
  a Flatpak-installed daemon does not autostart. D-Bus activation covers the
  case that matters — the daemon is running by the time the first tile has
  anything to ask — and what the portal would add on top is a daemon that is
  *already* running when the desktop appears, so the first reading is not
  fetched while somebody watches. That is a permission prompt, and it belongs
  with the notifications work rather than here.

The daemon is additive. The app does not link it, does not know about it, and
behaves exactly as it did before this existed when nothing is running.
