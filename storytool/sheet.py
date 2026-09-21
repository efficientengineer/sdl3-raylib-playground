"""The shot-sheet and reference-sheet package builders.

cmd_sheet turns one or more panel scenes into story/out/<name>.chatgpt.md plus the
sheet.json the slicer later cuts by; cmd_refsheet does a character's three-panel
reference sheet, whose middle panel becomes the dialogue portrait.

Must never do: hand-write style text — every block comes from style.py verbatim.
Public: cmd_sheet, cmd_refsheet. Imports: cast, field2, layout, md, package_io, paths,
rules, scenes, style.
"""
import json
import re
import sys
from .cast import load_cast
from .field2 import palette_attachment
from .layout import sheet_geometry
from .md import die, existing
from .package_io import attachments, package, pkg_path, return_lines
from .paths import OUT, PORTRAITS, ROOT
from .rules import SHEET_MAX_PANELS
from .scenes import acting_for, names_in, panel_file, panel_text, parse_scene, scene_context, scene_paths, validate
from .style import load_style



def cmd_sheet(args):
    style, cast = load_style(), load_cast()
    b, shots = style["blocks"], style["shots"]
    scenes, warn = [], []
    for path in scene_paths(args):
        scene = parse_scene(path)
        if scene["talk"]:
            die(f"{path.name} is a talk scene: it has no panels, so there is nothing to generate. Its only art is "
                f"the optional '- backdrop:' panel, which is generated with its own scene's sheet.")
        err, w = validate(scene, style, cast)
        warn += [f"{path.name}: {x}" for x in w]
        if err:
            for e in err:
                print(f"{path.name}: ERROR: {e}", file=sys.stderr)
            sys.exit("story_prompt: scene rejected. Fix the scene file; do not bypass the rules.")
        scenes.append(scene)

    flat = [(sc, n, sid, desc) for sc in scenes for n, (sid, desc) in enumerate(sc["panels"], 1)]
    if len(flat) > SHEET_MAX_PANELS:
        die(f"{len(flat)} panels on one sheet, max {SHEET_MAX_PANELS}. Split the scenes across sheets.")
    shapes = [shots[sid]["shape"] for _, _, sid, _ in flat]
    canvas, rects = sheet_geometry(shapes)
    name = scenes[0]["stem"] if len(scenes) == 1 else \
        "sheet_" + "-".join(re.match(r"\d+|\w+", sc["stem"]).group(0) for sc in scenes)

    present = []
    for _, _, _, desc in flat:
        present += [c for c in names_in(desc, cast) if c not in present]
    attach = attachments(style, cast, present, warn)

    L = [f"Create ONE image: a shot sheet of {len(flat)} separate comic panels. Canvas: {canvas}.",
         b["header"], ""]
    if attach:
        L.append("ATTACHED REFERENCE IMAGES, in the order I attached them:")
        L += [f"Image {n}: {note}" for n, (_, note) in enumerate(attach, 1)]
        L.append("")
    L += [f"SHEET LAYOUT: {b['sheet_layout']}", "",
          "PANEL SIZES AND POSITIONS, as percentages of the whole image (follow these closely):"]
    for i, (x, y, w, h) in enumerate(rects):
        L.append(f"Panel {i+1}: {style['sheet_shapes'][shapes[i]]}. Left edge at {x*100:.0f}%, right edge at "
                 f"{(x+w)*100:.0f}%, top edge at {y*100:.0f}%, bottom edge at {(y+h)*100:.0f}%.")
    gaps = []
    for i, a in enumerate(rects):                        # spell out the black gap between every pair that could collide
        for j in range(i + 1, len(rects)):
            b_ = rects[j]
            v_overlap = a[1] < b_[1] + b_[3] and b_[1] < a[1] + a[3]
            h_overlap = a[0] < b_[0] + b_[2] and b_[0] < a[0] + a[2]
            near = 0.12                                       # only true neighbours: facing edges this close
            if v_overlap:
                left, right = (i, j) if a[0] < b_[0] else (j, i)
                if rects[right][0] - (rects[left][0] + rects[left][2]) <= near:
                    gaps.append(f"between the right edge of panel {left+1} and the left edge of panel {right+1}")
            elif h_overlap:
                top, bottom = (i, j) if a[1] < b_[1] else (j, i)
                if rects[bottom][1] - (rects[top][1] + rects[top][3]) <= near:
                    gaps.append(f"between the bottom of panel {top+1} and the top of panel {bottom+1}")
    if gaps:
        L.append("A band of pure black must be clearly visible " + "; ".join(gaps) + ". If space is tight, draw the "
                 "panels smaller. No panel may cover any part of another panel, not even a corner.")
    L.append("")
    k, checks = 0, []
    for sc in scenes:
        first, last = k + 1, k + len(sc["panels"])
        span = f" for Panel {first}" if first == last else f" for Panels {first} to {last}"
        L += scene_context(sc, span)
        for (sid, desc), notes in zip(sc["panels"], acting_for(sc, cast)):
            k += 1
            L.append(f"Panel {k}: {panel_text(style, cast, sid, desc, notes)}")
            for c, note in notes.items():
                checks.append(f"- [ ] Panel {k}, {cast[c]['display']}: {note}")
        L.append("")
    if present:
        L.append("CHARACTERS, drawn identically in every panel they appear in:")
        L += [f"- {cast[c]['look']}" for c in present]
        L.append("")
    L += [b["acting"], "", f"CAMERA AND FRAMING inside each panel: {b['framing']}", "",
          f"CHARACTER DESIGN: {b['character_design']}", "",
          f"RENDERING: {b['rendering']}", "",
          f"AVOID: {b['sheet_avoid']}, {b['negative']}"]
    prompt = "\n".join(L)

    OUT.mkdir(exist_ok=True)
    manifest = pkg_path(name, "sheet.json")
    manifest.write_text(json.dumps({
        "sheet": name, "canvas": canvas, "rects": [[round(v, 4) for v in r] for r in rects],
        "panels": [{"n": i + 1, "scene": sc["stem"], "scene_panel": n, "shot": sid, "shape": shapes[i],
                    "content": desc} for i, (sc, n, sid, desc) in enumerate(flat)],
        "dialogue": {sc["stem"]: [{"speaker": w, "line": l} for w, l in sc["dialogue"]] for sc in scenes},
        "prompt": prompt,
    }, indent=2) + "\n")
    art = [f"- `{panel_file(sc, n, sid).relative_to(ROOT.parent)}` — "
           + ("**cut already**" if panel_file(sc, n, sid).exists() else "missing")
           for sc, n, sid, _ in flat]
    have = sum(1 for sc, n, sid, _ in flat if panel_file(sc, n, sid).exists())
    md = pkg_path(name, "chatgpt.md")
    md.write_text(package(f"shot sheet {name}", attach, prompt, [
        "Check the result against this list before accepting it (see 'Review checklist' in STYLE.md).",
        "Gaze and expression are the usual failures; reject on those even if the art is beautiful.", "",
        f"- [ ] {len(flat)} separate white-bordered panels on pure black, none touching, no labels or text",
        "- [ ] Every panel obeys the staging (same screen direction throughout)",
        *checks,
        "- [ ] Hair, outfits, colors, and marks match the reference sheets; hands and weapons look right", "",
        "If a panel fails, reply in the same chat, naming the panel and quoting the line it broke, for example:", "",
        "```",
        "Redraw only panel 3 and keep every other panel exactly as it is. In panel 3 both characters must look",
        "at the same spot, off-panel to the right, at the guard. <paste the failed acting lines here>",
        "```", "",
        *(return_lines(f"the {len(flat)} panel files listed above") or [
            "When it passes, download the image and cut it into panels:",
            "", "```", f"./story_prompt.py slice {manifest.relative_to(ROOT.parent)} ~/Downloads/<file>.png",
            "```"])],
        status=[f"{have} of {len(flat)} panels have been cut into `story/panels/` already.", ""] + art))
    for w in warn:
        print(f"warning: {w}", file=sys.stderr)
    print(f"wrote {md.relative_to(ROOT.parent)}  ({len(flat)} panels, {canvas}, {len(attach)} attachment(s))")


def cmd_refsheet(args):
    style, cast = load_style(), load_cast()
    if not args or args[0].lower() not in cast:
        die(f"usage: refsheet <Name>. Known: {', '.join(c['name'] for c in cast.values())}")
    c, b, warn = cast[args[0].lower()], style["blocks"], []
    attach = attachments(style, cast, [], warn)
    pa = palette_attachment(warn)                        # D19: the sheet's colours are the game's
    if pa:
        attach.append(pa)
    L = [f"Create ONE image: a character reference sheet for {c['name']}. Canvas: landscape, 1536x1024.",
         b["header"], ""]
    if attach:
        L += ["ATTACHED REFERENCE IMAGES, in the order I attached them:"]
        L += [f"Image {n}: {note}" for n, (_, note) in enumerate(attach, 1)] + [""]
    L += [f"SHEET: {b['refsheet']}", "", f"CHARACTER: {c['look']}", "",
          f"CHARACTER DESIGN: {b['character_design']}", "", f"RENDERING: {b['rendering']}", "",
          f"AVOID: {b['refsheet_avoid']}"]
    target = c.get("ref", f"story/refs/{c['name'].lower()}.png")
    handle = args[0].lower()
    OUT.mkdir(exist_ok=True)
    md = pkg_path(f"ref_{c['name'].lower()}", "chatgpt.md")
    ref_now, por_now = existing(target), PORTRAITS / f"{handle}.png"
    md.write_text(package(f"reference sheet for {c['name']}", attach, "\n".join(L), [
        f"- Regenerate until you like the design. This image becomes {c['name']}'s look in every scene,",
        f"  the head-and-shoulders portrait beside the dialogue box, and the walk sprite's design.",
        *(return_lines(f"`{target}` and the dialogue-box portrait cut out of it") or [
            f"- Save it as `{target}`. Every later `sheet` package attaches it automatically."]),
        "", f"- If the design differs from the `look` line in characters.md, update the `look` line to match."],
        status=[f"- reference sheet `{target}` — " + ("**exists**" if ref_now else "missing"),
                f"- dialogue portrait `{por_now.relative_to(ROOT.parent)}` — "
                + ("**exists**" if por_now.exists() else "missing")]))
    for w in warn:
        print(f"warning: {w}", file=sys.stderr)
    print(f"wrote {md.relative_to(ROOT.parent)}  (save the result as {target})")
