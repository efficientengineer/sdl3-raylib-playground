"""Step 2: the downscale ladder. For every representative asset, store it at a ladder of
sizes, render each the way the game would at the phone's display size (and at 1440p), and
score it against the display render of the full-res original.

Writes out/ladder.json and the stored PNGs under out/stored/.
"""
import json
import math
import multiprocessing as mp
import sys

import assets as A
import imglib as I

SCALES = [1.0, 0.75, 0.625, 0.5, 0.375, 0.25]
KNEE = dict(mean=0.010, p95=0.04, edge=0.9)


def display(img, size, mode):
    """stored -> display. 'bilinear' is the perceptual proxy the study asks for;
    'nearest' is what the engine actually does on magnification (GL_TEXTURE_MAG_FILTER)."""
    w, h = size
    if mode == "nearest":
        return _nearest_to(img, w, h)
    if w <= img["w"] and h <= img["h"]:
        return I.box_down(img, w, h)
    return I.bilinear(img, w, h)


def _nearest_to(img, nw, nh):
    w, h, px = img["w"], img["h"], img["px"]
    out = [None] * (nw * nh)
    xs = [min(w - 1, int((x + 0.5) * w / nw)) for x in range(nw)]
    for y in range(nh):
        sy = min(h - 1, int((y + 0.5) * h / nh)) * w
        for x in range(nw):
            out[y * nw + x] = px[sy + xs[x]]
    return {"w": nw, "h": nh, "px": out}


def candidates(img, pitch_px):
    """(label, stored image) for the whole ladder."""
    w, h = img["w"], img["h"]
    out = []
    for s in SCALES:
        nw, nh = max(1, round(w * s)), max(1, round(h * s))
        out.append((f"{int(s * 1000) / 10:g}%", I.resample(img, nw, nh)))
    if pitch_px >= 2:
        out.append(("grid1x", I.snap_grid(img, pitch_px, 1)))
        out.append(("grid2x", I.snap_grid(img, pitch_px, 2)))
    return out


def run_asset(job):
    cls, name, meta = job
    img = load_asset(cls, name, meta)
    pitch_px = meta["pitch"]
    res = {"cls": cls, "name": name, "src": [img["w"], img["h"]],
           "pitch": pitch_px, "disp": meta["disp"], "rows": []}
    refs = {}
    for dev, size in meta["disp"].items():
        for mode in meta["modes"]:
            refs[(dev, mode)] = display(img, size, mode)
    for label, st in candidates(img, pitch_px):
        row = {"label": label, "stored": [st["w"], st["h"]],
               "rgba": I.png_bytes(st), "idx": I.indexed_bytes(st), "dev": {}}
        for dev, size in meta["disp"].items():
            for mode in meta["modes"]:
                d = display(st, size, mode)
                mean, p95, over, er = I.compare(refs[(dev, mode)], d)
                row["dev"][f"{dev}/{mode}"] = dict(mean=round(mean, 5), p95=round(p95, 5),
                                                   over=round(over, 4), edge=round(er, 3))
        res["rows"].append(row)
        print(f"  {cls}/{name} {label} {st['w']}x{st['h']} "
              f"mean={row['dev'][list(row['dev'])[0]]['mean']:.4f}", flush=True)
    return res


def load_asset(cls, name, meta):
    if cls == "panel":
        return I.load(A.ROOT / "story/panels" / name)
    if cls == "portrait":
        return I.load(A.ROOT / "story/portraits" / name)
    if cls == "walker":
        n, row, col = meta["frame"]
        return A.load_walk(n, row, col)
    if cls in ("tile", "stamp"):
        return A.load_tile(name)[0]
    raise SystemExit(cls)


def build_jobs():
    pitch = json.loads((I.OUT / "pitch.json").read_text())
    jobs = []

    for _kind, f in A.PANEL_PICKS:
        p = pitch["panel"][f]
        jobs.append(("panel", f, {"pitch": int(round((p["ac_x"] + p["ac_y"]) / 2)),
                                  "disp": {"phone": tuple(p["disp_phone"]),
                                           "big": tuple(p["disp_big"])},
                                  "modes": ["bilinear"]}))
    for f in A.PORTRAIT_PICKS:
        p = pitch["portrait"][f]
        jobs.append(("portrait", f, {"pitch": int(round((p["ac_x"] + p["ac_y"]) / 2)),
                                     "disp": {"phone": tuple(p["disp_phone"]),
                                              "big": tuple(p["disp_big"])},
                                     "modes": ["bilinear", "nearest"]}))
    for name, row, col, lbl in [("falke", 0, 0, "S stand"), ("falke", 1, 1, "W step"),
                                ("ottilie", 0, 0, "S stand"), ("villager_a", 0, 0, "S stand")]:
        p = pitch["walker"][f"{name}[{'SWEN'[row]}{col}]"]
        jobs.append(("walker", f"{name}_{lbl.replace(' ', '')}",
                     {"pitch": int(round((p["ac_x"] + p["ac_y"]) / 2)),
                      "frame": (name, row, col),
                      "disp": {"phone": (96, 144), "big": (128, 192)},
                      "modes": ["bilinear", "nearest"]}))
    for tid in A.TILE_PICKS + A.STAMP_PICKS:
        p = pitch["tile"][tid]
        cw, ch = p["cells"]
        jobs.append(("tile" if tid in A.TILE_PICKS else "stamp", tid,
                     {"pitch": int(round((p["ac_x"] + p["ac_y"]) / 2)),
                      "disp": {"phone": (cw * 96, ch * 96), "big": (cw * 128, ch * 128)},
                      "modes": ["bilinear", "nearest"]}))
    return jobs


def main():
    jobs = build_jobs()
    with mp.Pool(min(10, mp.cpu_count())) as pool:
        out = pool.map(run_asset, jobs)
    (I.OUT / "ladder.json").write_text(json.dumps(out, indent=1))
    print("wrote", I.OUT / "ladder.json")


if __name__ == "__main__":
    main()
