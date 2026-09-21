"""The `walker` command: the 9-frame walk sheet package.

The look comes from characters.md verbatim and the character's reference sheet is
attached, so the sprite matches the portrait; a name with no reference sheet is an error,
not a warning. Up to three characters on one sheet. story/field/walkers.md is only for
NPCs with no cast entry at all.

Must never do: rename a handle. A story name that has moved on is an `- alias:` line.
Public: layout_walker, cmd_walker. Imports: canvas, cast, field, field2, manifest, md,
package_io, paths, style, walkers_cut.
"""
import json
from .canvas import inner_box
from .cast import load_cast, resolve_name
from .field import DIGIT_GAP, DIGIT_SCALE, MAGENTA, SLOT_MARGIN, TEMPLATE_CANVASES, WALK_H, WALK_MAX_PER_SHEET, WALK_MIN_SCALE, WALK_OUT_H, WALK_OUT_W, WALK_ROWS, WALK_ROWS_ASYM, WALK_STEPS, WALK_W, check_description, field_entries, field_look
from .field2 import TEMPLATE_RULES, field_after, field_attachments, field_rendering, field_status, write_field_package
from .manifest import manifest_path, report_field
from .md import die, existing
from .package_io import package_label, pkg_path
from .paths import FIELD_DIRS, FIELD_DOCS, ROOT
from .style import load_style
from .walkers_cut import cmd_walker_compact, walker_ident



def layout_walker(blocks):
    """The frame grid for `blocks` = [rows per character]. Three columns a character, side by side.

    Nine frames instead of sixteen (owner, 2026-09-19) and a frame stored at 128x192 instead of
    256x384 (WORLD.md, "Sizes") means a walk sheet is 384x576 — small enough that several
    characters fit one generation. They are laid out SIDE BY SIDE, each character its own block of
    three columns, because stacking them runs out of canvas height at two."""
    gutter, gap = 20, 40                  # across; the numbers sit in the gap ABOVE a slot, so the
    gy = DIGIT_SCALE * 5 + DIGIT_GAP      # vertical gutter has to clear a digit's height
    cols_total = 3 * len(blocks)
    rows_max = max(blocks)
    best = None
    for W, H, label in TEMPLATE_CANVASES:
        sw = ((W - 2 * SLOT_MARGIN - (cols_total - len(blocks)) * gutter - (len(blocks) - 1) * gap)
              // (cols_total * WALK_W))
        sh = (H - 2 * SLOT_MARGIN - (rows_max - 1) * gy) // (rows_max * WALK_H)
        s = min(sw, sh)
        if s >= WALK_MIN_SCALE and (best is None or s > best[3]):
            best = (W, H, label, s)
    if best is None:
        die(f"{len(blocks)} character(s) of up to {rows_max} rows x 3 frames at {WALK_MIN_SCALE}x do "
            f"not fit any template canvas. Ask for fewer characters on one sheet.")
    W, H, label, s = best
    fw, fh = WALK_W * s, WALK_H * s
    gw = cols_total * fw + (cols_total - len(blocks)) * gutter + (len(blocks) - 1) * gap
    gh = rows_max * fh + (rows_max - 1) * gy
    ox, oy = (W - gw) // 2, (H - gh) // 2
    boxes, x = [], ox
    for n in blocks:
        for r in range(n):
            for c in range(3):
                boxes.append((x + c * (fw + gutter), oy + r * (fh + gy), fw, fh))
        x += 3 * fw + 2 * gutter + gap
    # the slots of one character are numbered down its own block, which is the order cmd_walker
    # builds them in: rows outer, columns inner.
    return W, H, label, s, boxes


def cmd_walker(args):
    if args and args[0] == "compact":
        return cmd_walker_compact(args[1:])
    names = [a for a in args if not a.startswith("--")]
    if not names:
        die("usage: walker <Name> [<Name> ...]   (characters from characters.md, or ids from "
            "story/field/walkers.md)\n       walker compact <name> [...]   (an old 4x4 sheet -> 9 frames)")
    if len(names) > WALK_MAX_PER_SHEET:
        die(f"{len(names)} characters on one sheet. The cap is {WALK_MAX_PER_SHEET}: each one needs "
            f"its reference sheet attached, and past three attachments ChatGPT starts blending the "
            f"designs. Split them into two packages.")
    style, cast, warn = load_style(), load_cast(), []
    who = []
    for written in names:
        ident = walker_ident(written)
        handle = resolve_name(written, cast)
        if handle:
            c = cast[handle]
            rp = existing(c.get("ref"))
            if not rp:
                die(f"{c['name']} has no reference sheet at {c.get('ref')}, so a walk sheet would not "
                    f"match the portrait. Generate it first: ./story_prompt.py refsheet {c['name']}")
            asym = str(c.get("asymmetric", "")).lower().startswith("y")
            who.append({"ident": ident, "name": c["name"], "look": c["look"],
                        "ref": (rp, f"CHARACTER reference for {c['name']}. The walker is this "
                                    f"character: keep the face, hair, outfit and colours identical to "
                                    f"this image in every frame. Use it for the design only; ignore "
                                    f"its background and its three-panel layout."),
                        "rows": WALK_ROWS_ASYM if asym else WALK_ROWS})
        else:
            others = field_entries(FIELD_DOCS["walker"], "walker")
            if ident not in others:
                die(f"'{written}' is neither a name in characters.md (a handle or an '- alias:') nor "
                    f"an id in story/field/walkers.md. A story name that has moved on (see "
                    f"story/v3/NAMES.md) belongs on an '- alias:' line of its existing entry, not on "
                    f"a renamed heading; a one-off NPC belongs in story/field/walkers.md.")
            look = field_look(others[ident], ident, "story/field/walkers.md")
            if not look:
                die(f"story/field/walkers.md: '## {ident}' has no '- look:' line")
            check_description(look, ident, "story/field/walkers.md")
            warn.append(f"{ident} has no reference sheet; the look line carries the design alone")
            who.append({"ident": ident, "name": ident, "look": look, "ref": None, "rows": WALK_ROWS})

    W, H, label, s, boxes = layout_walker([len(c["rows"]) for c in who])
    slots, n = [], 0
    for c in who:
        for r, facing in enumerate(c["rows"]):
            for k, step in enumerate(WALK_STEPS):
                box = boxes[n]
                n += 1
                slots.append({"n": n, "id": f"{c['ident']}_{facing.lower()}{k + 1}",
                              "who": c["ident"], "row": r, "col": k, "facing": facing, "step": step,
                              "box": list(box), "inner": list(inner_box(box)),
                              "target": [WALK_OUT_W, WALK_OUT_H],
                              "out": str((FIELD_DIRS["walker"] / f"{c['ident']}.png")
                                         .relative_to(ROOT.parent))})
    name = "walker_" + "_".join(c["ident"] for c in who)
    b = style["blocks"]
    attach = field_attachments(style, pkg_path(name, "template.png"), None, warn)
    for c in who:                                        # every character's sheet, before the palette
        if c["ref"]:
            attach.insert(len(attach) - 1, c["ref"])
    fw, fh = slots[0]["inner"][2], slots[0]["inner"][3]
    subject = who[0]["name"] if len(who) == 1 else f"{len(who)} characters"
    L = [f"Create ONE image: the attached template with all {len(slots)} numbered slots filled in. "
         f"Canvas: {label}, the same size as the template.", b["header"], "",
         "ATTACHED REFERENCE IMAGES, in the order I attached them:"]
    L += [f"Image {i}: {note}" for i, (_, note) in enumerate(attach, 1)]
    L += ["", TEMPLATE_RULES, "",
          f"WHAT THIS IS: walking sprite sheets for {subject} in a top-down field map, cut out "
          f"against the flat magenta. The magenta is empty space, not a backdrop: it runs right up "
          f"to the figure on every side. No ground, no shadow, no scenery, no props that are not "
          f"part of the costume.", ""]
    at = 1
    for c in who:
        rows = c["rows"]
        first, last = at, at + 3 * len(rows) - 1
        L.append(f"SLOTS {first}-{last} ARE {c['name'].upper()}. {c['look']}")
        for r, facing in enumerate(rows):
            lo = at + r * 3
            if facing == "S":
                what = "walks toward the camera, seen from the front"
            elif facing == "N":
                what = "walks away from the camera, seen from behind"
            elif facing == "side":
                what = ("walks to the viewer's LEFT, seen in full side view from their right side. "
                        "There is only one side row: the game mirrors it for the other direction, so "
                        "draw the character so that mirroring it would still be right — nothing that "
                        "belongs on one particular side of the body")
            else:
                what = (f"walks to the viewer's {'left' if facing == 'W' else 'right'}, seen in full "
                        f"side view")
            L.append(f"  Slots {lo}-{lo + 2}, facing {facing}: {what}.")
        at = last + 1
        L.append("")
    L += ["THE THREE COLUMNS, the same in every row: column 1 is standing still with both feet "
          "together and arms at rest; column 2 is mid-stride with the LEFT leg forward and the right "
          "arm forward; column 3 is mid-stride with the RIGHT leg forward and the left arm forward. "
          "The game plays them 1, 2, 1, 3 in a loop, so column 1 has to read as the resting pose "
          "between the two strides.", "",
          f"FRAMING, the same in every frame on the sheet: the character is drawn at the same size, "
          f"upright and centred left to right, with the feet on the bottom edge of the frame and a "
          f"small gap of magenta above the head. The head does not move up or down between frames, "
          f"so the sheet does not bob when it is played. Each frame is {fw}x{fh} pixels here and is "
          f"stored at {WALK_OUT_W}x{WALK_OUT_H}, so keep the pixels large, the silhouette clear and "
          f"the face simple: a few pixels of eye, no fine detail.", ""]
    if any(c["ref"] for c in who):
        L += ["Each character must match their own attached reference sheet exactly: same face, hair, "
              "outfit, colours and marks, in every one of their frames. Two characters on this sheet "
              "must not borrow each other's clothes or hair.", ""]
    L += [f"CHARACTER DESIGN: {b['character_design']}", "", f"RENDERING: {field_rendering(b)}", "",
          f"AVOID: {b['negative']}, drawing outside a slot, moving or covering a slot number, ground "
          f"or shadow under the feet, a different size or costume between frames, the head bobbing "
          f"between frames, a background inside a frame, a soft blurred or glowing edge where the "
          f"figure meets the magenta, changing the size of the image"]
    outs = [str((FIELD_DIRS["walker"] / f"{c['ident']}.png").relative_to(ROOT.parent)) for c in who]
    after = field_after(manifest_path(name), [
        f"- [ ] All {len(slots)} slots filled, every border and number still exactly where it was",
        "- [ ] Columns stand, step-A, step-B; column 1 is the resting pose in every row",
        "- [ ] The head sits at the same height in every frame of a character",
        "- [ ] Feet on the bottom edge, magenta right up to the figure, no shadow and no ground",
        "- [ ] Hair, outfit, colours and marks match each character's own reference sheet", "",
        "If one row fails, reply in the same chat: \"Redraw only slots 4 to 6 and keep every other "
        "slot and the whole template exactly as it is. <what was wrong>\"."],
        makes=", ".join(f"`{o}`" for o in outs))
    template, manifest, md = write_field_package(
        "walker", name, label, W, H, MAGENTA, slots, "\n".join(L), attach, after,
        out_file=outs[0], status=field_status("walker", slots, outs[0]))
    data = json.loads(manifest.read_text())
    data["who"] = [{"id": c["ident"], "rows": list(c["rows"])} for c in who]
    manifest.write_text(json.dumps(data, indent=2) + "\n")
    report_field([{"id": c["ident"], "kind": "walker",
                   "target": f"{WALK_OUT_W * 3}x{WALK_OUT_H * len(c['rows'])}",
                   "package": package_label(name)} for c in who],
                 warn, md, template, f"{len(slots)} frames at {s}x, {label}")
