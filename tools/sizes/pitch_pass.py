"""Step 1: measure the fat-pixel pitch of every asset class. Writes out/pitch.json."""
import json
import statistics
import sys

import assets as A
import imglib as I


def measure(img):
    ac = I._autocorr_pitch(img)
    gp = I.grid_pitch(img)
    return {"ac_x": ac[0][0], "ac_x_r": ac[0][1], "ac_y": ac[1][0], "ac_y_r": ac[1][1],
            "grid_x": gp["x"][0], "grid_x_lift": gp["x"][1],
            "grid_y": gp["y"][0], "grid_y_lift": gp["y"][1],
            "w": img["w"], "h": img["h"]}


def main():
    out = {}

    # --- panels (all 29)
    rects = A.panel_rects()
    out["panel"] = {}
    for f in sorted((A.ROOT / "story/panels").glob("*.png")):
        img = I.load(f)
        m = measure(img)
        r = rects.get(f.name)
        if r:
            m["rect"] = r
            m["disp_phone"] = A.panel_display(r, A.PHONE)
            m["disp_big"] = A.panel_display(r, A.BIG)
        out["panel"][f.name] = m
        print("panel", f.name, m["ac_x"], m["ac_y"], m.get("disp_phone"), flush=True)

    # --- portraits
    out["portrait"] = {}
    for f in sorted((A.ROOT / "story/portraits").glob("*.png")):
        img = I.load(f)
        m = measure(img)
        m["disp_phone"] = A.portrait_display(img["w"] / img["h"], A.PHONE)
        m["disp_big"] = A.portrait_display(img["w"] / img["h"], A.BIG)
        out["portrait"][f.name] = m
        print("portrait", f.name, m["ac_x"], m["ac_y"], m["disp_phone"], flush=True)

    # --- walker frames (every frame of every sheet)
    out["walker"] = {}
    for name in ("falke", "ottilie", "villager_a", "villager_b"):
        for row in range(4):
            for col in range(4):
                img = A.load_walk(name, row, col)
                out["walker"][f"{name}[{'SWEN'[row]}{col}]"] = measure(img)
        print("walker", name, out["walker"][f"{name}[S0]"]["ac_x"], flush=True)

    # --- atlas tiles and stamps
    out["tile"] = {}
    for tid in list(A.ATLAS_JSON["tiles"]):
        img, cw, ch = A.load_tile(tid)
        m = measure(img)
        m["cells"] = [cw, ch]
        out["tile"][tid] = m
    print("tiles done", flush=True)

    (I.OUT / "pitch.json").write_text(json.dumps(out, indent=1))

    for cls in out:
        vals = []
        for k, v in out[cls].items():
            vals += [v["ac_x"], v["ac_y"]]
        print(f"{cls:9s} n={len(out[cls])} median pitch={statistics.median(vals)} "
              f"range={min(vals)}-{max(vals)}")


if __name__ == "__main__":
    main()
