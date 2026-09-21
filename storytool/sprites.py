"""The `sprites` command: the voxel world's billboards (WORLD.md).

Slots are sized by each entry's `- footprint: WxH` in story/field/sprites.md at 64 px to
the cell, one slot a frame. An id with no entry is an error; an unknown footprint gets
1x1 and a warning.

Must never do: cut — that is sprites_cut.py.
Public: sprite_frames, sprite_meta_path, cmd_sprites. Imports: canvas, field, field2,
manifest, md, package_io, paths, style.
"""
from .canvas import inner_box, layout_cells
from .field import CELL_PX, MAGENTA, SPRITE_DEFAULT_FPS, SPRITE_LOOPS, SPRITE_MAX_FRAMES, check_description, field_entries
from .field2 import TEMPLATE_RULES, field_after, field_attachments, field_rendering, field_status, write_field_package
from .manifest import dedupe_ids, footprint_of, kind_rows, manifest_path, package_name, report_field
from .md import die
from .package_io import package_label, pkg_path
from .paths import FIELD_DIRS, FIELD_DOCS, ROOT
from .style import load_style



def sprite_frames(i, entry, warn):
    """(frames, fps, loop) for a sprite entry. A still is one frame and gets no sidecar json."""
    raw = (entry.get("frames") or "1").strip()
    if not raw.isdigit() or not 1 <= int(raw) <= SPRITE_MAX_FRAMES:
        die(f"story/field/sprites.md: '## {i}' has '- frames: {raw}'; it must be 1 to {SPRITE_MAX_FRAMES}")
    n = int(raw)
    loop = (entry.get("loop") or SPRITE_LOOPS[0]).strip().lower()
    if loop not in SPRITE_LOOPS:
        die(f"story/field/sprites.md: '## {i}' has '- loop: {loop}'; it must be "
            f"{' or '.join(SPRITE_LOOPS)}")
    fps = (entry.get("fps") or str(SPRITE_DEFAULT_FPS)).strip()
    if not fps.isdigit() or not 1 <= int(fps) <= 30:
        die(f"story/field/sprites.md: '## {i}' has '- fps: {fps}'; it must be 1 to 30")
    for f in range(2, n + 1):
        if not (entry.get(f"frame{f}") or "").strip():
            warn.append(f"'{i}' asks for {n} frames but has no '- frame{f}:' line saying what moves; "
                        f"the prompt will describe frame {f} exactly like frame 1")
    if n == 1 and (entry.get("loop") or entry.get("fps")):
        warn.append(f"'{i}' is a single frame, so its '- loop:'/'- fps:' lines do nothing")
    return n, int(fps), loop


def sprite_meta_path(ident):
    return FIELD_DIRS["sprite"] / f"{ident}.json"


def cmd_sprites(args):
    """The voxel world's billboards: creatures, animals and the machines, one slot per frame.

    The same magenta-keyed template machinery the old `props` command used, with two differences:
    a sprite may be several frames, and the frames of one sprite are cropped to ONE shared box so
    they stay registered when the engine flips between them."""
    ids = [a.lower() for a in args if not a.startswith("--")]
    if not ids:
        die("usage: sprites <id> [<id>...]   (ids come from story/field/sprites.md)")
    entries, style, warn = field_entries(FIELD_DOCS["sprite"], "sprite"), load_style(), []
    ids = dedupe_ids(ids, warn)
    missing = [i for i in ids if i not in entries]
    if missing:
        die(f"no entry in story/field/sprites.md for: {', '.join(missing)}. Add a '## <id>' with a "
            f"one-line description and a '- footprint: WxH' line.")
    cells, spec = [], {}
    for i in ids:
        if not entries[i]["desc"]:
            die(f"story/field/sprites.md: '## {i}' has no description line")
        check_description(entries[i]["desc"], i, "story/field/sprites.md")
        fp = footprint_of(i, entries[i], warn)
        n, fps, loop = sprite_frames(i, entries[i], warn)
        spec[i] = {"footprint": fp, "frames": n, "fps": fps, "loop": loop}
        cells += [fp] * n                            # one slot a frame, so every frame is drawn big

    W, H, label, u, boxes = layout_cells(cells, ids)
    slots, k = [], 0
    for i in ids:
        s = spec[i]
        for f in range(s["frames"]):
            k += 1
            slots.append({"n": k, "id": i, "frame": f, "frames": s["frames"],
                          "cells": list(s["footprint"]), "box": list(boxes[k - 1]),
                          "inner": list(inner_box(boxes[k - 1])),
                          "target": [s["footprint"][0] * CELL_PX, s["footprint"][1] * CELL_PX],
                          "fps": s["fps"], "loop": s["loop"],
                          "out": str((FIELD_DIRS["sprite"] / f"{i}.png").relative_to(ROOT.parent))})
    name = package_name("sprites", ids)
    b = style["blocks"]
    attach = field_attachments(style, pkg_path(name, "template.png"), None, warn)
    anim = [i for i in ids if spec[i]["frames"] > 1]
    L = [f"Create ONE image: the attached template with all {len(slots)} numbered slots filled in. "
         f"Canvas: {label}, the same size as the template.", b["header"], "",
         "ATTACHED REFERENCE IMAGES, in the order I attached them:"]
    L += [f"Image {n}: {note}" for n, (_, note) in enumerate(attach, 1)]
    L += ["", TEMPLATE_RULES, "",
          "WHAT THESE ARE: single living things and single objects for a game world, one per slot, "
          "each cut out against the flat magenta. The magenta is not a backdrop, it is empty space: "
          "it runs right up to the edge of the subject on every side. No ground, no grass, no "
          "scenery, no base plate, no cast shadow, no glow, and never a second subject in a slot.", "",
          "CAMERA, the same for every slot: seen from the front and very slightly above, at eye "
          "level with the thing itself, as if all of these stood in one place and one camera looked "
          "at them straight on. The subject sits upright, centred left to right, and its lowest "
          "point touches the bottom edge of its slot, because that line is where it meets the "
          "ground in game.", "",
          f"SCALE: a slot is a grid of map cells, {CELL_PX} pixels to the cell in game, and each slot "
          f"below says how many cells it is. Everything shares one scale across the sheet: a "
          f"three-cell animal is three times the width of a one-cell one and is drawn with the same "
          f"size of pixel.", ""]
    if anim:
        L += ["FRAMES: some of these are short animations, drawn as two or three slots in a row. "
              "Every frame of one animation is THE SAME SUBJECT drawn again at the same size, from "
              "the same camera, in the same place in its slot, with only the moving part changed. "
              "Do not redesign it, do not recolour it, and do not move it up, down or sideways "
              "between frames: the game flips between them on one spot and any drift reads as the "
              "whole thing jumping.", ""]
    L.append("SLOTS:")
    for s in slots:
        cw, chh = s["cells"]
        of = (f", frame {s['frame'] + 1} of {s['frames']}" if s["frames"] > 1 else "")
        # Frame 1 is the entry's own description; frames 2 and 3 take a '- frame2:'/'- frame3:' line
        # saying what MOVES. Without that the prompt repeats itself word for word and comes back as
        # the same picture drawn twice, which is not an animation.
        move = entries[s["id"]].get(f"frame{s['frame'] + 1}", "").strip()
        note = (f" In this frame, and changing nothing else about it: {move.rstrip('.')}."
                if move else "")
        L.append(f"Slot {s['n']} ({s['id']}{of}), {cw} x {chh} cells, {s['inner'][2]}x{s['inner'][3]} px "
                 f"in the template: {entries[s['id']]['desc'].rstrip('.')}.{note} Seen from the front, "
                 f"filling the slot and standing on its bottom edge, magenta on every other side.")
    L += ["", f"RENDERING: {field_rendering(b)}", "",
          f"AVOID: {b['negative']}, drawing outside a slot, moving or covering a slot number, ground "
          f"or grass under a subject, a cast shadow on the magenta, a base plate or pedestal, a "
          f"scene or background inside a slot, two subjects in one slot, a soft blurred or glowing "
          f"edge where a subject meets the magenta, changing the size of the image"]
    prompt = "\n".join(L)
    check = [f"- [ ] All {len(slots)} slots filled, every border and number still exactly where it was",
             "- [ ] The magenta is untouched outside the slots, and comes right up to each subject",
             "- [ ] The edge of every subject is hard against the magenta, not soft, blurred or glowing",
             "- [ ] No ground, shadow, base plate or scenery under or behind a subject",
             "- [ ] Everything stands on the bottom edge of its slot and shares one camera angle",
             "- [ ] One scale across the sheet: the big animal really is bigger than the small one"]
    if anim:
        check.append("- [ ] Each animation's frames are the same subject at the same size in the same "
                     "spot, with only the moving part different")
    check += ["", "If one sprite fails, reply in the same chat: \"Redraw only slot 2 and keep every "
              "other slot and the whole template exactly as it is. <what was wrong>\"."]
    template, manifest, md = write_field_package(
        "sprites", name, label, W, H, MAGENTA, slots, prompt, attach,
        field_after(manifest_path(name), check, makes=f"the {len(ids)} sprite(s) listed above"),
        status=field_status("sprites", slots))
    report_field(kind_rows("sprite", slots, package_label(name)), warn, md, template,
                 f"{len(ids)} sprites in {len(slots)} slots, {label}")
