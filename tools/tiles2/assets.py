"""tiles2 prototype: cut swatches and decals from the clean-style test sheet.

stdlib only. Outputs to tools/tiles2/out/.
"""
import sys, math
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "tools" / "palette"))
import pngio  # noqa: E402

OUT = ROOT / "tools" / "tiles2" / "out"
OUT.mkdir(parents=True, exist_ok=True)
SHEET = ROOT / "story" / "sheets" / "tests" / "clean_style_test_v1.png"
CELL = 256


# ---------- tiny image type: (w, h, list of bytearray RGBA) ----------

def img_new(w, h, rgba=(0, 0, 0, 0)):
    row = bytearray(bytes(rgba) * w)
    return [w, h, [bytearray(row) for _ in range(h)]]


def get(im, x, y):
    w, h, rows = im
    r = rows[y]
    i = x * 4
    return r[i], r[i + 1], r[i + 2], r[i + 3]


def put(im, x, y, c):
    r = im[2][y]
    i = x * 4
    r[i], r[i + 1], r[i + 2], r[i + 3] = c


def load_sheet():
    w, h, ch, rows = pngio.read_png(SHEET)
    if ch == 3:
        rows = [bytearray(b for i in range(w) for b in (r[i * 3], r[i * 3 + 1], r[i * 3 + 2], 255))
                for r in rows]
    return [w, h, [bytearray(r) for r in rows]]


def crop(im, x0, y0, w, h):
    out = img_new(w, h)
    for y in range(h):
        src = im[2][y0 + y]
        out[2][y][:] = src[(x0 + 0) * 4:(x0 + w) * 4]
    return out


def resize(im, nw, nh):
    """Box filter down, bilinear up. RGB only (these are opaque swatches)."""
    w, h, _ = im
    out = img_new(nw, nh)
    if nw <= w and nh <= h:
        for oy in range(nh):
            y0, y1 = oy * h // nh, max(oy * h // nh + 1, (oy + 1) * h // nh)
            for ox in range(nw):
                x0, x1 = ox * w // nw, max(ox * w // nw + 1, (ox + 1) * w // nw)
                sr = sg = sb = n = 0
                for y in range(y0, y1):
                    row = im[2][y]
                    for x in range(x0, x1):
                        i = x * 4
                        sr += row[i]; sg += row[i + 1]; sb += row[i + 2]; n += 1
                put(out, ox, oy, (sr // n, sg // n, sb // n, 255))
    else:
        for oy in range(nh):
            fy = (oy + 0.5) * h / nh - 0.5
            y0 = max(0, min(h - 1, int(math.floor(fy)))); y1 = min(h - 1, y0 + 1); ty = fy - y0
            for ox in range(nw):
                fx = (ox + 0.5) * w / nw - 0.5
                x0 = max(0, min(w - 1, int(math.floor(fx)))); x1 = min(w - 1, x0 + 1); tx = fx - x0
                c = []
                for k in range(3):
                    a = get(im, x0, y0)[k] * (1 - tx) + get(im, x1, y0)[k] * tx
                    b = get(im, x0, y1)[k] * (1 - tx) + get(im, x1, y1)[k] * tx
                    c.append(int(a * (1 - ty) + b * ty))
                put(out, ox, oy, (c[0], c[1], c[2], 255))
    return out


def save(im, name):
    w, h, rows = im
    pngio.write_png(OUT / name, w, h, 4, rows)


# ---------- seamless: offset the tile by half, then heal the cross seam ----------

def roll(im, dx, dy):
    w, h, rows = im
    out = img_new(w, h)
    for y in range(h):
        sy = (y - dy) % h
        src = rows[sy]
        for x in range(w):
            sx = (x - dx) % w
            i, j = x * 4, sx * 4
            out[2][y][i:i + 4] = src[j:j + 4]
    return out


def flatten(im, r=40, strength=0.9, wrap=False):
    """Remove the drawing's low-frequency lighting so it can tile: subtract a heavy
    box blur's deviation from the image mean. A swatch drawn with a bright side or a
    grassy border will otherwise band wherever it repeats."""
    w, h, _ = im
    # mirror-pad by r so the blur near an edge is not biased by a truncated window:
    # a clamped window leaves a bright rim, and the roll puts that rim in the middle
    # of the tile where it reads as a pale cross at every repeat.
    def mir(v, n):
        if wrap:
            return v % n
        v = abs(v)
        return n - 1 - abs(n - 1 - (v % (2 * n - 2))) if n > 1 else 0
    pw, ph = w + 2 * r, h + 2 * r
    acc = [[[0, 0, 0] for _ in range(pw + 1)] for _ in range(ph + 1)]
    for py in range(ph):
        rowsum = [0, 0, 0]
        sy = mir(py - r, h)
        for px in range(pw):
            c = get(im, mir(px - r, w), sy)
            for k in range(3):
                rowsum[k] += c[k]
                acc[py + 1][px + 1][k] = acc[py][px + 1][k] + rowsum[k]
    tot = []
    for k in range(3):
        s = 0
        for y in range(h):
            for x in range(w):
                s += get(im, x, y)[k]
        tot.append(s / (w * h))
    out = img_new(w, h)
    for y in range(h):
        y0, y1 = y, y + 2 * r + 1
        for x in range(w):
            x0, x1 = x, x + 2 * r + 1
            n = (x1 - x0) * (y1 - y0)
            c = get(im, x, y)
            v = []
            for k in range(3):
                m = (acc[y1][x1][k] - acc[y0][x1][k] - acc[y1][x0][k] + acc[y0][x0][k]) / n
                v.append(max(0, min(255, int(c[k] - strength * (m - tot[k])))))
            put(out, x, y, tuple(v) + (255,))
    return out


def _diff(a, b):
    return abs(a[0] - b[0]) + abs(a[1] - b[1]) + abs(a[2] - b[2])


def _mincut_v(im, xc, band):
    """Minimum-error cut path through the vertical seam at column xc.

    Returns one cut column per row. Cost of cutting at column x on row y is how
    different that column's two sides are; the path may move one column a row.
    """
    w, h, _ = im
    xs = list(range(max(1, xc - band), min(w - 1, xc + band) + 1))
    cost = [[_diff(get(im, x - 1, y), get(im, x, y)) for x in xs] for y in range(h)]
    dp = [list(cost[0])]
    back = []
    for y in range(1, h):
        prev, cur, bk = dp[-1], [], []
        for i in range(len(xs)):
            best, bi = prev[i], i
            if i and prev[i - 1] < best:
                best, bi = prev[i - 1], i - 1
            if i + 1 < len(xs) and prev[i + 1] < best:
                best, bi = prev[i + 1], i + 1
            cur.append(cost[y][i] + best)
            bk.append(bi)
        dp.append(cur); back.append(bk)
    i = min(range(len(xs)), key=lambda k: dp[-1][k])
    path = [0] * h
    for y in range(h - 1, -1, -1):
        path[y] = xs[i]
        if y:
            i = back[y - 1][i]
    return path


def _transpose(im):
    w, h, _ = im
    out = img_new(h, w)
    for y in range(h):
        for x in range(w):
            put(out, y, x, get(im, x, y))
    return out


def make_seamless(im, band=None, feather=2):
    """The tile is rolled by half, so its outer edges were adjacent lines of the
    original drawing and the wrap is continuous by construction; the drawing's own
    discontinuity is now a cross through the middle, and that cross is hidden with a
    minimum-error cut (image-quilting style) rather than a blend, so nothing is blurred.
    """
    w, h, _ = im
    band = band or max(8, w // 5)
    r = roll(im, w // 2, h // 2)
    # The roll has made the image periodic except for the cross through its middle;
    # flatten it now, with a WRAPPED blur, so the low-frequency correction is itself
    # periodic and leaves no pale band at the repeat.
    r = flatten(r, r=max(8, w // 6), wrap=True)
    # vertical seam
    out = [w, h, [bytearray(row) for row in r[2]]]
    for axis in (0, 1):
        src = out if axis == 0 else _transpose(out)
        W2, H2, _ = src
        path = _mincut_v(src, W2 // 2, band)
        # shift each row so the jump lands on the chosen column: nothing to shift —
        # instead feather a couple of pixels across the cut to kill the 1px step.
        # The cut itself moves no pixels; all it can leave is a one-pixel step, and
        # that one column is softened against its two neighbours. Nothing else is
        # touched, so a flat man-made texture keeps its contrast (a wide cross-fade
        # here is exactly what read as a pale ribbon at every repeat).
        fixed = [W2, H2, [bytearray(row) for row in src[2]]]
        for y in range(H2):
            xc = path[y]
            if not (0 < xc < W2 - 1):
                continue
            a, b, c = get(src, xc - 1, y), get(src, xc + 1, y), get(src, xc, y)
            put(fixed, xc, y,
                tuple((a[i] + b[i] + 2 * c[i]) // 4 for i in range(3)) + (255,))
        out = fixed if axis == 0 else _transpose(fixed)
    return out


# ---------- decal keying ----------

def blobs(mask, w, h, lo, hi):
    seen = [bytearray(w) for _ in range(h)]
    found = []
    for y in range(h):
        for x in range(w):
            if not mask[y][x] or seen[y][x]:
                continue
            stack = [(x, y)]
            seen[y][x] = 1
            pix = []
            while stack:
                cx, cy = stack.pop()
                pix.append((cx, cy))
                for nx, ny in ((cx+1,cy),(cx-1,cy),(cx,cy+1),(cx,cy-1)):
                    if 0 <= nx < w and 0 <= ny < h and mask[ny][nx] and not seen[ny][nx]:
                        seen[ny][nx] = 1
                        stack.append((nx, ny))
            if lo <= len(pix) <= hi:
                found.append(pix)
    return found


def key_decals(cell, test, lo, hi, pad=3, merge=10):
    w, h, _ = cell
    mask = [bytearray(w) for _ in range(h)]
    for y in range(h):
        for x in range(w):
            if test(get(cell, x, y)):
                mask[y][x] = 1
    # dilate a little so a flower and its neighbour pixels join up
    for _ in range(2):
        nm = [bytearray(r) for r in mask]
        for y in range(h):
            for x in range(w):
                if mask[y][x]:
                    for dy in (-1, 0, 1):
                        for dx in (-1, 0, 1):
                            if 0 <= x+dx < w and 0 <= y+dy < h:
                                nm[y+dy][x+dx] = 1
        mask = nm
    out = []
    for pix in blobs(mask, w, h, lo, hi):
        xs = [p[0] for p in pix]; ys = [p[1] for p in pix]
        x0, x1 = min(xs) - pad, max(xs) + pad
        y0, y1 = min(ys) - pad, max(ys) + pad
        if x0 < 0 or y0 < 0 or x1 >= w or y1 >= h:
            continue
        bw, bh = x1 - x0 + 1, y1 - y0 + 1
        if bw > 110 or bh > 110:
            continue
        d = img_new(bw, bh)
        s = set(pix)
        for (px, py) in pix:
            put(d, px - x0, py - y0, get(cell, px, py)[:3] + (255,))
        # soften: any pixel next to a kept one inside the box joins at full alpha
        for y in range(bh):
            for x in range(bw):
                if get(d, x, y)[3]:
                    continue
                near = any((x0 + x + dx, y0 + y + dy) in s
                           for dy in (-1, 0, 1) for dx in (-1, 0, 1))
                if near:
                    put(d, x, y, get(cell, x0 + x, y0 + y)[:3] + (255,))
        out.append(d)
    return out


def main():
    sheet = load_sheet()
    names = {"grass": (0, 0), "grass_flower": (1, 0), "grass_tuft": (2, 0),
             "dirt": (3, 0), "paving": (4, 0), "water": (5, 0)}
    cells = {n: crop(sheet, cx * CELL, cy * CELL, CELL, CELL) for n, (cx, cy) in names.items()}
    for n, c in cells.items():
        save(c, f"cell_{n}.png")
        save(make_seamless(c), f"swatch_{n}.png")

    # decals
    def white_or_red(c):
        r, g, b, _ = c
        return (r > 170 and g > 170 and b > 150 and abs(r - g) < 60) or (r > 130 and r > g + 35 and r > b + 35)

    def grey_stone(c):
        r, g, b, _ = c
        mx, mn = max(r, g, b), min(r, g, b)
        return mx - mn < 34 and 110 < mx < 235

    def dark_tuft(c):
        r, g, b, _ = c
        return g > r + 18 and g < 150 and b < 120

    decals = {}
    decals["flower"] = key_decals(cells["grass_flower"], white_or_red, 40, 2600)
    decals["pebble"] = key_decals(cells["dirt"], grey_stone, 120, 4200)
    decals["tuft"] = key_decals(cells["grass_tuft"], dark_tuft, 220, 5200)
    for kind, ds in decals.items():
        for i, d in enumerate(ds[:12]):
            save(d, f"decal_{kind}_{i}.png")
        print(f"{kind}: {len(ds)} blobs kept, sizes "
              f"{[f'{d[0]}x{d[1]}' for d in ds[:12]]}")

    # contact sheet
    cs = img_new(900, 260, (40, 40, 48, 255))
    x, y, rowh = 4, 4, 0
    for kind, ds in decals.items():
        for d in ds[:12]:
            if x + d[0] > 896:
                x = 4; y += rowh + 4; rowh = 0
            for yy in range(d[1]):
                for xx in range(d[0]):
                    c = get(d, xx, yy)
                    if c[3] and y + yy < 260:
                        put(cs, x + xx, y + yy, c)
            x += d[0] + 4
            rowh = max(rowh, d[1])
    save(cs, "decals_preview.png")


if __name__ == "__main__":
    main()
