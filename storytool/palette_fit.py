"""`palette build`: fit master.hex to the raw returns in story/sheets and story/refs.

Builds a histogram of the corpus, groups it into hue families, fits a ramp per family at
one of three lengths, and writes master.hex, master.json, the swatch and the .pal.png. The
`# version N` line is the palette's identity; bump it when the colours change.

Must never do: put a cutscene panel in the corpus — panels get their own palette per scene.
Public: palette_corpus, palette_histogram, hue_families, build_ramp, ramp_lengths,
cmd_palette_build, write_swatch. Imports: colormap, md, oklab, palette, paths, png.
"""
import json
import math
import subprocess
import time
from .colormap import cmd_palette_colormap, write_cycles
from .md import die
from .oklab import _from_oklab, _lerp_ang, _to_oklab
from .palette import CYCLE_RAMPS, HILITE_HUE, MASTER_HEX, MASTER_JSON, MASTER_PAL_PNG, MASTER_SWATCH, MERGE_DE, NEUTRAL_C, PALETTE_DIR, PALETTE_VERSION, PAL_SIZE, RAMP_CLASSES, RAMP_FAMILIES, RAMP_HI, RAMP_LO, SHADOW_HUE, write_hex
from .paths import OUT, ROOT, SHEETS
from .png import read_png, write_png



# ── building the master from the corpus ──

# What the field is made of NOW. The v1 corpus was every sheet in the tray at equal weight, which
# meant the old noisy tile style — the one the owner replaced — set most of the palette, and the flat
# luminous greens of the new swatches had almost nothing near them. v2 weights by what the screen is
# actually going to be made of: the approved style test and the new ground art lead, the cast comes
# next (a face is on screen as much as the ground is), and the old stamps that are still in the atlas
# are kept at a low weight — they have to stay drawable, but they must not choose the greens.
CORPUS_WEIGHTS = (
    (("tilesets-valley-decals.png",), 6.0, "the decals: small, saturated, and few pixels each"),
    (("tests/clean_style_test_v1.png", "tilesets-valley-swatches.png"), 3.0,
     "the new flat style: the approved style test and the ground swatches"),
    (("cast-", "refs/"), 2.0, "the cast: walk sheets, reference sheets, portraits"),
    (("tilesets-valley-objects", "ch01-"), 0.5, "the stamps and props already in the atlas"),
)


def palette_corpus():
    """[(path, weight)] — the raw returns the master is fitted to, and how much each one counts.

    Cutscene panels are never in it: they get their own palette per scene (PALETTE.md), so letting
    their skies and their lamplight in would spend the field's slots on colours the field never uses.
    Screens, walk masks and the retired terrain sheet are out too."""
    files = sorted(SHEETS.rglob("*.png")) + sorted((ROOT / "refs").glob("*.png"))
    out = []
    for f in files:
        rel = str(f.relative_to(SHEETS)) if f.is_relative_to(SHEETS) else "refs/" + f.name
        if f.name.startswith("_") or f.stem.endswith("_walk") or f.stem.endswith("_prev"):
            continue
        if "-scenes-" in f.name or f.stem.startswith("003_the_warning") or "-screens-" in f.name:
            continue
        if f.name == "tilesets-valley-terrain.png":     # the retired noisy terrain sheet: D20 killed it
            continue
        if f.name.startswith("walker16-"):              # archived 16-frame sheets duplicate the cast
            continue
        if f.parent.name == "refs" and f.stem == "style":   # third-party screenshot, not our art
            continue
        w = next((wt for keys, wt, _ in CORPUS_WEIGHTS if any(k in rel for k in keys)), 1.0)
        out.append((f, w))
    return out


def corpus_keep(r, g, b):
    """Drop the template's magenta, its white borders and numbers, and the pure sheet background."""
    if min(r, b) - g > 40 and r > 90 and b > 90:
        return False
    if r > 236 and g > 236 and b > 236:
        return False
    if r < 12 and g < 12 and b < 12:
        return False
    return True


def palette_thumb(path, width=260):
    """A sips-downscaled copy, cached. The corpus is 60 MP of PNG; the histogram does not need it."""
    cache = OUT / "_palcache"
    cache.mkdir(parents=True, exist_ok=True)
    dst = cache / (path.parent.name + "__" + path.name)
    if not dst.exists() or dst.stat().st_mtime < path.stat().st_mtime:
        subprocess.run(["sips", "--resampleWidth", str(width), str(path), "--out", str(dst)],
                       check=True, capture_output=True)
    return dst


def palette_histogram(paths, thumbs=True, keep=corpus_keep, drop_bg=False):
    """[(L, a, b, weight, r, g, b)] over 5-bit RGB buckets, each bucket's true mean colour.

    `drop_bg` throws away each sheet's own background before counting: a template's flat grey or the
    neutral card behind a reference sheet is a third of its pixels and none of its art, and left in
    it buys the plaster ramp a dozen slots to describe one colour nobody ever draws."""
    h = {}
    for entry in paths:
        p, weight = entry if isinstance(entry, tuple) else (entry, 1.0)
        w, ht, ch, rows = read_png(palette_thumb(p) if thumbs else p)
        one = {}
        for y in range(ht):
            row = rows[y]
            for x in range(0, w * ch, ch):
                r, g, b = row[x], row[x + 1], row[x + 2]
                if ch == 4 and row[x + 3] < 128:
                    continue
                if keep and not keep(r, g, b):
                    continue
                k = (r >> 3) << 10 | (g >> 3) << 5 | (b >> 3)
                e = one.get(k)
                if e is None:
                    one[k] = [1, r, g, b]
                else:
                    e[0] += 1
                    e[1] += r
                    e[2] += g
                    e[3] += b
        if drop_bg and one:
            tot = sum(e[0] for e in one.values())
            top = max(one, key=lambda k: one[k][0])
            e = one[top]
            lab = _to_oklab(*(int(v / e[0]) for v in e[1:]))
            if e[0] > 0.18 * tot and math.hypot(lab[1], lab[2]) < 0.045:
                for k in [k for k in one
                          if ((lambda q: (q[0] - lab[0]) ** 2 + (q[1] - lab[1]) ** 2
                               + (q[2] - lab[2]) ** 2)(_to_oklab(*(int(v / one[k][0])
                                                                   for v in one[k][1:])))) < 0.0004]:
                    del one[k]
        for k, e in one.items():
            e.append(e[0] * weight)                      # [pixels, sr, sg, sb, weighted pixels]
            g0 = h.get(k)
            if g0 is None:
                h[k] = e
            else:
                for i in range(5):
                    g0[i] += e[i]
    pts = []
    for n, sr, sg, sb, wn in h.values():
        # the colour is the bucket's true mean over its real pixels; the WEIGHT is what k-means, the
        # ramp budget and the refill see, so a corpus weight moves attention without moving colour.
        r, g, b = sr / n, sg / n, sb / n
        L, A, B = _to_oklab(int(r), int(g), int(b))
        pts.append((L, A, B, wn, r, g, b))
    return pts


def hue_families(pts):
    """Circular k-means on Oklab hue, seeded on the material anchors, for the chromatic buckets."""
    seeds = [f for f in RAMP_FAMILIES if f[1] is not None]
    cents = [math.radians(f[1]) for f in seeds]
    groups = [[] for _ in cents]
    for _ in range(12):
        groups = [[] for _ in cents]
        for p in pts:
            a = math.atan2(p[2], p[1])
            best, bd = 0, 9.0
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
    return {seeds[i][0]: g for i, g in enumerate(groups)}


def _thirds(g):
    """Weighted (L, chroma, hue) of a family's dark, mid and light thirds — the ramp's spine."""
    g = sorted(g, key=lambda p: p[0])
    tot = sum(p[3] for p in g) or 1
    parts, acc, part = [], 0.0, []
    for p in g:
        part.append(p)
        acc += p[3]
        if acc >= tot / 3 and len(parts) < 2:
            parts.append(part)
            part, acc = [], 0.0
    parts.append(part)
    while len(parts) < 3:
        parts.append(parts[-1] or [g[0]])
    res = []
    for part in parts:
        part = part or [g[0]]
        wsum = sum(p[3] for p in part) or 1
        L = sum(p[0] * p[3] for p in part) / wsum
        ax = sum(p[1] * p[3] for p in part) / wsum
        by = sum(p[2] * p[3] for p in part) / wsum
        res.append((L, math.hypot(ax, by), math.atan2(by, ax)))
    return res


def build_ramp(g, n, lo=RAMP_LO, hi=RAMP_HI):
    """n steps through one family's own colours, shadows blue-violet, highlights yellow."""
    t3 = _thirds(g)
    Ls = [t3[0][0], t3[1][0], t3[2][0]]
    sh, hl = math.radians(SHADOW_HUE), math.radians(HILITE_HUE)
    out = []
    for i in range(n):
        u = i / max(1, n - 1)
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
        if L < mid:
            k = min(1.0, (mid - L) / max(1e-6, mid - lo))
            H = _lerp_ang(H, sh, 0.22 * k)
            C *= 1.0 - 0.45 * k * k
        else:
            k = min(1.0, (L - mid) / max(1e-6, hi - mid))
            H = _lerp_ang(H, hl, 0.18 * k)
            C *= 1.0 - 0.55 * k * k
        out.append(tuple(_from_oklab(L, C * math.cos(H), C * math.sin(H))))
    return out


def ramp_lengths(groups):
    """Each class's slot budget, split between its families by pixel share, inside the class bounds."""
    lens = {}
    for cls, (lo, hi, budget) in RAMP_CLASSES.items():
        fams = [f for f in RAMP_FAMILIES if f[2] == cls]
        share = {f[0]: max(1.0, sum(p[3] for p in groups.get(f[0], []))) for f in fams}
        root = {k: math.sqrt(v) for k, v in share.items()}
        tot = sum(root.values()) or 1.0
        raw = {k: budget * v / tot for k, v in root.items()}
        got = {k: max(lo, min(hi, int(round(v)))) for k, v in raw.items()}
        # hand back or take up the rounding and the clamping, largest share first
        order = sorted(fams, key=lambda f: -share[f[0]])
        while sum(got.values()) != budget:
            step = 1 if sum(got.values()) < budget else -1
            moved = False
            for f in (order if step > 0 else order[::-1]):
                if lo <= got[f[0]] + step <= hi:
                    got[f[0]] += step
                    moved = True
                    break
            if not moved:
                break
        lens.update(got)
    return lens


def cmd_palette_build(args):
    """story/palette/master.* — the 256 colours everything in the field is drawn from."""
    t0 = time.time()
    files = palette_corpus()
    if not files:
        die(f"no raw returns in {SHEETS.relative_to(ROOT.parent)}; there is nothing to fit a palette to")
    by_w = {}
    for _, wt in files:
        by_w[wt] = by_w.get(wt, 0) + 1
    print(f"corpus: {len(files)} sheet(s), weighted "
          + ", ".join(f"{n} at x{w:g}" for w, n in sorted(by_w.items(), reverse=True))
          + " (cutscene sheets excluded: panels get their own palette per scene)")
    pts = palette_histogram(files, drop_bg=True)
    total = sum(p[3] for p in pts)
    chromatic = [p for p in pts if math.hypot(p[1], p[2]) >= NEUTRAL_C]
    neutral = [p for p in pts if math.hypot(p[1], p[2]) < NEUTRAL_C]
    groups = hue_families(chromatic)
    groups["neutral warm (plaster, cloth)"] = [p for p in neutral if p[2] >= 0]
    groups["neutral cool (stone, steel)"] = [p for p in neutral if p[2] < 0]
    lens = ramp_lengths(groups)

    ramps, meta = [], []
    for name, _, cls in RAMP_FAMILIES:
        g = groups.get(name) or pts
        n = lens[name]
        ramps.append((name, build_ramp(g, n), cls, sum(p[3] for p in groups.get(name, []))))
    # the reserved three, then the CYCLE blocks, then the ramps, merging what is too close to tell
    # apart. The cycle blocks go first because they are the only entries whose *index* matters.
    pal, labs, spans, dropped = [(0, 0, 0), (0, 0, 0), (255, 255, 255)], [], [], 0
    for c in pal[1:]:
        labs.append(_to_oklab(*c))
    cycles = []
    for cname, fam, n, fps, (lo, hi), what in CYCLE_RAMPS:
        g = groups.get(fam) or pts
        start = len(pal)
        for c in build_ramp(g, n, lo, hi):
            pal.append(tuple(c))
            labs.append(_to_oklab(*c))
        cycles.append({"name": cname, "family": fam, "start": start, "len": n, "fps": fps,
                       "what": what})
        print(f"  cycle {cname:13s} {n} indices {start}-{start + n - 1} at {fps} fps, from {fam}")
    for name, ramp, cls, px in ramps:
        # A ramp keeps every step it asked for, but a step within MERGE_DE of a colour the palette
        # already holds SHARES that index instead of spending a new one. Skin, wood and earth really
        # are the same browns in this corpus: saying so costs nothing and frees the slot for a colour
        # nothing else serves. So a ramp is a list of indices, not a range, and two ramps may overlap.
        got, own = [], 0
        for c in ramp:
            lab = _to_oklab(*c)
            hit = next((j for j, q in enumerate(labs)
                        if ((lab[0] - q[0]) ** 2 + (lab[1] - q[1]) ** 2 + (lab[2] - q[2]) ** 2) ** 0.5
                        < MERGE_DE), None)
            if hit is not None:
                got.append(hit + 1)                     # labs[0] is index 1: index 0 is transparent
                dropped += 1
                continue
            got.append(len(pal))
            pal.append(tuple(c))
            labs.append(lab)
            own += 1
        spans.append({"name": name, "class": cls, "indices": got, "len": len(got), "own": own,
                      "corpus_px": int(px), "share": round(px / max(1, total), 4)})
        print(f"  {name:32s} {cls:5s} {len(ramp):2d} steps, {own:2d} of its own  "
              f"{100 * px / max(1, total):5.1f}% of the corpus")
    print(f"  {dropped} near-duplicate(s) merged (dE < {MERGE_DE}); "
          f"{PAL_SIZE - len(pal)} slot(s) go to the colours the ramps serve worst")

    # refill: the corpus colours furthest from anything the ramps offer, weighted by how much of the
    # corpus they are. This is what keeps a ramp palette honest against a purely statistical one.
    extra_start = len(pal)
    # each bucket keeps its own distance to the nearest palette entry, updated against the one colour
    # just added, so this is one pass over the corpus a slot instead of a full re-scan.
    near = [min((p[0] - q[0]) ** 2 + (p[1] - q[1]) ** 2 + (p[2] - q[2]) ** 2 for q in labs) for p in pts]
    weight = [math.log1p(p[3]) for p in pts]
    while len(pal) < PAL_SIZE and pts:
        k = max(range(len(pts)), key=lambda i: near[i] * weight[i])
        best = pts[k]
        c = (int(round(best[4])), int(round(best[5])), int(round(best[6])))
        pal.append(c)
        q = _to_oklab(*c)
        labs.append(q)
        for i, p in enumerate(pts):
            d = (p[0] - q[0]) ** 2 + (p[1] - q[1]) ** 2 + (p[2] - q[2]) ** 2
            if d < near[i]:
                near[i] = d
    while len(pal) < PAL_SIZE:
        pal.append((0, 0, 0))
    spans.append({"name": "worst-served corpus colours", "class": "extra",
                  "indices": list(range(extra_start, PAL_SIZE)), "len": PAL_SIZE - extra_start,
                  "own": PAL_SIZE - extra_start, "corpus_px": 0, "share": 0.0})

    PALETTE_DIR.mkdir(parents=True, exist_ok=True)
    write_hex(MASTER_HEX, pal)
    write_png(MASTER_PAL_PNG, PAL_SIZE, 1, 4,
              [bytearray(v for i, c in enumerate(pal)
                         for v in (c[0], c[1], c[2], 0 if i == 0 else 255))])
    write_swatch(MASTER_SWATCH, pal, spans, cycles)
    MASTER_JSON.write_text(json.dumps({
        "version": PALETTE_VERSION, "size": PAL_SIZE,
        "reserved": {"0": "transparent", "1": "black", "2": "white"},
        "cycles": cycles,
        "corpus_weights": [{"match": list(k), "weight": w, "what": t} for k, w, t in CORPUS_WEIGHTS],
        "ramp_lightness": [RAMP_LO, RAMP_HI],
        "shadow_hue": SHADOW_HUE, "highlight_hue": HILITE_HUE,
        "merge_dE": MERGE_DE, "merged": dropped,
        "corpus": [{"file": str(f.relative_to(ROOT.parent)), "weight": wt} for f, wt in files],
        "ramps": spans,
    }, indent=2) + "\n")
    print(f"wrote {MASTER_HEX.relative_to(ROOT.parent)}, master.pal.png, master_swatch.png, master.json "
          f"(version {PALETTE_VERSION}, {time.time()-t0:.1f}s)")
    cmd_palette_colormap([])
    write_cycles(pal, cycles)


def write_swatch(path, pal, spans, cycles=(), cell=24):
    """One material ramp a row, left-aligned, reserved three on the top row. No labels: it is a
    reference image for ChatGPT, and any text on it comes back drawn into the art."""
    rows_of = [[pal[1], pal[2]]]
    for c in cycles:                                    # the reserved cycle blocks get their own rows
        rows_of.append([pal[c["start"] + i] for i in range(c["len"])])
    for s in spans:
        cols = [pal[i] for i in s["indices"]]
        if s["class"] == "extra":                       # the leftovers wrap, or one row is 100 wide
            rows_of += [cols[i:i + 16] for i in range(0, len(cols), 16)]
        else:
            rows_of.append(cols)
    W = max(len(r) for r in rows_of) * cell
    out = []
    for r in rows_of:
        line = bytearray()
        for c in r:
            line += bytes(c) * cell
        line += bytes((16, 16, 18)) * (W // cell - len(r)) * cell
        for _ in range(cell):
            out.append(bytearray(line))
    write_png(path, W, len(out), 3, out)
