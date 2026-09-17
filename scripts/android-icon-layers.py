#!/usr/bin/env python3
# SPDX-FileCopyrightText: 2026 Jowi Aoun
# SPDX-License-Identifier: GPL-3.0-or-later
"""Cuts the two layers of the Android adaptive icon out of climat.svg.

    android-icon-layers.py packaging/icons/climat.svg <outdir>

Writes <outdir>/background.svg and <outdir>/foreground.svg, each a 108-unit
square, and <outdir>/feature.svg, the 1024 by 500 Play Store feature graphic.
The square is the adaptive icon canvas: the launcher masks it to a circle,
a squircle or whatever the phone's maker chose, and only the middle 66 units
are promised to survive every mask.

The background is the sky gradient, full bleed, so the mask always cuts sky.
The foreground is everything else in the master - stars, halo, sun, cloud -
with the master's 448-unit tile mapped onto the 66-unit safe zone. The cloud
runs past the tile's edge in the master and so runs past the safe zone here;
that is the same bleed, cut by a different shape. Nothing here is drawn by
hand: change climat.svg and both layers follow.

scripts/icons.sh runs this and rasterises the result. Plain string work on a
file whose shape we own, rather than an XML parser: the devshell's python has
no expat, and a packager should have nothing to install.
"""
import re
import sys

HEADER = '<?xml version="1.0" encoding="UTF-8"?>\n'
CANVAS = '<svg xmlns="http://www.w3.org/2000/svg" width="108" height="108" viewBox="0 0 108 108">'
SCENE_OPEN = '<g clip-path="url(#tile)">'


def main(master, outdir):
    with open(master, encoding="utf-8") as handle:
        text = handle.read()
    # Comments are for the reader of the master, not for a rasteriser.
    text = re.sub(r"<!--.*?-->", "", text, flags=re.S)

    try:
        defs = text[text.index("<defs>"):text.index("</defs>") + len("</defs>")]
        scene = text[text.index(SCENE_OPEN) + len(SCENE_OPEN):text.rindex("</g>")]
    except ValueError:
        sys.exit("android-icon-layers: climat.svg has no <defs> or no clipped scene <g>")

    drawing, sky = re.subn(r'\s*<rect[^>]*fill="url\(#sky\)"/>', "", scene)
    if sky != 1:
        sys.exit(f"android-icon-layers: expected one sky rect in the scene, found {sky}")

    background = f"""{HEADER}{CANVAS}
  {defs}
  <rect width="108" height="108" fill="url(#sky)"/>
</svg>
"""
    # 66 / 448, then the tile's own origin taken off first. Six digits, so the
    # file is the same bytes on every machine.
    foreground = f"""{HEADER}{CANVAS}
  {defs}
  <g transform="translate(21 21) scale(0.147321) translate(-32 -32)">{drawing}
  </g>
</svg>
"""
    # The feature graphic: the whole scene, sun and cloud and stars, on the
    # sky, at 360 of the 500 units of height and centred. No words - Play
    # prints the name beside it, and a graphic that repeats it is a graphic
    # that says it twice.
    feature = f"""{HEADER}<svg xmlns="http://www.w3.org/2000/svg" width="1024" height="500" viewBox="0 0 1024 500">
  {defs}
  <rect width="1024" height="500" fill="url(#sky)"/>
  <g transform="translate(332 70) scale(0.803571) translate(-32 -32)">{drawing}
  </g>
</svg>
"""
    for name, content in (("background", background), ("foreground", foreground),
                          ("feature", feature)):
        with open(f"{outdir}/{name}.svg", "w", encoding="utf-8") as handle:
            handle.write(content)


if __name__ == "__main__":
    if len(sys.argv) != 3:
        sys.exit(__doc__)
    main(sys.argv[1], sys.argv[2])
