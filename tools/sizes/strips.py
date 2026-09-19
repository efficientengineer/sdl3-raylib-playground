"""Step 3: look at it. Side-by-side display renders + a 2x nearest zoom of the busiest region.

Each strip: [full-res original | candidate A | candidate B | candidate C] at the phone's display
size, with a 2x nearest zoom of the most detailed region underneath each.
"""
import json
import sys

import assets as A
import imglib as I
import ladder as L

BG = (16, 16, 20, 255)
GAP = 10


def busiest(img, bw, bh):
    """Top-left of the bw x bh window with the most gradient energy (the face / the roof edge)."""
    w, h, px = img["w"], img["h"], img["px"]
    bw, bh = min(bw, w), min(bh, h)
    step = max(2, min(w, h) // 64)
    best, bx, by = -1, 0, 0
    for y0 in range(0, h - bh + 1, max(1, bh // 4)):
        for x0 in range(0, w - bw + 1, max(1, bw // 4)):
            s = 0.0
            for y in range(y0 + 1, y0 + bh, step):
                base = y * w
                for x in range(x0 + 1, x0 + bw, step):
                    p, q = px[base + x], px[base + x - 1]
                    if p[3] < 8 or q[3] < 8:
                        continue
                    s += I.de(I.lab(p[0], p[1], p[2]), I.lab(q[0], q[1], q[2]))
            if s > best:
                best, bx, by = s, x0, y0
    return bx, by, bw, bh


def onbg(img):
    o = I.blank(img["w"], img["h"], BG)
    for i, p in enumerate(img["px"]):
        if p[3] > 8:
            o["px"][i] = (p[0], p[1], p[2], 255)
    return o


def build(title_cells, path):
    """title_cells: [(top image, zoom image)] -> one strip."""
    tops = [onbg(t) for t, _ in title_cells]
    zooms = [onbg(z) for _, z in title_cells]
    cw = max(max(t["w"] for t in tops), max(z["w"] for z in zooms))
    th = max(t["h"] for t in tops)
    zh = max(z["h"] for z in zooms)
    W = len(tops) * cw + GAP * (len(tops) + 1)
    H = th + zh + GAP * 3
    c = I.blank(W, H, (8, 8, 10, 255))
    for i, (t, z) in enumerate(zip(tops, zooms)):
        x = GAP + i * (cw + GAP)
        I.paste(c, t, x + (cw - t["w"]) // 2, GAP + (th - t["h"]))
        I.paste(c, z, x + (cw - z["w"]) // 2, GAP * 2 + th)
    I.save(c, path)
    print("strip", path.name, W, "x", H, flush=True)


def one(cls, name, meta, picks, zoom_frac=0.30):
    img = L.load_asset(cls, name, meta)
    disp = meta["disp"]["phone"]
    mode = meta["modes"][0]
    bx, by, bw, bh = busiest(img, int(img["w"] * zoom_frac), int(img["h"] * zoom_frac))
    cells = []
    labels = []
    for label, st in L.candidates(img, meta["pitch"]):
        if label not in picks:
            continue
        d = L.display(st, disp, mode)
        # zoom: same region of the display render, 2x nearest
        zx = round(bx * disp[0] / img["w"])
        zy = round(by * disp[1] / img["h"])
        zw = max(8, round(bw * disp[0] / img["w"]))
        zh = max(8, round(bh * disp[1] / img["h"]))
        zx = min(zx, d["w"] - zw)
        zy = min(zy, d["h"] - zh)
        cells.append((d, I.nearest(I.crop(d, zx, zy, zw, zh), 2)))
        labels.append(f"{label} {st['w']}x{st['h']}")
    build(cells, I.OUT / f"strip_{cls}_{name.replace('.png', '')}.png")
    return labels


def main():
    jobs = L.build_jobs()
    want = sys.argv[1:] if len(sys.argv) > 1 else None
    order = {}
    for cls, name, meta in jobs:
        if want and cls not in want:
            continue
        if cls == "panel":
            picks = ["100%", "75%", "50%", "grid1x"]
        elif cls == "portrait":
            picks = ["100%", "37.5%", "25%", "grid1x"]
        else:
            picks = ["100%", "50%", "37.5%", "grid1x"]
        order[f"{cls}/{name}"] = one(cls, name, meta, picks)
    (I.OUT / "strip_labels.json").write_text(json.dumps(order, indent=1))


if __name__ == "__main__":
    main()
