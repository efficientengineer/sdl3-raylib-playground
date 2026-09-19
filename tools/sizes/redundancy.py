"""Walker-sheet redundancy: duplicate stands, mirrored side rows, mirrored step pairs.

Rows are S W E N, columns stand / step-left / stand / step-right.
Writes out/redundancy.json and overlay + heatmap strips under out/.
"""
import json
import math

import assets as A
import imglib as I

ROWS = "SWEN"
COLS = ["stand", "stepL", "stand2", "stepR"]
NAMES = ["falke", "ottilie", "villager_a", "villager_b"]


def bbox(img):
    w, h, px = img["w"], img["h"], img["px"]
    x0, y0, x1, y1 = w, h, -1, -1
    for y in range(h):
        base = y * w
        for x in range(w):
            if px[base + x][3] > 16:
                if x < x0:
                    x0 = x
                if x > x1:
                    x1 = x
                if y < y0:
                    y0 = y
                if y > y1:
                    y1 = y
    if x1 < 0:
        return None
    return x0, y0, x1, y1


def mirror(img):
    w, h, px = img["w"], img["h"], img["px"]
    out = []
    for y in range(h):
        out.extend(reversed(px[y * w:(y + 1) * w]))
    return {"w": w, "h": h, "px": out}


def align(a, b, pad=8):
    """Place a and b on a common canvas, matched on bbox centre-x and feet (bbox bottom)."""
    ba, bb = bbox(a), bbox(b)
    if not ba or not bb:
        return None
    wa, ha = ba[2] - ba[0] + 1, ba[3] - ba[1] + 1
    wb, hb = bb[2] - bb[0] + 1, bb[3] - bb[1] + 1
    W = max(wa, wb) + pad * 2
    H = max(ha, hb) + pad * 2
    ca = I.blank(W, H, (0, 0, 0, 0))
    cb = I.blank(W, H, (0, 0, 0, 0))
    for canvas, img, bx, ww, hh in ((ca, a, ba, wa, ha), (cb, b, bb, wb, hb)):
        sub = I.crop(img, bx[0], bx[1], ww, hh)
        I.paste(canvas, sub, (W - ww) // 2, H - pad - hh)
    return ca, cb


def diff(a, b):
    """mean dE over the union of opaque pixels, % of union differing by >0.05, heatmap."""
    w, h = a["w"], a["h"]
    ds = []
    heat = []
    for pa, pb in zip(a["px"], b["px"]):
        oa, ob = pa[3] > 16, pb[3] > 16
        if not oa and not ob:
            heat.append((0, 0, 0, 255))
            ds.append(None)
            continue
        if oa != ob:
            d = 1.0                              # silhouette mismatch: the worst kind
        else:
            d = I.de(I.lab(pa[0], pa[1], pa[2]), I.lab(pb[0], pb[1], pb[2]))
        ds.append(d)
        t = min(1.0, d / 0.35)
        heat.append((int(255 * t), int(80 * (1 - t)), int(255 * (1 - t)), 255))
    vals = [d for d in ds if d is not None]
    if not vals:
        return 0.0, 0.0, {"w": w, "h": h, "px": heat}
    mean = sum(vals) / len(vals)
    over = sum(1 for d in vals if d > 0.05) / len(vals)
    return mean, over, {"w": w, "h": h, "px": heat}


def frames(name):
    return {f"{ROWS[r]}{COLS[c]}": A.load_walk(name, r, c) for r in range(4) for c in range(4)}


def main():
    out = {}
    for name in NAMES:
        fr = frames(name)
        rec = {}

        # duplicate stand: col0 vs col2 in every row
        for r in ROWS:
            a, b = align(fr[f"{r}stand"], fr[f"{r}stand2"])
            m, o, heat = diff(a, b)
            rec[f"dup_stand_{r}"] = dict(mean=round(m, 4), over=round(o, 4))

        # E row vs mirrored W row, frame by frame and best match
        best = {}
        for ci, c in enumerate(COLS):
            em = mirror(fr[f"E{c}"])
            row = {}
            for c2 in COLS:
                a, b = align(em, fr[f"W{c2}"])
                m, o, heat = diff(a, b)
                row[c2] = dict(mean=round(m, 4), over=round(o, 4))
                if c2 == c:
                    rec[f"mirrorEW_{c}"] = row[c2]
                    I.save(heat, I.OUT / f"heat_{name}_EW_{c}.png")
            best[c] = min(row, key=lambda k: row[k]["mean"])
            rec[f"mirrorEW_{c}_best"] = [best[c], row[best[c]]["mean"]]
        rec["EW_cycle_aligned"] = all(best[c] == c for c in COLS)

        # step-left vs mirrored step-right inside S and inside N
        for r in ("S", "N"):
            a, b = align(fr[f"{r}stepL"], mirror(fr[f"{r}stepR"]))
            m, o, heat = diff(a, b)
            rec[f"step_mirror_{r}"] = dict(mean=round(m, 4), over=round(o, 4))
            I.save(heat, I.OUT / f"heat_{name}_step_{r}.png")

        out[name] = rec
        print(name, json.dumps(rec)[:400], flush=True)

    (I.OUT / "redundancy.json").write_text(json.dumps(out, indent=1))


if __name__ == "__main__":
    main()
