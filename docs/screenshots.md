<!--
SPDX-FileCopyrightText: 2026 Jowi Aoun
SPDX-License-Identifier: CC-BY-SA-4.0
-->

# Screenshots

There are **three** capture profiles in this repository and they are
deliberately different from one another. Using the wrong one is not a matter of
taste — one of them gets rejected by Flathub's linter, and another one fails CI.

| Profile | Made by | Looks like | Compared how |
|---|---|---|---|
| **golden** | `scripts/golden.sh` | raw, exact, 50 scenes | byte for byte, every commit |
| **showcase** | `scripts/shots.sh` | device bezels, composed | byte for byte, every commit |
| **store** | `clima --grab` | raw, un-bezelled, whole window | not compared |

## golden — the regression detector

50 scenes from the app and the gallery, in `tests/golden/images/`. Nobody looks
at these; a machine does.

```sh
scripts/golden.sh check      # compare
scripts/golden.sh accept     # re-record, deliberately
```

They reproduce because `flake.lock` pins nixpkgs by revision, so Qt, FreeType
and fontconfig are identical everywhere down to the store hash — a stronger
guarantee than the digest-pinned container the plan originally assumed. On top
of that, `tests/golden/fontconfig.conf` declares **no font directories at all**
(the typeface is inside the binary), which is what stops a host font being
substituted. Measured: the same scene under host, pinned and empty fontconfig
produced three different checksums.

`tst_environment` runs first by way of its `DEPENDS`, so rasterisation drift
arrives as one sentence about the machine rather than as forty picture diffs.

## showcase — the images people look at

Five composed sheets in `docs/images/`, used by the README.

```sh
scripts/shots.sh             # render
scripts/shots.sh check       # CI gate
```

These are not screenshots pasted into a README. `ShotSheet.qml` builds real
`MobileShell` and `WeatherPage` instances at the widths `Viewports` declares,
inside `DeviceFrame` bezels, on the app's own page gradient — so a README image
cannot show a layout the app does not produce. Change a breakpoint and the
images move; CI notices.

The catalogue is `gallery/qml/Clima/Gallery/shots.js`, and it carries no pixel
dimensions. A sheet says which devices it shows and how far they are zoomed; the
size comes from `Viewports` at build time. A `.pragma library` cannot reach a
QML singleton, which is a limitation worth keeping — it is what stops the
numbers being copied in.

Adding a sheet means editing three places, and `tests/qml/tst_shots.qml` fails
until they agree: `shots.js`, `GalleryOptions::shotIds()` (a command line has to
reject a bad id before any QML loads, and C++ cannot read a `.pragma library`),
and then `scripts/shots.sh` picks it up automatically.

## store — what Flathub and GNOME Software show

Raw grabs from the app itself, with no frame around them:

```sh
clima --viewport desktop --scheme dark  --grab desktop-dark.png
clima --viewport desktop --scheme light --grab desktop-light.png
clima --viewport mobile  --tab monthly  --grab mobile-daily.png
```

**Do not use the showcase images here.** Flathub's linter reads a marketing
composite with a device frame around it as excessive whitespace and rejects the
submission. A store screenshot is meant to be the application's window, nothing
else.

They are published to `gh-pages` by the release workflow rather than committed,
because AppStream wants stable absolute URLs and a `raw.githubusercontent` link
moves with the branch. The URLs are declared in
`packaging/linux/clima.metainfo.xml.in`.

## Determinism, for all three

Every profile captures against a **fixture at a frozen clock**, never live data
— `--grab` implies `--fixture toronto` unless told otherwise. A live capture is
a picture of the weather that afternoon and cannot be compared to anything.

`Theme.stillness` collapses every animation duration to zero for a still
capture. This exists because `--grab` once had a race with `PagerButton`'s
150 ms opacity fade: two alternating outputs 35 pixels apart, invisible to the
eye and fatal to a byte comparison. The same mechanism serves the reduced-motion
accessibility setting.

### The suite is not byte-deterministic on this machine, and here is exactly how

Repeated full runs of `scripts/golden.sh check`, same machine, same build, no
source change between them. Two separate failures, and they are not the same
kind of thing.

**A whole `Shape` missing.** One run in five came back with the preferences gear
absent from `alert-desktop` and `alert-desktop-light` — 600 bytes smaller, and
it reads exactly like a bug in the gear rather than a bug in the harness. Qt
Quick's render loop is threaded by default and `grabToImage()` completes on the
render thread, so the shutter races the scene. `QSG_RENDER_LOOP=basic` is now
pinned beside the rasteriser, every recorded image still matched when it was
added, and it has not recurred in fourteen runs. That is evidence, not proof,
for something that appeared once in five.

**Twenty-three pixels, off by one.** Still happening, in about two runs in five,
and always identically: `desktop-preferences.png`, x 89–95, y 827–838 — the
chevron of the left pager in the hourly chart, under the sheet's scrim. The
largest channel difference is **1**, which is a rasteriser rounding a blend two
ways rather than anything in the scene. It survives `QSG_RENDER_LOOP=basic`
(5 runs, 2 failed) and it survives `LP_NUM_THREADS=1` (5 runs, 2 failed), so it
is neither the grab race above nor llvmpipe's thread count.

It is not `Theme.stillness`: that already removed this control's fade, which is
a *different* PagerButton race and is documented in `Theme.qml`. A shade caught
mid-fade would be tens of levels, not one.

**What it costs, and the two ways out.** A suite that fails two runs in five
teaches people to re-run it, which is the same as not having it — so this needs
settling before it trains anybody. Either find the remaining rounding (the next
suspect is scene-graph batching: the chevron is the one antialiased thing under
a translucent full-page layer, and how it batches with that layer is not a
stable property of the scene), or compare with a tolerance — no pixel off by
more than one level, no more than N pixels, and report every tolerated pixel so
nothing hides behind it. The second gives up the byte-equality claim that is
this suite's whole argument, so it is a decision rather than a fix.

Two things worth taking from it either way. An intermittent capture does not
look intermittent — it looks like whatever it dropped, so the first suspect is
the newest thing on the page, which is how this cost an afternoon on a gear that
was never broken. And a harness that pins twelve variables to make the pixels
reproducible had pinned nothing about *which frame* they came from.

## When an image changes

A pull request that re-records any compared image **has to say why the pixels
moved**. That sentence is the entire value of both suites — see
[`CONTRIBUTING.md`](../CONTRIBUTING.md). Look at the `.actual.png` files a
failing run leaves beside the originals before assuming the test is wrong.
