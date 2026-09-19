"""tiles2 prototype, part two: the overlay stack the owner asked for.

  base only -> +decals -> +macro tone drift -> +second grass terrain -> all

Same 20x12 map every time. stdlib only.
"""
import math, sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "tools" / "palette"))
sys.path.insert(0, str(ROOT / "tools" / "tiles2"))
from assets import (img_new, get, put, crop, resize, save, load_sheet,  # noqa
                    make_seamless, key_decals)
from render import (T, W, H, h32, vnoise, bake, rot, CASES, build_map, zoom)  # noqa

GRASS, CLOVER, DIRT, PAVE, WATER = 0, 1, 2, 3, 4      # = priority order
CELL_SRC = {GRASS: (0, 0), CLOVER: (2, 0), DIRT: (3, 0), PAVE: (4, 0), WATER: (5, 0)}
STYLE = {GRASS: "ragged", CLOVER: "ragged", DIRT: "ragged", PAVE: "smooth", WATER: "shore"}
SWATCH_TILES = {GRASS: 3, CLOVER: 3, DIRT: 3, PAVE: 2, WATER: 3}
SAND = (214, 190, 142)
CELLPX = 256


# ---------------- swatch tone ----------------

def calm(im, contrast=0.5, tint=None):
    """Tone a swatch down: pull every pixel's luma toward the swatch mean by
    `contrast`, keeping its hue. A calm ground is what lets decals read."""
    w, h, _ = im
    mean = [0, 0, 0]
    for y in range(h):
        for x in range(w):
            c = get(im, x, y)
            for k in range(3):
                mean[k] += c[k]
    mean = [m / (w * h) for m in mean]
    out = img_new(w, h)
    for y in range(h):
        for x in range(w):
            c = get(im, x, y)
            v = []
            for k in range(3):
                t = mean[k] + (c[k] - mean[k]) * contrast
                if tint:
                    t = t * tint[k]
                v.append(max(0, min(255, int(t))))
            put(out, x, y, tuple(v) + (255,))
    return out


def swatch_set(calm_grass=True):
    sheet = load_sheet()
    raw = {t: crop(sheet, cx * CELLPX, cy * CELLPX, CELLPX, CELLPX)
           for t, (cx, cy) in CELL_SRC.items()}
    sw = {}
    for t, im in raw.items():
        px = SWATCH_TILES[t] * T
        s = resize(im, px, px)
        if t == GRASS and calm_grass:
            s = calm(s, 0.28)
        if t == CLOVER:
            s = calm(s, 0.30, tint=(1.42, 0.94, 1.45))    # dry, straw-tinged grass
        sw[t] = make_seamless(s)
    return sw, sheet


def sample(sw, t, px, py):
    im = sw[t]
    return get(im, px % im[0], py % im[1])


# ---------------- decals ----------------

def mirror(im):
    w, h, _ = im
    out = img_new(w, h)
    for y in range(h):
        for x in range(w):
            put(out, w - 1 - x, y, get(im, x, y))
    return out


def scaled(im, k):
    """Nearest resample that keeps alpha — a decal is a cut-out, and resize() is for
    opaque swatches only (it writes 255 alpha, which is what drew black boxes)."""
    nw, nh = max(4, int(im[0] * k)), max(4, int(im[1] * k))
    out = img_new(nw, nh)
    for y in range(nh):
        for x in range(nw):
            put(out, x, y, get(im, x * im[0] // nw, y * im[1] // nh))
    return out


def decal_set(sheet):
    def C(cx, cy):
        return crop(sheet, cx * CELLPX, cy * CELLPX, CELLPX, CELLPX)

    def white_or_red(c):
        r, g, b, _ = c
        return ((r > 170 and g > 170 and b > 150 and abs(r - g) < 60)
                or (r > 130 and r > g + 35 and r > b + 35))

    def grey_stone(c):
        r, g, b, _ = c
        mx, mn = max(r, g, b), min(r, g, b)
        return mx - mn < 34 and 110 < mx < 235

    def dark_tuft(c):
        r, g, b, _ = c
        return g > r + 18 and g < 150 and b < 120

    pools = {
        "flower": key_decals(C(1, 0), white_or_red, 40, 2600),
        "tuft":   key_decals(C(2, 0), dark_tuft, 220, 5200),
        "pebble": key_decals(C(3, 0), grey_stone, 120, 4200),
    }
    out = {}
    for k, ds in pools.items():
        v = []
        for d in ds:
            base = scaled(d, T / CELLPX * 2.0)     # a decal is drawn at map scale
            for s in (0.75, 1.0, 1.35):            # three sizes
                v.append(scaled(base, s))
                v.append(mirror(scaled(base, s)))  # and a mirror of each
        out[k] = v
    return out


def blit(canvas, d, x0, y0):
    for y in range(d[1]):
        cy = y0 + y
        if not (0 <= cy < canvas[1]):
            continue
        for x in range(d[0]):
            cx = x0 + x
            if 0 <= cx < canvas[0]:
                c = get(d, x, y)
                if c[3] > 127:
                    put(canvas, cx, cy, c)


# ---------------- the map, with a second grass ----------------

def build_map2(second_grass):
    g = build_map()
    if not second_grass:
        return [[(0 if v == 0 else v + 1) for v in row] for row in g]
    # remap render.py's ids (0 grass 1 dirt 2 pave 3 water) onto this file's
    out = [[(0 if v == 0 else v + 1) for v in row] for row in g]
    for y in range(H):
        for x in range(W):
            if out[y][x] != GRASS:
                continue
            n = vnoise(x / 5.5, y / 5.5, 4242)
            if n > 0.18:
                out[y][x] = CLOVER
    return out


def at(g, x, y):
    if 0 <= x < W and 0 <= y < H:
        return g[y][x]
    return GRASS


# ---------------- the renderer ----------------

_maskcache = {}


def mask(shape, style, variant, r):
    k = (shape, style, variant, r)
    if k not in _maskcache:
        _maskcache[k] = rot(bake(shape, style, variant), r)
    return _maskcache[k]


def render(g, sw, decals, use_decals, use_macro, terrains):
    canvas = img_new(W * T, H * T, (0, 0, 0, 255))
    for py in range(H * T):
        for px in range(W * T):
            put(canvas, px, py, sample(sw, GRASS, px, py))

    for t in terrains:
        if t == GRASS:
            continue
        style = STYLE[t]
        band = 0.085 if t == WATER else 0.0
        for passno in (0, 1):
            if passno == 0 and band == 0:
                continue
            for j in range(H + 1):
                for i in range(W + 1):
                    b = 0
                    if at(g, i - 1, j - 1) == t: b |= 1
                    if at(g, i, j - 1) == t: b |= 2
                    if at(g, i, j) == t: b |= 4
                    if at(g, i - 1, j) == t: b |= 8
                    c = CASES[b]
                    if c is None:
                        continue
                    shape, r = c
                    f = mask(shape, style, h32(i, j, t) % 3, r)
                    ox, oy = i * T - T // 2, j * T - T // 2
                    for py in range(T):
                        cy = oy + py
                        if not (0 <= cy < H * T):
                            continue
                        row = f[py]
                        for px in range(T):
                            cx = ox + px
                            if not (0 <= cx < W * T):
                                continue
                            d = row[px]
                            if passno:
                                if d > 0:
                                    put(canvas, cx, cy, sample(sw, t, cx, cy))
                            elif -band < d <= 0:
                                put(canvas, cx, cy, SAND + (255,))

    if use_decals:
        scatter(canvas, g, decals)

    if use_macro:
        macro(canvas)
    return canvas


def scatter(canvas, g, decals):
    """Hashed placement: a low-frequency density field makes drifts and patches,
    the cell's terrain and its neighbours choose the pool, and every decal gets
    sub-tile jitter, one of three sizes and a mirror. Decals straddle cells."""
    for y in range(H):
        for x in range(W):
            t = at(g, x, y)
            nb = [at(g, x + 1, y), at(g, x - 1, y), at(g, x, y + 1), at(g, x, y - 1)]
            if t in (GRASS, CLOVER):
                pool = ["tuft", "tuft", "flower"]
                if WATER in nb:
                    pool = ["tuft", "tuft", "tuft", "pebble"]      # reeds at the shore
                elif DIRT in nb or PAVE in nb:
                    pool = ["tuft", "pebble", "pebble"]            # path dressing
            elif t == DIRT:
                pool = ["pebble"]
            else:
                continue                                           # never on paving
            dens = 0.5 + 0.5 * vnoise(x / 4.0, y / 4.0, 777)       # drifts and patches
            if t == DIRT:
                dens *= 0.45
            n = 0
            hh = h32(x, y, 11)
            if (hh & 0xff) / 255.0 < dens * 1.15:
                n = 1
            if ((hh >> 8) & 0xff) / 255.0 < dens * dens * 0.55:
                n = 2
            for k in range(n):
                hk = h32(x, y, 11 + 97 * k)
                kind = pool[hk % len(pool)]
                v = decals[kind]
                d = v[(hk >> 3) % len(v)]
                jx = x * T + ((hk >> 11) % T) - d[0] // 2
                jy = y * T + ((hk >> 19) % T) - d[1] // 2
                blit(canvas, d, jx, jy)


def macro(canvas):
    """Very low frequency drift of the light level, quantised to shade steps. In the
    engine this is an offset into the colormap's light rows, not a colour computation."""
    w, h, _ = canvas
    for py in range(h):
        for px in range(w):
            n = vnoise(px / (T * 7.0), py / (T * 7.0), 31337)
            step = round(n * 2)                      # -2..+2 shade steps
            if step == 0:
                continue
            f = 1.0 + 0.035 * step
            c = get(canvas, px, py)
            put(canvas, px, py,
                tuple(max(0, min(255, int(c[k] * f))) for k in range(3)) + (255,))


def strip(images, width=960):
    panels = [resize(im, width, im[1] * width // im[0]) for im in images]
    gap = 6
    hh = sum(p[1] for p in panels) + gap * (len(panels) - 1)
    out = img_new(width, hh, (18, 18, 22, 255))
    y = 0
    for p in panels:
        for yy in range(p[1]):
            out[2][y + yy][:] = p[2][yy]
        y += p[1] + gap
    return out


def main():
    sw, sheet = swatch_set()
    decals = decal_set(sheet)
    print("decal variants:", {k: len(v) for k, v in decals.items()})
    g4 = build_map2(False)
    g5 = build_map2(True)
    four = (GRASS, DIRT, PAVE, WATER)
    five = (GRASS, CLOVER, DIRT, PAVE, WATER)

    steps = []
    a = render(g4, sw, decals, False, False, four); save(a, "s1_base.png"); steps.append(a)
    b = render(g4, sw, decals, True, False, four); save(b, "s2_decals.png"); steps.append(b)
    c = render(g4, sw, decals, True, True, four); save(c, "s3_macro.png"); steps.append(c)
    d = render(g5, sw, decals, True, False, five); save(d, "s4_clover.png"); steps.append(d)
    e = render(g5, sw, decals, True, True, five); save(e, "s5_all.png"); steps.append(e)
    save(strip(steps), "stack_strip.png")
    save(zoom(e, 3 * T, 4 * T, 8 * T, 5 * T, 2), "s5_detail.png")
    print("wrote s1..s5, stack_strip.png, s5_detail.png")


if __name__ == "__main__":
    main()
