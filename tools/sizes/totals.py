"""Step 4: projected shipped size, full-res vs recommended, RGBA and indexed PNG bytes."""
import json, os
import assets as A, imglib as I

REC = {}   # class -> (recompute fn)
out = {"full": {}, "rec": {}}

def add(bucket, key, rgba, idx, note=""):
    out[bucket][key] = dict(rgba=rgba, idx=idx, note=note)

# --- panels: unchanged (already at phone display size). Measure indexed for all 29.
pf = sorted((A.ROOT/"story/panels").glob("*.png"))
rects = A.panel_rects()
prgba = pidx = rrgba = ridx = 0
for f in pf:
    im = I.load(f)
    r = I.png_bytes(im); q = I.indexed_bytes(im)
    prgba += r; pidx += q
    # recommended: exactly the 1080p display size when a rect exists, else unchanged
    rect = rects.get(f.name)
    if rect:
        dw, dh = A.panel_display(rect, A.PHONE)
        if dw < im["w"] or dh < im["h"]:
            im2 = I.resample(im, dw, dh)
        else:
            im2 = im
    else:
        im2 = im
    rrgba += I.png_bytes(im2); ridx += I.indexed_bytes(im2)
    print("panel", f.name, im["w"], im["h"], "->", im2["w"], im2["h"], flush=True)
add("full", "panels(29)", prgba, pidx)
add("rec", "panels(29)", rrgba, ridx, "store at 1080p display size")

# --- portraits: 6, recommended 192px tall box -> scale so height = 288
frgba = fidx = rr = ri = 0
for f in sorted((A.ROOT/"story/portraits").glob("*.png")):
    im = I.load(f)
    frgba += I.png_bytes(im); fidx += I.indexed_bytes(im)
    nh = 288; nw = max(1, round(im["w"]*nh/im["h"]))
    im2 = I.resample(im, nw, nh)
    rr += I.png_bytes(im2); ri += I.indexed_bytes(im2)
    print("portrait", f.name, im["w"], im["h"], "->", nw, nh, flush=True)
add("full", "portraits(6)", frgba, fidx)
add("rec", "portraits(6)", rr, ri, "~190x288")

# --- walkers: 4 sheets 1024x1536 -> 512x768 (frames 128x192); plus the 9-frame layout
wr = wi = rr = ri = nr = ni = 0
for f in sorted((A.ROOT/"story/field/walkers").glob("*.png")):
    im = I.load(f)
    wr += I.png_bytes(im); wi += I.indexed_bytes(im)
    im2 = I.resample(im, 512, 768)
    rr += I.png_bytes(im2); ri += I.indexed_bytes(im2)
    print("walker", f.name, "->512x768", flush=True)
add("full", "walkers(4)", wr, wi, "16 frames @256x384")
add("rec", "walkers(4) 16f", rr, ri, "16 frames @128x192")
add("rec", "walkers(4) 9f", round(rr*9/16), round(ri*9/16), "3x3 layout, side mirrored at draw")

json.dump(out, open("out/totals.json","w"), indent=1)
for b in out:
    print(b, {k: (v["rgba"], v["idx"]) for k,v in out[b].items()})
