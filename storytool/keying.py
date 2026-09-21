"""Turning the magenta background of a template sheet into transparency.

Keying is by CAST, not by distance: a black outline blended half and half with magenta is
further from pure magenta than a mid grey is, so no distance threshold can catch it
without eating real colour. min(r,b) - g measures it. The flood comes in from the
background only, so an object's interior is never touched and paint may be any colour.
check_alpha then counts what came out, because a slot painted edge to edge keys nothing
and renders on the phone as a black rectangle.

Must never do: threshold on distance to magenta, and never touch an interior pixel the
background cannot reach.
Public: magenta_cast, key_magenta, nearest_clean, erode_alpha, check_alpha, LOUD.
Imports: field.
"""
import sys
from .field import KEY_CAST_CLEAN, KEY_CAST_MIN, KEY_CLEAN_RADIUS, KEY_EDGE_REACH, KEY_HARD, KEY_MIN_ALPHA



# ── cutting a returned template ──

def magenta_cast(r, g, b):
    """How much magenta is mixed into a pixel, 0-255. See the note on KEY_HARD."""
    return min(r, b) - g


def key_magenta(rows, w, h, ch, fringe=0):
    """RGBA rows with the magenta keyed out: flat magenta transparent, the anti-aliased edge cleaned.

    Three passes, because the engine alpha-tests at 0.5 and draws whatever survives at full strength,
    so a surviving pixel has to carry a clean colour — a half-magenta pixel kept at alpha 1 is the
    purple outline the phone showed.

    1. Flat magenta goes transparent, and every pixel within KEY_EDGE_REACH of it is marked as edge.
       Only edge pixels are ever touched again: an object's interior may be any colour it likes.
    2. Each edge pixel's alpha comes from its magenta cast (255 - cast), and the magenta is taken back
       out of its colour at that alpha. Under KEY_MIN_ALPHA it is background.
    3. Any edge pixel still carrying a cast after that — the un-matte only corrects what the estimate
       got right — takes the colour of the nearest clean opaque pixel and keeps its own alpha. With no
       clean pixel within reach (a one-pixel-wide rope) the cast is subtracted off instead.

    `fringe` then erodes the alpha by that many pixels, for a sheet that stays dirty anyway."""
    px = [bytearray(w * 4) for _ in range(h)]
    for y in range(h):
        src, dst = rows[y], px[y]
        for x in range(w):
            r, g, b = src[x * ch], src[x * ch + 1], src[x * ch + 2]
            if ((r - 255) ** 2 + g * g + (b - 255) ** 2) ** 0.5 <= KEY_HARD:
                continue                                        # flat background: leave it at 0,0,0,0
            dst[x * 4:x * 4 + 4] = bytes((r, g, b, 255))

    # The edge is grown from the background rather than measured off it: flood outward through every
    # pixel that still carries a magenta cast, however deep the ramp goes, then take KEY_EDGE_REACH
    # more rings for the colour clamp. A fixed band missed the middle of a soft edge and left the
    # worst pixels — the ones nearest pure magenta — sitting at full alpha. An object that is itself
    # magenta and touches the background would be eaten by this, which is the bargain the contract
    # already makes: on these sheets the magenta is empty space, never paint.
    edge = [[False] * w for _ in range(h)]
    front = [(x, y) for y in range(h) for x in range(w) if not px[y][x * 4 + 3]]
    while front:
        nxt = []
        for x, y in front:
            for nx, ny in ((x + 1, y), (x - 1, y), (x, y + 1), (x, y - 1)):
                if 0 <= nx < w and 0 <= ny < h and px[ny][nx * 4 + 3] and not edge[ny][nx] \
                        and magenta_cast(px[ny][nx * 4], px[ny][nx * 4 + 1], px[ny][nx * 4 + 2]) >= KEY_CAST_MIN:
                    edge[ny][nx] = True
                    nxt.append((nx, ny))
        front = nxt
    front = [(x, y) for y in range(h) for x in range(w) if edge[y][x] or not px[y][x * 4 + 3]]
    for _ in range(KEY_EDGE_REACH):
        nxt = []
        for x, y in front:
            for nx, ny in ((x + 1, y), (x - 1, y), (x, y + 1), (x, y - 1)):
                if 0 <= nx < w and 0 <= ny < h and px[ny][nx * 4 + 3] and not edge[ny][nx]:
                    edge[ny][nx] = True
                    nxt.append((nx, ny))
        front = nxt

    for y in range(h):                                          # 2. alpha from the cast, then un-matte
        row = px[y]
        for x in range(w):
            if not edge[y][x]:
                continue
            r, g, b = row[x * 4], row[x * 4 + 1], row[x * 4 + 2]
            cast = magenta_cast(r, g, b)
            if cast < KEY_CAST_MIN:
                continue
            a = max(0, 255 - cast)
            if a < KEY_MIN_ALPHA:
                row[x * 4:x * 4 + 4] = b"\0\0\0\0"
                continue
            f = a / 255.0                                       # observed = f*colour + (1-f)*magenta
            row[x * 4:x * 4 + 4] = bytes((min(255, max(0, int((r - 255 * (1 - f)) / f))),
                                          min(255, max(0, int(g / f))),
                                          min(255, max(0, int((b - 255 * (1 - f)) / f))), a))

    clean = [[px[y][x * 4 + 3] == 255 and not edge[y][x] for x in range(w)] for y in range(h)]
    for y in range(h):                                          # 3. whatever is still purple
        row = px[y]
        for x in range(w):
            if not edge[y][x] or not row[x * 4 + 3]:
                continue
            r, g, b, a = row[x * 4:x * 4 + 4]
            if magenta_cast(r, g, b) <= KEY_CAST_CLEAN:
                continue
            got = nearest_clean(px, clean, w, h, x, y)
            row[x * 4:x * 4 + 4] = bytes((*got, a)) if got else \
                bytes((max(0, r - magenta_cast(r, g, b)), g, max(0, b - magenta_cast(r, g, b)), a))

    return erode_alpha(px, w, h, fringe) if fringe else px


def nearest_clean(px, clean, w, h, x, y):
    """The colour of the nearest fully opaque pixel that the background never touched, or None."""
    for rad in range(1, KEY_CLEAN_RADIUS + 1):
        best, bd = None, None
        for dy in range(-rad, rad + 1):
            for dx in range(-rad, rad + 1):
                if max(abs(dx), abs(dy)) != rad:
                    continue
                nx, ny = x + dx, y + dy
                if 0 <= nx < w and 0 <= ny < h and clean[ny][nx]:
                    d = dx * dx + dy * dy
                    if bd is None or d < bd:
                        best, bd = px[ny][nx * 4:nx * 4 + 3], d
        if best:
            return tuple(best)
    return None


def erode_alpha(px, w, h, n):
    """Shave n pixels off the alpha, for a sheet whose edge stays dirty however it is keyed."""
    for _ in range(n):
        gone = [(x, y) for y in range(h) for x in range(w) if px[y][x * 4 + 3]
                and any(not (0 <= x + dx < w and 0 <= y + dy < h) or not px[y + dy][(x + dx) * 4 + 3]
                        for dx, dy in ((1, 0), (-1, 0), (0, 1), (0, -1)))]
        for x, y in gone:
            px[y][x * 4:x * 4 + 4] = b"\0\0\0\0"
    return px


LOUD = []           # warnings that have to survive run_quiet's swallowed output; `ingest` prints them


def check_alpha(rows, w, h, out):
    """A prop or a walker must come out with real transparency, or the game draws a black rectangle.

    `write_png` writes colour type 6 whenever it is handed 4 channels, and `key_magenta` always hands
    it 4, so every prop and walker file is RGBA by construction — that part cannot go wrong. What a
    real cut can still get wrong is the *content*: a generator that painted over every last pixel of
    magenta leaves a sprite with nothing keyed out, and the engine draws it as a solid rectangle. Say
    so here, loudly, instead of leaving it to be found on the phone. A sprite that is honestly a
    rectangle trips this too, and should: it will look like one in game."""
    clear = sum(1 for r in rows for i in range(3, len(r), 4) if r[i] < 128)
    if clear == 0:
        LOUD.append(f"{out.name} came out with no transparent pixel anywhere, so the game will draw it "
                    f"as a solid rectangle. Either the magenta was painted over, or the object really "
                    f"does fill its slot. Ask for that slot again with the background left untouched "
                    f"right up to the edge of the object.")
        print(f"WARNING: {LOUD[-1]}", file=sys.stderr)
        return "   <-- NO TRANSPARENCY, see the warning"
    return f"  ({100 * clear // (w * h)}% transparent)"
