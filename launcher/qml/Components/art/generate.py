#!/usr/bin/env python3
# SPDX-FileCopyrightText: 2026 Project Tick
# SPDX-FileContributor: Project Tick
# SPDX-License-Identifier: Apache-2.0
"""
Generates every original pixel-art asset MeshMC's QML ships as PNGs:
block textures, the chrome-only-screen ambient tile, per-item landscape
fallbacks (24 scenes at three shapes, drawn by scenes.py), the Home page
hero banner and a handful of empty-state illustrations.

Original artwork only -- no Mojang/Minecraft assets are read, copied or
referenced; palettes and shapes below are this script's own, only evoking
the same blocky, low-resolution style MeshMC's design direction calls for
(design-plan.md "Imagery and copy rules").

Deterministic: every image is built from a fixed random.Random(seed), so
re-running this script reproduces byte-identical PNGs. That is also why the
output is checked into git rather than generated at build or run time -- see
each QML consumer's own comment for why no QML code ever runs this at
runtime.

Usage: python3 generate.py (from anywhere; paths below are relative to this
file, not the working directory).
"""

import os
import random
from PIL import Image

import scenes

ROOT = os.path.dirname(os.path.abspath(__file__))


def save(img: Image.Image, *parts: str) -> None:
    path = os.path.join(ROOT, *parts)
    os.makedirs(os.path.dirname(path), exist_ok=True)
    img.save(path, optimize=True)
    print("wrote", os.path.relpath(path, ROOT), img.size)


def upscale(img: Image.Image, factor: int) -> Image.Image:
    """Nearest-neighbour upscale -- bakes the blocky look into the file
    itself, on top of whatever further nearest-neighbour scaling QML does
    at display time (smooth: false)."""
    return img.resize((img.width * factor, img.height * factor), Image.NEAREST)


def jitter(rng: random.Random, color, spread=10):
    return tuple(max(0, min(255, c + rng.randint(-spread, spread))) for c in color)


# ---------------------------------------------------------------------------
# 1. 16x16 block textures
# ---------------------------------------------------------------------------

def speckle(size, base, dark, light, seed, dark_p=0.18, light_p=0.12, spread=6):
    rng = random.Random(seed)
    img = Image.new("RGB", (size, size))
    for y in range(size):
        for x in range(size):
            r = rng.random()
            if r < dark_p:
                c = jitter(rng, dark, spread)
            elif r < dark_p + light_p:
                c = jitter(rng, light, spread)
            else:
                c = jitter(rng, base, spread)
            img.putpixel((x, y), c)
    return img


def block_grass_top(seed):
    return speckle(16, (0x6A, 0xA3, 0x49), (0x54, 0x86, 0x39), (0x86, 0xC1, 0x5B), seed,
                    dark_p=0.16, light_p=0.14, spread=8)


def block_grass_side(seed):
    rng = random.Random(seed)
    img = Image.new("RGB", (16, 16))
    dirt_base, dirt_dark, dirt_light = (0x8B, 0x5A, 0x2B), (0x6E, 0x46, 0x20), (0x9C, 0x6A, 0x38)
    grass_base, grass_dark = (0x6A, 0xA3, 0x49), (0x54, 0x86, 0x39)
    # A jagged 3-4px grass fringe over dirt, not a straight cut.
    edge = [4 + rng.randint(-1, 1) for _ in range(16)]
    for x in range(16):
        for y in range(16):
            if y < edge[x] - 1:
                c = jitter(rng, grass_base, 8) if rng.random() > 0.2 else jitter(rng, grass_dark, 8)
            elif y < edge[x]:
                c = jitter(rng, grass_dark, 6)
            else:
                r = rng.random()
                c = jitter(rng, dirt_dark if r < 0.18 else dirt_light if r < 0.30 else dirt_base, 6)
            img.putpixel((x, y), c)
    return img


def block_dirt(seed):
    return speckle(16, (0x8B, 0x5A, 0x2B), (0x6E, 0x46, 0x20), (0x9C, 0x6A, 0x38), seed,
                    dark_p=0.20, light_p=0.14, spread=8)


def block_stone(seed):
    return speckle(16, (0x8A, 0x8A, 0x8E), (0x74, 0x74, 0x78), (0x9C, 0x9C, 0xA0), seed,
                    dark_p=0.16, light_p=0.12, spread=6)


def block_cobblestone(seed):
    rng = random.Random(seed)
    img = Image.new("RGB", (16, 16), (0x4B, 0x4B, 0x4E))
    base, dark, light = (0x8A, 0x8A, 0x8E), (0x6E, 0x6E, 0x72), (0x9E, 0x9E, 0xA2)
    # A handful of rounded cobble blobs with dark mortar showing between them.
    centers = [(rng.randint(1, 14), rng.randint(1, 14), rng.randint(2, 3)) for _ in range(9)]
    for x in range(16):
        for y in range(16):
            hit = None
            for cx, cy, r in centers:
                if (x - cx) ** 2 + (y - cy) ** 2 <= r * r:
                    hit = (cx, cy, r)
            if hit:
                cx, cy, r = hit
                shade = base if (x + y + cx) % 3 else (dark if (x + y) % 2 else light)
                img.putpixel((x, y), jitter(rng, shade, 5))
    return img


def block_oak_planks(seed):
    rng = random.Random(seed)
    img = Image.new("RGB", (16, 16))
    base, grain, edge = (0xB9, 0x86, 0x50), (0xA5, 0x74, 0x42), (0x8F, 0x62, 0x38)
    for y in range(16):
        row_edge = (y % 4 == 0)
        for x in range(16):
            if row_edge:
                c = edge
            elif rng.random() < 0.12:
                c = grain
            else:
                c = base
            img.putpixel((x, y), jitter(rng, c, 5))
    return img


def block_deepslate(seed):
    return speckle(16, (0x3F, 0x41, 0x47), (0x2E, 0x30, 0x35), (0x50, 0x53, 0x5A), seed,
                    dark_p=0.20, light_p=0.12, spread=6)


def block_sand(seed):
    return speckle(16, (0xDC, 0xC9, 0x8B), (0xC8, 0xB3, 0x74), (0xE8, 0xD9, 0xA0), seed,
                    dark_p=0.16, light_p=0.14, spread=6)


def block_gravel(seed):
    return speckle(16, (0x8D, 0x8A, 0x87), (0x6A, 0x67, 0x64), (0xAB, 0xA6, 0xA0), seed,
                    dark_p=0.26, light_p=0.22, spread=14)


BLOCKS = {
    "grass_top": block_grass_top,
    "grass_side": block_grass_side,
    "dirt": block_dirt,
    "stone": block_stone,
    "cobblestone": block_cobblestone,
    "oak_planks": block_oak_planks,
    "deepslate": block_deepslate,
    "sand": block_sand,
    "gravel": block_gravel,
}


def generate_blocks():
    for i, (name, fn) in enumerate(BLOCKS.items()):
        save(fn(seed=1000 + i), "blocks", f"{name}.png")


# ---------------------------------------------------------------------------
# 1b. Ambient wash mask -- a 2x2 dirt/deepslate checkerboard, converted to a
#     luminance alpha mask so QML's IconImage (the same alpha-channel
#     recolouring MeshIcon.qml already uses for every SVG icon, no
#     ShaderEffect/MultiEffect involved) can tint it to any palette colour
#     while its own speckle/grain still reads as texture, not a flat square.
# ---------------------------------------------------------------------------

def _luminance_alpha(img, curve=lambda l: l):
    """RGB image -> RGBA mask: alpha = curve(luminance), RGB = white (the
    RGB channel is irrelevant, since IconImage overwrites it with its own
    `color` and only reads this image's alpha)."""
    w, h = img.size
    src = img.load()
    out = Image.new("RGBA", (w, h))
    dst = out.load()
    for y in range(h):
        for x in range(w):
            r, g, b = src[x, y]
            lum = (r * 0.299 + g * 0.587 + b * 0.114) / 255
            a = max(0, min(255, round(255 * curve(lum))))
            dst[x, y] = (255, 255, 255, a)
    return out


def ambient_tile():
    dirt = block_dirt(seed=5000)
    deepslate = block_deepslate(seed=5001)
    tile = Image.new("RGB", (32, 32))
    tile.paste(dirt, (0, 0))
    tile.paste(deepslate, (16, 0))
    tile.paste(deepslate, (0, 16))
    tile.paste(dirt, (16, 16))
    return _luminance_alpha(tile, curve=lambda l: 0.4 + l * 0.6)


def generate_ambient():
    save(ambient_tile(), "ambient", "blocks_mask.png")


# ---------------------------------------------------------------------------
# 2. Landscape fallbacks -- 24 scenes x 3 shapes (scenes.py), nearest-upscaled
#    2x. See scenes.py for why one scene is drawn three times.
# ---------------------------------------------------------------------------

def generate_landscapes():
    for i in range(scenes.SCENE_COUNT):
        for shape in scenes.SHAPES:
            save(upscale(scenes.render(i, shape), 2), "landscapes", shape, f"{i:02d}.png")


# Helpers the hero banner below still uses.

def draw_sky(img, w, h, bands):
    for i, (pos, color) in enumerate(bands):
        y0 = int(pos * h)
        y1 = int(bands[i + 1][0] * h) if i + 1 < len(bands) else h
        for y in range(y0, y1):
            for x in range(w):
                img.putpixel((x, y), color)


def draw_hillband(img, w, h, base_y, amplitude, color, rng, step=3):
    heights = []
    cur = base_y
    for x in range(0, w + step, step):
        cur += rng.randint(-amplitude, amplitude)
        cur = max(base_y - amplitude * 2, min(h - 2, cur))
        heights.extend([cur] * step)
    for x in range(w):
        top = heights[x] if x < len(heights) else heights[-1]
        for y in range(int(top), h):
            img.putpixel((x, y), color)


# ---------------------------------------------------------------------------
# 3. Home hero banner -- logical 96x18 grid, nearest-upscaled 5x to 480x90.
# ---------------------------------------------------------------------------

def hero_banner(seed=3000):
    w, h = 96, 18
    img = Image.new("RGB", (w, h))
    rng = random.Random(seed)
    sky = [(0.00, (0x35, 0x40, 0x64)), (0.35, (0x5C, 0x5C, 0x86)), (0.60, (0x9A, 0x76, 0x86)), (0.82, (0xD4, 0x9A, 0x6C))]
    draw_sky(img, w, h, sky)

    # A quiet moon/sun disc, off-centre.
    ax, ay, r = int(w * 0.74), 4, 2
    for dx in range(-r, r + 1):
        for dy in range(-r, r + 1):
            if dx * dx + dy * dy <= r * r + 1:
                xx, yy = ax + dx, ay + dy
                if 0 <= xx < w and 0 <= yy < h:
                    img.putpixel((xx, yy), (0xF0, 0xD8, 0xA6))

    horizon = int(h * 0.60)
    draw_hillband(img, w, h, horizon, 1, (0x2E, 0x33, 0x38), rng, step=3)
    draw_hillband(img, w, h, horizon + 2, 1, (0x20, 0x24, 0x28), rng, step=5)
    for y in range(horizon + 4, h):
        for x in range(w):
            img.putpixel((x, y), (0x16, 0x19, 0x1C))

    # A distant, blocky silhouette skyline -- towers, not trees, so the
    # banner reads as "a place with structures", not another landscape tile.
    x = 4
    while x < w - 4:
        bw = rng.randint(2, 4)
        bh = rng.randint(2, 5)
        by = horizon + 3 - bh
        for xx in range(x, min(w, x + bw)):
            for yy in range(by, horizon + 3):
                if 0 <= yy < h:
                    img.putpixel((xx, yy), (0x1C, 0x20, 0x24))
        x += bw + rng.randint(2, 5)

    return upscale(img, 5)


def generate_hero():
    save(hero_banner(), "hero", "home_hero.png")


# ---------------------------------------------------------------------------
# 4. Empty-state illustrations -- logical 24x24 grid, nearest-upscaled 2x.
# ---------------------------------------------------------------------------

def empty_no_instances(seed=4000):
    # A single lidless crafting-table-like block with a "+" carved in, on a
    # short ground strip -- reads as "place a new one here".
    w = h = 24
    img = Image.new("RGBA", (w, h), (0, 0, 0, 0))
    rng = random.Random(seed)
    base, dark, light = (0x9C, 0x6A, 0x38), (0x7A, 0x50, 0x28), (0xB8, 0x86, 0x50)
    for y in range(14, 20):
        for x in range(4, 20):
            img.putpixel((x, y), jitter(rng, base, 6) + (255,))
    for x in range(4, 20):
        img.putpixel((x, 14), dark + (255,))
    # ground shadow
    for x in range(3, 21):
        img.putpixel((x, 20), (0, 0, 0, 60))
    # a plus mark
    for d in range(-3, 4):
        img.putpixel((12 + d, 9), light + (255,))
        img.putpixel((12, 9 + d), light + (255,))
    return upscale(img, 2)


def empty_no_worlds(seed=4001):
    # A small pixel globe/compass -- a blank map with a dashed border.
    w = h = 24
    img = Image.new("RGBA", (w, h), (0, 0, 0, 0))
    rng = random.Random(seed)
    paper, edge = (0xE7, 0xD9, 0xAE), (0xB8, 0x9F, 0x6C)
    for y in range(4, 20):
        for x in range(4, 20):
            img.putpixel((x, y), jitter(rng, paper, 5) + (255,))
    for x in range(4, 20):
        img.putpixel((x, 4), edge + (255,))
        img.putpixel((x, 19), edge + (255,))
    for y in range(4, 20):
        img.putpixel((4, y), edge + (255,))
        img.putpixel((19, y), edge + (255,))
    # a dashed compass needle
    for i, (x, y) in enumerate([(11, 9), (12, 10), (12, 11), (11, 12), (10, 13), (11, 14)]):
        img.putpixel((x, y), (0xB0, 0x4A, 0x3A, 255) if i % 2 else (0x3A, 0x5C, 0xB0, 255))
    return upscale(img, 2)


def empty_not_found(seed=4002):
    # A cracked stone block with a "?" notch -- nothing matched.
    w = h = 24
    img = Image.new("RGBA", (w, h), (0, 0, 0, 0))
    rng = random.Random(seed)
    base, dark = (0x8A, 0x8A, 0x8E), (0x66, 0x66, 0x6A)
    for y in range(4, 20):
        for x in range(4, 20):
            img.putpixel((x, y), jitter(rng, base, 5) + (255,))
    # a jagged crack
    cx = 12
    for y in range(4, 20):
        cx += rng.choice([-1, 0, 0, 1])
        cx = max(6, min(17, cx))
        img.putpixel((cx, y), dark + (255,))
    return upscale(img, 2)


def generate_empty_states():
    save(empty_no_instances(), "empty", "no_instances.png")
    save(empty_no_worlds(), "empty", "no_worlds.png")
    save(empty_not_found(), "empty", "not_found.png")


if __name__ == "__main__":
    generate_blocks()
    generate_ambient()
    generate_landscapes()
    generate_hero()
    generate_empty_states()
