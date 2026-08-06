<!-- SPDX-FileCopyrightText: 2026 Clima contributors -->
<!-- SPDX-License-Identifier: CC-BY-SA-4.0 -->

## What this changes, and why

<!-- The why, not the what — the diff already has the what. -->

## Checklist

- [ ] Commits are signed off (`git commit -s`). This project uses the DCO, not a CLA.
- [ ] Commit messages are conventional. `feat` only for a new user-facing capability, `fix` only for a bug fix — those two drive the version number.
- [ ] `ctest --test-dir build/dev` passes.
- [ ] `reuse lint` passes; any new file carries an SPDX header.
- [ ] New `.qml` files are listed in the module's `CMakeLists.txt` (`scripts/check-qml-files.sh`).
- [ ] `qmllint` warnings did not go up (`scripts/check-qmllint.sh`).
- [ ] I did not run `qmlformat` or `clang-format` over the tree. See CONTRIBUTING.md.

## If an image moved

<!--
  DELETE THIS SECTION if no golden image or README image changed.

  If one did: say why the pixels moved. This is the entire value of the suite —
  "re-baselined goldens" turns a regression detector into a formality, whereas
  "the ink ladder was re-spaced, so every label one step down is lighter" tells a
  reviewer exactly where to look.
-->

- [ ] `scripts/golden.sh accept` and/or `scripts/shots.sh` re-recorded
- Why the pixels moved:

## If a new test was added

- [ ] I proved it fails by injecting the defect it is written for, then removed the injection.
