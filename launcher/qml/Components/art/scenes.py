# SPDX-FileCopyrightText: 2026 Project Tick
# SPDX-FileContributor: Project Tick
# SPDX-License-Identifier: Apache-2.0
"""
Procedural pixel-art landscapes for generate.py: 24 scenes (6 moods x 4
compositions), each drawn at three shapes so the scene survives whatever band
hosts it -- a 96x54 scene cropped into a 12:1 dock bar only ever samples a
sliver of plain sky (design-plan.md G3c), so a wide host gets a scene drawn
for its own shape rather than a crop of the card one:

    card   48x27 logical (2x baked ->  96x54)   grid/jump cards, tiles
    band   96x18 logical (2x baked -> 192x36)   instance-page hero, ~5:1
    strip 192x16 logical (2x baked -> 384x32)   dock bar, ~12:1

Index i is mood i // 4, composition i % 4; QML (PixelArt.qml) picks i from a
hash of the instance/world id, so a given item keeps one look at every shape.

Original artwork only -- no Mojang/Minecraft assets are read or copied.
Deterministic: every scene is built from a fixed random.Random seed.
"""

import math
import random
from PIL import Image

SHAPES = {"card": (48, 27), "band": (96, 18), "strip": (192, 16)}
MOOD_ORDER = ("day", "sunset", "night", "snow", "desert", "forest")
COMP_ORDER = ("meadow", "ridge", "lake", "outpost")
SCENE_COUNT = len(MOOD_ORDER) * len(COMP_ORDER)


def scene_label(index):
    return f"{MOOD_ORDER[index // 4]}/{COMP_ORDER[index % 4]}"


def _c8(v):
    return max(0, min(255, int(round(v))))


def mix(a, b, t):
    return tuple(_c8(a[i] + (b[i] - a[i]) * t) for i in range(3))


def shade(c, f):
    return tuple(_c8(v * f) for v in c)


def jit(rng, c, spread):
    return tuple(_c8(v + rng.randint(-spread, spread)) for v in c)


class Canvas:
    def __init__(self, w, h):
        self.w, self.h = w, h
        self.img = Image.new("RGB", (w, h))
        self.px = self.img.load()

    def put(self, x, y, c):
        if 0 <= x < self.w and 0 <= y < self.h:
            self.px[x, y] = c

    def rect(self, x0, y0, x1, y1, c):
        for y in range(max(0, y0), min(self.h, y1)):
            for x in range(max(0, x0), min(self.w, x1)):
                self.px[x, y] = c


def _rgb(hexstr):
    return tuple(int(hexstr[i:i + 2], 16) for i in (0, 2, 4))


# sky: (position, colour) bands, drawn as solid strips with a one-row
# checker dither at each seam -- pixel-art skies are banded, not smooth.
MOODS = {
    "day": dict(
        sky=[(0.00, _rgb("8FBEE0")), (0.42, _rgb("A9D2EA")), (0.68, _rgb("C9E4F2"))],
        far=_rgb("5C8F5B"), near=_rgb("42733F"), ground=_rgb("34592F"), tuft=_rgb("5E9450"),
        peak=_rgb("7C93A6"), cap=_rgb("F0F4F6"), accent=_rgb("F4D35E"), cloud=_rgb("F6F6F6"),
        water=_rgb("4F8FB8"), water_hi=_rgb("9CCBE4"), stars=0, clouds=True,
        tree=_rgb("2C5533"), tree_hi=_rgb("3F7444"), trunk=_rgb("5A3E26"),
        wall=_rgb("B98A55"), roof=_rgb("8A3F34"), door=_rgb("4A3320"), window=None,
        tree_style="round", moon=False,
    ),
    "sunset": dict(
        sky=[(0.00, _rgb("3E3E6C")), (0.30, _rgb("7C567A")), (0.52, _rgb("C86F62")), (0.72, _rgb("E8965E"))],
        far=_rgb("48394A"), near=_rgb("2C222E"), ground=_rgb("1C1620"), tuft=_rgb("3A2C3A"),
        peak=_rgb("54405A"), cap=_rgb("D08A78"), accent=_rgb("F6BC58"), cloud=_rgb("E08C74"),
        water=_rgb("6A4A6A"), water_hi=_rgb("EAA060"), stars=0, clouds=True,
        tree=_rgb("1E1722"), tree_hi=_rgb("2E2230"), trunk=_rgb("241A20"),
        wall=_rgb("3C2C34"), roof=_rgb("241820"), door=_rgb("140E14"), window=_rgb("F2BE62"),
        tree_style="round", moon=False,
    ),
    "night": dict(
        sky=[(0.00, _rgb("0A0F20")), (0.45, _rgb("121B34")), (0.72, _rgb("1C2A46"))],
        far=_rgb("1A2634"), near=_rgb("111A22"), ground=_rgb("0B1116"), tuft=_rgb("1C2A32"),
        peak=_rgb("243048"), cap=_rgb("6E80A2"), accent=_rgb("E2EAF4"), cloud=_rgb("2A3854"),
        water=_rgb("122036"), water_hi=_rgb("4C6488"), stars=1, clouds=False,
        tree=_rgb("0B1418"), tree_hi=_rgb("14222A"), trunk=_rgb("0A1014"),
        wall=_rgb("1C2632"), roof=_rgb("0E151C"), door=_rgb("080C10"), window=_rgb("F2C86A"),
        tree_style="pine", moon=True,
    ),
    "snow": dict(
        sky=[(0.00, _rgb("B4C8D8")), (0.42, _rgb("CFDDE7")), (0.70, _rgb("E5EDF2"))],
        far=_rgb("BCCBD6"), near=_rgb("DDE7ED"), ground=_rgb("EEF3F6"), tuft=_rgb("FFFFFF"),
        peak=_rgb("8FA3B6"), cap=_rgb("FCFDFE"), accent=_rgb("F4EFE0"), cloud=_rgb("F7FAFB"),
        water=_rgb("9EC2D4"), water_hi=_rgb("DCEEF6"), stars=0, clouds=True,
        tree=_rgb("2A4A40"), tree_hi=_rgb("F2F7F8"), trunk=_rgb("4A362A"),
        wall=_rgb("9A6E48"), roof=_rgb("F4F8FA"), door=_rgb("4A3322"), window=_rgb("F0C468"),
        tree_style="pine", moon=False,
    ),
    "desert": dict(
        sky=[(0.00, _rgb("E1CB92")), (0.44, _rgb("EBDBAC")), (0.70, _rgb("F4E9C6"))],
        far=_rgb("D2B168"), near=_rgb("B99552"), ground=_rgb("9A783C"), tuft=_rgb("D8BA74"),
        peak=_rgb("B4694A"), cap=_rgb("D88E62"), accent=_rgb("F4C450"), cloud=_rgb("F8EFD6"),
        water=_rgb("46969E"), water_hi=_rgb("9ED4D6"), stars=0, clouds=False,
        tree=_rgb("46703E"), tree_hi=_rgb("62924F"), trunk=_rgb("6A4A2E"),
        wall=_rgb("D6B47C"), roof=_rgb("B48E58"), door=_rgb("4A3320"), window=None,
        tree_style="cactus", moon=False,
    ),
    "forest": dict(
        sky=[(0.00, _rgb("8DB6B6")), (0.44, _rgb("ACCDC7")), (0.70, _rgb("C8E0D9"))],
        far=_rgb("3F6E44"), near=_rgb("2B5231"), ground=_rgb("1E3A24"), tuft=_rgb("3C7040"),
        peak=_rgb("4F7472"), cap=_rgb("BFD4CF"), accent=_rgb("F2EBC0"), cloud=_rgb("E6F0EC"),
        water=_rgb("3C7883"), water_hi=_rgb("8AB8BA"), stars=0, clouds=False,
        tree=_rgb("1B3B23"), tree_hi=_rgb("2E5A34"), trunk=_rgb("4A3524"),
        wall=_rgb("8A6238"), roof=_rgb("4A2E28"), door=_rgb("2E2016"), window=_rgb("F0C468"),
        tree_style="pine", moon=False,
    ),
}

# Four compositions, each a different silhouette rather than a re-scatter of
# the same one: horizon height, the far layer's kind, the near layer's shape
# and what stands on it, and where the sun/moon sits.
COMPS = {
    "meadow": dict(horizon=0.68, ax=0.78, ay=0.16, far="hills", far_amp=1.7, near_off=3.4,
                   near_amp=1.5, trees=7, clouds=3, star_mul=1.0, water=False, huts=0),
    "ridge": dict(horizon=0.64, ax=0.20, ay=0.20, far="peaks", far_amp=11.0, near_off=4.0,
                  near_amp=1.1, trees=5, clouds=2, star_mul=1.5, water=False, huts=0),
    "lake": dict(horizon=0.50, ax=0.55, ay=0.14, far="hills", far_amp=1.9, near_off=4.0,
                 near_amp=0.8, trees=4, clouds=2, star_mul=0.9, water=True, huts=0),
    "outpost": dict(horizon=0.60, ax=0.34, ay=0.34, far="hills", far_amp=2.6, near_off=3.8,
                    near_amp=0.9, trees=3, clouds=1, star_mul=0.7, water=False, huts=3),
}
# The desert has no rolling green hills: dunes, mesas and an oasis instead.
DESERT_FAR = {"meadow": "dunes", "ridge": "mesa", "lake": "dunes", "outpost": "dunes"}


def _sky(cv, bands):
    """Solid horizontal strips whose colours follow the mood's anchor bands
    -- pixel-art skies are banded rather than smooth, but with enough strips
    that no single seam reads as a hard line."""
    def smooth(pos):
        lo = bands[0]
        for b in bands:
            if pos >= b[0]:
                lo = b
        hi = next((b for b in bands if b[0] > lo[0]), None)
        if hi is None:
            return lo[1]
        return mix(lo[1], hi[1], (pos - lo[0]) / (hi[0] - lo[0]))

    strips = max(4, cv.h // 3)
    rows = {}
    for y in range(cv.h):
        k = min(strips - 1, int(y / cv.h * strips))
        rows[y] = smooth((k + 0.5) / strips * 0.78)
        for x in range(cv.w):
            cv.px[x, y] = rows[y]
    return lambda y: rows[max(0, min(cv.h - 1, y))]


def _disc(cv, cx, cy, r, color):
    n = int(math.ceil(r)) + 1
    for dy in range(-n, n + 1):
        for dx in range(-n, n + 1):
            if dx * dx + dy * dy <= r * r + 0.6:
                cv.put(cx + dx, cy + dy, color)


def _cloud(cv, x, y, cw, color, shadow):
    cv.rect(x + 1, y, x + cw - 1, y + 1, color)
    cv.rect(x, y + 1, x + cw, y + 2, color)
    cv.rect(x + 1, y + 2, x + cw - 2, y + 3, shadow)


def _rolling(w, h, base, amp, rng):
    scale = (w / h) / (48 / 27)
    waves = [(rng.uniform(0.7, 1.3) * scale, rng.uniform(0, 6.28), 0.60),
             (rng.uniform(1.8, 2.8) * scale, rng.uniform(0, 6.28), 0.30),
             (rng.uniform(4.0, 6.0) * scale, rng.uniform(0, 6.28), 0.10)]
    return [int(round(base + amp * sum(a * math.sin(2 * math.pi * f * x / w + p) for f, p, a in waves)))
            for x in range(w)]


def _fill_layer(cv, tops, color, edge=None):
    for x, t in enumerate(tops):
        for y in range(max(0, t), cv.h):
            cv.px[x, y] = color
        if edge is not None:
            cv.put(x, t, edge)


def _peaks(cv, base_y, height, count, color, cap, rng):
    w = cv.w
    n = max(2, round(count * w / 48))
    peaks = []
    for i in range(n):
        cx = (i + 0.5) * w / n + rng.uniform(-0.22, 0.22) * w / n
        ph = height * rng.uniform(0.62, 1.0)
        peaks.append((cx, ph, ph * rng.uniform(1.05, 1.55)))
    for x in range(w):
        best = None
        for cx, ph, hw in peaks:
            top = base_y - ph * (1 - abs(x - cx) / hw)
            if abs(x - cx) < hw and (best is None or top < best[0]):
                best = (top, cx, ph)
        top = int(round(best[0])) if best else base_y
        tone = shade(color, 0.86) if best and x > best[1] else color
        for y in range(max(0, min(top, base_y)), cv.h):
            cv.px[x, y] = tone
        if best and (base_y - top) >= best[2] * (0.68 + rng.choice((-0.06, 0, 0, 0.06))):
            for y in range(max(0, top), max(0, top) + max(1, round((base_y - top) - best[2] * 0.62))):
                cv.put(x, y, cap if x <= best[1] else shade(cap, 0.9))


def _mesas(cv, base_y, height, count, color, rng):
    w = cv.w
    n = max(2, round(count * w / 48))
    stripe = shade(color, 0.86)
    for i in range(n):
        cx = int((i + 0.5) * w / n + rng.uniform(-0.2, 0.2) * w / n)
        mh = max(3, int(height * rng.uniform(0.55, 1.0)))
        top_half = max(2, int(mh * rng.uniform(0.55, 0.9)))
        for k in range(mh):
            half = top_half + (k * 2) // 3 + (k // 3)
            y = base_y - mh + k
            c = stripe if (k // 2) % 2 else color
            for x in range(cx - half, cx + half + 1):
                cv.put(x, y, c)
    for y in range(base_y, cv.h):
        for x in range(w):
            cv.px[x, y] = color


def _tree(cv, x, base_y, s, style, m, rng):
    canopy, hi, trunk = m["tree"], m["tree_hi"], m["trunk"]
    if style == "cactus":
        col = m["tree"]
        for dy in range(s + 1):
            cv.put(x, base_y - 1 - dy, col)
        if s >= 4:
            cv.put(x - 1, base_y - 1 - s // 2, col)
            cv.put(x - 1, base_y - 2 - s // 2, col)
            cv.put(x + 1, base_y - 2 - s // 2, col)
            cv.put(x + 1, base_y - 3 - s // 2, col)
        cv.put(x, base_y - 1 - s, m["tree_hi"])
        return
    if style == "palm":
        th = s + 1
        for dy in range(th):
            cv.put(x + (1 if dy > th // 2 else 0), base_y - 1 - dy, trunk)
        tx, ty = x + 1, base_y - th
        for dx, dy in ((-2, 0), (-1, -1), (0, -1), (1, -1), (2, 0), (-1, 0), (3, 1), (-3, 1)):
            cv.put(tx + dx, ty + dy, canopy)
        cv.put(tx, ty - 1, hi)
        return
    if style == "pine":
        cv.put(x, base_y - 1, trunk)
        top = base_y - 1 - s
        for dy in range(s):
            half = ((dy + 1) * (s // 2 + 1)) // s
            for dx in range(-half, half + 1):
                cv.put(x + dx, top + dy, hi if (dx < 0 and dy % 2 == 0) else canopy)
        return
    th = max(1, s // 3)
    for dy in range(th):
        cv.put(x, base_y - 1 - dy, trunk)
    top = base_y - th - s
    rx, ry = (s + 1) / 2.0, s / 2.0
    for dy in range(s):
        for dx in range(-(s // 2) - 1, s // 2 + 2):
            if (dx / rx) ** 2 + ((dy - ry + 0.5) / ry) ** 2 <= 1.0:
                cv.put(x + dx, top + dy, hi if (dy < s / 3 and dx <= 0) else canopy)


def _hut(cv, x, base_y, u, m, flat):
    bw = max(4, round(6 * u))
    bh = max(3, round(4 * u))
    x0 = x - bw // 2
    cv.rect(x0, base_y - bh, x0 + bw, base_y, m["wall"])
    if flat:
        cv.rect(x0 - 1, base_y - bh - 1, x0 + bw + 1, base_y - bh, m["roof"])
    else:
        rh = max(2, (bw + 1) // 2)
        for k in range(rh):
            if x0 + bw + 1 - k <= x0 - 1 + k:
                break
            cv.rect(x0 - 1 + k, base_y - bh - 1 - k, x0 + bw + 1 - k, base_y - bh - k, m["roof"])
    cv.put(x0 + bw // 2, base_y - 1, m["door"])
    if bh >= 3:
        cv.put(x0 + bw // 2, base_y - 2, m["door"])
    if bw >= 5:
        cv.put(x0 + 1, base_y - bh + 1, m["window"] or shade(m["wall"], 0.7))


def render(index, shape):
    mood_name = MOOD_ORDER[index // 4]
    comp_name = COMP_ORDER[index % 4]
    m, comp = MOODS[mood_name], COMPS[comp_name]
    w, h = SHAPES[shape]
    rng = random.Random(7000 + index * 31 + list(SHAPES).index(shape))
    u = h / 27.0
    cv = Canvas(w, h)
    sky_at = _sky(cv, m["sky"])

    horizon = round(h * comp["horizon"])
    ax = int(w * comp["ax"]) + rng.randint(-2, 2)
    ay = int(h * comp["ay"])
    if mood_name == "sunset":
        ay = min(int(h * (comp["ay"] + 0.16)), horizon - 1)
    r = max(1.5, 2.3 * u)

    for _ in range(round(m["stars"] * comp["star_mul"] * w * h / 64)):
        sx, sy = rng.randint(0, w - 1), rng.randint(0, max(1, horizon - 2))
        cv.put(sx, sy, mix(m["accent"], sky_at(sy), rng.choice((0.0, 0.35, 0.55))))
    _disc(cv, ax, ay, r, m["accent"])
    if m["moon"] and comp_name in ("ridge", "outpost"):
        _disc(cv, ax + 1, ay - 1, r * 0.9, sky_at(ay))

    if m["clouds"]:
        for _ in range(round(comp["clouds"] * w / 48)):
            cx = rng.randint(1, w - 9)
            cy = rng.randint(1, max(2, int(horizon * 0.5)))
            _cloud(cv, cx, cy, rng.randint(max(4, int(5 * u * 1.5)), max(6, int(9 * u * 1.5))),
                   m["cloud"], mix(m["cloud"], sky_at(cy + 2), 0.35))

    far_kind = DESERT_FAR[comp_name] if mood_name == "desert" else comp["far"]
    far_amp = comp["far_amp"] * u
    if far_kind == "peaks":
        _peaks(cv, horizon + 1, far_amp, 3, m["peak"], m["cap"], rng)
        far_tops = [horizon + 1] * w
    elif far_kind == "mesa":
        _mesas(cv, horizon + 1, far_amp * 0.9, 3, m["peak"], rng)
        far_tops = [horizon + 1] * w
    else:
        amp = far_amp * (1.6 if far_kind == "dunes" else 1.0)
        far_tops = _rolling(w, h, horizon, amp, rng)
        _fill_layer(cv, far_tops, m["far"], mix(m["far"], sky_at(max(0, horizon)), 0.28))

    fg = max(2, round(2.5 * u))
    fg_top = h - fg
    water_top = None
    if comp["water"]:
        water_top = min(fg_top - 1, max(far_tops) + 1)
        for y in range(water_top, fg_top):
            for x in range(w):
                cv.px[x, y] = m["water"]
        for y in range(water_top, fg_top):
            for _ in range(w // 10):
                dx = rng.randint(0, w - 4)
                for k in range(rng.randint(2, 4)):
                    cv.put(dx + k, y, mix(m["water"], m["water_hi"], 0.55))
        for k, y in enumerate(range(water_top, fg_top)):
            cv.put(ax + (k % 2), y, m["water_hi"])
            cv.put(ax - 2 + (k % 3), y, mix(m["water"], m["water_hi"], 0.7))
        for x in range(w):
            cv.px[x, water_top] = mix(m["water"], sky_at(water_top), 0.35)

    near_base = horizon + round(comp["near_off"] * u)
    if comp["water"]:
        bank = max(2, (fg_top - water_top) * 0.9)
        near_tops = []
        for x in range(w):
            t = abs(x - w * 0.5) / (w * 0.5)
            lift = bank * (max(0.0, t - 0.42) / 0.58) ** 1.4
            near_tops.append(int(round(fg_top - lift + rng.choice((0, 0, 0, -1)))))
    else:
        near_tops = _rolling(w, h, near_base, comp["near_amp"] * u, rng)
    _fill_layer(cv, near_tops, m["near"], mix(m["near"], m["tuft"], 0.5))

    def stand_on(x):
        return max(0, min(w - 1, x))

    if comp["huts"]:
        step = w / comp["huts"]
        for i in range(comp["huts"]):
            hx = stand_on(int(step * (i + 0.5) + rng.uniform(-0.18, 0.18) * step))
            _hut(cv, hx, near_tops[hx] + 1, u, m, flat=(mood_name == "desert"))

    style = "palm" if (mood_name == "desert" and comp_name == "lake") else m["tree_style"]
    n_trees = max(1, round(comp["trees"] * w / 48 * (0.75 if w > 48 else 1.0)))
    for _ in range(n_trees):
        tx = rng.randint(2, w - 3)
        base_y = near_tops[tx] + rng.randint(0, 1)
        if base_y >= fg_top:
            continue
        size = max(3, round(5.6 * u * rng.uniform(0.8, 1.25)))
        _tree(cv, tx, base_y, size, style, m, rng)

    for y in range(fg_top, h):
        for x in range(w):
            cv.px[x, y] = jit(rng, m["ground"], 4)
    for x in range(w):
        cv.px[x, fg_top] = m["tuft"]
        if rng.random() < 0.2:
            cv.put(x, fg_top - 1, m["tuft"])
    return cv.img
