"""Cutting a walk sheet, and the old 16-frame sheets.

A walker is anchored, not merely cut: ChatGPT draws each pose where it likes in its slot,
so every frame is centred on its own silhouette and each direction takes ONE vertical
offset, measured from its stand frames, so a step lifts a foot without the body hopping.
walker_meta_path/write_walker_meta write the .json the engine reads the layout from —
they live here, not in walkers.py, so the cutter does not have to import the builder.

Must never do: squeeze the art down to the old 32x48 design; that turned careful work to
mush.
Public: walker_ident, walker_meta_path, write_walker_meta, cut_walker, cmd_walker_compact.
Imports: cutter, field, keying, manifest, md, oklab, palette, paths, png.
"""
from pathlib import Path
import json
import re
from .cutter import body_bounds, scaled_inner, scrub_border, shift_frame
from .field import EXTRUDE_PX, WALK_FOOT, WALK_OLD_STEPS, WALK_OUT_H, WALK_OUT_W, WALK_ROWS, WALK_ROWS_ASYM, WALK_STEPS
from .keying import check_alpha, key_magenta
from .manifest import write_field_manifest
from .md import die
from .oklab import _to_oklab
from .palette import PalIndex, master_palette, ship_png
from .paths import FIELD_DIRS, ROOT, SHEETS
from .png import crop, read_png, resample, to_rgba



def walker_ident(written):
    return re.sub(r"[^a-z0-9_]", "", written.lower())


def walker_meta_path(ident):
    return FIELD_DIRS["walker"] / f"{ident}.json"


def write_walker_meta(ident, rows, cols=None):
    """The sheet's own layout, beside the sheet. The engine reads this, not a hard-coded 4."""
    p = walker_meta_path(ident)
    p.parent.mkdir(parents=True, exist_ok=True)
    p.write_text(json.dumps({
        "id": ident, "rows": list(rows), "cols": list(cols or WALK_STEPS),
        "frame": [WALK_OUT_W, WALK_OUT_H],
        "sheet": [WALK_OUT_W * len(cols or WALK_STEPS), WALK_OUT_H * len(rows)],
        "mirror_side": "side" in rows,
        "note": "row 'side' is drawn facing WEST and MIRRORED for east. A sheet with rows "
                "S, W, E, N is a character whose design is not symmetric and is never mirrored.",
    }, indent=2) + "\n")
    return p


def cut_walker(data, slots, rows, w, h, ch, sx, sy, fringe=0):
    """A returned walk sheet: one localised frame per slot, keyed, ANCHORED, and one sheet a character.

    Every frame is placed rather than merely cut: ChatGPT draws each pose where it likes inside its
    slot, so without this the character slid sideways and bobbed as the cycle played. Each frame is
    centred horizontally on its own silhouette, and each ROW takes one vertical offset measured from
    its stand frame, so a step may lift a foot without the whole body hopping."""
    FIELD_DIRS["walker"].mkdir(parents=True, exist_ok=True)
    OW, OH = WALK_OUT_W, WALK_OUT_H
    ncol = max(s["col"] for s in slots) + 1               # 3 now; 4 on a sheet drawn before D20
    if data.get("who"):
        who = {c["id"]: c["rows"] for c in data["who"]}
    else:                                                 # one character, rows named by their facing
        ident = slots[0].get("who") or Path(slots[0]["out"]).stem
        rows_named, seen = [], set()
        for s in sorted(slots, key=lambda s: s["row"]):
            if s["row"] not in seen:
                seen.add(s["row"])
                rows_named.append(s["facing"])
        who = {ident: rows_named}
    frames, boxes = {}, {}
    for s in slots:
        x, y, bw, bh = scaled_inner(s, sx, sy, w, h)
        art = crop(rows, ch, x, y, bw, bh)
        if (bw, bh) != (OW, OH):
            art = resample(art, bw, bh, ch, OW, OH)      # area-average down: the size study's rule
        px = key_magenta(art, OW, OH, ch, fringe)
        scrub_border(px, OW, OH, 4, True, EXTRUDE_PX * 2)
        ident = s.get("who") or Path(s["out"]).stem
        frames[(ident, s["row"], s["col"])] = px
        boxes[(ident, s["row"], s["col"])] = body_bounds(px, OW, OH)

    if ncol == 4:
        # A sheet drawn before D20: four columns (stand, step-left, stand, step-right) and four rows
        # (S W E N). Columns 1 and 3 were asked for as the same pose and the E row is the W row
        # mirrored, so the shipping layout is taken straight out of it — cols 0, 1, 3 and rows S, W,
        # N — rather than writing a 16-frame sheet nothing reads any more. `walker compact` reports
        # how true the mirror actually is; this only picks the frames.
        pick_c, pick_r = (0, 1, 3), (0, 1, 3)
        new_frames, new_boxes, new_who = {}, {}, {}
        for ident, rownames in who.items():
            keep = [r for r in pick_r if r < len(rownames)]
            new_who[ident] = [("side" if rownames[r] == "W" else rownames[r]) for r in keep]
            for nr, r in enumerate(keep):
                for nc, c in enumerate(pick_c):
                    if (ident, r, c) in frames:
                        new_frames[(ident, nr, nc)] = frames[(ident, r, c)]
                        new_boxes[(ident, nr, nc)] = boxes[(ident, r, c)]
        frames, boxes, who, ncol = new_frames, new_boxes, new_who, 3
        print(f"  a pre-D20 4x4 sheet: taking columns 1, 2, 4 and rows S, W, N as the 9-frame layout")

    pal, pidx = master_palette(), PalIndex(master_palette())
    report = []
    for ident, rownames in who.items():
        for r in range(len(rownames)):
            stand = boxes.get((ident, r, 0))
            dy = (OH - WALK_FOOT) - (stand[1] + stand[3]) if stand else 0
            for c in range(ncol):
                b = boxes.get((ident, r, c))
                if not b:
                    report.append(f"    {ident} {rownames[r]} frame {c} is empty")
                    continue
                dx = (OW - b[2]) // 2 - b[0]
                frames[(ident, r, c)] = shift_frame(frames[(ident, r, c)], OW, OH, dx, dy)
                nb = body_bounds(frames[(ident, r, c)], OW, OH)
                step = WALK_STEPS[c] if c < len(WALK_STEPS) else WALK_OLD_STEPS[c]
                report.append(f"    {ident} {rownames[r]} {step}: bbox {b[2]}x{b[3]} at "
                              f"{b[0]},{b[1]} -> {nb[0]},{nb[1]} (dx {dx:+d}, dy {dy:+d})")
        SW, SH = OW * ncol, OH * len(rownames)
        sheet = [bytearray(SW * 4) for _ in range(SH)]
        for r in range(len(rownames)):
            for c in range(ncol):
                px = frames.get((ident, r, c))
                if not px:
                    continue
                for k in range(OH):
                    sheet[r * OH + k][c * OW * 4:(c + 1) * OW * 4] = px[k]
        out = FIELD_DIRS["walker"] / f"{ident}.png"
        ship_png(out, SW, SH, 4, sheet, pal, pidx)
        meta = write_walker_meta(ident, rownames, WALK_STEPS[:ncol])
        print(f"  {len(rownames) * ncol} frames of {OW}x{OH} -> {out.relative_to(ROOT.parent)}  "
              f"{SW}x{SH} (rows {', '.join(rownames)}; {meta.name})"
              f"{check_alpha(sheet, SW, SH, out)}")
    for l in report:
        print(l)


def cmd_walker_compact(args):
    """`walker compact <id>...` — an old 16-frame sheet becomes the 9-frame one, at 128x192.

    Three things happen and each is checked. The four duplicate stand frames go (columns 1 and 3 of
    the old sheet were asked for as identical). The E row goes, because the engine mirrors the W row
    — and this REPORTS the Oklab dE between E and mirror(W) before it does, so a design that is not
    actually symmetric is caught rather than quietly flipped. And every frame is area-averaged down
    to 128x192, which the size study measured as the phone's own display size."""
    ids = [walker_ident(a) for a in args if not a.startswith("--")]
    force = "--force" in args
    if not ids:
        ids = sorted(p.stem for p in FIELD_DIRS["walker"].glob("*.png"))
        if not ids:
            die("usage: walker compact <id> [<id>...]   (converts story/field/walkers/<id>.png)")
    pal, pidx = master_palette(), PalIndex(master_palette())
    rows_out = []
    for ident in ids:
        src = FIELD_DIRS["walker"] / f"{ident}.png"
        if not src.exists():
            die(f"{src.relative_to(ROOT.parent)} not found")
        w, h, ch, px = read_png(src)
        if w == len(WALK_STEPS) * WALK_OUT_W and not force:      # already the 9- (or 12-) frame sheet
            meta = walker_meta_path(ident)
            rows_named = WALK_ROWS if h == len(WALK_ROWS) * WALK_OUT_H else WALK_ROWS_ASYM
            print(f"skip  {ident}: already {w}x{h}, rows {', '.join(rows_named)}"
                  + ("" if meta.exists() else "  (wrote its .json)"))
            if not meta.exists():
                write_walker_meta(ident, rows_named)
            continue
        if w % 4 or h % 4:
            die(f"{src.name} is {w}x{h}, which is not the 4x4 grid this converts from")
        fw, fh = w // 4, h // 4
        SHEETS.mkdir(exist_ok=True)
        keep = SHEETS / f"walker16-{ident}.png"           # the 16-frame sheet is archived, not lost
        if not keep.exists():
            keep.write_bytes(src.read_bytes())
        grab = lambda r, c: crop(px, ch, c * fw, r * fh, fw, fh)
        # How far is the E row from a mirror of the W row? Measured both ways round the gait, because
        # a mirrored stride is the OTHER column: a generator that drew the east row independently
        # usually picked the opposite phase, and that is not a reason to call the design asymmetric.
        def mirror_score(pairs):
            de_sum, de_n, shape = 0.0, 0, 0
            for ca, cb in pairs:
                a, b = grab(1, ca), grab(2, cb)
                for y in range(0, fh, 3):
                    for x in range(0, fw, 3):
                        i, j = x * ch, (fw - 1 - x) * ch
                        oa = ch != 4 or a[y][i + 3] >= 128
                        ob = ch != 4 or b[y][j + 3] >= 128
                        if not oa or not ob:
                            shape += 1 if oa != ob else 0
                            continue
                        p1 = _to_oklab(a[y][i], a[y][i + 1], a[y][i + 2])
                        p2 = _to_oklab(b[y][j], b[y][j + 1], b[y][j + 2])
                        de_sum += ((p1[0] - p2[0]) ** 2 + (p1[1] - p2[1]) ** 2
                                   + (p1[2] - p2[2]) ** 2) ** 0.5
                        de_n += 1
            n = de_n + shape
            return de_sum / max(1, de_n), 100.0 * shape / max(1, n)
        same = mirror_score(((0, 0), (1, 1), (3, 3)))
        swap = mirror_score(((0, 0), (1, 3), (3, 1)))
        (mirror_de, shape_pct), phase = (same, "same phase") if same[0] <= swap[0] else \
                                        (swap, "strides swapped")
        OW, OH = WALK_OUT_W, WALK_OUT_H
        sheet = [bytearray(OW * 3 * 4) for _ in range(OH * 3)]
        for r_out, r_in in enumerate((0, 1, 3)):                  # S, side (the old W row), N
            for c_out, c_in in enumerate((0, 1, 3)):              # stand, step-A, step-B
                f = to_rgba(grab(r_in, c_in), fw, fh, ch)
                f = resample(f, fw, fh, 4, OW, OH)
                for y in range(OH):
                    sheet[r_out * OH + y][c_out * OW * 4:(c_out + 1) * OW * 4] = f[y]
        ship_png(src, OW * 3, OH * 3, 4, sheet, pal, pidx)
        write_walker_meta(ident, WALK_ROWS)
        verdict = ("E is a clean mirror of W" if mirror_de < 0.045 and shape_pct < 12 else
                   "E is NOT a mirror of W — this design is asymmetric, so it wants "
                   "'- asymmetric: yes' in characters.md and a 12-frame sheet")
        print(f"ok    {ident}: {w}x{h} (16 frames of {fw}x{fh}) -> {OW * 3}x{OH * 3} "
              f"(9 frames of {OW}x{OH}); mirror dE {mirror_de:.4f}, silhouette "
              f"{shape_pct:.0f}% off ({phase}) — {verdict}"
              f"  [kept {keep.relative_to(ROOT.parent)}]")
        rows_out.append({"id": ident, "kind": "walker", "target": f"{OW * 3}x{OH * 3}",
                         "package": "compacted"})
    if rows_out:
        write_field_manifest(rows_out)
