"""Baking the dual-grid edge masks a terrain's boundary is drawn with.

Four images a terrain became one: the swatch is sampled in world space and its edge is
one of these generated 1-bit masks, rotated. The invariant that must never change is that
a mask's boundary crosses each tile edge at that edge's midpoint — that is what makes any
two terrains meet cleanly.

Must never do: break that midpoint invariant, or bake a mask that is not deterministic in
(shape, style, variant).
Public: h32, vnoise, mask_sdf, bake_mask, bits_rot, dual_grid_cases, tile_mask,
write_masks, DUAL_CASES. Imports: md, palette, tset.
"""
import json
import math
from .md import die
from .palette import PAL_BLACK, PAL_WHITE, master_palette, write_indexed_png
from .tset import EDGE_STYLES, MASK_PX, MASK_SHAPES, MASK_VARIANTS, MASK_WINDOW, TSET_MASKS, TSET_MASKS_JSON, tileset_dir



def h32(*v):
    """FNV-1a. The engine computes the same hash for the same (i, j, terrain), so the tool's preview
    and the phone scatter identically — keep the constants if this is ever ported."""
    n = 2166136261
    for x in v:
        n = ((n ^ (int(x) & 0xffffffff)) * 16777619) & 0xffffffff
    return n


def vnoise(x, y, seed):
    """Value noise on the unit lattice, smoothstepped. Range about -1..1."""
    xi, yi = math.floor(x), math.floor(y)
    tx, ty = x - xi, y - yi
    tx = tx * tx * (3 - 2 * tx)
    ty = ty * ty * (3 - 2 * ty)
    r = lambda a, b: (h32(a, b, seed) & 0xffff) / 65535.0 * 2 - 1
    a = r(xi, yi) * (1 - tx) + r(xi + 1, yi) * tx
    b = r(xi, yi + 1) * (1 - tx) + r(xi + 1, yi + 1) * tx
    return a * (1 - ty) + b * ty


def mask_sdf(shape, x, y):
    """Signed distance over the unit display tile, positive inside the terrain.

    Radius 0.5 is not taste. It is what puts every boundary through the midpoint of the tile edge it
    crosses, which is the invariant that lets any shape meet any other shape, in any rotation, in any
    variant, with no break. Change it and the set stops composing."""
    if shape == "full":
        return 1.0
    if shape == "edge":
        return 0.5 - y                                             # terrain in the north half
    if shape == "corner":
        return 0.5 - math.hypot(x, y)                              # quarter disc at NW
    if shape == "diag":                                            # two quarter discs TOUCHING at the
        return max(0.5 - math.hypot(x, y),                         # centre: the saddle is drawn
                   0.5 - math.hypot(x - 1, y - 1))                 # connected, by convention
    if shape == "inv":
        return math.hypot(x - 1, y - 1) - 0.5                      # all but a bite at SE
    die(f"unknown mask shape '{shape}'")


def bake_mask(shape, style, variant, px=MASK_PX):
    """One mask as rows of 0/1. Rotation is not baked: the engine swizzles the UV."""
    a1, f1, a2, f2 = EDGE_STYLES[style]
    seed = h32(MASK_SHAPES.index(shape), sorted(EDGE_STYLES).index(style), variant)
    rows = []
    for py in range(px):
        y = (py + 0.5) / px
        line = bytearray(px)
        for x_ in range(px):
            x = (x_ + 0.5) / px
            d = mask_sdf(shape, x, y)
            if shape != "full":
                win = min(1.0, min(x, y, 1 - x, 1 - y) / MASK_WINDOW)
                d += win * (a1 * vnoise(x * f1, y * f1, seed)
                            + a2 * vnoise(x * f2, y * f2, seed + 7))
            line[x_] = 1 if d > 0 else 0
        rows.append(line)
    return rows


def bits_rot(b):
    """One clockwise 90-degree step of the corner bit word (1 NW, 2 NE, 4 SE, 8 SW)."""
    return ((b << 1) | (b >> 3)) & 15


def dual_grid_cases():
    """{corner bits: (shape, rotation)} for all 16 patterns, built from four canonical ones.

    The 16 corner masks fall into exactly six classes under rotation — empty, corner (4), edge (4),
    diag (2), inv (4), full — and every class is already closed under reflection, which is the
    research finding that says mirroring buys nothing for transitions."""
    cases = {0: None, 15: ("full", 0)}
    for shape, b in (("corner", 1), ("edge", 3), ("diag", 5), ("inv", 11)):
        c = b
        for r in range(4):
            cases.setdefault(c, (shape, r))
            c = bits_rot(c)
    if len(cases) != 16:
        die(f"the dual-grid case table came out with {len(cases)} entries, not 16")
    return cases


DUAL_CASES = dual_grid_cases()


_MASK_CACHE = {}


def tile_mask(shape, style, variant, rot=0):
    """A baked mask, rotated clockwise `rot` quarter turns. Cached; the preview asks for these a lot."""
    key = (shape, style, variant, rot)
    if key not in _MASK_CACHE:
        m = bake_mask(shape, style, variant)
        for _ in range(rot):
            n = len(m)
            m = [bytearray(m[n - 1 - x][y] for x in range(n)) for y in range(n)]
        _MASK_CACHE[key] = m
    return _MASK_CACHE[key]


def write_masks(setname, styles=None):
    """<set>/masks.png + masks.json — every mask the set's terrains can need.

    An indexed PNG like everything else the game ships: index 1 (black) outside the terrain, index 2
    (white) inside, so `palette check` covers it and the engine reads one byte a pixel."""
    styles = sorted(set(styles or EDGE_STYLES))
    for s in styles:
        if s not in EDGE_STYLES:
            die(f"unknown edge_style '{s}'; use one of {', '.join(sorted(EDGE_STYLES))}")
    cols = len(MASK_SHAPES) * MASK_VARIANTS
    W, H = cols * MASK_PX, len(styles) * MASK_PX
    idx = [bytearray(W) for _ in range(H)]
    slots = []
    for r, style in enumerate(styles):
        for si, shape in enumerate(MASK_SHAPES):
            for v in range(MASK_VARIANTS):
                c = si * MASK_VARIANTS + v
                m = tile_mask(shape, style, v)
                for y in range(MASK_PX):
                    row, src = idx[r * MASK_PX + y], m[y]
                    for x in range(MASK_PX):
                        row[c * MASK_PX + x] = PAL_WHITE if src[x] else PAL_BLACK
                slots.append({"style": style, "shape": shape, "variant": v,
                              "x": c * MASK_PX, "y": r * MASK_PX})
    d = tileset_dir(setname)
    d.mkdir(parents=True, exist_ok=True)
    write_indexed_png(d / TSET_MASKS, W, H, idx, master_palette())
    (d / TSET_MASKS_JSON).write_text(json.dumps({
        "set": setname, "cell": MASK_PX, "width": W, "height": H,
        "styles": styles, "shapes": list(MASK_SHAPES), "variants": MASK_VARIANTS,
        "inside": PAL_WHITE, "outside": PAL_BLACK,
        "note": "1-bit masks, GENERATED — never art. A display tile's four corners are four world "
                "cells; bits 1 NW, 2 NE, 4 SE, 8 SW pick a shape and a clockwise rotation from "
                "'cases'. Rotation is a UV swizzle, so no mask is stored twice. "
                f"variant = hash(i, j, terrain) % {MASK_VARIANTS}. Every boundary crosses a tile "
                "edge at that edge's midpoint, which is what makes any two shapes meet cleanly.",
        "cases": {str(b): (list(c) if c else None) for b, c in sorted(DUAL_CASES.items())},
        "slots": slots,
    }, indent=2) + "\n")
    return d / TSET_MASKS, len(slots)
