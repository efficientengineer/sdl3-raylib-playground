"""story/palette/colormap.png: lighting and time of day as table rows, not as redrawn art.

Eight tables x 32 light levels — day dusk night lamp lantern flash poison stone — with the
row offsets in colormap.json, which readers must read rather than assume. Column 0 is
transparent in every row. lamp is the point-light table, so a lantern pool stays warm
inside a blue night.

Must never do: tint by rotating hue. Night and lamp ADD a cast in Oklab and take chroma
out; rotating turned the first night table pink.
Public: shade_colour, build_colormap, cmd_palette_colormap, write_cycles,
COLORMAP_TABLES. Imports: oklab, palette, paths, png.
"""
import json
import math
import time
from .oklab import _from_oklab, _lerp_ang, _to_oklab
from .palette import COLORMAP_JSON, COLORMAP_PNG, CYCLES_MD, LEVELS, PALETTE_DIR, PALETTE_VERSION, PAL_SIZE, PAL_TRANSPARENT, PalIndex, master_palette, read_hex
from .paths import ROOT
from .png import write_png



# ── the colormap: light, time of day and effects, as table rows ──

COLORMAP_TABLES = ("day", "dusk", "night", "lamp", "lantern", "flash", "poison", "stone")


SHADE_FLOOR = 0.15                    # a colour that started above this lightness never falls below it


AMBIENT_SHADOW_HUE = 285.0            # where a colour drifts as the ambient darkens. NOT the ramps'


                                      # 295: 295 is violet enough to read as magenta once a whole
                                      # screen is at level 24, which is exactly what the first night
                                      # render looked like — a pink village.
NIGHT_CHROMA = 0.35                   # what a colour keeps of its own chroma at night


NIGHT_TINT = (-0.004, -0.042)         # the moonlight itself, added in Oklab coordinates


LAMP_TINT = (0.005, 0.022)            # firelight, added the same way: red-yellow, not a rotation


def shade_colour(lab, s, table):
    """One palette colour at light level s (1.0 full light, 0.0 darkest), in the named table."""
    L0, a, b = lab
    C0, H0 = math.hypot(a, b), math.atan2(b, a)
    C, H = C0, H0
    L = L0 * s
    if table not in ("night", "lamp", "lantern"):        # those two set their own direction outright
        C *= 0.30 + 0.70 * s
        H = _lerp_ang(H, math.radians(AMBIENT_SHADOW_HUE), 0.22 * (1 - s))
    if table == "night":
        # Moonlight, not a magenta filter, and NOT a hue rotation. Rotating every hue toward blue is
        # what made the first two night tables pink: the short way round from a red roof or an orange
        # jerkin to blue runs straight through magenta, so the warmest things on screen came out the
        # loudest colour on screen. What night actually does is take the colour out and lay a blue
        # over what is left — so chroma drops to a third, each colour keeps its own direction, and
        # one flat blue-violet cast is added in Oklab coordinates. Greens land as dark teal-blue,
        # reds as dark maroon, neutrals as blue, and skin stays a face.
        L = L0 * s * 0.78
        C = C0 * NIGHT_CHROMA
        a, b = C * math.cos(H0) + NIGHT_TINT[0], C * math.sin(H0) + NIGHT_TINT[1]
    elif table in ("lamp", "lantern"):
        # The pool under a lantern or a fire. The engine looks a point light up in this table and
        # mixes it in by how far the light is above ambient, so level 0 has to be full-strength warm
        # light — brighter and warmer than day, never bluer — and the dim end is the edge of the pool.
        # Warm by ADDITION, for the same reason night is blue by addition: rotating a blue roof tile
        # toward orange takes it through magenta and a purple water trough is not firelight.
        L = L0 * (0.12 + 0.88 * s) + 0.05 * s
        C = C0 * 1.10 * (0.45 + 0.55 * s)
        a, b = C * math.cos(H0) + LAMP_TINT[0] * s, C * math.sin(H0) + LAMP_TINT[1] * s
    elif table == "dusk":
        H = _lerp_ang(H, math.radians(55), 0.32 * (1 - s) + 0.12)
        C = C * 1.05 + 0.018 * (1 - s)
        L, a, b = L * 0.95, C * math.cos(H), C * math.sin(H) + 0.010
    elif table == "flash":                              # a hit, a spell, lightning: washed toward white
        f = 0.75 * s
        a, b = C * math.cos(H) * (1 - f), C * math.sin(H) * (1 - f)
        L = L + (1.0 - L) * f
    elif table == "poison":
        H = _lerp_ang(H, math.radians(140), 0.55)
        C = C * 0.75 + 0.030
        a, b = C * math.cos(H), C * math.sin(H)
    elif table == "stone":                              # petrified, or a statue: all the colour out
        a, b = C * math.cos(H) * 0.10, C * math.sin(H) * 0.10
        L = L * 0.92 + 0.04
    else:
        a, b = C * math.cos(H), C * math.sin(H)
    if L0 > SHADE_FLOOR:                                # the floor PALETTE.md asks for: it still reads
        L = max(L, SHADE_FLOOR)
    return (L, a, b)


def build_colormap(pal):
    """[(table, [row of 256 palette indices] per light level)] — row 0 is full light."""
    idx = PalIndex(pal)
    out = []
    for table in COLORMAP_TABLES:
        rows = []
        for lv in range(LEVELS):
            s = 1.0 - 0.95 * (lv / (LEVELS - 1))
            row = bytearray(PAL_SIZE)
            for i, c in enumerate(pal):
                if i == PAL_TRANSPARENT:
                    continue
                L, a, b = shade_colour(_to_oklab(*c), s, table)
                row[i] = idx.of(*_from_oklab(L, a, b))
            rows.append(row)
        out.append((table, rows))
        print(f"  {table}: {LEVELS} light levels")
    return out


def cmd_palette_colormap(args):
    """story/palette/colormap.png — every table at every light level, as colours the engine can use."""
    pal = read_hex(args[0]) if args and not args[0].startswith("--") else master_palette()
    t0 = time.time()
    tables = build_colormap(pal)
    rows, meta = [], []
    for table, trows in tables:
        meta.append({"table": table, "row0": len(rows), "levels": LEVELS,
                     **({"alias_of": "lamp"} if table == "lantern" else {})})
        for r in trows:
            # RGBA, and column 0 is transparent in EVERY row. Index 0 is the transparent index, so a
            # colormap that wrote it as opaque black handed the engine a black texel to blend at
            # every sprite edge; it was patching that on load. There is nothing to patch now.
            line = bytearray()
            for i in range(PAL_SIZE):
                line += bytes(pal[r[i]]) + bytes((0 if i == PAL_TRANSPARENT else 255,))
            rows.append(line)
    PALETTE_DIR.mkdir(parents=True, exist_ok=True)
    write_png(COLORMAP_PNG, PAL_SIZE, len(rows), 4, rows)
    COLORMAP_JSON.write_text(json.dumps({
        "palette": "master.hex", "version": PALETTE_VERSION,
        "width": PAL_SIZE, "height": len(rows), "levels": LEVELS, "channels": "RGBA",
        "note": "row (table.row0 + level) column i = the colour palette index i takes at that light. "
                "Level 0 is full light, level 31 the darkest. Each entry is the RGB of the nearest "
                "MASTER palette index, so the engine may use the colormap directly as colour. "
                "Column 0 is the transparent index: RGBA (0,0,0,0) in every row, so the engine needs "
                "no patch on load. Every other column is opaque.",
        "floor": SHADE_FLOOR,
        "point_light_table": "lamp",
        "tables": meta,
    }, indent=2) + "\n")
    print(f"wrote {COLORMAP_PNG.relative_to(ROOT.parent)}  {PAL_SIZE}x{len(rows)} "
          f"({len(tables)} tables x {LEVELS} levels) and colormap.json ({time.time()-t0:.1f}s)")


def write_cycles(pal, cycles):
    """story/palette/cycles.md, from the blocks `palette build` reserved. Generated, not hand-written."""
    lines = ["# Palette cycles (D19/D20)", "",
             "One line a cycle: `name: index index index… @ fps`. The engine rewrites those columns of",
             "the colormap every frame, rotating the listed indices through each other's colours, so a",
             "river moves and a lamp flickers with no second frame of art.", "",
             "These index ranges are **reserved**: `palette build` lays them down immediately after",
             "black and white, before any material ramp, and nothing else in the palette shares them.",
             "That is the point — an index shared with a roof tile would make the roof flicker too.",
             "Regenerated by `./story_prompt.py palette build`; do not edit by hand.", ""]
    for c in cycles:
        idx = list(range(c["start"], c["start"] + c["len"]))
        lines.append(f"{c['name']}: {' '.join(str(i) for i in idx)} @ {c['fps']}"
                     f"   # {c['what']}")
    lines.append("")
    CYCLES_MD.write_text("\n".join(lines))
    print(f"wrote {CYCLES_MD.relative_to(ROOT.parent)}  "
          + ", ".join(f"{c['name']} {c['start']}-{c['start'] + c['len'] - 1}" for c in cycles))
