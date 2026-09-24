#!/usr/bin/env python3
# SPDX-FileCopyrightText: 2026 Project Tick
# SPDX-FileContributor: Project Tick
# SPDX-License-Identifier: Apache-2.0
"""Recolours the cat model's own texture (textures/cat_calico.png, from
assets/cat.glb -- see assets/CREDITS.md) into the other coat variants.

Every pixel is sorted into a coat class by its colour -- light grey fur,
orange/cream patches, brown, dark patches -- and repainted with that
variant's colour for the class, keeping each pixel's relative brightness so
the original shading survives. Eyes, the nose and the paw pads are left
alone except where a coat needs different eyes to stay visible.

    python3 tools/gen_cat_variants.py      (run from launcher/qml/Cat)
"""
import colorsys
from pathlib import Path

from PIL import Image

HERE = Path(__file__).resolve().parent.parent
SRC = HERE / "textures" / "cat_calico.png"

# Face row with the eyes (x, y) -> eye pixel; recoloured per variant only.
EYES = {(6, 35): "pupil", (8, 35): "pupil", (9, 35): "iris", (5, 35): "iris"}
KEEP = {(169, 116, 116), (248, 221, 114)}  # nose, paw pads

VARIANTS = {
    #          light fur        patches          cream            brown           dark            pupil           iris
    "ginger":  ((236, 166, 92), (200, 112, 48), (245, 205, 150), (176, 104, 52), (150, 82, 36), (40, 30, 20), (120, 170, 60)),
    "black":   ((50, 48, 54),   (40, 38, 44),   (62, 60, 66),    (38, 36, 42),   (26, 25, 30),  (214, 222, 70), (170, 210, 60)),
    "white":   ((236, 236, 236), (222, 216, 208), (242, 234, 222), (200, 196, 190), (172, 168, 162), (30, 30, 34), (82, 171, 188)),
    "siamese": ((238, 226, 204), (214, 196, 168), (242, 232, 214), (120, 96, 74),  (84, 66, 52),   (30, 40, 80), (70, 130, 210)),
}


def coat_class(rgb):
    r, g, b = (c / 255 for c in rgb)
    h, l, s = colorsys.rgb_to_hls(r, g, b)
    if l < 0.30:
        return "dark", l
    if s < 0.12:
        return "light", l
    if l > 0.68:
        return "cream", l
    if h * 360 < 55 and s > 0.35:
        return "patch", l
    return "brown", l


REF = {"light": 200 / 255 * 0.99, "patch": 0.54, "cream": 0.70, "brown": 0.46, "dark": 0.23}


def shade(colour, l, ref):
    k = max(0.6, min(1.4, l / ref if ref else 1.0))
    return tuple(max(0, min(255, round(c * k))) for c in colour)


def main():
    src = Image.open(SRC).convert("RGBA")
    for name, (light, patch, cream, brown, dark, pupil, iris) in VARIANTS.items():
        out = src.copy()
        table = {"light": light, "patch": patch, "cream": cream, "brown": brown, "dark": dark}
        for y in range(src.height):
            for x in range(src.width):
                r, g, b, a = src.getpixel((x, y))
                if a == 0 or (r, g, b) in KEEP:
                    continue
                eye = EYES.get((x, y))
                if eye:
                    out.putpixel((x, y), (*(pupil if eye == "pupil" else iris), a))
                    continue
                cls, l = coat_class((r, g, b))
                out.putpixel((x, y), (*shade(table[cls], l, REF[cls]), a))
        out.save(HERE / "textures" / f"cat_{name}.png")
        print("wrote", f"cat_{name}.png")


if __name__ == "__main__":
    main()
