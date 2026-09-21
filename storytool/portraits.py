"""The `portraits` command: a reference sheet's middle panel becomes the portrait.

Reuses the slicer's border detection, needs exactly three panels on the sheet, and warns
and skips otherwise. fast_reload.sh and deploy.sh run this before the export, so
regenerating a reference sheet updates the in-game portrait.

Must never do: touch story/refs/ — it only reads it.
Public: portrait_from_ref, cmd_portraits. Imports: cast, cutter, md, palette, paths, png,
rules.
"""
import sys
from .cast import load_cast
from .cutter import find_panels
from .md import existing
from .palette import ship_png
from .paths import PORTRAITS, ROOT
from .png import read_png, resample
from .rules import PORTRAIT_H, PORTRAIT_TRIM



def portrait_from_ref(key, c):
    """One character's dialogue-box portrait, cut out of their reference sheet. (ok, message)."""
    ref = existing(c.get("ref"))
    if not ref:
        return False, (f"no reference sheet for {c['name']} at {c.get('ref')}; "
                       f"run: story_prompt.py refsheet {c['name']}")
    w, h, ch, rows = read_png(ref)
    boxes = find_panels(w, h, ch, rows)
    if len(boxes) != 3:
        return False, (f"{ref.name} yielded {len(boxes)} panel(s), not the reference sheet's 3 "
                       f"(full body | portrait | profile); no portrait written for {c['name']}")
    x0, y0, x1, y1 = sorted(boxes, key=lambda b: b[0] + b[2])[1]          # middle by x = the portrait
    t = max(2, int(min(x1 - x0, y1 - y0) * PORTRAIT_TRIM))                # shave the white border
    x0, y0, x1, y1 = x0 + t, y0 + t, x1 - t, y1 - t
    PORTRAITS.mkdir(exist_ok=True)
    out = PORTRAITS / f"{key}.png"
    cw, chh = x1 - x0, y1 - y0
    px = [rows[y][x0 * ch:x1 * ch] for y in range(y0, y1)]
    nw, nh = cw, chh
    if chh > PORTRAIT_H:                      # area-average down to the size it is actually drawn at
        nh = PORTRAIT_H
        nw = max(1, round(cw * PORTRAIT_H / chh))
        px = resample(px, cw, chh, ch, nw, nh)
    ship_png(out, nw, nh, ch, px)
    return True, (f"wrote {out.relative_to(ROOT.parent)}  {nw}x{nh}"
                  + (f"  (from {cw}x{chh})" if (nw, nh) != (cw, chh) else "")
                  + f"  (ships as portrait_{key}.png)")


def cmd_portraits(args):
    """story/refs/<name>.png -> story/portraits/<name>.png: the reference sheet's middle panel.

    The sheet is full body | head-and-shoulders portrait | profile (see the 'refsheet' block in
    STYLE.md), so the portrait is the middle box by x. The game draws it beside the dialogue box."""
    cast = load_cast()
    PORTRAITS.mkdir(exist_ok=True)
    made = 0
    for key, c in cast.items():
        ok, msg = portrait_from_ref(key, c)
        print(msg if ok else f"warning: {msg}", file=sys.stdout if ok else sys.stderr)
        made += 1 if ok else 0
    if not made:
        print("warning: no portraits written; the game falls back to a dialogue box with no portrait",
              file=sys.stderr)
