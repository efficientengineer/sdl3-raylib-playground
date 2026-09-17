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

  ./story_prompt.py export                     write src/cutscene_data.h from story/playlist.md for the game
                                               (scenes whose panels are not all generated yet are skipped)

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
PANELS_MIN, PANELS_MAX = 2, 4                       # R1, per page
SCENE_MAX_PANELS = 8                                # R1, per scene (one shot sheet)
ANCHOR_SCALES = {"wide", "full", "medium"}          # R5
PUNCH_SCALES = {"close", "extreme", "insert"}       # R5
MAX_PANEL_WORDS = 40                                # R7
MAX_ACTING_WORDS = 25                               # R11
GAZE_WORDS = ["look", "glar", "star", "gaz", "glanc", "watch", "eyes", "eye ", "facing", "faces ", "turned",
              "peer", "squint", "fixed on"]          # R11: an acting line must say where the eyes point
STYLE_WORDS = ["gradient", "photorealistic", "3d", "blur", "glow", "painterly",
               "realistic", "hd", "4k", "smooth"]   # R8
TEXT_WORDS = ["text", "caption", "speech bubble", "lettering", "written", "inscription reading",
              "sign saying", "sign that says", "sign reading", "the words", "subtitle"]  # R9
MOODS = ["wonder", "dread", "tense", "confront", "sorrow", "hope"]      # music moods; order = CsMood enum in the game
LOOK_BANNED = ["dwarf", "dwarven", "halfling", "hobbit", "elf", "elven", "gnome", "orc", "fighter",
               "rogue", "cleric", "wizard", "ranger", "barbarian", "paladin", "beard", "bearded"]  # see characters.md
REQUIRED_BLOCKS = ["header", "layout", "framing", "acting", "character_design", "rendering",
                   "dialogue_box", "negative", "sheet_layout", "sheet_avoid", "refsheet", "refsheet_avoid"]

# ── Sheet mechanics (layout maths, not style) ──
SHEET_MAX_PANELS = 8
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
    panels, acting, pages, page = [], [], [], 0
    for raw in secs.get("panels", "").splitlines():
        if re.match(r"^\s*-{3,}\s*$", raw):                     # "---" starts a new page: the screen clears
            page += 1 if panels and pages[-1] == page else 0
            continue
        m = re.match(r"^\d+\.\s*([\w-]+)\s*\|\s*(.+)$", raw)
        if m:
            panels.append((m.group(1), m.group(2).strip()))
            acting.append({})
            pages.append(page)
            continue
        m = re.match(r"^\s+- ([^:]+):\s*(.+)$", raw)           # indented "- Name: where they look, expression, body"
        if m and panels:
            acting[-1][m.group(1).strip().lower()] = m.group(2).strip()
    dialogue, reveals, moods = [], [], []
    for m in re.finditer(r"^- ([^:\[{\n]+?)\s*(?:\[(\d+)\])?\s*(?:\{(\w+)\})?\s*:\s*(.+)$",
                         secs.get("dialogue", ""), flags=re.M):
        dialogue.append((m.group(1).strip(), m.group(4).strip()))
        reveals.append(int(m.group(2)) if m.group(2) else None)
        moods.append(m.group(3).lower() if m.group(3) else None)
    meta = kv_lines(head)
    return {
        "path": path, "stem": path.stem,
        "title": title.group(1).strip() if title else path.stem,
        "meta": meta,
        "characters": [c.strip() for c in meta.get("characters", "").split(",") if c.strip()],
        "beat": squash(secs.get("beat", "")),
        "panels": panels, "acting": acting, "pages": pages, "dialogue": dialogue, "reveals": reveals, "moods": moods,
        "narration": meta.get("type", "").lower() == "narration",
    }


def names_in(text, cast):
    return [c for c in cast if re.search(rf"\b{re.escape(c)}\b", text, flags=re.I)]


def validate(scene, style, cast):
    """Returns (errors, warnings). Errors block the build."""
    err, warn = [], []
    shots, panels = style["shots"], scene["panels"]

    for n, mood in enumerate(scene["moods"], 1):
        if mood and mood not in MOODS:
            err.append(f"dialogue line {n}: unknown mood '{{{mood}}}'. Use one of: {', '.join(MOODS)}")
    if scene["narration"]:
        if panels:
            err.append("a narration scene has no panels; remove '## Panels' or the 'type: narration' line")
        if not scene["dialogue"]:
            err.append("a narration scene needs at least one line under '## Dialogue'")
        return err, warn

    if not scene["meta"].get("location"):
        err.append("missing '- location:' line")
    if len(panels) > SCENE_MAX_PANELS:
        err.append(f"R1 panel count: {len(panels)} panels in the scene, max {SCENE_MAX_PANELS} (one shot sheet)")
    unknown = [sid for sid, _ in panels if sid not in shots]
    for sid in unknown:
        err.append(f"R2 menu only: unknown shot '{sid}' (run: story_prompt.py shots)")
    if unknown or not panels:
        if not panels:
            err.append("R1 panel count: no panels")
        return err, warn                                   # remaining rules need valid shots

    for pg in sorted(set(scene["pages"])):                 # composition rules apply to each page on its own
        idx = [i for i, p in enumerate(scene["pages"]) if p == pg]
        where = f"page {pg + 1}" if len(set(scene["pages"])) > 1 else "the page"
        if not PANELS_MIN <= len(idx) <= PANELS_MAX:
            err.append(f"R1 panel count: {where} has {len(idx)} panels, need {PANELS_MIN}-{PANELS_MAX}")
        scales = [shots[panels[i][0]]["scale"] for i in idx]
        shapes = [shots[panels[i][0]]["shape"] for i in idx]
        for k in range(len(idx) - 1):
            if scales[k] == scales[k + 1]:
                err.append(f"R3 alternate scale: panels {idx[k]+1} and {idx[k+1]+1} are both '{scales[k]}' "
                           f"({panels[idx[k]][0]}, {panels[idx[k+1]][0]})")
        need = 2 if len(idx) <= 2 else 3
        if len(set(shapes)) < need:
            err.append(f"R4 vary shape: {where} has {len(set(shapes))} distinct shape(s) {sorted(set(shapes))}, need {need}")
        if len(idx) >= 3:
            if not ANCHOR_SCALES & set(scales):
                err.append(f"R5 anchor and punch: {where} has no anchor panel (wide, full, or medium)")
            if not PUNCH_SCALES & set(scales):
                err.append(f"R5 anchor and punch: {where} has no punch panel (close, extreme, or insert)")

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
    if not scene["meta"].get("staging"):
        err.append("R10 staging: missing '- staging:' line (who is on which side, facing which way, where the threat is)")
    for n, ((sid, desc), notes) in enumerate(zip(panels, scene["acting"]), 1):
        named = names_in(desc, cast)
        for c in named:
            if c not in notes:
                err.append(f"R11 acting: panel {n} shows {c.title()} but has no acting line "
                           f"('   - {c.title()}: where they look, expression, body')")
        for c, note in notes.items():
            if c not in named:
                err.append(f"R11 acting: panel {n} has an acting line for '{c}' who is not named in the panel")
            if not any(g in note.lower() for g in GAZE_WORDS):
                err.append(f"R11 acting: panel {n}, {c.title()}: say where the eyes point (looking at..., glaring toward...)")
            if len(note.split()) > MAX_ACTING_WORDS:
                err.append(f"R11 acting: panel {n}, {c.title()}: {len(note.split())} words, max {MAX_ACTING_WORDS}")
            for w in STYLE_WORDS:
                if re.search(rf"\b{re.escape(w)}\b", note.lower()):
                    err.append(f"R8 no style leakage: panel {n} acting line for {c.title()} contains '{w}'")
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


def panel_text(style, cast, sid, desc, notes):
    """One panel's line for the image prompt: shot, content, then who looks where and feels what."""
    text = f"{style['shots'][sid]['prompt']} Content: {desc.rstrip('.')}."
    if notes:
        text += " Acting: " + " ".join(f"{cast[c]['name']}: {note.rstrip('.')}." for c, note in notes.items())
    return text


def scene_context(scene, span=""):
    """Story context for the image model: what is happening, where, and the fixed screen direction."""
    meta = scene["meta"]
    out = [f"SITUATION{span}: {scene['beat']}"] if scene["beat"] else []
    setting = f"SETTING{span}: {meta['location'].rstrip('.')}."
    if meta.get("mood"):
        setting += f" Mood: {meta['mood']}."
    out.append(setting)
    if meta.get("staging"):
        out.append(f"STAGING{span}, the same in every panel: {meta['staging'].rstrip('.')}.")
    return out


def assemble(scene, style, cast):
    b, shots, shapes = style["blocks"], style["shots"], style["shapes"]
    meta, panels = scene["meta"], scene["panels"]
    present = []
    for _, desc in panels:
        for c in names_in(desc, cast):
            if c not in present:
                present.append(c)
    lines = [b["header"], "", f"SUBJECT: {scene['title']}."] + scene_context(scene)
    lines += ["", f"LAYOUT: Exactly {len(panels)} panels. {b['layout']}", "", "PANELS:"]
    for n, ((sid, desc), notes) in enumerate(zip(panels, scene["acting"]), 1):
        lines.append(f"Panel {n}, {shapes[shots[sid]['shape']]}: {panel_text(style, cast, sid, desc, notes)}")
    if present:
        lines += ["", "CHARACTERS, drawn identically in every panel they appear in:"]
        lines += [f"- {cast[c]['look']}" for c in present]
    lines += ["", b["acting"],
              "", f"CAMERA AND FRAMING: {b['framing']}",
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
            pg = len(set(scene["pages"])) if scene["panels"] else 0
            print(f"{path.name}: ok ({len(scene['panels'])} panels" + (f", {pg} pages)" if pg > 1 else ")"))
            continue
        if len(set(scene["pages"])) > 1:
            print(f"{path.name}: skipped, 'build' draws one finished page and this scene has several. Use 'sheet'.", file=sys.stderr)
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

# Relative size of each shape on a sheet, as (width, height) in units of u. A wide panel is the yardstick.
SHAPE_UNITS = {"wide": (2.0, 1.0), "tall": (0.8, 1.6), "square": (0.8, 0.8), "slit": (1.8, 0.45)}


def _pack(shapes, W, H, u):
    """Pack panels in order into top-aligned rows at unit size `u`. Returns rects in pixels, or None if it overflows."""
    g, rg = 0.05 * W, 0.06 * W                              # outer margin / row gap, and gap between panels in a row
    rows, cur, width = [], [], 0.0
    for i, sh in enumerate(shapes):
        w = SHAPE_UNITS[sh][0] * u
        if cur and width + rg + w > W - 2 * g:
            rows.append(cur)
            cur, width = [], 0.0
        width += (rg if cur else 0) + w
        cur.append(i)
    rows.append(cur)
    rects, y = [None] * len(shapes), g
    for r, row in enumerate(rows):
        used = sum(SHAPE_UNITS[shapes[i]][0] * u for i in row) + rg * (len(row) - 1)
        if used > W - 2 * g:
            return None
        x = g if r % 2 == 0 else W - g - used                # stagger rows left / right so it never reads as a grid
        for i in row:
            w, h = SHAPE_UNITS[shapes[i]][0] * u, SHAPE_UNITS[shapes[i]][1] * u
            rects[i] = (x, y, w, h)
            x += w + rg
        y += max(SHAPE_UNITS[shapes[i]][1] * u for i in row) + g
    return rects if y - g <= H - g else None


def sheet_geometry(shapes):
    """Lay the panels out on a canvas. Returns (canvas text, [(x, y, w, h)] normalized 0-1).

    Every shape has a fixed relative size (SHAPE_UNITS), so the important wide panels are always the
    big ones and insets stay small. Panels pack into rows in reading order, and the whole sheet is scaled
    up until it just fits. The canvas that lets the panels be largest wins. Up to 4 panels use the
    standard sizes; more use the large ones (2560x1440 is the biggest size OpenAI does not call experimental).
    """
    small = ((1536, 1024, "landscape, 1536x1024"), (1024, 1536, "portrait, 1024x1536"), (1024, 1024, "square, 1024x1024"))
    large = ((2560, 1440, "landscape, 2560x1440"), (1440, 2560, "portrait, 1440x2560"), (2048, 2048, "square, 2048x2048"))
    best = None
    for W, H, label in (small if len(shapes) <= 4 else large):
        lo, hi = 1.0, float(max(W, H))
        for _ in range(40):                                  # largest u that still fits
            mid = (lo + hi) / 2
            if _pack(shapes, W, H, mid): lo = mid
            else: hi = mid
        rects = _pack(shapes, W, H, lo)
        if not rects:
            continue
        fill = sum(w * h for _, _, w, h in rects) / (W * H)
        if best is None or fill > best[0]:
            best = (fill, label, [(x / W, y / H, w / W, h / H) for x, y, w, h in rects])
    if best is None:
        die("could not fit the panels on any canvas")
    return best[1], best[2]


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
                            f"colors identical to this image in every panel {cast[c]['name']} appears in. Use it only "
                            f"for the character's design: ignore its background colors and its three-panel layout, "
                            f"and do NOT copy its calm neutral expression or head angle. Expressions come from the acting notes."))
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
        for (sid, desc), notes in zip(sc["panels"], sc["acting"]):
            k += 1
            L.append(f"Panel {k}: {panel_text(style, cast, sid, desc, notes)}")
            for c, note in notes.items():
                checks.append(f"- [ ] Panel {k}, {cast[c]['name']}: {note}")
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
    manifest = OUT / f"{name}.sheet.json"
    manifest.write_text(json.dumps({
        "sheet": name, "canvas": canvas, "rects": [[round(v, 4) for v in r] for r in rects],
        "panels": [{"n": i + 1, "scene": sc["stem"], "scene_panel": n, "shot": sid, "shape": shapes[i],
                    "content": desc} for i, (sc, n, sid, desc) in enumerate(flat)],
        "dialogue": {sc["stem"]: [{"speaker": w, "line": l} for w, l in sc["dialogue"]] for sc in scenes},
        "prompt": prompt,
    }, indent=2) + "\n")
    md = OUT / f"{name}.chatgpt.md"
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
        "When it passes, download the image and cut it into panels:",
        "", "```", f"./story_prompt.py slice {manifest.relative_to(ROOT.parent)} ~/Downloads/<file>.png", "```"]))
    for w in warn:
        print(f"warning: {w}", file=sys.stderr)
    print(f"wrote {md.relative_to(ROOT.parent)}  ({len(flat)} panels, {canvas}, {len(attach)} attachment(s))")


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


def match_boxes(boxes, rects, w, h):
    """Order detected boxes to match the expected panels: nearest centre plus closest aspect ratio."""
    import itertools
    import math

    def cost(b, r):
        cx, cy = (b[0] + b[2]) / 2 / w, (b[1] + b[3]) / 2 / h
        ex, ey = r[0] + r[2] / 2, r[1] + r[3] / 2
        got = (b[2] - b[0]) / (b[3] - b[1]) * (w / h) ** 0   # pixel aspect
        exp = (r[2] * w) / (r[3] * h)
        return math.hypot(cx - ex, cy - ey) + 0.5 * abs(math.log(got / exp))
    best = min(itertools.permutations(range(len(boxes))),
               key=lambda perm: sum(cost(boxes[j], rects[i]) for i, j in enumerate(perm)))
    return [boxes[j] for j in best]


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
    if manifest.get("rects") and not manual:
        boxes = match_boxes(boxes, manifest["rects"], w, h)
    PANELS.mkdir(exist_ok=True)
    for b, p in zip(boxes, want):
        x0, y0, x1, y1 = b[0] + trim, b[1] + trim, b[2] - trim, b[3] - trim
        out = PANELS / f"{p['scene']}_p{p['scene_panel']}_{p['shot']}.png"
        write_png(out, x1 - x0, y1 - y0, ch, [rows[y][x0 * ch:x1 * ch] for y in range(y0, y1)])
        got, exp = (x1 - x0) / (y1 - y0), SHAPE_ASPECT[p["shape"]]
        note = "" if 0.6 * exp <= got <= 1.6 * exp else f"   <-- aspect {got:.2f}, expected about {exp:.1f} ({p['shape']}): check the order"
        print(f"  wrote {out.relative_to(ROOT.parent)}  {x1-x0}x{y1-y0}{note}")


# ───────────────────────── Browser preview (panel reveal + dialogue) ─────────────────────────

# Two page layouts, as percentages of a stage. "land": 16:10 stage, dialogue box overlays the bottom.
# "port": 4:5 stage for an upright phone, dialogue box sits below the stage.
LAYOUTS = {
    "land": {"aspect": 1.6, "bottom": 74.0, "tuck": (7, 9), "slots": {
        "wide": (52, [(4, 5), (20, 40)]), "tall": (22, [(73, 3), (5, 8)]),
        "slit": (60, [(20, 52), (8, 6)]), "square": (17, None)}},
    "port": {"aspect": 0.8, "bottom": 99.0, "tuck": (14, 10), "slots": {
        "wide": (80, [(3, 2), (17, 52)]), "tall": (36, [(61, 10), (3, 30)]),
        "slit": (92, [(4, 40), (4, 8)]), "square": (30, None)}},
}


def png_size(path):
    head = path.read_bytes()[:24]
    return struct.unpack(">II", head[16:24]) if head[:8] == b"\x89PNG\r\n\x1a\n" else None


def panel_file(scene, n, sid):
    return PANELS / f"{scene['stem']}_p{n}_{sid}.png"


def page_layout(scene, style, mode="land"):
    """Where each panel sits on the composed page. Square panels tuck over the previous panel's corner."""
    lay, out, used, prev, page = LAYOUTS[mode], [], {}, None, 0
    for n, (sid, desc) in enumerate(scene["panels"], 1):
        if scene["pages"][n - 1] != page:                  # a new page starts from an empty screen
            page, used, prev = scene["pages"][n - 1], {}, None
        shape = style["shots"][sid]["shape"]
        img = panel_file(scene, n, sid)
        size = png_size(img) if img.exists() else None
        aspect = size[0] / size[1] if size else SHAPE_ASPECT[shape]
        w, spots = lay["slots"][shape]
        if spots:
            x, y = spots[min(used.get(shape, 0), len(spots) - 1)]
        elif prev:
            x, y = prev["x"] + prev["w"] - lay["tuck"][0], prev["y"] + prev["h"] - lay["tuck"][1]
        else:
            x, y = 6, 40
        h = w / aspect * lay["aspect"]
        x = max(1, min(x, 99 - w))
        y = max(1, min(y, lay["bottom"] - h))
        used[shape] = used.get(shape, 0) + 1
        prev = {"n": n, "page": page, "shot": sid, "x": round(x, 1), "y": round(y, 1), "w": w, "h": round(h, 1),
                "img": f"../panels/{img.name}" if size else None, "file": img.name, "content": desc}
        out.append(prev)
    return out


def reveal_plan(scene):
    """Panel revealed by each dialogue line: the [n] tag, else the next unseen panel (0 when none are left)."""
    shown, plan, total = set(), [], len(scene["panels"])
    for tag in scene["reveals"]:
        n = tag or next((i for i in range(1, total + 1) if i not in shown), 0)
        if n:
            shown.add(n)
        plan.append(n)
    return plan


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
const port=location.hash.includes('port')||(!location.hash.includes('land')&&innerWidth<innerHeight);
if(port){stage.style.width='min(100vw,62vh)';stage.style.aspectRatio='4/6.4';D.panels=D.port;
  const b=document.getElementById('box');b.style.height='20%';b.style.bottom='1%'}
const els=D.panels.map(p=>{const e=document.createElement('div');e.className='p'+(p.img?'':' ph');
  e.style.cssText=port?`left:${p.x}%;top:${p.y*0.78}%;width:${p.w}%;height:${p.h*0.78}%`
                      :`left:${p.x}%;top:${p.y}%;width:${p.w}%;height:${p.h}%`;
  if(p.img){const i=new Image();i.src=p.img;e.appendChild(i)}else e.textContent=`P${p.n} ${p.shot}\n${p.content}`;
  stage.insertBefore(e,document.getElementById('box'));return e});
let line=-1,typing=null,z=1;
let page=0;
function reveal(n){const e=els[n-1],p=D.panels[n-1];if(!e)return;if(p.page!==page){page=p.page;els.forEach(x=>x.classList.remove('on'))}
  if(!e.classList.contains('on')){e.style.zIndex=z++;e.classList.add('on')}}
function show(){const l=D.lines[line];reveal(l.reveal);who.textContent=l.speaker;txt.textContent='';arrow.style.display='none';
  let i=0;typing=setInterval(()=>{txt.textContent=l.text.slice(0,++i);if(i>=l.text.length)done()},28)}
function done(){clearInterval(typing);typing=null;txt.textContent=D.lines[line].text;arrow.style.display=''}
function next(){if(typing)return done();
  if(line>=D.lines.length-1){els.forEach(e=>e.classList.remove('on'));z=1;line=-1}
  line++;show()}
stage.onclick=next;document.onkeydown=e=>{if(e.key===' '||e.key==='Enter')next()};
const upto=(location.hash.match(/line(\d+)/)||[])[1];
if(upto){for(let i=0;i<Math.min(+upto,D.lines.length);i++)reveal(D.lines[i].reveal);line=Math.min(+upto,D.lines.length)-1;show();done()}
else if(location.hash.includes('all')){D.panels.forEach(p=>reveal(p.n));if(D.lines.length){line=D.lines.length-1;show();done()}}
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
        panels = page_layout(scene, style, "land")
        lines = [{"speaker": who, "text": text, "reveal": n}
                 for (who, text), n in zip(scene["dialogue"], reveal_plan(scene))]
        data = {"panels": panels, "port": page_layout(scene, style, "port"), "lines": lines}
        OUT.mkdir(exist_ok=True)
        html = OUT / f"{scene['stem']}.preview.html"
        html.write_text(PREVIEW_HTML.replace("__TITLE__", scene["title"]).replace("__DATA__", json.dumps(data)))
        have = sum(1 for p in panels if p["img"])
        print(f"wrote {html.relative_to(ROOT.parent)}  ({have}/{len(panels)} panel images found, "
              f"{len(lines)} dialogue lines). Open it in a browser; tap or press space to advance.")


# ───────────────────────── Export to the game ─────────────────────────

GAME_HEADER = ROOT.parent / "src" / "cutscene_data.h"


def c_str(text):
    for a, b in (("‘", "'"), ("’", "'"), ("“", '"'), ("”", '"'), ("—", " - "), ("…", "...")):
        text = text.replace(a, b)
    text = text.encode("ascii", "replace").decode()              # the game font covers Basic Latin
    return '"' + text.replace("\\", "\\\\").replace('"', '\\"') + '"'


def cmd_export(args):
    style, cast = load_style(), load_cast()
    secs = h2_sections((ROOT / "playlist.md").read_text())
    stems = re.findall(r"^- ([\w-]+)\s*$", secs.get("intro", ""), flags=re.M)
    if not stems:
        die("playlist.md: no scenes listed under '## intro'")
    out = ["// GENERATED by ./story_prompt.py export. Do not edit: change story/scenes/*.md or story/playlist.md.",
           "#pragma once", "",
           "enum CsMood { " + ", ".join(f"CS_{m.upper()}" for m in MOODS) + ", CS_MOOD_COUNT };",
           "struct CsRect { float x, y, w, h; };                 // percent of the stage",
           "struct CsPanel { const char *file; int page; CsRect land, port; };   // a new page clears the screen",
           "struct CsLine { const char *speaker; const char *text; int reveal; CsMood mood; };  // reveal 0 = none",
           "struct CsScene { const char *id; const char *title; bool narration;",
           "                 const CsPanel *panels; int panel_count; const CsLine *lines; int line_count; };", ""]
    table, mood = [], "tense"
    for stem in stems:
        path = SCENES / f"{stem}.md"
        if not path.exists():
            die(f"playlist.md lists '{stem}' but {path.relative_to(ROOT.parent)} does not exist")
        scene = parse_scene(path)
        err, _ = validate(scene, style, cast)
        if err:
            for e in err:
                print(f"{path.name}: ERROR: {e}", file=sys.stderr)
            sys.exit("story_prompt: scene rejected. Fix the scene file; do not bypass the rules.")
        missing = [panel_file(scene, n, sid).name for n, (sid, _) in enumerate(scene["panels"], 1)
                   if not panel_file(scene, n, sid).exists()]
        if scene["panels"] and len(missing) == len(scene["panels"]):
            print(f"skipped {stem}: no panel art generated yet", file=sys.stderr)
            continue
        if missing:                                            # the game draws a placeholder box for these
            print(f"{stem}: {len(missing)} of {len(scene['panels'])} panels have no art yet, shown as placeholders "
                  f"({', '.join(m.split('_', 3)[-1].removesuffix('.png') for m in missing)})", file=sys.stderr)
        ident = re.sub(r"\W", "_", stem)
        if scene["panels"]:
            land, port = page_layout(scene, style, "land"), page_layout(scene, style, "port")
            out.append(f"static const CsPanel CS_{ident}_PANELS[] = {{")
            for a, b in zip(land, port):
                rect = lambda r: "{" + ", ".join(f"{float(r[k]):.1f}f" for k in "xywh") + "}"
                out.append(f'    {{ "{a["file"]}", {a["page"]}, {rect(a)}, {rect(b)} }},')
            out.append("};")
        out.append(f"static const CsLine CS_{ident}_LINES[] = {{")
        for (who, text), reveal, tag in zip(scene["dialogue"], reveal_plan(scene), scene["moods"]):
            mood = tag or mood                                    # an untagged line keeps the current mood
            out.append(f"    {{ {c_str(who)}, {c_str(text)}, {reveal}, CS_{mood.upper()} }},")
        out += ["};", ""]
        title = re.sub(r"^(Scene \d+|Prologue):\s*", "", scene["title"])
        panels = f"CS_{ident}_PANELS, {len(scene['panels'])}" if scene["panels"] else "nullptr, 0"
        table.append(f'    {{ "{stem}", {c_str(title)}, {"true" if scene["narration"] else "false"}, '
                     f'{panels}, CS_{ident}_LINES, {len(scene["dialogue"])} }},')
    if not table:
        die("nothing to export: no listed scene is complete")
    out += ["static const CsScene CS_INTRO[] = {"] + table + ["};",
            "static const int CS_INTRO_COUNT = sizeof(CS_INTRO) / sizeof(CS_INTRO[0]);", ""]
    GAME_HEADER.write_text("\n".join(out))
    print(f"wrote {GAME_HEADER.relative_to(ROOT.parent)}  ({len(table)} of {len(stems)} scenes)")


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
    elif cmd == "export":
        cmd_export(args[1:])
    else:
        print(__doc__.strip())
        sys.exit(0 if cmd in ("", "-h", "--help", "help") else 2)


if __name__ == "__main__":
    main()
