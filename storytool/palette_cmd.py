"""The `palette` command: apply, check, and the per-scene panel palettes.

palette check fails any shipped image that is not indexed or whose PLTE is not its own
palette, and is also run by `check --all`. palettise_scene builds a scene's own palette
from its own art at slice time, because panels are never lit.

Must never do: paste a colour into a prompt or edit anything under story/palette by hand.
Public: kmeans_palette, scene_panels, palettise_scene, palette_for, cmd_palette_apply,
png_palette, shipped_images, cmd_palette_check, cmd_palette. Imports: colormap, md,
palette, palette_fit, paths, png.
"""
from pathlib import Path
import random
import re
import struct
import sys
import time
from .colormap import cmd_palette_colormap
from .md import die
from .palette import MASTER_HEX, PAL_SIZE, PalIndex, SHIPPED_MASTER, master_palette, read_hex, scene_hex, ship_png, write_hex
from .palette_fit import cmd_palette_build, palette_histogram
from .paths import PANELS, ROOT
from .png import read_png



# ── per-scene palettes for cutscene panels ──

def kmeans_palette(pts, k, iters=8, seed=7):
    """k-means++ in Oklab, weighted by pixel count. Returns sRGB means — statistical, no ramps.

    Panels are never lit and never share the screen with the field, so nothing here has to shade
    gracefully; what matters is that a sky with forty blues keeps forty blues."""
    rnd = random.Random(seed)
    if len(pts) <= k:
        return [(int(round(p[4])), int(round(p[5])), int(round(p[6]))) for p in pts]
    first = max(pts, key=lambda p: p[3] * rnd.random())
    cents = [(first[0], first[1], first[2])]
    d2 = [((p[0] - cents[0][0]) ** 2 + (p[1] - cents[0][1]) ** 2 + (p[2] - cents[0][2]) ** 2) * p[3]
          for p in pts]
    while len(cents) < k:
        tot = sum(d2)
        if tot <= 0:
            cents.append(pts[rnd.randrange(len(pts))][0:3])
            continue
        t, acc, pick = rnd.random() * tot, 0.0, len(pts) - 1
        for i, v in enumerate(d2):
            acc += v
            if acc >= t:
                pick = i
                break
        c = (pts[pick][0], pts[pick][1], pts[pick][2])
        cents.append(c)
        for i, p in enumerate(pts):
            nd = ((p[0] - c[0]) ** 2 + (p[1] - c[1]) ** 2 + (p[2] - c[2]) ** 2) * p[3]
            if nd < d2[i]:
                d2[i] = nd
    for _ in range(iters):
        sums = [[0.0] * 7 for _ in range(k)]
        for p in pts:
            best, bd = 0, 1e9
            for j, c in enumerate(cents):
                d = (p[0] - c[0]) ** 2 + (p[1] - c[1]) ** 2 + (p[2] - c[2]) ** 2
                if d < bd:
                    bd, best = d, j
            s = sums[best]
            for n, v in enumerate((p[0] * p[3], p[1] * p[3], p[2] * p[3], p[3],
                                   p[4] * p[3], p[5] * p[3], p[6] * p[3])):
                s[n] += v
        for j in range(k):
            if sums[j][3] > 0:
                cents[j] = (sums[j][0] / sums[j][3], sums[j][1] / sums[j][3], sums[j][2] / sums[j][3])
    out, sums = [], [[0.0] * 4 for _ in range(k)]
    for p in pts:
        best, bd = 0, 1e9
        for j, c in enumerate(cents):
            d = (p[0] - c[0]) ** 2 + (p[1] - c[1]) ** 2 + (p[2] - c[2]) ** 2
            if d < bd:
                bd, best = d, j
        s = sums[best]
        s[0] += p[3]
        s[1] += p[4] * p[3]
        s[2] += p[5] * p[3]
        s[3] += p[6] * p[3]
    for s in sums:
        if s[0] > 0:
            out.append(tuple(int(round(s[i] / s[0])) for i in (1, 2, 3)))
    return out


def scene_panels(scene):
    return sorted(PANELS.glob(f"{scene}_p*.png"))


def palettise_scene(scene):
    """Fit a palette to one scene's own panels and convert them all. (message)."""
    files = scene_panels(scene)
    if not files:
        return f"{scene}: no panels to palettise"
    pts = palette_histogram(files, thumbs=True, keep=None)
    cols = kmeans_palette(pts, PAL_SIZE - 1)
    pal = [(0, 0, 0)] + cols[:PAL_SIZE - 1]
    while len(pal) < PAL_SIZE:
        pal.append((0, 0, 0))
    write_hex(scene_hex(scene), pal)
    idx = PalIndex(pal)
    for f in files:
        w, h, ch, rows = read_png(f)
        ship_png(f, w, h, ch, rows, pal, idx)
    return (f"{scene}: {len(files)} panel(s) -> {scene_hex(scene).name} "
            f"({len(cols)} colours + transparent)")


# ── apply, check ──

def palette_for(arg):
    if not arg or arg == "master":
        return master_palette()
    return read_hex(Path(arg))


def cmd_palette_apply(args):
    pal_arg, files = None, []
    it = iter(args)
    for a in it:
        if a == "--palette":
            pal_arg = next(it, "master")
        elif not a.startswith("--"):
            files.append(a)
    if not files:
        die("usage: palette apply <file.png> [more files] [--palette master|<hex file>]")
    pal = palette_for(pal_arg)
    idx = PalIndex(pal)
    for f in files:
        p = Path(f).expanduser()
        if not p.exists():
            die(f"{p} not found")
        w, h, ch, rows = read_png(p)
        was = p.stat().st_size
        t0 = time.time()
        ship_png(p, w, h, ch, rows, pal, idx, note=False)
        print(f"{p.name}: {w}x{h} -> indexed, {was} -> {p.stat().st_size} bytes ({time.time()-t0:.1f}s)")


def png_palette(path):
    """(colour type, PLTE as [(r,g,b)], tRNS bytes) without expanding the image."""
    data = Path(path).read_bytes()
    if data[:8] != b"\x89PNG\r\n\x1a\n":
        return None, [], b""
    pos, ctype, plte, trns = 8, None, [], b""
    while pos < len(data):
        n, kind = struct.unpack(">I4s", data[pos:pos + 8])
        body = data[pos + 8:pos + 8 + n]
        pos += 12 + n
        if kind == b"IHDR":
            ctype = body[9]
        elif kind == b"PLTE":
            plte = [tuple(body[i:i + 3]) for i in range(0, len(body), 3)]
        elif kind == b"tRNS":
            trns = body
        elif kind == b"IDAT":
            break
    return ctype, plte, trns


def shipped_images():
    """[(path, palette, what)] — every image the game ships, with the palette it must be drawn from."""
    out = []
    for rel in SHIPPED_MASTER:
        d = ROOT / rel
        if not d.exists():
            continue
        for p in sorted(d.rglob("*.png")):
            out.append((p, "master"))
    for p in sorted(PANELS.glob("*.png")):
        scene = re.sub(r"_p\d+_.*$", "", p.stem)
        out.append((p, scene))
    return out


def cmd_palette_check(args, quiet=False):
    """Every shipped image is an indexed PNG holding nothing but its own palette. (errors)."""
    err, seen, pals = [], 0, {}
    for p, which in shipped_images():
        seen += 1
        rel = p.relative_to(ROOT.parent)
        if which not in pals:
            hexf = MASTER_HEX if which == "master" else scene_hex(which)
            if not Path(hexf).exists():
                err.append(f"{rel}: no palette at {Path(hexf).relative_to(ROOT.parent)}; "
                           f"run ./story_prompt.py ingest --force on the package that made it")
                pals[which] = None
            else:
                pals[which] = read_hex(hexf)
        pal = pals[which]
        if pal is None:
            continue
        ctype, plte, trns = png_palette(p)
        if ctype != 3:
            err.append(f"{rel}: colour type {ctype}, not an indexed PNG (PALETTE.md: everything ships "
                       f"at one byte a pixel). Recut it, or: ./story_prompt.py palette apply {rel}")
            continue
        if plte[:len(pal)] != [tuple(c) for c in pal]:
            bad = next((i for i in range(min(len(plte), len(pal))) if tuple(plte[i]) != tuple(pal[i])), -1)
            err.append(f"{rel}: its PLTE is not the {which} palette (first difference at index {bad}). "
                       f"The palette changed since this was cut: ./story_prompt.py ingest --force")
            continue
        if list(trns) != [0]:
            err.append(f"{rel}: tRNS is {list(trns)}; only index 0 may be transparent")
    if not quiet:
        for e in err:
            print(f"ERROR: {e}", file=sys.stderr)
        print(f"palette check: {seen} shipped image(s), {len(err)} problem(s)")
    return err


def cmd_palette(args):
    sub = args[0] if args else ""
    if sub == "build":
        cmd_palette_build(args[1:])
    elif sub == "apply":
        cmd_palette_apply(args[1:])
    elif sub == "check":
        if cmd_palette_check(args[1:]):
            sys.exit("story_prompt: shipped art does not match its palette.")
    elif sub == "colormap":
        cmd_palette_colormap(args[1:])
    elif sub == "scene":
        for s in args[1:]:
            print(palettise_scene(s))
    else:
        die("usage: palette build | apply <files> [--palette master|<hex>] | check | colormap | "
            "scene <scene stem>")
