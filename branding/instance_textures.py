# SPDX-FileCopyrightText: 2026 grxtor
# SPDX-License-Identifier: Apache-2.0
"""The 16x16 textures of Kite's instance blocks, drawn from palettes and
seeded noise (see instance_cubes.py)."""
import numpy as np

TEXELS = 16


def rgb(hex_colour):
    h = hex_colour.lstrip('#')
    return np.array([int(h[i:i + 2], 16) for i in (0, 2, 4)], dtype=float)


def noise(seed, smooth=0.6, grain=None):
    """Seeded noise, optionally stretched along rows ('h') or columns ('v')."""
    v = np.random.default_rng(seed).random((TEXELS, TEXELS))
    if grain:
        axis = 1 if grain == 'h' else 0
        v = (v + np.roll(v, 1, axis) + np.roll(v, -1, axis) + 0.5 * np.roll(v, 2, axis)) / 3.5
    if smooth:
        around = (np.roll(v, 1, 0) + np.roll(v, -1, 0) + np.roll(v, 1, 1) + np.roll(v, -1, 1)) / 4
        v = (v + smooth * around) / (1 + smooth)
    return v


def paint(palette, values):
    """Each texel takes the palette colour of its rank, dark to light."""
    flat = values.flatten()
    ranks = np.empty(flat.size, dtype=int)
    ranks[flat.argsort()] = np.arange(flat.size)
    level = (ranks * len(palette) // flat.size).reshape(values.shape)
    t = np.zeros((TEXELS, TEXELS, 3))
    for i, colour in enumerate(palette):
        t[level == i] = rgb(colour)
    return t


def tex(palette, seed, smooth=0.6, grain=None):
    return paint(palette, noise(seed, smooth, grain))


def cells(seed, count):
    """Tileable Voronoi cells: which cell owns each texel, and how near it
    lies to the next one."""
    pts = np.random.default_rng(seed).random((count, 2)) * TEXELS
    yy, xx = np.mgrid[0:TEXELS, 0:TEXELS] + 0.5
    d = np.zeros((count, TEXELS, TEXELS))
    for i, (px, py) in enumerate(pts):
        dx = np.abs(xx - px)
        dy = np.abs(yy - py)
        d[i] = np.hypot(np.minimum(dx, TEXELS - dx), np.minimum(dy, TEXELS - dy))
    nearest = np.sort(d, axis=0)
    return d.argmin(0), nearest[1] - nearest[0]


STONE = ['#666666', '#727272', '#7d7d7d', '#878787', '#919191', '#9b9b9b']
DEEPSLATE = ['#2c2e33', '#34373d', '#3d4047', '#474a52', '#51545d']
DIRT = ['#5a3c27', '#694630', '#775237', '#865e40', '#966b49']
GRASS = ['#477a2a', '#528a31', '#5d9838', '#69a640', '#76b349', '#84bf53']
SNOW = ['#dfe8f1', '#e6eef5', '#edf3f8', '#f3f7fb']
VEIN_SHAPES = [
    [(0, 0), (1, 0), (0, 1), (1, 1), (2, 1)],
    [(1, 0), (0, 1), (1, 1), (2, 1), (1, 2)],
    [(0, 0), (1, 0), (1, 1), (2, 1), (2, 2)],
    [(0, 0), (1, 0), (2, 0), (1, 1), (0, 1)],
]


def ore(seed, base, mid, light, dark):
    """One vein per quarter, lit from the top left, over @p base rock."""
    t = tex(base, seed, 0.5, 'h' if base is DEEPSLATE else None)
    rng = np.random.default_rng(seed + 99)
    for qx in (0, 8):
        for qy in (0, 8):
            x, y = qx + int(rng.integers(1, 5)), qy + int(rng.integers(1, 5))
            vein = [(x + dx, y + dy) for dx, dy in VEIN_SHAPES[int(rng.integers(len(VEIN_SHAPES)))]]
            for px, py in vein:
                t[py, px] = rgb(mid)
            for px, py in vein:
                for nx, ny in ((px + 1, py), (px, py + 1)):
                    if nx < TEXELS and ny < TEXELS and (nx, ny) not in vein:
                        t[ny, nx] *= 0.78
            tl = min(vein, key=lambda p: p[0] + p[1])
            br = max(vein, key=lambda p: p[0] + p[1])
            t[tl[1], tl[0]] = rgb(light)
            t[br[1], br[0]] = rgb(dark)
    return t


def capped(seed, cap):
    """Dirt under a ragged cap of @p cap, the way grass and snow lie."""
    t = tex(DIRT, seed, 0.4)
    top = tex(cap, seed + 5, 0.5)
    rng = np.random.default_rng(seed)
    for x in range(TEXELS):
        depth = 3 + int(rng.integers(0, 3)) + (1 if x % 5 == 2 else 0)
        t[:depth, x] = top[:depth, x]
    return t


def cobblestone(seed):
    owner, gap = cells(seed, 10)
    shade = np.random.default_rng(seed).uniform(0.86, 1.08, 10)
    t = tex(['#6f6f6f', '#7b7b7b', '#878787', '#959595', '#a3a3a3'], seed + 1, 0.4) * shade[owner][..., None]
    t[gap < 1.0] = rgb('#4b4b4b')
    return t


def planks(seed):
    t = tex(['#8a6c43', '#977749', '#a38250', '#ae8c57', '#b8955d'], seed, 0.2, 'h')
    for row in range(3, TEXELS, 4):
        t[row, :] = rgb('#6a5233')
    for plank, x in enumerate((5, 12, 2, 9)):
        t[plank * 4:plank * 4 + 3, x] = rgb('#735a37')
    return t


def log_side(seed):
    t = tex(['#3b2c18', '#46351e', '#524024', '#5e4a2b', '#6b5532'], seed, 0.3, 'v')
    t[:, (3, 10)] *= 0.8
    return t


def log_top(seed):
    yy, xx = np.mgrid[0:TEXELS, 0:TEXELS]
    ring = np.floor(np.maximum(np.abs(xx - 7.5), np.abs(yy - 7.5))).astype(int)
    t = np.zeros((TEXELS, TEXELS, 3))
    for i in range(8):
        t[ring == i] = rgb('#b8955d' if i % 2 == 0 else '#9c7c4b')
    t[ring == 7] = log_side(seed)[ring == 7]
    return t * np.random.default_rng(seed).uniform(0.95, 1.05, (TEXELS, TEXELS, 1))


def brick(seed):
    rng = np.random.default_rng(seed)
    t = tex(['#8c877e', '#99938a', '#a59f95'], seed + 1, 0.2)
    clay = tex(['#87392a', '#953f2f', '#a24835', '#ae523d'], seed + 2, 0.3)
    for course in range(4):
        y = course * 4
        for b in range(-1, 3):
            x = b * 8 + (4 if course % 2 else 0)
            lo, hi = max(x, 0), min(x + 7, TEXELS)
            if lo < hi:
                t[y:y + 3, lo:hi] = clay[y:y + 3, lo:hi] * rng.uniform(0.88, 1.1)
    return t


def ice(seed):
    t = tex(['#7aa3e6', '#88aeee', '#96baf3', '#a6c6f8', '#b8d3fc'], seed, 0.6)
    for y in range(TEXELS):
        if y % 5 != 4:
            t[y, [(y + 2) % TEXELS, (y + 9) % TEXELS]] = rgb('#dbe9ff')
    return t


def magma(seed):
    _, gap = cells(seed, 9)
    t = tex(['#30100b', '#3c140e', '#4a1912', '#571f16'], seed, 0.5)
    glow = tex(['#d9501a', '#ee7a22', '#fba232', '#ffd04a'], seed + 1, 0.2)
    t[gap < 1.3] = glow[gap < 1.3] * 0.85
    t[gap < 0.6] = glow[gap < 0.6]
    return t


def obsidian(seed):
    t = tex(['#0c0912', '#130e1d', '#1b1429', '#251b38', '#33264c'], seed, 0.8)
    for x, y in np.random.default_rng(seed).integers(0, 15, (6, 2)):
        t[y, x], t[y, x + 1] = rgb('#5a4384'), rgb('#3d2c5c')
    return t


def amethyst(seed):
    yy, xx = np.mgrid[0:TEXELS, 0:TEXELS]
    facets = 0.9 * (((xx + 2 * yy) % 7) < 3) + 0.4 * (((2 * xx - yy) % 9) < 2)
    v = np.random.default_rng(seed).random((TEXELS, TEXELS)) + facets
    return paint(['#4f3080', '#5e3b96', '#6e48aa', '#8260c2', '#9a7ad6', '#b799ec'], v)


def lantern(seed):
    yy, xx = np.mgrid[0:TEXELS, 0:TEXELS]
    band = np.clip((np.hypot(xx - 7.5, yy - 7.5) / 7.5 * 3.2).astype(int), 0, 3)
    t = np.array([rgb(c) for c in ('#fff6c4', '#ffd86a', '#f5a83a', '#d9792a')])[band]
    frame = tex(['#26272b', '#2f3035', '#393a40', '#44454c'], seed, 0.3)
    border = (xx < 2) | (xx > 13) | (yy < 2) | (yy > 13)
    bars = ((xx == 7) | (xx == 8) | (yy == 7) | (yy == 8)) & ~border
    t[border] = frame[border]
    t[bars] = frame[bars] * 1.15
    return t


def copper(seed):
    t = tex(['#a85a35', '#b8663f', '#c6744a', '#d48356', '#e19363'], seed, 0.4)
    yy, xx = np.mgrid[0:TEXELS, 0:TEXELS]
    t[(xx % 8 == 7) | (yy % 8 == 7)] *= 0.72
    for cx, cy in np.random.default_rng(seed).integers(1, 14, (4, 2)):
        for dx, dy in ((0, 0), (1, 0), (0, 1), (1, 1), (2, 1)):
            t[cy + dy, min(cx + dx, TEXELS - 1)] = rgb('#4f9f86' if (dx + dy) % 2 else '#66b89a')
    return t


def glass(seed):
    """Lilac glass in the logo's colours, Kite's own block."""
    yy, xx = np.mgrid[0:TEXELS, 0:TEXELS]
    f = ((xx + yy) / 30.0)[..., None]
    t = (rgb('#b3a0ff') * (1 - f) + rgb('#6f50f0') * f) * np.random.default_rng(seed).uniform(0.96, 1.04, (TEXELS, TEXELS, 1))
    inner = (xx > 1) & (xx < 14) & (yy > 1) & (yy < 14)
    t[np.isin(xx + yy, (9, 10, 14)) & inner] = rgb('#e9e2ff')
    t[(xx == 0) | (yy == 0)] = rgb('#d9cfff')
    t[(xx == 15) | (yy == 15)] = rgb('#5a3fd6')
    return t


def blocks():
    same = lambda t: (t, t, t)  # noqa: E731
    return [
        ('moss', tex(GRASS, 6), capped(1, GRASS), capped(1, GRASS), False),
        ('cobblestone', *same(cobblestone(2)), False),
        ('log', log_top(3), log_side(3), log_side(3), False),
        ('planks', *same(planks(4)), False),
        ('brick', *same(brick(5)), False),
        ('sand', *same(tex(['#c8b77f', '#d2c28b', '#dacb96', '#e2d4a2', '#e9ddae'], 7, 0.25)), False),
        ('snow', tex(SNOW, 8, 0.4), capped(8, SNOW), capped(8, SNOW), False),
        ('ice', *same(ice(9)), False),
        ('magma', *same(magma(10)), True),
        ('obsidian', *same(obsidian(11)), False),
        ('cyan_ore', *same(ore(12, DEEPSLATE, '#5ee8e2', '#c9fff8', '#1d9a93')), False),
        ('gold_ore', *same(ore(13, STONE, '#f6d33c', '#fff3a0', '#b58a1d')), False),
        ('ruby_ore', *same(ore(14, DEEPSLATE, '#e2304a', '#ff9aa8', '#7a0f1f')), False),
        ('emerald_ore', *same(ore(15, STONE, '#2fd66b', '#a8ffc6', '#0f7a37')), False),
        ('amethyst', *same(amethyst(16)), False),
        ('lantern', *same(lantern(17)), True),
        ('copper', *same(copper(18)), False),
        ('glass', *same(glass(19)), True),
    ]
