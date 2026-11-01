#!/usr/bin/env python3
# SPDX-FileCopyrightText: 2026 grxtor
# SPDX-License-Identifier: Apache-2.0
"""Kite's own instance icons: isometric blocks, each wearing the KITE band
with a small kite on its right side.

Every texture is drawn in instance_textures.py from palettes and seeded
noise, so the same run always gives the same pixels; nothing is taken from
Minecraft. Writes
kite_<name>.png into the multimc icon theme at 32, 128 and 256 px.

usage: python3 branding/instance_cubes.py [launcher/resources/multimc]
needs: Pillow, numpy
"""
import os
import sys

import numpy as np
from PIL import Image, ImageDraw

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from instance_textures import TEXELS, blocks, rgb  # noqa: E402

LICENSE = 'SPDX-FileCopyrightText: 2026 grxtor\n\nSPDX-License-Identifier: Apache-2.0\n'
SUPERSAMPLE = 4
SIZES = (32, 128, 256)
EDGE = 0.47                      # cube edge, as a share of the canvas
LIGHT = (1.0, 0.80, 0.62)        # top, left, right face
GLOW_LIGHT = (1.0, 0.88, 0.76)   # blocks that give their own light
BAND_TOP, BAND_BOTTOM = 5, 11    # band rows; the bottom one is its dark edge
# gradient start, end, bottom edge, letters, kite's lit and shaded half
BAND = {'lilac': ('#a891ff', '#7657f7', '#4b33b8', '#f6f3ff', '#f6f3ff', '#c4b5ff'),
        # on a block that is lilac itself the band turns light
        'light': ('#f3efff', '#d6ccff', '#a891ff', '#6a4bea', '#7657f7', '#a891ff')}

FONT = {
    'K': ["#..#", "#.#.", "##..", "#.#.", "#..#"],
    'I': ["#", "#", "#", "#", "#"],
    'T': ["###", ".#.", ".#.", ".#.", ".#."],
    'E': ["###", "#..", "##.", "#..", "###"],
}
# the kite on the right side: row -> first and last column, then its string
KITE_ROWS = {3: (5, 5), 4: (4, 6), 5: (3, 7), 6: (2, 8), 7: (3, 7), 8: (3, 7), 9: (4, 6), 10: (5, 5)}
KITE_STRING = [(11, 5), (12, 6), (13, 6), (14, 5), (14, 7), (15, 4), (15, 8)]


def ribbon(t, half, band):
    """The band runs on round the front corner: the left face carries the
    first half of the gradient, the right face the second."""
    start, end, edge = (rgb(c) for c in BAND[band][:3])
    t = t.copy()
    for x in range(TEXELS):
        f = x / (TEXELS - 1) * 0.5 + (0.5 if half == 'right' else 0.0)
        t[BAND_TOP:BAND_BOTTOM, x] = start * (1 - f) + end * f
    t[BAND_TOP, :] = np.clip(t[BAND_TOP, :] * 1.18, 0, 255)
    t[BAND_BOTTOM, :] = edge
    return t


def labelled(t, band):
    t = ribbon(t, 'left', band)
    ink = rgb(BAND[band][3])
    x = 1
    for letter in 'KITE':
        for dy, row in enumerate(FONT[letter]):
            for dx, cell in enumerate(row):
                if cell == '#':
                    t[BAND_TOP + 1 + dy, x + dx] = ink
        x += len(FONT[letter][0]) + 1
    return t


def kited(t, band):
    t = ribbon(t, 'right', band)
    lit, shaded = rgb(BAND[band][4]), rgb(BAND[band][5])
    for y, (first, last) in KITE_ROWS.items():
        for x in range(first, last + 1):
            t[y, x] = lit if x < 5 else shaded
    t[6, 5] = rgb(BAND[band][0])
    for y, x in KITE_STRING:
        t[y, x] = lit
    return t


def cube(top, left, right, light):
    """The block on a transparent canvas, faces lit by @p light."""
    side = SIZES[-1] * SUPERSAMPLE
    s = side * EDGE
    a, b = s * np.cos(np.pi / 6), s * 0.5
    cx, y0 = side / 2, (side - (2 * b + s)) / 2
    canvas = Image.new('RGBA', (side, side), (0, 0, 0, 0))
    faces = [(left, light[1], (cx - a, y0 + b), (a, b), (0, s)),
             (right, light[2], (cx, y0 + 2 * b), (a, -b), (0, s)),
             (top, light[0], (cx, y0), (a, b), (-a, b))]
    for t, lit, origin, u, v in faces:
        texels = np.clip(t * lit, 0, 255).astype(np.uint8)
        # one texel of edge copied round it, so no seam shows at the corners
        img = Image.fromarray(np.pad(texels, ((1, 1), (1, 1), (0, 0)), mode='edge')).convert('RGBA')
        inv = np.linalg.inv(np.array([[u[0], v[0]], [u[1], v[1]]]) / TEXELS)
        shift = -inv @ np.array(origin) + 1
        warped = img.transform((side, side), Image.AFFINE,
                               (inv[0, 0], inv[0, 1], shift[0], inv[1, 0], inv[1, 1], shift[1]),
                               resample=Image.NEAREST)
        corners = [origin, (origin[0] + u[0], origin[1] + u[1]),
                   (origin[0] + u[0] + v[0], origin[1] + u[1] + v[1]), (origin[0] + v[0], origin[1] + v[1])]
        mx, my = np.mean(corners, axis=0)
        grown = [(x + np.sign(x - mx) * 2, y + np.sign(y - my) * 2) for x, y in corners]
        mask = Image.new('L', (side, side), 0)
        ImageDraw.Draw(mask).polygon(grown, fill=255)
        warped.putalpha(mask)
        canvas.alpha_composite(warped)
    return canvas


def main():
    theme = sys.argv[1] if len(sys.argv) > 1 else os.path.join('launcher', 'resources', 'multimc')
    for name, top, left, right, glows in blocks():
        band = 'light' if name == 'glass' else 'lilac'
        big = cube(top, labelled(left, band), kited(right, band), GLOW_LIGHT if glows else LIGHT).convert('RGBa')
        for size in SIZES:
            folder = os.path.join(theme, f'{size}x{size}', 'instances')
            os.makedirs(folder, exist_ok=True)
            path = os.path.join(folder, f'kite_{name}.png')
            big.resize((size, size), Image.LANCZOS).convert('RGBA').save(path, optimize=True)
            with open(path + '.license', 'w', encoding='utf-8') as sidecar:
                sidecar.write(LICENSE)
        print(f'kite_{name}')


if __name__ == '__main__':
    main()
