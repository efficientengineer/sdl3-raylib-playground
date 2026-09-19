"""Item C: near-duplicate art. Atlas entries of equal cell size, and panel crop reuse."""
import json, itertools
import assets as A, imglib as I

# --- atlas: compare every pair of same-shape entries at 32x32 (the logical tile size)
tiles = A.ATLAS_JSON["tiles"]
small = {}
for tid in tiles:
    im, cw, ch = A.load_tile(tid)
    small[tid] = (I.resample(im, 32*cw, 32*ch), cw, ch)
    print("loaded", tid, flush=True)

pairs = []
for a, b in itertools.combinations(small, 2):
    ia, ca, ha = small[a]; ib, cb, hb = small[b]
    if (ca, ha) != (cb, hb): continue
    m, p95, over, er = I.compare(ia, ib)
    pairs.append((round(m, 4), round(over, 3), a, b))
pairs.sort()
json.dump(pairs[:40], open("out/atlas_pairs.json", "w"), indent=1)
for r in pairs[:25]: print(r)
