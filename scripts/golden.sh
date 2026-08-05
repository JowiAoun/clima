#!/usr/bin/env bash
# SPDX-FileCopyrightText: 2026 Jowi Aoun
# SPDX-License-Identifier: GPL-3.0-or-later
#
# Golden images: capture, compare, and re-record.
#
#   scripts/golden.sh check              compare against tests/golden/images
#   scripts/golden.sh accept             re-record them, deliberately
#   scripts/golden.sh capture <dir>      write them somewhere and stop
#
# CLIMA_BUILD_DIR selects the build to photograph; it defaults to build/dev.
# CLIMA_GOLDEN_FILTER=<substring> narrows the run while working on one case.
#
# ---- how this is reproducible on a machine that is not yours ----------------
#
# Not a container, which is what docs/07 and the original plan assumed. This
# repository has something stronger already: flake.nix pins nixpkgs by revision
# in flake.lock, so `nix develop` produces the same Qt, the same FreeType and
# the same fontconfig everywhere, down to the store hash. A `debian:trixie`
# tag is a moving target by comparison — the image behind it is rebuilt, and
# `trixie` in six months is not `trixie` today.
#
# So the rule is: run this under `nix develop`. CI does. If you run it outside
# one, tst_environment will tell you the text is rasterising differently before
# any image is compared, which is the failure this is designed to produce
# instead of thirty picture diffs.
#
# The other half is FONTCONFIG_FILE, exported below. It replaces the host's
# fontconfig outright with tests/golden/fontconfig.conf, which declares no font
# directories at all — so no host font can be substituted — and pins hinting and
# antialiasing, which is where the same face on two machines otherwise lands
# glyphs at different subpixel offsets. Measured: the identical scene under
# host, pinned and empty fontconfig produced three different checksums.
set -euo pipefail

here="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
root="$(cd "$here/.." && pwd)"

cases_file="$root/tests/golden/cases"
images_dir="$root/tests/golden/images"
build_dir="${CLIMA_BUILD_DIR:-$root/build/dev}"
filter="${CLIMA_GOLDEN_FILTER:-}"

command="${1:-check}"
shift || true

case "$command" in
    check|accept) target="" ;;
    capture)
        target="${1:-}"
        if [[ -z "$target" ]]; then
            echo "golden: capture needs an output directory" >&2
            exit 2
        fi
        ;;
    -h|--help)
        sed -n '5,12p' "${BASH_SOURCE[0]}" | sed 's/^# \?//'
        exit 0
        ;;
    *)
        echo "golden: unknown command \"$command\" — check, accept or capture" >&2
        exit 2
        ;;
esac

app="$build_dir/app/clima"
gallery="$build_dir/gallery/clima-gallery"

for binary in "$app" "$gallery"; do
    if [[ ! -x "$binary" ]]; then
        echo "golden: $binary is not there. Build first, or set CLIMA_BUILD_DIR." >&2
        exit 2
    fi
done

# The pinned capture environment. Identical to scripts/grab.sh's — that file
# carries the long argument for each line — plus the fontconfig replacement,
# which is what makes these comparable across machines rather than merely
# repeatable on one.
export QT_QPA_PLATFORM=offscreen
export LIBGL_ALWAYS_SOFTWARE=1
export GALLIUM_DRIVER=llvmpipe
unset QT_QUICK_BACKEND QSG_RHI_BACKEND QMLSCENE_DEVICE
export QT_SCALE_FACTOR=1
export QT_ENABLE_HIGHDPI_SCALING=0
export QT_SCREEN_SCALE_FACTORS=
export QT_FONT_DPI=96
export QT_QPA_PLATFORMTHEME=
export LC_ALL=C.UTF-8
export LANG=C.UTF-8
export TZ=UTC
export QT_FORCE_STDERR_LOGGING=1
export FONTCONFIG_FILE="$root/tests/golden/fontconfig.conf"

# shellcheck source=scripts/qt-env.sh
. "$here/qt-env.sh" >/dev/null

# A run must not read or write the developer's own settings: a remembered
# window size would change a capture, and a capture must not leave one behind.
scratch="$(mktemp -d)"
trap 'rm -rf "$scratch"' EXIT
export XDG_CONFIG_HOME="$scratch/config"
export XDG_DATA_HOME="$scratch/data"
export XDG_CACHE_HOME="$scratch/cache"

out="${target:-$scratch/images}"
mkdir -p "$out"

captured=0

while IFS='|' read -r name binary size args; do
    [[ -z "${name// }" || "${name:0:1}" == "#" ]] && continue
    if [[ -n "$filter" && "$name" != *"$filter"* ]]; then
        continue
    fi

    case "$binary" in
        app)     executable="$app" ;;
        gallery) executable="$gallery" ;;
        *)
            echo "golden: case \"$name\" names binary \"$binary\", which is not app or gallery" >&2
            exit 2
            ;;
    esac

    # Deliberately unquoted: the arguments column is a list, and the whole
    # point is that it splits. Quoted values in it — "Feels like" — are handled
    # by eval rather than by word splitting, because a gallery entry's name has
    # a space in it and there is no way to say that with IFS alone.
    eval "set -- $args"

    if ! "$executable" --size "$size" "$@" --grab "$out/$name.png" >/dev/null 2>"$scratch/err"; then
        echo "golden: $name failed to render" >&2
        sed 's/^/  /' "$scratch/err" >&2
        exit 1
    fi

    # A capture that renders but warns is a capture of a broken scene. The QML
    # suite asserts this too, on components; here it covers the assembled page.
    if grep -qiE 'warning|error|undefined|TypeError' "$scratch/err"; then
        echo "golden: $name rendered with diagnostics" >&2
        sed 's/^/  /' "$scratch/err" >&2
        exit 1
    fi

    captured=$((captured + 1))
done < "$cases_file"

if [[ $captured -eq 0 ]]; then
    echo "golden: no cases matched${filter:+ filter \"$filter\"}" >&2
    exit 2
fi

if [[ "$command" == "capture" ]]; then
    echo "golden: wrote $captured image(s) to $out"
    exit 0
fi

if [[ "$command" == "accept" ]]; then
    mkdir -p "$images_dir"
    cp "$out"/*.png "$images_dir/"
    echo "golden: re-recorded $captured image(s) in tests/golden/images"
    echo "golden: the pull request has to say what changed in them and why."
    exit 0
fi

# ---- check ------------------------------------------------------------------
status=0
missing=()
differing=()

for image in "$out"/*.png; do
    name="$(basename "$image")"
    expected="$images_dir/$name"

    if [[ ! -f "$expected" ]]; then
        missing+=("$name")
        status=1
        continue
    fi

    if ! cmp -s "$image" "$expected"; then
        differing+=("$name")
        # Kept, next to the recorded one, so the failure can be looked at
        # rather than only read about.
        cp "$image" "$images_dir/$name.actual.png"
        status=1
    fi
done

if [[ ${#missing[@]} -gt 0 ]]; then
    echo "golden: no recorded image for: ${missing[*]}" >&2
    echo "golden: run scripts/golden.sh accept if these cases are new." >&2
fi

if [[ ${#differing[@]} -gt 0 ]]; then
    echo "golden: ${#differing[@]} image(s) differ: ${differing[*]}" >&2
    echo "golden: the rendering next to each is written as <name>.png.actual.png." >&2
    echo "golden: if tst_environment also failed, the machine changed and not the app —" >&2
    echo "golden: re-record rather than reading the diffs." >&2
fi

if [[ $status -eq 0 ]]; then
    echo "golden: ok — $captured image(s) match"
fi

exit $status
