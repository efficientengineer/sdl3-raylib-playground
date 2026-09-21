"""The `expressions` command: ten numbered head-and-shoulders slots.

The ids and their ORDER are the contract in rules.py — the slot number on the template is
the index into that list and the file name is the id. A dialogue line picks one with
`- Bron (biglaugh): text`.

Must never do: reorder or rename an expression id; sheets already drawn are cut by slot
number.
Public: layout_expressions, cmd_expressions. Imports: canvas, cast, field, field2,
manifest, md, package_io, paths, rules, style.
"""
import json
import sys
from .canvas import inner_box
from .cast import load_cast, resolve_name
from .field import SLOT_MARGIN, TEMPLATE_CANVASES
from .field2 import TEMPLATE_RULES, field_after, field_status, palette_attachment, write_field_package
from .manifest import manifest_path
from .md import die, existing
from .package_io import pkg_path
from .paths import ROOT, expr_portrait_path
from .rules import EXPRESSIONS, EXPR_ASPECT, EXPR_COLS, EXPR_OUT_H, EXPR_OUT_W, EXPR_ROWS
from .style import load_style



EXPR_BG = (40, 40, 44)                # the canvas behind the slots: dark, so a wrong file is caught


EXPR_GREY = (128, 128, 132)           # inside a slot: the same flat neutral mid-grey as the refsheet


EXPR_MARGIN = SLOT_MARGIN             # the margin check samples inside this, so do not shrink it


EXPR_GUTTER_X, EXPR_GUTTER_Y = 24, 44 # the vertical gutter holds the slot number (5*DIGIT_SCALE + gap)


def layout_expressions():
    """(W, H, label, boxes) for EXPR_ROWS x EXPR_COLS portrait slots, as big as the canvas allows."""
    W, H, label = TEMPLATE_CANVASES[0]                    # landscape: 5 across reads as two rows of five
    sw = (W - 2 * EXPR_MARGIN - (EXPR_COLS - 1) * EXPR_GUTTER_X) // EXPR_COLS
    sh_fit = (H - 2 * EXPR_MARGIN - (EXPR_ROWS - 1) * EXPR_GUTTER_Y) // EXPR_ROWS
    w = min(sw, int(sh_fit * EXPR_ASPECT))
    h = int(round(w / EXPR_ASPECT))
    gw = EXPR_COLS * w + (EXPR_COLS - 1) * EXPR_GUTTER_X
    gh = EXPR_ROWS * h + (EXPR_ROWS - 1) * EXPR_GUTTER_Y
    ox, oy = (W - gw) // 2, (H - gh) // 2
    boxes = [(ox + c * (w + EXPR_GUTTER_X), oy + r * (h + EXPR_GUTTER_Y), w, h)
             for r in range(EXPR_ROWS) for c in range(EXPR_COLS)]
    return W, H, label, boxes


def cmd_expressions(args):
    """One expression sheet a character: ten dialogue-box portraits of the same head.

    Template-drawn like the walker and the prop sheets — the tool owns the slots, ChatGPT fills them
    — because the ten faces have to be the same shot ten times, and a generator left to lay the page
    out itself returns ten different crops. The reference sheet is REQUIRED: the portrait beside the
    dialogue box is cut from it, and a face that does not match it is a different character."""
    names = [a for a in args if not a.startswith("--")]
    if not names:
        die("usage: expressions <Name> [<Name> ...]   (one sheet a character, from characters.md)")
    style, cast, warn = load_style(), load_cast(), []
    b = style["blocks"]
    made = []
    for written in names:
        handle = resolve_name(written, cast)
        if not handle:
            die(f"'{written}' is not a name in characters.md (a '## handle' or an '- alias:'). An "
                f"expression sheet is a cast member's face; a one-off NPC has no portrait at all.")
        c = cast[handle]
        rp = existing(c.get("ref"))
        if not rp:
            die(f"{c['display']} has no reference sheet at {c.get('ref')}, so the expression sheet "
                f"would not match the portrait. Generate it first: ./story_prompt.py refsheet {c['display']}")
        W, H, label, boxes = layout_expressions()
        slots = []
        for n, ((eid, _), box) in enumerate(zip(EXPRESSIONS, boxes), 1):
            slots.append({"n": n, "id": f"{handle}_{eid}", "who": handle, "expr": eid,
                          "box": list(box), "inner": list(inner_box(box)),
                          "target": [EXPR_OUT_W, EXPR_OUT_H],
                          "out": str(expr_portrait_path(handle, eid).relative_to(ROOT.parent))})
        name = f"expressions_{handle}"
        # The template, then the CUTSCENE style reference (a portrait belongs to the cutscene look,
        # not the map look, so the field's own style sheet is deliberately not attached), then the
        # character, then the palette last as everywhere else.
        attach = [(pkg_path(name, "template.png"),
                   "TEMPLATE. Redraw this exact image with every numbered slot filled in and "
                   "everything else left untouched. It is the canvas, not a reference.")]
        sp = existing(style["refs"].get("style"))
        if sp:
            attach.append((sp, "STYLE reference. " + style["refs"].get("style_note", "")))
        else:
            warn.append(f"no style reference at {style['refs'].get('style')}; "
                        f"the prompt text carries the style alone")
        attach.append((rp, f"CHARACTER reference for {c['display']}. Every slot is this character: keep "
                           f"the face, hair, colours, collar and marks identical to the middle panel of "
                           f"this sheet. Use it for the design only — ignore its background, its "
                           f"three-panel layout and its neutral expression, which is only slot 1."))
        pa = palette_attachment(warn)
        if pa:
            attach.append(pa)
        fw, fh = slots[0]["inner"][2], slots[0]["inner"][3]
        L = [f"Create ONE image: the attached template with all {len(slots)} numbered slots filled in. "
             f"Canvas: {label}, the same size as the template.", b["header"], "",
             "ATTACHED REFERENCE IMAGES, in the order I attached them:"]
        L += [f"Image {i}: {note}" for i, (_, note) in enumerate(attach, 1)]
        L += ["", TEMPLATE_RULES, "",
              f"WHAT THIS IS: the dialogue-box faces for {c['display']}. The game draws one of these "
              f"beside the text box while {c['display']} is speaking, so all ten have to read as the "
              f"same person in the same shot with a different feeling.", "",
              f"CHARACTER: {c['look']}", "",
              f"SHEET: {b['expressions']}", "",
              f"THE TEN SLOTS, in this order:"]
        for n, (eid, how) in enumerate(EXPRESSIONS, 1):
            L.append(f"  Slot {n} ({eid}): {how}.")
        L += ["",
              f"FRAMING, identical in every slot: three-quarter view facing slightly to the viewer's "
              f"left, head and the top of the shoulders only, the top of the head a little below the "
              f"top border and the chin around two thirds down the slot, the head the same size and "
              f"in the same place in all ten. Each slot is {fw}x{fh} pixels here and is stored at "
              f"{EXPR_OUT_W}x{EXPR_OUT_H}, so keep the pixels large and the face readable at a glance: "
              f"the feeling has to be clear from the brows and the mouth alone.", "",
              "NOTHING IS HELD AND NOTHING IS CARRIED: no hands, no arms raised into frame, no "
              "weapon, no sword, staff, bow or knife, no props, no objects, no cartoon symbols, no "
              "effect lines. Only the head and shoulders against the flat grey.", "",
              f"CHARACTER DESIGN: {b['character_design']}", "", f"RENDERING: {b['rendering']}", "",
              f"AVOID: {b['expressions_avoid']}, drawing outside a slot, moving or covering a slot "
              f"number, painting over the grey background, changing the size of the image"]
        after = field_after(manifest_path(name), [
            f"- [ ] All {len(slots)} slots filled, every border and number still exactly where it was",
            "- [ ] The same head, the same size, in the same place, in all ten slots",
            "- [ ] Hair, colours, collar and marks match the reference sheet in all ten",
            "- [ ] Each slot's feeling is the one listed for that number, and no two slots are the same face",
            "- [ ] No hands, no weapons, no props, nothing held; flat grey behind every head", "",
            "If one face fails, reply in the same chat: \"Redraw only slot 6 and keep every other slot "
            "and the whole template exactly as it is. <what was wrong>\"."],
            makes=", ".join(f"`{s['out']}`" for s in slots))
        template, manifest, md = write_field_package(
            "expressions", name, label, W, H, EXPR_BG, slots, "\n".join(L), attach, after,
            status=field_status("expressions", slots), fill=EXPR_GREY)
        data = json.loads(manifest.read_text())
        data["who"] = handle
        manifest.write_text(json.dumps(data, indent=2) + "\n")
        made.append((c["display"], template, md, f"{len(slots)} faces at {fw}x{fh}, {label}"))
    for w in warn:
        print(f"warning: {w}", file=sys.stderr)
    for who, template, md, what in made:
        print(f"wrote {template.relative_to(ROOT.parent)}  ({who}: {what})")
        print(f"      {md.relative_to(ROOT.parent)}  — attach the template first, then paste the prompt")
