"""`tmap preview`, and the `tmap` command itself.

Renders a map the v2 way — dual-grid masks, swatches sampled in world space, hashed
decals — to story/out/<map>.tmap.png, drawing a flat labelled colour for any tile whose
atlas cell is still empty, so a map can be judged on the Mac before any art exists.

Must never do: diverge from how src/ draws the same map.
Public: placeholder_rgb, terrain_colour, load_swatch, preview_tmap, cmd_tmap.
Imports: masks, md, paths, png, terrain, tileset, tmap, tset.
"""
import sys
import zlib
from .masks import DUAL_CASES, h32, tile_mask, vnoise
from .md import die, slug
from .paths import OUT, ROOT
from .png import read_png, resize_box, to_rgba, write_png
from .terrain import set_decals, set_terrains
from .tileset import tileset_entries
from .tmap import check_tmap, parse_tmap, tmap_path
from .tset import ATLAS_COLS, MASK_VARIANTS, TMAPS, all_tmaps, atlas_path, decal_path, swatch_path



def placeholder_rgb(name):
    """A stable muted colour for a tile with no atlas cell yet, so a preview is still readable."""
    v = zlib.crc32(name.encode())
    return (90 + (v & 63), 90 + ((v >> 6) & 63), 90 + ((v >> 12) & 63))


PREVIEW_TILE = 32                     # the preview draws a tile this big; the maps are 48x36


TERRAIN_STANDIN = {                   # a readable flat colour until the swatch is drawn
    "grass": (104, 132, 78), "grass_dry": (150, 148, 96), "crop": (128, 140, 70),
    "mud": (86, 72, 58), "dirt": (150, 126, 94), "gravel": (146, 142, 134),
    "paving": (150, 148, 142), "bridge_deck": (134, 104, 70), "plank": (134, 104, 70),
    "water": (70, 104, 140), "sand": (204, 184, 140), "snow": (220, 224, 232),
}


def terrain_colour(e):
    """A flat stand-in for a terrain with no swatch yet.

    Named where the name is one we use, hashed otherwise — a preview whose dirt is bright purple
    tells you nothing about whether the map reads."""
    if e["id"] in TERRAIN_STANDIN:
        return TERRAIN_STANDIN[e["id"]]
    for k, v in TERRAIN_STANDIN.items():
        if k in e["id"]:
            return v
    return placeholder_rgb(e["id"])


def load_swatch(setname, e, px):
    """(w, h, rows RGBA) of a terrain's swatch, scaled to `px` per tile. A flat colour if unpainted."""
    p = swatch_path(setname, e["id"])
    tw, th = e["swatch"]
    W, H = tw * px, th * px
    if p.exists():
        w, h, ch, rows = read_png(p)
        rows = to_rgba(rows, w, h, ch)
        if (w, h) != (W, H):
            rows = resize_box(rows, w, h, 4, W, H)
        return W, H, rows
    c = bytes(terrain_colour(e)) + b"\xff"
    return W, H, [bytearray(c * W) for _ in range(H)]


def preview_tmap(mp):
    """Render a map the way the engine will (D20): dual-grid masks over world-space swatches.

    This exists so a map can be judged on the Mac before the engine lands, and so the masks can be
    judged at all — they are generated, so nobody ever sees them until something draws them. Terrains
    with no swatch yet come out as flat palette colours, which is still the right SHAPE."""
    m = parse_tmap(mp)
    setname = slug(m["meta"].get("tileset", ""))
    entries = tileset_entries(setname)
    w, h = (int(x) for x in m["meta"]["size"].split()[:2])
    T = PREVIEW_TILE
    W, H = w * T, h * T
    img = [bytearray(b"\x20\x20\x28\xff" * W) for _ in range(H)]

    # ── the ground: one pass per terrain, low priority first, through the dual grid ──
    terrains = set_terrains(entries)
    tidx = {t: n for n, t in enumerate(terrains)}
    ground = [[None] * w for _ in range(h)]
    for y, line in enumerate(m["ground"][:h]):
        for x, c in enumerate(line[:w]):
            ident = m["legend"].get(c)
            if ident in tidx:
                ground[y][x] = ident
    base = terrains[0] if terrains else None
    at = lambda x, y: (ground[y][x] if 0 <= x < w and 0 <= y < h else base) or base
    sw = {t: load_swatch(setname, entries[t], T) for t in terrains}
    masks = {}
    for n, t in enumerate(terrains):
        e = entries[t]
        style = e["edge_style"]
        sww, swh, swpx = sw[t]
        if n == 0:                                             # the lowest terrain floods the map
            for py in range(H):
                src = swpx[py % swh]
                row = img[py]
                for px in range(W):
                    o, so = px * 4, (px % sww) * 4
                    row[o:o + 4] = src[so:so + 4]
            continue
        for j in range(h + 1):
            for i in range(w + 1):
                b = 0                                          # 1 NW, 2 NE, 4 SE, 8 SW
                if at(i - 1, j - 1) == t: b |= 1
                if at(i, j - 1) == t: b |= 2
                if at(i, j) == t: b |= 4
                if at(i - 1, j) == t: b |= 8
                case = DUAL_CASES[b]
                if case is None:
                    continue
                shape, rot = case
                v = h32(i, j, tidx[t]) % MASK_VARIANTS
                key = (shape, style, v, rot)
                if key not in masks:
                    mk = tile_mask(shape, style, v, rot)
                    n_ = len(mk)
                    masks[key] = [bytearray(mk[y * n_ // T][x * n_ // T] for x in range(T))
                                  for y in range(T)]
                mk = masks[key]
                ox, oy = i * T - T // 2, j * T - T // 2
                for py in range(T):
                    cy = oy + py
                    if not (0 <= cy < H):
                        continue
                    mrow, row, src = mk[py], img[cy], swpx[cy % swh]
                    for px in range(T):
                        cx = ox + px
                        if 0 <= cx < W and mrow[px]:
                            o, so = cx * 4, (cx % sww) * 4
                            row[o:o + 4] = src[so:so + 4]

    # ── the decals: the same hash scatter the engine will run, so the density is judged here ──
    decals = [entries[i] for i in set_decals(entries)]
    art = {}
    for e in decals:
        p = decal_path(setname, e["id"])
        if p.exists():
            dw, dh, dch, dpx = read_png(p)
            art[e["id"]] = (dw, dh, to_rgba(dpx, dw, dh, dch))
    placed = 0
    for y in range(h):
        for x in range(w):
            t = at(x, y)
            here = [e for e in decals if t in e["on"]]
            if not here:
                continue
            touch = {at(x + dx, y + dy) for dx, dy in ((1, 0), (-1, 0), (0, 1), (0, -1))}
            for e in here:
                bias = max([e["edge_bias"].get(n, 1.0) for n in touch] or [1.0])
                field = 0.5 + 0.5 * vnoise(x / e["cluster"], y / e["cluster"],
                                           h32(zlib.crc32(e["id"].encode())))
                want = e["density"] * bias * field
                hh = h32(x, y, zlib.crc32(e["id"].encode()))
                if (hh & 0xffff) / 65535.0 > want:
                    continue
                size = e["sizes"][(hh >> 17) % len(e["sizes"])]
                px_ = int(e["tiles"] * size * T)
                if e["id"] not in art:                       # no art yet: a dot, so density reads
                    r = max(1, px_ // 3)
                    cx, cy = x * T + (hh >> 5) % T, y * T + (hh >> 11) % T
                    for yy in range(-r, r + 1):
                        for xx in range(-r, r + 1):
                            if xx * xx + yy * yy > r * r:
                                continue
                            ax, ay = cx + xx, cy + yy
                            if 0 <= ax < W and 0 <= ay < H:
                                o = ax * 4
                                img[ay][o:o + 3] = bytes(max(0, v - 28) for v in img[ay][o:o + 3])
                    placed += 1
                    continue
                dw, dh, dpx = art[e["id"]]
                nw, nh = max(1, round(px_ * dw / max(dw, dh))), max(1, round(px_ * dh / max(dw, dh)))
                sc = resize_box(dpx, dw, dh, 4, nw, nh)
                flip = e["flip"] and (hh >> 3) & 1
                ox, oy = x * T + (hh >> 5) % max(1, T - nw // 2), y * T + (hh >> 11) % max(1, T - nh // 2)
                for yy in range(nh):
                    ay = oy + yy
                    if not (0 <= ay < H):
                        continue
                    for xx in range(nw):
                        ax = ox + xx
                        sx_ = (nw - 1 - xx) if flip else xx
                        if 0 <= ax < W and sc[yy][sx_ * 4 + 3] >= 128:
                            img[ay][ax * 4:ax * 4 + 4] = sc[yy][sx_ * 4:sx_ * 4 + 4]
                placed += 1

    # ── the stamps: straight out of the atlas, exactly as before ──
    atlas, arows = None, 0
    if atlas_path(setname).exists():
        aw, ah, ach, apx = read_png(atlas_path(setname))
        apx = to_rgba(apx, aw, ah, ach)
        if aw != ATLAS_COLS * T:
            k = ATLAS_COLS * T
            ah = max(1, round(ah * k / aw))
            apx, aw = resize_box(apx, aw, len(apx), 4, k, ah), k
        atlas, arows = apx, ah
    flips = {(int(f[0]), int(f[1])) for f in m.get("flips", []) if len(f) == 2}

    def blit(index, cw, chh, px, py, mirror=False):
        if px + cw * T > W or py + chh * T > H:
            return True
        for r in range(chh):
            for c in range(cw):
                cell = index + ATLAS_COLS * r + (cw - 1 - c if mirror else c)
                ax, ay = (cell % ATLAS_COLS) * T, (cell // ATLAS_COLS) * T
                if atlas is None or ay + T > arows:
                    return False
                for y in range(T):
                    src, dst = atlas[ay + y], img[py + r * T + y]
                    for x in range(T):
                        sx_ = (T - 1 - x) if mirror else x
                        if src[(ax + sx_) * 4 + 3] >= 128:
                            o = (px + c * T + x) * 4
                            dst[o:o + 4] = src[(ax + sx_) * 4:(ax + sx_) * 4 + 4]
        return True

    def flat(name, px, py, cw, chh):
        rgb = bytes(placeholder_rgb(name)) + b"\xff"
        cw = min(cw, (W - px) // T)
        chh = min(chh, (H - py) // T)
        for y in range(chh * T):
            row = img[py + y]
            for x in range(cw * T):
                edge = x < 1 or y < 1 or x >= cw * T - 1 or y >= chh * T - 1
                row[(px + x) * 4:(px + x) * 4 + 4] = b"\x18\x18\x18\xff" if edge else rgb

    miss = set()
    for y, line in enumerate(m["objects"][:h]):
        for x, c in enumerate(line[:w]):
            if c in (".", "+", " "):
                continue
            e = entries.get(m["legend"].get(c, ""))
            if not e or e["kind"] != "tile":
                continue
            cw, chh = e["foot"]
            if not blit(e["index"], cw, chh, x * T, y * T, (x, y) in flips):
                flat(e["id"], x * T, y * T, cw, chh)
                miss.add(e["id"])
    # an authored decal beats the scatter, so it is drawn last
    for f in m.get("decals", []):
        if len(f) >= 3 and f[2] in art:
            dw, dh, dpx = art[f[2]]
            x, y = int(f[0]) * T, int(f[1]) * T
            for yy in range(min(dh, H - y)):
                for xx in range(min(dw, W - x)):
                    sx_ = (dw - 1 - xx) if len(f) > 3 else xx
                    if dpx[yy][sx_ * 4 + 3] >= 128:
                        img[y + yy][(x + xx) * 4:(x + xx) * 4 + 4] = dpx[yy][sx_ * 4:sx_ * 4 + 4]

    OUT.mkdir(exist_ok=True)
    out = OUT / f"{slug(mp)}.tmap.png"
    write_png(out, W, H, 4, img)
    painted = sum(1 for t in terrains if swatch_path(setname, t).exists())
    print(f"{m['path'].name}: {w}x{h} tiles -> {out.relative_to(ROOT.parent)}  {W}x{H}   "
          f"{len(terrains)} terrain(s) ({painted} with a swatch), {placed} decal(s) scattered"
          + (f", {len(miss)} stamp(s) still flat placeholders: {', '.join(sorted(miss))}" if miss else ""))


def cmd_tmap(args):
    sub = args[0] if args else ""
    names = [a for a in args[1:] if not a.startswith("--")]
    if sub not in ("check", "preview"):
        die("usage: tmap check <map>|--all   |   tmap preview <map>")
    names = names or (all_tmaps() if "--all" in args or not names else [])
    if not names:
        die(f"no .tmap files in {TMAPS.relative_to(ROOT.parent)}")
    bad = 0
    for mp in names:
        if sub == "preview":
            preview_tmap(mp)
            continue
        err, warn = check_tmap(mp)
        for wmsg in warn:
            print(f"warning: {wmsg}", file=sys.stderr)
        for e in err:
            print(f"ERROR: {e}", file=sys.stderr)
        print(f"{tmap_path(mp).name}: " + (f"{len(err)} error(s)" if err else "ok"))
        bad += 1 if err else 0
    if bad:
        sys.exit(f"story_prompt: {bad} map(s) rejected")
