"""The `slice` and `cut` commands: pick the right cutter for a returned image.

cmd_cut reads a template sheet's sheet.json, works out which kind of package it came
from, and calls that kind's cutter; cmd_slice redirects here when the JSON is a template
package rather than a shot sheet. It refuses an image whose margins are not the
template's background colour — that is what a file from the wrong package looks like.

Must never do: contain a cutter. Adding a package kind adds a case here and a module.
Public: cmd_slice, cmd_cut. Imports: atlas, cutter, expressions_cut, field, manifest,
md, palette_cmd, paths, png, rules, sprites_cut, tset_cut, walkers_cut.
"""
from pathlib import Path
import json
from .atlas import cut_tileset
from .cutter import find_panels, localise_slots, match_boxes
from .expressions_cut import cut_expressions
from .field import FIELD_KINDS, KIND_DIR, SLOT_MARGIN, WALK_OUT_H, WALK_OUT_W
from .manifest import kind_rows, write_field_manifest
from .md import die
from .palette_cmd import palettise_scene
from .paths import FIELD_MANIFEST, PANELS, ROOT
from .png import read_png, write_png
from .rules import SHAPE_ASPECT
from .sprites_cut import cut_sprites
from .tset_cut import cut_decals, cut_swatches
from .walkers_cut import cut_walker



def cmd_slice(args):
    pos, trim, manual, it = [], 0, None, iter(args)
    for a in it:
        if a == "--trim":
            trim = int(next(it, "0"))
        elif a == "--boxes":
            manual = next(it, "")
        else:
            pos.append(a)
    if len(pos) != 2:
        die('usage: slice story/out/<name>.sheet.json <image> [--trim N] [--boxes "x,y,w,h;..."]')
    manifest, image = json.loads(Path(pos[0]).read_text()), Path(pos[1]).expanduser()
    if manifest.get("kind") in FIELD_KINDS + ("expressions",):   # a template package: the boxes are known
        return cmd_cut(args)                             # --trim and --boxes mean nothing there, --fringe does
    if not image.exists():
        die(f"{image} not found")
    w, h, ch, rows = read_png(image)
    if manual:
        boxes = [[x, y, x + bw, y + bh] for x, y, bw, bh in
                 (map(int, part.split(",")) for part in manual.split(";") if part.strip())]
    else:
        boxes = find_panels(w, h, ch, rows)
    want = manifest["panels"]
    print(f"{image.name}: {w}x{h}, found {len(boxes)} panel(s), sheet expects {len(want)}")
    for n, b in enumerate(boxes, 1):
        print(f"  box {n}: x={b[0]} y={b[1]} w={b[2]-b[0]} h={b[3]-b[1]}")
    if len(boxes) != len(want):
        die("panel count mismatch, nothing written. Panels probably touch or overlap: ask ChatGPT to regenerate "
            'with wider black gutters, or pass the boxes by hand with --boxes "x,y,w,h;x,y,w,h".')
    if manifest.get("rects") and not manual:
        boxes = match_boxes(boxes, manifest["rects"], w, h)
    PANELS.mkdir(exist_ok=True)
    for b, p in zip(boxes, want):
        x0, y0, x1, y1 = b[0] + trim, b[1] + trim, b[2] - trim, b[3] - trim
        out = PANELS / f"{p['scene']}_p{p['scene_panel']}_{p['shot']}.png"
        write_png(out, x1 - x0, y1 - y0, ch, [rows[y][x0 * ch:x1 * ch] for y in range(y0, y1)])
        got, exp = (x1 - x0) / (y1 - y0), SHAPE_ASPECT[p["shape"]]
        note = "" if 0.6 * exp <= got <= 1.6 * exp else f"   <-- aspect {got:.2f}, expected about {exp:.1f} ({p['shape']}): check the order"
        print(f"  wrote {out.relative_to(ROOT.parent)}  {x1-x0}x{y1-y0}{note}")
    for scene in sorted({p["scene"] for p in want}):      # D19: a scene's panels get their own palette
        print("  " + palettise_scene(scene))


def cmd_cut(args):
    pos, fringe, palette, it = [], 0, 0, iter(args)
    for a in it:
        if a == "--fringe":
            fringe = int(next(it, "0"))
        elif a == "--palette":
            palette = int(next(it, "0"))
        elif not a.startswith("--"):
            pos.append(a)
    heal, snap = "--no-heal" not in args, "--snap32" in args
    if len(pos) != 2:
        die("usage: cut story/out/<name>.sheet.json <image> [--fringe N] "
            "[--palette N] [--no-heal] [--snap32]")
    data = json.loads(Path(pos[0]).read_text())
    image = Path(pos[1]).expanduser()
    kind = data.get("kind")
    if kind not in FIELD_KINDS + ("tileset", "swatch", "decal", "expressions"):
        die(f"{pos[0]} is not a template package (kind '{kind}'). For a shot sheet use: story_prompt.py slice")
    if not image.exists():
        die(f"{image} not found")
    slots, (tw, th) = data["slots"], data["canvas"]
    if kind == "walker" and len(slots) % 3 and len(slots) != 16:
        die(f"{pos[0]} has {len(slots)} slots; a walk sheet is 3 columns x 3 or 4 rows a character "
            f"(or the 4x4 grid of a sheet drawn before D20). Nothing written.")
    w, h, ch, rows = read_png(image)
    sx, sy = w / tw, h / th
    if abs((w / h) / (tw / th) - 1) > 0.06:
        die(f"{image.name} is {w}x{h}, the template is {tw}x{th}: a different shape, so the slot boxes "
            f"would not line up. Ask ChatGPT to redraw the template at its own proportions. Nothing written.")
    bg, off, pts = data.get("background", [0, 0, 0]), 0, []
    for k in range(1, 12):                              # the outer margin: outside every slot, by construction
        m = int(SLOT_MARGIN * sx / 2)
        pts += [(m, k * h // 12), (w - 1 - m, k * h // 12), (k * w // 12, m), (k * w // 12, h - 1 - m)]
    for x, y in pts:
        px = rows[y][x * ch:x * ch + 3]
        off += sum((a - b) ** 2 for a, b in zip(px, bg)) ** 0.5 > 90
    if off > len(pts) * 0.5:
        die(f"{image.name}'s margins are not the template's background colour "
            f"(rgb {tuple(bg)}); {off} of {len(pts)} sampled points differ. Either this is the wrong file for "
            f"this package, or the generator repainted the background. Nothing written.")
    print(f"{image.name}: {w}x{h}, template {tw}x{th} ({sx:.2f}x), {len(slots)} slot(s), kind {kind}")
    localise_slots(rows, w, h, ch, slots, sx, sy, f" ({image.name})")

    if kind == "tileset":                                 # a tile sheet: snap, key, pack into the atlas
        return cut_tileset(data, slots, rows, w, h, ch, sx, sy, palette, heal, snap)
    if kind == "swatch":                                  # D20: flatten, make it wrap, palettise
        return cut_swatches(data, slots, rows, w, h, ch, sx, sy)
    if kind == "decal":                                   # D20: key, trim to the object, palettise
        return cut_decals(data, slots, rows, w, h, ch, sx, sy, fringe)
    if kind == "expressions":                             # ten dialogue-box faces: opaque, like a portrait
        return cut_expressions(data, slots, rows, w, h, ch, sx, sy, "--neutral-main" in args)
    if kind == "sprites":                                 # billboards: key, trim, one box per sprite
        cut_sprites(slots, rows, w, h, ch, sx, sy, fringe)
    else:
        cut_walker(data, slots, rows, w, h, ch, sx, sy, fringe)

    rows_out = ([{"id": data["template"].split("_", 1)[1], "kind": "walker",
                  "target": f"{WALK_OUT_W * 4}x{WALK_OUT_H * 4}", "package": data["template"]}]
                if kind == "walker" else kind_rows(KIND_DIR[kind], slots, data["template"]))
    write_field_manifest(rows_out)
    print(f"{FIELD_MANIFEST.relative_to(ROOT.parent)} updated")
