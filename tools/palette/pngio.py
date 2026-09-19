"""Minimal stdlib PNG read/write. Reader adapted from story_prompt.py (read-only copy).

Adds: indexed (colour type 3) writing with PLTE/tRNS, and palette-PNG reading.
"""
import struct
import subprocess
import zlib
from pathlib import Path


def read_png(path):
    """(w, h, ch, rows) for 8-bit PNG. ch is 3 (RGB) or 4 (RGBA); palette images are expanded."""
    path = Path(path)
    data = path.read_bytes()
    if data[:8] != b"\x89PNG\r\n\x1a\n":
        tmp = path.parent / f"_{path.stem}.conv.png"
        subprocess.run(["sips", "-s", "format", "png", str(path), "--out", str(tmp)],
                       check=True, capture_output=True)
        data = tmp.read_bytes()
    pos, idat, plte, trns, w, h, ctype = 8, b"", b"", b"", 0, 0, 2
    while pos < len(data):
        n, kind = struct.unpack(">I4s", data[pos:pos + 8])
        body = data[pos + 8:pos + 8 + n]
        pos += 12 + n
        if kind == b"IHDR":
            w, h, depth, ctype, _, _, interlace = struct.unpack(">IIBBBBB", body)
            if depth != 8 or ctype not in (0, 2, 3, 6) or interlace:
                raise SystemExit(f"{path.name}: unsupported PNG (8-bit non-interlaced only)")
        elif kind == b"PLTE":
            plte = body
        elif kind == b"tRNS":
            trns = body
        elif kind == b"IDAT":
            idat += body
    src_ch = {0: 1, 2: 3, 3: 1, 6: 4}[ctype]
    raw, stride = zlib.decompress(idat), w * src_ch
    rows, prev = [], bytearray(stride)
    for y in range(h):
        f = raw[y * (stride + 1)]
        line = bytearray(raw[y * (stride + 1) + 1:(y + 1) * (stride + 1)])
        if f == 1:
            for i in range(src_ch, stride):
                line[i] = (line[i] + line[i - src_ch]) & 255
        elif f == 2:
            for i in range(stride):
                line[i] = (line[i] + prev[i]) & 255
        elif f == 3:
            for i in range(stride):
                left = line[i - src_ch] if i >= src_ch else 0
                line[i] = (line[i] + ((left + prev[i]) >> 1)) & 255
        elif f == 4:
            for i in range(stride):
                a = line[i - src_ch] if i >= src_ch else 0
                bb, c = prev[i], (prev[i - src_ch] if i >= src_ch else 0)
                pa, pb, pc = abs(bb - c), abs(a - c), abs(a + bb - 2 * c)
                line[i] = (line[i] + (a if pa <= pb and pa <= pc else bb if pb <= pc else c)) & 255
        rows.append(line)
        prev = line
    if ctype == 3:
        alpha = list(trns) + [255] * (len(plte) // 3 - len(trns))
        out = []
        for line in rows:
            o = bytearray()
            for i in line:
                o += plte[i * 3:i * 3 + 3] + bytes([alpha[i]])
            out.append(o)
        return w, h, 4, out
    if ctype == 0:
        out = [bytearray(b for v in line for b in (v, v, v)) for line in rows]
        return w, h, 3, out
    return w, h, src_ch, rows


def _chunk(kind, body):
    return struct.pack(">I", len(body)) + kind + body + struct.pack(">I", zlib.crc32(kind + body))


def write_png(path, w, h, ch, rows):
    raw = b"".join(b"\x00" + bytes(r) for r in rows)
    Path(path).write_bytes(
        b"\x89PNG\r\n\x1a\n"
        + _chunk(b"IHDR", struct.pack(">IIBBBBB", w, h, 8, 2 if ch == 3 else 6, 0, 0, 0))
        + _chunk(b"IDAT", zlib.compress(raw, 9)) + _chunk(b"IEND", b""))


def write_indexed_png(path, w, h, index_rows, palette, alpha=None):
    """index_rows: list of bytearray of palette indices. palette: list of (r,g,b)."""
    plte = b"".join(bytes(c) for c in palette)
    raw = b"".join(b"\x00" + bytes(r) for r in index_rows)
    body = (b"\x89PNG\r\n\x1a\n"
            + _chunk(b"IHDR", struct.pack(">IIBBBBB", w, h, 8, 3, 0, 0, 0))
            + _chunk(b"PLTE", plte))
    if alpha:
        body += _chunk(b"tRNS", bytes(alpha))
    body += _chunk(b"IDAT", zlib.compress(raw, 9)) + _chunk(b"IEND", b"")
    Path(path).write_bytes(body)
