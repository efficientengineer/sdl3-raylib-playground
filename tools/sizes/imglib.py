"""Shared image helpers for the size study. Python 3 stdlib only.

Images are carried as a dict: {"w","h","px"} where px is a flat list of (r,g,b,a) tuples.
Small enough for the assets in this repo (largest single image measured is ~900x550).
"""
import math
import os
import struct
import subprocess
import sys
import zlib
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "tools" / "palette"))
import pngio            # noqa: E402  (read-only reuse)
import oklab            # noqa: E402

OUT = Path(__file__).resolve().parent / "out"
OUT.mkdir(exist_ok=True)

# ---------------------------------------------------------------- lab cache
_LAB = {}


def lab(r, g, b):
    k = (r << 16) | (g << 8) | b
    v = _LAB.get(k)
    if v is None:
        v = oklab.srgb_to_oklab(r, g, b)
        _LAB[k] = v
    return v


def de(p, q):
    return math.sqrt((p[0] - q[0]) ** 2 + (p[1] - q[1]) ** 2 + (p[2] - q[2]) ** 2)


# ---------------------------------------------------------------- image io
def load(path):
    w, h, ch, rows = pngio.read_png(path)
    px = []
    for row in rows:
        if ch == 4:
            for x in range(w):
                i = x * 4
                px.append((row[i], row[i + 1], row[i + 2], row[i + 3]))
        else:
            for x in range(w):
                i = x * 3
                px.append((row[i], row[i + 1], row[i + 2], 255))
    return {"w": w, "h": h, "px": px}


def load_crop(path, x0, y0, w, h):
    """Crop straight out of the file, so a 2048x3072 atlas never becomes 6M tuples."""
    W, H, ch, rows = pngio.read_png(path)
    px = []
    for y in range(y0, y0 + h):
        row = rows[y]
        for x in range(x0, x0 + w):
            i = x * ch
            px.append((row[i], row[i + 1], row[i + 2], row[i + 3] if ch == 4 else 255))
    return {"w": w, "h": h, "px": px}


def save(img, path):
    w, h, px = img["w"], img["h"], img["px"]
    rows = []
    for y in range(h):
        r = bytearray()
        for p in px[y * w:(y + 1) * w]:
            r += bytes(p)
        rows.append(r)
    pngio.write_png(path, w, h, 4, rows)


def crop(img, x0, y0, w, h):
    W = img["w"]
    px = img["px"]
    out = []
    for y in range(y0, y0 + h):
        out.extend(px[y * W + x0:y * W + x0 + w])
    return {"w": w, "h": h, "px": out}


def blank(w, h, col=(0, 0, 0, 255)):
    return {"w": w, "h": h, "px": [col] * (w * h)}


def paste(dst, src, x0, y0):
    W = dst["w"]
    for y in range(src["h"]):
        ty = y0 + y
        if ty < 0 or ty >= dst["h"]:
            continue
        for x in range(src["w"]):
            tx = x0 + x
            if 0 <= tx < W:
                dst["px"][ty * W + tx] = src["px"][y * src["w"] + x]


# ---------------------------------------------------------------- resampling
def box_down(img, nw, nh):
    """Area-average downscale (also works as a generic box resample down)."""
    w, h, px = img["w"], img["h"], img["px"]
    if nw == w and nh == h:
        return {"w": w, "h": h, "px": list(px)}
    out = [None] * (nw * nh)
    xs = [(int(x * w / nw), max(int(x * w / nw) + 1, int((x + 1) * w / nw))) for x in range(nw)]
    ys = [(int(y * h / nh), max(int(y * h / nh) + 1, int((y + 1) * h / nh))) for y in range(nh)]
    for oy in range(nh):
        y0, y1 = ys[oy]
        for ox in range(nw):
            x0, x1 = xs[ox]
            sr = sg = sb = sa = 0
            n = 0
            for y in range(y0, min(y1, h)):
                base = y * w
                for x in range(x0, min(x1, w)):
                    r, g, b, a = px[base + x]
                    # premultiply so transparent pixels don't bleed colour
                    sr += r * a
                    sg += g * a
                    sb += b * a
                    sa += a
                    n += 1
            if sa == 0:
                out[oy * nw + ox] = (0, 0, 0, 0)
            else:
                out[oy * nw + ox] = (min(255, sr // sa), min(255, sg // sa), min(255, sb // sa), sa // n)
    return {"w": nw, "h": nh, "px": out}


def bilinear(img, nw, nh):
    """Bilinear resample (what the GPU does stored -> display)."""
    w, h, px = img["w"], img["h"], img["px"]
    out = [None] * (nw * nh)
    for oy in range(nh):
        fy = (oy + 0.5) * h / nh - 0.5
        y0 = int(math.floor(fy))
        ty = fy - y0
        y0c = min(max(y0, 0), h - 1)
        y1c = min(max(y0 + 1, 0), h - 1)
        for ox in range(nw):
            fx = (ox + 0.5) * w / nw - 0.5
            x0 = int(math.floor(fx))
            tx = fx - x0
            x0c = min(max(x0, 0), w - 1)
            x1c = min(max(x0 + 1, 0), w - 1)
            p00 = px[y0c * w + x0c]
            p10 = px[y0c * w + x1c]
            p01 = px[y1c * w + x0c]
            p11 = px[y1c * w + x1c]
            c = []
            for i in range(4):
                a = p00[i] + (p10[i] - p00[i]) * tx
                b = p01[i] + (p11[i] - p01[i]) * tx
                c.append(int(a + (b - a) * ty + 0.5))
            out[oy * nw + ox] = tuple(min(255, max(0, v)) for v in c)
    return {"w": nw, "h": nh, "px": out}


def resample(img, nw, nh):
    """Down -> area average; up -> bilinear."""
    if nw <= img["w"] and nh <= img["h"]:
        return box_down(img, nw, nh)
    return bilinear(img, nw, nh)


def nearest(img, k):
    w, h, px = img["w"], img["h"], img["px"]
    nw, nh = w * k, h * k
    out = [None] * (nw * nh)
    for y in range(nh):
        sy = y // k
        for x in range(nw):
            out[y * nw + x] = px[sy * w + x // k]
    return {"w": nw, "h": nh, "px": out}


def snap_grid(img, pitch, per_art=1):
    """One stored pixel per art pixel (mode colour of each pitch x pitch block), or `per_art` per."""
    w, h, px = img["w"], img["h"], img["px"]
    nw = max(1, int(round(w / pitch))) * per_art
    nh = max(1, int(round(h / pitch))) * per_art
    cells_x, cells_y = nw // per_art, nh // per_art
    out = [None] * (nw * nh)
    for cy in range(cells_y):
        y0, y1 = int(cy * h / cells_y), max(int(cy * h / cells_y) + 1, int((cy + 1) * h / cells_y))
        for cx in range(cells_x):
            x0, x1 = int(cx * w / cells_x), max(int(cx * w / cells_x) + 1, int((cx + 1) * w / cells_x))
            counts = {}
            for y in range(y0, min(y1, h)):
                base = y * w
                for x in range(x0, min(x1, w)):
                    p = px[base + x]
                    counts[p] = counts.get(p, 0) + 1
            best = max(counts.items(), key=lambda kv: kv[1])[0]
            for sy in range(per_art):
                for sx in range(per_art):
                    out[(cy * per_art + sy) * nw + cx * per_art + sx] = best
    return {"w": nw, "h": nh, "px": out}


# ---------------------------------------------------------------- art-pixel pitch
def _runs(px, w, h, horiz, tol=0.02, step=1):
    """Run lengths of near-constant colour (dE < tol in Oklab)."""
    hist = {}
    if horiz:
        lines = range(0, h, step)
        for y in lines:
            base = y * w
            prev = None
            run = 0
            for x in range(w):
                p = px[base + x]
                c = lab(p[0], p[1], p[2]) if p[3] > 8 else None
                if prev is not None and c is not None and de(prev, c) < tol:
                    run += 1
                else:
                    if run > 0:
                        hist[run] = hist.get(run, 0) + 1
                    run = 1 if c is not None else 0
                    prev = c
                    continue
                prev = c
            if run:
                hist[run] = hist.get(run, 0) + 1
    else:
        for x in range(0, w, step):
            prev = None
            run = 0
            for y in range(h):
                p = px[y * w + x]
                c = lab(p[0], p[1], p[2]) if p[3] > 8 else None
                if prev is not None and c is not None and de(prev, c) < tol:
                    run += 1
                else:
                    if run > 0:
                        hist[run] = hist.get(run, 0) + 1
                    run = 1 if c is not None else 0
                    prev = c
                    continue
                prev = c
            if run:
                hist[run] = hist.get(run, 0) + 1
    return hist


def pitch(img, tol=0.02, maxp=12):
    """Estimate the fat-pixel pitch.

    Two independent estimators, reported together:
      * run mode: the smallest run length that is a strong mode of the run histogram
        (counting multiples, since an N-wide art pixel next to an identical one makes a 2N run).
      * gradient autocorrelation: the lag maximising the correlation of the column/row
        gradient-magnitude profile, which is the period of the art-pixel grid.
    """
    w, h, px = img["w"], img["h"], img["px"]
    step = max(1, min(w, h) // 160)
    hist = _runs(px, w, h, True, tol, step)
    for k, v in _runs(px, w, h, False, tol, step).items():
        hist[k] = hist.get(k, 0) + v
    tot = sum(n * c for n, c in hist.items()) or 1
    # score p by the mass sitting on multiples of p (tolerating +-0 exactly)
    scores = {}
    for p in range(1, maxp + 1):
        mass = sum(n * c for n, c in hist.items() if n >= p and n % p == 0)
        # penalise p=1 style trivial fits by requiring runs >= p to dominate
        long_mass = sum(n * c for n, c in hist.items() if n >= p) or 1
        scores[p] = mass / long_mass * (long_mass / tot)
    best_run = max(scores.items(), key=lambda kv: (kv[1], kv[0]))[0]
    # refine: prefer the smallest p within 3% of the best score
    for p in range(1, best_run + 1):
        if scores[p] >= scores[best_run] * 0.97:
            best_run = p
            break
    return best_run, scores, hist, _autocorr_pitch(img, maxp)


def grid_pitch(img, maxp=12, thr=0.06):
    """Fat-pixel pitch by edge-phase alignment.

    Fat pixels put every colour change on a lattice. For each candidate pitch p we find
    the offset that captures the most edge mass and report the captured fraction; a true
    fat-pixel image scores near 1.0 at its pitch and ~1/p at every wrong one. The score
    reported is the *lift* over chance, so p=1 is always 1.0-chance = 0.
    """
    w, h, px = img["w"], img["h"], img["px"]
    xe = [0.0] * w
    ye = [0.0] * h
    sx = max(1, h // 200)
    sy = max(1, w // 200)
    for y in range(0, h, sx):
        base = y * w
        for x in range(1, w):
            a, b = px[base + x], px[base + x - 1]
            if a[3] < 8 or b[3] < 8:
                continue
            xe[x] += de(lab(a[0], a[1], a[2]), lab(b[0], b[1], b[2]))
    for x in range(0, w, sy):
        for y in range(1, h):
            a, b = px[y * w + x], px[(y - 1) * w + x]
            if a[3] < 8 or b[3] < 8:
                continue
            ye[y] += de(lab(a[0], a[1], a[2]), lab(b[0], b[1], b[2]))
    out = {}
    for name, sig in (("x", xe), ("y", ye)):
        tot = sum(sig) or 1.0
        best = (0.0, 1)
        for p in range(2, maxp + 1):
            frac = max(sum(sig[i] for i in range(o, len(sig), p)) for o in range(p)) / tot
            lift = (frac - 1.0 / p) / (1.0 - 1.0 / p)
            if lift > best[0]:
                best = (lift, p)
        out[name] = (best[1], round(best[0], 3))
    return out


def _autocorr_pitch(img, maxp=12):
    """Period of the gradient-magnitude profile along x and y."""
    w, h, px = img["w"], img["h"], img["px"]
    prof = {}
    for axis in (0, 1):
        n = w if axis == 0 else h
        g = [0.0] * n
        stepo = max(1, (h if axis == 0 else w) // 120)
        for i in range(1, n):
            s = 0.0
            cnt = 0
            rng = range(0, h, stepo) if axis == 0 else range(0, w, stepo)
            for j in rng:
                a = px[j * w + i] if axis == 0 else px[i * w + j]
                b = px[j * w + i - 1] if axis == 0 else px[(i - 1) * w + j]
                if a[3] < 8 or b[3] < 8:
                    continue
                s += de(lab(a[0], a[1], a[2]), lab(b[0], b[1], b[2]))
                cnt += 1
            g[i] = s / cnt if cnt else 0.0
        m = sum(g) / len(g)
        gg = [v - m for v in g]
        denom = sum(v * v for v in gg) or 1.0
        cs = {}
        for lag in range(2, maxp + 1):
            cs[lag] = sum(gg[i] * gg[i - lag] for i in range(lag, n)) / denom
        bv = max(cs.values())
        best = max(cs, key=lambda k: cs[k])
        # the fundamental, not a harmonic: the smallest lag within 80% of the peak
        for lag in range(2, best + 1):
            if cs[lag] >= bv * 0.8:
                best = lag
                break
        prof[axis] = (best, round(bv, 3))
    return prof


# ---------------------------------------------------------------- metrics
def compare(a, b):
    """a, b same size. Returns mean dE, p95 dE, %>0.05, edge ratio (b/a)."""
    assert a["w"] == b["w"] and a["h"] == b["h"]
    w, h = a["w"], a["h"]
    ds = []
    for pa, pb in zip(a["px"], b["px"]):
        if pa[3] < 8 and pb[3] < 8:
            continue
        la = lab(pa[0], pa[1], pa[2])
        lb = lab(pb[0], pb[1], pb[2])
        d = de(la, lb)
        # alpha difference counts as luminance-scale error
        d = math.sqrt(d * d + ((pa[3] - pb[3]) / 255.0) ** 2)
        ds.append(d)
    ds.sort()
    if not ds:
        return 0.0, 0.0, 0.0, 1.0
    mean = sum(ds) / len(ds)
    p95 = ds[int(len(ds) * 0.95) - 1]
    over = sum(1 for d in ds if d > 0.05) / len(ds)
    return mean, p95, over, edge(b) / (edge(a) or 1e-9)


def edge(img):
    w, h, px = img["w"], img["h"], img["px"]
    s, n = 0.0, 0
    for y in range(1, h):
        for x in range(1, w):
            p = px[y * w + x]
            a = px[y * w + x - 1]
            b = px[(y - 1) * w + x]
            if p[3] < 8:
                continue
            lp = lab(p[0], p[1], p[2])
            gx = de(lp, lab(a[0], a[1], a[2])) if a[3] >= 8 else 0.0
            gy = de(lp, lab(b[0], b[1], b[2])) if b[3] >= 8 else 0.0
            s += math.hypot(gx, gy)
            n += 1
    return s / n if n else 0.0


# ---------------------------------------------------------------- sizes on disk
def png_bytes(img):
    """RGBA PNG bytes (zlib 9), what we ship today."""
    w, h = img["w"], img["h"]
    raw = b""
    rows = []
    for y in range(h):
        r = bytearray(b"\x00")
        for p in img["px"][y * w:(y + 1) * w]:
            r += bytes(p)
        rows.append(bytes(r))
    raw = b"".join(rows)
    return 8 + 25 + 12 + len(zlib.compress(raw, 9))


def quantise(img, ncol=256):
    """Median-cut to <=ncol colours; returns (index_rows, palette, alpha, quantised image)."""
    px = img["px"]
    has_alpha = any(p[3] < 250 for p in px)
    # bucket by RGB (alpha binarised: the engine alpha-tests)
    box = [[p[0], p[1], p[2], (0 if p[3] < 128 else 255)] for p in px]
    opaque = [c for c in box if c[3]]
    if not opaque:
        opaque = box
    slots = ncol - (1 if has_alpha else 0)
    boxes = [opaque]
    while len(boxes) < slots:
        boxes.sort(key=lambda b: -(_spread(b) * len(b)))
        b = boxes.pop(0)
        if len(b) < 2 or _spread(b) == 0:
            boxes.append(b)
            break
        ch = _widest(b)
        b.sort(key=lambda c: c[ch])
        boxes.append(b[:len(b) // 2])
        boxes.append(b[len(b) // 2:])
    pal = []
    for b in boxes:
        n = len(b)
        pal.append((sum(c[0] for c in b) // n, sum(c[1] for c in b) // n, sum(c[2] for c in b) // n))
    if has_alpha:
        pal.append((0, 0, 0))
    alpha = [255] * len(pal)
    if has_alpha:
        alpha[-1] = 0
    labpal = [lab(*c) for c in pal[:len(pal) - (1 if has_alpha else 0)]]
    cache = {}
    w, h = img["w"], img["h"]
    rows = []
    qpx = []
    for y in range(h):
        r = bytearray()
        for p in px[y * w:(y + 1) * w]:
            if has_alpha and p[3] < 128:
                r.append(len(pal) - 1)
                qpx.append((0, 0, 0, 0))
                continue
            k = (p[0] << 16) | (p[1] << 8) | p[2]
            i = cache.get(k)
            if i is None:
                lp = lab(p[0], p[1], p[2])
                i = min(range(len(labpal)), key=lambda j: de(lp, labpal[j]))
                cache[k] = i
            r.append(i)
            qpx.append((pal[i][0], pal[i][1], pal[i][2], 255))
        rows.append(r)
    return rows, pal, (alpha if has_alpha else None), {"w": w, "h": h, "px": qpx}


def _spread(b):
    return max(max(c[i] for c in b) - min(c[i] for c in b) for i in range(3))


def _widest(b):
    return max(range(3), key=lambda i: max(c[i] for c in b) - min(c[i] for c in b))


def indexed_bytes(img, ncol=256):
    rows, pal, alpha, _ = quantise(img, ncol)
    raw = b"".join(b"\x00" + bytes(r) for r in rows)
    body = len(zlib.compress(raw, 9)) + len(pal) * 3 + (len(alpha) if alpha else 0)
    return 8 + 25 + 12 + 12 + body + (12 if alpha else 0)
