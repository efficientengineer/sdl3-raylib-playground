"""Cutting the two generated-from-art tileset pieces: swatches and decals.

Kept apart from atlas.py so that terrain.py (parsing) and masks.py (baking) can stay
below both without a cycle: this is the only module that needs the parser, the mask
writer and the atlas at once.

Must never do: write an atlas cell; that is atlas.place_in_atlas.
Public: cut_swatches, cut_decals. Imports: atlas, cutter, field, keying, masks, palette,
paths, png, seamless, tileset, tset.
"""
import sys
from .atlas import exact_inner, load_atlas, place_in_atlas, write_atlas_json
from .cutter import scaled_inner, scrub_border
from .field import ATLAS_CELL, EXTRUDE_PX
from .keying import LOUD, check_alpha, key_magenta
from .masks import write_masks
from .palette import PalIndex, master_palette, ship_png
from .paths import ROOT
from .png import crop, opaque_bounds, resample, to_rgba
from .seamless import band_check, flatten_lighting, make_seamless, seam_error, swatch_is_calm
from .tileset import tileset_entries
from .tset import ATLAS_COLS, TSET_SWATCHES, atlas_path, swatch_path, tileset_dir



# ── the swatch sheet: one slot per terrain, and nothing else on it ──


def cut_swatches(data, slots, rows, w, h, ch, sx, sy):
    """Cut, flatten the drawing's own lighting, make it wrap, palettise. The order matters.

    A swatch is the one piece of art in the game that is sampled in WORLD space, so its only job is
    to tile: the flattening and the seam healing are not polish, they are what make it usable at
    all. Both happen in full colour, and the palette step is last, as everywhere else."""
    setname = data["set"]
    out_dir = tileset_dir(setname) / TSET_SWATCHES
    out_dir.mkdir(parents=True, exist_ok=True)
    pal, pidx = master_palette(), PalIndex(master_palette())
    lines = []
    for s in slots:
        x, y, bw, bh = exact_inner(s, sx, sy, w, h)
        tw, th = s["target"]
        px = to_rgba(resample(crop(rows, ch, x, y, bw, bh), bw, bh, ch, tw, th), tw, th, ch)
        scrub_border(px, tw, th, 4, False, max(1, EXTRUDE_PX * 2))
        flat, dev = flatten_lighting(px, tw, th)
        make_seamless(px, tw, th)
        band_check(px, tw, th, s["id"])
        calm, contrast = swatch_is_calm(px, tw, th)
        hz, vt = seam_error(px, tw, th)
        out = swatch_path(setname, s["id"])
        ship_png(out, tw, th, 4, px, pal, pidx)
        note = (f"  wrap h{hz:.0f}/v{vt:.0f}, contrast {contrast:.3f}, "
                f"lighting {dev:.3f}" + (f" -> flattened {flat:.3f}" if flat else " (flat already)"))
        if not calm:
            LOUD.append(f"{s['id']}'s swatch is busy (contrast {contrast:.3f}): it will fight the "
                        f"decals and show its own repeat. Ask for that slot again, calmer — three "
                        f"or four flat tones, close together, nothing the eye goes to.")
            print(f"WARNING: {LOUD[-1]}", file=sys.stderr)
            note += "  <-- BUSY"
        lines.append(f"  slot {s['n']} {s['id']}: {bw}x{bh} -> {out.relative_to(ROOT.parent)}  "
                     f"{tw}x{th} ({s['tiles'][0]}x{s['tiles'][1]} tiles){note}")
    for l in lines:
        print(l)
    mp, n = write_masks(setname, [e["edge_style"] for e in tileset_entries(setname).values()
                                  if e.get("kind") == "terrain"])
    print(f"  -> {mp.relative_to(ROOT.parent)}  ({n} masks, generated — no art)")


def cut_decals(data, slots, rows, w, h, ch, sx, sy, fringe=0):
    """Key the magenta, trim to the object, scale, palettise, and pack into the ATLAS.

    A decal is drawn from `atlas.png` like every other sprite — one cell each, the art in the cell's
    top-left corner and its real pixel size in `atlas.json` — because the engine has one tile shader
    with one atlas bound and a second texture of loose files would be a second batch for the smallest
    thing on screen. The loose `decals/<id>.png` is still written: the preview uses it, and it is how
    you look at one."""
    setname = data["set"]
    entries = tileset_entries(setname)
    pal, pidx = master_palette(), PalIndex(master_palette())
    need = max((entries[s["id"]]["index"] // ATLAS_COLS) + 1 for s in slots if entries.get(s["id"]))
    atlas = load_atlas(setname, need)
    for s in slots:
        x, y, bw, bh = scaled_inner(s, sx, sy, w, h)
        px = key_magenta(crop(rows, ch, x, y, bw, bh), bw, bh, ch, fringe)
        scrub_border(px, bw, bh, 4, True, max(2, int(round(2 * sx))))
        bounds = opaque_bounds(px, bw, bh)
        if not bounds:
            print(f"  slot {s['n']} {s['id']}: nothing but background in the slot; not written",
                  file=sys.stderr)
            continue
        cx, cy, cw, chh = bounds
        px = crop(px, 4, cx, cy, cw, chh)
        k = s["target"][0] / max(bw, bh)
        nw, nh = max(2, round(cw * k)), max(2, round(chh * k))
        px = resample(px, cw, chh, 4, nw, nh)
        out = ROOT.parent / s["out"]
        ship_png(out, nw, nh, 4, px, pal, pidx)
        e = entries.get(s["id"])
        where = ""
        if e and e["index"] is not None:                  # into its own atlas cell, top-left
            cell = [bytearray(ATLAS_CELL * 4) for _ in range(ATLAS_CELL)]
            for y in range(min(nh, ATLAS_CELL)):
                cell[y][:min(nw, ATLAS_CELL) * 4] = px[y][:min(nw, ATLAS_CELL) * 4]
            place_in_atlas(atlas, e["index"], cell, 1, 1)
            where = f", atlas index {e['index']}"
        print(f"  slot {s['n']} {s['id']}: {bw}x{bh} -> trimmed {cw}x{chh} -> "
              f"{out.relative_to(ROOT.parent)}  {nw}x{nh}{where}"
              f"{check_alpha(px, nw, nh, out)}")
    ap = atlas_path(setname)
    ap.parent.mkdir(parents=True, exist_ok=True)
    ship_png(ap, ATLAS_COLS * ATLAS_CELL, len(atlas), 4, atlas, pal, pidx)
    write_atlas_json(setname, entries)
    print(f"  -> {ap.relative_to(ROOT.parent)}  {ATLAS_COLS * ATLAS_CELL}x{len(atlas)} "
          f"({len(slots)} decal(s) packed), atlas.json updated")
