#!/usr/bin/env python3
# SPDX-FileCopyrightText: 2026 Project Tick
# SPDX-FileContributor: Project Tick
# SPDX-License-Identifier: Apache-2.0
"""
Builds the fallback cover scenes PixelArt.landscapeUrl() serves: 32 rendered
voxel-world scenes, each cut into the three crops CoverArt picks between by
the shape of its host (PixelArt.shapeForAspect()):

    card    16:9   768x432   grid/jump cards, Library covers
    band     5:1  1600x320   instance-page hero
    strip   12:1  1920x160   PlayDock's bar

The source renders (1920x1280, one per scene, named NN.png) are not checked
in -- only this script and its output are. Every crop spans the full source
width; what differs per scene is where each crop sits vertically, chosen by
eye below so the thin strip still holds the horizon and the scene's subject
rather than a slice of plain sky or ground. Each value is the crop's centre
line as a fraction of the source height; a crop that would run past an edge
is pushed back inside it.

The same renders also feed three smaller sets, each skipped when its source
is absent:

    worlds/<biome>.jpg      256x256   PixelArt.worldUrlFor(): a world's thumbnail
                                      (source w_<biome>.png, subject centred)
    packs/NN.jpg           1280x720   PixelArt.packCoverUrl(): Discover's generic
                                      modpack cover (source p_NN.png)
    ../empty/<name>.jpg     480x360   PixelArt.emptyUrl(): empty-state art
                                      (source e_<name>.png)

Usage: python3 build.py <folder with 00.png .. 31.png, w_*.png, p_*.png, e_*.png> [more folders]
Writes card/, band/ and strip/ next to this file, worlds/, packs/ and empty/ in
the art/ folder above it. Scenes whose source render is in none of the folders
are left as they are.
"""

import os
import sys
from PIL import Image

HERE = os.path.dirname(os.path.abspath(__file__))

SHAPES = {"card": (768, 432), "band": (1600, 320), "strip": (1920, 160)}
QUALITY = 84

# name, what it shows, centre line of the card / band / strip crop.
SCENES = [
    ("00", "plains village, noon", 0.52, 0.50, 0.52),
    ("01", "snowy spruce taiga, dawn", 0.50, 0.47, 0.42),
    ("02", "desert pyramid, golden hour", 0.45, 0.42, 0.44),
    ("03", "jungle river, misty morning", 0.50, 0.55, 0.56),
    ("04", "tropical beach and islets, midday", 0.45, 0.46, 0.43),
    ("05", "badlands mesa canyon, late afternoon", 0.52, 0.48, 0.51),
    ("06", "cherry blossom grove, spring morning", 0.50, 0.53, 0.56),
    ("07", "foggy swamp, overcast", 0.50, 0.50, 0.53),
    ("08", "peaks above the clouds, sunrise", 0.45, 0.39, 0.36),
    ("09", "autumn birch forest, afternoon", 0.50, 0.54, 0.59),
    ("10", "torch-lit cave, lava and ores", 0.50, 0.53, 0.56),
    ("11", "moonlit lakeside cabin, night", 0.45, 0.50, 0.53),
    ("12", "savanna acacias and a hut, sunset", 0.50, 0.48, 0.47),
    ("13", "ice spikes under an aurora, night", 0.45, 0.45, 0.48),
    ("14", "mushroom island, pastel overcast", 0.45, 0.42, 0.45),
    ("15", "dark oak forest in rain, lit cabin", 0.50, 0.50, 0.53),
    ("16", "flower meadow with a windmill, morning", 0.45, 0.35, 0.38),
    ("17", "stony shore with a lighthouse, evening", 0.40, 0.33, 0.29),
    ("18", "ocean monument at the waterline, midday", 0.42, 0.42, 0.44),
    ("19", "nether fortress over a lava sea", 0.45, 0.36, 0.38),
    ("20", "warped forest, nether", 0.45, 0.45, 0.40),
    ("21", "crimson forest, nether", 0.45, 0.45, 0.42),
    ("22", "basalt deltas, nether", 0.50, 0.50, 0.55),
    ("23", "end city on floating islands", 0.42, 0.40, 0.42),
    ("24", "ancient city, deep dark", 0.50, 0.50, 0.52),
    ("25", "lush cave with glow berries", 0.48, 0.45, 0.50),
    ("26", "castle on a hill, dusk", 0.45, 0.35, 0.40),
    ("27", "harbour town, moonlit night", 0.55, 0.58, 0.60),
    ("28", "snowy mountain village, evening", 0.50, 0.50, 0.52),
    ("29", "bamboo jungle clearing with a shrine", 0.50, 0.50, 0.55),
    ("30", "sunflower plains, sunrise", 0.50, 0.56, 0.50),
    ("31", "river canyon with a rope bridge, afternoon", 0.50, 0.52, 0.55),
]

# Worlds' thumbnails: biome, horizontal centre of the square crop.
WORLDS = [
    ("plains", 0.5), ("desert", 0.46), ("snow", 0.66), ("jungle", 0.5),
    ("ocean", 0.5), ("cave", 0.6), ("nether", 0.5), ("end", 0.5),
]
WORLD_SIZE = 256
PACKS = ["00", "01", "02"]
PACK_SIZE = (1280, 720)
# name, crop centre x / y and zoom: the small subjects sit in plain surroundings,
# so the crop is pulled in until they read at 480x360.
EMPTY = [
    ("no_instances", 0.50, 0.56, 1.3),
    ("no_worlds", 0.50, 0.58, 1.2),
    ("not_found", 0.50, 0.59, 1.6),
]
EMPTY_SIZE = (480, 360)


def crop(img, shape, centre):
    out_w, out_h = SHAPES[shape]
    w, h = img.size
    ch = round(w * out_h / out_w)
    top = min(max(0, round(centre * h - ch / 2)), h - ch)
    return img.crop((0, top, w, top + ch)).resize((out_w, out_h), Image.LANCZOS)


def find(raw, name):
    for folder in raw:
        path = os.path.join(folder, name + ".png")
        if os.path.exists(path):
            return Image.open(path).convert("RGB")
    return None


def fit(img, size, cx=0.5, cy=0.5, zoom=1.0):
    """Largest crop of the aspect of @p size (shrunk by @p zoom) centred on
    (cx, cy), resized."""
    w, h = img.size
    out_w, out_h = size
    cw, ch = (w, round(w * out_h / out_w)) if w * out_h <= h * out_w \
        else (round(h * out_w / out_h), h)
    cw, ch = round(cw / zoom), round(ch / zoom)
    left = min(max(0, round(cx * w - cw / 2)), w - cw)
    top = min(max(0, round(cy * h - ch / 2)), h - ch)
    return img.crop((left, top, left + cw, top + ch)).resize(size, Image.LANCZOS)


def save(img, path):
    os.makedirs(os.path.dirname(path), exist_ok=True)
    img.save(path, "JPEG", quality=QUALITY, optimize=True, progressive=True)
    print(path)


def main():
    if len(sys.argv) < 2:
        sys.exit("usage: python3 build.py <render folder> [more folders]")
    raw = sys.argv[1:]
    for name, _label, *centres in SCENES:
        img = find(raw, name)
        if img is None:
            continue
        for shape, centre in zip(SHAPES, centres):
            save(crop(img, shape, centre), os.path.join(HERE, shape, name + ".jpg"))
    art = os.path.dirname(HERE)
    for biome, cx in WORLDS:
        img = find(raw, "w_" + biome)
        if img is not None:
            save(fit(img, (WORLD_SIZE, WORLD_SIZE), cx), os.path.join(art, "worlds", biome + ".jpg"))
    for name in PACKS:
        img = find(raw, "p_" + name)
        if img is not None:
            save(fit(img, PACK_SIZE), os.path.join(art, "packs", name + ".jpg"))
    for name, cx, cy, zoom in EMPTY:
        img = find(raw, "e_" + name)
        if img is not None:
            save(fit(img, EMPTY_SIZE, cx, cy, zoom), os.path.join(art, "empty", name + ".jpg"))


if __name__ == "__main__":
    main()
