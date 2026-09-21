"""A tileset's entries, and how they are packed into sheets.

tileset_entries reads tiles.md in order and is the one reader of it; TsetSheet is the
skyline packer — lowest then leftmost, 20 px gutters, capped at 26 slots and 85% of the
canvas, numbered afterwards in reading order, canvas trimmed to the content. Two pools:
terrain (ground and its fringes, one magenta background) and objects.

Must never do: renumber a sheet that has been drawn. New ids go to a fresh <sheet>_2.
Public: tileset_entries, tset_slot_box, TsetSheet. Imports: field, md, names, paths,
terrain, tset.
"""
import re
from .field import SLOT_BORDER, check_description
from .md import die, h2_sections, kv_lines, squash
from .names import detok
from .paths import ROOT
from .terrain import parse_decal, parse_passmask, parse_terrain
from .tset import ATLAS_COLS, TERRAIN_KINDS, TSET_CANVAS, TSET_DIGIT, TSET_GUTTER_X, TSET_GUTTER_Y, TSET_LAYERS, TSET_MARGIN, TSET_MAX_FILL, TSET_MAX_SLOTS, TSET_SLOT, is_fringe, parse_wh, sheet_guess, tileset_doc



def tileset_entries(name):
    """Ordered {id: entry} from story/field/tilesets/<set>/tiles.md. The engine owns this file."""
    path = tileset_doc(name)
    if not path.exists():
        die(f"{path.relative_to(ROOT.parent)} not found. The engine writes it: one '## <id>' per tile "
            f"or stamp, with '- layer:', '- solid:' and '- desc:'.")
    out, unknown = {}, []
    for ident, body in h2_sections(detok(path.read_text(), unknown)).items():
        if not re.fullmatch(r"[a-z0-9_]+", ident):
            die(f"{path.name}: '## {ident}' is not a usable tile id (lower case, digits, underscores)")
        kv = kv_lines(body)
        desc = kv.get("desc") or next((squash(l) for l in body.splitlines()
                                       if l.strip() and not re.match(r"^\s*- [\w ]+?:", l)), "")
        layer = (kv.get("layer") or "ground").lower()
        if layer == "fringe":                       # a friendlier synonym the engine may use
            layer = "ground"
        if layer not in TSET_LAYERS:
            die(f"{path.name}: '## {ident}' has '- layer: {layer}'; use one of: {', '.join(TSET_LAYERS)}")
        idx, w, h = None, 1, 1
        if "index" in kv:
            m = re.fullmatch(r"(\d+)(?:\s+(\d+\s*[xX]\s*\d+))?", kv["index"].strip())
            if not m:
                die(f"{path.name}: '## {ident}' has '- index: {kv['index']}'; "
                    f"write 'index: 12' or 'index: 12 4x3' for a stamp")
            idx = int(m.group(1))
            if m.group(2):
                w, h = parse_wh(m.group(2), f"{path.name} '{ident}'")
        elif kv.get("size") or kv.get("footprint"):
            w, h = parse_wh(kv.get("size") or kv["footprint"], f"{path.name} '{ident}'")
        frames = int(kv["frames"]) if re.fullmatch(r"\d+", (kv.get("frames") or "").strip()) else 1
        if frames < 1 or frames > ATLAS_COLS:
            die(f"{path.name}: '## {ident}' has '- frames: {kv.get('frames')}'; 1..{ATLAS_COLS}")
        e = {"id": ident, "desc": desc, "layer": layer, "index": idx, "w": w, "h": h,
             "frames": frames, "solid": (kv.get("solid") or "no").strip(),
             "sheet": kv.get("sheet"), "raw": kv}
        # TILES2 (D20): an entry is a terrain (a swatch and a computed edge), a decal (a scattered
        # cut-out) or a plain atlas tile/stamp. Anything with no '- kind:' is a tile, which is what
        # every entry written before D20 is.
        e["kind"] = (kv.get("kind") or ("terrain" if kv.get("terrain", "").lower().startswith("y")
                                        else "decal" if kv.get("decal", "").lower().startswith("y")
                                        else "tile")).strip().lower()
        if e["kind"] not in TERRAIN_KINDS:
            die(f"{path.name}: '## {ident}' has '- kind: {e['kind']}'; "
                f"use one of: {', '.join(TERRAIN_KINDS)}")
        e["pass"] = parse_passmask(kv.get("pass"), ident, path.name)
        e["tag"] = (kv.get("tag") or "").strip().lower() or None
        e["over_rows"] = int(kv["over"]) if re.fullmatch(r"\d+", (kv.get("over") or "").strip()) else 0
        e["flip"] = "h" in (kv.get("flip") or "").lower()
        if e["kind"] == "terrain":
            parse_terrain(ident, kv, e, path.name)
        elif e["kind"] == "decal":
            parse_decal(ident, kv, e, path.name)
        e["opaque"] = e["layer"] == "ground" and not is_fringe(ident)
        e["cells"] = (w * frames, h)                 # what it occupies in the atlas and on the sheet
        e["foot"] = (w, h)                           # what it occupies on a map: frames are alternatives
        e["sheet"] = sheet_guess(ident, e)
        if not desc:
            die(f"{path.name}: '## {ident}' has no '- desc:' line; the art pipeline has nothing to ask for")
        check_description(desc, ident, str(path.relative_to(ROOT.parent)))
        out[ident] = e
    if not out:
        die(f"{path.name}: no '## <id>' entries")
    if unknown:
        die(f"{path.name}: unknown name token(s) {', '.join('{{%s}}' % t for t in unknown)}")
    return out


# ── laying the sheets out ──
#
# Two jobs here: which entries share a canvas, and where on that canvas each one goes. The first cut
# of this did one sheet per category and one shelf-row at a time, and left most of nine canvases
# empty — twelve ground tiles had a whole 1536x1024 to themselves, the guild hall used a quarter of
# another. The owner's question ("can't we generate more tiles per image?") is answered here.
#
# GROUPING. A category is rarely a canvas's worth on its own, so they are merged into two pools:
# `terrain` (ground plus its fringes — they are the same materials and must share one palette) and
# `objects` (buildings, nature and props together). A `- sheet:` line in tiles.md still wins outright.
#
# PLACEMENT. A skyline packer, biggest first: each slot is dropped at the lowest, then leftmost,
# place it fits. That is what stands three 1x1 props up the side of an 8x6 guild hall instead of
# opening a ninth sheet for them. A sheet stops at TSET_MAX_SLOTS slots or TSET_MAX_FILL of the
# canvas, whichever comes first; slots are numbered afterwards, in reading order.

def tset_slot_box(cells):
    """(w, h) of the white rectangle for an entry of (cw, ch) cells: the art area plus its border."""
    return (cells[0] * TSET_SLOT + 2 * SLOT_BORDER, cells[1] * TSET_SLOT + 2 * SLOT_BORDER)


class TsetSheet:
    """One canvas being filled. `place` drops a slot at the lowest then leftmost spot it fits."""

    def __init__(self, W=None, H=None):
        W = W or TSET_CANVAS[0]
        H = H or TSET_CANVAS[1]
        scale, gap = TSET_DIGIT
        self.top = TSET_MARGIN + 5 * scale + gap      # the first row's numbers live above it
        self.avail_w = W - 2 * TSET_MARGIN
        self.avail_h = H - TSET_MARGIN - self.top
        self.sky = [0] * self.avail_w                 # the used height at each x, from `self.top`
        self.boxes, self.area, self.canvas = [], 0, W * H

    def _spots(self):
        """Every x where the skyline steps — the only x worth trying."""
        return [0] + [x for x in range(1, self.avail_w) if self.sky[x] != self.sky[x - 1]]

    def place(self, cells, commit=True):
        """The box this entry would take, or None if it will not fit or would break a cap."""
        bw, bh = tset_slot_box(cells)
        if bw > self.avail_w or bh > self.avail_h:
            return None
        if commit and (len(self.boxes) >= TSET_MAX_SLOTS
                       or (self.area + bw * bh) / self.canvas > TSET_MAX_FILL):
            return None
        pw = bw + TSET_GUTTER_X                       # the gutter is carried on the right and below
        best = None
        for x in self._spots():
            if x + bw > self.avail_w:
                break
            y = max(self.sky[x:x + pw])
            if y + bh <= self.avail_h and (best is None or (y, x) < best):
                best = (y, x)
        if best is None:
            return None
        y, x = best
        box = (TSET_MARGIN + x, self.top + y, bw, bh)
        if commit:
            hi = y + bh + TSET_GUTTER_Y
            for xx in range(x, min(self.avail_w, x + pw)):
                if self.sky[xx] < hi:
                    self.sky[xx] = hi
            self.area += bw * bh
            self.boxes.append(box)
        return box

    def fill(self):
        return self.area / self.canvas

    def used(self):
        """The height the content actually needs, margin included."""
        return max((y + h for _, y, _, h in self.boxes), default=self.top) + TSET_MARGIN
