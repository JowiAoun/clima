<!-- SPDX-FileCopyrightText: 2026 Clima contributors -->
<!-- SPDX-License-Identifier: CC-BY-SA-4.0 -->

# Contributing to Clima

Thank you for looking. This file is longer than most because several of the
rules here are unusual, and a rule you find out about in review is a rule that
wasted your afternoon.

Start with [`BUILDING.md`](BUILDING.md) to get it compiling, and
[`docs/README.md`](docs/README.md) if you want to know why anything is the way
it is.

## Sign your commits: DCO, not a CLA

This project uses the [Developer Certificate of
Origin](https://developercertificate.org/). There is no CLA, and you keep the
copyright in what you write.

```sh
git commit -s
```

That adds a `Signed-off-by:` line, which is you asserting the following:

> **Developer Certificate of Origin, Version 1.1**
>
> By making a contribution to this project, I certify that:
>
> (a) The contribution was created in whole or in part by me and I have the
> right to submit it under the open source license indicated in the file; or
>
> (b) The contribution is based upon previous work that, to the best of my
> knowledge, is covered under an appropriate open source license and I have the
> right under that license to submit that work with modifications, whether
> created in whole or in part by me, under the same open source license (unless
> I am permitted to submit under a different license), as indicated in the file;
> or
>
> (c) The contribution was provided directly to me by some other person who
> certified (a), (b) or (c) and I have not modified it.
>
> (d) I understand and agree that this project and the contribution are public
> and that a record of the contribution (including all personal information I
> submit with it, including my sign-off) is maintained indefinitely and may be
> redistributed consistent with this project or the open source license(s)
> involved.

## Commit messages

Conventional commits, and the history is strictly conventional because
`release-please` derives the changelog and the version bump from it. A
mislabelled commit ships a wrong version number.

```
<type>(<scope>): <description in the imperative, lowercase, no full stop>
```

`feat` means a **new user-facing capability** and `fix` means a **bug fix**.
Those two drive the version, so a refactor labelled `feat` cuts a minor release
for nothing. Everything else is `docs`, `style`, `refactor`, `perf`, `test`,
`build`, `ci`, `chore` or `revert`.

Scopes in use, so you can pick an existing one rather than inventing a
neighbour: `app`, `libclima`, `gallery`, `packaging`, `net`, `alerts`,
`golden`, `android`, `design`, `build`, `licensing`.

Write a body whenever the *why* is not obvious from the diff. Explain the
motivation and the consequence, not the code — the diff already has the code.

## Three rules that are not obvious

### 1. Do not run `qmlformat` over the tree

Not on your files, not on the tree. It reflows 13,400 lines and destroys the
aligned comment blocks that are this codebase's distinguishing feature. The
same goes for `clang-format` over the C++: the repository has no formatting
gate, and [`.github/workflows/ci.yml`](.github/workflows/ci.yml) records the
measurements behind that decision — every stock style wanted between 5,585 and
35,380 replacements, and a hand-tuned config still wanted 2,719.

New code is reviewed for style by a person. Match the file you are editing.

### 2. The engine may not touch the GUI

`libclima/` is MPL-2.0 and links no Qt GUI module. This is not a preference:
`cmake/ClimaEngineGuard.cmake` asserts it, and a test inspects the built binary
for QtGui symbols. It keeps the engine reusable outside a GPL program, which is
decision D6 in [`docs/03-tech-stack.md`](docs/03-tech-stack.md).

If your change wants a `QColor` in `libclima`, it belongs in `app/` instead.

### 3. Every file carries an SPDX header

`reuse lint` gates every commit. A new file needs two comment lines: an
`SPDX-FileCopyrightText` naming you and the year, and an
`SPDX-License-Identifier`. The identifier is `GPL-3.0-or-later` for most of the
tree, `MPL-2.0` under `libclima/`, and `CC-BY-SA-4.0` for documentation and
artwork.

The reliable way to get it right is to copy the header off the file next door,
which is also how you inherit the right licence for wherever you happen to be.
`reuse annotate` will write one for you.

Binaries and JSON, which have nowhere to put a comment, go in `REUSE.toml`
instead — one explicit path each, never a glob.

(This paragraph is prose rather than a code block on purpose: `reuse` reads
example headers in fenced blocks as real declarations, so a sample here would
license this file twice.)

## When your change moves a picture

Two suites compare images byte for byte, and both will fail on a change that is
perfectly correct.

```sh
scripts/golden.sh check          # 46 scenes from the app and the gallery
scripts/shots.sh check           # 5 composed device images for the README
```

To re-record them:

```sh
scripts/golden.sh accept
scripts/shots.sh
```

**A pull request that re-records an image has to say why the pixels moved.**
That sentence is the entire value of the suite. "Re-baselined goldens" tells a
reviewer nothing and turns a regression detector into a formality; "the ink
ladder was re-spaced, so every label one step down is lighter" tells them
exactly what to look at.

If the diff is not what you expected, look at the `.actual.png` files the run
leaves beside the originals before assuming the test is wrong.

## Before you push

```sh
nix develop --command ctest --test-dir build/dev --output-on-failure
nix develop --command reuse lint
nix develop --command shellcheck --external-sources scripts/*.sh
nix develop --command actionlint
nix develop --command bash scripts/check-qml-files.sh
CLIMA_BUILD_DIR=build/lint nix develop --command bash scripts/check-qmllint.sh
```

`check-qml-files.sh` is the one people trip over: the QML module lists every
file explicitly and never globs, so a new `.qml` has to be added to
`app/CMakeLists.txt` by hand. Forget it and the type simply does not exist, and
QML reports that at the point of *use* — naming a file you did not touch.

`check-qmllint.sh` is a ratchet, not a threshold. It fails when the warning
count goes **up**. Unqualified property access is what stops `qmlcachegen`
compiling a binding ahead of time, so this is a performance gate wearing a lint
costume.

## Writing a test that can actually fail

The house rule, learned the expensive way: **prove a new test fails by injecting
the defect it is written for**, then remove the injection. The suite here has
been green while the app printed four warnings a launch, because QTestLib
installs its own message handler and silently replaced ours.

Tests live in `tests/` as Qt Test, and in `tests/qml/` as QtQuickTest. There is
no Catch2 and no GoogleTest — decision D8 rules out any dependency fetched at
configure time, so that a distribution packager can build this from what they
already ship.

## Reporting a bug

Use the issue template, and attach a screenshot taken by the app itself:

```sh
clima --grab bug.png
```

That captures exactly what the app rendered, at the size it rendered it, with no
window manager in the way — which answers half the questions a report otherwise
generates. Tell us the install channel (Flatpak, `.deb`, MSI, source) and
whether you are on Wayland or X11.

## Security

Do not open a public issue for a vulnerability. See
[`SECURITY.md`](SECURITY.md), which also explains why the attack surface here is
smaller than you might expect — there is no server, no account and no telemetry.
