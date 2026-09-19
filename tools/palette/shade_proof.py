#!/usr/bin/env python3
"""Look at a colormap table on real art, not just on the swatch.

  python3 tools/palette/shade_proof.py [table ...]

For each table it writes tools/palette/out/s_<table>.png: the valley atlas and Falke's walk sheet
with that table applied at a few light levels, side by side with full light. A table that reads wrong
(the first `night` was magenta) is obvious here and invisible in a strip of 256 swatches.
"""
import json
import sys
from pathlib import Path

HERE = Path(__file__).resolve().parent
ROOT = HERE.parent.parent
OUT = HERE / "out"
sys.path.insert(0, str(ROOT))
import story_prompt as sp                                                  # noqa: E402

ART = [("atlas", "story/field/tilesets/valley/atlas.png", 0, 0, 2048, 1400),
       ("falke", "story/field/walkers/falke.png", 0, 0, None, None)]
SHOWN = [("full light", "day", 0), ("level 8", None, 8), ("level 16", None, 16),
         ("level 24", None, 24), ("level 31", None, 31)]
GAP, GUTTER = 12, (24, 24, 28)


def indices(path):
    """The stored index of every pixel, by matching the decoded RGBA back to the PLTE."""
    ctype, plte, trns = sp.png_palette(ROOT / path)
    if ctype != 3:
        sp.die(f"{path} is not indexed")
    w, h, ch, rows = sp.read_png(ROOT / path)
    back = {}
    for i, c in enumerate(plte):
        back.setdefault(tuple(c), i)
    out = []
    for y in range(h):
        r, o = rows[y], bytearray(w)
        for x in range(w):
            i = x * 4
            o[x] = 0 if r[i + 3] < 128 else back.get((r[i], r[i + 1], r[i + 2]), 0)
        out.append(o)
    return w, h, out


def colormap_rows():
    meta = json.loads((sp.PALETTE_DIR / "colormap.json").read_text())
    w, h, ch, rows = sp.read_png(sp.COLORMAP_PNG)
    return meta, ch, rows


def main():
    meta, ch, cmrows = colormap_rows()
    row0 = {t["table"]: t["row0"] for t in meta["tables"]}
    want = sys.argv[1:] or ["night", "lamp", "dusk"]
    for table in want:
        if table not in row0:
            print(f"  no table '{table}'")
            continue
        bands = []
        for label, over, lv in SHOWN:
            band = []
            for name, path, x0, y0, cw, chh in ART:
                w, h, idx = indices(path)
                cw = min(cw or w, w)
                chh = min(chh or h, h)
                cm = cmrows[row0[over or table] + lv]
                px = []
                for y in range(y0, y0 + chh):
                    line = bytearray()
                    for x in range(x0, x0 + cw):
                        i = idx[y][x]
                        if i == 0:
                            line += bytes((24, 20, 28))
                        else:
                            line += bytes(cm[i * ch:i * ch + 3])
                    px.append(line)
                band.append((cw, chh, px))
            bands.append((label, band))
        # one column per light level, art stacked in it
        colw = 300
        cols = []
        for label, band in bands:
            stack, W = [], colw
            for cw, chh, px in band:
                th = max(1, round(chh * colw / cw))
                stack += sp.resize_box(px, cw, chh, 3, colw, th)
                stack += [bytearray(bytes(GUTTER) * colw) for _ in range(GAP)]
            cols.append(stack)
        H = max(len(c) for c in cols)
        for c in cols:
            c += [bytearray(bytes(GUTTER) * colw) for _ in range(H - len(c))]
        rows = []
        for y in range(H):
            line = bytearray()
            for n, c in enumerate(cols):
                if n:
                    line += bytes(GUTTER) * GAP
                line += c[y]
            rows.append(line)
        W = len(cols) * colw + (len(cols) - 1) * GAP
        OUT.mkdir(parents=True, exist_ok=True)
        p = OUT / f"s_{table}.png"
        sp.write_png(p, W, H, 3, rows)
        print(f"  {p.relative_to(ROOT)}  {W}x{H}   columns: "
              + ", ".join(l for l, _, _ in SHOWN))


if __name__ == "__main__":
    main()
