# SPDX-FileCopyrightText: 2026 grxtor
# SPDX-License-Identifier: Apache-2.0
"""
Lifts the glass kite out of the app icon for the website's sky.

The icon (branding/Kite.icon/Assets/kite.png) draws a glass kite on an opaque
violet backdrop. Glass mostly adds light to what is behind it, so: estimate
the backdrop without the kite (inpaint), keep only the light the kite adds,
and save it as white with that light as alpha. Laid over any sky it looks
like the icon's kite floating there.

    python3 kite-glass.py <kite.png> <out.png>

Needs numpy and opencv-python. The kite's corners below are for the 1024 px
icon; redraw them if the icon changes.
"""
import sys

import cv2
import numpy as np

BODY = [[340, 212], [778, 92], [890, 528], [392, 745]]
TAIL = [[0, 870], [60, 770], [170, 780], [280, 740], [380, 700], [430, 760],
        [380, 840], [300, 900], [170, 940], [60, 960], [0, 960]]
MARGIN = 61         # px around the kite treated as "not backdrop"
SCALE = 4           # inpaint at 1/4 size for a smooth backdrop
CROP = (60, 980, 0, 930)  # y0, y1, x0, x1: the kite and its tail
TAIL_FADE = 140     # px over which the tail fades out at the left edge
GAIN = 1.15
TINT = (255, 240, 244)  # BGR of #f4f0ff


def main(src: str, out: str) -> None:
    img = cv2.imread(src).astype(np.float32)
    h, w = img.shape[:2]

    mask = np.zeros((h, w), np.uint8)
    cv2.fillPoly(mask, [np.array(BODY, np.int32), np.array(TAIL, np.int32)], 255)
    mask = cv2.dilate(mask, cv2.getStructuringElement(cv2.MORPH_ELLIPSE, (MARGIN, MARGIN)))

    small = cv2.resize(img, (w // SCALE, h // SCALE), interpolation=cv2.INTER_AREA).astype(np.uint8)
    small_mask = cv2.resize(mask, (w // SCALE, h // SCALE), interpolation=cv2.INTER_NEAREST)
    backdrop = cv2.inpaint(small, small_mask, 12, cv2.INPAINT_TELEA)
    backdrop = cv2.GaussianBlur(backdrop, (0, 0), 6)
    backdrop = cv2.resize(backdrop, (w, h), interpolation=cv2.INTER_CUBIC).astype(np.float32)

    light = np.clip(img - backdrop, 0, 255)
    light *= cv2.GaussianBlur(mask.astype(np.float32) / 255, (0, 0), 12)[..., None]

    y0, y1, x0, x1 = CROP
    light = light[y0:y1, x0:x1]
    luma = 0.114 * light[..., 0] + 0.587 * light[..., 1] + 0.299 * light[..., 2]
    alpha = np.clip(luma * GAIN, 0, 255)
    alpha *= np.clip(np.arange(alpha.shape[1]) / TAIL_FADE, 0, 1)[None, :]

    rgba = np.zeros((*alpha.shape, 4), np.uint8)
    rgba[..., :3] = TINT
    rgba[..., 3] = alpha.astype(np.uint8)
    cv2.imwrite(out, rgba)


if __name__ == '__main__':
    main(sys.argv[1], sys.argv[2])
