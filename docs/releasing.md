<!--
SPDX-FileCopyrightText: 2026 Jowi Aoun
SPDX-License-Identifier: CC-BY-SA-4.0
-->

# Releasing

How a version of Clima gets from `main` to a download. Most of it is automatic;
the parts that are not are the parts that should not be.

## The short version

1. Merge work into `main` with conventional-commit messages. You already do.
2. `release-please` keeps a pull request open titled **chore(main): release
   X.Y.Z**. It accumulates every commit since the last tag.
3. **The release PR will be red.** Add the AppStream release note (below).
4. Merge it. `release-please` tags `vX.Y.Z` and creates the GitHub Release.
5. The tag triggers `release.yml`, which builds every artefact and attaches
   them to that release.

There is no manual `git tag`. If you find yourself typing one, something above
has gone wrong and tagging by hand will hide it.

## The one manual step, and why it is not automated

`release-please` rewrites exactly one thing: the version on the line in
`CMakeLists.txt` marked `# x-release-please-version`. Everything downstream —
`CLIMA_VERSION`, the `.deb` version, the MSI `ProductVersion`, the MET Norway
User-Agent — reads from there, so one edit moves all of them.

It does **not** write the AppStream release note, and `packaging/CMakeLists.txt`
fails the configure step until somebody does:

```
clima: version disagreement. project() says 0.2.0, the newest <release> in
packaging/linux/clima.metainfo.xml.in says 0.1.0.
```

So the review of a release PR is: read the generated changelog, then write the
human version of it into `packaging/linux/clima.metainfo.xml.in` as a new
`<release>` block at the top of `<releases>`.

That is deliberate. The `<releases>` block is what GNOME Software and KDE
Discover show a user deciding whether to update — curated prose, in the voice
of the product, not a list of commit subjects. Generating it would produce
"fix(net): keep the reason a 4xx gave in the error message" on a store page.
And a version bump with no note is invisible until it is published, which is
the worst moment to notice it.

## What a release carries

| Artefact | Built by | State |
|---|---|---|
| `clima_X.Y.Z_amd64.deb` | `debian:trixie` container | verified |
| `clima-X.Y.Z-x86_64.flatpak` | `flatpak-builder` | verified |
| `clima-X.Y.Z-windows-x64.msi` | WiX v5 on `windows-latest` | **never run** |
| `clima-X.Y.Z-windows-x64.zip` | `Compress-Archive` on the staged install | **never run** |
| `clima-X.Y.Z-x86_64.AppImage` | `linuxdeploy` on `ubuntu-22.04` | **never run**, `continue-on-error` |
| `SHA256SUMS` | `sha256sum` | |
| `clima.spdx` | `reuse spdx` | SBOM, from the SPDX headers CI already gates |
| `THIRD-PARTY-LICENCES.txt` | `scripts/licence-bundle.sh` | |
| `QT-SOURCE-OFFER.txt` | committed, copied | LGPLv3 obligation |
| build provenance | `actions/attest-build-provenance` | `gh attestation verify` |

The publish job prints which artefacts arrived and marks any that did not, into
both the job summary and the release body. A missing file and a file nobody
promised look identical on a release page otherwise.

## The obligations, in one place

Two of the attachments are not optional and both are easy to drop by accident.

**`THIRD-PARTY-LICENCES.txt`.** `clima` is statically linked and carries Inter,
the GeoNames place index and recorded ECCC, NWS and Open-Meteo payloads inside
the executable. Five licences that are not ours, in a program that otherwise
appears to be GPL and nothing else. Generated from `packaging/linux/copyright`
and `LICENSES/`, both of which `reuse lint` gates, so it cannot describe a set
of components that is not the set shipped.

**`QT-SOURCE-OFFER.txt`.** The Windows artefacts and the AppImage bundle Qt,
which makes them LGPLv3 conveyances. A link to qt.io does not discharge that —
GPLv3 §6 permits pointing at a third party's server only when the recipient got
the object code from that same server. The offer is valid three years and the
`.deb` and Flatpak are explicitly outside it, because they convey no Qt.

This is also why `cmake/ClimaCPack.cmake` defines no Windows generator. A
`cpack -G ZIP` would produce a Qt-bundling archive carrying neither file, from
one command, on anybody's machine.

## Rehearsing without releasing

```sh
scripts/deb.sh inspect          # the .deb, in debian:trixie, with its control file
scripts/flatpak.sh deps         # once: the runtime and SDK
scripts/flatpak.sh bundle       # the single-file .flatpak
scripts/licence-bundle.sh       # THIRD-PARTY-LICENCES.txt
nix develop --command actionlint
```

`release.yml` also accepts `workflow_dispatch`, which runs the whole thing
without a tag. Note that the publish step still wants `$GITHUB_REF_NAME` to be
a release name, so dispatch it from a tag ref if you want it to go all the way.

## When Flathub happens

The manifest in `packaging/flatpak/` builds a `dir` source — this working tree
— which is what makes `scripts/flatpak.sh build` useful on a branch. A Flathub
submission is a **separate repository**, `flathub/io.github.JowiAoun.Clima`,
whose manifest is the same file with a `git` source pinned to a tag and a
commit. A published build has to be reproducible from something immutable.

Flathub also runs `appstreamcli validate` and requires screenshots at stable
URLs. `scripts/check-packaging.sh` runs the same validation locally; the
screenshots are published by the release workflow to `gh-pages`, un-bezelled,
because Flathub's linter reads a marketing composite with a device frame around
it as excessive whitespace.
