"""Reads story/STYLE.md: the locked prompt blocks, the shot menu, the shapes.

Nothing stylistic is written here — load_style() parses the prose file, and the tool
pastes those blocks into a prompt verbatim. That is the whole point: to change the look
you edit STYLE.md, never a prompt.

Must never do: hand-write or paraphrase prompt text.
Public: load_style. Imports: md, paths, rules.
"""
import re
from .md import die, h2_sections, h3_entries, kv_lines, section, squash
from .paths import STYLE
from .rules import REQUIRED_BLOCKS, SHAPE_ASPECT



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
