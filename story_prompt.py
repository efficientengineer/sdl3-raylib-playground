#!/usr/bin/env python3
"""Story scene -> image prompt builder. Enforces story/STYLE.md.

  ./story_prompt.py shots                      list the shot menu
  ./story_prompt.py check  story/scenes/*.md   validate scenes against the rules
  ./story_prompt.py brief  ["next beat"]       print a brief for an LLM to write the next scene file

  ChatGPT path (reference images, one shot sheet per generation, sliced into panels):
  ./story_prompt.py refsheet Bron              write story/out/ref_bron.chatgpt.md (character reference sheet)
  ./story_prompt.py sheet  story/scenes/001_x.md [more scenes]
                                               write story/out/<name>.chatgpt.md + .sheet.json (max 6 panels)
  ./story_prompt.py slice  story/out/<name>.sheet.json downloaded.png [--trim N] [--boxes "x,y,w,h;..."]
                                               cut the sheet into story/panels/<scene>_pN_<shot>.png

  ./story_prompt.py preview story/scenes/003_x.md
                                               write story/out/<scene>.preview.html: panels layered manga-style and
                                               revealed line by line with a dialogue box (placeholders if not sliced yet)

  Single finished page (generators without reference images):
  ./story_prompt.py build  story/scenes/001_x.md | --all    write story/out/001_x.prompt.txt + .json

Style text, shots, and shapes are parsed from story/STYLE.md; character looks from
story/characters.md. Nothing stylistic is hard-coded here except the rule thresholds
below, which mirror the "Composition rules" section of STYLE.md. Keep the two in sync.
Stdlib only.
"""
import json
import re
import struct
import subprocess
import sys
import zlib
from pathlib import Path

ROOT = Path(__file__).resolve().parent / "story"
STYLE, CAST, PLOT = ROOT / "STYLE.md", ROOT / "characters.md", ROOT / "plot.md"
SCENES, OUT, PANELS = ROOT / "scenes", ROOT / "out", ROOT / "panels"

# ── Rule thresholds (mirror STYLE.md "Composition rules") ──
PANELS_MIN, PANELS_MAX = 2, 4                       # R1
ANCHOR_SCALES = {"wide", "full", "medium"}          # R5
PUNCH_SCALES = {"close", "extreme", "insert"}       # R5
MAX_PANEL_WORDS = 40                                # R7
STYLE_WORDS = ["gradient", "photorealistic", "3d", "blur", "glow", "painterly",
               "realistic", "hd", "4k", "smooth"]   # R8
TEXT_WORDS = ["text", "caption", "speech bubble", "lettering", "written", "inscription reading",
              "sign saying", "sign that says", "sign reading", "the words", "subtitle"]  # R9
LOOK_BANNED = ["dwarf", "dwarven", "halfling", "hobbit", "elf", "elven", "gnome", "orc", "fighter",
               "rogue", "cleric", "wizard", "ranger", "barbarian", "paladin", "beard", "bearded"]  # see characters.md
REQUIRED_BLOCKS = ["header", "layout", "framing", "character_design", "rendering",
                   "dialogue_box", "negative", "sheet_layout", "sheet_avoid", "refsheet", "refsheet_avoid"]

# ── Sheet mechanics (layout maths, not style) ──
SHEET_MAX_PANELS = 6
SHAPE_ASPECT = {"wide": 2.0, "tall": 0.5, "square": 1.0, "slit": 4.0}   # width / height
ROW_CAPACITY = 4.0            # max summed aspect per row
ROW_MIN_ASPECT = 2.0          # a lone small panel does not get a giant row
BLACK_MAX = 40                # a pixel is background if max(r,g,b) <= this
GRID = 4                      # slicer works on a 1/GRID downsample, then refines
MIN_PANEL_AREA = 0.012        # ignore specks smaller than this fraction of the image


def die(msg):
    sys.exit(f"story_prompt: {msg}")


def squash(text):
    return re.sub(r"\s+", " ", text).strip()


def h2_sections(text):
    """{'h2 title lowercased': body} for a markdown document."""
    out, name, buf = {}, None, []
    for line in text.splitlines():
        m = re.match(r"^## (?!#)(.+)$", line)
        if m:
            if name is not None:
                out[name] = "\n".join(buf)
            name, buf = m.group(1).strip().lower(), []
        elif name is not None:
            buf.append(line)
    if name is not None:
        out[name] = "\n".join(buf)
    return out


def h3_entries(body):
    """[(id, body)] for each ### heading inside an H2 body."""
    parts = re.split(r"^### (.+)$", body, flags=re.M)
    return [(parts[i].strip(), parts[i + 1]) for i in range(1, len(parts), 2)]


def kv_lines(body):
    return {m.group(1).strip().lower(): m.group(2).strip()
            for m in re.finditer(r"^- ([\w ]+?):\s*(.+)$", body, flags=re.M)}


def section(sections, prefix, where):
    for name, body in sections.items():
        if name.startswith(prefix):
            return body
    die(f"{where}: missing '## {prefix}...' section")


def load_style():
    if not STYLE.exists():
        die(f"{STYLE} not found")
    secs = h2_sections(STYLE.read_text())
    blocks = {}
    for name, body in h3_entries(section(secs, "locked style blocks", "STYLE.md")):
        m = re.search(r"```[^\n]*\n(.*?)```", body, flags=re.S)
        if m:
            blocks[name] = squash(m.group(1))
    missing = [b for b in REQUIRED_BLOCKS if b not in blocks]
    if missing:
        die(f"STYLE.md: missing locked blocks: {', '.join(missing)}")
    shapes = kv_lines(section(secs, "panel shapes", "STYLE.md"))
    sheet_shapes = kv_lines(section(secs, "sheet panel shapes", "STYLE.md"))
    refs = kv_lines(section(secs, "reference images", "STYLE.md"))
    for shape in shapes:
        if shape not in sheet_shapes or shape not in SHAPE_ASPECT:
            die(f"STYLE.md: shape '{shape}' needs a 'Sheet panel shapes' line and a SHAPE_ASPECT entry")
    shots = {}
    for sid, body in h3_entries(section(secs, "shot menu", "STYLE.md")):
        kv = kv_lines(body)
        for key in ("shape", "scale", "prompt"):
            if key not in kv:
                die(f"STYLE.md: shot '{sid}' has no '{key}'")
        if kv["shape"] not in shapes:
            die(f"STYLE.md: shot '{sid}' uses unknown shape '{kv['shape']}'")
        shots[sid] = kv
    if not shots:
        die("STYLE.md: shot menu is empty")
    return {"blocks": blocks, "shapes": shapes, "sheet_shapes": sheet_shapes, "refs": refs,
            "shots": shots, "sections": secs}


def load_cast():
    if not CAST.exists():
        die(f"{CAST} not found")
    cast = {}
    for name, body in h2_sections(CAST.read_text()).items():
        kv = kv_lines(body)
        if "look" in kv:
            bad = [w for w in LOOK_BANNED if re.search(rf"\b{w}\b", kv["look"], flags=re.I)]
            if bad:
                die(f"characters.md: {name.title()}'s look contains {bad}. Race, class, and beard words pull image "
                    f"models toward Western fantasy. Describe only what is visible (see 'Design direction').")
            # h2_sections lowercases; recover the display name from the look or title-case it
            cast[name] = {"name": name.title(), **kv}
    if not cast:
        die("characters.md: no characters with a 'look' line")
    return cast


def parse_scene(path):
    text = path.read_text()
    title = re.search(r"^# (.+)$", text, flags=re.M)
    head = text.split("\n## ", 1)[0]
    secs = h2_sections(text)
    panels = [(m.group(1), m.group(2).strip())
              for m in re.finditer(r"^\d+\.\s*([\w-]+)\s*\|\s*(.+)$", secs.get("panels", ""), flags=re.M)]
    dialogue, reveals = [], []
    for m in re.finditer(r"^- ([^:\[\n]+?)\s*(?:\[(\d+)\])?\s*:\s*(.+)$", secs.get("dialogue", ""), flags=re.M):
        dialogue.append((m.group(1).strip(), m.group(3).strip()))
        reveals.append(int(m.group(2)) if m.group(2) else None)
    meta = kv_lines(head)
    return {
        "path": path, "stem": path.stem,
        "title": title.group(1).strip() if title else path.stem,
        "meta": meta,
        "characters": [c.strip() for c in meta.get("characters", "").split(",") if c.strip()],
        "beat": squash(secs.get("beat", "")),
        "panels": panels, "dialogue": dialogue, "reveals": reveals,
    }


def names_in(text, cast):
    return [c for c in cast if re.search(rf"\b{re.escape(c)}\b", text, flags=re.I)]


def validate(scene, style, cast):
    """Returns (errors, warnings). Errors block the build."""
    err, warn = [], []
    shots, panels = style["shots"], scene["panels"]

    if not scene["meta"].get("location"):
        err.append("missing '- location:' line")
    if not PANELS_MIN <= len(panels) <= PANELS_MAX:
        err.append(f"R1 panel count: {len(panels)} panels, need {PANELS_MIN}-{PANELS_MAX}")
    unknown = [sid for sid, _ in panels if sid not in shots]
    for sid in unknown:
        err.append(f"R2 menu only: unknown shot '{sid}' (run: story_prompt.py shots)")
    if unknown or not panels:
        return err, warn                                   # remaining rules need valid shots

    scales = [shots[sid]["scale"] for sid, _ in panels]
    shapes = [shots[sid]["shape"] for sid, _ in panels]
    for i in range(len(panels) - 1):
        if scales[i] == scales[i + 1]:
            err.append(f"R3 alternate scale: panels {i+1} and {i+2} are both '{scales[i]}' "
                       f"({panels[i][0]}, {panels[i+1][0]})")
    need = 2 if len(panels) == 2 else 3
    if len(set(shapes)) < need:
        err.append(f"R4 vary shape: {len(set(shapes))} distinct shape(s) {sorted(set(shapes))}, need {need}")
    if len(panels) >= 3:
        if not ANCHOR_SCALES & set(scales):
            err.append("R5 anchor and punch: no anchor panel (wide, full, or medium)")
        if not PUNCH_SCALES & set(scales):
            err.append("R5 anchor and punch: no punch panel (close, extreme, or insert)")

    listed = [c.lower() for c in scene["characters"]]
    for c in listed:
        if c not in cast:
            err.append(f"R6 known cast: '{c}' is not in characters.md")
    mentioned = set()
    for n, (sid, desc) in enumerate(panels, 1):
        for c in names_in(desc, cast):
            mentioned.add(c)
            if c not in listed:
                err.append(f"R6 known cast: panel {n} names '{c}' but the characters: line does not list them")
        words = len(desc.split())
        if words > MAX_PANEL_WORDS:
            err.append(f"R7 short panels: panel {n} is {words} words, max {MAX_PANEL_WORDS}")
        low = desc.lower()
        for w in STYLE_WORDS:
            if re.search(rf"\b{re.escape(w)}\b", low):
                err.append(f"R8 no style leakage: panel {n} contains '{w}'")
        hit = next((w for w in TEXT_WORDS if re.search(rf"\b{re.escape(w)}\b", low)), None)
        if hit is None and re.search(r'["“][^"”]+["”]', desc):
            hit = "a quoted string"
        if hit:
            err.append(f"R9 no text: panel {n} asks for written words ({hit})")
    for c in listed:
        if c in cast and c not in mentioned:
            warn.append(f"'{c}' is listed but never named in a panel; their look will not be included")
    for n, r in enumerate(scene["reveals"], 1):
        if r is not None and not 1 <= r <= len(panels):
            err.append(f"dialogue line {n} reveals panel [{r}] but the scene has {len(panels)} panels")
    for who, _ in scene["dialogue"]:
        if who.lower() not in cast:
            warn.append(f"dialogue speaker '{who}' is not in characters.md (fine for one-off NPCs)")
    return err, warn


def assemble(scene, style, cast):
    b, shots, shapes = style["blocks"], style["shots"], style["shapes"]
    meta, panels = scene["meta"], scene["panels"]
    present = []
    for _, desc in panels:
        for c in names_in(desc, cast):
            if c not in present:
                present.append(c)
    lines = [b["header"], ""]
    subject = f"SUBJECT: {scene['title']}. Location: {meta['location'].rstrip('.')}."
    if meta.get("mood"):
        subject += f" Mood: {meta['mood']}."
    lines += [subject, "", f"LAYOUT: Exactly {len(panels)} panels. {b['layout']}", "", "PANELS:"]
    for n, (sid, desc) in enumerate(panels, 1):
        shot = shots[sid]
        lines.append(f"Panel {n}, {shapes[shot['shape']]}: {shot['prompt']} Content: {desc.rstrip('.')}.")
    if present:
        lines += ["", "CHARACTERS, drawn identically in every panel they appear in:"]
        lines += [f"- {cast[c]['look']}" for c in present]
    lines += ["", f"CAMERA AND FRAMING: {b['framing']}",
              "", f"CHARACTER DESIGN: {b['character_design']}",
              "", f"RENDERING: {b['rendering']}", ""]
    if meta.get("dialogue_box", "no").lower() in ("yes", "true", "1"):
        lines.append(f"DIALOGUE BOX: {b['dialogue_box']}")
    else:
        lines.append("No dialogue box. No text anywhere in the image.")
    return "\n".join(lines), b["negative"]


def scene_paths(args):
    if "--all" in args:
        return sorted(p for p in SCENES.glob("*.md") if not p.stem.startswith("000"))
    paths = [Path(a) for a in args if not a.startswith("--")]
    if not paths:
        die("give one or more scene files, or --all")
    for p in paths:
        if not p.exists():
            die(f"{p} not found")
    return paths


def cmd_check_build(args, build):
    style, cast = load_style(), load_cast()
    failed = 0
    for path in scene_paths(args):
        scene = parse_scene(path)
        err, warn = validate(scene, style, cast)
        for w in warn:
            print(f"{path.name}: warning: {w}", file=sys.stderr)
        if err:
            failed += 1
            for e in err:
                print(f"{path.name}: ERROR: {e}", file=sys.stderr)
            continue
        if not build:
            print(f"{path.name}: ok ({len(scene['panels'])} panels)")
            continue
        prompt, negative = assemble(scene, style, cast)
        OUT.mkdir(exist_ok=True)
        txt = OUT / f"{scene['stem']}.prompt.txt"
        txt.write_text(f"{prompt}\n\n--- NEGATIVE ---\n{negative}\n")
        (OUT / f"{scene['stem']}.json").write_text(json.dumps({
            "scene": scene["stem"], "title": scene["title"], "prompt": prompt, "negative": negative,
            "panels": [{"shot": s, "content": d} for s, d in scene["panels"]],
            "dialogue": [{"speaker": w, "line": l} for w, l in scene["dialogue"]],
        }, indent=2) + "\n")
        print(f"{path.name}: wrote {txt.relative_to(ROOT.parent)}")
    if failed:
        sys.exit(f"story_prompt: {failed} scene(s) rejected. Fix the scene file; do not bypass the rules.")


def cmd_shots():
    style = load_style()
    print(f"{'id':<20}{'shape':<8}{'scale':<9}use")
    for sid, s in style["shots"].items():
        print(f"{sid:<20}{s['shape']:<8}{s['scale']:<9}{s.get('use', '')}")


def cmd_brief(args):
    style = load_style()
    load_cast()
    plot = PLOT.read_text() if PLOT.exists() else ""
    beat = " ".join(args).strip()
    if not beat:
        m = re.search(r"^## Next beat\s*\n(.+?)(?=^## |\Z)", plot, flags=re.M | re.S)
        beat = squash(m.group(1)) if m else ""
    if not beat:
        die("no beat given and plot.md has no '## Next beat'")
    done = sorted(p for p in SCENES.glob("*.md") if not p.stem.startswith("000"))
    nums = [int(m.group(1)) for p in done if (m := re.match(r"(\d+)", p.stem))]
    nxt = (max(nums) + 1) if nums else 1
    secs = style["sections"]
    rules = next(v for k, v in secs.items() if k.startswith("composition rules"))
    menu = "\n".join(f"- {sid} (shape {s['shape']}, scale {s['scale']}): {s.get('use', '')}"
                     for sid, s in style["shots"].items())
    recent = "\n\n".join(f"### {p.name}\n{p.read_text().strip()}" for p in done[-2:]) or "(none yet)"
    template = (SCENES / "000_TEMPLATE.md").read_text().strip()
    print(f"""You are writing the next scene file for a manga-cutscene story. Output ONLY the scene
file in the exact format of the template. Do not write an image prompt, and do not describe
art style, palette, or rendering: a tool adds the locked style afterwards and rejects scenes
that break the rules.

# The beat to stage
{beat}

# Save as
story/scenes/{nxt:03d}_short_slug.md  (title line: "# Scene {nxt:03d}: Title")

# Composition rules (the validator enforces these)
{rules.strip()}

# Shot menu (the only allowed shot ids)
{menu}

# Scene file template
{template}

# Cast (use these exact names; only characters whose status allows it)
{CAST.read_text().strip()}

# Story state
{plot.strip()}

# Most recent scenes (for continuity; vary the camera from these)
{recent}

After the scene validates with `./story_prompt.py build`, update story/plot.md: append the beat
to "Beats so far", revise "Open threads", write a new "Next beat", and update any changed
character status in story/characters.md.""")


# ───────────────────────── ChatGPT shot sheets ─────────────────────────

def pack_rows(shapes):
    """Greedy reading-order rows. Returns (rows of panel indexes, row height %, canvas text)."""
    rows, cur, total = [], [], 0.0
    for i, shape in enumerate(shapes):
        a = SHAPE_ASPECT[shape]
        if cur and total + a > ROW_CAPACITY:
            rows.append(cur)
            cur, total = [], 0.0
        cur.append(i)
        total += a
    if cur:
        rows.append(cur)
    heights = [1.0 / max(sum(SHAPE_ASPECT[shapes[i]] for i in row), ROW_MIN_ASPECT) for row in rows]
    page = sum(heights) * 1.25                         # gutters
    canvas = ("portrait, 1024x1536" if page > 1.05 else
              "landscape, 1536x1024" if page < 0.8 else "square, 1024x1024")
    pct = [round(100 * h / sum(heights)) for h in heights]
    return rows, pct, canvas


def existing(path_text):
    if not path_text:
        return None
    p = (ROOT.parent / path_text).resolve()
    return p if p.exists() else None


def attachments(style, cast, names, warn):
    """[(path, instruction)] in attach order: style reference first, then characters."""
    out = []
    sp = existing(style["refs"].get("style"))
    if sp:
        out.append((sp, "STYLE reference. " + style["refs"].get("style_note", "")))
    else:
        warn.append(f"no style reference at {style['refs'].get('style')}; the prompt text carries the style alone")
    for c in names:
        rp = existing(cast[c].get("ref"))
        if rp:
            out.append((rp, f"CHARACTER reference for {cast[c]['name']}. Keep the face, hair, outfit, and "
                            f"colors identical to this image in every panel {cast[c]['name']} appears in."))
        else:
            warn.append(f"no reference image for {cast[c]['name']} at {cast[c].get('ref')}; "
                        f"run: story_prompt.py refsheet {cast[c]['name']}")
    return out


def package(title, attach, prompt, after):
    rel = lambda p: p.relative_to(ROOT.parent)
    lines = [f"# ChatGPT package: {title}", "",
             "## 1. Start a new chat and attach these files, in this order", ""]
    lines += [f"{n}. `{rel(p)}`" for n, (p, _) in enumerate(attach, 1)] or \
             ["(no reference images found; see the warnings the tool printed)"]
    lines += ["", "## 2. Paste this prompt exactly", "", "````", prompt, "````", "", "## 3. Afterwards", ""]
    lines += after
    return "\n".join(lines) + "\n"


def cmd_sheet(args):
    style, cast = load_style(), load_cast()
    b, shots = style["blocks"], style["shots"]
    scenes, warn = [], []
    for path in scene_paths(args):
        scene = parse_scene(path)
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
    rows, pct, canvas = pack_rows(shapes)
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
          f"The sheet has {len(rows)} row(s), read left to right, top to bottom:"]
    for r, row in enumerate(rows, 1):
        cells = ", then ".join(f"Panel {i+1} ({style['sheet_shapes'][shapes[i]]})" for i in row)
        L.append(f"Row {r}, about {pct[r-1]} percent of the image height: {cells}.")
    L.append("")
    k = 0
    for sc in scenes:
        first, last = k + 1, k + len(sc["panels"])
        span = f"Panel {first}" if first == last else f"Panels {first} to {last}"
        setting = f"SETTING for {span}: {sc['meta']['location'].rstrip('.')}."
        if sc["meta"].get("mood"):
            setting += f" Mood: {sc['meta']['mood']}."
        L.append(setting)
        for sid, desc in sc["panels"]:
            k += 1
            L.append(f"Panel {k}: {shots[sid]['prompt']} Content: {desc.rstrip('.')}.")
        L.append("")
    if present:
        L.append("CHARACTERS, drawn identically in every panel they appear in:")
        L += [f"- {cast[c]['look']}" for c in present]
        L.append("")
    L += [f"CAMERA AND FRAMING inside each panel: {b['framing']}", "",
          f"CHARACTER DESIGN: {b['character_design']}", "",
          f"RENDERING: {b['rendering']}", "",
          f"AVOID: {b['sheet_avoid']}, {b['negative']}"]
    prompt = "\n".join(L)

    OUT.mkdir(exist_ok=True)
    manifest = OUT / f"{name}.sheet.json"
    manifest.write_text(json.dumps({
        "sheet": name, "canvas": canvas, "rows": [[i + 1 for i in row] for row in rows],
        "panels": [{"n": i + 1, "scene": sc["stem"], "scene_panel": n, "shot": sid, "shape": shapes[i],
                    "content": desc} for i, (sc, n, sid, desc) in enumerate(flat)],
        "dialogue": {sc["stem"]: [{"speaker": w, "line": l} for w, l in sc["dialogue"]] for sc in scenes},
        "prompt": prompt,
    }, indent=2) + "\n")
    md = OUT / f"{name}.chatgpt.md"
    md.write_text(package(f"shot sheet {name}", attach, prompt, [
        f"- Check the result: {len(flat)} separate white-bordered panels on pure black, none touching, no labels.",
        "  If panels overlap or merged, reply: \"Regenerate. Keep every panel separate with wide pure black gutters.\"",
        "- Download the image, then cut it into panels:",
        "", "```", f"./story_prompt.py slice {manifest.relative_to(ROOT.parent)} ~/Downloads/<file>.png", "```"]))
    for w in warn:
        print(f"warning: {w}", file=sys.stderr)
    print(f"wrote {md.relative_to(ROOT.parent)}  ({len(flat)} panels, {len(rows)} row(s), {canvas}, "
          f"{len(attach)} attachment(s))")


def cmd_refsheet(args):
    style, cast = load_style(), load_cast()
    if not args or args[0].lower() not in cast:
        die(f"usage: refsheet <Name>. Known: {', '.join(c['name'] for c in cast.values())}")
    c, b, warn = cast[args[0].lower()], style["blocks"], []
    attach = attachments(style, cast, [], warn)
    L = [f"Create ONE image: a character reference sheet for {c['name']}. Canvas: landscape, 1536x1024.",
         b["header"], ""]
    if attach:
        L += ["ATTACHED REFERENCE IMAGES, in the order I attached them:"]
        L += [f"Image {n}: {note}" for n, (_, note) in enumerate(attach, 1)] + [""]
    L += [f"SHEET: {b['refsheet']}", "", f"CHARACTER: {c['look']}", "",
          f"CHARACTER DESIGN: {b['character_design']}", "", f"RENDERING: {b['rendering']}", "",
          f"AVOID: {b['refsheet_avoid']}"]
    target = c.get("ref", f"story/refs/{c['name'].lower()}.png")
    OUT.mkdir(exist_ok=True)
    md = OUT / f"ref_{c['name'].lower()}.chatgpt.md"
    md.write_text(package(f"reference sheet for {c['name']}", attach, "\n".join(L), [
        f"- Regenerate until you like the design. This image becomes {c['name']}'s look in every scene.",
        f"- Save it as `{target}`. Every later `sheet` package attaches it automatically.",
        f"- If the design differs from the `look` line in characters.md, update the `look` line to match."]))
    for w in warn:
        print(f"warning: {w}", file=sys.stderr)
    print(f"wrote {md.relative_to(ROOT.parent)}  (save the result as {target})")


# ───────────────────────── Slicer (stdlib PNG) ─────────────────────────

def read_png(path):
    """(width, height, channels, rows) for 8-bit RGB/RGBA non-interlaced PNG. Other formats go through sips."""
    data = path.read_bytes()
    if data[:8] != b"\x89PNG\r\n\x1a\n":
        tmp = OUT / f"_{path.stem}.png"
        OUT.mkdir(exist_ok=True)
        try:
            subprocess.run(["sips", "-s", "format", "png", str(path), "--out", str(tmp)],
                           check=True, capture_output=True)
        except (OSError, subprocess.CalledProcessError):
            die(f"{path.name} is not a PNG and sips could not convert it. Convert it to PNG first.")
        data = tmp.read_bytes()
    pos, idat, w = 8, b"", 0
    while pos < len(data):
        n, kind = struct.unpack(">I4s", data[pos:pos + 8])
        body = data[pos + 8:pos + 8 + n]
        pos += 12 + n
        if kind == b"IHDR":
            w, h, depth, ctype, _, _, interlace = struct.unpack(">IIBBBBB", body)
            if depth != 8 or ctype not in (2, 6) or interlace:
                die(f"{path.name}: unsupported PNG (need 8-bit RGB/RGBA, non-interlaced). "
                    f"Fix with: sips -s format png -s formatOptions default {path.name} --out fixed.png")
            ch = 3 if ctype == 2 else 4
        elif kind == b"IDAT":
            idat += body
    raw, stride = zlib.decompress(idat), w * ch
    rows, prev = [], bytearray(stride)
    for y in range(h):
        f = raw[y * (stride + 1)]
        line = bytearray(raw[y * (stride + 1) + 1:(y + 1) * (stride + 1)])
        if f == 1:
            for i in range(ch, stride):
                line[i] = (line[i] + line[i - ch]) & 255
        elif f == 2:
            for i in range(stride):
                line[i] = (line[i] + prev[i]) & 255
        elif f == 3:
            for i in range(stride):
                left = line[i - ch] if i >= ch else 0
                line[i] = (line[i] + ((left + prev[i]) >> 1)) & 255
        elif f == 4:
            for i in range(stride):
                a = line[i - ch] if i >= ch else 0
                bb, c = prev[i], (prev[i - ch] if i >= ch else 0)
                pa, pb, pc = abs(bb - c), abs(a - c), abs(a + bb - 2 * c)
                line[i] = (line[i] + (a if pa <= pb and pa <= pc else bb if pb <= pc else c)) & 255
        rows.append(line)
        prev = line
    return w, h, ch, rows


def write_png(path, w, h, ch, rows):
    def chunk(kind, body):
        return struct.pack(">I", len(body)) + kind + body + struct.pack(">I", zlib.crc32(kind + body))
    raw = b"".join(b"\x00" + bytes(r) for r in rows)
    path.write_bytes(b"\x89PNG\r\n\x1a\n" + chunk(b"IHDR", struct.pack(">IIBBBBB", w, h, 8, 2 if ch == 3 else 6, 0, 0, 0))
                     + chunk(b"IDAT", zlib.compress(raw, 6)) + chunk(b"IEND", b""))


def find_panels(w, h, ch, rows):
    """Bounding boxes (x0, y0, x1, y1) of non-black regions, in reading order."""
    lit = lambda x, y: max(rows[y][x * ch:x * ch + 3]) > BLACK_MAX
    gw, gh = w // GRID, h // GRID
    mask = [[lit(gx * GRID + GRID // 2, gy * GRID + GRID // 2) for gx in range(gw)] for gy in range(gh)]
    seen = [[False] * gw for _ in range(gh)]
    boxes = []
    for gy in range(gh):
        for gx in range(gw):
            if not mask[gy][gx] or seen[gy][gx]:
                continue
            stack, x0, y0, x1, y1 = [(gx, gy)], gx, gy, gx, gy
            seen[gy][gx] = True
            while stack:
                cx, cy = stack.pop()
                x0, y0, x1, y1 = min(x0, cx), min(y0, cy), max(x1, cx), max(y1, cy)
                for nx, ny in ((cx + 1, cy), (cx - 1, cy), (cx, cy + 1), (cx, cy - 1)):
                    if 0 <= nx < gw and 0 <= ny < gh and mask[ny][nx] and not seen[ny][nx]:
                        seen[ny][nx] = True
                        stack.append((nx, ny))
            boxes.append([x0 * GRID, y0 * GRID, min(w, (x1 + 1) * GRID), min(h, (y1 + 1) * GRID)])
    boxes = [b for b in boxes if (b[2] - b[0]) * (b[3] - b[1]) >= MIN_PANEL_AREA * w * h]
    boxes = [b for b in boxes if not any(o is not b and o[0] <= b[0] and o[1] <= b[1] and o[2] >= b[2] and o[3] >= b[3]
                                         for o in boxes)]
    for b in boxes:                                   # refine each side to the exact first lit line
        b[0], b[1] = max(0, b[0] - GRID), max(0, b[1] - GRID)
        b[2], b[3] = min(w, b[2] + GRID), min(h, b[3] + GRID)
        while b[0] < b[2] - 1 and not any(lit(b[0], y) for y in range(b[1], b[3], 2)): b[0] += 1
        while b[2] > b[0] + 1 and not any(lit(b[2] - 1, y) for y in range(b[1], b[3], 2)): b[2] -= 1
        while b[1] < b[3] - 1 and not any(lit(x, b[1]) for x in range(b[0], b[2], 2)): b[1] += 1
        while b[3] > b[1] + 1 and not any(lit(x, b[3] - 1) for x in range(b[0], b[2], 2)): b[3] -= 1
    boxes.sort(key=lambda b: (b[1] + b[3]) / 2)
    ordered, row = [], []
    for b in boxes:                                   # same row if its centre falls inside the row's first box
        if row and not (row[0][1] <= (b[1] + b[3]) / 2 <= row[0][3]):
            ordered += sorted(row)
            row = []
        row.append(b)
    return ordered + sorted(row)


def cmd_slice(args):
    pos, trim, manual, it = [], 0, None, iter(args)
    for a in it:
        if a == "--trim":
            trim = int(next(it, "0"))
        elif a == "--boxes":
            manual = next(it, "")
        else:
            pos.append(a)
    if len(pos) != 2:
        die('usage: slice story/out/<name>.sheet.json <image> [--trim N] [--boxes "x,y,w,h;..."]')
    manifest, image = json.loads(Path(pos[0]).read_text()), Path(pos[1]).expanduser()
    if not image.exists():
        die(f"{image} not found")
    w, h, ch, rows = read_png(image)
    if manual:
        boxes = [[x, y, x + bw, y + bh] for x, y, bw, bh in
                 (map(int, part.split(",")) for part in manual.split(";") if part.strip())]
    else:
        boxes = find_panels(w, h, ch, rows)
    want = manifest["panels"]
    print(f"{image.name}: {w}x{h}, found {len(boxes)} panel(s), sheet expects {len(want)}")
    for n, b in enumerate(boxes, 1):
        print(f"  box {n}: x={b[0]} y={b[1]} w={b[2]-b[0]} h={b[3]-b[1]}")
    if len(boxes) != len(want):
        die("panel count mismatch, nothing written. Panels probably touch or overlap: ask ChatGPT to regenerate "
            'with wider black gutters, or pass the boxes by hand with --boxes "x,y,w,h;x,y,w,h".')
    PANELS.mkdir(exist_ok=True)
    for b, p in zip(boxes, want):
        x0, y0, x1, y1 = b[0] + trim, b[1] + trim, b[2] - trim, b[3] - trim
        out = PANELS / f"{p['scene']}_p{p['scene_panel']}_{p['shot']}.png"
        write_png(out, x1 - x0, y1 - y0, ch, [rows[y][x0 * ch:x1 * ch] for y in range(y0, y1)])
        got, exp = (x1 - x0) / (y1 - y0), SHAPE_ASPECT[p["shape"]]
        note = "" if 0.6 * exp <= got <= 1.6 * exp else f"   <-- aspect {got:.2f}, expected about {exp:.1f} ({p['shape']}): check the order"
        print(f"  wrote {out.relative_to(ROOT.parent)}  {x1-x0}x{y1-y0}{note}")


# ───────────────────────── Browser preview (panel reveal + dialogue) ─────────────────────────

STAGE_ASPECT = 1.6            # 16:10 stage; the dialogue box covers the bottom of it
PAGE_BOTTOM = 74.0            # panels stay above this percent of the stage height
SLOTS = {                     # shape -> (width %, [(x %, y %) for 1st, 2nd use])
    "wide":   (52, [(4, 5), (20, 40)]),
    "tall":   (22, [(73, 3), (5, 8)]),
    "slit":   (60, [(20, 52), (8, 6)]),
    "square": (17, None),     # tucked over the bottom-right corner of the previous panel
}


def png_size(path):
    head = path.read_bytes()[:24]
    return struct.unpack(">II", head[16:24]) if head[:8] == b"\x89PNG\r\n\x1a\n" else None


def page_layout(scene, style):
    out, used, prev = [], {}, None
    for n, (sid, desc) in enumerate(scene["panels"], 1):
        shape = style["shots"][sid]["shape"]
        img = PANELS / f"{scene['stem']}_p{n}_{sid}.png"
        size = png_size(img) if img.exists() else None
        aspect = size[0] / size[1] if size else SHAPE_ASPECT[shape]
        w, spots = SLOTS[shape]
        if spots:
            x, y = spots[min(used.get(shape, 0), len(spots) - 1)]
        elif prev:
            x, y = prev["x"] + prev["w"] - 7, prev["y"] + prev["h"] - 9
        else:
            x, y = 6, 40
        h = w / aspect * STAGE_ASPECT
        x = max(1, min(x, 99 - w))
        y = max(1, min(y, PAGE_BOTTOM - h))
        used[shape] = used.get(shape, 0) + 1
        prev = {"n": n, "shot": sid, "x": round(x, 1), "y": round(y, 1), "w": w, "h": round(h, 1),
                "img": f"../panels/{img.name}" if size else None, "content": desc}
        out.append(prev)
    return out


PREVIEW_HTML = r"""<!doctype html><meta charset="utf-8"><meta name="viewport" content="width=device-width,initial-scale=1">
<title>__TITLE__</title>
<style>
html,body{margin:0;height:100%;background:#000;color:#fff;font-family:ui-monospace,Menlo,monospace}
#stage{position:relative;width:min(100vw,160vh);aspect-ratio:16/10;margin:0 auto;background:#000;overflow:hidden;
 cursor:pointer;user-select:none;container-type:inline-size}
.p{position:absolute;opacity:0;transform:scale(.97);transition:opacity .25s,transform .25s;box-sizing:border-box}
.p.on{opacity:1;transform:none}
.p img{width:100%;height:100%;display:block;image-rendering:pixelated}
.p.ph{background:#15131f;border:.35cqw solid #fff;outline:.15cqw solid #222;outline-offset:-.5cqw;
 display:flex;align-items:center;justify-content:center;text-align:center;font-size:1.5cqw;color:#889;padding:1cqw}
#box{position:absolute;left:4%;right:4%;bottom:2%;height:22%;background:#0b2a7c;border:.45cqw solid #d8d8e0;
 outline:.25cqw solid #5a5a6a;border-radius:.6cqw;box-sizing:border-box;padding:1.4cqw 2cqw;z-index:99}
#who{color:#ffd84a;font-weight:700;font-size:2cqw;margin-bottom:.3cqw}
#txt{font-weight:700;font-size:2.4cqw;line-height:1.3;white-space:pre-wrap}
#arrow{position:absolute;right:1.6cqw;bottom:.6cqw;font-size:2cqw;animation:b 1s steps(2) infinite}
@keyframes b{50%{opacity:0}}
</style>
<div id="stage"><div id="box"><div id="who"></div><div id="txt"></div><div id="arrow">&#9660;</div></div></div>
<script>
const D=__DATA__, stage=document.getElementById('stage'), who=document.getElementById('who'),
      txt=document.getElementById('txt'), arrow=document.getElementById('arrow');
const els=D.panels.map(p=>{const e=document.createElement('div');e.className='p'+(p.img?'':' ph');
  e.style.cssText=`left:${p.x}%;top:${p.y}%;width:${p.w}%;height:${p.h}%`;
  if(p.img){const i=new Image();i.src=p.img;e.appendChild(i)}else e.textContent=`P${p.n} ${p.shot}\n${p.content}`;
  stage.insertBefore(e,document.getElementById('box'));return e});
let line=-1,typing=null,z=1;
function reveal(n){const e=els[n-1];if(e&&!e.classList.contains('on')){e.style.zIndex=z++;e.classList.add('on')}}
function show(){const l=D.lines[line];reveal(l.reveal);who.textContent=l.speaker;txt.textContent='';arrow.style.display='none';
  let i=0;typing=setInterval(()=>{txt.textContent=l.text.slice(0,++i);if(i>=l.text.length)done()},28)}
function done(){clearInterval(typing);typing=null;txt.textContent=D.lines[line].text;arrow.style.display=''}
function next(){if(typing)return done();
  if(line>=D.lines.length-1){els.forEach(e=>e.classList.remove('on'));z=1;line=-1}
  line++;show()}
stage.onclick=next;document.onkeydown=e=>{if(e.key===' '||e.key==='Enter')next()};
if(location.hash==='#all'){D.panels.forEach(p=>reveal(p.n));if(D.lines.length){line=D.lines.length-1;show();done()}}
else if(D.lines.length)next();else D.panels.forEach(p=>reveal(p.n));
</script>
"""


def cmd_preview(args):
    style, cast = load_style(), load_cast()
    for path in scene_paths(args):
        scene = parse_scene(path)
        err, _ = validate(scene, style, cast)
        if err:
            for e in err:
                print(f"{path.name}: ERROR: {e}", file=sys.stderr)
            sys.exit("story_prompt: scene rejected. Fix the scene file; do not bypass the rules.")
        panels = page_layout(scene, style)
        shown, lines = set(), []
        for (speaker, text), tag in zip(scene["dialogue"], scene["reveals"]):
            n = tag or next((p["n"] for p in panels if p["n"] not in shown), len(panels))
            shown.add(n)
            lines.append({"speaker": speaker, "text": text, "reveal": n})
        data = {"panels": panels, "lines": lines}
        OUT.mkdir(exist_ok=True)
        html = OUT / f"{scene['stem']}.preview.html"
        html.write_text(PREVIEW_HTML.replace("__TITLE__", scene["title"]).replace("__DATA__", json.dumps(data)))
        have = sum(1 for p in panels if p["img"])
        print(f"wrote {html.relative_to(ROOT.parent)}  ({have}/{len(panels)} panel images found, "
              f"{len(lines)} dialogue lines). Open it in a browser; tap or press space to advance.")


def main():
    args = sys.argv[1:]
    cmd = args[0] if args else ""
    if cmd == "shots":
        cmd_shots()
    elif cmd in ("check", "build"):
        cmd_check_build(args[1:], build=(cmd == "build"))
    elif cmd == "brief":
        cmd_brief(args[1:])
    elif cmd == "sheet":
        cmd_sheet(args[1:])
    elif cmd == "refsheet":
        cmd_refsheet(args[1:])
    elif cmd == "slice":
        cmd_slice(args[1:])
    elif cmd == "preview":
        cmd_preview(args[1:])
    else:
        print(__doc__.strip())
        sys.exit(0 if cmd in ("", "-h", "--help", "help") else 2)


if __name__ == "__main__":
    main()
