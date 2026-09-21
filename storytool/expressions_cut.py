"""Cutting an expression sheet into story/portraits/<name>_<id>.png.

Compares each slot against the others on an 8x12 thumbnail: two faces that are the same
drawing twice, or a slot that came back flat, are reported rather than shipped.

Must never do: guess which slot is which — the slot number is the id's index.
Public: expr_signature, cut_expressions. Imports: cutter, keying, palette, paths, png,
rules.
"""
import sys
from .cutter import scaled_inner, scrub_border
from .keying import LOUD
from .palette import ship_png
from .paths import PORTRAITS, ROOT
from .png import crop, resample
from .rules import EXPR_OUT_H, EXPR_OUT_W



EXPR_SIG = (8, 12)                    # the thumbnail two faces are compared on


EXPR_SAME = 3.0                       # mean per-channel difference under this: the same drawing twice


EXPR_FLAT = 4.0                       # spread under this: the slot came back empty


def expr_signature(px, w, h):
    """A tiny RGB thumbnail of one cut face, for the empty/duplicate checks.

    Colour, not luminance: two faces can differ only in what is coloured (a blush, a tear, bared
    teeth) and a grey thumbnail would call them the same drawing."""
    sw, sh = EXPR_SIG
    small = resample(px, w, h, 3, sw, sh)
    return [float(v) for r in small for v in r[:sw * 3]]


def cut_expressions(data, slots, rows, w, h, ch, sx, sy, neutral_main=False):
    """Ten head-and-shoulders portraits out of one expression sheet.

    A portrait is an opaque rectangle, not a sprite: nothing is keyed out, the flat grey behind the
    head ships with it exactly as the reference-sheet portrait's does. The only cleaning is the
    border scrub, because a localised slot can still hold a pixel of the white border."""
    PORTRAITS.mkdir(exist_ok=True)
    sigs, main = [], None
    for s in slots:
        x, y, bw, bh = scaled_inner(s, sx, sy, w, h)
        # A portrait is opaque, so a surviving sliver of the white border would ship as a bright line
        # down the side of the face. There is nothing to lose by shaving the slot: the head sits in
        # the middle of it and the rest is flat grey.
        pad = max(2, int(round(2 * sx)))
        x, y, bw, bh = x + pad, y + pad, bw - 2 * pad, bh - 2 * pad
        px = crop(rows, ch, x, y, bw, bh)
        scrub_border(px, bw, bh, ch, False)
        if ch == 4:                                       # a portrait has no transparency
            px = [bytearray(b for i, b in enumerate(r) if i % 4 != 3) for r in px]
        px = resample(px, bw, bh, 3, EXPR_OUT_W, EXPR_OUT_H)
        out = ROOT.parent / s["out"]
        ship_png(out, EXPR_OUT_W, EXPR_OUT_H, 3, px)
        print(f"  slot {s['n']} {s['expr']}: {bw}x{bh} -> {out.relative_to(ROOT.parent)}  "
              f"{EXPR_OUT_W}x{EXPR_OUT_H}  (ships as portrait_{out.stem}.png)")
        sig = expr_signature(px, EXPR_OUT_W, EXPR_OUT_H)
        chans = [sig[k::3] for k in range(3)]             # flat per channel, not flat on average
        spread = max(sum(abs(v - sum(c) / len(c)) for v in c) / len(c) for c in chans)
        if spread < EXPR_FLAT:
            LOUD.append(f"slot {s['n']} ({s['expr']}) came back flat — nothing was drawn in it")
            print(f"WARNING: {LOUD[-1]}", file=sys.stderr)
        for other, osig in sigs:
            if sum(abs(a - b) for a, b in zip(sig, osig)) / len(sig) < EXPR_SAME:
                LOUD.append(f"slot {s['n']} ({s['expr']}) is the same drawing as slot {other['n']} "
                            f"({other['expr']}); ask for that one to be redrawn")
                print(f"WARNING: {LOUD[-1]}", file=sys.stderr)
                break
        sigs.append((s, sig))
        if s["expr"] == "neutral":
            main = px
    if neutral_main and main is not None:
        who = data.get("who") or slots[0]["who"]
        out = PORTRAITS / f"{who}.png"
        ship_png(out, EXPR_OUT_W, EXPR_OUT_H, 3, main)
        print(f"  --neutral-main: {out.relative_to(ROOT.parent)} replaced from slot 1")
    else:
        print("  the main portrait is still the one cut from the reference sheet "
              "(pass --neutral-main to replace it with slot 1)")
