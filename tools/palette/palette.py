#!/usr/bin/env python3
"""256-colour master palette experiment.

  palette.py build                       build master palette from story/sheets + story/refs
  palette.py apply <in> <out> [opts]     convert an image to the master palette
  palette.py compare <in> <out> [opts]   side-by-side original | converted
  palette.py crop <in> <out> x y w h [opts]   3x nearest zoom, side by side

opts: --dither none|bayer4|bayer8   --alpha hard|keep   --indexed
"""
import json
import math
import os
import random
import struct
import subprocess
import sys
import time
from pathlib import Path

HERE = Path(__file__).resolve().parent
ROOT = HERE.parent.parent
OUT = HERE / "out"
CACHE = HERE / "out" / "_cache"
VARIANT = "master"          # "master" or "master_ramps"


def master_path():
    return HERE / f"{VARIANT}.json"


def lut_path():
    return OUT / f"lut32_{VARIANT}.bin"

sys.path.insert(0, str(HERE))
from pngio import read_png, write_png, write_indexed_png          # noqa: E402
from oklab import srgb_to_oklab, oklab_to_srgb, de, hue, chroma   # noqa: E402

# ─────────────────────────── corpus & categories ───────────────────────────

BUDGET = {"field": 96, "cast": 80, "panel": 79}


def categorise(p):
    n = p.name
    if p.parent.name == "refs":
        return "cast"
    if n.startswith("cast-"):
        return "cast"
    if "scenes-" in n or n.startswith("003_the_warning"):
        return "panel"
    return "field"


def corpus():
    files = sorted((ROOT / "story" / "sheets").glob("*.png")) + \
            sorted((ROOT / "story" / "refs").glob("*.png"))
    # *_walk.png are pure-green walkability masks, not art
    return [f for f in files if not f.name.startswith("_") and not f.stem.endswith("_walk")]


def thumb(path, width=220):
    """sips-downscaled copy, cached. Native = fast."""
    CACHE.mkdir(parents=True, exist_ok=True)
    dst = CACHE / (path.parent.name + "__" + path.name)
    if not dst.exists() or dst.stat().st_mtime < path.stat().st_mtime:
        subprocess.run(["sips", "--resampleWidth", str(width), str(path), "--out", str(dst)],
                       check=True, capture_output=True)
    return dst


# ─────────────────────────── pixel filtering ───────────────────────────

def keep(r, g, b):
    """Drop template magenta, near-white borders/numbers, pure black sheet background."""
    if min(r, b) - g > 40 and r > 90 and b > 90:      # magenta cast (template key)
        return False
    if r > 236 and g > 236 and b > 236:               # border / slot numbers
        return False
    if r < 12 and g < 12 and b < 12:                  # sheet background
        return False
    return True


# ─────────────────────────── k-means++ in Oklab ───────────────────────────

def histogram(paths):
    """5-bit RGB buckets -> [L,a,b,w,sr,sg,sb] using true mean colour per bucket."""
    h = {}
    for p in paths:
        w, ht, ch, rows = read_png(thumb(p))
        for y in range(ht):
            row = rows[y]
            for x in range(0, w * ch, ch):
                r, g, b = row[x], row[x + 1], row[x + 2]
                if ch == 4 and row[x + 3] < 128:
                    continue
                if not keep(r, g, b):
                    continue
                k = (r >> 3) << 10 | (g >> 3) << 5 | (b >> 3)
                e = h.get(k)
                if e is None:
                    h[k] = [1, r, g, b]
                else:
                    e[0] += 1
                    e[1] += r
                    e[2] += g
                    e[3] += b
    pts = []
    for n, sr, sg, sb in h.values():
        r, g, b = sr / n, sg / n, sb / n
        L, A, B = srgb_to_oklab(int(r), int(g), int(b))
        pts.append((L, A, B, n, r, g, b))
    return pts


def kmeans(pts, k, iters=8, seed=7):
    rnd = random.Random(seed)
    if len(pts) <= k:
        return [(p[0], p[1], p[2]) for p in pts]
    # kmeans++ init, weighted
    cents = []
    first = max(pts, key=lambda p: p[3] * rnd.random())
    cents.append((first[0], first[1], first[2]))
    d2 = [((p[0] - cents[0][0]) ** 2 + (p[1] - cents[0][1]) ** 2 + (p[2] - cents[0][2]) ** 2) * p[3]
          for p in pts]
    while len(cents) < k:
        tot = sum(d2)
        if tot <= 0:
            cents.append((pts[rnd.randrange(len(pts))][0:3]))
            continue
        t, acc, pick = rnd.random() * tot, 0.0, len(pts) - 1
        for i, v in enumerate(d2):
            acc += v
            if acc >= t:
                pick = i
                break
        c = (pts[pick][0], pts[pick][1], pts[pick][2])
        cents.append(c)
        for i, p in enumerate(pts):
            nd = ((p[0] - c[0]) ** 2 + (p[1] - c[1]) ** 2 + (p[2] - c[2]) ** 2) * p[3]
            if nd < d2[i]:
                d2[i] = nd
    for _ in range(iters):
        sums = [[0.0] * 7 for _ in range(k)]
        for p in pts:
            L, A, B, w = p[0], p[1], p[2], p[3]
            best, bd = 0, 1e9
            for j, c in enumerate(cents):
                d = (L - c[0]) ** 2 + (A - c[1]) ** 2 + (B - c[2]) ** 2
                if d < bd:
                    bd, best = d, j
            s = sums[best]
            s[0] += L * w
            s[1] += A * w
            s[2] += B * w
            s[3] += w
            s[4] += p[4] * w
            s[5] += p[5] * w
            s[6] += p[6] * w
        for j in range(k):
            if sums[j][3] > 0:
                cents[j] = (sums[j][0] / sums[j][3], sums[j][1] / sums[j][3], sums[j][2] / sums[j][3])
    # return sRGB means (true colour), recomputed in Oklab later
    out = []
    sums = [[0.0] * 4 for _ in range(k)]
    for p in pts:
        best, bd = 0, 1e9
        for j, c in enumerate(cents):
            d = (p[0] - c[0]) ** 2 + (p[1] - c[1]) ** 2 + (p[2] - c[2]) ** 2
            if d < bd:
                bd, best = d, j
        s = sums[best]
        s[0] += p[3]
        s[1] += p[4] * p[3]
        s[2] += p[5] * p[3]
        s[3] += p[6] * p[3]
    for s in sums:
        if s[0] > 0:
            out.append((int(round(s[1] / s[0])), int(round(s[2] / s[0])), int(round(s[3] / s[0]))))
    return out


def sort_palette(cols):
    """Readable ramps: greys first by lightness, then hue buckets, each by lightness."""
    NB = 14
    keyed = []
    for c in cols:
        lab = srgb_to_oklab(*c)
        ch = chroma(lab)
        if ch < 0.025:
            keyed.append((-1, lab[0], c))
        else:
            keyed.append((int(hue(lab) / (360.0 / NB)), lab[0], c))
    keyed.sort(key=lambda t: (t[0], t[1]))
    return [t[2] for t in keyed]


def cmd_build(args):
    if "--ramps" in args:
        return cmd_build_ramps(args)
    t0 = time.time()
    OUT.mkdir(parents=True, exist_ok=True)
    files = corpus()
    groups = {}
    for f in files:
        groups.setdefault(categorise(f), []).append(f)
    for k in groups:
        print(f"  {k:6s} {len(groups[k])} files")
    cols, per = [], {}
    for cat, budget in BUDGET.items():
        pts = histogram(groups.get(cat, []))
        c = kmeans(pts, budget)
        per[cat] = len(c)
        cols += c
        print(f"  {cat}: {len(pts)} buckets -> {len(c)} colours  ({time.time()-t0:.1f}s)")
    # merge near-duplicates in Oklab
    labs = [srgb_to_oklab(*c) for c in cols]
    kept, keptlab = [], []
    for c, lab in zip(cols, labs):
        if any(de(lab, q) < 0.02 for q in keptlab):
            continue
        kept.append(c)
        keptlab.append(lab)
    merged = len(cols) - len(kept)
    # refill from the worst-represented buckets across the whole corpus
    if len(kept) < 253:
        allpts = histogram(files)
        while len(kept) < 253:
            best, bd = None, -1
            for p in allpts:
                d = min(de(p[:3], q) for q in keptlab)
                score = d * d * math.log1p(p[3])
                if score > bd:
                    bd, best = score, p
            if best is None:
                break
            c = (int(round(best[4])), int(round(best[5])), int(round(best[6])))
            kept.append(c)
            keptlab.append(srgb_to_oklab(*c))
    # reserve true black and true white: ink lines and sheet borders must not shift hue
    pal = [(0, 0, 0), (255, 255, 255)] + sort_palette(kept[:253])
    pal = [(0, 0, 0)] + pal                      # index 0 = transparent
    alpha = [0] + [255] * (len(pal) - 1)
    write_palette_files(pal, "master", {"per_category": per, "merged": merged})
    print(f"merged {merged} near-duplicates ({time.time()-t0:.1f}s)")
    return


def write_palette_files(pal, name, meta=None):
    alpha = [0] + [255] * (len(pal) - 1)
    (HERE / f"{name}.json").write_text(json.dumps({"palette": pal, **(meta or {})}))
    write_png(HERE / f"{name}.pal.png", 256, 1, 4,
              [bytearray(v for i, c in enumerate(pal) for v in (c[0], c[1], c[2], alpha[i]))])
    (HERE / f"{name}.hex").write_text("".join("#%02x%02x%02x\n" % c for c in pal))
    # swatch sheet
    S, G = 32, 16
    rows = []
    for gy in range(G):
        line = bytearray()
        for gx in range(G):
            c = pal[gy * G + gx]
            line += bytes(c) * S
        for _ in range(S):
            rows.append(bytearray(line))
    write_png(HERE / f"{name}_swatch.png", G * S, G * S, 3, rows)
    lp = OUT / f"lut32_{name}.bin"
    if lp.exists():
        lp.unlink()
    print(f"wrote {name}.json / {name}.pal.png / {name}.hex / {name}_swatch.png")


# ─────────────────────────── LUT + apply ───────────────────────────

def load_palette():
    return [tuple(c) for c in json.loads(master_path().read_text())["palette"]]


def build_lut(pal):
    """32^3 RGB cell -> (nearest, second-nearest, mix ratio 0..255) in Oklab."""
    LUTFILE = lut_path()
    if LUTFILE.exists():
        raw = LUTFILE.read_bytes()
        n = 32768
        return raw[:n], raw[n:2 * n], raw[2 * n:]
    t0 = time.time()
    labs = [srgb_to_oklab(*c) for c in pal[1:]]      # skip transparent index 0
    c1 = bytearray(32768)
    c2 = bytearray(32768)
    mix = bytearray(32768)
    for i in range(32768):
        r = ((i >> 10) & 31) * 255 // 31
        g = ((i >> 5) & 31) * 255 // 31
        b = (i & 31) * 255 // 31
        L, A, B = srgb_to_oklab(r, g, b)
        b1 = b2 = -1
        d1 = d2 = 1e9
        for j, q in enumerate(labs):
            d = (L - q[0]) ** 2 + (A - q[1]) ** 2 + (B - q[2]) ** 2
            if d < d1:
                d2, b2, d1, b1 = d1, b1, d, j
            elif d < d2:
                d2, b2 = d, j
        p, q = labs[b1], labs[b2]
        vx, vy, vz = q[0] - p[0], q[1] - p[1], q[2] - p[2]
        den = vx * vx + vy * vy + vz * vz
        t = 0.0 if den <= 0 else ((L - p[0]) * vx + (A - p[1]) * vy + (B - p[2]) * vz) / den
        c1[i] = b1 + 1
        c2[i] = b2 + 1
        mix[i] = max(0, min(255, int(round(t * 255))))
        if i % 8192 == 0:
            print(f"    lut {i}/32768 ({time.time()-t0:.0f}s)", file=sys.stderr)
    LUTFILE.parent.mkdir(parents=True, exist_ok=True)
    LUTFILE.write_bytes(bytes(c1) + bytes(c2) + bytes(mix))
    print(f"  lut built in {time.time()-t0:.1f}s", file=sys.stderr)
    return bytes(c1), bytes(c2), bytes(mix)


BAYER4 = [[0, 8, 2, 10], [12, 4, 14, 6], [3, 11, 1, 9], [15, 7, 13, 5]]


def _bayer8():
    m = [[0] * 8 for _ in range(8)]
    for y in range(8):
        for x in range(8):
            v, mask = 0, 4
            for bit in range(3):
                v = (v << 2) | ((((y ^ x) & mask) and 1) << 1 | ((y & mask) and 1))
                mask >>= 1
            m[y][x] = v
    return m


BAYER8 = _bayer8()


def quantise(w, h, ch, rows, pal, dither="none", alpha_mode="hard", key=False):
    """-> (index_rows, stats dict)."""
    c1, c2, mix = build_lut(pal)
    if dither == "bayer4":
        mat, n = BAYER4, 4
        thr = [[(v + 0.5) / 16.0 for v in r] for r in mat]
    elif dither == "bayer8":
        mat, n = BAYER8, 8
        thr = [[(v + 0.5) / 64.0 for v in r] for r in mat]
    else:
        thr, n = None, 0
    labs = [None] + [srgb_to_oklab(*c) for c in pal[1:]]
    memo = {}
    out = []
    distinct = set()
    errs = []
    for y in range(h):
        row = rows[y]
        o = bytearray(w)
        trow = thr[y % n] if thr else None
        for x in range(w):
            i = x * ch
            r, g, b = row[i], row[i + 1], row[i + 2]
            if ch == 4 and row[i + 3] < 128 and alpha_mode == "hard":
                o[x] = 0
                continue
            if key and min(r, b) - g > 40 and r > 90 and b > 90:
                o[x] = 0
                continue
            distinct.add((r, g, b))
            cell = (r >> 3) << 10 | (g >> 3) << 5 | (b >> 3)
            if trow is not None and mix[cell] > 0:
                idx = c2[cell] if trow[x % n] < mix[cell] / 255.0 else c1[cell]
            else:
                idx = c1[cell]
            o[x] = idx
            if (x & 1) == 0 and (y & 1) == 0:
                key = (r, g, b)
                lab = memo.get(key)
                if lab is None:
                    lab = memo[key] = srgb_to_oklab(r, g, b)
                errs.append(de(lab, labs[idx]))
        out.append(o)
    errs.sort()
    nn = len(errs) or 1
    stats = {"distinct_before": len(distinct),
             "mean_dE": sum(errs) / nn,
             "p95_dE": errs[int(nn * 0.95)] if errs else 0.0,
             "pct_over_005": 100.0 * sum(1 for e in errs if e > 0.05) / nn}
    return out, stats


def to_rgb_rows(index_rows, pal, w, bg=None):
    rows = []
    flat = [bytes(c) for c in pal]
    if bg:
        flat[0] = bytes(bg)
    for ir in index_rows:
        line = bytearray()
        for i in ir:
            line += flat[i]
        rows.append(line)
    return rows


def parse_opts(args):
    o = {"dither": "none", "alpha": "hard", "indexed": False, "key": False}
    global VARIANT
    i = 0
    rest = []
    while i < len(args):
        a = args[i]
        if a == "--dither":
            o["dither"] = args[i + 1]
            i += 2
        elif a == "--alpha":
            o["alpha"] = args[i + 1]
            i += 2
        elif a == "--ramps":
            VARIANT = "master_ramps"
            i += 1
        elif a == "--key":
            o["key"] = True
            i += 1
        elif a == "--indexed":
            o["indexed"] = True
            i += 1
        else:
            rest.append(a)
            i += 1
    return o, rest


def cmd_apply(args):
    o, rest = parse_opts(args)
    src, dst = Path(rest[0]), Path(rest[1])
    pal = load_palette()
    w, h, ch, rows = read_png(src)
    idx, st = quantise(w, h, ch, rows, pal, o["dither"], o["alpha"], o["key"])
    OUT.mkdir(parents=True, exist_ok=True)
    write_png(dst, w, h, 3, to_rgb_rows(idx, pal, w, (255, 0, 255) if o["key"] else None))
    st["rgba_bytes"] = src.stat().st_size
    if o["indexed"]:
        ip = dst.with_suffix(".idx.png")
        write_indexed_png(ip, w, h, idx, pal, alpha=[0] + [255] * 255)
        st["indexed_bytes"] = ip.stat().st_size
        st["raw_1bpp_bytes"] = w * h
    print(json.dumps({"file": src.name, "dither": o["dither"], **st}))
    return st


# ─────────────────────────── comparison images ───────────────────────────

def box_scale(w, h, ch, rows, tw):
    """Box-filter downscale to width tw (keeps aspect)."""
    if tw >= w:
        return w, h, rows
    th = max(1, int(round(h * tw / w)))
    out = []
    for ty in range(th):
        y0, y1 = ty * h // th, max(ty * h // th + 1, (ty + 1) * h // th)
        line = bytearray()
        for tx in range(tw):
            x0, x1 = tx * w // tw, max(tx * w // tw + 1, (tx + 1) * w // tw)
            acc = [0, 0, 0]
            n = 0
            for y in range(y0, y1):
                r = rows[y]
                for x in range(x0, x1):
                    i = x * ch
                    acc[0] += r[i]
                    acc[1] += r[i + 1]
                    acc[2] += r[i + 2]
                    n += 1
            line += bytes(v // n for v in acc)
        out.append(line)
    return tw, th, out


def side_by_side(dst, a, b, maxw=2500, gutter=16):
    """a, b: (w,h,ch,rows) both same size."""
    w, h, ch, rows = a
    _, _, ch2, rows2 = b
    tw = min(w, (maxw - gutter) // 2)
    w1, h1, r1 = box_scale(w, h, ch, rows, tw)
    w2, h2, r2 = box_scale(w, h, ch2, rows2, tw)
    W, H = w1 + gutter + w2, max(h1, h2)
    gap = bytes([24, 24, 28]) * gutter
    out = []
    for y in range(H):
        la = r1[y] if y < h1 else bytearray(bytes([24, 24, 28]) * w1)
        lb = r2[y] if y < h2 else bytearray(bytes([24, 24, 28]) * w2)
        if ch == 4:
            la = bytearray(b for i in range(w1) for b in la[i * 4:i * 4 + 3])
        if ch2 == 4:
            lb = bytearray(b for i in range(w2) for b in lb[i * 4:i * 4 + 3])
        out.append(bytearray(la) + gap + bytearray(lb))
    write_png(dst, W, H, 3, out)
    return W, H


def cmd_compare(args):
    o, rest = parse_opts(args)
    src, dst = Path(rest[0]), OUT / rest[1]
    pal = load_palette()
    w, h, ch, rows = read_png(src)
    idx, st = quantise(w, h, ch, rows, pal, o["dither"], o["alpha"], o["key"])
    conv = to_rgb_rows(idx, pal, w, (255, 0, 255) if o["key"] else None)
    OUT.mkdir(parents=True, exist_ok=True)
    side_by_side(dst, (w, h, ch, rows), (w, h, 3, conv))
    st["rgba_bytes"] = src.stat().st_size
    if o["indexed"]:
        ip = OUT / (dst.stem + ".idx.png")
        write_indexed_png(ip, w, h, idx, pal, alpha=[0] + [255] * 255)
        st["indexed_bytes"] = ip.stat().st_size
        rp = OUT / (dst.stem + ".rgba.png")
        write_png(rp, w, h, 3, rows if ch == 3 else to_rgb_rows(idx, pal, w))
        st["src_repacked_bytes"] = rp.stat().st_size
        rp.unlink()
    print(json.dumps({"file": src.name, "out": dst.name, "dither": o["dither"], **st}))
    return st


def crop_rows(w, h, ch, rows, x, y, cw, chh):
    return [bytearray(rows[yy][x * ch:(x + cw) * ch]) for yy in range(y, min(h, y + chh))]


def zoom(rows, ch, cw, z):
    out = []
    for r in rows:
        line = bytearray()
        for x in range(cw):
            line += bytes(r[x * ch:x * ch + 3]) * z
        for _ in range(z):
            out.append(bytearray(line))
    return out


def cmd_crop(args):
    o, rest = parse_opts(args)
    src, name = Path(rest[0]), rest[1]
    x, y, cw, chh = (int(v) for v in rest[2:6])
    z = 3
    pal = load_palette()
    w, h, ch, rows = read_png(src)
    idx, st = quantise(w, h, ch, rows, pal, o["dither"], o["alpha"], o["key"])
    conv = to_rgb_rows(idx, pal, w, (255, 0, 255) if o["key"] else None)
    ca = crop_rows(w, h, ch, rows, x, y, cw, chh)
    cb = crop_rows(w, h, 3, conv, x, y, cw, chh)
    za, zb = zoom(ca, ch, cw, z), zoom(cb, 3, cw, z)
    OUT.mkdir(parents=True, exist_ok=True)
    side_by_side(OUT / name, (cw * z, len(za), 3, za), (cw * z, len(zb), 3, zb))
    print(json.dumps({"file": src.name, "out": name, "dither": o["dither"],
                      "crop": [x, y, cw, chh], **st}))




# ─────────────────────── ramp-structured palette ───────────────────────

FAMILY_SEEDS = [           # Oklab hue anchors (degrees) and the material each is meant to catch
    (32, "roof red / brick"), (45, "orange (Falke) / terracotta"), (57, "skin"),
    (68, "wood"), (78, "earth / dirt"), (90, "gold / blond"),
    (108, "olive / dry grass"), (135, "foliage green (warm)"), (158, "foliage green (cool)"),
    (205, "teal / shallow water"), (238, "sky / cyan"), (262, "water blue / metal"),
    (300, "purple / shadow"),
]
RAMP_LEN = 16
N_NEUTRAL = 2              # warm neutral (plaster, white cloth) + cool neutral (stone, steel)


def _circ_kmeans(pts, seeds, iters=10):
    """Cluster chromatic buckets by Oklab hue (weighted, circular)."""
    cents = [math.radians(h) for h, _ in seeds]
    groups = [[] for _ in cents]
    for _ in range(iters):
        groups = [[] for _ in cents]
        for p in pts:
            a = math.atan2(p[2], p[1])
            best, bd = 0, 9
            for j, c in enumerate(cents):
                d = abs(math.atan2(math.sin(a - c), math.cos(a - c)))
                if d < bd:
                    bd, best = d, j
            groups[best].append(p)
        for j, g in enumerate(groups):
            if not g:
                continue
            sx = sum(math.cos(math.atan2(q[2], q[1])) * q[3] for q in g)
            sy = sum(math.sin(math.atan2(q[2], q[1])) * q[3] for q in g)
            if sx or sy:
                cents[j] = math.atan2(sy, sx)
    return groups


def _thirds(g):
    """Weighted (L, C, h) of the dark / mid / light thirds of a family."""
    g = sorted(g, key=lambda p: p[0])
    tot = sum(p[3] for p in g)
    out, acc, part = [], 0.0, []
    for p in g:
        part.append(p)
        acc += p[3]
        if acc >= tot / 3 and len(out) < 2:
            out.append(part)
            part, acc = [], 0.0
    out.append(part)
    while len(out) < 3:
        out.append(out[-1])
    res = []
    for part in out:
        w = sum(p[3] for p in part) or 1
        L = sum(p[0] * p[3] for p in part) / w
        ax = sum(p[1] * p[3] for p in part) / w
        by = sum(p[2] * p[3] for p in part) / w
        res.append((L, math.hypot(ax, by), math.atan2(by, ax)))
    return res


def _lerp_ang(a, b, t):
    d = math.atan2(math.sin(b - a), math.cos(b - a))
    return a + d * t


def build_ramp(g, lo=0.12, hi=0.97):
    """16 steps through a family's own colours, shadows toward blue-violet, highlights toward yellow."""
    t3 = _thirds(g)
    Ls = [t3[0][0], t3[1][0], t3[2][0]]
    SHADOW_H, HILITE_H = math.radians(295), math.radians(95)
    out = []
    for i in range(RAMP_LEN):
        u = i / (RAMP_LEN - 1)
        L = lo + (hi - lo) * u
        if L <= Ls[0]:
            C, H = t3[0][1], t3[0][2]
        elif L <= Ls[1]:
            t = (L - Ls[0]) / max(1e-6, Ls[1] - Ls[0])
            C = t3[0][1] + (t3[1][1] - t3[0][1]) * t
            H = _lerp_ang(t3[0][2], t3[1][2], t)
        elif L <= Ls[2]:
            t = (L - Ls[1]) / max(1e-6, Ls[2] - Ls[1])
            C = t3[1][1] + (t3[2][1] - t3[1][1]) * t
            H = _lerp_ang(t3[1][2], t3[2][2], t)
        else:
            C, H = t3[2][1], t3[2][2]
        mid = (Ls[0] + Ls[2]) / 2
        if L < mid:                                  # shadows: rotate to blue-violet, lose chroma
            k = min(1.0, (mid - L) / max(1e-6, mid - lo))
            H = _lerp_ang(H, SHADOW_H, 0.22 * k)
            C *= 1.0 - 0.45 * k * k
        else:                                        # highlights: rotate to yellow, wash out
            k = min(1.0, (L - mid) / max(1e-6, hi - mid))
            H = _lerp_ang(H, HILITE_H, 0.18 * k)
            C *= 1.0 - 0.55 * k * k
        out.append(oklab_to_srgb(L, C * math.cos(H), C * math.sin(H)))
    return out


def cmd_build_ramps(args):
    t0 = time.time()
    OUT.mkdir(parents=True, exist_ok=True)
    files = corpus()
    pts = histogram(files)
    neutral = [p for p in pts if math.hypot(p[1], p[2]) < 0.030]
    chromatic = [p for p in pts if math.hypot(p[1], p[2]) >= 0.030]
    fams = _circ_kmeans(chromatic, FAMILY_SEEDS)
    warm = [p for p in neutral if p[2] >= 0]
    cool = [p for p in neutral if p[2] < 0]
    named = [(FAMILY_SEEDS[i][1], g) for i, g in enumerate(fams)] + \
            [("neutral warm (plaster, cloth)", warm), ("neutral cool (stone, steel)", cool)]
    ramps, labels = [], []
    for name, g in named:
        if len(g) < 4:
            print(f"  family '{name}' has only {len(g)} buckets - using it anyway")
            g = g or pts
        ramps.append(build_ramp(g))
        labels.append(name)
        print(f"  {name:34s} {len(g):5d} buckets, {sum(p[3] for p in g):9d} px")
    flat = [c for r in ramps for c in r]
    seen, uniq = set(), []
    for c in flat:
        if c not in seen:
            seen.add(c)
            uniq.append(c)
    extras = [(0, 0, 0), (255, 255, 255)]
    labs = [srgb_to_oklab(*c) for c in uniq + extras]
    while len(uniq) + len(extras) < 255:
        best, bd = None, -1
        for p in pts:
            d = min(de(p[:3], q) for q in labs)
            sc = d * d * math.log1p(p[3])
            if sc > bd:
                bd, best = sc, p
        if best is None:
            break
        c = (int(round(best[4])), int(round(best[5])), int(round(best[6])))
        extras.append(c)
        labs.append(srgb_to_oklab(*c))
    pal = [(0, 0, 0)] + uniq + extras
    pal = pal[:256] + [(0, 0, 0)] * (256 - len(pal))
    write_palette_files(pal, "master_ramps", {"labels": labels, "ramp_len": RAMP_LEN})
    # ramp-per-row swatch
    S = 32
    rows = []
    grid = ramps + [extras + [(0, 0, 0)] * (16 - len(extras) % 16 or 0)]
    grid = ramps + [[extras[i] if i < len(extras) else (0, 0, 0) for i in range(16)]]
    for r in grid:
        line = bytearray()
        for c in r:
            line += bytes(c) * S
        for _ in range(S):
            rows.append(bytearray(line))
    write_png(HERE / "master_ramps_swatch.png", 16 * S, len(grid) * S, 3, rows)
    print(f"{len(ramps)} ramps x {RAMP_LEN} + {len(extras)} extras = {len(uniq)+len(extras)+1} "
          f"({time.time()-t0:.1f}s)")


# ─────────────────────── shade tables (Doom COLORMAP) ───────────────────────

LEVELS = 32


def shade_colour(lab, s, mode="day"):
    """Darken in Oklab: L scaled, chroma reduced, hue rotated toward blue-violet."""
    L, a, b = lab
    C, H = math.hypot(a, b), math.atan2(b, a)
    L *= s
    C *= 0.30 + 0.70 * s
    H = _lerp_ang(H, math.radians(295), 0.30 * (1 - s))
    if mode == "night":
        L *= 0.80
        H = _lerp_ang(H, math.radians(265), 0.45)
        C = C * 0.55 + 0.022
        a, b = C * math.cos(H), C * math.sin(H)
        a -= 0.004
        b -= 0.030
        return (L, a, b)
    if mode == "dusk":
        H = _lerp_ang(H, math.radians(55), 0.32 * (1 - s) + 0.12)
        C = C * 1.05 + 0.018 * (1 - s)
        a, b = C * math.cos(H), C * math.sin(H)
        return (L * 0.95, a, b + 0.010)
    return (L, C * math.cos(H), C * math.sin(H))


def build_colormap(pal, mode="day"):
    """LEVELS x 256 table of palette indices."""
    labs = [srgb_to_oklab(*c) for c in pal[1:]]
    table = []
    for lv in range(LEVELS):
        s = 1.0 - 0.95 * (lv / (LEVELS - 1))
        row = bytearray(256)
        for i, c in enumerate(pal):
            if i == 0:
                continue
            t = shade_colour(srgb_to_oklab(*c), s, mode)
            best, bd = 0, 1e9
            for j, q in enumerate(labs):
                d = (t[0] - q[0]) ** 2 + (t[1] - q[1]) ** 2 + (t[2] - q[2]) ** 2
                if d < bd:
                    bd, best = d, j
            row[i] = best + 1
        table.append(row)
    return table


def shade_png(path, table, pal):
    rows = []
    for row in table:
        line = bytearray()
        for i in range(256):
            line += bytes(pal[row[i]])
        rows.append(line)
    write_png(path, 256, LEVELS, 3, rows)


def _demo_source(src, width):
    """sips-downscaled copy of a sheet, read back."""
    CACHE.mkdir(parents=True, exist_ok=True)
    dst = CACHE / f"demo{width}__{src.name}"
    if not dst.exists():
        subprocess.run(["sips", "--resampleWidth", str(width), str(src), "--out", str(dst)],
                       check=True, capture_output=True)
    return read_png(dst)


def cmd_shade(args):
    o, rest = parse_opts(args)
    sfx = "_ramps" if VARIANT == "master_ramps" else ""
    pal = load_palette()
    OUT.mkdir(parents=True, exist_ok=True)
    tables = {}
    for mode in ("day", "night", "dusk"):
        t = build_colormap(pal, mode)
        tables[mode] = t
        shade_png(OUT / f"shadetable_{mode}{sfx}.png", t, pal)
        print(f"  shadetable_{mode}{sfx}.png")
    demos = [("valley", ROOT / "story/sheets/tilesets-valley-objects.png", 740, True),
             ("falke", ROOT / "story/sheets/cast-falke-walker.png", 420, True)]
    for name, src, wid, key in demos:
        w, h, ch, rows = _demo_source(src, wid)
        idx, _ = quantise(w, h, ch, rows, pal, "none", "hard", key)
        bg = (255, 0, 255) if key else None
        # 1. hard light levels + night + dusk, as a 3x2 grid
        tiles = []
        for lab, mode, frac in (("100%", "day", 1.0), ("75%", "day", 0.75), ("50%", "day", 0.5),
                                ("25%", "day", 0.25), ("night", "night", 0.65), ("dusk", "dusk", 0.7)):
            lv = int(round((1.0 - frac) * (LEVELS - 1)))
            row = tables[mode][lv]
            tiles.append([bytearray(row[i] for i in r) for r in idx])
        grid = []
        gap = 8
        for gy in range(2):
            band = [bytearray() for _ in range(h)]
            for gx in range(3):
                t = tiles[gy * 3 + gx]
                rgb = to_rgb_rows(t, pal, w, bg)
                for y in range(h):
                    band[y] += rgb[y] + bytes([16, 16, 18]) * gap
            grid += band
            grid += [bytearray(bytes([16, 16, 18]) * (3 * (w + gap))) for _ in range(gap)]
        write_png(OUT / f"f1_{name}_levels{sfx}.png", 3 * (w + gap), len(grid), 3, grid)
        # 2. smooth light gradients: hard vs bayer4 between adjacent levels
        for gname, field in (("gradient", "lin"), ("lantern", "rad")):
            outs = []
            for dith in (False, True):
                o2 = []
                for y in range(h):
                    line = bytearray()
                    trow = BAYER4[y % 4]
                    for x in range(w):
                        if field == "lin":
                            f = 1.0 - 0.80 * (x / (w - 1))
                        else:
                            dx, dy = (x - w * 0.5) / (w * 0.5), (y - h * 0.5) / (h * 0.5)
                            f = max(0.12, 1.0 - 1.05 * math.hypot(dx, dy) ** 1.4)
                        lv = (1.0 - f) * (LEVELS - 1)
                        lo = int(lv)
                        if dith:
                            lo += 1 if (lv - lo) > (trow[x % 4] + 0.5) / 16.0 else 0
                        else:
                            lo = int(round(lv))
                        lo = max(0, min(LEVELS - 1, lo))
                        line.append(tables["day"][lo][idx[y][x]])
                    o2.append(line)
                outs.append(to_rgb_rows(o2, pal, w, bg))
            side_by_side(OUT / f"f2_{name}_{gname}{sfx}.png",
                         (w, h, 3, outs[0]), (w, h, 3, outs[1]))
            print(f"  f2_{name}_{gname}{sfx}.png  (left hard, right bayer4)")


if __name__ == "__main__":
    cmd = sys.argv[1] if len(sys.argv) > 1 else "build"
    if "--ramps" in sys.argv:
        VARIANT = "master_ramps"
    {"build": cmd_build, "apply": cmd_apply, "compare": cmd_compare,
     "crop": cmd_crop, "shade": cmd_shade}[cmd](sys.argv[2:])
