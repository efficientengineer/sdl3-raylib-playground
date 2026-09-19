#!/usr/bin/env python3
"""Side-by-side proof images: the cut in full colour, and the 256-colour file the game ships.

  python3 tools/palette/proof.py

The left half is the *same cut* with the palette step switched off (`story_prompt.PALETTE_OFF`), so
the only difference between the halves is the palette — not the keying, not the scaling, not the
seam healing. Output goes to tools/palette/out/ (gitignored) at 50% scale.
"""
import json
import sys
from pathlib import Path

HERE = Path(__file__).resolve().parent
ROOT = HERE.parent.parent
OUT = HERE / "out"
sys.path.insert(0, str(ROOT))
import story_prompt as sp                                                  # noqa: E402

GAP, GUTTER = 14, (26, 26, 30)
CHECK = (46, 34, 52)          # what transparency is shown as: neither black nor magenta

PACKAGES = [
    ("p1_atlas",     "story/packages/tilesets/valley/terrain",       "story/field/tilesets/valley/atlas.png"),
    ("p2_falke",     "story/packages/cast/falke/walker",             "story/field/walkers/falke.png"),
    ("p3_ottilie",   "story/packages/cast/ottilie/walker",           "story/field/walkers/ottilie.png"),
    ("p4_hart",      "story/packages/cast/hart/refsheet",            "story/portraits/hart.png"),
    ("p5_stolz",     "story/packages/cast/stolz/refsheet",           "story/portraits/stolz.png"),
    ("p6_panels",    "story/packages/ch01/stair_shrine/scenes/0170_the_jar", None),
]


def flat(w, h, ch, rows, bg=CHECK):
    out = []
    for y in range(h):
        line, r = bytearray(), rows[y]
        for x in range(w):
            i = x * ch
            line += bytes(bg) if (ch == 4 and r[i + 3] < 128) else bytes(r[i:i + 3])
        out.append(line)
    return out


def half(path, tw):
    w, h, ch, rows = sp.read_png(Path(path))
    rgb = flat(w, h, ch, rows)
    th = max(1, round(h * tw / w))
    return sp.resize_box(rgb, w, h, 3, tw, th), th


def pair(name, before, after, width):
    a, ah = half(before, width)
    b, bh = half(after, width)
    n = min(ah, bh)
    rows = [bytearray(a[y]) + bytes(GUTTER) * GAP + bytearray(b[y]) for y in range(n)]
    OUT.mkdir(parents=True, exist_ok=True)
    p = OUT / f"{name}.png"
    sp.write_png(p, width * 2 + GAP, n, 3, rows)
    print(f"  {p.relative_to(ROOT)}  {width * 2 + GAP}x{n}   "
          f"{Path(before).name} {Path(before).stat().st_size} B full colour  ->  "
          f"{Path(after).name} {Path(after).stat().st_size} B indexed")
    return p


def raw_of(shipped):
    return Path(shipped).with_suffix(".raw.png")


def main():
    made = []
    for name, pkg, shipped in PACKAGES:
        d = ROOT / pkg
        meta = json.loads((d / "package.json").read_text())
        returned = sp.pkg_returned(d)
        if not returned:
            print(f"  skip {pkg}: no returned image")
            continue
        import contextlib
        import io
        sp.PALETTE_OFF = True
        try:
            with contextlib.redirect_stdout(io.StringIO()), contextlib.redirect_stderr(io.StringIO()):
                ok, msg = sp.ingest_one(d, meta, returned)
        finally:
            sp.PALETTE_OFF = False
        if meta.get("kind") == "sheet":
            # slice writes the panels themselves with write_png, so the shipped files are full colour
            # again for a moment; putting the scene back through its own palette restores them.
            for scene in sorted({p["scene"] for p in json.loads((d / "sheet.json").read_text())["panels"]}):
                with contextlib.redirect_stdout(io.StringIO()):
                    sp.palettise_scene(scene)
        if not ok:
            print(f"  FAIL {pkg}: {msg}")
            continue
        if shipped is None:                                  # a shot sheet: take its widest panel
            panels = sorted((ROOT / "story/panels").glob("0170_the_jar_p*.png"))
            shipped = max((p for p in panels if raw_of(p).exists()),
                          key=lambda p: raw_of(p).stat().st_size, default=None)
            if shipped is None:
                print(f"  skip {pkg}: no panel raw written")
                continue
        shipped = Path(shipped) if Path(shipped).is_absolute() else ROOT / shipped
        raw = raw_of(shipped)
        if not raw.exists():
            print(f"  skip {pkg}: {raw.name} not written")
            continue
        made.append(pair(name, raw, shipped, 620))
    for p in ROOT.rglob("*.raw.png"):                        # the full-colour copies are scratch
        p.unlink()
    print(f"{len(made)} proof image(s) in {OUT}")
    return made


if __name__ == "__main__":
    main()
