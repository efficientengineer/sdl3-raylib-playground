"""Where a tileset's files are, and the constants its geometry is built from.

story/field/tilesets/<set>/: tiles.md is the engine's list, atlas.png is 16 columns of
128x128 cells, masks and swatches and decals beside it. A slot is exactly 4x the tile it
holds. The mask constants live here too, so terrain parsing and mask baking can both see
them without importing each other.

Must never do: move an index that is already written, rename an entry, or change a size.
Public: tileset_dir, tileset_doc, atlas_path, cut_sheet_path, is_fringe, sheet_guess,
parse_wh, swatch_path, decal_path, decal_px, all_tmaps, and the TSET_/MASK_/ATLAS_
constants. Imports: field, md, paths.
"""
import re
from .field import ATLAS_CELL
from .md import die, slug
from .paths import FIELD



# ───────────────────────── Tile field (D18): tilesets, atlases and tmaps ─────────────────────────
#
# Owner's call, 2026-09-19, final: "Sega style, top down, using tile sheets, grid walking. No more
# screen navmesh." WORLD.md is the contract. The engine owns `story/field/tilesets/<set>/tiles.md`
# (the names, the sizes, the solid rows, the layers); this half owns everything that turns that list
# into art and back into an atlas.
#
#   tileset <set>   read tiles.md, assign any missing atlas index, lay every tile and stamp out on
#                   template sheets at EXACTLY 4x (a 32-px tile is a 128-px slot), write one package
#                   per sheet under story/packages/tilesets/<set>/<sheet>/
#   ingest          cut a returned sheet: snap to the 4x4 art-pixel grid by mode filter, downsample
#                   4:1, key the magenta on anything that is not opaque ground, hard-threshold the
#                   alpha (the engine alpha-tests; there is no partial alpha in a tile), then pack
#                   every tile into story/field/tilesets/<set>/atlas.png at its own index
#   tmap check|preview   validate a .tmap against its tileset, or render it from the atlas
#
# Two things are load-bearing and easy to get wrong:
#
# 1. **Nothing ever moves in the atlas.** An index that tiles.md already carries is never reassigned,
#    and a cut only ever writes the cells its own entries own. That is what lets the engine hard-code
#    indices and lets one sheet be recut years later.
# 2. **The 4x grid.** ChatGPT does not draw on a pixel grid, it draws a picture that looks pixelated.
#    Downsampling that by averaging turns every hard tile edge into mush and the seams stop meeting.
#    So the cut resizes each slot to exactly 4x the tile, then takes the MODE colour of each 4x4
#    block — the colour the artist meant that art pixel to be — and that is the tile.

TILESETS = FIELD / "tilesets"


TMAPS = FIELD / "tmaps"


TSET_TILE = 32                        # a tile is 32x32 map cells' worth of design


TSET_SCALE = 4                        # ...drawn at 4x on the sheet, so one art pixel is a 4x4 block


TSET_SLOT = TSET_TILE * TSET_SCALE    # 128: one tile's slot on a template sheet


ATLAS_COLS = 16                       # the atlas is 16 tiles wide; index 0 is the empty tile


TSET_CANVAS = (1536, 1024, "landscape, 1536x1024")   # one sheet, never bigger


TSET_LAYERS = ("ground", "object", "over")


FRINGE_KINDS = ("edge", "corner_out", "corner_in")   # three fringe tiles a terrain, rotated


ATLAS_JSON = "atlas.json"


# ── ground families: variants of one material, meant to be MIXED on the map ──
#
# A `.tmap` scatters `paving_worn` through `paving` and `grass_tuft` through `grass` so the ground
# does not repeat. That only works if the variants differ in DETAIL. ChatGPT draws each slot on its
# own and gives one a lighter cast than the next, and the map then reads as a checkerboard of pale
# and dark squares — which is exactly what the first Halm capture showed on the square. So every
# variant is pulled to the FIRST member's mean lightness and mean chroma in Oklab (a perceptual
# space: shifting L there does not swing the hue the way scaling RGB does). Detail, texture and
# local contrast are untouched; only the overall tone is matched.
GROUND_FAMILIES = (
    ("grass", "grass_tuft", "grass_flower"),
    ("paving", "paving_worn"),
    ("dirt", "dirt_rut"),
)


# ── how full a sheet is packed ──
#
# A tile slot is fixed at 4x (128 px) and cannot be shrunk to fit, so the only way to ask for fewer
# generations is to put more slots on each canvas. The first cut of this laid one sheet out per
# category, one shelf-row at a time, and left most of nine canvases empty: 12 ground tiles took a
# whole 1536x1024 sheet, the guild hall took another with a quarter of the canvas used. These
# numbers and the skyline packer below fill the canvas instead — big things first, small stamps
# tucked into the space beside and under them.
TSET_MARGIN = 20                      # canvas edge to the first slot border


TSET_GUTTER_X, TSET_GUTTER_Y = 20, 28 # between slot borders; the vertical one holds the slot number


TSET_DIGIT = (4, 4)                   # (scale, gap): a 3x5 digit at 4x is 12x20, inside GUTTER_Y


TSET_MAX_SLOTS = 26                   # more than this on one sheet and the detail per slot suffers


TSET_MAX_FILL = 0.85                  # ...and no more than this much of the canvas is slot area


# Which sheet an entry lands on when it carries no '- sheet:' line. The keyword decides the *family*;
# the packer below then merges families onto shared canvases, because a family rarely fills one.
TSET_SHEET_WORDS = (
    ("buildings", ("house", "roof", "wall", "door", "window", "shop", "inn", "gate", "chimney",
                   "eave", "porch", "hut", "tower", "shrine", "steps", "stair")),
    ("nature", ("tree", "bush", "rock", "stump", "log", "flower", "grass_tuft", "reed", "cliff",
                "boulder", "hedge", "crop", "vine", "water_", "fall")),
)


def tileset_dir(name):
    return TILESETS / slug(name)


def tileset_doc(name):
    return tileset_dir(name) / "tiles.md"


def atlas_path(name):
    return tileset_dir(name) / "atlas.png"


def cut_sheet_path(name, sheet):
    return tileset_dir(name) / "cut" / f"{sheet}.png"


def is_fringe(name):
    return any(name.endswith("_" + k) for k in FRINGE_KINDS)


def sheet_guess(name, entry):
    """The sheet an entry belongs on with no '- sheet:' line of its own."""
    if entry.get("sheet"):
        return slug(entry["sheet"])
    if is_fringe(name):
        return "fringes"
    if entry["layer"] == "ground":
        return "ground"
    for sheet, words in TSET_SHEET_WORDS:
        if any(w in name for w in words):
            return sheet
    return "props"


def parse_wh(text, where):
    m = re.fullmatch(r"(\d+)\s*[xX]\s*(\d+)", (text or "").strip())
    if not m:
        die(f"{where}: '{text}' is not a WxH size")
    w, h = int(m.group(1)), int(m.group(2))
    if not (1 <= w <= ATLAS_COLS and 1 <= h <= 16):
        die(f"{where}: {w}x{h} is not a usable stamp size (1..{ATLAS_COLS} across, 1..16 down)")
    return w, h


# ───────────────────── TILES2 (D20): terrains, masks, swatches, decals ─────────────────────
#
# Terrains, masks and decals (D20). The masks are generated; only the swatch is art.
# The short version: drawn transition tiles are gone. A terrain is ONE seamless swatch sampled in
# world space, and the boundary between two terrains is a 1-bit mask the TOOL computes — never art.
# Every mask boundary crosses a tile edge at that edge's midpoint, perpendicular, which is the whole
# reason any shape meets any other shape in any rotation with no break.

TSET_MASKS = "masks.png"


TSET_MASKS_JSON = "masks.json"


TSET_SWATCHES = "swatches"            # <set>/swatches/<terrain>.png


MASK_PX = ATLAS_CELL                  # a mask is one display tile at the atlas's own cell size


MASK_VARIANTS = 3                     # per (shape, style); hash(i, j, terrain) picks one


MASK_SHAPES = ("corner", "edge", "diag", "inv", "full")


EDGE_STYLES = {                       # amp/freq of two octaves of value noise displacing the boundary
    "ragged": (0.105, 5.5, 0.045, 13.0),      # grass, dirt, crop, mud
    "smooth": (0.022, 3.0, 0.010, 7.0),       # paving, plank, any laid floor
    "bank":   (0.060, 3.5, 0.022, 9.0),       # water: a hard Phantasy Star bank, not a soft shore
}


MASK_WINDOW = 0.16                    # the displacement is windowed to zero this close to the border,


                                      # so the pinned midpoints survive whatever the noise does
SWATCH_MIN, SWATCH_MAX = 2, 4         # tiles a side


TERRAIN_KINDS = ("terrain", "decal", "tile")


def swatch_path(setname, terrain):
    return tileset_dir(setname) / TSET_SWATCHES / f"{terrain}.png"


# ── the decal sheet: ~20 cut-outs of nothing but the object ──


def decal_path(setname, ident):
    """The loose cut-out. It is kept for the preview and for eyeballing; the GAME reads the atlas."""
    return tileset_dir(setname) / "decals" / f"{ident}.png"


def decal_px(e):
    """A decal's size in atlas pixels — `size_tiles` of a cell, at the atlas cell size."""
    n = max(2, int(round(e["tiles"] * ATLAS_CELL)))
    return [min(n, ATLAS_CELL), min(n, ATLAS_CELL)]


def all_tmaps():
    return sorted(p.stem for p in TMAPS.glob("*.tmap")) if TMAPS.exists() else []
