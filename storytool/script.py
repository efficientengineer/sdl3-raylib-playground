"""`script`: rebuild story/v3/chapterNN_script.md, the chapter read as a play.

Every scene the chapter file names, in play order, with the PICTURE that is on screen
described at the moment it appears — the same reveal rule the game and the preview use
(preview.reveal_plan: the line's [n] tag, else the next unseen panel), the page clears
where a `---` starts a new page, and each line's expression tag shown. Whatever sits
between the scenes — the owner's italic play notes — is kept exactly as it was.

This was tools/script/reading_script.py, which had its own scene parser and its own name
table; folding it in means it now shares parse_scene, detok and reveal_plan, so it can
never drift from what the game plays.

Must never do: write a scene file, or invent a heading. It rewrites only the body under a
`## NNNN` heading whose scene file exists, and leaves every other heading untouched.
Public: cmd_script. Imports: md, paths, preview, scenes.
"""
import glob
import re
from pathlib import Path

from .md import die
from .paths import REPO, ROOT, SCENES
from .scenes import parse_scene, reveal_plan


def scene_block(path):
    """One scene as play text: the pictures where they appear, then the lines."""
    sc = parse_scene(path)
    out = []
    loc = sc["meta"].get("location") or sc["meta"].get("backdrop")
    if sc["kind"] != "panels" and loc:
        out.append(f"> *No pictures — a talk scene: {loc[:160]}*\n")
    pages, panels = sc["pages"], sc["panels"]
    cur_page = pages[0] if pages else 0
    seen = set()
    for (who, text), expr, n in zip(sc["dialogue"], sc["exprs"], reveal_plan(sc)):
        if n and n not in seen:
            if pages[n - 1] != cur_page:
                out.append("> — *the page clears* —\n")
                cur_page = pages[n - 1]
            out.append(f"> 🖼 *{panels[n - 1][1]}*\n")
            seen.add(n)
        face = f" *({expr})*" if expr else ""
        out.append(f"**{who}**{face}: {text}  ")
    missing = [i for i in range(1, len(panels) + 1) if i not in seen]
    if missing:
        out.append(f"\n> ⚠ panels never shown by a line: {missing}")
    return "\n".join(out)


def cmd_script(args):
    chap = (args[0] if args else "01").lstrip("-")
    dst = ROOT / "v3" / f"chapter{chap}_script.md"
    if not dst.exists():
        die(f"no {dst.relative_to(REPO)} to rebuild")
    parts = re.split(r"(?m)^(## .*)$", dst.read_text())
    res = [parts[0]]
    for i in range(1, len(parts), 2):
        head, body = parts[i], parts[i + 1]
        m = re.match(r"## (\d{4})", head)
        fs = sorted(glob.glob(str(SCENES / f"{m.group(1)}_*.md"))) if m else []
        if not fs:
            res += [head, body]
            continue
        tail = re.search(r"(?ms)^\*[^*].*", body)        # the italic play note after the scene, if any
        res += [head, "\n\n" + scene_block(Path(fs[0])) + "\n\n"
                + (tail.group(0).rstrip() + "\n\n" if tail else "")]
    dst.write_text("".join(res))
    print("wrote", dst.relative_to(REPO))
