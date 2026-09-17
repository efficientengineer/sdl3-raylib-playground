#!/usr/bin/env python3
"""Story scene -> image prompt builder. Enforces story/STYLE.md.

  ./story_prompt.py shots                      list the shot menu
  ./story_prompt.py check  story/scenes/*.md   validate scenes against the rules
  ./story_prompt.py build  story/scenes/001_x.md   validate, then write story/out/001_x.prompt.txt + .json
  ./story_prompt.py build  --all               every scene except 000_TEMPLATE
  ./story_prompt.py brief  ["next beat"]       print a brief for an LLM to write the next scene file

Style text, shots, and shapes are parsed from story/STYLE.md; character looks from
story/characters.md. Nothing stylistic is hard-coded here except the rule thresholds
below, which mirror the "Composition rules" section of STYLE.md. Keep the two in sync.
Stdlib only.
"""
import json
import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent / "story"
STYLE, CAST, PLOT = ROOT / "STYLE.md", ROOT / "characters.md", ROOT / "plot.md"
SCENES, OUT = ROOT / "scenes", ROOT / "out"

# ── Rule thresholds (mirror STYLE.md "Composition rules") ──
PANELS_MIN, PANELS_MAX = 2, 4                       # R1
ANCHOR_SCALES = {"wide", "full", "medium"}          # R5
PUNCH_SCALES = {"close", "extreme", "insert"}       # R5
MAX_PANEL_WORDS = 40                                # R7
STYLE_WORDS = ["gradient", "photorealistic", "3d", "blur", "glow", "painterly",
               "realistic", "hd", "4k", "smooth"]   # R8
TEXT_WORDS = ["text", "caption", "speech bubble", "lettering", "written", "inscription reading",
              "sign saying", "sign that says", "sign reading", "the words", "subtitle"]  # R9
REQUIRED_BLOCKS = ["header", "layout", "framing", "character_design", "rendering",
                   "dialogue_box", "negative"]


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
    return {"blocks": blocks, "shapes": shapes, "shots": shots, "sections": secs}


def load_cast():
    if not CAST.exists():
        die(f"{CAST} not found")
    cast = {}
    for name, body in h2_sections(CAST.read_text()).items():
        kv = kv_lines(body)
        if "look" in kv:
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
    dialogue = [(m.group(1).strip(), m.group(2).strip())
                for m in re.finditer(r"^- ([^:\n]+):\s*(.+)$", secs.get("dialogue", ""), flags=re.M)]
    meta = kv_lines(head)
    return {
        "path": path, "stem": path.stem,
        "title": title.group(1).strip() if title else path.stem,
        "meta": meta,
        "characters": [c.strip() for c in meta.get("characters", "").split(",") if c.strip()],
        "beat": squash(secs.get("beat", "")),
        "panels": panels, "dialogue": dialogue,
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


def main():
    args = sys.argv[1:]
    cmd = args[0] if args else ""
    if cmd == "shots":
        cmd_shots()
    elif cmd in ("check", "build"):
        cmd_check_build(args[1:], build=(cmd == "build"))
    elif cmd == "brief":
        cmd_brief(args[1:])
    else:
        print(__doc__.strip())
        sys.exit(0 if cmd in ("", "-h", "--help", "help") else 2)


if __name__ == "__main__":
    main()
