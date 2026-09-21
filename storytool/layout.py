"""Where the panels of one shot sheet go.

SHAPE_UNITS gives each shot shape a fixed relative size; they are packed into staggered
rows and scaled to fit the canvas, so a wide panel is always one of the big ones. Two
earlier search-based engines produced huge insets and tiny two-shots — do not go back to
a scoring function.

Must never do: decide a canvas size or write a file.
Public: sheet_geometry, SHAPE_UNITS. Imports: md.
"""
from .md import die



# ───────────────────────── ChatGPT shot sheets ─────────────────────────

# Relative size of each shape on a sheet, as (width, height) in units of u. A wide panel is the yardstick.
SHAPE_UNITS = {"wide": (2.0, 1.0), "tall": (0.8, 1.6), "square": (0.8, 0.8), "slit": (1.8, 0.45)}


def _pack(shapes, W, H, u):
    """Pack panels in order into top-aligned rows at unit size `u`. Returns rects in pixels, or None if it overflows."""
    g, rg = 0.05 * W, 0.06 * W                              # outer margin / row gap, and gap between panels in a row
    rows, cur, width = [], [], 0.0
    for i, sh in enumerate(shapes):
        w = SHAPE_UNITS[sh][0] * u
        if cur and width + rg + w > W - 2 * g:
            rows.append(cur)
            cur, width = [], 0.0
        width += (rg if cur else 0) + w
        cur.append(i)
    rows.append(cur)
    rects, y = [None] * len(shapes), g
    for r, row in enumerate(rows):
        used = sum(SHAPE_UNITS[shapes[i]][0] * u for i in row) + rg * (len(row) - 1)
        if used > W - 2 * g:
            return None
        x = g if r % 2 == 0 else W - g - used                # stagger rows left / right so it never reads as a grid
        for i in row:
            w, h = SHAPE_UNITS[shapes[i]][0] * u, SHAPE_UNITS[shapes[i]][1] * u
            rects[i] = (x, y, w, h)
            x += w + rg
        y += max(SHAPE_UNITS[shapes[i]][1] * u for i in row) + g
    return rects if y - g <= H - g else None


def sheet_geometry(shapes):
    """Lay the panels out on a canvas. Returns (canvas text, [(x, y, w, h)] normalized 0-1).

    Every shape has a fixed relative size (SHAPE_UNITS), so the important wide panels are always the
    big ones and insets stay small. Panels pack into rows in reading order, and the whole sheet is scaled
    up until it just fits. The canvas that lets the panels be largest wins. Up to 4 panels use the
    standard sizes; more use the large ones (2560x1440 is the biggest size OpenAI does not call experimental).
    """
    small = ((1536, 1024, "landscape, 1536x1024"), (1024, 1536, "portrait, 1024x1536"), (1024, 1024, "square, 1024x1024"))
    large = ((2560, 1440, "landscape, 2560x1440"), (1440, 2560, "portrait, 1440x2560"), (2048, 2048, "square, 2048x2048"))
    best = None
    for W, H, label in (small if len(shapes) <= 4 else large):
        lo, hi = 1.0, float(max(W, H))
        for _ in range(40):                                  # largest u that still fits
            mid = (lo + hi) / 2
            if _pack(shapes, W, H, mid): lo = mid
            else: hi = mid
        rects = _pack(shapes, W, H, lo)
        if not rects:
            continue
        fill = sum(w * h for _, _, w, h in rects) / (W * H)
        if best is None or fill > best[0]:
            best = (fill, label, [(x / W, y / H, w / W, h / H) for x, y, w, h in rects])
    if best is None:
        die("could not fit the panels on any canvas")
    return best[1], best[2]
