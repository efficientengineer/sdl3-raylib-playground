"""PNG in and out, and resampling, with the standard library only.

There is no Pillow on this machine and there never will be: read_png/write_png speak
zlib and struct directly, and everything downstream works on lists of row bytes. resample
picks area-average going down and bilinear going up — never nearest, which tears the
straight edges a return has to line up with.

Must never do: know what an image means (a tile, a portrait, a panel).
Public: read_png, write_png, png_size, resize_box, resize_bilinear, resample, crop,
resize_nn, opaque_bounds, to_rgba. Imports: md, paths.
"""
import struct
import subprocess
import zlib
from .md import die
from .paths import OUT



# ───────────────────────── Slicer (stdlib PNG) ─────────────────────────

def read_png(path):
    """(width, height, channels, rows) for 8-bit RGB/RGBA non-interlaced PNG. Other formats go through sips."""
    data = path.read_bytes()
    if data[:8] != b"\x89PNG\r\n\x1a\n":
        tmp = OUT / f"_{path.stem}.png"
        OUT.mkdir(exist_ok=True)
        try:
            subprocess.run(["sips", "-s", "format", "png", str(path), "--out", str(tmp)],
                           check=True, capture_output=True)
        except (OSError, subprocess.CalledProcessError):
            die(f"{path.name} is not a PNG and sips could not convert it. Convert it to PNG first.")
        data = tmp.read_bytes()
    pos, idat, w, plte, trns = 8, b"", 0, b"", b""
    while pos < len(data):
        n, kind = struct.unpack(">I4s", data[pos:pos + 8])
        body = data[pos + 8:pos + 8 + n]
        pos += 12 + n
        if kind == b"IHDR":
            w, h, depth, ctype, _, _, interlace = struct.unpack(">IIBBBBB", body)
            if depth != 8 or ctype not in (0, 2, 3, 6) or interlace:
                die(f"{path.name}: unsupported PNG (need 8-bit grey/RGB/RGBA/indexed, non-interlaced). "
                    f"Fix with: sips -s format png -s formatOptions default {path.name} --out fixed.png")
            ch = {0: 1, 2: 3, 3: 1, 6: 4}[ctype]
        elif kind == b"PLTE":
            plte = body
        elif kind == b"tRNS":
            trns = body
        elif kind == b"IDAT":
            idat += body
    raw, stride = zlib.decompress(idat), w * ch
    rows, prev = [], bytearray(stride)
    for y in range(h):
        f = raw[y * (stride + 1)]
        line = bytearray(raw[y * (stride + 1) + 1:(y + 1) * (stride + 1)])
        if f == 1:
            for i in range(ch, stride):
                line[i] = (line[i] + line[i - ch]) & 255
        elif f == 2:
            for i in range(stride):
                line[i] = (line[i] + prev[i]) & 255
        elif f == 3:
            for i in range(stride):
                left = line[i - ch] if i >= ch else 0
                line[i] = (line[i] + ((left + prev[i]) >> 1)) & 255
        elif f == 4:
            for i in range(stride):
                a = line[i - ch] if i >= ch else 0
                bb, c = prev[i], (prev[i - ch] if i >= ch else 0)
                pa, pb, pc = abs(bb - c), abs(a - c), abs(a + bb - 2 * c)
                line[i] = (line[i] + (a if pa <= pb and pa <= pc else bb if pb <= pc else c)) & 255
        rows.append(line)
        prev = line
    if ctype == 3:                                   # an indexed PNG the palette step wrote: expand it
        alpha = list(trns) + [255] * (len(plte) // 3 - len(trns))
        out = []
        for line in rows:
            o = bytearray()
            for i in line:
                o += plte[i * 3:i * 3 + 3] + bytes((alpha[i],))
            out.append(o)
        return w, h, 4, out
    if ctype == 0:
        return w, h, 3, [bytearray(b for v in line for b in (v, v, v)) for line in rows]
    return w, h, ch, rows


def write_png(path, w, h, ch, rows):
    def chunk(kind, body):
        return struct.pack(">I", len(body)) + kind + body + struct.pack(">I", zlib.crc32(kind + body))
    raw = b"".join(b"\x00" + bytes(r) for r in rows)
    path.write_bytes(b"\x89PNG\r\n\x1a\n" + chunk(b"IHDR", struct.pack(">IIBBBBB", w, h, 8, 2 if ch == 3 else 6, 0, 0, 0))
                     + chunk(b"IDAT", zlib.compress(raw, 6)) + chunk(b"IEND", b""))


def png_size(path):
    head = path.read_bytes()[:24]
    return struct.unpack(">II", head[16:24]) if head[:8] == b"\x89PNG\r\n\x1a\n" else None


# ── painted views: a finished background over the engine's block-out ──
#
# The owner's direction is the Final Fantasy VIII arrangement: the game keeps the 3D block-out for
# collision, depth and camera, and what you see is a painting laid over it. So the capture is not a
# reference for the painter to interpret — it is the layout, and nothing in it may move.

def resize_box(rows, w, h, ch, nw, nh):
    """Area-average downscale. See cut_view for why this rather than nearest."""
    out = []
    for y in range(nh):
        y0, y1 = y * h // nh, max(y * h // nh + 1, (y + 1) * h // nh)
        dst = bytearray(nw * ch)
        for x in range(nw):
            x0, x1 = x * w // nw, max(x * w // nw + 1, (x + 1) * w // nw)
            n = (x1 - x0) * (y1 - y0)
            for c in range(ch):
                s = 0
                for yy in range(y0, y1):
                    row = rows[yy]
                    for xx in range(x0, x1):
                        s += row[xx * ch + c]
                dst[x * ch + c] = s // n
        out.append(dst)
    return out


def resize_bilinear(rows, w, h, ch, nw, nh):
    """Bilinear enlargement. Used where the return came back a little smaller than the target."""
    out = []
    for y in range(nh):
        fy = (y + 0.5) * h / nh - 0.5
        y0 = max(0, min(h - 1, int(fy // 1)))
        y1 = min(h - 1, y0 + 1)
        ty = max(0.0, fy - y0)
        r0, r1 = rows[y0], rows[y1]
        dst = bytearray(nw * ch)
        for x in range(nw):
            fx = (x + 0.5) * w / nw - 0.5
            x0 = max(0, min(w - 1, int(fx // 1)))
            x1 = min(w - 1, x0 + 1)
            tx = max(0.0, fx - x0)
            for c in range(ch):
                a = r0[x0 * ch + c] * (1 - tx) + r0[x1 * ch + c] * tx
                b = r1[x0 * ch + c] * (1 - tx) + r1[x1 * ch + c] * tx
                dst[x * ch + c] = int(a * (1 - ty) + b * ty + 0.5)
        out.append(dst)
    return out


def resample(rows, w, h, ch, nw, nh):
    """To an exact size: area-average going down, bilinear going up. Never nearest.

    A returned sheet is never an exact scale of the template, so every slot's art area has to be
    brought onto the grid. Nearest at a non-integer ratio drops whole columns and tears the straight
    edges; this keeps them."""
    if (w, h) == (nw, nh):
        return [bytearray(r) for r in rows]
    if nw <= w and nh <= h:
        return resize_box(rows, w, h, ch, nw, nh)
    return resize_bilinear(rows, w, h, ch, nw, nh)


def crop(rows, ch, x, y, w, h):
    return [bytearray(rows[yy][x * ch:(x + w) * ch]) for yy in range(y, y + h)]


def resize_nn(rows, w, h, ch, nw, nh):
    out = []
    for y in range(nh):
        src = rows[min(h - 1, y * h // nh)]
        dst = bytearray(nw * ch)
        for x in range(nw):
            sx = min(w - 1, x * w // nw)
            dst[x * ch:(x + 1) * ch] = src[sx * ch:(sx + 1) * ch]
        out.append(dst)
    return out


def opaque_bounds(rows, w, h):
    x0, y0, x1, y1 = w, h, 0, 0
    for y in range(h):
        line = rows[y]
        for x in range(w):
            if line[x * 4 + 3]:
                x0, x1 = min(x0, x), max(x1, x)
                y0, y1 = min(y0, y), max(y1, y)
    return None if x1 < x0 else (x0, y0, x1 - x0 + 1, y1 - y0 + 1)


# ── cutting a returned tile sheet ──

def to_rgba(rows, w, h, ch, alpha=255):
    if ch == 4:
        return [bytearray(r) for r in rows]
    out = []
    for r in rows:
        dst = bytearray(w * 4)
        for x in range(w):
            dst[x * 4:x * 4 + 3] = r[x * 3:x * 3 + 3]
            dst[x * 4 + 3] = alpha
        out.append(dst)
    return out
