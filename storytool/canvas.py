"""Drawing a template sheet: rectangles, borders, slot numbers, and packing.

new_canvas/fill_rect/stroke_rect/draw_digits are the whole drawing library — there is no
image library here. layout_cells packs slot sizes into staggered rows at a scale that
fits, and the digits go in the gutter beside each slot, never inside it, because the cut
takes the pixels inside the border.

Must never do: know what a slot holds.
Public: new_canvas, fill_rect, stroke_rect, draw_digits, draw_template, pack_cells,
try_layout_cells, layout_cells, inner_box. Imports: field, md.
"""
from .field import DIGIT_FONT, DIGIT_GAP, DIGIT_SCALE, SLOT_BORDER, SLOT_GUTTER, SLOT_MARGIN, SLOT_MIN, TEMPLATE_CANVASES, WHITE
from .md import die



# ── drawing the template ──

def new_canvas(w, h, rgb):
    return [bytearray(bytes(rgb) * w) for _ in range(h)]


def fill_rect(rows, x, y, w, h, rgb):
    W = len(rows[0]) // 3
    x0, y0, x1, y1 = max(0, x), max(0, y), min(W, x + w), min(len(rows), y + h)
    if x1 <= x0 or y1 <= y0:
        return
    span = bytes(rgb) * (x1 - x0)
    for yy in range(y0, y1):
        rows[yy][x0 * 3:x1 * 3] = span


def stroke_rect(rows, x, y, w, h, t, rgb):
    fill_rect(rows, x, y, w, t, rgb)
    fill_rect(rows, x, y + h - t, w, t, rgb)
    fill_rect(rows, x, y + t, t, h - 2 * t, rgb)
    fill_rect(rows, x + w - t, y + t, t, h - 2 * t, rgb)


def draw_digits(rows, text, x, y, scale, rgb):
    """The slot number, 3x5 pixels a digit, scaled. Returns the width drawn."""
    cx = x
    for chsym in text:
        glyph = DIGIT_FONT.get(chsym)
        if glyph:
            for gy, line in enumerate(glyph):
                for gx, on in enumerate(line):
                    if on == "1":
                        fill_rect(rows, cx + gx * scale, y + gy * scale, scale, scale, rgb)
        cx += 4 * scale
    return cx - x


def draw_template(W, H, bg, slots, digit=None, fill=None):
    """The template: a white border round each slot and its number in the gutter above it.

    `digit` is (scale, gap); the tile sheets pass a smaller one because their gutters are tight.
    `fill` paints the inside of every slot a different colour from the canvas — the expression sheet
    wants flat mid-grey behind each head while the margins stay dark, so the margin check that tells
    a file from the wrong package apart still has a background of its own to test."""
    scale, gap = digit or (DIGIT_SCALE, DIGIT_GAP)
    rows = new_canvas(W, H, bg)
    for s in slots:
        x, y, w, h = s["box"]
        if fill:
            fill_rect(rows, x, y, w, h, fill)
        stroke_rect(rows, x, y, w, h, SLOT_BORDER, WHITE)
        draw_digits(rows, str(s["n"]), x, y - gap - 5 * scale, scale, WHITE)
    return rows


# ── slot layout ──

def pack_cells(sizes, W, H, u, margin=SLOT_MARGIN, gutter=SLOT_GUTTER):
    """Boxes for slots of (w, h) cells at u pixels per cell, packed into rows. None if it overflows.

    Slots in a row are bottom-aligned, so props of different heights stand on one ground line."""
    avail = W - 2 * margin
    rows, cur, used = [], [], 0.0
    for i, (cw, chh) in enumerate(sizes):
        w = cw * u
        if w > avail:
            return None
        if cur and used + gutter + w > avail:
            rows.append(cur)
            cur, used = [], 0.0
        used += (gutter if cur else 0) + w
        cur.append(i)
    rows.append(cur)
    out, y = [None] * len(sizes), float(margin)
    for row in rows:
        rh = max(sizes[i][1] * u for i in row)
        x = float(margin)
        for i in row:
            w, h = sizes[i][0] * u, sizes[i][1] * u
            out[i] = (int(x), int(y + rh - h), int(w), int(h))
            x += w + gutter
        y += rh + gutter
    return out if y - gutter <= H - margin else None


def try_layout_cells(sizes):
    """Pick the canvas and cell size that make the slots biggest. ((W, H, label, u, boxes), "") or (None, why)."""
    best = None
    for W, H, label in TEMPLATE_CANVASES:
        lo, hi = 1.0, float(max(W, H))
        for _ in range(40):
            mid = (lo + hi) / 2
            if pack_cells(sizes, W, H, mid):
                lo = mid
            else:
                hi = mid
        boxes = pack_cells(sizes, W, H, lo)
        if boxes:
            dy = (H - max(b[1] + b[3] for b in boxes) - SLOT_MARGIN) // 2      # sit the block in the canvas
            boxes = [(x, y + dy, w, h) for x, y, w, h in boxes]
        if boxes and (best is None or lo > best[3]):
            best = (W, H, label, lo, boxes)
    if best is None:
        return None, f"{len(sizes)} slots do not fit on any template canvas"
    W, H, label, u, boxes = best
    small = min(min(b[2], b[3]) for b in boxes) - 2 * SLOT_BORDER
    if small < SLOT_MIN:
        return None, (f"{len(sizes)} slots leave the smallest one {small}px across, under {SLOT_MIN}px: "
                      f"ChatGPT has nothing to draw with")
    return best, ""


def layout_cells(sizes, ids):
    """As try_layout_cells, but a sheet that cannot be drawn is the end of the run."""
    best, why = try_layout_cells(sizes)
    if best is None:
        die(f"{why}. Split them across two packages: {', '.join(ids)}")
    return best


def inner_box(box):
    x, y, w, h = box
    t = SLOT_BORDER
    return (x + t, y + t, w - 2 * t, h - 2 * t)
