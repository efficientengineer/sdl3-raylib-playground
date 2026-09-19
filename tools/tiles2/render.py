"""tiles2 prototype renderer.

Two renders of the SAME 20x12 map:
  new.png  - dual grid + 1-bit masks + world-space swatches + hashed decals
  old.png  - one repeated tile per terrain + three rotated fringe tiles (today's system)

stdlib only.
"""
import math, sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "tools" / "palette"))
sys.path.insert(0, str(ROOT / "tools" / "tiles2"))
import pngio  # noqa
from assets import (img_new, get, put, crop, resize, save, load_sheet, make_seamless,
                    flatten, key_decals, OUT, CELL)  # noqa

T = 96                     # device px per logical tile
W, H = 20, 12

GRASS, DIRT, PAVE, WATER = 0, 1, 2, 3
NAMES = {GRASS: "grass", DIRT: "dirt", PAVE: "paving", WATER: "water"}
STYLE = {GRASS: "ragged", DIRT: "ragged", PAVE: "smooth", WATER: "shore"}
SWATCH_TILES = {GRASS: 3, DIRT: 3, PAVE: 2, WATER: 3}
SAND = (214, 190, 142)


# ---------------- the map ----------------

def build_map():
    g = [[GRASS] * W for _ in range(H)]
    # a lake in the north-west, ragged
    lake = [(1,0),(2,0),(3,0),(4,0),(0,1),(1,1),(2,1),(3,1),(4,1),(5,1),
            (0,2),(1,2),(2,2),(3,2),(4,2),(0,3),(1,3),(2,3),(3,3),(1,4),(2,4)]
    for x, y in lake:
        g[y][x] = WATER
    # a winding dirt path from the west edge to the paved square
    path = [(0,7),(1,7),(2,7),(2,6),(3,6),(4,6),(5,6),(5,5),(6,5),(7,5),(7,4),(8,4),
            (9,4),(10,4),(10,5),(11,5),(12,5),(12,6),(13,6),(14,6),(15,6),(16,6),
            (16,7),(17,7),(18,7),(19,7),(9,3),(9,2),(10,2),(10,1),(4,8),(4,9),(5,9),
            (5,10),(6,10),(6,11)]
    for x, y in path:
        g[y][x] = DIRT
        if x + 1 < W and g[y][x + 1] == GRASS and (x + y) % 3:
            g[y][x + 1] = DIRT
    # the paved square, lopsided
    for y in range(7, 11):
        for x in range(11, 17):
            if (y == 10 and x > 15) or (y == 7 and x < 12):
                continue
            g[y][x] = PAVE
    g[6][13] = PAVE; g[6][14] = PAVE
    # water touching dirt and paving: a channel down to the square (three-way meetings)
    for y, x in ((3,4),(4,4),(4,5),(5,5)):
        pass
    g[2][5] = WATER; g[3][4] = WATER
    return g


def at(g, x, y):
    if 0 <= x < W and 0 <= y < H:
        return g[y][x]
    return GRASS


# ---------------- noise ----------------

def h32(*v):
    n = 2166136261
    for x in v:
        n = ((n ^ (int(x) & 0xffffffff)) * 16777619) & 0xffffffff
    return n


def vnoise(x, y, seed):
    """value noise on the unit lattice"""
    xi, yi = math.floor(x), math.floor(y)
    tx, ty = x - xi, y - yi
    tx = tx * tx * (3 - 2 * tx); ty = ty * ty * (3 - 2 * ty)
    def r(a, b):
        return (h32(a, b, seed) & 0xffff) / 65535.0 * 2 - 1
    a = r(xi, yi) * (1 - tx) + r(xi + 1, yi) * tx
    b = r(xi, yi + 1) * (1 - tx) + r(xi + 1, yi + 1) * tx
    return a * (1 - ty) + b * ty


# ---------------- the five mask shapes ----------------
# A display tile's four corners are four world cells. In tile units (0..1):
#   FULL, EDGE (north half), CORNER (quarter disc r=0.5 at NW),
#   DIAG (two quarter discs at NW and SE), INV (all but a quarter disc bite at SE).
# EVERY boundary crosses a tile edge exactly at that edge's MIDPOINT, which is what
# makes any shape meet any other shape with no break.

def sdf(shape, x, y):
    if shape == "full":
        return 1.0
    if shape == "edge":
        return 0.5 - y
    if shape == "corner":
        return 0.5 - math.hypot(x, y)
    if shape == "diag":
        return max(0.5 - math.hypot(x, y), 0.5 - math.hypot(x - 1, y - 1))
    if shape == "inv":
        return math.hypot(x - 1, y - 1) - 0.5
    raise ValueError(shape)


STYLE_P = {"ragged": (0.105, 5.5, 0.045, 13.0),
           "smooth": (0.022, 3.0, 0.010, 7.0),
           "shore":  (0.060, 3.5, 0.022, 9.0)}


def bake(shape, style, variant):
    """TxT float field, >0 inside. Displacement is windowed to zero at the tile border,
    so the boundary still crosses every edge at its midpoint whatever the noise does."""
    a1, f1, a2, f2 = STYLE_P[style]
    seed = h32(hash(shape) & 0xffff, hash(style) & 0xffff, variant)
    f = [[0.0] * T for _ in range(T)]
    for py in range(T):
        y = (py + 0.5) / T
        for px in range(T):
            x = (px + 0.5) / T
            d = sdf(shape, x, y)
            if shape != "full":
                win = min(1.0, min(x, y, 1 - x, 1 - y) / 0.16)
                n = a1 * vnoise(x * f1, y * f1, seed) + a2 * vnoise(x * f2, y * f2, seed + 7)
                d += n * win
            f[py][px] = d
    return f


def rot(f, k):
    if k == 0:
        return f
    out = f
    for _ in range(k):
        out = [[out[T - 1 - x][y] for x in range(T)] for y in range(T)]
    return out


def bits_rot(b):
    return ((b << 1) | (b >> 3)) & 15


# bits: 1=NW 2=NE 4=SE 8=SW
CASES = {0: None, 15: ("full", 0)}
for _sh, _b in (("corner", 1), ("edge", 3), ("diag", 5), ("inv", 11)):
    _c = _b
    for _r in range(4):
        CASES.setdefault(_c, (_sh, _r))
        _c = bits_rot(_c)
assert len(CASES) == 16, CASES


# ---------------- swatches ----------------

def swatches():
    sheet = load_sheet()
    pos = {"grass": (0, 0), "grass_flower": (1, 0), "grass_tuft": (2, 0),
           "dirt": (3, 0), "paving": (4, 0), "water": (5, 0)}
    raw = {n: crop(sheet, cx * CELL, cy * CELL, CELL, CELL) for n, (cx, cy) in pos.items()}
    # heal AFTER resampling: an upscale that clamps at the border would otherwise
    # break the wrap the heal just established.
    def prep(n, px):
        return make_seamless(flatten(resize(raw[n], px, px), r=max(8, px // 6)))
    sw = {t: prep(n, SWATCH_TILES[t] * T) for t, n in NAMES.items()}
    heal = {n: prep(n, T) for n in raw}
    return sw, raw, heal


def sample(sw, t, px, py):
    im = sw[t]
    w, h = im[0], im[1]
    return get(im, px % w, py % h)


# ---------------- the new renderer ----------------

def render_new(g, sw, decals):
    canvas = img_new(W * T, H * T, (0, 0, 0, 255))
    # base: grass everywhere, in world space
    for py in range(H * T):
        for px in range(W * T):
            put(canvas, px, py, sample(sw, GRASS, px, py))

    cache = {}
    def mask(shape, style, variant, r):
        k = (shape, style, variant, r)
        if k not in cache:
            cache[k] = rot(bake(shape, style, variant), r)
        return cache[k]

    for t in (DIRT, PAVE, WATER):
        style = STYLE[t]
        band = 0.085 if t == WATER else 0.0
        for passno in (0, 1):          # 0 = the sand/foam band, 1 = the terrain
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
                    v = h32(i, j, t) % 3
                    f = mask(shape, style, v, r)
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
                            if passno == 1:
                                if d > 0:
                                    put(canvas, cx, cy, sample(sw, t, cx, cy))
                            else:
                                if -band < d <= 0:
                                    put(canvas, cx, cy, SAND + (255,))
    # hashed decals on grass and dirt
    for y in range(H):
        for x in range(W):
            terr = g[y][x]
            kinds = {GRASS: ("flower", "tuft"), DIRT: ("pebble",), PAVE: (), WATER: ()}[terr]
            if not kinds:
                continue
            hh = h32(x, y, 99)
            n = (hh % 5 == 0) + (hh % 7 == 0)
            for k in range(n):
                hk = h32(x, y, 99 + k * 31)
                kind = kinds[hk % len(kinds)]
                pool = decals[kind]
                if not pool:
                    continue
                d = pool[(hk >> 4) % len(pool)]
                dx = x * T + (hk >> 8) % (T - d[0])
                dy = y * T + (hk >> 16) % (T - d[1])
                for yy in range(d[1]):
                    for xx in range(d[0]):
                        c = get(d, xx, yy)
                        if c[3] and 0 <= dx + xx < W * T and 0 <= dy + yy < H * T:
                            # a decal only sits on its own terrain's cell
                            put(canvas, dx + xx, dy + yy, c)
    return canvas


# ---------------- today's system, for comparison ----------------

def fringe_alpha(kind, style, variant):
    """The three drawn fringe tiles, as alpha over the terrain's own art."""
    a1, f1, a2, f2 = STYLE_P[style]
    seed = h32(hash(kind) & 0xffff, variant, 3)
    f = [[0.0] * T for _ in range(T)]
    depth = 10.0 / 32.0
    for py in range(T):
        y = (py + 0.5) / T
        for px in range(T):
            x = (px + 0.5) / T
            n = a1 * vnoise(x * f1, y * f1, seed) + a2 * vnoise(x * f2, y * f2, seed + 5)
            if kind == "edge":
                d = depth - y + n
            elif kind == "corner_out":
                d = depth - max(y, 1 - x) + n
            else:  # corner_in: everything but a bite out of the south-west
                d = max(min(1 - depth - x, 1 - depth - (1 - y)), 0.0)
                d = (1 if not (x < depth and y > 1 - depth) else -1)
                d = 1.0 if not (x < depth * 1.6 and y > 1 - depth * 1.6) else -1.0
                d += n
            f[py][px] = d
    return f


def render_old(g, heal):
    tiles = {}
    for t, n in NAMES.items():
        tiles[t] = resize(heal[n], T, T)
    var = {GRASS: [resize(heal["grass"], T, T), resize(heal["grass_tuft"], T, T),
                   resize(heal["grass_flower"], T, T)]}
    canvas = img_new(W * T, H * T, (0, 0, 0, 255))
    for y in range(H):
        for x in range(W):
            t = g[y][x]
            im = tiles[t]
            if t == GRASS:
                hh = h32(x, y, 5)
                im = var[GRASS][0 if hh % 6 else (1 if hh % 12 == 0 else 2)]
            for py in range(T):
                for px in range(T):
                    put(canvas, x * T + px, y * T + py, get(im, px, py))

    fr = {}
    def F(kind, style, r):
        k = (kind, style, r)
        if k not in fr:
            fr[k] = rot(fringe_alpha(kind, style, 0), r)
        return fr[k]

    # engine rule: a higher-priority terrain overlays the neighbour it touches
    for y in range(H):
        for x in range(W):
            base = g[y][x]
            for t in (DIRT, PAVE, WATER):
                if t <= base:
                    continue
                style = STYLE[t]
                lays = []
                n_, s_, e_, w_ = (at(g,x,y-1)==t, at(g,x,y+1)==t, at(g,x+1,y)==t, at(g,x-1,y)==t)
                if n_: lays.append(F("edge", style, 0))
                if e_: lays.append(F("edge", style, 1))
                if s_: lays.append(F("edge", style, 2))
                if w_: lays.append(F("edge", style, 3))
                for (a, b, r) in ((n_, e_, 0), (e_, s_, 1), (s_, w_, 2), (w_, n_, 3)):
                    if a and b:
                        lays.append(F("corner_in", style, r))
                diags = ((-1,-1,3),(1,-1,0),(1,1,1),(-1,1,2))
                for dx, dy, r in diags:
                    if at(g, x+dx, y+dy) == t:
                        ox = at(g, x+dx, y) == t; oy = at(g, x, y+dy) == t
                        if not ox and not oy:
                            lays.append(F("corner_out", style, r))
                for f in lays:
                    src = tiles[t]
                    for py in range(T):
                        row = f[py]
                        for px in range(T):
                            if row[px] > 0:
                                put(canvas, x * T + px, y * T + py, get(src, px, py))
    return canvas


def zoom(im, x0, y0, w, h, k):
    out = img_new(w * k, h * k)
    for y in range(h * k):
        for x in range(w * k):
            put(out, x, y, get(im, x0 + x // k, y0 + y // k))
    return out


def main():
    g = build_map()
    sw, raw, heal = swatches()
    ds = {"flower": [], "pebble": [], "tuft": []}
    from assets import key_decals as kd
    cells = raw
    def white_or_red(c):
        r, gg, b, _ = c
        return (r > 170 and gg > 170 and b > 150 and abs(r - gg) < 60) or (r > 130 and r > gg + 35 and r > b + 35)
    def grey_stone(c):
        r, gg, b, _ = c
        mx, mn = max(r, gg, b), min(r, gg, b)
        return mx - mn < 34 and 110 < mx < 235
    def dark_tuft(c):
        r, gg, b, _ = c
        return gg > r + 18 and gg < 150 and b < 120
    sc = lambda im, n: [resize(d, max(1, d[0] * T // 256), max(1, d[1] * T // 256)) for d in im][:n]
    # decals are drawn at 256-per-tile on the sheet; the map is T per tile
    def shrink(dlist):
        out = []
        for d in dlist:
            nw, nh = max(6, d[0] * T // 256 * 2), max(6, d[1] * T // 256 * 2)
            s = img_new(nw, nh)
            for y in range(nh):
                for x in range(nw):
                    c = get(d, x * d[0] // nw, y * d[1] // nh)
                    put(s, x, y, c)
            out.append(s)
        return out
    ds["flower"] = shrink(kd(cells["grass_flower"], white_or_red, 40, 2600))
    ds["pebble"] = shrink(kd(cells["dirt"], grey_stone, 120, 4200))
    ds["tuft"] = shrink(kd(cells["grass_tuft"], dark_tuft, 220, 5200))
    print("decals", {k: len(v) for k, v in ds.items()})

    new = render_new(g, sw, ds)
    save(new, "new.png"); print("new.png")
    old = render_old(g, heal)
    save(old, "old.png"); print("old.png")
    save(zoom(new, 3 * T, 2 * T, 7 * T, 4 * T, 2), "new_detail.png")
    save(zoom(old, 3 * T, 2 * T, 7 * T, 4 * T, 2), "old_detail.png")
    print("details")


if __name__ == "__main__":
    main()
