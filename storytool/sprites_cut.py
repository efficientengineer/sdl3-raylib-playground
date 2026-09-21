"""Cutting a sprites sheet: trim, scale back to 64 px a cell, split the frames.

Several frames become one strip beside a .json. The alpha count is checked here and the
complaint goes on keying.LOUD so ingest cannot swallow it.

Must never do: pad or crop a sprite away from its bottom-centre anchor.
Public: cut_sprites. Imports: cutter, field, keying, palette, paths, png, sprites.
"""
import json
import sys
from .cutter import scaled_inner, scrub_border
from .field import SPRITE_DEFAULT_FPS, SPRITE_LOOPS
from .keying import check_alpha, key_magenta
from .palette import ship_png
from .paths import FIELD_DIRS, ROOT
from .png import crop, opaque_bounds, resize_nn
from .sprites import sprite_meta_path



def cut_sprites(slots, rows, w, h, ch, sx, sy, fringe):
    """The billboards. Like the old prop cut, with one difference that matters: the frames of ONE
    sprite are trimmed to ONE shared box.

    Trimming each frame to its own silhouette is what a prop cut does and it is exactly wrong here:
    a creature that opens its mouth in frame two is a pixel wider there, so a per-frame trim would
    shift every frame by a different amount and the engine's flip would read as the whole animal
    twitching sideways. The union of the frames' bounds is taken instead, every frame is cut from
    it, and the result is a strip of identical frames that are registered to each other."""
    out_dir = FIELD_DIRS["sprite"]
    out_dir.mkdir(parents=True, exist_ok=True)
    by_id = {}
    for s in slots:
        by_id.setdefault(s["id"], []).append(s)
    for ident, group in by_id.items():
        group.sort(key=lambda s: s.get("frame", 0))
        keyed, bounds = [], None
        for s in group:
            x, y, bw, bh = scaled_inner(s, sx, sy, w, h)
            px = key_magenta(crop(rows, ch, x, y, bw, bh), bw, bh, ch, fringe)
            scrub_border(px, bw, bh, 4, True, max(2, int(round(2 * sx))))
            b = opaque_bounds(px, bw, bh)
            keyed.append((s, px, bw, bh, b))
            if b:
                bounds = b if bounds is None else (
                    min(bounds[0], b[0]), min(bounds[1], b[1]),
                    max(bounds[0] + bounds[2], b[0] + b[2]) - min(bounds[0], b[0]),
                    max(bounds[1] + bounds[3], b[1] + b[3]) - min(bounds[1], b[1]))
        if bounds is None:
            print(f"  {ident}: nothing but background in {len(group)} slot(s); not written", file=sys.stderr)
            continue
        cx, cy, cw, chh = bounds
        k = group[0]["target"][0] / keyed[0][2]        # the slot is the cell grid: back to 64 px a cell
        fw, fh = max(1, round(cw * k)), max(1, round(chh * k))
        frames = []
        for s, px, bw, bh, _ in keyed:
            frames.append(resize_nn(crop(px, 4, cx, cy, cw, chh), cw, chh, 4, fw, fh))
        strip = [bytearray().join(f[y] for f in frames) for y in range(fh)]
        sw = fw * len(frames)
        out = ROOT.parent / group[0]["out"]
        ship_png(out, sw, fh, 4, strip)
        note = check_alpha(strip, sw, fh, out)
        meta = sprite_meta_path(ident)
        if len(frames) > 1:
            meta.write_text(json.dumps({
                "id": ident, "frames": len(frames), "frame": [fw, fh], "sheet": [sw, fh],
                "footprint": list(group[0]["cells"]),
                "fps": group[0].get("fps", SPRITE_DEFAULT_FPS),
                "loop": group[0].get("loop", SPRITE_LOOPS[0]),
                "anchor": "bottom-centre",
                "note": "A billboard: no direction rows, it always faces the camera. Read `frame` "
                        "from here rather than dividing the sheet.",
            }, indent=2) + "\n")
        elif meta.exists():                            # it used to animate and does not any more
            meta.unlink()
        print(f"  {ident}: {len(frames)} frame(s), trimmed {cw}x{chh} -> "
              f"{out.relative_to(ROOT.parent)}  {sw}x{fh}{note}"
              + (f"  + {meta.name}" if len(frames) > 1 else ""))
