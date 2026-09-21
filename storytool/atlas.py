"""The tileset atlas: load it, place a cut cell in it, write atlas.json — and cut_tileset.

Each entry is packed at its own index without disturbing a cell that belongs to anything
else. The cut keeps the art at the scale it was drawn: a slot is localised, resampled to
exactly cells x 128, and that is the atlas cell. Keying is decided per ENTRY, never per
sheet, because a terrain sheet holds opaque ground and keyed fringes over the same
magenta.

Must never do: mode-snap the art down (--snap32 writes that reading beside the atlas for
comparison; it is not shipped), or disturb another entry's cell.
Public: normalise_families, median_cut, apply_palette, load_atlas, place_in_atlas,
write_atlas_json, exact_inner, fill_ground_magenta, snap32, cut_tileset, write_cut_sheet.
Imports: cutter, field, keying, md, oklab, palette, paths, png, seamless, tileset, tset.
"""
import json
import sys
from .cutter import scrub_border
from .field import ATLAS_CELL, EXTRUDE_PX, KEY_HARD, MAGENTA
from .keying import LOUD, key_magenta, magenta_cast
from .md import die
from .oklab import match_tone, oklab_means
from .palette import PalIndex, master_palette, ship_png
from .paths import ROOT
from .png import crop, read_png, resample, to_rgba
from .seamless import border_gradient, make_seamless, seam_error
from .tileset import tileset_entries
from .tset import ATLAS_COLS, ATLAS_JSON, GROUND_FAMILIES, TSET_TILE, atlas_path, cut_sheet_path, decal_px, tileset_dir



def normalise_families(cut):
    """Pull every ground variant on this sheet onto the tone of the first tile of its family."""
    by_id = {s["id"]: (px, w, h) for px, w, h, s in cut if s["opaque"] and tuple(s["cells"]) == (1, 1)}
    notes = []
    for fam in GROUND_FAMILIES:
        head = next((n for n in fam if n in by_id), None)
        if head is None:
            continue
        want = oklab_means(*by_id[head])
        notes.append(f"  tone {head}: L {want[0]:.3f} chroma {want[1]:.3f} (the family's reference)")
        for name in fam:
            if name == head or name not in by_id:
                continue
            wasL, wasC, nowL, nowC = match_tone(*by_id[name], want[0], want[1])
            notes.append(f"  tone {name}: L {wasL:.3f} -> {nowL:.3f}, "
                         f"chroma {wasC:.3f} -> {nowC:.3f}")
    return notes


def median_cut(colours, n):
    """A palette of at most n colours from [((r,g,b), count)]. Plain median cut, stdlib only."""
    spread = lambda box: max(max(c[0][i] for c in box) - min(c[0][i] for c in box) for i in range(3))
    boxes = [list(colours)]
    while len(boxes) < n:
        can = [b for b in boxes if len(b) > 1 and spread(b) > 0]
        if not can:
            break
        big = max(can, key=lambda b: spread(b) * sum(c[1] for c in b))
        i = max(range(3), key=lambda c: max(x[0][c] for x in big) - min(x[0][c] for x in big))
        big.sort(key=lambda x: x[0][i])
        half, run, target = 1, 0, sum(c[1] for c in big) / 2
        for k, c in enumerate(big):
            run += c[1]
            if run >= target:
                half = max(1, min(len(big) - 1, k))
                break
        boxes.remove(big)
        boxes += [big[:half], big[half:]]
    out = []
    for b in boxes:
        tot = sum(c[1] for c in b) or 1
        out.append(tuple(sum(c[0][i] * c[1] for c in b) // tot for i in range(3)))
    return out


def apply_palette(tiles, n):
    """Quantise every cut tile on one sheet to a shared palette of n colours."""
    counts = {}
    for px, w, h, _ in tiles:
        for y in range(h):
            for x in range(w):
                if px[y][x * 4 + 3]:
                    c = tuple(px[y][x * 4:x * 4 + 3])
                    counts[c] = counts.get(c, 0) + 1
    if len(counts) <= n:
        return len(counts), len(counts)
    pal = median_cut([(c, k) for c, k in counts.items()], n)
    cache = {}
    for px, w, h, _ in tiles:
        for y in range(h):
            for x in range(w):
                if not px[y][x * 4 + 3]:
                    continue
                c = tuple(px[y][x * 4:x * 4 + 3])
                if c not in cache:
                    cache[c] = min(pal, key=lambda p: sum((a - b) ** 2 for a, b in zip(p, c)))
                px[y][x * 4:x * 4 + 3] = bytes(cache[c])
    return len(counts), len(pal)


def load_atlas(setname, need_rows, cell=None):
    """The atlas as RGBA rows, grown to need_rows cells tall. Existing cells are never touched."""
    cell = cell or ATLAS_CELL
    p = atlas_path(setname) if cell == ATLAS_CELL else atlas_path(setname).with_name("atlas32.png")
    W = ATLAS_COLS * cell
    rows = []
    if p.exists():
        w, h, ch, px = read_png(p)
        if w != W:
            die(f"{p.relative_to(ROOT.parent)} is {w}px wide, expected {W} "
                f"({ATLAS_COLS} columns of {cell}). Delete it and recut every sheet.")
        rows = to_rgba(px, w, h, ch)
    while len(rows) < need_rows * cell:
        rows.append(bytearray(W * 4))
    return rows


def place_in_atlas(atlas, index, px, cw, chh, cell=None):
    """Write one entry's cells into its own place in the atlas, and nobody else's."""
    cell = cell or ATLAS_CELL
    for r in range(chh):
        for c in range(cw):
            n = index + ATLAS_COLS * r + c
            ax, ay = (n % ATLAS_COLS) * cell, (n // ATLAS_COLS) * cell
            for y in range(cell):
                sy = r * cell + y
                atlas[ay + y][ax * 4:(ax + cell) * 4] = \
                    px[sy][c * cell * 4:(c + 1) * cell * 4]


def write_atlas_json(setname, entries):
    d = tileset_dir(setname)
    d.mkdir(parents=True, exist_ok=True)
    top = max((e["index"] or 0) + ATLAS_COLS * (e["cells"][1] - 1) + e["cells"][0] - 1
              for e in entries.values())
    (d / ATLAS_JSON).write_text(json.dumps({
        "set": setname, "tile": TSET_TILE, "cell": ATLAS_CELL, "cols": ATLAS_COLS,
        "rows": top // ATLAS_COLS + 1,
        "note": "generated by story_prompt.py tileset; tiles.md is the source of truth",
        "tiles": {i: {"index": e["index"], "w": e["w"], "h": e["h"], "frames": e["frames"],
                      "layer": e["layer"], "solid": e["solid"], "sheet": e["sheet"],
                      "kind": e["kind"],
                      **({"pass": e["pass"]} if e.get("pass", "NESW") != "NESW" else {}),
                      **({"tag": e["tag"]} if e.get("tag") else {}),
                      **({"px": decal_px(e), "on": e["on"], "density": e["density"],
                          "cluster": e["cluster"], "edge_bias": e["edge_bias"],
                          "sizes": e["sizes"], "flip": e["flip"]}
                         if e["kind"] == "decal" else {})}
                  for i, e in entries.items() if e["index"] is not None},
        "terrains": {i: {"priority": e["priority"], "edge_style": e["edge_style"],
                         "swatch": list(e["swatch"]), "border": list(e["border"]) if e["border"] else None,
                         "drift": e["drift"], "cycle": e["cycle"], "solid": e["solid"]}
                     for i, e in entries.items() if e["kind"] == "terrain"},
    }, indent=2) + "\n")


def exact_inner(slot, sx, sy, w, h):
    """A tile slot's art area in the returned image, to the pixel — no padding shaved off.

    The other template kinds shave CUT_PAD off each side to lose a soft border. A tile cannot afford
    that: the art area is exactly TSET_SCALE times the tile, and shaving two pixels would put every
    4x4 art block half a pixel out of step with the grid the cut reads it back on. The border is
    drawn outside the art area instead, so there is nothing to shave."""
    if slot.get("_art"):                                 # localised against the border really drawn
        x0, y0, x1, y1 = slot["_art"]
        return x0, y0, x1 - x0, y1 - y0
    ix, iy, iw, ih = slot["inner"]
    x0, y0 = int(round(ix * sx)), int(round(iy * sy))
    x1, y1 = int(round((ix + iw) * sx)), int(round((iy + ih) * sy))
    if x0 < 0 or y0 < 0 or x1 > w or y1 > h or x1 - x0 < 8 or y1 - y0 < 8:
        die(f"slot {slot['n']} falls outside the image; is this the right file for this package?")
    return x0, y0, x1 - x0, y1 - y0


def fill_ground_magenta(px, w, h):
    """An opaque tile on a magenta sheet: any background the artist left showing is a hole. Fill it.

    A sheet has one background colour, and the ground tiles now share theirs with the fringes, so a
    corner left unpainted would go into the atlas as a magenta square. Nothing is keyed on a ground
    tile — instead every pixel still carrying a magenta cast takes the colour of the nearest painted
    pixel, growing in from the clean part. Returns (repaired, repaired away from the tile's edge):
    a hole in the outermost ring is what a non-integer return does to a slot's border and is repaired
    without a word, while one further in is the artist leaving the background showing."""
    # Only the background colour itself counts, and then a blend that is more than half background:
    # a ground tile may legitimately be any colour it likes, purples included, and a cast threshold
    # as loose as the keying's would call a heather-coloured moor a hole.
    def hole(x, y):
        r, g, b = px[y][x * 4], px[y][x * 4 + 1], px[y][x * 4 + 2]
        return ((r - 255) ** 2 + g * g + (b - 255) ** 2) ** 0.5 <= KEY_HARD

    def blend(x, y):
        return magenta_cast(px[y][x * 4], px[y][x * 4 + 1], px[y][x * 4 + 2]) >= 128

    dirty = [[hole(x, y) for x in range(w)] for y in range(h)]
    for y in range(h):                                   # the soft edge of a hole, but only there
        for x in range(w):
            if not dirty[y][x] and blend(x, y) and any(
                    0 <= nx < w and 0 <= ny < h and dirty[ny][nx]
                    for nx, ny in ((x + 1, y), (x - 1, y), (x, y + 1), (x, y - 1))):
                dirty[y][x] = True
    n = sum(r.count(True) for r in dirty)
    inner = sum(1 for y in range(1, h - 1) for x in range(1, w - 1) if dirty[y][x])
    if not n or n == w * h:
        return n, inner
    front = [(x, y) for y in range(h) for x in range(w) if not dirty[y][x]]
    while front:
        nxt = []
        for x, y in front:
            for nx, ny in ((x + 1, y), (x - 1, y), (x, y + 1), (x, y - 1)):
                if 0 <= nx < w and 0 <= ny < h and dirty[ny][nx]:
                    dirty[ny][nx] = False
                    px[ny][nx * 4:nx * 4 + 3] = px[y][x * 4:x * 4 + 3]
                    nxt.append((nx, ny))
        front = nxt
    return n, inner


def snap32(px, w, h):
    """The old 32-px tile: the mode colour of each 4x4 art block of the full-resolution one.

    Kept behind --snap32 so the pixel-art reading of a sheet can still be produced and compared; the
    atlas the game ships is the full-resolution one."""
    k = ATLAS_CELL // TSET_TILE
    ow, oh = w // k, h // k
    out = []
    for y in range(oh):
        dst = bytearray(ow * 4)
        for x in range(ow):
            counts, best, bn = {}, None, 0
            for yy in range(y * k, y * k + k):
                for xx in range(x * k, x * k + k):
                    q = bytes(px[yy][xx * 4:xx * 4 + 4])
                    n = counts.get(q, 0) + 1
                    counts[q] = n
                    if n > bn:
                        best, bn = q, n
            dst[x * 4:x * 4 + 4] = best
        out.append(dst)
    return out


def cut_tileset(data, slots, rows, w, h, ch, sx, sy, palette=0, heal=True, snap=False):
    """A returned tile sheet: localise, resample onto the atlas grid, key, pack into the atlas.

    Nothing is thrown away here. The sheet is drawn at TSET_SCALE and the atlas keeps it at that
    scale, so a slot's art is resampled to exactly its cells x ATLAS_CELL and no further: the owner
    compared the returns with the old mode-snapped 32-px cut and preferred the art as drawn. The
    keying therefore leaves alpha SOFT (the engine filters linearly), un-mattes the edge colour so no
    pink shows through it, and the silhouette is extruded outward so linear sampling has clean colour
    to pull from.

    A sheet carries one background colour but its entries are not all of one kind: a terrain sheet
    holds opaque ground tiles AND keyed fringes over the same magenta. So the keying is decided per
    ENTRY, from its own `opaque` flag, never from the sheet."""
    setname = data["set"]
    entries = tileset_entries(setname)
    keyed_bg = list(data.get("background") or MAGENTA) == list(MAGENTA)
    ring = max(1, EXTRUDE_PX * 2)                        # the border residue at ATLAS_CELL, in pixels
    cut, lines, scrubbed = [], [], 0
    for s in slots:
        x, y, bw, bh = exact_inner(s, sx, sy, w, h)
        cw, chh = s["cells"]
        tw, th = cw * ATLAS_CELL, chh * ATLAS_CELL
        art = resample(crop(rows, ch, x, y, bw, bh), bw, bh, ch, tw, th)
        if s["opaque"]:
            px = to_rgba(art, tw, th, ch)
            for r in px:
                for i in range(3, len(r), 4):
                    r[i] = 255
            scrubbed += scrub_border(px, tw, th, 4, False, ring)
            if keyed_bg:                               # ground on a magenta sheet: never keyed
                holes, inner = fill_ground_magenta(px, tw, th)
                if inner > tw:                         # a ring's worth is the border, not a hole
                    LOUD.append(f"{s['id']} is an opaque ground tile but came back with {holes} "
                                f"background-coloured pixel(s) in it ({100 * holes // (tw * th)}%); "
                                f"they were filled from the nearest painted pixel. Ask for that slot "
                                f"again, painted edge to edge, if it shows.")
                    print(f"WARNING: {LOUD[-1]}", file=sys.stderr)
        else:
            px = key_magenta(art, tw, th, ch)
            scrubbed += scrub_border(px, tw, th, 4, True, ring)
            # No edge extrusion any more (D19): alpha is hard and the atlas holds indices, so there
            # is no colour under a transparent texel to bleed into. What the bleed was for — linear
            # filtering pulling background through the silhouette — the tile shader now does by
            # blending the four neighbours' colormap COLOURS with premultiplied alpha, index 0 being
            # zero. The cells still have to come out clean, which is what the scrub above is for.
        cut.append((px, tw, th, s))
    if scrubbed:
        print(f"  border scrub: {scrubbed} white edge-run pixel(s) removed")
    for n in normalise_families(cut):
        print(n)
    if palette:
        was, now = apply_palette(cut, palette)
        print(f"  palette: {was} colours -> {now}")

    atlas_rows = max((s["index"] + ATLAS_COLS * (s["cells"][1] - 1)) // ATLAS_COLS + 1 for s in slots)
    atlas = load_atlas(setname, atlas_rows)
    small = load_atlas(setname, atlas_rows, ATLAS_CELL) if snap else None
    for px, tw, th, s in cut:
        cw, chh = s["cells"]
        note = ""
        if s["opaque"] and cw == chh == 1:
            if heal:
                make_seamless(px, tw, th)
            hz, vt = seam_error(px, tw, th)
            (cx, ix), (cy, iy) = border_gradient(px, tw, th)
            rx, ry = (cx / ix if ix else 0.0), (cy / iy if iy else 0.0)
            note = (f"  wrap h{hz:.0f}/v{vt:.0f}  border/interior gradient "
                    f"h{rx:.2f} v{ry:.2f}")
            if max(rx, ry) > 1.15:                       # a real discontinuity is left on some edge
                LOUD.append(f"{s['id']} does not tile cleanly: the gradient across its border is "
                            f"{max(rx, ry):.2f}x its own interior gradient — a line. Recut without "
                            f"--no-heal, or ask for that slot again.")
                print(f"WARNING: {LOUD[-1]}", file=sys.stderr)
                note += "  <-- LINE"
            elif min(rx, ry) < 0.08:                     # nothing real is that flat: it was averaged
                LOUD.append(f"{s['id']}'s border is far flatter than its interior "
                            f"(h{rx:.2f} v{ry:.2f}) — a soft band, not a join.")
                print(f"WARNING: {LOUD[-1]}", file=sys.stderr)
                note += "  <-- BAND"
        elif not s["opaque"]:
            clear = sum(1 for r in px for i in range(3, len(r), 4) if not r[i])
            note = f"  ({100 * clear // (tw * th)}% transparent)"
            if not clear:
                LOUD.append(f"{s['id']} came out with no transparent pixel anywhere; the magenta was "
                            f"painted over, so the game will draw a solid rectangle.")
                print(f"WARNING: {LOUD[-1]}", file=sys.stderr)
        place_in_atlas(atlas, s["index"], px, cw, chh)
        if small is not None:
            place_in_atlas(small, s["index"], snap32(px, tw, th), cw, chh, TSET_TILE)
        lines.append(f"  slot {s['n']} {s['id']}: index {s['index']}, {cw}x{chh} cell(s) "
                     f"at {tw}x{th}{note}")

    ap = atlas_path(setname)
    ap.parent.mkdir(parents=True, exist_ok=True)
    pal, pidx = master_palette(), PalIndex(master_palette())
    ship_png(ap, ATLAS_COLS * ATLAS_CELL, len(atlas), 4, atlas, pal, pidx)
    strip = write_cut_sheet(setname, data["sheet"], cut)
    for l in lines:
        print(l)
    print(f"  -> {ap.relative_to(ROOT.parent)}  {ATLAS_COLS * ATLAS_CELL}x{len(atlas)} "
          f"({len(slots)} entries placed), {strip.relative_to(ROOT.parent)}")
    if small is not None:
        sp = ap.with_name("atlas32.png")
        ship_png(sp, ATLAS_COLS * TSET_TILE, len(small), 4, small, pal, pidx)
        print(f"  -> {sp.relative_to(ROOT.parent)}  {ATLAS_COLS * TSET_TILE}x{len(small)} (--snap32)")
    write_atlas_json(setname, entries)


def write_cut_sheet(setname, sheet, cut):
    """Every tile this sheet produced, laid out at 1:1 — the receipt the package's status reads."""
    MAXW = ATLAS_COLS * ATLAS_CELL
    lanes, x, lane = [[]], 0, 0
    for px, tw, th, s in cut:
        if x and x + tw > MAXW:
            lanes.append([])
            lane, x = lane + 1, 0
        lanes[lane].append((px, x, tw, th))
        x += tw
    W = max((t[1] + t[2] for L in lanes for t in L), default=ATLAS_CELL)
    out = []
    for L in lanes:
        if not L:
            continue
        lh = max(t[3] for t in L)
        band = [bytearray(W * 4) for _ in range(lh)]
        for px, lx, tw, th in L:
            for r in range(th):
                band[r][lx * 4:(lx + tw) * 4] = px[r]
        out += band
    p = cut_sheet_path(setname, sheet)
    p.parent.mkdir(parents=True, exist_ok=True)
    ship_png(p, W, len(out), 4, out)                     # the receipt ships too, so it is checked too
    return p
