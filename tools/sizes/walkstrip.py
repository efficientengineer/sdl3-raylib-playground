"""Visual: walker frame comparisons (duplicate stands, W vs mirrored E, step mirrors)."""
import assets as A, imglib as I, redundancy as R

BG = (26, 26, 34, 255)

def onbg(img):
    o = I.blank(img["w"], img["h"], BG)
    for i, p in enumerate(img["px"]):
        if p[3] > 8:
            o["px"][i] = (p[0], p[1], p[2], 255)
    return o

def strip(tiles, path, scale=1):
    tiles = [I.resample(t, t["w"]*scale, t["h"]*scale) if scale != 1 else t for t in tiles]
    W = sum(t["w"] for t in tiles) + 8*(len(tiles)+1)
    H = max(t["h"] for t in tiles) + 16
    c = I.blank(W, H, (12, 12, 16, 255))
    x = 8
    for t in tiles:
        I.paste(c, t, x, 8); x += t["w"] + 8
    I.save(c, path); print(path, W, H)

def main():
    for name in R.NAMES:
        fr = R.frames(name)
        half = lambda im: I.resample(im, 128, 192)
        # W row vs mirrored E row + heatmaps
        row = []
        for c in R.COLS:
            a, b = R.align(R.mirror(fr[f"E{c}"]), fr[f"W{c}"])
            m, o, heat = R.diff(a, b)
            row += [half(onbg(b)), half(onbg(a)), half(heat)]
        strip(row, I.OUT / f"strip_{name}_W_vs_mirroredE.png")
        # duplicate stand check, S row: stand | stand2 | heat, and step mirror
        a, b = R.align(fr["Sstand"], fr["Sstand2"]); m, o, heat = R.diff(a, b)
        t1 = [half(onbg(a)), half(onbg(b)), half(heat)]
        a, b = R.align(fr["SstepL"], R.mirror(fr["SstepR"])); m2, o2, h2 = R.diff(a, b)
        t1 += [half(onbg(a)), half(onbg(b)), half(h2)]
        strip(t1, I.OUT / f"strip_{name}_dupstand_stepmirror.png")

main()
