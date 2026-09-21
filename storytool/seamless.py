"""Making a returned swatch tile with itself, and measuring whether it did.

flatten_lighting takes out the low-frequency lit side a generator paints in, make_seamless
cross-blends the wrap, seam_error measures what is left and band_check/swatch_is_calm
report a swatch that is too busy or too banded to repeat. All of it is pure pixel work on
row lists.

Must never do: read or write a file, or know which tileset it is working for.
Public: flatten_lighting, band_check, swatch_is_calm, seam_error, border_gradient,
make_seamless. Imports: md, oklab.
"""
from .md import die
from .oklab import _to_oklab



FLATTEN_FLOOR = 0.010                 # low-frequency deviation under this is not a lit side


FLATTEN_FULL = 0.045                  # ...and at this much, take all of it out


def flatten_lighting(px, w, h, strength=0.9):
    """Subtract the drawing's own low-frequency lighting, so the swatch can tile. (how much moved)

    A swatch with a bright side or a grassy rim bands wherever it repeats, and the model draws one
    whatever the prompt says. This is a wrapped box blur's deviation from the image mean. The blur
    MUST wrap: a clamped one leaves a bright rim, and the seam roll then puts that rim through the
    middle of the tile, where it reads as a pale cross at every repeat.

    It is also **measured before it is applied**. Art that is already flat — which is what the owner's
    new style returns — has nothing to take out, and subtracting 90% of a deviation that is only
    noise adds noise. The strength ramps from zero at a deviation of 1% of range to full at 4.5%, so
    a flat return is left alone and a lit one is still fixed."""
    r = max(8, w // 6)
    sums = [[[0, 0, 0] for _ in range(w + 1)] for _ in range(h + 1)]
    for y in range(h):
        run = [0, 0, 0]
        for x in range(w):
            i = x * 4
            for k in range(3):
                run[k] += px[y][i + k]
                sums[y + 1][x + 1][k] = sums[y][x + 1][k] + run[k]
    tot = [sums[h][w][k] / (w * h) for k in range(3)]

    def box(x0, y0, x1, y1, k):                       # inclusive-exclusive, already clamped
        return sums[y1][x1][k] - sums[y0][x1][k] - sums[y1][x0][k] + sums[y0][x0][k]

    def wrapped(cx, cy, k):                           # sum over a wrapped window, as up to 4 boxes
        total, n = 0.0, 0
        for y0, y1 in _wrap_spans(cy - r, cy + r + 1, h):
            for x0, x1 in _wrap_spans(cx - r, cx + r + 1, w):
                total += box(x0, y0, x1, y1, k)
                n += (x1 - x0) * (y1 - y0)
        return total / max(1, n)

    # 1. how much low-frequency deviation is there at all? (sampled, on the green-ish mean)
    dev, n = 0.0, 0
    step = max(1, w // 48)
    for y in range(0, h, step):
        for x in range(0, w, step):
            dev += abs(sum(wrapped(x, y, k) - tot[k] for k in range(3)) / 3.0)
            n += 1
    dev /= max(1, n) * 255.0
    if dev <= FLATTEN_FLOOR:                          # already flat: touching it can only add noise
        return 0.0, dev
    k_ = min(1.0, (dev - FLATTEN_FLOOR) / (FLATTEN_FULL - FLATTEN_FLOOR))
    strength *= k_

    moved = 0.0
    for y in range(h):
        for x in range(w):
            i = x * 4
            for k in range(3):
                m = wrapped(x, y, k)
                v = px[y][i + k] - strength * (m - tot[k])
                moved += abs(strength * (m - tot[k]))
                px[y][i + k] = 0 if v < 0 else (255 if v > 255 else int(v))
    return moved / (w * h * 3 * 255), dev


def band_check(px, w, h, ident):
    """A finished swatch may hold NO pale or transparent band. Loud, and it fails the cut.

    The white cross that got through was a whole row and column band of saturated pixels, and every
    check we had looked at single pixels or at edges. This looks at the thing that was actually
    wrong: a row (or column) whose mean lightness is far from the swatch's own median, or which has
    gone transparent. A real texture's row means sit inside a few percent of each other."""
    def means(vertical):
        out = []
        for i in range(h if not vertical else w):
            s_, a_, n_ = 0, 0, 0
            for j in range(0, (w if not vertical else h), 3):
                y, x = (i, j) if not vertical else (j, i)
                o = x * 4
                s_ += px[y][o] + px[y][o + 1] + px[y][o + 2]
                a_ += px[y][o + 3]
                n_ += 1
            out.append((s_ / (3 * n_), a_ / n_))
        return out

    bad = []
    for vertical, what in ((False, "row"), (True, "column")):
        ms = means(vertical)
        vals = sorted(m for m, _ in ms)
        med = vals[len(vals) // 2]
        spread = max(1.0, (vals[int(len(vals) * 0.9)] - vals[int(len(vals) * 0.1)]))
        for i, (m, a) in enumerate(ms):
            if a < 250:
                bad.append(f"{what} {i} is transparent (mean alpha {a:.0f})")
            elif abs(m - med) > max(26.0, 6.0 * spread):
                bad.append(f"{what} {i} has mean {m:.0f} against the swatch's median {med:.0f}")
            if len(bad) > 4:
                break
    if bad:
        die(f"{ident}: the finished swatch has a band in it — {'; '.join(bad[:5])}"
            + (f" and {len(bad) - 5} more" if len(bad) > 5 else "")
            + ". A swatch is one material edge to edge, so a whole row or column that is pale, dark "
              "or transparent is a bug in the cut, not art. Nothing was written.")


def _wrap_spans(lo, hi, n):
    """[lo, hi) on a ring of n, as up to two half-open spans inside [0, n).

    The width is taken BEFORE lo is folded into the ring. Computing it afterwards — `hi = lo + (hi -
    lo)` with lo already reassigned — silently returns a backwards span like (90, 11), and a
    backwards span through a prefix-sum table is not a small error: it is a garbage window mean, and
    at strength 0.9 that saturates a band of pixels to white. That was the white cross through every
    swatch, and it only showed at the image border, which is exactly where the seam roll then put
    it."""
    width = hi - lo
    if width >= n:
        return [(0, n)]
    lo %= n
    hi = lo + width
    return [(lo, n), (0, hi - n)] if hi > n else [(lo, hi)]


def swatch_is_calm(px, w, h, limit=0.115):
    """(calm, contrast). Contrast is the mean |L - mean L| in Oklab, which is what the eye reads."""
    n, s, vals = 0, 0.0, []
    step = max(1, w // 160)
    for y in range(0, h, step):
        for x in range(0, w, step):
            i = x * 4
            L = _to_oklab(px[y][i], px[y][i + 1], px[y][i + 2])[0]
            vals.append(L)
            s += L
            n += 1
    m = s / max(1, n)
    c = sum(abs(v - m) for v in vals) / max(1, n)
    return c <= limit, c


def seam_error(px, w, h):
    """(horizontal, vertical) mean per-channel difference across a tile's wrap. 0 = perfectly seamless."""
    hz = sum(abs(px[y][(w - 1) * 4 + c] - px[y][c]) for y in range(h) for c in range(3)) / (h * 3)
    vt = sum(abs(px[h - 1][x * 4 + c] - px[0][x * 4 + c]) for x in range(w) for c in range(3)) / (w * 3)
    return hz, vt


def _hash01(x, y, salt):
    """A cheap hashed [0,1) per pixel — the dither threshold. Deterministic, so a recut is identical."""
    n = (x * 374761393 + y * 668265263 + salt * 2246822519) & 0xFFFFFFFF
    n = ((n ^ (n >> 13)) * 1274126177) & 0xFFFFFFFF
    return ((n ^ (n >> 16)) & 0xFFFF) / 65535.0


def _smoothstep(t):
    t = 0.0 if t < 0 else (1.0 if t > 1 else t)
    return t * t * (3 - 2 * t)


def border_gradient(px, w, h):
    """((across-x, interior-x), (across-y, interior-y)) mean gradients — the band / line test.

    The wrap error on its own cannot tell a healed tile from a blurred one: an averaged edge scores
    zero and looks like a crease. What separates them is the gradient *across* the tile border
    compared with the texture's own mean gradient on that axis. Much lower than 1 and the border is
    a soft band (averaged pixels); much higher than 1 and it is a line (a real discontinuity). A
    make-seamless by construction puts real adjacent pixels there, so it sits at or just under 1 —
    under, because the split is chosen where the art happens to be quietest."""
    def d(ax, ay, bx, by):
        return sum(abs(px[ay][ax * 4 + c] - px[by][bx * 4 + c]) for c in range(3)) / 3.0
    xs, ys = list(range(0, w - 1, 3)), list(range(0, h - 1, 3))
    cx = sum(d(0, y, w - 1, y) for y in range(h)) / h
    ix = sum(d(x, y, x + 1, y) for y in range(h) for x in xs) / (h * len(xs))
    cy = sum(d(x, 0, x, h - 1) for x in range(w)) / w
    iy = sum(d(x, y, x, y + 1) for x in range(w) for y in ys) / (w * len(ys))
    return (cx, ix), (cy, iy)


def make_seamless(px, w, h, feather=16, hold=3):
    """Make an opaque ground tile tile perfectly, *by construction*, keeping its detail crisp.

    The old `--heal-seams` cross-blended a band at each edge: it removed the wrap error and put a
    soft, blurred ribbon in its place. That ribbon is exactly the faint grid the phone showed on
    grass, dirt and gravel — every tile border was a line of averaged pixels, and averaged pixels
    read as a crease whatever the art either side of them is.

    Nothing is averaged here. The construction is an offset, one axis at a time, so each axis is
    exact and no pixel of the output is anything but one real pixel of the returned art:

    1. **Roll the tile** along the axis by `k`, wrapping. The output's first and last line were
       *adjacent* lines of the original, so the wrap on that axis is now continuous by construction.
       `k` is not h/2 but the **min-error split**: of the middle third of the possible splits, the
       one whose two lines are most alike, so the join is not merely legal but invisible. (At a flat
       h/2 the split can land on a mortar course or a furrow and read as a line even though it
       wraps.)
    2. The roll has moved the original's own discontinuity into the middle, as a line across the
       tile. **Cover it with a strip of the tile's own interior**, copied as whole lines of the
       *other* axis, so the (already exact) continuity of that other axis is carried along with it.
       The source offset is picked from a dozen candidates by least mean error against what it lands
       on, and the strip is narrow — 3 px held, 16 px of feather.
    3. The strip's own two boundaries are cut with a **hashed dither** against the feather ramp, not
       an alpha average: each pixel is taken whole from one source or the other, chosen by a hash of
       its position. Detail stays crisp and the join reads as texture rather than as an edge.
    4. Repeat on the other axis. A roll along x permutes columns only, so step 1's y-continuity
       survives it exactly, and a whole-column patch carries its own y-continuity with it.

    The result: the tile's border pixels *are* interior pixels, so the border is neither a line
    (nothing was left discontinuous) nor a band (nothing was blurred)."""
    R = hold + feather

    def alpha(dist):
        return _smoothstep((R - dist) / float(feather))

    for axis in (0, 1):                                  # 0 = roll in y, 1 = roll in x
        n = h if axis == 0 else w                        # the length along the rolled axis
        m = w if axis == 0 else h                        # the length of one line
        if n < 4 * R:
            continue

        # 1. the min-error split, over the middle third, so the old seam lands clear of both edges
        lo, hi = max(R + 4, n // 3), min(n - R - 4, 2 * n // 3)
        best_k, best_e = n // 2, None
        for kk in range(lo, hi + 1):
            if axis == 0:
                a, b = px[kk - 1], px[kk]
                e = sum(abs(a[t * 4 + c] - b[t * 4 + c]) for t in range(0, m, 3) for c in range(3))
            else:
                e = sum(abs(px[t][kk * 4 + c] - px[t][(kk - 1) * 4 + c])
                        for t in range(0, m, 3) for c in range(3))
            if best_e is None or e < best_e:
                best_k, best_e = kk, e
        k = best_k
        if axis == 0:
            rows = [bytearray(px[(y + k) % h]) for y in range(h)]
            line = lambda i: rows[i]
        else:
            c = k * 4
            rows = [bytearray(px[y][c:] + px[y][:c]) for y in range(h)]
            line = lambda i: bytes(bb for y in range(h) for bb in rows[y][i * 4:i * 4 + 4])

        # 2. the old seam now sits between n-k-1 and n-k; patch a strip of interior over it
        mid = n - k
        band = [i for i in range(n) if abs(i - mid) <= R and 0 <= i < n]
        cands = [o for o in range(R + 6, n - R - 5, 5)]  # never puts the seam back inside the band
        src_off, berr = cands[0] if cands else n // 2, None
        for o in cands:
            err = 0
            for i in band[::3]:
                a, b = line(i), line((i + o) % n)
                for t in range(0, m, 7):
                    err += abs(a[t * 4] - b[t * 4]) + abs(a[t * 4 + 1] - b[t * 4 + 1]) \
                         + abs(a[t * 4 + 2] - b[t * 4 + 2])
            if berr is None or err < berr:
                src_off, berr = o, err
        patch = {i: bytes(line((i + src_off) % n)) for i in band}
        # 3. hashed dither across the feather — a whole pixel from one side or the other, never a mix
        for i in band:
            a = alpha(abs(i - mid))
            if a <= 0:
                continue
            s = patch[i]
            for j in range(m):
                if _hash01(i, j, axis + 1) >= a:
                    continue
                if axis == 0:
                    rows[i][j * 4:j * 4 + 3] = s[j * 4:j * 4 + 3]
                else:
                    rows[j][i * 4:i * 4 + 3] = s[j * 4:j * 4 + 3]
        px[:] = rows
    return px
