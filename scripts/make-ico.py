#!/usr/bin/env python3
# SPDX-FileCopyrightText: 2026 Jowi Aoun
# SPDX-License-Identifier: GPL-3.0-or-later
"""Assemble a Windows .ico from the PNG sizes scripts/icons.sh already renders.

    scripts/make-ico.py <icons-dir> <output.ico>

Why this exists rather than a dependency: an .ico is a six-byte header, a
sixteen-byte directory entry per image, and then the images. Since Windows
Vista those images may be PNG files stored verbatim, which is exactly what we
already have on disk — so the whole format is forty lines of struct.pack, and
the alternative is asking every contributor to install ImageMagick to convert
eight PNGs into one file.

It is deliberately not a rasteriser. It cannot invent a size that is not
already beside it, which keeps clima.svg the single master: change the drawing,
run `scripts/icons.sh render`, and the .ico follows because its inputs did.
"""

import pathlib
import struct
import sys

# What Windows actually asks for. 16 is the title bar and the taskbar at 100%,
# 32 the desktop, 48 the "medium icons" view, 256 the extra-large view and the
# installer. The intermediate sizes are for fractional scaling, where Windows
# picks the nearest and downsamples rather than scaling 16 up.
SIZES = [16, 24, 32, 48, 64, 128, 256]


def main() -> int:
    if len(sys.argv) != 3:
        print(__doc__.strip(), file=sys.stderr)
        return 2

    icons_dir = pathlib.Path(sys.argv[1])
    output = pathlib.Path(sys.argv[2])

    images = []
    for size in SIZES:
        png = icons_dir / f"clima-{size}.png"
        if not png.is_file():
            print(f"make-ico: {png} is missing — run scripts/icons.sh render", file=sys.stderr)
            return 1
        images.append((size, png.read_bytes()))

    # ICONDIR: reserved, type 1 (icon, as opposed to 2 for a cursor), count.
    header = struct.pack("<HHH", 0, 1, len(images))

    # Each directory entry is a fixed sixteen bytes, so the first image starts
    # after all of them and the offsets can be computed in one pass.
    offset = len(header) + 16 * len(images)

    entries = b""
    blob = b""
    for size, data in images:
        # 256 is written as 0. The field is one byte, so 256 does not fit, and
        # this is the escape the format defines for it. Getting it wrong makes
        # the largest icon silently 0x0 and Windows drops it.
        dimension = 0 if size >= 256 else size

        entries += struct.pack(
            "<BBBBHHII",
            dimension,   # width
            dimension,   # height
            0,           # palette size; 0 for truecolour
            0,           # reserved
            1,           # colour planes
            32,          # bits per pixel
            len(data),
            offset,
        )
        offset += len(data)
        blob += data

    output.write_bytes(header + entries + blob)
    print(f"icons: {output.name} ({len(images)} sizes, {len(header + entries + blob)} bytes)")
    return 0


if __name__ == "__main__":
    sys.exit(main())
