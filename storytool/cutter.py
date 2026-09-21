"""Finding the art inside a returned sheet.

Two jobs. find_panels/match_boxes detect white-bordered panels on a shot sheet by their
borders and match them to the expected panels by position and proportion, not reading
order. localise_slot hunts each side of a template slot's white border within 12% of the
slot's size, innermost candidate wins, and scrub_border removes what is left — edges
only, never the interior, so white headbands and water foam survive.

Must never do: key colour or write a file; that is keying.py and the cut_* modules.
Public: find_panels, match_boxes, localise_slot, localise_slots, scrub_border,
scaled_inner, body_bounds, shift_frame. Imports: field, md, rules.
"""
import sys
from .field import CUT_PAD
from .md import die
from .rules import BLACK_MAX, GRID, MIN_PANEL_AREA



def find_panels(w, h, ch, rows):
    """Bounding boxes (x0, y0, x1, y1) of non-black regions, in reading order."""
    lit = lambda x, y: max(rows[y][x * ch:x * ch + 3]) > BLACK_MAX
    gw, gh = w // GRID, h // GRID
    mask = [[lit(gx * GRID + GRID // 2, gy * GRID + GRID // 2) for gx in range(gw)] for gy in range(gh)]
    seen = [[False] * gw for _ in range(gh)]
    boxes = []
    for gy in range(gh):
        for gx in range(gw):
            if not mask[gy][gx] or seen[gy][gx]:
                continue
            stack, x0, y0, x1, y1 = [(gx, gy)], gx, gy, gx, gy
            seen[gy][gx] = True
            while stack:
                cx, cy = stack.pop()
                x0, y0, x1, y1 = min(x0, cx), min(y0, cy), max(x1, cx), max(y1, cy)
                for nx, ny in ((cx + 1, cy), (cx - 1, cy), (cx, cy + 1), (cx, cy - 1)):
                    if 0 <= nx < gw and 0 <= ny < gh and mask[ny][nx] and not seen[ny][nx]:
                        seen[ny][nx] = True
                        stack.append((nx, ny))
            boxes.append([x0 * GRID, y0 * GRID, min(w, (x1 + 1) * GRID), min(h, (y1 + 1) * GRID)])
    boxes = [b for b in boxes if (b[2] - b[0]) * (b[3] - b[1]) >= MIN_PANEL_AREA * w * h]
    boxes = [b for b in boxes if not any(o is not b and o[0] <= b[0] and o[1] <= b[1] and o[2] >= b[2] and o[3] >= b[3]
                                         for o in boxes)]
    for b in boxes:                                   # refine each side to the exact first lit line
        b[0], b[1] = max(0, b[0] - GRID), max(0, b[1] - GRID)
        b[2], b[3] = min(w, b[2] + GRID), min(h, b[3] + GRID)
        while b[0] < b[2] - 1 and not any(lit(b[0], y) for y in range(b[1], b[3], 2)): b[0] += 1
        while b[2] > b[0] + 1 and not any(lit(b[2] - 1, y) for y in range(b[1], b[3], 2)): b[2] -= 1
        while b[1] < b[3] - 1 and not any(lit(x, b[1]) for x in range(b[0], b[2], 2)): b[1] += 1
        while b[3] > b[1] + 1 and not any(lit(x, b[3] - 1) for x in range(b[0], b[2], 2)): b[3] -= 1
    boxes.sort(key=lambda b: (b[1] + b[3]) / 2)
    ordered, row = [], []
    for b in boxes:                                   # same row if its centre falls inside the row's first box
        if row and not (row[0][1] <= (b[1] + b[3]) / 2 <= row[0][3]):
            ordered += sorted(row)
            row = []
        row.append(b)
    return ordered + sorted(row)


def match_boxes(boxes, rects, w, h):
    """Order detected boxes to match the expected panels: nearest centre plus closest aspect ratio."""
    import itertools
    import math

    def cost(b, r):
        cx, cy = (b[0] + b[2]) / 2 / w, (b[1] + b[3]) / 2 / h
        ex, ey = r[0] + r[2] / 2, r[1] + r[3] / 2
        got = (b[2] - b[0]) / (b[3] - b[1]) * (w / h) ** 0   # pixel aspect
        exp = (r[2] * w) / (r[3] * h)
        return math.hypot(cx - ex, cy - ey) + 0.5 * abs(math.log(got / exp))
    best = min(itertools.permutations(range(len(boxes))),
               key=lambda perm: sum(cost(boxes[j], rects[i]) for i, j in enumerate(perm)))
    return [boxes[j] for j in best]


# ── slot localisation: finding the border ChatGPT actually drew ──
#
# The returned sheet is never an exact scale of the template — it comes back at whatever size the app
# felt like (about 1.5 megapixels), and the model redraws each slot's white border a few pixels off
# where the scaled box says it is. Cutting on the scaled box therefore leaves a sliver of the white
# border inside the art: thin white lines along tile edges in the atlas, and a seam metric that reads
# the white row rather than the art. So every side of every slot is searched for the border line it
# actually has, and the art area starts just inside it.

NEAR_WHITE = 235                      # r,g,b all above this is "the template's white border"


BORDER_RUN = 0.55                     # ...over this much of the side's length


BORDER_RUN_LOW = 0.3                  # ...or this much, on a second pass, where the art overlaps it


BORDER_REACH = 0.12                   # how far either side of the scaled edge the border is hunted


BORDER_SKIRT = 200                    # the border's anti-aliased edge: bright, but not white


SKIRT_FRAC, SKIRT_MAX = 0.3, 3        # how much of a line is skirt, and how far in to chase it


def _bright_frac(rows, w, h, ch, pos, lo, hi, vertical, level=NEAR_WHITE):
    """How much of a column (vertical) or row between lo and hi is brighter than `level`."""
    n = hi - lo
    if n <= 0 or pos < 0 or pos >= (w if vertical else h):
        return 0.0
    if vertical:
        it = (rows[y][pos * ch:pos * ch + 3] for y in range(lo, hi))
    else:
        r = rows[pos]
        it = (r[x * ch:x * ch + 3] for x in range(lo, hi))
    return sum(1 for p in it if p[0] > level and p[1] > level and p[2] > level) / n


def _white_frac(rows, w, h, ch, pos, lo, hi, vertical):
    return _bright_frac(rows, w, h, ch, pos, lo, hi, vertical)


def localise_slot(rows, w, h, ch, box):
    """Refine a scaled slot box (x0, y0, x1, y1 exclusive) onto the border the sheet really has.

    Each side is searched +-3% of the slot's size (at least 6 px) for the white border line, and the
    art is taken to start just inside it. For a left or top edge the INNERMOST candidate wins, for a
    right or bottom edge likewise, so a neighbouring slot's border across the gutter can never be
    mistaken for this one's. A side with no border found falls back to the scaled edge inset by 2 px.
    Returns (box, sides not found)."""
    x0, y0, x1, y1 = box
    rx, ry = max(24, int(round(BORDER_REACH * (x1 - x0)))), max(24, int(round(BORDER_REACH * (y1 - y0))))
    ylo, yhi = y0 + (y1 - y0) // 10, y1 - (y1 - y0) // 10
    xlo, xhi = x0 + (x1 - x0) // 10, x1 - (x1 - x0) // 10
    miss = []

    def side(name, cands, vertical, lo, hi, inner, default):
        # The innermost candidate always wins, so searching generously outward can never pick up a
        # neighbour's border across the gutter, and searching generously inward is what finds the one
        # ChatGPT redrew forty pixels off. A second, slacker pass catches a border the art overlaps.
        for level in (BORDER_RUN, BORDER_RUN_LOW):
            found = [p for p in cands
                     if _white_frac(rows, w, h, ch, p, lo, hi, vertical) >= level]
            if found:
                return inner(found)
        miss.append(name)
        return default

    def skirt(pos, step, vertical, lo, hi):
        """Step further in past the border's anti-aliased edge: bright, but never white.

        Without this the cut keeps a half-lit row of the border, which the mode filter promotes to a
        full art pixel on a sheet that came back at about 1:1 — the dashed white outlines that were
        left around the trees and the barn when only the white line itself was skipped."""
        for _ in range(SKIRT_MAX):
            if _bright_frac(rows, w, h, ch, pos, lo, hi, vertical, BORDER_SKIRT) < SKIRT_FRAC:
                break
            pos += step
        return pos

    nx0 = side("left", range(x0 - rx, x0 + rx + 1), True, ylo, yhi, lambda f: max(f) + 1, x0 + 2)
    nx1 = side("right", range(x1 - rx, x1 + rx + 1), True, ylo, yhi, min, x1 - 2)
    ny0 = side("top", range(y0 - ry, y0 + ry + 1), False, xlo, xhi, lambda f: max(f) + 1, y0 + 2)
    ny1 = side("bottom", range(y1 - ry, y1 + ry + 1), False, xlo, xhi, min, y1 - 2)
    nx0 = skirt(nx0, 1, True, ylo, yhi)
    nx1 = skirt(nx1 - 1, -1, True, ylo, yhi) + 1
    ny0 = skirt(ny0, 1, False, xlo, xhi)
    ny1 = skirt(ny1 - 1, -1, False, xlo, xhi) + 1
    if nx1 - nx0 < 8 or ny1 - ny0 < 8:                   # nonsense: keep the scaled box, inset
        return (x0 + 2, y0 + 2, x1 - 2, y1 - 2), ["left", "right", "top", "bottom"]
    return (max(0, nx0), max(0, ny0), min(w, nx1), min(h, ny1)), miss


def localise_slots(rows, w, h, ch, slots, sx, sy, label=""):
    """Refine every slot of a template package in place, as `_art`. One warning per sheet."""
    missed = 0
    for s in slots:
        ix, iy, iw, ih = s["inner"]
        box = (int(round(ix * sx)), int(round(iy * sy)),
               int(round((ix + iw) * sx)), int(round((iy + ih) * sy)))
        if box[0] < 0 or box[1] < 0 or box[2] > w or box[3] > h:
            die(f"slot {s['n']} falls outside the image; is this the right file for this package?")
        s["_art"], miss = localise_slot(rows, w, h, ch, box)
        missed += len(miss)
    if missed:
        print(f"  note: {missed} slot side(s) had no white border where one was expected; "
              f"the scaled edge inset by 2 px was used there{label}", file=sys.stderr)
    return missed


def scrub_border(px, w, h, ch, keyed, ring=1):
    """Wipe what is left of the template's white border from a cut image's outermost pixels.

    Two things are residue, and nothing else is:

    * a near-white STRAIGHT RUN along an edge, 60% of that edge's length or more — a border line the
      cut kept whole; and
    * a near-white edge pixel with nothing bright behind it — the DASHED remains of a border line on
      a sheet that came back at about 1:1, where the mode filter promoted the border in some blocks
      and not others. A border line is one pixel thin by construction; a headband, a flower, a
      plaster wall or the white water foam is a body of white with more white just inside it, so the
      support test leaves all of them alone.

    Nothing in the interior is ever touched either way."""
    def white(x, y, level=NEAR_WHITE):
        r = px[y]
        return r[x * ch] > level and r[x * ch + 1] > level and r[x * ch + 2] > level

    def bright(x, y):
        r = px[y]
        return (r[x * ch] > BORDER_SKIRT and r[x * ch + 1] > BORDER_SKIRT
                and r[x * ch + 2] > BORDER_SKIRT and (ch < 4 or r[x * ch + 3]))

    def longest(flags):
        best = run = 0
        for f in flags:
            run = run + 1 if f else 0
            best = max(best, run)
        return best

    n = 0
    for r in range(min(ring, (min(w, h) - 1) // 2), -1, -1):
        for edge in ("top", "bottom", "left", "right"):
            if edge in ("top", "bottom"):
                y = r if edge == "top" else h - 1 - r
                sy = y + 1 if edge == "top" else y - 1
                cells = [(x, y, x, sy) for x in range(w)]
            else:
                x = r if edge == "left" else w - 1 - r
                sx_ = x + 1 if edge == "left" else x - 1
                cells = [(x, y, sx_, y) for y in range(h)]
            # A whole white line has to be white; a dash of one only has to be pale, because a border
            # that the keying caught half of comes back as a half-transparent pale pixel, and that is
            # still a line on the phone. Either way it only goes if the art behind it is not pale too.
            flags = [white(cx, cy) for cx, cy, _, _ in cells]
            whole = longest(flags) >= BORDER_RUN * len(cells)
            pale = [white(cx, cy, BORDER_SKIRT) for cx, cy, _, _ in cells]
            for (cx, cy, ax, ay), f, pl in zip(cells, flags, pale):
                if not ((f and whole) or (pl and not bright(ax, ay))):
                    continue
                if keyed:
                    px[cy][cx * ch:cx * ch + ch] = b"\0" * ch
                else:
                    px[cy][cx * ch:(cx + 1) * ch] = px[ay][ax * ch:(ax + 1) * ch]
                n += 1
    return n


def scaled_inner(slot, sx, sy, w, h):
    """A slot's inner box in the returned image, with a couple of pixels shaved off for soft borders."""
    if slot.get("_art"):
        x0, y0, x1, y1 = slot["_art"]
        return x0, y0, x1 - x0, y1 - y0
    ix, iy, iw, ih = slot["inner"]
    x0 = int(round(ix * sx)) + CUT_PAD
    y0 = int(round(iy * sy)) + CUT_PAD
    x1 = int(round((ix + iw) * sx)) - CUT_PAD
    y1 = int(round((iy + ih) * sy)) - CUT_PAD
    if x0 < 0 or y0 < 0 or x1 > w or y1 > h or x1 - x0 < 8 or y1 - y0 < 8:
        die(f"slot {slot['n']} falls outside the image; is this the right file for this package?")
    return x0, y0, x1 - x0, y1 - y0


def body_bounds(px, w, h, alpha=200, least=3):
    """The silhouette's box, ignoring dust: a row or column counts only once `least` pixels in it are
    solid. With a soft-keyed frame a single half-transparent speck in a corner would otherwise make
    every frame's box the whole frame, and the anchoring would have nothing to work from."""
    xs = [x for x in range(w) if sum(1 for y in range(h) if px[y][x * 4 + 3] >= alpha) >= least]
    ys = [y for y in range(h) if sum(1 for x in range(w) if px[y][x * 4 + 3] >= alpha) >= least]
    if not xs or not ys:
        return None
    return xs[0], ys[0], xs[-1] - xs[0] + 1, ys[-1] - ys[0] + 1


def shift_frame(px, w, h, dx, dy):
    """Move a frame's art by (dx, dy) inside its own box; what falls off the box is gone."""
    out = [bytearray(w * 4) for _ in range(h)]
    for y in range(h):
        sy = y - dy
        if not (0 <= sy < h):
            continue
        row, dst = px[sy], out[y]
        if dx >= 0:
            dst[dx * 4:w * 4] = row[0:(w - dx) * 4]
        else:
            dst[0:(w + dx) * 4] = row[-dx * 4:w * 4]
    return out
