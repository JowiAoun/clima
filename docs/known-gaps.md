<!--
SPDX-FileCopyrightText: 2026 Jowi Aoun
SPDX-License-Identifier: CC-BY-SA-4.0
-->

# Known gaps

Things this app does not do, or does not do yet, written down where a reader
can find them rather than discovered by using it. Each entry says what is
missing, what it costs, and what would have to happen for it to close.

A gap is not a bug. A bug is behaviour that contradicts what the app claims;
everything here is something the app has never claimed, and the point of the
file is to keep it that way.

---

## Android: the app runs, the alerts do not

**Status: the build is written and has never run. Nothing has been on a device.**

What exists today is the whole of the client side. The mobile shell is the
tablet shell is the phone shell, the touch targets clear the 44 px floor, the
back gesture pops a tab and then lets the platform close the app, the drawing
tier halves the star field on a handheld, and `app/CMakeLists.txt` carries the
Qt Android deployment properties and the two permissions the app needs. There
is a CI job, gated on `workflow_dispatch` because it has never executed.

None of that is the gate. **The gate is delivering a severe weather alert to a
phone that is asleep**, and it is not a rendering problem or a packaging
problem — it is a problem Qt does not have an answer to.

### What the desktop does, and why it does not port

On a desktop, alert polling is `AlertsData`'s timer: three minutes with the
window focused, ten idle, and **stopped entirely when the window is hidden**.
That last rule is what makes the poll cost defensible — see
`docs/04-architecture.md` §4.5 — and it is also exactly the rule that makes the
feature useless on a phone, where the window is hidden almost all of the time.

An Android app that wants to poll while it is not on screen needs, in order:

1. **A `WorkManager` periodic job**, which is Java. Qt gives you `QJniObject`
   and nothing above it, so this is hand-written JNI plus a Java class in the
   package source directory — the first Java in this repository.
2. **A notification channel**, created at first run, with the severity opt-in
   the desktop already has mapped onto Android's channel importance levels.
3. **Battery-optimisation UX.** Doze batches `WorkManager` jobs into
   maintenance windows; the effective floor is roughly 15 minutes and in Doze
   it is longer than that. An extreme heat warning arriving 40 minutes late is
   defensible. A tornado warning arriving 40 minutes late is not, and an app
   that appears to deliver tornado warnings and does not is worse than an app
   that says it does not.
4. **A foreground-service declaration** if 3 is unacceptable, which on Google
   Play means declaring a foreground service type and justifying it in review.
   `dataSync` is the honest type and Play has been rejecting it for exactly
   this shape of use.
5. **`SCHEDULE_EXACT_ALARM`** if even that is not enough, which since Android
   13 is granted by the user in a system settings page most users never open.

### What that means for scope

Steps 1 and 2 are a week of work and are worth doing. Steps 3 to 5 are a
product decision, not an engineering one, and the decision is between:

- **Ship the app without background alerts.** Alerts appear when the app is
  opened, which is honest, useful, and how most weather apps behaved before
  push. The app must then say so in its own settings screen — an alert toggle
  that silently means "when you happen to look" is the failure this whole
  feature exists to avoid.
- **Ship a foreground service.** Reliable, visible in the notification shade
  forever, and a Play review argument. F-Droid, which
  `docs/06-roadmap.md` names as the natural primary channel for a GPL,
  no-telemetry, no-account weather app, has no such review.

**The recommendation is the first one**, with the second reachable as an opt-in
later. It is reversible, it is truthful, and it does not put the release behind
a Play policy conversation.

### What would close this

An APK built by somebody with the toolchain, installed on a device, and the
sentence "alerts arrive only while the app is open" written into the settings
screen next to the toggle that controls them.

---

## Every capture runs the fallback observation, never the live one

`ConditionsData::buildContext` has two ways to decide what "now" is. Open-Meteo
sends a `current` block stamped to the quarter hour, and that is used whenever it
is actually current — within an hour of the clock. When it is not, the
observation is rebuilt from the hour the reader is standing in.

**The fixtures always take the second branch.** `tests/fixtures/wire/toronto.json`
carries `recordedAt: 12:28` and a `current.time` of `06:30`, six hours apart, so
every one of the fifty-three golden images and every test that loads a fixture
photographs the fallback. The branch the app takes on every real run has no
picture of it anywhere.

That is not academic. The card drawing `current.weather_code` while the chart
drew the hourly series was a visible contradiction — "Mainly sunny" a few
centimetres above a Now column showing heavy rain, in the rain — and it could
not have been caught by any capture, because in a capture the two are the same
value. It was found by someone looking at the running app. `tst_conditionsdata`
now moves the clock onto the block's own stamp to reach the branch, which tests
the arithmetic but photographs nothing.

Closing it means re-recording the fixtures so `current.time` sits inside the
hour `recordedAt` names — `scripts/` has the recorder — and then re-accepting
the goldens, which will move: the hero would show the block's readings rather
than the hour's, which differ by a degree or two. Worth doing next time the
fixtures are refreshed for another reason, since the two changes land in the
same images.

---

## The detail cards are about now, whichever day the chart is showing

The day strip moves the hourly window: pick Friday and the chart, the list and
the precipitation strip are Friday's, midnight to midnight. **The twelve detail
cards below it are not.** They are `Detail` — `app/viewmodels/conditionsdata.h`
— which is built entirely around the present observation, and a card that reads
"Peaks at 4:00 p.m." means today whatever the strip says.

This is defensible as it stands and it is not invisible. On the desktop the two
are separate sections with their own headers and the details carry the
observation stamp, so neither claims to be the other. On the phone one line did
claim it — the Hourly screen's daily summary put today's sentence under the
selected day's high and low — and that line is now hidden on any day but today,
which is honest and is also obviously a stopgap.

Closing it means giving `ConditionsData` the same treatment `ForecastData` just
had: a selected day, a window that follows it, and a decision per block about
what each of the fifteen means on a day that is not today. Several of them have
no meaning at all there — "feels like" is a reading, not a forecast, and an air
quality index four days out is a different product from the one this shows.
So it is a design question first and a port second, and the honest intermediate
is what exists now: the sections that follow the day say so, and the ones that
do not are dated.

---

## A glyph on the night plate is measured against a ground it never touches

`WeatherGlyph` takes one boolean, `onLightBackground`, and it means "the thing
under me is pale". Exactly one caller sets it: `DayIconBadge`, for the near-white
day plate on the selected day card. Every mark therefore has two values — a card
one and an `…OnLight` one — and `Theme.qml`'s contrast table scores the second
against `badge.dayTop`, which is what it is actually drawn on.

**The same badge has a night plate, and nothing measures against it.** It is a
mid-blue disc, `badge.nightTop` `#6d9ae8` down to `#3f63bd`, and a glyph on it is
drawn in the card colours because a boolean cannot say "pale, mid, or dark".
`glyph.rain` `#7fb6e8` on that plate is **1.31:1** — the night half of a rainy
day card has raindrops that are, measurably, not visible. The cloud carries the
glyph on its own, which is why nobody noticed.

The audit cannot see it either: `glyph.rain`'s declared ground is `surface.base`,
where it measures 5.26:1 and passes. A token is only ever scored against one
ground, and this one has three.

Closing it means the flag stops being a boolean. `onLightBackground: bool`
becomes something like `plate: color` — the actual ground, defaulting to
transparent for "the card" — and each mark picks its ink from the measured
contrast rather than from a name. That is a change to thirteen call sites and to
how the contrast contract is written, which is why it is not folded into the
commit that found it.

---

## The desktop page is not touch-audited

`tests/qml/tst_hittargets.qml` measures every tappable area on every screen the
mobile shell can reach, and it does not measure `WeatherPage` or the twelve
detail cards. That is deliberate — a desktop is a pointer device, and a pointer
is one pixel — but it is a gap and not a proof: a 1024 px touch screen runs the
desktop page today, and nothing checks what that is like to use.

The two controls the mobile shell borrows from the desktop, `PagerButton` and
`FeelsLikeToggle`, are covered because the phone's hourly screen reaches them.

Closing this means either adding the desktop groups to that test and raising
whatever it finds, or deciding that a touch device never gets the desktop page
— which is a change to `Viewports.classOf` and to nothing else.

---

## `SafeArea` is a constant, not a measurement

`Theme.metric.navSafeArea` is 12 px, and on a real phone the gesture strip is
whatever the device says it is. Qt exposes that as the `SafeArea` attached
property in 6.9; this project's floor is 6.8, so the constant stands in.

The constant is not only a stopgap. The gallery's device frames are drawn
against it, and golden images need a number that does not depend on which
handset the capture ran on. When `SafeArea` arrives, the app should read it and
the gallery should keep the constant.

---




## The desktop widgets have never been pinned on a KDE session

**Status: two mechanisms, one measured by hand, one measured in CI, and neither
measurement was taken on Plasma.**

The tiles reach a desktop two different ways and both of them work:

- **GNOME.** A shell extension spawns `clima-widget`, adopts the window,
  re-types it as a dock and lowers it. Mutter exposes no protocol for this, so
  there is no other way in. Measured by hand on GNOME Shell 46, Wayland — see
  `docs/widgets.md`.
- **Everywhere else.** `clima-widget --pin` asks the compositor for a
  `zwlr_layer_shell_v1` surface and places itself. Measured in CI, against a
  real headless wlroots compositor, by `scripts/check-layer-shell.sh`.

The gap is in the second row. **wlroots is not KWin.** It is the reference
implementation of that protocol, KWin was written against the same protocol, and
the surface `clima-widget` creates uses nothing outside version 1 of it — which
is a good argument and is not a measurement. `docs/widgets.md` exists because
the GNOME mechanism was measured before anything was built on it, and the same
standard applies here.

Two smaller ones travel with it. The GNOME extension declares
`shell-version` 45 to 48 and only 46 has been run, which is a claim to re-check
before the first upload to extensions.gnome.org. And the monitor-hotplug
recovery in `widgets/layershell.cpp` — unplug the screen a pinned surface lives
on and the tiles come back on another one — has been exercised against sway's
`output … unplug`, which is a developer command, not a cable.

**What closes it:** `clima-widget --pin on` on a Plasma 6 session and on one
other wlroots compositor that is not sway, with the results written into
`packaging/plasma/README.md`. Nothing is expected to need changing; what is
missing is somebody having looked.

---

## The Windows build is unsigned

**Status: shipped this way, deliberately, because the alternative is worse.**

The MSI and the portable ZIP carry no Authenticode signature, so Windows
SmartScreen shows an "unknown publisher" dialog the first time somebody runs
either one. That is not a defect in the build; it is the absence of a code
signing certificate, and there is no way to produce one from CI.

`docs/07-packaging.md` §7.1 lists **signed MSIX** as the P0 Windows channel.
That is corrected to **unsigned MSI**, and the reason is the signing rather
than the format. An unsigned MSIX cannot be side-loaded at all until the user
imports a certificate into their trusted root store, which is a worse thing to
ask of somebody than dismissing a warning — it teaches them to trust an
arbitrary publisher permanently in order to run one program once. An unsigned
MSI simply warns.

MSI also earns three things independently of that: winget validates it
natively, it installs per-user with no administrator rights, and it produces a
real Add/Remove Programs entry with an upgrade code, so version two replaces
version one instead of sitting beside it.

### The mitigations, in the order they should be attempted

1. **Azure Trusted Signing**, roughly $10/month, authenticates from Actions
   over OIDC with no hardware token. This is the real fix. Confirm eligibility
   first: individual accounts need a three-year identity history, which is a
   requirement a new account cannot satisfy by waiting a week.
2. **Publish to winget.** The manifest pins a SHA-256, so `winget install`
   verifies the download against a hash in a reviewed, public repository. It
   does not remove the SmartScreen dialog; it does mean the bytes were checked
   by something other than the user's judgement.
3. **`SHA256SUMS` and build provenance**, which the release workflow already
   attaches. `gh attestation verify` proves an artefact came out of this
   workflow at this commit. That is weaker than a signature in exactly one way
   — it is not checked by the operating system — and stronger in one way, since
   it names the source revision.

Until 1 happens, the README has to say the build is unsigned. A project that
quietly ships unsigned binaries and lets users discover it from a Windows
dialog has told them something about how it handles the things they cannot see.

---

## There is no macOS build

**Status: builds in CI, ships nothing, and that is a decision rather than an
oversight.**

Notarising a macOS application requires an Apple Developer ID at $99/year.
Without notarisation, Gatekeeper on a current macOS refuses to open a
downloaded app at all — not a warning, a refusal — and the workaround is a
right-click-open dance that changes with every release. Shipping a DMG nobody
can open would be worse than shipping none.

The engine is licensed to keep the door open: `libclima` is MPL-2.0 precisely
so that a macOS build is a packaging decision later rather than a licensing
problem. The Mac App Store stays ruled out regardless — D6, GPLv3 against the
App Store terms.

---

## The .deb does not cover Ubuntu 24.04

**Status: correct behaviour, and the Flatpak is the answer.**

Ubuntu 24.04 LTS ships Qt 6.4.2. This project's floor is Qt 6.8, which is where
the Qt Quick features it relies on settle, so the package declares
`libqt6core6t64 (>= 6.8.2)` and apt correctly refuses to install it there.

That is the right failure. A package that installed and then would not start is
worse than one that says why up front. 24.04 users, and anybody on a
distribution older than Debian 13, get the Flatpak — which carries its own Qt
out of `org.kde.Platform` and does not care what the host has. That is the
whole reason `docs/07-packaging.md` §7.1 makes Flathub the P0 channel.

`docs/07-packaging.md` §7.3's `linux-system-qt` on `ubuntu-24.04` is corrected
to `debian:trixie` for the same reason: a job pinned to a distribution that
cannot satisfy the floor cannot prove the packager build path works.

---

## The Windows and AppImage release jobs have never run

**Status: written from documentation, executed never.**

The same footing as the Android job, and recorded here for the same reason. The
development environment for this work is a Nix devshell on Linux: there is no
MSVC, no Windows, no `wix`, and no 22.04 userland with `linuxdeploy` in it. So
`packaging/windows/clima.wxs` has never been compiled by `wix build`, and the
AppImage job has never produced an AppImage.

Both are written against the documented behaviour of their tools, which is a
first draft of a build rather than a check that passed. The AppImage job is
`continue-on-error` so that a release with a working `.deb`, Flatpak and MSI is
not blocked by the P1 artefact, and the publish job prints which artefacts
arrived so that a missing one is stated rather than merely absent.

What closes it: one tagged release, and fixing whatever it says. Move the
AppImage job into the required set in the commit that makes it green.
