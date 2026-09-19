#!/usr/bin/env python3
"""Same test images as metrics.json, three palettes, one exact nearest-in-Oklab converter.

  python3 tools/palette/metrics_new.py [image ...]

Prints a table of mean / p95 dE and the share of pixels over dE 0.05 for:
  statistical  tools/palette/master.json        (the first experiment: k-means per category)
  ramps v1     tools/palette/master_ramps.json  (13 fixed 16-step ramps + extras)
  ramps v2     story/palette/master.hex         (variable-length ramps, the shipped master)
"""
import json
import sys
import time
from pathlib import Path

HERE = Path(__file__).resolve().parent
ROOT = HERE.parent.parent
sys.path.insert(0, str(ROOT))
import story_prompt as sp                                                  # noqa: E402

TESTS = ["tilesets-valley-objects.png", "tilesets-valley-terrain.png",
         "cast-falke-walker.png", "cast-ottilie-walker.png", "cast-hart-refsheet.png",
         "ch01-halm-scenes-0110_the_board.png", "ch01-stair_shrine-scenes-0170_the_jar.png"]
GROUP = {"tilesets-valley-objects.png": "tiles", "tilesets-valley-terrain.png": "tiles",
         "cast-falke-walker.png": "walkers", "cast-ottilie-walker.png": "walkers",
         "cast-hart-refsheet.png": "refsheet",
         "ch01-halm-scenes-0110_the_board.png": "panel",
         "ch01-stair_shrine-scenes-0170_the_jar.png": "panel"}


def pal_from_json(p):
    return [tuple(c) for c in json.loads(Path(p).read_text())["palette"]]


def measure(path, pal):
    """Exact nearest for every unique colour, weighted by how often it occurs."""
    w, h, ch, rows = sp.read_png(path)
    counts = {}
    for y in range(h):
        row = rows[y]
        for x in range(w):
            i = x * ch
            if ch == 4 and row[i + 3] < 128:
                continue
            if not sp.corpus_keep(row[i], row[i + 1], row[i + 2]):
                continue                     # the template's magenta, its borders, the sheet's black
            k = (row[i] << 16) | (row[i + 1] << 8) | row[i + 2]
            counts[k] = counts.get(k, 0) + 1
    idx = sp.PalIndex(pal)
    labs = [sp._to_oklab(*c) for c in pal]
    t0 = time.time()
    errs = []
    for k, n in counts.items():
        r, g, b = (k >> 16) & 255, (k >> 8) & 255, k & 255
        j = idx.of(r, g, b)
        p, q = sp._to_oklab(r, g, b), labs[j]
        d = ((p[0] - q[0]) ** 2 + (p[1] - q[1]) ** 2 + (p[2] - q[2]) ** 2) ** 0.5
        errs.append((d, n))
    errs.sort()
    tot = sum(n for _, n in errs) or 1
    mean = sum(d * n for d, n in errs) / tot
    acc, p95 = 0, errs[-1][0] if errs else 0.0
    for d, n in errs:
        acc += n
        if acc >= tot * 0.95:
            p95 = d
            break
    over = 100.0 * sum(n for d, n in errs if d > 0.05) / tot
    return {"uniques": len(counts), "px": tot, "mean": mean, "p95": p95, "over05": over,
            "secs": time.time() - t0}


def main():
    pals = {"statistical": pal_from_json(HERE / "master.json"),
            "ramps v1": pal_from_json(HERE / "master_ramps.json"),
            "ramps v2": sp.read_hex(sp.MASTER_HEX)}
    names = sys.argv[1:] or TESTS
    print(f"{'image':42s} {'group':8s} {'palette':12s} {'mean dE':>8s} {'p95':>7s} "
          f"{'>0.05%':>7s} {'uniq':>8s} {'secs':>6s}")
    rows = []
    for n in names:
        p = sp.SHEETS / n
        for label, pal in pals.items():
            st = measure(p, pal)
            rows.append((n, label, st))
            print(f"{n:42s} {GROUP.get(n, '?'):8s} {label:12s} {st['mean']:8.4f} {st['p95']:7.4f} "
                  f"{st['over05']:7.2f} {st['uniques']:8d} {st['secs']:6.1f}")
    print()
    for grp in ("tiles", "walkers", "refsheet", "panel"):
        base = None
        for label in pals:
            sel = [st["mean"] for n, l, st in rows if l == label and GROUP.get(n) == grp]
            if not sel:
                continue
            m = sum(sel) / len(sel)
            base = m if label == "statistical" else base
            print(f"{grp:10s} {label:12s} mean dE {m:.4f}"
                  + (f"   {100 * (m / base - 1):+.1f}% vs statistical" if base and label != "statistical" else ""))
    (HERE / "metrics_new.json").write_text(
        "\n".join(json.dumps({"file": n, "palette": l, **st}) for n, l, st in rows) + "\n")


if __name__ == "__main__":
    main()
