"""The 256-colour contract (PALETTE.md, D19): read it, match to it, ship an indexed PNG.

Two palettes: story/palette/master.hex for everything that can share the field screen, and
story/panels/<scene>.hex for one cutscene scene. Matching is exact nearest in Oklab, no
dithering ever, alpha hard at 0.5. ship_png is the last step of every cutter.

Must never do: dither, soften the alpha test, or let a shipped file out unindexed.
Public: read_hex, write_hex, master_palette, scene_hex, PalIndex, palettise,
write_indexed_png, ship_png, and the palette paths. Imports: md, oklab, paths, png.
"""
from pathlib import Path
import bisect
import re
import struct
import time
import zlib
from .md import die
from .oklab import _to_oklab
from .paths import PANELS, ROOT
from .png import write_png



# ───────────────────────── Palette (D19): 256 colours, one byte a pixel ─────────────────────────
#
# PALETTE.md is the contract. Everything the game ships is an indexed PNG (colour type 3) drawn from
# one of two palettes: the master, shared by everything that can be on screen together in the field,
# and a per-scene palette for cutscene panels, which are never lit and need their own skies.
#
# Index 0 is transparent, 1 is pure black, 2 is pure white, and the other 253 are material ramps
# (variable length) plus the corpus colours the ramps serve worst. A ramp does not spend slots on
# near-black or near-white — that is what 1 and 2 are for — its shadows rotate toward blue-violet and
# its highlights toward yellow, because a ramp that is a straight line through Oklab goes muddy the
# moment the colormap darkens it.

PALETTE_DIR = ROOT / "palette"


MASTER_HEX = PALETTE_DIR / "master.hex"


MASTER_JSON = PALETTE_DIR / "master.json"


MASTER_SWATCH = PALETTE_DIR / "master_swatch.png"


MASTER_PAL_PNG = PALETTE_DIR / "master.pal.png"


COLORMAP_PNG = PALETTE_DIR / "colormap.png"


COLORMAP_JSON = PALETTE_DIR / "colormap.json"


CYCLES_MD = PALETTE_DIR / "cycles.md"


PALETTE_VERSION = 2           # v2 (2026-09-19): refitted to the owner's flat luminous field style,


                              # and the first version to reserve contiguous ramps for palette cycling
PAL_SIZE = 256


PAL_TRANSPARENT, PAL_BLACK, PAL_WHITE = 0, 1, 2


MERGE_DE = 0.015                      # two palette entries this close in Oklab are one colour


LEVELS = 32                           # light levels a colormap table carries


# The material families. (name, hue anchor in Oklab degrees or None for a neutral, class) — the class
# sets how many slots the family may have, and the corpus's own pixel share distributes the class
# budget inside those bounds. "long" is for the materials a whole map is made of, where a missing
# step reads as a band; "short" is for a hue that turns up on one object.
RAMP_CLASSES = {"long": (16, 24, 104), "med": (10, 12, 50), "short": (6, 8, 24)}


RAMP_FAMILIES = [
    ("roof red / brick",             32,   "long"),
    ("orange / terracotta (hair)",   45,   "med"),
    ("skin",                         57,   "long"),
    ("wood",                         68,   "long"),
    ("earth / dirt",                 78,   "long"),
    ("gold / blond",                 90,   "med"),
    ("olive / dry grass",            108,  "short"),
    ("foliage green (warm)",         135,  "long"),
    ("foliage green (cool)",         158,  "long"),
    ("teal / shallow water",         205,  "short"),
    ("sky / cyan",                   238,  "med"),
    ("water blue / metal",           262,  "med"),
    ("purple / shadow",              300,  "short"),
    ("neutral warm (plaster, cloth)", None, "med"),
    ("neutral cool (stone, steel)",   None, "long"),
]


# Palette cycling needs its indices CONTIGUOUS and its own: the engine rewrites those columns of the
# colormap every frame, so an index shared with a roof tile would make the roof flicker. Reserved
# immediately after black and white, before any ramp, and recorded in master.json and cycles.md.
# (name, family the colours come from, length, fps, lightness window, what it is)
CYCLE_RAMPS = (
    ("water",        "water blue / metal",        8, 6,  (0.34, 0.66),
     "the light moving on running water"),
    ("fire_lamp",    "orange / terracotta (hair)", 8, 10, (0.46, 0.86),
     "a fire, a lantern, a forge: the warm end flickering"),
    ("foliage_wind", "foliage green (warm)",       6, 4,  (0.38, 0.64),
     "leaves turning in the wind, a slow shimmer through a canopy"),
    ("sparkle",      "sky / cyan",                 4, 12, (0.72, 0.95),
     "a glint on water or metal, and anything that has to twinkle"),
)


NEUTRAL_C = 0.030                     # chroma under this is a neutral, not a hue


RAMP_LO, RAMP_HI = 0.16, 0.93         # a ramp's lightness range: black and white are indices 1 and 2


SHADOW_HUE, HILITE_HUE = 295.0, 95.0  # shadows rotate toward blue-violet, highlights toward yellow


# Where shipped art lives. `palette check` walks these; everything in them must be an indexed PNG.
SHIPPED_MASTER = ("field/tilesets", "field/walkers", "field/sprites", "portraits")


# ── the hex file ──

def read_hex(path):
    """[(r, g, b)] from a '# version N' hex file. The version line is the palette's identity."""
    if not Path(path).exists():
        die(f"{Path(path)} not found. Build it with: ./story_prompt.py palette build")
    cols, ver = [], None
    for line in Path(path).read_text().splitlines():
        line = line.strip()
        if line.startswith("#") and not re.fullmatch(r"#[0-9a-fA-F]{6}", line):
            m = re.match(r"#\s*version\s+(\d+)", line)
            if m:
                ver = int(m.group(1))
            continue
        if not line:
            continue
        if not re.fullmatch(r"#[0-9a-fA-F]{6}", line):
            die(f"{Path(path).name}: '{line}' is not a #rrggbb colour")
        cols.append(tuple(int(line[i:i + 2], 16) for i in (1, 3, 5)))
    if ver is None:
        die(f"{Path(path).name} has no '# version N' first line; it is not a palette this tool wrote")
    if len(cols) != PAL_SIZE:
        die(f"{Path(path).name} holds {len(cols)} colours, not {PAL_SIZE}")
    return cols


def write_hex(path, pal, version=PALETTE_VERSION):
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(f"# version {version}\n" + "".join("#%02x%02x%02x\n" % tuple(c) for c in pal))


_MASTER = None


def master_palette():
    global _MASTER
    if _MASTER is None:
        _MASTER = read_hex(MASTER_HEX)
    return _MASTER


def scene_hex(scene):
    return PANELS / f"{scene}.hex"


# ── exact nearest colour in Oklab ──

class PalIndex:
    """Nearest palette index for an sRGB colour, exact, in Oklab.

    Exact and fast enough to be the last step of every cut: the palette is sorted by lightness, and
    |dL| is a lower bound on the distance, so a scan outward from the query's lightness stops as soon
    as the lightness gap alone exceeds the best distance so far. Two dozen candidates out of 253 is
    typical. Every unique colour in an image is looked up once and cached — a 1.5 MP sheet holds a
    few hundred thousand of them, so the cache is what keeps this in seconds rather than minutes."""

    def __init__(self, pal):
        self.pal = list(pal)
        self.labs = [_to_oklab(*c) for c in self.pal]
        self.order = sorted(range(1, len(self.pal)), key=lambda i: self.labs[i][0])
        self.Ls = [self.labs[i][0] for i in self.order]
        self.cache = {}
        self.probes = 0
        self.lookups = 0

    def of(self, r, g, b):
        k = (r << 16) | (g << 8) | b
        got = self.cache.get(k)
        if got is not None:
            return got
        L, A, B = _to_oklab(r, g, b)
        order, Ls, labs, n = self.order, self.Ls, self.labs, len(self.order)
        lo = bisect.bisect_left(Ls, L)
        hi, best, bd = lo, order[lo if lo < n else n - 1], 1e9
        while True:
            moved = False
            if hi < n:
                d = Ls[hi] - L
                if d * d < bd:
                    i = order[hi]
                    q = labs[i]
                    dd = (L - q[0]) ** 2 + (A - q[1]) ** 2 + (B - q[2]) ** 2
                    if dd < bd:
                        bd, best = dd, i
                    hi += 1
                    moved = True
                    self.probes += 1
            if lo > 0:
                d = L - Ls[lo - 1]
                if d * d < bd:
                    lo -= 1
                    i = order[lo]
                    q = labs[i]
                    dd = (L - q[0]) ** 2 + (A - q[1]) ** 2 + (B - q[2]) ** 2
                    if dd < bd:
                        bd, best = dd, i
                    moved = True
                    self.probes += 1
            if not moved:
                break
        self.lookups += 1
        self.cache[k] = best
        return best


def palettise(rows, w, h, ch, pal, idx=None):
    """RGB(A) rows -> one byte a pixel. Alpha is HARD: under 0.5 is index 0, nothing else is.

    Un-matting has already happened in the keying, so an edge pixel that survives carries its own
    colour and takes its own nearest entry. No dithering, anywhere: every dither mode tested made
    this cel-shaded art worse, and an ordered pattern is exactly what a palette-cycled colormap
    turns into a crawling texture."""
    idx = idx or PalIndex(pal)
    out = []
    of = idx.of
    for y in range(h):
        row, o = rows[y], bytearray(w)
        if ch == 4:
            for x in range(w):
                i = x * 4
                if row[i + 3] >= 128:
                    o[x] = of(row[i], row[i + 1], row[i + 2])
        else:
            for x in range(w):
                i = x * 3
                o[x] = of(row[i], row[i + 1], row[i + 2])
        out.append(o)
    return out


def write_indexed_png(path, w, h, index_rows, pal):
    """Colour type 3: PLTE + a tRNS of one byte, so index 0 alone is transparent."""
    def chunk(kind, body):
        return struct.pack(">I", len(body)) + kind + body + struct.pack(">I", zlib.crc32(kind + body))
    plte = b"".join(bytes(c) for c in pal) + b"\x00" * 3 * (PAL_SIZE - len(pal))
    raw = b"".join(b"\x00" + bytes(r) for r in index_rows)
    Path(path).parent.mkdir(parents=True, exist_ok=True)
    Path(path).write_bytes(
        b"\x89PNG\r\n\x1a\n"
        + chunk(b"IHDR", struct.pack(">IIBBBBB", w, h, 8, 3, 0, 0, 0))
        + chunk(b"PLTE", plte) + chunk(b"tRNS", b"\x00")
        + chunk(b"IDAT", zlib.compress(raw, 9)) + chunk(b"IEND", b""))


PALETTE_OFF = False       # a debug switch: write the cut in full colour beside the shipped


                          # file, as <name>.raw.png, so the two can be looked at side by side


def ship_png(path, w, h, ch, rows, pal=None, idx=None, note=True):
    """The last step of every cutter: palettise and write the indexed PNG the game ships."""
    if PALETTE_OFF:
        write_png(Path(path).with_suffix(".raw.png"), w, h, ch, rows)
        return None
    pal = pal or master_palette()
    t0 = time.time()
    index_rows = palettise(rows, w, h, ch, pal, idx)
    write_indexed_png(path, w, h, index_rows, pal)
    if note:
        PAL_TIMING.append((Path(path).name, w * h, time.time() - t0))
    return index_rows


PAL_TIMING = []
