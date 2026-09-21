"""The field-art data files and the sizes they are drawn at.

story/field/walkers.md and sprites.md, one `## <id>` an entry; the cell size, the walk
sheet's rows and columns, the keying thresholds and the slot geometry every template
sheet is built from. The numbers here are the contract with src/FIELD_NOTES.md — the
engine reads the files these produce, so a size changed here is an engine change.

Must never do: draw anything (canvas.py) or cut anything (the cut_* modules).
Public: field_entries, field_look, check_description, sprite_art, plus the WALK_*, KEY_*,
SLOT_*, CELL_PX and template constants. Imports: md, names, paths, rules.
"""
import re
from .md import die, h2_sections, kv_lines, slug, squash
from .names import NAMES_FILE, detok
from .paths import FIELD_DIRS, ROOT
from .rules import LOOK_BANNED, STYLE_WORDS, TEXT_WORDS



V3_STYLE, SMELLS = ROOT / "v3" / "STYLE.md", ROOT / "v3" / "SMELLS.md"   # the BAN: lists


TEXT_MAX_SENTENCES = 2                # it is a box on a phone that a thumb dismisses


TODO_RE = re.compile(r"^\[[\w.]+:\s*TODO\]$")   # a placeholder the writer has not filled yet


FIELD_NAME_MAX = 24                   # '- name:' is drawn over the box, like a speaker name


FIELD_KINDS = ("walker", "sprites")   # the two template commands the voxel world needs


KIND_DIR = {"walker": "walker", "sprites": "sprite"}


# Billboards (D22/D23). A still has no sidecar; a sprite with `- frames: N` writes <id>.json beside
# the strip. The engine reads `frame` from there rather than dividing the sheet.
SPRITE_MAX_FRAMES = 3


SPRITE_SLOTS_MAX = 8                  # slots on one sheet. ChatGPT returns about 1.5 MP whatever it


                                      # is asked for, so eighteen slots would give a creature 126 px
                                      # to be drawn in and every one of them would come back mush.
SPRITE_DEFAULT_FPS = 3


SPRITE_LOOPS = ("pingpong", "loop")


CELL_PX = 64                          # a map cell is 64 internal pixels (WORLD.md)


# Buildings are map geometry with three textures on them, not sprites. The sizes and the mapping are
# the engine's contract (src/FIELD_NOTES.md, "Face-texture contract"); change them there first.
BUILDING_FACES = ("front", "side", "roof")


FACE_PX = {"front": (128, 128), "side": (128, 128), "roof": (64, 64)}


ATLAS_CELL = 128                      # ...but the atlas keeps the art at the 4x the sheet was drawn


                                      # at. The owner compared the sheets with the cut and preferred
                                      # the full-resolution art: nothing is thrown away here any
                                      # more. --snap32 still writes the old 32-px atlas beside it.
EXTRUDE_PX = 2                        # opaque colour bled outward under the transparent edge, so


                                      # linear filtering never pulls black or magenta into a sprite
WALK_W, WALK_H = 32, 48               # one walker frame in LOGICAL px (one tile wide, 1.5 tall)


WALK_OUT_W, WALK_OUT_H = 128, 192     # ...and as stored: 4x logical, which the size study


                                      # (WORLD.md, "Sizes") measured as exactly the phone's
                                      # display size at 1440p. 256x384 was twice that in each axis.
WALK_FOOT = 4                         # the lowest opaque row sits this far above the frame's bottom


# The 9-frame layout (owner, 2026-09-19): rows S, side, N x columns stand, step-A, step-B. The engine
# MIRRORS the side row for the other direction, so 16 frames became 9 and the four duplicate stand
# frames went away. A character whose design is not symmetric (a sword on one hip, an eyepatch) takes
# `- asymmetric: yes` in characters.md and keeps both side rows: 12 frames, 4 rows.
WALK_ROWS = ("S", "side", "N")


WALK_ROWS_ASYM = ("S", "W", "E", "N")


WALK_STEPS = ("stand", "step-A", "step-B")                   # column order


WALK_OLD_STEPS = ("stand", "step-left", "stand", "step-right")


WALK_MIN_SCALE = 4                    # frames drawn at least 4x up so ChatGPT has pixels to work with


WALK_MAX_PER_SHEET = 3                # characters on one walk sheet. The frames would fit six, but


                                      # each needs its reference sheet attached and past three
                                      # attachments ChatGPT starts blending the designs.
MAGENTA, BLACK, WHITE = (255, 0, 255), (0, 0, 0), (255, 255, 255)


KEY_HARD = 56                         # RGB distance to magenta: at or under this the pixel is background


#   Distance to magenta is a bad measure of how much magenta is *in* a pixel. A black outline blended
#   half and half with the background lands at rgb(126,0,128) — 181 away from pure magenta, further
#   than a mid grey is — so any distance threshold either keeps it (a purple outline on the phone,
#   which is what the first real cut showed) or eats real colour. What does measure it is the magenta
#   cast, min(r, b) - g: magenta is the one colour with red and blue at the top and green at the
#   bottom, so for a blend with a roughly neutral foreground the cast is about 255 * the magenta
#   fraction, and 255 - cast is the alpha. That estimate only gets applied near the background, so an
#   object may be any colour it likes in its own interior.
KEY_EDGE_REACH = 3                    # how many pixels in from the background the anti-aliasing reaches


KEY_CAST_MIN = 16                     # a smaller cast than this is the artist's colour, not the background


KEY_CAST_CLEAN = 24                   # cast still left after un-matting: the pixel needs a neighbour's colour


KEY_MIN_ALPHA = 64                    # under this the engine's 0.5 alpha test drops the pixel anyway


KEY_CLEAN_RADIUS = 4                  # how far to look for a clean opaque neighbour to borrow a colour from


SLOT_BORDER = 3                       # white border drawn inside each slot rectangle


SLOT_MARGIN, SLOT_GUTTER = 52, 48     # canvas margin, gap between slots (the numbers live in the gap)


SLOT_MIN = 96                         # an inner slot smaller than this is not worth asking for


CUT_PAD = 2                           # extra pixels shaved off each side when cutting, for soft borders


DIGIT_SCALE, DIGIT_GAP = 6, 8         # 3x5 digits at 6x are 18x30: they survive a regeneration


TEMPLATE_CANVASES = ((1536, 1024, "landscape, 1536x1024"), (1024, 1536, "portrait, 1024x1536"))


DEFAULT_FOOTPRINT = (1, 1)            # an entry with no '- footprint: WxH' line, and a warning


# A 3x5 bitmap font, drawn scaled. Slot numbers only, so digits are all it needs.
DIGIT_FONT = {
    "0": ("111", "101", "101", "101", "111"), "1": ("010", "110", "010", "010", "111"),
    "2": ("111", "001", "111", "100", "111"), "3": ("111", "001", "111", "001", "111"),
    "4": ("101", "101", "111", "001", "001"), "5": ("111", "100", "111", "001", "111"),
    "6": ("111", "100", "111", "101", "111"), "7": ("111", "001", "001", "010", "010"),
    "8": ("111", "101", "111", "101", "111"), "9": ("111", "101", "111", "001", "111"),
}


# ── the description files ──

def field_entries(path, what):
    """{id: {'desc': one line, plus any '- key: value'}} from story/field/<what>.md."""
    if not path.exists():
        die(f"{path.relative_to(ROOT.parent)} not found; it holds one '## <id>' entry per {what}")
    out, unknown = {}, []
    for name, body in h2_sections(detok(path.read_text(), unknown)).items():
        if not re.fullmatch(r"[a-z0-9_]+", name):
            die(f"{path.name}: '## {name}' is not a usable id (lower case, digits and underscores only)")
        desc = next((squash(l) for l in body.splitlines()
                     if l.strip() and not re.match(r"^\s*- [\w ]+?:", l)), "")
        out[name] = {"desc": desc, **kv_lines(body)}
    if not out:
        die(f"{path.name}: no '## <id>' entries")
    if unknown:
        die(f"{path.name}: unknown name token(s) {', '.join('{{%s}}' % t for t in unknown)}. "
            f"Every token needs a row in {NAMES_FILE.relative_to(ROOT.parent)}.")
    return out


def field_look(entry, name, where):
    """A walker's look line, checked the same way characters.md is."""
    look = entry.get("look", "")
    bad = [w for w in LOOK_BANNED if re.search(rf"\b{w}\b", look, flags=re.I)]
    if bad:
        die(f"{where}: {name}'s look contains {bad}. Describe only what is visible (see characters.md, "
            f"'Design direction').")
    return look


def check_description(text, ident, where):
    """Descriptions obey R8 and R9 like a panel does: no style words, nothing written on anything."""
    low = text.lower()
    for w in STYLE_WORDS:
        if re.search(rf"\b{re.escape(w)}\b", low):
            die(f"{where}: '{ident}' contains the style word '{w}'. Style comes from STYLE.md, never from a description.")
    for w in TEXT_WORDS:
        if re.search(rf"\b{re.escape(w)}\b", low):
            die(f"{where}: '{ident}' asks for written words ('{w}'). Image models mangle letters; describe the object instead.")


def sprite_art(ident):
    return FIELD_DIRS["sprite"] / f"{slug(ident)}.png"
