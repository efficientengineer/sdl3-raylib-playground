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

  ./story_prompt.py portraits                  cut each character's reference sheet down to story/portraits/<name>.png
                                               (the middle head-and-shoulders panel; shipped as portrait_<name>.png)

  Field art (FIELD.md): the tool draws the sheet's layout itself as a template PNG, ChatGPT only fills
  the numbered slots, and cutting needs no border detection because the boxes are in the package JSON.
  ./story_prompt.py tiles  grass dirt water    story/out/tiles_....template.png + .chatgpt.md + .sheet.json
  ./story_prompt.py props  well cart sign      slots sized by each prop's footprint in map cells
  ./story_prompt.py walker Bron                the 4x4 walk grid (rows S W E N), the ref sheet attached
  ./story_prompt.py cut    story/out/<name>.sheet.json downloaded.png
                                               key the magenta, cut the slots, write story/field/tiles|props|walkers/
                                               and regenerate story/field/manifest.md ('slice' redirects here)

  ./story_prompt.py preview story/scenes/003_x.md
                                               write story/out/<scene>.preview.html: panels layered manga-style and
                                               revealed line by line with a dialogue box (placeholders if not sliced yet)

  ./story_prompt.py export                     write src/cutscene_data.h from story/playlist.md for the game
                                               (scenes whose panels are not all generated yet are skipped)

  ./story_prompt.py names                      the {{TOKEN}} table from story/v3/NAMES.md, and every token
                                               used in the story that has no row in it (exit 1 if any)

  ./story_prompt.py stats [--all]              per chapter: scenes by type, panels, dialogue lines, optional
                                               scenes, panel art; then totals and the one-off NPC speakers
  ./story_prompt.py packages [--all]           build story/packages/ as a folder tree, by chapter and map:
                                               cast/<name>/refsheet|walker, chNN/<map>/tiles|props|scenes/<scene>,
                                               each with prompt.md, sheet.json, template.png, RETURN_HERE.md
                                               and a README index (one failing scene does not stop the run)
  ./story_prompt.py ingest [folder] [--force]  cut every image saved as returned.png in story/packages/:
                                               shot sheets into panels, templates into tiles/props/walkers,
                                               a reference sheet into story/refs/ plus its portrait

  stats and packages consider the scenes story/playlist.md names; --all takes story/scenes/ as it stands.

Scene types (the '- type:' meta line): panels (default, manga page), narration (text over
black), talk (dialogue box over black or a dimmed backdrop panel, with speaker portraits).

  Single finished page (generators without reference images):
  ./story_prompt.py build  story/scenes/001_x.md | --all    write story/out/001_x.prompt.txt + .json

Style text, shots, and shapes are parsed from story/STYLE.md; character looks from
story/characters.md. Nothing stylistic is hard-coded here except the rule thresholds
below, which mirror the "Composition rules" section of STYLE.md. Keep the two in sync.

Names are tokens (DECISIONS.md D13): the story text says {{HERO}}, story/v3/NAMES.md says what
{{HERO}} is called this week, and every command substitutes before it uses the text, so the game
and ChatGPT only ever see real names and a rename is one line. The files stay tokenised.
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
PORTRAITS = ROOT / "portraits"                      # generated by `portraits` from story/refs/
SHEETS = ROOT / "sheets"                            # every raw generation, kept so a sheet can be re-cut

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
NARRATOR = "narrator"                               # speaker for an unattributed box: no name, no portrait
LOOK_BANNED = ["dwarf", "dwarven", "halfling", "hobbit", "elf", "elven", "gnome", "orc", "fighter",
               "rogue", "cleric", "wizard", "ranger", "barbarian", "paladin", "beard", "bearded"]  # see characters.md
REQUIRED_BLOCKS = ["header", "layout", "framing", "acting", "character_design", "rendering",
                   "dialogue_box", "negative", "sheet_layout", "sheet_avoid", "refsheet", "refsheet_avoid"]

# ── Sheet mechanics (layout maths, not style) ──
SHEET_MAX_PANELS = 8
SHAPE_ASPECT = {"wide": 2.0, "tall": 0.5, "square": 1.0, "slit": 4.0}   # width / height
SCENE_KINDS = ("panels", "narration", "talk")  # order = CsKind enum in the game
PORTRAIT_TRIM = 0.022         # fraction of the panel's short side shaved off, to lose the white border
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


# ───────────────────────── Name tokens (DECISIONS.md D13) ─────────────────────────
#
# Every proper noun in the story text is a token in double braces — {{HERO}}, {{HOME_TOWN}} — and
# story/v3/NAMES.md is the one table that says what each token is currently called. Renaming a
# character is a one-line edit there. The raw files stay tokenised forever; every command that
# *consumes* text (check, brief, sheet, packages, preview, export, stats) substitutes first, so the
# game and ChatGPT only ever see real names.
#
# A token whose table entry is descriptive rather than a name — '*(descriptive)*', '*(unnamed yet)*'
# — has no name yet, so the text reads a plain phrase built from the token ({{SEA_WALL}} -> "the sea
# wall"). A token that is not in the table at all is a validation error: nothing silently ships with
# braces in it.

NAMES_FILE = ROOT / "v3" / "NAMES.md"
TOKEN_RE = re.compile(r"\{\{([A-Z][A-Z0-9_]*)\}\}")
_NAMES = None                                       # {TOKEN: name or None}, loaded once


_ARTICLED = []                                      # table values that arrived with an article on them


def _name_cell(cell):
    """The 'current name' column of a NAMES.md row, or None when it is a note rather than a name.

    A value never carries its own article. The text says 'the {{STAIR}}', so substitution puts the
    value in verbatim and the sentence supplies the article; a value of 'the stair' would read 'the
    the stair'. A leading article in the table is stripped here and reported by `names`."""
    c = re.sub(r"^\*+|\*+$", "", cell.strip().strip("`").strip()).strip()
    c = c.strip('"').strip("“”").strip()
    if not c or re.fullmatch(r"\(.*\)", c):
        return None
    m = re.match(r"^(the|a|an)\s+(.*)$", c, flags=re.I)
    if m:
        _ARTICLED.append(c)
        return m.group(2)
    return c


def load_names():
    """{TOKEN: current name or None} from every markdown table row in story/v3/NAMES.md."""
    global _NAMES
    if _NAMES is None:
        _NAMES = {}
        if NAMES_FILE.exists():
            for line in NAMES_FILE.read_text().splitlines():
                if not line.strip().startswith("|"):
                    continue
                cells = [c.strip() for c in line.strip().strip("|").split("|")]
                m = re.fullmatch(r"`?\{\{([A-Z][A-Z0-9_]*)\}\}`?", cells[0]) if cells else None
                if m and len(cells) >= 2:
                    _NAMES[m.group(1)] = _name_cell(cells[1])
    return _NAMES


def token_phrase(tok):
    """What a token with no name in the table reads as: {{SEA_WALL}} -> 'sea wall', with no article.

    The text writes the article itself ('the {{SEA_WALL}}'), exactly as it does for a named token."""
    return re.sub(r"^the\s+", "", tok.lower().replace("_", " "))


def tokens_in(text):
    return TOKEN_RE.findall(text or "")


def detok(text, unknown=None, unnamed=None):
    """Substitute every {{TOKEN}} with its current name. Unknown tokens are left alone and collected.

    A token that opens a sentence is capitalised, so a descriptive phrase ('the stair') still reads
    as prose wherever the writer put it. A real name is already capitalised, so this is a no-op there."""
    if not text or "{{" not in text:
        return text
    names = load_names()

    def sub(m):
        tok = m.group(1)
        if tok not in names:
            if unknown is not None and tok not in unknown:
                unknown.append(tok)
            return m.group(0)
        word = names[tok]
        if word is None:
            word = token_phrase(tok)
            if unnamed is not None and tok not in unnamed:
                unnamed.append(tok)
        before = text[:m.start()].rstrip(" \t")
        if not before or before[-1] in ".!?:;\"'“”(\n":
            word = word[:1].upper() + word[1:]
        return word
    return TOKEN_RE.sub(sub, text)


def token_of_name(name):
    """The token a written name currently stands for, or None. Used to explain a missing alias."""
    key = name.strip().lower()
    return next((t for t, n in load_names().items() if n and n.lower() == key), None)


def names_table():
    """The token table as markdown, for a brief or a package: token, what it reads as."""
    names = load_names()
    if not names:
        return f"\n(no table found at {NAMES_FILE.relative_to(ROOT.parent)})\n"
    L = ["", "| token | reads as |", "| --- | --- |"]
    L += [f"| `{{{{{t}}}}}` | {n if n else token_phrase(t) + ' _(no name yet)_'} |"
          for t, n in names.items()]
    return "\n".join(L) + "\n"


# Files whose text is tokenised. Everything here is substituted when it is consumed, never rewritten.
def tokenised_files():
    out = sorted(SCENES.glob("*.md")) + [CAST, PLOT, ROOT / "playlist.md"] + list(FIELD_DOCS.values())
    return [p for p in out if p.exists()]


def cmd_names(args):
    """Print the token table, and every token used in the story that has no row in it."""
    names = load_names()
    if not names:
        die(f"{NAMES_FILE.relative_to(ROOT.parent)} not found or has no token table")
    used = {}
    for path in tokenised_files():
        for tok in set(tokens_in(path.read_text())):
            used.setdefault(tok, []).append(path.name)
    print(f"{NAMES_FILE.relative_to(ROOT.parent)} — {len(names)} token(s)\n")
    print(f"{'token':<20}{'reads as':<34}used in")
    print("-" * 78)
    for tok, name in names.items():
        where = used.get(tok, [])
        reads = name if name else f"{token_phrase(tok)}  (no name yet)"
        note = f"{len(where)} file(s)" if where else "unused"
        print(f"{'{{' + tok + '}}':<20}{reads:<34}{note}")
    missing = {t: w for t, w in used.items() if t not in names}
    print()
    if _ARTICLED:
        print(f"{len(_ARTICLED)} value(s) in the table carry their own article and were read without it: "
              f"{', '.join(sorted(set(_ARTICLED)))}. The text writes 'the {{{{TOKEN}}}}', so a value of "
              f"'the x' would reach the screen as 'the the x'. Drop the article in the table.",
              file=sys.stderr)
    if missing:
        print(f"{len(missing)} token(s) used but not in the table — every one of these is a "
              f"validation error:", file=sys.stderr)
        for tok in sorted(missing):
            files = sorted(set(missing[tok]))
            print(f"  {{{{{tok}}}}}  in {', '.join(files[:6])}"
                  + (f" and {len(files) - 6} more" if len(files) > 6 else ""), file=sys.stderr)
        print(f"\nAdd a row for each to {NAMES_FILE.relative_to(ROOT.parent)}, or fix the spelling in the "
              f"scene. Never write the bare name in the text.", file=sys.stderr)
        sys.exit(1)
    unused = [t for t in names if t not in used]
    print("every token used in the story has a row in the table.")
    if unused:
        print(f"not used anywhere yet: {', '.join('{{%s}}' % t for t in unused)}")


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
    cast, unknown = {}, []
    for name, body in h2_sections(detok(CAST.read_text(), unknown)).items():
        kv = kv_lines(body)
        if "look" in kv:
            bad = [w for w in LOOK_BANNED if re.search(rf"\b{w}\b", kv["look"], flags=re.I)]
            if bad:
                die(f"characters.md: {name.title()}'s look contains {bad}. Race, class, and beard words pull image "
                    f"models toward Western fantasy. Describe only what is visible (see 'Design direction').")
            # h2_sections lowercases; recover the display name from the look or title-case it
            cast[name] = {"name": name.title(), **kv,
                          "aliases": [a.strip() for a in kv.get("alias", "").split(",") if a.strip()]}
    if not cast:
        die("characters.md: no characters with a 'look' line")
    if unknown:
        die(f"characters.md: unknown name token(s) {', '.join('{{%s}}' % t for t in unknown)}. "
            f"Every token needs a row in {NAMES_FILE.relative_to(ROOT.parent)}.")
    claimed = {}
    for handle, c in cast.items():
        for a in c["aliases"]:
            key = a.lower()
            if key in cast:
                die(f"characters.md: {c['name']}'s alias '{a}' is already a '## ' handle")
            if key == NARRATOR:
                die(f"characters.md: '{a}' cannot be an alias; the game reserves it for boxes with no speaker")
            if key in claimed:
                die(f"characters.md: alias '{a}' is claimed by both {claimed[key]} and {c['name']}")
            claimed[key] = c["name"]
    for handle, c in cast.items():                   # what each character is called in the story right now
        c["display"] = c["name"]
    for name in load_names().values():               # a token's current name outranks the '## handle'
        handle = name and resolve_name(name, cast)
        if handle:
            cast[handle]["display"] = name
    return cast


def alias_map(cast):
    """{display name lowercased: handle} for every '- alias:' name in characters.md.

    An alias is a display name only: a dialogue speaker, a name on a scene's 'characters:' line, and
    the name the game prints over the box. Aliases are deliberately NOT matched inside panel
    descriptions, where an alias like 'Nine' would fire on 'the Nine Doors'; panel text matches the
    '## Handle' alone, which is why the handles are never common words."""
    return {a.lower(): handle for handle, c in cast.items() for a in c["aliases"]}


def resolve_name(name, cast, aliases=None):
    """The cast handle behind a written name: the handle itself, or one of its aliases. None if neither."""
    key = name.strip().lower()
    if key in cast:
        return key
    return (alias_map(cast) if aliases is None else aliases).get(key)


def is_narrator(speaker):
    return speaker.strip().lower() == NARRATOR


def unknown_speakers(scene, cast, aliases=None):
    """Speakers who are neither a handle, nor an alias, nor the Narrator: the one-off NPCs, in order."""
    out = []
    for who, _ in scene["dialogue"]:
        if not is_narrator(who) and not resolve_name(who, cast, aliases) and who not in out:
            out.append(who)
    return out


def speaker_warnings(scene, cast, aliases):
    """One warning per speaker the cast file does not know.

    A speaker written as a token resolves token -> name -> handle or alias, exactly as an alias does,
    so a name that came out of NAMES.md and still finds nobody means the alias line is missing rather
    than that this is a one-off NPC. Say which."""
    out = []
    for who in unknown_speakers(scene, cast, aliases):
        tok = token_of_name(who)
        if tok:
            out.append(f"dialogue speaker '{who}' is {{{{{tok}}}}} in {NAMES_FILE.relative_to(ROOT.parent)}, "
                       f"but no entry in characters.md carries it on an '- alias:' line, so the game draws "
                       f"no portrait for them")
        else:
            out.append(f"dialogue speaker '{who}' is not in characters.md (fine for one-off NPCs)")
    return out


DOUBLE_ARTICLE = re.compile(r"\b(the|a|an)\s+(the|a|an)\b", flags=re.I)


def token_problems(scene):
    """(errors, warnings) for the {{TOKEN}}s in a scene file, after substitution."""
    err = [f"unknown name token {{{{{t}}}}}: it has no row in {NAMES_FILE.relative_to(ROOT.parent)}. "
           f"Add it to the table there, or fix the spelling; never write the bare name in the scene."
           for t in scene.get("unknown_tokens", [])]
    warn = [f"{{{{{t}}}}} has no name in {NAMES_FILE.relative_to(ROOT.parent)}, so the text reads "
            f"'{token_phrase(t)}' and the sentence must supply the article"
            for t in scene.get("unnamed_tokens", [])]
    # The sentence owns the article, the token never does. 'the {{STAIR}}' with a table value of
    # 'the stair' would reach the screen as 'the the stair', so it is an error, not a warning.
    where = [(f"dialogue line {n}", text) for n, (_, text) in enumerate(scene["dialogue"], 1)]
    where += [(f"panel {n}", desc) for n, (_, desc) in enumerate(scene["panels"], 1)]
    where += [(f"the '- {k}:' line", v) for k, v in scene["meta"].items() if k != "characters"]
    where += [("the '## Beat' paragraph", scene["beat"])]
    for what, text in where:
        m = DOUBLE_ARTICLE.search(text or "")
        if m:
            err.append(f"{what} reads '{squash(text[max(0, m.start() - 20):m.end() + 20])}' after name "
                       f"substitution: two articles in a row. A token's value never carries 'the' or 'a' "
                       f"— the sentence writes it. Fix the line, or the value in "
                       f"{NAMES_FILE.relative_to(ROOT.parent)}.")
    return err, warn


def parse_scene(path):
    raw = path.read_text()
    unknown, unnamed = [], []
    text = detok(raw, unknown, unnamed)              # the file stays tokenised; everything downstream sees names
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
    kind = meta.get("type", "").lower() or "panels"
    return {
        "path": path, "stem": path.stem,
        "title": title.group(1).strip() if title else path.stem,
        "meta": meta,
        "characters": [c.strip() for c in meta.get("characters", "").split(",") if c.strip()],
        "beat": squash(secs.get("beat", "")),
        "panels": panels, "acting": acting, "pages": pages, "dialogue": dialogue, "reveals": reveals, "moods": moods,
        "kind": kind,
        "narration": kind == "narration",
        "talk": kind == "talk",
        "map": meta.get("map", "").strip().lower(),
        "unknown_tokens": unknown, "unnamed_tokens": unnamed,
    }


def speaker_sides(scene, cast=None):
    """{speaker lowercased: 0 left / 1 right}. The first distinct speaker takes the left, the
    second the right, later ones alternate; a speaker keeps their side for the whole scene.

    The Narrator has no portrait, so it takes no side and does not use up one of the two ends.
    A character called by an alias in one line and by their handle in another keeps one side."""
    sides, order = {}, {}
    aliases = alias_map(cast) if cast else {}
    for who, _ in scene["dialogue"]:
        key = who.strip().lower()
        if is_narrator(who):
            sides[key] = 0
            continue
        ident = (resolve_name(who, cast, aliases) if cast else None) or key
        if ident not in order:
            order[ident] = len(order) % 2
        sides[key] = order[ident]
    return sides


def portrait_file(speaker, cast=None):
    """story/portraits/<handle>.png for a speaker, or None when there is none.

    A speaker written as an alias uses the portrait of the handle it resolves to; the Narrator never
    has one (the game draws that line as a box with no name and no portrait)."""
    if is_narrator(speaker):
        return None
    key = (resolve_name(speaker, cast) if cast else None) or speaker.strip().lower()
    p = PORTRAITS / f"{re.sub(r'[^a-z0-9_-]', '', key)}.png"
    return p if p.exists() else None


def backdrop_panel(scene):
    """(scene stem, panel number, panel file Path) for a talk scene's '- backdrop: stem:N', or None.

    Returns the file even when it has not been generated yet; the caller decides whether to warn."""
    spec = scene["meta"].get("backdrop", "").strip()
    m = re.match(r"^([\w-]+)\s*:\s*(\d+)$", spec)
    if not m:
        return None
    other = SCENES / f"{m.group(1)}.md"
    if not other.exists():
        return (m.group(1), int(m.group(2)), None)
    src, n = parse_scene(other), int(m.group(2))
    if not 1 <= n <= len(src["panels"]):
        return (m.group(1), n, None)
    return (m.group(1), n, panel_file(src, n, src["panels"][n - 1][0]))


def name_matches(cast):
    """{written form: handle} for every name allowed to identify a character inside panel text.

    That is the '## handle' itself, and any alias that a name token currently resolves to. An alias
    is otherwise never matched in panel text, because an alias like 'Nine' would fire on 'the Nine
    Doors'. A token's current name is the exception, and has to be: with names tokenised (D13) it is
    the only form a writer ever types, and the table's names are proper nouns by construction."""
    out = {h: h for h in cast}
    for name in load_names().values():
        handle = name and resolve_name(name, cast)
        if handle:
            out[name.lower()] = handle
    return out


def names_in(text, cast):
    """Cast handles named in a piece of panel text, in the cast file's order."""
    found = []
    for written, handle in name_matches(cast).items():
        if handle not in found and re.search(rf"\b{re.escape(written)}\b", text, flags=re.I):
            found.append(handle)
    return found


def acting_for(scene, cast):
    """The scene's acting notes with every name resolved to a cast handle: [{handle: note}] per panel.

    A writer labels an acting line with the name in the panel above it — which, names being tokens, is
    whatever the token currently says. Everything downstream works in handles."""
    aliases = alias_map(cast)
    return [{(resolve_name(who, cast, aliases) or who.lower()): note for who, note in notes.items()}
            for notes in scene["acting"]]


def validate(scene, style, cast):
    """Returns (errors, warnings). Errors block the build."""
    err, warn = token_problems(scene)
    shots, panels = style["shots"], scene["panels"]
    aliases = alias_map(cast)

    for n, mood in enumerate(scene["moods"], 1):
        if mood and mood not in MOODS:
            err.append(f"dialogue line {n}: unknown mood '{{{mood}}}'. Use one of: {', '.join(MOODS)}")
    if scene["kind"] not in SCENE_KINDS:
        err.append(f"unknown '- type: {scene['kind']}'. Use one of: {', '.join(SCENE_KINDS)}")
        return err, warn
    if scene["narration"] or scene["talk"]:
        kind = scene["kind"]
        if panels:
            err.append(f"a {kind} scene has no panels; remove '## Panels' or the 'type: {kind}' line")
        if not scene["dialogue"]:
            err.append(f"a {kind} scene needs at least one line under '## Dialogue'")
        if scene["talk"]:
            for n, r in enumerate(scene["reveals"], 1):
                if r is not None:
                    err.append(f"dialogue line {n}: a talk scene has no panels to reveal; drop the [{r}] tag")
            spec = scene["meta"].get("backdrop", "").strip()
            if spec:
                bd = backdrop_panel(scene)
                if not bd:
                    err.append(f"'- backdrop: {spec}' is malformed; write '<scene_stem>:<panel_number>'")
                elif bd[2] is None:
                    err.append(f"'- backdrop: {spec}' names no existing panel "
                               f"(check story/scenes/{bd[0]}.md and its panel count)")
                elif not bd[2].exists():
                    warn.append(f"backdrop panel {bd[2].name} has not been generated yet; the game shows black")
        warn += speaker_warnings(scene, cast, aliases)
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

    listed = []                                        # handles; a 'characters:' entry may be an alias
    for c in scene["characters"]:
        handle = resolve_name(c, cast, aliases)
        if handle is None:
            err.append(f"R6 known cast: '{c.lower()}' is not in characters.md")
        elif handle not in listed:
            listed.append(handle)
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
    shown = lambda c: cast[c]["display"] if c in cast else c.title()   # the name the writer actually types
    for n, ((sid, desc), notes) in enumerate(zip(panels, acting_for(scene, cast)), 1):
        named = names_in(desc, cast)
        for c in named:
            if c not in notes:
                err.append(f"R11 acting: panel {n} shows {shown(c)} but has no acting line "
                           f"('   - {shown(c)}: where they look, expression, body')")
        for c, note in notes.items():
            if c not in named:
                err.append(f"R11 acting: panel {n} has an acting line for '{shown(c)}' who is not named in the panel")
            if not any(g in note.lower() for g in GAZE_WORDS):
                err.append(f"R11 acting: panel {n}, {shown(c)}: say where the eyes point (looking at..., glaring toward...)")
            if len(note.split()) > MAX_ACTING_WORDS:
                err.append(f"R11 acting: panel {n}, {shown(c)}: {len(note.split())} words, max {MAX_ACTING_WORDS}")
            for w in STYLE_WORDS:
                if re.search(rf"\b{re.escape(w)}\b", note.lower()):
                    err.append(f"R8 no style leakage: panel {n} acting line for {shown(c)} contains '{w}'")
    for c in listed:
        if c not in mentioned:
            warn.append(f"'{shown(c)}' is listed but never named in a panel; their look will not be included")
    for n, r in enumerate(scene["reveals"], 1):
        if r is not None and not 1 <= r <= len(panels):
            err.append(f"dialogue line {n} reveals panel [{r}] but the scene has {len(panels)} panels")
    warn += speaker_warnings(scene, cast, aliases)
    return err, warn


def panel_text(style, cast, sid, desc, notes):
    """One panel's line for the image prompt: shot, content, then who looks where and feels what."""
    text = f"{style['shots'][sid]['prompt']} Content: {desc.rstrip('.')}."
    if notes:
        text += " Acting: " + " ".join(f"{cast[c]['display']}: {note.rstrip('.')}." for c, note in notes.items())
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
    for n, ((sid, desc), notes) in enumerate(zip(panels, acting_for(scene, cast)), 1):
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
            what = f"{len(scene['panels'])} panels" if scene["kind"] == "panels" else \
                   f"{scene['kind']}, {len(scene['dialogue'])} lines"
            print(f"{path.name}: ok ({what}" + (f", {pg} pages)" if pg > 1 else ")"))
            continue
        if scene["talk"]:
            print(f"{path.name}: skipped, a talk scene has no panels to draw. Its only art is the optional "
                  f"'- backdrop:' panel, which belongs to another scene.", file=sys.stderr)
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

# Name tokens (the one rule that outranks the cast file)
Every proper noun you write is a token in double braces, speaker labels included: `{{{{HERO}}}}: line`,
never the name itself. The tool substitutes the current name everywhere text is consumed, so a
rename is a one-line edit in story/v3/NAMES.md and nothing else. A token with no row in that table
is a validation error. Role-only speakers (CLERK, SOLDIER, BAKER) stay plain English and take no
token. These are the tokens that exist; ask before inventing a fifteenth named thing.
{names_table()}
# Scene types (the '- type:' meta line)
Omit it for a panel scene: a manga page, needs '## Panels' and obeys every composition rule below.
'- type: narration' is text over black: no panels, '## Dialogue' only.
'- type: talk' is a field conversation in the manner of Phantasy Star IV: no panels, '## Dialogue'
only, speaker portraits drawn beside the dialogue box, and an optional
'- backdrop: <scene_stem>:<panel_number>' naming an existing panel from another scene to show
dimmed behind the conversation. Talk scenes take no '[n]' reveal tags and need no art of their own.

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
            out.append((rp, f"CHARACTER reference for {cast[c]['display']}. Keep the face, hair, outfit, and "
                            f"colors identical to this image in every panel {cast[c]['display']} appears in. Use it only "
                            f"for the character's design: ignore its background colors and its three-panel layout, "
                            f"and do NOT copy its calm neutral expression or head angle. Expressions come from the acting notes."))
        else:
            warn.append(f"no reference image for {cast[c]['display']} at {cast[c].get('ref')}; "
                        f"run: story_prompt.py refsheet {cast[c]['display']}")
    return out


# ── writing a package into a folder of story/packages/ ──
#
# The same builders serve both shapes. Loose in story/out/ (one writer, one scene at a time) the
# files keep their <name>.chatgpt.md / .sheet.json / .template.png names and the closing instruction
# is the matching `slice` or `cut` command. Inside a story/packages/ folder (the owner's tray, walked
# top to bottom) the names are fixed — prompt.md, sheet.json, template.png — and the closing
# instruction is always the same one: save the download here as returned.png, then run `ingest`.

PKG_DIR, PKG_TITLE = None, None                    # the package folder being written, and its heading
PKG_FILES = {"chatgpt.md": "prompt.md", "sheet.json": "sheet.json", "template.png": "template.png"}
RETURNED = "returned"                              # the file name the owner saves ChatGPT's image under
RETURN_SUFFIXES = (".png", ".jpg", ".jpeg", ".webp", ".heic", ".gif", ".tif", ".tiff")


def pkg_path(name, suffix):
    """Where one of a package's artifacts goes."""
    return (PKG_DIR / PKG_FILES[suffix]) if PKG_DIR is not None else (OUT / f"{name}.{suffix}")


def in_package(directory, title=None):
    """Context manager: write the next package into this folder, under the fixed file names."""
    import contextlib

    @contextlib.contextmanager
    def ctx():
        global PKG_DIR, PKG_TITLE
        was, PKG_DIR = (PKG_DIR, PKG_TITLE), directory
        PKG_TITLE = title
        directory.mkdir(parents=True, exist_ok=True)
        try:
            yield directory
        finally:
            PKG_DIR, PKG_TITLE = was
    return ctx()


def return_lines(what):
    """The closing instruction inside a package folder: one file name, one command, always the same."""
    if PKG_DIR is None:
        return None
    rel = PKG_DIR.relative_to(ROOT.parent)
    return [f"Save ChatGPT's image into **this folder** as `{RETURNED}.png` — the whole path is",
            f"`{rel}/{RETURNED}.png`. A .jpg or .webp works too; the tool converts it.", "",
            "Then cut it up, from the repository root:", "", "```", f"./story_prompt.py ingest {rel}", "```", "",
            f"That writes {what}. Running `./story_prompt.py ingest` with no path does every package in",
            "`story/packages/` that has a new image waiting. The returned file is never deleted, so a",
            "bad cut can always be redone after a fix.", "",
            "If the image needs another go, reply in the same chat and save the new one over "
            f"`{RETURNED}.png`."]


def package(title, attach, prompt, after, status=()):
    rel = lambda p: p.relative_to(ROOT.parent)
    lines = [f"# ChatGPT package: {PKG_TITLE or title}", ""]
    if status:
        lines += ["## What exists already", ""] + list(status) + [""]
    lines += ["## 1. Start a new chat and attach these files, in this order", ""]
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
    if manifest.get("kind") in FIELD_KINDS:              # a field template package: the boxes are known
        return cmd_cut(pos)                              # --trim and --boxes mean nothing here
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


def portrait_from_ref(key, c):
    """One character's dialogue-box portrait, cut out of their reference sheet. (ok, message)."""
    ref = existing(c.get("ref"))
    if not ref:
        return False, (f"no reference sheet for {c['name']} at {c.get('ref')}; "
                       f"run: story_prompt.py refsheet {c['name']}")
    w, h, ch, rows = read_png(ref)
    boxes = find_panels(w, h, ch, rows)
    if len(boxes) != 3:
        return False, (f"{ref.name} yielded {len(boxes)} panel(s), not the reference sheet's 3 "
                       f"(full body | portrait | profile); no portrait written for {c['name']}")
    x0, y0, x1, y1 = sorted(boxes, key=lambda b: b[0] + b[2])[1]          # middle by x = the portrait
    t = max(2, int(min(x1 - x0, y1 - y0) * PORTRAIT_TRIM))                # shave the white border
    x0, y0, x1, y1 = x0 + t, y0 + t, x1 - t, y1 - t
    PORTRAITS.mkdir(exist_ok=True)
    out = PORTRAITS / f"{key}.png"
    write_png(out, x1 - x0, y1 - y0, ch, [rows[y][x0 * ch:x1 * ch] for y in range(y0, y1)])
    return True, f"wrote {out.relative_to(ROOT.parent)}  {x1-x0}x{y1-y0}  (ships as portrait_{key}.png)"


def cmd_portraits(args):
    """story/refs/<name>.png -> story/portraits/<name>.png: the reference sheet's middle panel.

    The sheet is full body | head-and-shoulders portrait | profile (see the 'refsheet' block in
    STYLE.md), so the portrait is the middle box by x. The game draws it beside the dialogue box."""
    cast = load_cast()
    PORTRAITS.mkdir(exist_ok=True)
    made = 0
    for key, c in cast.items():
        ok, msg = portrait_from_ref(key, c)
        print(msg if ok else f"warning: {msg}", file=sys.stdout if ok else sys.stderr)
        made += 1 if ok else 0
    if not made:
        print("warning: no portraits written; the game falls back to a dialogue box with no portrait",
              file=sys.stderr)


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
#bd{position:absolute;inset:0;width:100%;height:100%;object-fit:contain;image-rendering:pixelated;
 filter:brightness(.34) saturate(.75)}
#box{position:absolute;left:4%;right:4%;bottom:2%;height:22%;background:#0b2a7c;border:.45cqw solid #d8d8e0;
 outline:.25cqw solid #5a5a6a;border-radius:.6cqw;box-sizing:border-box;padding:1.4cqw 2cqw;z-index:99}
#who{color:#ffd84a;font-weight:700;font-size:2cqw;margin-bottom:.3cqw}
#txt{font-weight:700;font-size:2.4cqw;line-height:1.3;white-space:pre-wrap}
#face{position:absolute;top:7%;height:86%;width:auto;display:none;box-sizing:border-box;background:#05060f;
 border:.35cqw solid #d8d8e0;outline:.15cqw solid #5a5a6a;object-fit:cover;image-rendering:pixelated}
#arrow{position:absolute;right:1.6cqw;bottom:.6cqw;font-size:2cqw;animation:b 1s steps(2) infinite}
@keyframes b{50%{opacity:0}}
@keyframes fin{from{opacity:0;transform:translateX(var(--fx))}to{opacity:1;transform:none}}
</style>
<div id="stage"><div id="box"><img id="face" alt=""><div id="inner"><div id="who"></div><div id="txt"></div></div>
 <div id="arrow">&#9660;</div></div></div>
<script>
const D=__DATA__, stage=document.getElementById('stage'), who=document.getElementById('who'),
      txt=document.getElementById('txt'), arrow=document.getElementById('arrow'),
      box=document.getElementById('box'), face=document.getElementById('face'),
      inner=document.getElementById('inner');
const port=location.hash.includes('port')||(!location.hash.includes('land')&&innerWidth<innerHeight);
if(D.backdrop){const b=new Image();b.id='bd';b.src=D.backdrop;stage.insertBefore(b,box)}
// Speaker portrait at one end of the box, the text narrowed to make room (Phantasy Star IV field talk).
let faceRight=false,faceKey=null;
function fitFace(){const g=box.clientWidth*0.015,w=face.offsetWidth+g*2;
  inner.style.marginLeft=faceRight?0:w+'px';inner.style.marginRight=faceRight?w+'px':0}
face.onload=fitFace;
function setFace(l){
  if(!l.portrait){face.style.display='none';faceKey=null;inner.style.margin='0';return}
  faceRight=l.side===1;face.style.display='block';
  face.style.left=faceRight?'auto':'1.5%';face.style.right=faceRight?'1.5%':'auto';
  if(l.portrait+faceRight!==faceKey){faceKey=l.portrait+faceRight;face.src=l.portrait;
    face.style.setProperty('--fx',(faceRight?'':'-')+'3cqw');
    face.style.animation='none';void face.offsetWidth;face.style.animation='fin .22s ease-out'}
  fitFace()}
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
function show(){const l=D.lines[line];reveal(l.reveal);setFace(l);who.textContent=l.speaker;txt.textContent='';arrow.style.display='none';
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
        sides = speaker_sides(scene, cast)
        lines = [{"speaker": "" if is_narrator(who) else who, "text": text, "reveal": n,
                  "side": sides[who.strip().lower()],
                  "portrait": (lambda p: f"../portraits/{p.name}" if p else None)(portrait_file(who, cast))}
                 for (who, text), n in zip(scene["dialogue"], reveal_plan(scene))]
        bd = backdrop_panel(scene) if scene["talk"] else None
        data = {"panels": panels, "port": page_layout(scene, style, "port"), "lines": lines,
                "backdrop": f"../panels/{bd[2].name}" if bd and bd[2] and bd[2].exists() else None}
        OUT.mkdir(exist_ok=True)
        html = OUT / f"{scene['stem']}.preview.html"
        html.write_text(PREVIEW_HTML.replace("__TITLE__", scene["title"]).replace("__DATA__", json.dumps(data)))
        have = sum(1 for p in panels if p["img"])
        print(f"wrote {html.relative_to(ROOT.parent)}  ({have}/{len(panels)} panel images found, "
              f"{len(lines)} dialogue lines). Open it in a browser; tap or press space to advance.")


# ───────────────────────── Chapters, stats, and ChatGPT packages ─────────────────────────

EPILOGUE = 16                                       # chapter number the epilogue files are numbered in
PACKAGES = ROOT / "packages"                        # tracked; story/out/ is the scratch copy
CAST_NOTES = ROOT / "notes" / "cast-designer-2.md"  # holds the reference sheet priority order


def chapter_of(stem):
    """Chapter number from a scene file name, or None for the template (which is not a scene).

    The numbering is <chapter><scene> in four digits: 0105 is chapter 1, 1595 chapter 15, 16xx the
    epilogue. The intro files written before that scheme ('p01_prologue', '001'-'003b') are chapter 1."""
    if stem.startswith("000"):
        return None
    m = re.match(r"(\d+)", stem)
    if not m:
        return 1                                    # p01_prologue and anything else unnumbered
    return int(m.group(1)[:2]) if len(m.group(1)) >= 4 else 1


def chapter_label(ch):
    return f"ch{ch:02d}" + (" (epilogue)" if ch == EPILOGUE else "")


PLAYLIST = ROOT / "playlist.md"


def playlist_lists():
    """{section: [(stem, marker)]} for every '## ' list in playlist.md, in file order.

    Sections are 'intro' and 'chapter01'-'chapter16'; a marker is the '(optional)' or '(branch: ...)'
    note after the stem. This is the one list of scenes the game and the art pipeline care about:
    a scene file that no section names is a draft, however finished it looks."""
    if not PLAYLIST.exists():
        die(f"{PLAYLIST.relative_to(ROOT.parent)} not found")
    out = {}
    for name, body in h2_sections(detok(PLAYLIST.read_text())).items():
        # A list entry is '- <stem>' with an optional note after it: '(optional)', '(branch: "..." — ...)'.
        # A stem is a scene file name, so it starts with its number; prose bullets never match.
        rows = [(m.group(1), squash(m.group(2)))
                for m in re.finditer(r"^- ([\w-]+)[ \t]*(.*)$", body, flags=re.M)
                if re.match(r"^(\d|p\d)", m.group(1)) or (SCENES / f"{m.group(1)}.md").exists()]
        if rows:
            out[name] = rows
    return out


def playlist_selection():
    """(stems, numbers) named anywhere in playlist.md.

    Matching by number as well as by stem means a scene that has been renamed (0105_the_long_way_home
    -> 0105_the_water_run) is still recognised while the playlist catches up."""
    stems = {stem for rows in playlist_lists().values() for stem, _ in rows}
    nums = {m.group(1) for s in stems if (m := re.match(r"(\d+)", s))}
    return stems, nums


def in_playlist(stem, selection):
    stems, nums = selection
    m = re.match(r"(\d+)", stem)
    return stem in stems or (m is not None and m.group(1) in nums)


def all_scenes(args=()):
    """[(chapter, scene)] for the scenes under consideration, in file order, skipping the template.

    By default that is the scenes story/playlist.md actually names. Everything else in story/scenes/
    is a draft that no chapter plays: it is not counted, not given a package, and not exported.
    `--all` takes the folder as it stands, which is how a rejected draft is still measurable."""
    every = "--all" in args
    selection = None if every else playlist_selection()
    out = []
    for path in sorted(SCENES.glob("*.md")):
        ch = chapter_of(path.stem)
        if ch is None or (selection and not in_playlist(path.stem, selection)):
            continue
        out.append((ch, parse_scene(path)))
    return out


def is_optional(scene):
    return scene["meta"].get("optional", "no").lower() in ("yes", "true", "1")


def panel_art(scene):
    """(panels with art, panels in the scene) for a panel scene."""
    have = sum(1 for n, (sid, _) in enumerate(scene["panels"], 1) if panel_file(scene, n, sid).exists())
    return have, len(scene["panels"])


def cmd_stats(args):
    cast = load_cast()
    aliases = alias_map(cast)
    chapters, speakers, oneoffs, tokens = {}, {}, {}, {}
    scenes = all_scenes(args)
    skipped = len([p for p in SCENES.glob("*.md") if chapter_of(p.stem) is not None]) - len(scenes)
    print(f"{len(scenes)} scene file(s) in story/scenes/" if "--all" in args else
          f"{len(scenes)} scene(s) named in story/playlist.md" +
          (f"; {skipped} file(s) in story/scenes/ that no list names (add --all to count them)"
           if skipped else ""), end="\n\n")
    for ch, scene in scenes:
        for t in scene["unknown_tokens"]:
            tokens.setdefault(t, []).append(scene["stem"])
        c = chapters.setdefault(ch, {"kinds": {}, "panels": 0, "lines": 0, "optional": [], "art": []})
        c["kinds"][scene["kind"]] = c["kinds"].get(scene["kind"], 0) + 1
        c["panels"] += len(scene["panels"])
        c["lines"] += len(scene["dialogue"])
        if is_optional(scene):
            c["optional"].append(scene["stem"])
        if scene["kind"] == "panels" and scene["panels"]:
            have, total = panel_art(scene)
            if have:
                c["art"].append(f"{scene['stem']} {have}/{total}")
        for who, _ in scene["dialogue"]:
            speakers[who] = speakers.get(who, 0) + 1
            if not is_narrator(who) and not resolve_name(who, cast, aliases):
                oneoffs.setdefault(who, []).append(scene["stem"])

    kinds = sorted({k for c in chapters.values() for k in c["kinds"]}, key=lambda k: (k not in SCENE_KINDS, k))
    head = f"{'chapter':<14}{'scenes':>7}" + "".join(f"{k:>11}" for k in kinds) + \
           f"{'panels':>8}{'lines':>8}{'optional':>10}{'art':>6}"
    print(head)
    print("-" * len(head))
    tot = {"scenes": 0, "panels": 0, "lines": 0, "optional": 0, "art": 0, "kinds": {}}
    for ch in sorted(chapters):
        c = chapters[ch]
        n = sum(c["kinds"].values())
        print(f"{chapter_label(ch):<14}{n:>7}" + "".join(f"{c['kinds'].get(k, 0):>11}" for k in kinds) +
              f"{c['panels']:>8}{c['lines']:>8}{len(c['optional']):>10}{len(c['art']):>6}")
        tot["scenes"] += n
        for k in kinds:
            tot["kinds"][k] = tot["kinds"].get(k, 0) + c["kinds"].get(k, 0)
        for key in ("panels", "lines"):
            tot[key] += c[key]
        tot["optional"] += len(c["optional"])
        tot["art"] += len(c["art"])
    print("-" * len(head))
    print(f"{'total':<14}{tot['scenes']:>7}" + "".join(f"{tot['kinds'].get(k, 0):>11}" for k in kinds) +
          f"{tot['panels']:>8}{tot['lines']:>8}{tot['optional']:>10}{tot['art']:>6}")
    print()
    for ch in sorted(chapters):
        c = chapters[ch]
        if c["optional"]:
            print(f"{chapter_label(ch)} optional: {', '.join(c['optional'])}")
        if c["art"]:
            print(f"{chapter_label(ch)} panel art: {', '.join(c['art'])}")
    print(f"\ndistinct speakers: {len(speakers)}  "
          f"({len(speakers) - len(oneoffs)} cast or Narrator, {len(oneoffs)} one-off)")
    if oneoffs:
        print("one-off speakers (neither a handle, nor an alias, nor Narrator):")
        for who in sorted(oneoffs, key=lambda w: (-len(oneoffs[w]), w.lower())):
            tok = token_of_name(who)
            print(f"  {who:<16}{len(oneoffs[who]):>3} line(s) in {', '.join(sorted(set(oneoffs[who])))}"
                  + (f"   <-- {{{{{tok}}}}}, needs an '- alias: {who}' in characters.md" if tok else ""))
    if tokens:
        print(f"\n{len(tokens)} name token(s) with no row in {NAMES_FILE.relative_to(ROOT.parent)} "
              f"(each one is a validation error):", file=sys.stderr)
        for tok in sorted(tokens):
            print(f"  {{{{{tok}}}}}  in {', '.join(sorted(set(tokens[tok]))[:5])}", file=sys.stderr)


def refsheet_priority(cast):
    """[(handle, tier)] in the cast designer's order, then everyone that file does not rank.

    Parsed from story/notes/cast-designer-2.md '## 5. Reference sheet priority', whose items look
    like '1. Zeph - 2. Pip - ... 12. Tibb and Sela'. Anything unparsable is simply not ranked."""
    order, ranked = [], set()
    if CAST_NOTES.exists():
        body = next((b for name, b in h2_sections(CAST_NOTES.read_text()).items()
                     if "reference sheet priority" in name), "")
        parts = re.split(r"^\*\*(Tier [^*]+)\*\*\s*$", body, flags=re.M)         # [prose, tier, items, ...]
        for i in range(1, len(parts), 2):
            tier = squash(parts[i])
            for item in re.findall(r"\d+\.\s*([^·]+)", squash(parts[i + 1])):   # items split on the middot
                for word in re.findall(r"[A-Z][a-z]+", item):                    # '12. Tibb and Sela', notes and all
                    handle = word.lower()
                    if handle in cast and handle not in ranked:
                        ranked.add(handle)
                        order.append((handle, tier))
    return order + [(h, "unranked") for h in cast if h not in ranked]


def run_quiet(fn, args):
    """Run a command function with its output swallowed. (ok, message); die() is a failure, not the end."""
    import contextlib
    import io
    buf = io.StringIO()
    try:
        with contextlib.redirect_stdout(buf), contextlib.redirect_stderr(buf):
            fn(args)
        return True, ""
    except SystemExit as e:
        why = [l for l in buf.getvalue().splitlines() if "ERROR" in l] or [squash(str(e.code or "rejected"))]
        return False, "; ".join(why)
    except Exception as e:                                      # one bad scene must not stop the run
        return False, f"{type(e).__name__}: {e}"


# ───────────────────────── Field art: template sheets (tiles, props, walkers) ─────────────────────────
#
# The template sheet, from FIELD.md: the tool does not ask ChatGPT to invent a layout. It draws the
# sheet itself — a flat magenta (props, walkers) or black (tiles) canvas with white-bordered slots and
# a painted number beside each — and the prompt says "fill slot 1 with ..., keep the borders and the
# numbers exactly where they are, draw nothing outside a slot". Cutting the returned image needs no
# border detection: the boxes are in the package's .sheet.json because this tool put them there.
#
# The numbers sit in the gutter just outside each slot's top-left corner, never inside it: the cut
# takes the pixels inside the border, and a digit drawn inside would end up baked into the tile.

FIELD = ROOT / "field"
FIELD_DIRS = {"tile": FIELD / "tiles", "prop": FIELD / "props", "walker": FIELD / "walkers"}
FIELD_DOCS = {"tile": FIELD / "tiles.md", "prop": FIELD / "props.md", "walker": FIELD / "walkers.md"}
FIELD_MANIFEST = FIELD / "manifest.md"
FIELD_KINDS = ("tiles", "props", "walker")          # the three commands; singular forms key the dicts above

CELL_PX = 64                          # a map cell is 64 internal pixels (FIELD.md, "Art contract")
TILE_PX = 64                          # every tile is 64x64 and seamless
WALK_W, WALK_H = 32, 48               # one walker frame
WALK_FACINGS = ("S", "W", "E", "N")   # row order
WALK_STEPS = ("stand", "step-left", "stand", "step-right")   # column order
WALK_MIN_SCALE = 4                    # frames drawn at least 4x up so ChatGPT has pixels to work with
TILE_KINDS = ("ground", "wall")

MAGENTA, BLACK, WHITE = (255, 0, 255), (0, 0, 0), (255, 255, 255)
KEY_HARD, KEY_SOFT = 56, 180          # RGB distance to magenta: <= hard is background, <= soft fades out
#   A half-and-half blend of a mid tone with magenta lands around 130, so the soft band has to reach
#   past that or every sprite keeps a pink outline. The band only applies to magenta-hued pixels.
SLOT_BORDER = 3                       # white border drawn inside each slot rectangle
SLOT_MARGIN, SLOT_GUTTER = 52, 48     # canvas margin, gap between slots (the numbers live in the gap)
SLOT_MIN = 96                         # an inner slot smaller than this is not worth asking for
CUT_PAD = 2                           # extra pixels shaved off each side when cutting, for soft borders
DIGIT_SCALE, DIGIT_GAP = 6, 8         # 3x5 digits at 6x are 18x30: they survive a regeneration
TEMPLATE_CANVASES = ((1536, 1024, "landscape, 1536x1024"), (1024, 1536, "portrait, 1024x1536"))

# Expected props and their box in map cells (W across, H tall). props.md's own '- footprint:' line wins;
# this table is the fallback, and an id in neither gets 1x1 and a warning.
PROP_FOOTPRINTS = {
    "house_a": (3, 3), "house_b": (2, 3), "guild_hall": (4, 3), "grain_shed": (3, 3),
    "ladder_house": (3, 3), "well": (1, 2), "cart": (2, 2), "barrel": (1, 1),
    "practice_post": (1, 2), "fence": (2, 1), "tree_a": (2, 3), "tree_b": (1, 2),
    "sign": (1, 2), "milestone": (1, 1),
}
DEFAULT_FOOTPRINT = (1, 1)

# A 3x5 bitmap font, drawn scaled. Slot numbers only, so digits are all it needs.
DIGIT_FONT = {
    "0": ("111", "101", "101", "101", "111"), "1": ("010", "110", "010", "010", "111"),
    "2": ("111", "001", "111", "100", "111"), "3": ("111", "001", "111", "001", "111"),
    "4": ("101", "101", "111", "001", "001"), "5": ("111", "100", "111", "001", "111"),
    "6": ("111", "100", "111", "101", "111"), "7": ("111", "001", "001", "010", "010"),
    "8": ("111", "101", "111", "101", "111"), "9": ("111", "101", "111", "001", "111"),
}


# ── the description files ──

def field_entries(path, what):
    """{id: {'desc': one line, plus any '- key: value'}} from story/field/<what>.md."""
    if not path.exists():
        die(f"{path.relative_to(ROOT.parent)} not found; it holds one '## <id>' entry per {what}")
    out, unknown = {}, []
    for name, body in h2_sections(detok(path.read_text(), unknown)).items():
        if not re.fullmatch(r"[a-z0-9_]+", name):
            die(f"{path.name}: '## {name}' is not a usable id (lower case, digits and underscores only)")
        desc = next((squash(l) for l in body.splitlines()
                     if l.strip() and not re.match(r"^\s*- [\w ]+?:", l)), "")
        out[name] = {"desc": desc, **kv_lines(body)}
    if not out:
        die(f"{path.name}: no '## <id>' entries")
    if unknown:
        die(f"{path.name}: unknown name token(s) {', '.join('{{%s}}' % t for t in unknown)}. "
            f"Every token needs a row in {NAMES_FILE.relative_to(ROOT.parent)}.")
    return out


def field_look(entry, name, where):
    """A walker's look line, checked the same way characters.md is."""
    look = entry.get("look", "")
    bad = [w for w in LOOK_BANNED if re.search(rf"\b{w}\b", look, flags=re.I)]
    if bad:
        die(f"{where}: {name}'s look contains {bad}. Describe only what is visible (see characters.md, "
            f"'Design direction').")
    return look


def check_description(text, ident, where):
    """Descriptions obey R8 and R9 like a panel does: no style words, nothing written on anything."""
    low = text.lower()
    for w in STYLE_WORDS:
        if re.search(rf"\b{re.escape(w)}\b", low):
            die(f"{where}: '{ident}' contains the style word '{w}'. Style comes from STYLE.md, never from a description.")
    for w in TEXT_WORDS:
        if re.search(rf"\b{re.escape(w)}\b", low):
            die(f"{where}: '{ident}' asks for written words ('{w}'). Image models mangle letters; describe the object instead.")


# ── drawing the template ──

def new_canvas(w, h, rgb):
    return [bytearray(bytes(rgb) * w) for _ in range(h)]


def fill_rect(rows, x, y, w, h, rgb):
    W = len(rows[0]) // 3
    x0, y0, x1, y1 = max(0, x), max(0, y), min(W, x + w), min(len(rows), y + h)
    if x1 <= x0 or y1 <= y0:
        return
    span = bytes(rgb) * (x1 - x0)
    for yy in range(y0, y1):
        rows[yy][x0 * 3:x1 * 3] = span


def stroke_rect(rows, x, y, w, h, t, rgb):
    fill_rect(rows, x, y, w, t, rgb)
    fill_rect(rows, x, y + h - t, w, t, rgb)
    fill_rect(rows, x, y + t, t, h - 2 * t, rgb)
    fill_rect(rows, x + w - t, y + t, t, h - 2 * t, rgb)


def draw_digits(rows, text, x, y, scale, rgb):
    """The slot number, 3x5 pixels a digit, scaled. Returns the width drawn."""
    cx = x
    for chsym in text:
        glyph = DIGIT_FONT.get(chsym)
        if glyph:
            for gy, line in enumerate(glyph):
                for gx, on in enumerate(line):
                    if on == "1":
                        fill_rect(rows, cx + gx * scale, y + gy * scale, scale, scale, rgb)
        cx += 4 * scale
    return cx - x


def draw_template(W, H, bg, slots):
    rows = new_canvas(W, H, bg)
    for s in slots:
        x, y, w, h = s["box"]
        stroke_rect(rows, x, y, w, h, SLOT_BORDER, WHITE)
        draw_digits(rows, str(s["n"]), x, y - DIGIT_GAP - 5 * DIGIT_SCALE, DIGIT_SCALE, WHITE)
    return rows


# ── slot layout ──

def pack_cells(sizes, W, H, u, margin=SLOT_MARGIN, gutter=SLOT_GUTTER):
    """Boxes for slots of (w, h) cells at u pixels per cell, packed into rows. None if it overflows.

    Slots in a row are bottom-aligned, so props of different heights stand on one ground line."""
    avail = W - 2 * margin
    rows, cur, used = [], [], 0.0
    for i, (cw, chh) in enumerate(sizes):
        w = cw * u
        if w > avail:
            return None
        if cur and used + gutter + w > avail:
            rows.append(cur)
            cur, used = [], 0.0
        used += (gutter if cur else 0) + w
        cur.append(i)
    rows.append(cur)
    out, y = [None] * len(sizes), float(margin)
    for row in rows:
        rh = max(sizes[i][1] * u for i in row)
        x = float(margin)
        for i in row:
            w, h = sizes[i][0] * u, sizes[i][1] * u
            out[i] = (int(x), int(y + rh - h), int(w), int(h))
            x += w + gutter
        y += rh + gutter
    return out if y - gutter <= H - margin else None


def try_layout_cells(sizes):
    """Pick the canvas and cell size that make the slots biggest. ((W, H, label, u, boxes), "") or (None, why)."""
    best = None
    for W, H, label in TEMPLATE_CANVASES:
        lo, hi = 1.0, float(max(W, H))
        for _ in range(40):
            mid = (lo + hi) / 2
            if pack_cells(sizes, W, H, mid):
                lo = mid
            else:
                hi = mid
        boxes = pack_cells(sizes, W, H, lo)
        if boxes:
            dy = (H - max(b[1] + b[3] for b in boxes) - SLOT_MARGIN) // 2      # sit the block in the canvas
            boxes = [(x, y + dy, w, h) for x, y, w, h in boxes]
        if boxes and (best is None or lo > best[3]):
            best = (W, H, label, lo, boxes)
    if best is None:
        return None, f"{len(sizes)} slots do not fit on any template canvas"
    W, H, label, u, boxes = best
    small = min(min(b[2], b[3]) for b in boxes) - 2 * SLOT_BORDER
    if small < SLOT_MIN:
        return None, (f"{len(sizes)} slots leave the smallest one {small}px across, under {SLOT_MIN}px: "
                      f"ChatGPT has nothing to draw with")
    return best, ""


def layout_cells(sizes, ids):
    """As try_layout_cells, but a sheet that cannot be drawn is the end of the run."""
    best, why = try_layout_cells(sizes)
    if best is None:
        die(f"{why}. Split them across two packages: {', '.join(ids)}")
    return best


def layout_walker():
    """The 4x4 frame grid: the largest whole-number upscale of a 32x48 frame that fits a canvas."""
    gutter = SLOT_GUTTER
    best = None
    for W, H, label in TEMPLATE_CANVASES:
        s = min((W - 2 * SLOT_MARGIN - 3 * gutter) // (4 * WALK_W),
                (H - 2 * SLOT_MARGIN - 3 * gutter) // (4 * WALK_H))
        if s >= WALK_MIN_SCALE and (best is None or s > best[3]):
            best = (W, H, label, s)
    if best is None:
        die(f"a 4x4 grid of {WALK_W}x{WALK_H} frames at {WALK_MIN_SCALE}x does not fit any template canvas")
    W, H, label, s = best
    fw, fh = WALK_W * s, WALK_H * s
    gw, gh = 4 * fw + 3 * gutter, 4 * fh + 3 * gutter
    ox, oy = (W - gw) // 2, (H - gh) // 2
    boxes = [(ox + c * (fw + gutter), oy + r * (fh + gutter), fw, fh) for r in range(4) for c in range(4)]
    return W, H, label, s, boxes


def inner_box(box):
    x, y, w, h = box
    t = SLOT_BORDER
    return (x + t, y + t, w - 2 * t, h - 2 * t)


# ── the package ──

def field_attachments(style, template, ref, warn):
    """[(path, note)] in attach order: the template, the style reference, then a character sheet."""
    out = [(template, "TEMPLATE. Redraw this exact image with every numbered slot filled in and "
                      "everything else left untouched. It is the canvas, not a reference.")]
    sp = existing(style["refs"].get("style"))
    if sp:
        out.append((sp, "STYLE reference. " + style["refs"].get("style_note", "")))
    else:
        warn.append(f"no style reference at {style['refs'].get('style')}; the prompt text carries the style alone")
    if ref:
        out.append(ref)
    return out


TEMPLATE_RULES = (
    "TEMPLATE RULES: Return the whole template at the same size and proportions as the image I "
    "attached. Every white slot border and every slot number stays exactly where it is, the same size "
    "and the same place, down to the pixel. Draw only inside the slots. Everything outside a slot is "
    "left as it is: the margins, the gutters between the slots, and the background behind the numbers. "
    "Nothing crosses a border, nothing leans into a neighbouring slot, nothing is added between the "
    "slots. No text, no labels, no captions, no arrows, no colour swatches, no signature."
)


def field_status(kind, slots, out_file=None):
    """'What exists already' for a field package: one line per file the cut will write."""
    if out_file:
        p = ROOT.parent / out_file
        return [f"- `{out_file}` — " + (f"**exists**, {png_size(p)[0]}x{png_size(p)[1]}" if p.exists() else "missing")]
    seen, L = set(), []
    for s in slots:
        if s["out"] in seen:
            continue
        seen.add(s["out"])
        p = ROOT.parent / s["out"]
        L.append(f"- slot {s['n']} `{s['out']}` — " + ("**exists**" if p.exists() else "missing"))
    have = sum(1 for l in L if "exists" in l)
    return [f"{have} of {len(L)} files in this package have been cut already.", ""] + L


def write_field_package(kind, name, canvas_label, W, H, bg, slots, prompt, attach, after, out_file=None,
                        status=()):
    OUT.mkdir(exist_ok=True)
    template = pkg_path(name, "template.png")
    write_png(template, W, H, 3, draw_template(W, H, bg, slots))
    manifest = pkg_path(name, "sheet.json")
    manifest.write_text(json.dumps({
        "template": name, "kind": kind, "canvas": [W, H], "canvas_label": canvas_label,
        "background": list(bg), "border": SLOT_BORDER, "out": out_file, "slots": slots, "prompt": prompt,
    }, indent=2) + "\n")
    md = pkg_path(name, "chatgpt.md")
    md.write_text(package(f"{kind} template {name}", attach, prompt, after, status=status))
    return template, manifest, md


def field_after(manifest, lines, makes="the files listed above"):
    return lines + [""] + (return_lines(makes) or
                           ["When it passes, download the image and cut it up:", "", "```",
                            f"./story_prompt.py cut {manifest.relative_to(ROOT.parent)} ~/Downloads/<file>.png",
                            "```"])


def cmd_tiles(args):
    ids = [a.lower() for a in args if not a.startswith("--")]
    if not ids:
        die("usage: tiles <id> [<id>...]   (ids come from story/field/tiles.md)")
    entries, style, warn = field_entries(FIELD_DOCS["tile"], "tile"), load_style(), []
    ids = dedupe_ids(ids, warn)
    missing = [i for i in ids if i not in entries]
    if missing:
        die(f"no entry in story/field/tiles.md for: {', '.join(missing)}. Add a '## <id>' with a one-line "
            f"description and a '- kind: ground' or '- kind: wall' line.")
    for i in ids:
        if not entries[i]["desc"]:
            die(f"story/field/tiles.md: '## {i}' has no description line")
        if entries[i].get("kind", "") not in TILE_KINDS:
            die(f"story/field/tiles.md: '## {i}' has '- kind: {entries[i].get('kind', '(none)')}'; "
                f"use one of: {', '.join(TILE_KINDS)}")
        check_description(entries[i]["desc"], i, "story/field/tiles.md")

    W, H, label, u, boxes = layout_cells([(1, 1)] * len(ids), ids)
    slots = [{"n": n, "id": i, "kind": entries[i]["kind"], "box": list(boxes[n - 1]),
              "inner": list(inner_box(boxes[n - 1])), "target": [TILE_PX, TILE_PX],
              "out": str((FIELD_DIRS["tile"] / f"{i}.png").relative_to(ROOT.parent))}
             for n, i in enumerate(ids, 1)]
    name = package_name("tiles", ids)
    b = style["blocks"]
    attach = field_attachments(style, pkg_path(name, "template.png"), None, warn)
    L = [f"Create ONE image: the attached template with all {len(ids)} numbered slots filled in. "
         f"Canvas: {label}, the same size as the template.", b["header"], "",
         "ATTACHED REFERENCE IMAGES, in the order I attached them:"]
    L += [f"Image {n}: {note}" for n, (_, note) in enumerate(attach, 1)]
    L += ["", TEMPLATE_RULES, "",
          "WHAT THESE ARE: ground and wall textures for a 2.5D field map, one per slot. Each slot is "
          "filled edge to edge with its texture, right up to the white border, with no frame, no "
          "vignette, no border of its own and no empty corner. Every tile is SEAMLESS: the right edge "
          "continues into the left edge and the bottom edge into the top, so a floor tiled with copies "
          "of it shows no seam and no repeating landmark. Keep the detail even: no single large feature "
          "that the eye can count across a field, no object, no character, no shadow of anything "
          "outside the tile. All of these tiles belong to one valley town and share one palette and one "
          "pixel size.", "",
          f"A ground tile is seen straight down from directly above. A wall tile is seen level from the "
          f"front, is the face of a step in the ground, and repeats upward as well as sideways. In game "
          f"every tile is {TILE_PX}x{TILE_PX} pixels, so keep the pixels large and the shapes simple.", "",
          "SLOTS:"]
    for s in slots:
        e = entries[s["id"]]
        view = "seen straight down from directly above" if e["kind"] == "ground" else \
               "seen level from the front, the face of a step in the ground"
        L.append(f"Slot {s['n']} ({s['id']}), {e['kind']} tile, {s['inner'][2]}x{s['inner'][3]} px in the "
                 f"template: {e['desc'].rstrip('.')}. {view.capitalize()}, seamless on all four edges.")
    L += ["", f"RENDERING: {b['rendering']}", "",
          f"AVOID: {b['negative']}, drawing outside a slot, moving or covering a slot number, a border or "
          f"frame inside a slot, a visible seam at a tile edge, one big feature in the middle of a tile, "
          f"objects or characters, changing the size of the image"]
    prompt = "\n".join(L)
    template, manifest, md = write_field_package(
        "tiles", name, label, W, H, BLACK, slots, prompt, attach, field_after(manifest_path(name), [
            f"- [ ] All {len(ids)} slots filled, every border and number still exactly where it was",
            "- [ ] Nothing drawn in the gutters; the black between the slots is still flat black",
            "- [ ] Each tile fills its slot to the border, with no frame and no empty corner",
            "- [ ] Tiling test: the left edge of a tile would meet its right edge without a seam",
            "- [ ] No text, labels or swatches anywhere", "",
            "If one tile fails, reply in the same chat: \"Redraw only slot 4 and keep every other slot and "
            "the whole template exactly as it is. <what was wrong>\"."],
            makes=f"the {len(ids)} tile file(s) listed above"), status=field_status("tiles", slots))
    report_field(kind_rows("tile", slots, package_label(name)), warn, md, template, f"{len(ids)} tiles, {label}")


def cmd_props(args):
    ids = [a.lower() for a in args if not a.startswith("--")]
    if not ids:
        die("usage: props <id> [<id>...]   (ids come from story/field/props.md)")
    entries, style, warn = field_entries(FIELD_DOCS["prop"], "prop"), load_style(), []
    ids = dedupe_ids(ids, warn)
    missing = [i for i in ids if i not in entries]
    if missing:
        die(f"no entry in story/field/props.md for: {', '.join(missing)}. Add a '## <id>' with a one-line "
            f"description and a '- footprint: WxH' line.")
    cells = []
    for i in ids:
        if not entries[i]["desc"]:
            die(f"story/field/props.md: '## {i}' has no description line")
        check_description(entries[i]["desc"], i, "story/field/props.md")
        cells.append(footprint_of(i, entries[i], warn))

    W, H, label, u, boxes = layout_cells(cells, ids)
    slots = [{"n": n, "id": i, "cells": list(cells[n - 1]), "box": list(boxes[n - 1]),
              "inner": list(inner_box(boxes[n - 1])),
              "target": [cells[n - 1][0] * CELL_PX, cells[n - 1][1] * CELL_PX],
              "out": str((FIELD_DIRS["prop"] / f"{i}.png").relative_to(ROOT.parent))}
             for n, i in enumerate(ids, 1)]
    name = package_name("props", ids)
    b = style["blocks"]
    attach = field_attachments(style, pkg_path(name, "template.png"), None, warn)
    L = [f"Create ONE image: the attached template with all {len(ids)} numbered slots filled in. "
         f"Canvas: {label}, the same size as the template.", b["header"], "",
         "ATTACHED REFERENCE IMAGES, in the order I attached them:"]
    L += [f"Image {n}: {note}" for n, (_, note) in enumerate(attach, 1)]
    L += ["", TEMPLATE_RULES, "",
          "WHAT THESE ARE: single objects for a 2.5D field map, one object per slot, each cut out "
          "against the flat magenta. The magenta is not a backdrop, it is empty space: it runs right up "
          "to the edge of the object on every side. No ground, no grass, no paving, no base plate, no "
          "cast shadow, no glow, no scenery and no second object in a slot.", "",
          "CAMERA, the same for every slot: a front three-quarter view from slightly above, looking "
          "about fifty degrees down, as if all of these objects stood in one town seen from one fixed "
          "camera. The object sits upright, centred left to right, and touches the bottom edge of its "
          "slot, because that line is where it meets the ground in game.", "",
          f"SCALE: a slot is a grid of map cells, {CELL_PX} pixels to the cell in game, and each slot "
          f"below says how many cells it is. Objects share one scale across the sheet: a four-cell hall "
          f"is four times the width of a one-cell barrel and is drawn with the same size of pixel.", "",
          "SLOTS:"]
    for s in slots:
        cw, chh = s["cells"]
        L.append(f"Slot {s['n']} ({s['id']}), {cw} x {chh} cells, {s['inner'][2]}x{s['inner'][3]} px in the "
                 f"template: {entries[s['id']]['desc'].rstrip('.')}. Front three-quarter view from above, "
                 f"standing on the bottom edge of the slot, magenta on every other side.")
    L += ["", f"RENDERING: {b['rendering']}", "",
          f"AVOID: {b['negative']}, drawing outside a slot, moving or covering a slot number, ground or "
          f"grass or paving under an object, a cast shadow on the magenta, a base plate or pedestal, a "
          f"scene or background inside a slot, two objects in one slot, changing the size of the image"]
    prompt = "\n".join(L)
    template, manifest, md = write_field_package(
        "props", name, label, W, H, MAGENTA, slots, prompt, attach, field_after(manifest_path(name), [
            f"- [ ] All {len(ids)} slots filled, every border and number still exactly where it was",
            "- [ ] The magenta is untouched outside the slots, and comes right up to each object",
            "- [ ] No ground, shadow, base plate or scenery under or behind an object",
            "- [ ] Every object stands on the bottom edge of its slot and shares one camera angle",
            "- [ ] One scale across the sheet: the big buildings really are bigger than the barrel", "",
            "If one prop fails, reply in the same chat: \"Redraw only slot 2 and keep every other slot and "
            "the whole template exactly as it is. <what was wrong>\"."],
            makes=f"the {len(ids)} prop sprite(s) listed above"), status=field_status("props", slots))
    report_field(kind_rows("prop", slots, package_label(name)), warn, md, template, f"{len(ids)} props, {label}")


def cmd_walker(args):
    names = [a for a in args if not a.startswith("--")]
    if len(names) != 1:
        die("usage: walker <Name>   (a character from characters.md, or an id from story/field/walkers.md)")
    style, cast, warn = load_style(), load_cast(), []
    written = names[0]
    ident = re.sub(r"[^a-z0-9_]", "", written.lower())
    handle = resolve_name(written, cast)
    ref = None
    if handle:
        c = cast[handle]
        look, who = c["look"], c["name"]
        rp = existing(c.get("ref"))
        if not rp:
            die(f"{c['name']} has no reference sheet at {c.get('ref')}, so a walk sheet would not match the "
                f"portrait. Generate it first: ./story_prompt.py refsheet {c['name']}")
        ref = (rp, f"CHARACTER reference for {c['name']}. The walker is this character: keep the face, hair, "
                   f"outfit, and colors identical to this image in every frame. Use it for the design only; "
                   f"ignore its background and its three-panel layout.")
    else:
        others = field_entries(FIELD_DOCS["walker"], "walker")
        if ident not in others:
            die(f"'{written}' is neither a name in characters.md (a handle or an '- alias:') nor an id in "
                f"story/field/walkers.md. A story name that has moved on (see story/v3/NAMES.md) belongs on "
                f"an '- alias:' line of its existing entry, not on a renamed heading; a one-off NPC belongs "
                f"in story/field/walkers.md.")
        look, who = field_look(others[ident], ident, "story/field/walkers.md"), ident
        if not look:
            die(f"story/field/walkers.md: '## {ident}' has no '- look:' line")
        check_description(look, ident, "story/field/walkers.md")
        warn.append(f"{ident} has no reference sheet; the look line carries the design alone")

    W, H, label, s, boxes = layout_walker()
    slots = []
    for n, box in enumerate(boxes, 1):
        r, c_ = (n - 1) // 4, (n - 1) % 4
        slots.append({"n": n, "id": f"{ident}_{WALK_FACINGS[r].lower()}{c_ + 1}", "row": r, "col": c_,
                      "facing": WALK_FACINGS[r], "step": WALK_STEPS[c_], "box": list(box),
                      "inner": list(inner_box(box)), "target": [WALK_W, WALK_H],
                      "out": str((FIELD_DIRS["walker"] / f"{ident}.png").relative_to(ROOT.parent))})
    name = f"walker_{ident}"
    b = style["blocks"]
    attach = field_attachments(style, pkg_path(name, "template.png"), ref, warn)
    fw, fh = slots[0]["inner"][2], slots[0]["inner"][3]
    L = [f"Create ONE image: the attached template with all 16 numbered slots filled in. "
         f"Canvas: {label}, the same size as the template.", b["header"], "",
         "ATTACHED REFERENCE IMAGES, in the order I attached them:"]
    L += [f"Image {n}: {note}" for n, (_, note) in enumerate(attach, 1)]
    L += ["", TEMPLATE_RULES, "",
          f"WHAT THIS IS: a walking sprite sheet for {who} in a 2.5D field map, 16 frames of one "
          f"character, cut out against the flat magenta. The magenta is empty space, not a backdrop: it "
          f"runs right up to the figure on every side. No ground, no shadow, no scenery, no props that "
          f"are not part of the costume.", "",
          "THE GRID, four rows of four frames, read left to right, top to bottom:",
          f"Row 1 (slots 1-4), facing S: the character walks toward the camera, seen from the front.",
          f"Row 2 (slots 5-8), facing W: walks to the viewer's left, seen in side view from their right side.",
          f"Row 3 (slots 9-12), facing E: walks to the viewer's right, seen in side view from their left side. "
          f"This is row 2 mirrored, and the costume details stay on the correct side of the body.",
          f"Row 4 (slots 13-16), facing N: walks away from the camera, seen from behind.", "",
          "THE COLUMNS, the same four poses in every row: column 1 standing still with both feet "
          "together; column 2 mid-stride with the left leg forward and the right arm forward; column 3 "
          "the same standing pose as column 1, identical to it; column 4 mid-stride with the right leg "
          "forward and the left arm forward. Frames 1 and 3 must match each other exactly, because the "
          "game plays them as 1, 2, 3, 4 in a loop.", "",
          f"FRAMING, the same in all 16 frames: the character is drawn at the same size, upright and "
          f"centred left to right, with the feet on the bottom edge of the frame and a small gap of "
          f"magenta above the head. The head does not move up or down between frames, so the sheet does "
          f"not bob when it is played. Each frame is {fw}x{fh} pixels here and is squeezed down to "
          f"{WALK_W}x{WALK_H} in game, so keep the pixels large, the silhouette clear and the face simple: "
          f"a few pixels of eye, no fine detail.", "",
          f"CHARACTER: {look}", ""]
    if ref:
        L += ["The character must match the attached reference sheet exactly: same face, hair, outfit, "
              "colors and marks, in all sixteen frames.", ""]
    L += [f"CHARACTER DESIGN: {b['character_design']}", "", f"RENDERING: {b['rendering']}", "",
          f"AVOID: {b['negative']}, drawing outside a slot, moving or covering a slot number, ground or "
          f"shadow under the feet, a different size or costume between frames, the head bobbing between "
          f"frames, a background inside a frame, changing the size of the image"]
    prompt = "\n".join(L)
    out_file = str((FIELD_DIRS["walker"] / f"{ident}.png").relative_to(ROOT.parent))
    template, manifest, md = write_field_package(
        "walker", name, label, W, H, MAGENTA, slots, prompt, attach, field_after(manifest_path(name), [
            "- [ ] All 16 slots filled, every border and number still exactly where it was",
            "- [ ] Rows in the order S, W, E, N; columns stand, step-left, stand, step-right",
            "- [ ] Slots 1 and 3 of each row are the same pose; the head sits at the same height in all 16",
            "- [ ] Feet on the bottom edge, magenta right up to the figure, no shadow and no ground",
            "- [ ] Hair, outfit, colors and marks match the reference sheet in every frame", "",
            "If one row fails, reply in the same chat: \"Redraw only slots 5 to 8 and keep every other slot "
            "and the whole template exactly as it is. <what was wrong>\"."],
            makes=f"`{out_file}`, the 4x4 walk sheet"),
        out_file=out_file, status=field_status("walker", slots, out_file))
    report_field([{"id": ident, "kind": "walker", "target": f"{WALK_W * 4}x{WALK_H * 4}", "package": package_label(name)}],
                 warn, md, template, f"16 frames at {s}x, {label}")


def dedupe_ids(ids, warn):
    out = []
    for i in ids:
        if i in out:
            warn.append(f"'{i}' listed twice; one slot is enough")
        else:
            out.append(i)
    return out


def package_name(kind, ids):
    head = "-".join(ids[:3])
    return f"{kind}_{head}" + (f"-plus{len(ids) - 3}" if len(ids) > 3 else "")


def manifest_path(name):
    return pkg_path(name, "sheet.json")


def package_label(name):
    """What the manifest calls the package that asked for an id: the folder inside the tray if there
    is one, because that is what the owner opens; otherwise the loose story/out/ name."""
    return str(PKG_DIR.relative_to(ROOT.parent)) if PKG_DIR is not None else name


def footprint_of(i, entry, warn):
    m = re.fullmatch(r"\s*(\d+)\s*x\s*(\d+)\s*", entry.get("footprint", ""))
    if m:
        return (int(m.group(1)), int(m.group(2)))
    if i in PROP_FOOTPRINTS:
        return PROP_FOOTPRINTS[i]
    warn.append(f"'{i}' has no '- footprint: WxH' line and is not in PROP_FOOTPRINTS; "
                f"drawn at {DEFAULT_FOOTPRINT[0]}x{DEFAULT_FOOTPRINT[1]} cells")
    return DEFAULT_FOOTPRINT


def kind_rows(kind, slots, package):
    seen, rows = set(), []
    for s in slots:
        if s["id"] in seen:
            continue
        seen.add(s["id"])
        rows.append({"id": s["id"], "kind": kind, "target": f"{s['target'][0]}x{s['target'][1]}",
                     "package": package})
    return rows


def report_field(rows, warn, md, template, what):
    write_field_manifest(rows)
    for w in warn:
        print(f"warning: {w}", file=sys.stderr)
    print(f"wrote {template.relative_to(ROOT.parent)}  ({what})")
    print(f"      {md.relative_to(ROOT.parent)}  — attach the template first, then paste the prompt")
    print(f"      {FIELD_MANIFEST.relative_to(ROOT.parent)} updated")


# ── the manifest ──

def read_manifest_rows():
    """The rows already in story/field/manifest.md: every id ever requested keeps its package name."""
    rows = {}
    if not FIELD_MANIFEST.exists():
        return rows
    for line in FIELD_MANIFEST.read_text().splitlines():
        if not line.startswith("|"):
            continue
        cells = [c.strip().strip("`") for c in line.strip().strip("|").split("|")]
        if len(cells) < 4 or cells[0] in ("id", "") or set(cells[0]) <= set("-: "):
            continue
        rows[(cells[1], cells[0])] = {"id": cells[0], "kind": cells[1], "target": cells[2], "package": cells[3]}
    return rows


def write_field_manifest(new_rows=()):
    """Regenerate story/field/manifest.md: every id ever requested, plus every id the docs declare."""
    rows = read_manifest_rows()
    declared = []
    tiles = field_entries(FIELD_DOCS["tile"], "tile") if FIELD_DOCS["tile"].exists() else {}
    props = field_entries(FIELD_DOCS["prop"], "prop") if FIELD_DOCS["prop"].exists() else {}
    walkers = field_entries(FIELD_DOCS["walker"], "walker") if FIELD_DOCS["walker"].exists() else {}
    for i in tiles:
        declared.append({"id": i, "kind": "tile", "target": f"{TILE_PX}x{TILE_PX}", "package": "-"})
    for i, e in props.items():
        cw, chh = footprint_of(i, e, [])
        declared.append({"id": i, "kind": "prop", "target": f"{cw * CELL_PX}x{chh * CELL_PX}", "package": "-"})
    for i in walkers:
        declared.append({"id": i, "kind": "walker", "target": f"{WALK_W * 4}x{WALK_H * 4}", "package": "-"})
    for r in declared:                                   # a declared id keeps whatever package asked for it
        key = (r["kind"], r["id"])
        r["package"] = rows.get(key, {}).get("package", "-")
        rows[key] = r
    for r in new_rows:
        rows[(r["kind"], r["id"])] = dict(r)

    L = ["# story/field/manifest.md — every field art id",
         "",
         "Generated by `./story_prompt.py tiles|props|walker|cut`. The engine does not read it; people and",
         "agents do. `package` is the last template sheet that asked for the id, `-` if none has yet.",
         "Sizes are the target the pipeline writes; the last column is what is actually on disk.",
         ""]
    for kind, title, note in (("tile", "Tiles", f"{TILE_PX}x{TILE_PX}, opaque, seamless on all four edges"),
                              ("prop", "Props", f"alpha, {CELL_PX} px to a map cell, standing on the bottom row"),
                              ("walker", "Walkers", f"alpha, 4 rows (S, W, E, N) x 4 columns of "
                                                    f"{WALK_W}x{WALK_H} frames")):
        group = sorted((r for (k, _), r in rows.items() if k == kind), key=lambda r: r["id"])
        L += [f"## {title} — {note}", "", "| id | kind | target | package | file |", "| --- | --- | --- | --- | --- |"]
        for r in group:
            f = FIELD_DIRS[kind] / f"{r['id']}.png"
            size = png_size(f) if f.exists() else None
            state = f"yes, {size[0]}x{size[1]}" if size else ("yes" if f.exists() else "missing")
            L.append(f"| `{r['id']}` | {r['kind']} | {r['target']} | `{r['package']}` | {state} |")
        if not group:
            L.append("| — | | | | |")
        L.append("")
    FIELD.mkdir(parents=True, exist_ok=True)
    FIELD_MANIFEST.write_text("\n".join(L))


# ── cutting a returned template ──

def key_magenta(rows, w, h, ch):
    """RGBA rows with the magenta keyed out: flat magenta goes transparent, the fringe fades.

    A pixel is background when it is nearer to magenta than to any colour the art would use, which in
    practice means near-magenta AND magenta-hued (red and blue both above green). The fringe ChatGPT
    leaves around an object is a blend of the object with the magenta, so it gets a part alpha and the
    magenta is taken back out of the colour; otherwise every sprite would have a pink outline."""
    out = []
    for y in range(h):
        src, dst = rows[y], bytearray(w * 4)
        for x in range(w):
            r, g, b = src[x * ch], src[x * ch + 1], src[x * ch + 2]
            d = ((r - 255) ** 2 + g * g + (b - 255) ** 2) ** 0.5
            if d <= KEY_HARD:
                continue                                        # leave the pixel at 0,0,0,0
            a = 255
            if d < KEY_SOFT and r > g and b > g:
                a = int(255 * (d - KEY_HARD) / (KEY_SOFT - KEY_HARD))
                if a < 64:                                      # mostly magenta: call it background
                    continue
                f = a / 255.0                                   # un-matte: observed = f*colour + (1-f)*magenta
                r = min(255, max(0, int((r - 255 * (1 - f)) / f)))
                g = min(255, max(0, int(g / f)))
                b = min(255, max(0, int((b - 255 * (1 - f)) / f)))
            dst[x * 4:x * 4 + 4] = bytes((r, g, b, a))
        out.append(dst)
    return out


def crop(rows, ch, x, y, w, h):
    return [bytearray(rows[yy][x * ch:(x + w) * ch]) for yy in range(y, y + h)]


def resize_nn(rows, w, h, ch, nw, nh):
    out = []
    for y in range(nh):
        src = rows[min(h - 1, y * h // nh)]
        dst = bytearray(nw * ch)
        for x in range(nw):
            sx = min(w - 1, x * w // nw)
            dst[x * ch:(x + 1) * ch] = src[sx * ch:(sx + 1) * ch]
        out.append(dst)
    return out


def opaque_bounds(rows, w, h):
    x0, y0, x1, y1 = w, h, 0, 0
    for y in range(h):
        line = rows[y]
        for x in range(w):
            if line[x * 4 + 3]:
                x0, x1 = min(x0, x), max(x1, x)
                y0, y1 = min(y0, y), max(y1, y)
    return None if x1 < x0 else (x0, y0, x1 - x0 + 1, y1 - y0 + 1)


def scaled_inner(slot, sx, sy, w, h):
    """A slot's inner box in the returned image, with a couple of pixels shaved off for soft borders."""
    ix, iy, iw, ih = slot["inner"]
    x0 = int(round(ix * sx)) + CUT_PAD
    y0 = int(round(iy * sy)) + CUT_PAD
    x1 = int(round((ix + iw) * sx)) - CUT_PAD
    y1 = int(round((iy + ih) * sy)) - CUT_PAD
    if x0 < 0 or y0 < 0 or x1 > w or y1 > h or x1 - x0 < 8 or y1 - y0 < 8:
        die(f"slot {slot['n']} falls outside the image; is this the right file for this package?")
    return x0, y0, x1 - x0, y1 - y0


def cmd_cut(args):
    pos = [a for a in args if not a.startswith("--")]
    if len(pos) != 2:
        die("usage: cut story/out/<name>.sheet.json <image>")
    data = json.loads(Path(pos[0]).read_text())
    image = Path(pos[1]).expanduser()
    kind = data.get("kind")
    if kind not in FIELD_KINDS:
        die(f"{pos[0]} is not a template package (kind '{kind}'). For a shot sheet use: story_prompt.py slice")
    if not image.exists():
        die(f"{image} not found")
    slots, (tw, th) = data["slots"], data["canvas"]
    expect = 16 if kind == "walker" else len(slots)
    if len(slots) != expect:
        die(f"{pos[0]} has {len(slots)} slots, expected {expect}; nothing written")
    w, h, ch, rows = read_png(image)
    sx, sy = w / tw, h / th
    if abs((w / h) / (tw / th) - 1) > 0.06:
        die(f"{image.name} is {w}x{h}, the template is {tw}x{th}: a different shape, so the slot boxes "
            f"would not line up. Ask ChatGPT to redraw the template at its own proportions. Nothing written.")
    bg, off, pts = data.get("background", [0, 0, 0]), 0, []
    for k in range(1, 12):                              # the outer margin: outside every slot, by construction
        m = int(SLOT_MARGIN * sx / 2)
        pts += [(m, k * h // 12), (w - 1 - m, k * h // 12), (k * w // 12, m), (k * w // 12, h - 1 - m)]
    for x, y in pts:
        px = rows[y][x * ch:x * ch + 3]
        off += sum((a - b) ** 2 for a, b in zip(px, bg)) ** 0.5 > 90
    if off > len(pts) * 0.5:
        die(f"{image.name}'s margins are not the template's background colour "
            f"(rgb {tuple(bg)}); {off} of {len(pts)} sampled points differ. Either this is the wrong file for "
            f"this package, or the generator repainted the background. Nothing written.")
    print(f"{image.name}: {w}x{h}, template {tw}x{th} ({sx:.2f}x), {len(slots)} slot(s), kind {kind}")

    if kind == "tiles":
        FIELD_DIRS["tile"].mkdir(parents=True, exist_ok=True)
        for s in slots:
            x, y, bw, bh = scaled_inner(s, sx, sy, w, h)
            px = resize_nn(crop(rows, ch, x, y, bw, bh), bw, bh, ch, TILE_PX, TILE_PX)
            if ch == 4:                                   # a tile is opaque: drop the alpha channel
                px = [bytearray(b for i, b in enumerate(r) if i % 4 != 3) for r in px]
            out = ROOT.parent / s["out"]
            write_png(out, TILE_PX, TILE_PX, 3, px)
            print(f"  slot {s['n']} {s['id']}: {bw}x{bh} -> {out.relative_to(ROOT.parent)}  {TILE_PX}x{TILE_PX}")
    elif kind == "props":
        FIELD_DIRS["prop"].mkdir(parents=True, exist_ok=True)
        for s in slots:
            x, y, bw, bh = scaled_inner(s, sx, sy, w, h)
            px = key_magenta(crop(rows, ch, x, y, bw, bh), bw, bh, ch)
            bounds = opaque_bounds(px, bw, bh)
            if not bounds:
                print(f"  slot {s['n']} {s['id']}: nothing but background in the slot; not written", file=sys.stderr)
                continue
            cx, cy, cw, chh = bounds
            px = crop(px, 4, cx, cy, cw, chh)
            k = s["target"][0] / bw                       # the slot is the cell grid: bring it back to 64 px a cell
            nw, nh = max(1, round(cw * k)), max(1, round(chh * k))
            px = resize_nn(px, cw, chh, 4, nw, nh)
            out = ROOT.parent / s["out"]
            write_png(out, nw, nh, 4, px)
            print(f"  slot {s['n']} {s['id']}: {bw}x{bh} -> trimmed {cw}x{chh} -> "
                  f"{out.relative_to(ROOT.parent)}  {nw}x{nh}")
    else:
        FIELD_DIRS["walker"].mkdir(parents=True, exist_ok=True)
        sheet = [bytearray(WALK_W * 4 * 4) for _ in range(WALK_H * 4)]
        for s in slots:
            x, y, bw, bh = scaled_inner(s, sx, sy, w, h)
            px = key_magenta(crop(rows, ch, x, y, bw, bh), bw, bh, ch)
            px = resize_nn(px, bw, bh, 4, WALK_W, WALK_H)
            ox, oy = s["col"] * WALK_W, s["row"] * WALK_H
            for r in range(WALK_H):
                sheet[oy + r][ox * 4:(ox + WALK_W) * 4] = px[r]
        out = ROOT.parent / data["out"]
        write_png(out, WALK_W * 4, WALK_H * 4, 4, sheet)
        print(f"  16 frames -> {out.relative_to(ROOT.parent)}  {WALK_W * 4}x{WALK_H * 4} "
              f"(rows {', '.join(WALK_FACINGS)})")

    kinds = {"tiles": "tile", "props": "prop", "walker": "walker"}
    rows_out = ([{"id": data["template"].split("_", 1)[1], "kind": "walker",
                  "target": f"{WALK_W * 4}x{WALK_H * 4}", "package": data["template"]}]
                if kind == "walker" else kind_rows(kinds[kind], slots, data["template"]))
    write_field_manifest(rows_out)
    print(f"{FIELD_MANIFEST.relative_to(ROOT.parent)} updated")


# ───────────────────────── The packages tree (the owner's art tray) ─────────────────────────
#
# `packages` builds story/packages/ as a folder tree the owner walks top to bottom. One folder per
# generation: `prompt.md` is self-contained (what exists already, what to attach, the prompt in one
# block, the review checklist), `sheet.json` and `template.png` are the machine's half, and
# `RETURN_HERE.md` says the one thing to do afterwards — save the image in this folder as
# `returned.png` and run `ingest`. The owner never types a path.
#
#   story/packages/README.md                    the index, in the order to generate
#   story/packages/cast/<name>/refsheet/        the character reference sheet (everything attaches it)
#   story/packages/cast/<name>/walker/          that character's 4x4 walk sheet (needs the refsheet)
#   story/packages/ch01/<map>/tiles/            the ground and wall tiles that map uses
#   story/packages/ch01/<map>/props_1/          its props, split so no slot is too small to draw in
#   story/packages/ch01/<map>/scenes/<scene>/   a shot sheet per panel scene played on that map
#
# Rerunning is always safe: it rewrites prompts and templates, never a returned image, and it only
# deletes a folder that has gone out of the playlist AND holds nothing the owner generated.

PKG_META = "package.json"          # the machine index in every package folder; `ingest` reads it
COMMON_MAP = "common"              # a scene or field entry with no '- map:' line yet


def slug(text):
    return re.sub(r"[^a-z0-9_]", "", (text or "").strip().lower())


def first_map(value):
    """The map a '- map:' line names. Several maps means the art lives with the first."""
    return slug((value or "").split(",")[0]) or COMMON_MAP


def known_maps():
    d = FIELD / "maps"
    return sorted(p.stem for p in d.glob("*.map")) if d.exists() else []


def cast_in_play(scenes, cast):
    """Cast handles that the selected scenes actually use, in the order they first turn up."""
    aliases, order = alias_map(cast), []
    for _, sc in scenes:
        for written in sc["characters"] + [w for w, _ in sc["dialogue"]]:
            handle = resolve_name(written, cast, aliases)
            if handle and handle not in order:
                order.append(handle)
    return order


def split_field_ids(ids, sizes):
    """Group ids into sheets that lay out with usable slots. ([[id...]], [(id, why it never fits)])."""
    groups, bad, cur, cur_sizes = [], [], [], []
    for i, s in zip(ids, sizes):
        best, why = try_layout_cells([s])
        if best is None:
            bad.append((i, why))
            continue
        if cur and try_layout_cells(cur_sizes + [s])[0] is None:
            groups.append(cur)
            cur, cur_sizes = [], []
        cur.append(i)
        cur_sizes.append(s)
    if cur:
        groups.append(cur)
    return groups, bad


def pkg_returned(d):
    """The image the owner saved in a package folder, newest first, or None."""
    files = [p for p in d.iterdir() if p.stem == RETURNED and p.suffix.lower() in RETURN_SUFFIXES] \
        if d.is_dir() else []
    return max(files, key=lambda p: p.stat().st_mtime) if files else None


def pkg_state(d, meta):
    """(status word, files cut, files the package makes) — the same answer for README and prompt.md."""
    outs = [ROOT.parent / o for o in meta.get("outputs", [])]
    have = [o for o in outs if o.exists()]
    ret = pkg_returned(d)
    if ret:
        stale = len(have) < len(outs) or any(o.stat().st_mtime < ret.stat().st_mtime for o in have)
        if stale:
            return "**image waiting** — run ingest", len(have), len(outs)
    if outs and len(have) == len(outs):
        return "done", len(have), len(outs)
    if have:
        return f"{len(have)}/{len(outs)} cut", len(have), len(outs)
    return "to generate", 0, len(outs)


def write_return_here(d, meta):
    rel = d.relative_to(ROOT.parent)
    outs = "\n".join(f"- `{o}`" for o in meta.get("outputs", [])) or "- (nothing listed)"
    (d / "RETURN_HERE.md").write_text(f"""# {meta['title']} — save the image here

1. Open `prompt.md` in this folder. Attach the files it lists, in that order, then paste its prompt.
2. Save what ChatGPT gives back **into this folder**, named `returned.png`.
   The whole path is `{rel}/{RETURNED}.png`. A `.jpg` or `.webp` works too; the tool converts it.
3. From the repository root:

```
./story_prompt.py ingest {rel}
```

That cuts the image into:

{outs}

`{RETURNED}.png` is never deleted, so a bad cut can be redone after a fix, and a regeneration is just
saving the new image over it and running `ingest` again. `./story_prompt.py ingest` with no path does
every package in `story/packages/` that has a new image waiting.
""")


def write_package(d, meta, builder, args):
    """Build one package into its folder, then drop the machine index and the return note."""
    d.mkdir(parents=True, exist_ok=True)
    with in_package(d, meta.get("title")):
        ok, msg = run_quiet(builder, args)
    if not ok:
        return False, msg or "no package written"
    (d / PKG_META).write_text(json.dumps({**meta, "dir": str(d.relative_to(ROOT.parent))}, indent=2) + "\n")
    write_return_here(d, meta)
    return True, ""


def cmd_packages(args):
    """Rebuild story/packages/ as the folder tree the owner works through."""
    every = "--all" in args
    cast = load_cast()
    scenes = all_scenes(args)
    PACKAGES.mkdir(parents=True, exist_ok=True)
    index, failures, notes, built = [], [], [], set()

    def add(d, meta, builder, bargs, **row):
        ok, msg = write_package(d, meta, builder, bargs)
        built.add(d.resolve())
        if not ok:
            failures.append((str(d.relative_to(ROOT.parent)), msg))
        index.append({"dir": d, "meta": meta, "ok": ok, **row})

    # ── the cast: a reference sheet, then a walk sheet ──
    # A folder is named for what the character is called now, not for the '## handle' behind it, because
    # the map files name walkers the same way (`hart`, `falke`) and the owner reads the tree, not the code.
    handles = cast_in_play(scenes, cast) or ([h for h, _ in refsheet_priority(cast)] if every else [])
    if every:
        handles += [h for h, _ in refsheet_priority(cast) if h not in handles]
    for handle in handles:
        c = cast[handle]
        who, key = c["display"], slug(c["display"])
        ref = c.get("ref") or f"story/refs/{handle}.png"
        add(PACKAGES / "cast" / key / "refsheet",
            {"kind": "refsheet", "handle": handle, "title": f"{who} — reference sheet", "ref": ref,
             "makes": f"`{ref}` and the dialogue-box portrait cut out of it",
             "outputs": [ref, str((PORTRAITS / f"{handle}.png").relative_to(ROOT.parent))]},
            cmd_refsheet, [handle], group="refsheet", who=who)
        walker_out = str((FIELD_DIRS["walker"] / f"{key}.png").relative_to(ROOT.parent))
        meta = {"kind": "walker", "handle": handle, "title": f"{who} — walk sheet",
                "makes": f"`{walker_out}`, the 4x4 walk sprite sheet", "outputs": [walker_out]}
        d = PACKAGES / "cast" / key / "walker"
        if existing(ref):
            add(d, meta, cmd_walker, [who], group="walker", who=who)
        else:                                    # the walker prompt attaches the reference sheet
            index.append({"dir": d, "meta": meta, "ok": False, "group": "walker", "who": who,
                          "blocked": "needs the reference sheet first"})
    for ident in (field_entries(FIELD_DOCS["walker"], "walker") if FIELD_DOCS["walker"].exists() else {}):
        owner = resolve_name(ident, cast)
        if owner:                                # the cast file has taken this name over since
            notes.append(f"story/field/walkers.md '## {ident}' is now {cast[owner]['display']} in "
                         f"characters.md; the cast entry wins and the walkers.md entry is ignored")
            continue
        out = str((FIELD_DIRS["walker"] / f"{ident}.png").relative_to(ROOT.parent))
        add(PACKAGES / "cast" / ident / "walker",
            {"kind": "walker", "handle": ident, "title": f"{ident} — walk sheet (no cast entry)",
             "makes": f"`{out}`, the 4x4 walk sprite sheet", "outputs": [out]},
            cmd_walker, [ident], group="walker", who=ident)

    # ── the field art, by the map it belongs to ──
    tiles = field_entries(FIELD_DOCS["tile"], "tile") if FIELD_DOCS["tile"].exists() else {}
    props = field_entries(FIELD_DOCS["prop"], "prop") if FIELD_DOCS["prop"].exists() else {}
    panel_scenes = [(ch, sc) for ch, sc in scenes if sc["kind"] == "panels" and sc["panels"]]
    map_chapter, default_ch = {}, min([ch for ch, _ in scenes], default=1)
    for ch, sc in scenes:
        map_chapter.setdefault(first_map(sc["map"]), ch)
    chap_of_map = lambda m: map_chapter.get(m, default_ch)

    shared = {}                    # (map, kind) -> [(id, the map whose package draws it)]
    for kind, entries, dirname in (("tiles", tiles, "tiles"), ("props", props, "props")):
        by_map = {}
        for i, e in entries.items():
            maps = [slug(m) for m in (e.get("map") or "").split(",") if slug(m)] or [COMMON_MAP]
            by_map.setdefault(maps[0], []).append(i)
            for other in maps[1:]:            # drawn once, used on several maps: say so, never redraw it
                shared.setdefault((other, kind), []).append((i, maps[0]))
        for mp in sorted(by_map):
            ids = sorted(by_map[mp])
            sizes = [(1, 1)] * len(ids) if kind == "tiles" else [footprint_of(i, entries[i], []) for i in ids]
            groups, bad = split_field_ids(ids, sizes)
            for i, why in bad:
                failures.append((f"{kind} {i}", why))
            for n, group in enumerate(groups, 1):
                folder = dirname if len(groups) == 1 else f"{dirname}_{n}"
                d = PACKAGES / chapter_label(chap_of_map(mp)).split()[0] / mp / folder
                outs = [str((FIELD_DIRS[kind[:-1]] / f"{i}.png").relative_to(ROOT.parent)) for i in group]
                add(d, {"kind": kind, "map": mp, "ids": group,
                        "title": f"{mp} — {len(group)} {kind}" + (f" ({n} of {len(groups)})" if len(groups) > 1 else ""),
                        "makes": ", ".join(f"`{o}`" for o in outs), "outputs": outs},
                    cmd_tiles if kind == "tiles" else cmd_props, list(group),
                    group=kind, map=mp, chapter=chap_of_map(mp))

    # ── one shot sheet per panel scene, filed under the map it is played on ──
    for ch, sc in panel_scenes:
        mp = first_map(sc["map"])
        d = PACKAGES / chapter_label(ch).split()[0] / mp / "scenes" / sc["stem"]
        outs = [str(panel_file(sc, n, sid).relative_to(ROOT.parent))
                for n, (sid, _) in enumerate(sc["panels"], 1)]
        add(d, {"kind": "sheet", "scene": sc["stem"], "map": mp, "title": f"{sc['stem']} — shot sheet",
                "makes": f"{len(outs)} panel image(s) in `story/panels/`", "outputs": outs},
            cmd_sheet, [str(sc["path"])], group="scene", map=mp, chapter=ch, scene=sc)

    by_stem = {sc["stem"]: sc for _, sc in scenes}                # playlist order, not file order
    played = [by_stem[s] for rows in playlist_lists().values() for s, _ in rows if s in by_stem]
    lead_map = next((first_map(sc["map"]) for sc in played if sc["map"]), None)
    all_maps = {m: chap_of_map(m) for m in known_maps()}          # every map, art or not
    for r in index:
        if r.get("map"):
            all_maps.setdefault(r["map"], r.get("chapter", default_ch))
    for mp, _ in shared:
        all_maps.setdefault(mp, chap_of_map(mp))
    orphans = prune_packages(built)
    write_packages_readme(index, orphans, failures, notes, every, scenes, shared, all_maps, lead_map, cast)
    print(f"wrote {len(built)} package folder(s) under {PACKAGES.relative_to(ROOT.parent)}/")
    for g, label in (("refsheet", "reference sheets"), ("walker", "walk sheets"), ("tiles", "tile sheets"),
                     ("props", "prop sheets"), ("scene", "scene shot sheets")):
        n = sum(1 for r in index if r["group"] == g and r["ok"])
        blocked = sum(1 for r in index if r["group"] == g and r.get("blocked"))
        if n or blocked:
            print(f"  {n:>3} {label}" + (f"   ({blocked} waiting on a reference sheet)" if blocked else ""))
    print(f"index: {(PACKAGES / 'README.md').relative_to(ROOT.parent)}   "
          f"(then: generate -> save as {RETURNED}.png -> ./story_prompt.py ingest)")
    if orphans:
        print(f"{len(orphans)} folder(s) no longer in the playlist but holding a {RETURNED} image were kept; "
              f"they are listed at the end of the README.", file=sys.stderr)
    if failures:
        print(f"\n{len(failures)} package(s) could not be built:", file=sys.stderr)
        for what, why in failures:
            print(f"  {what}: {why}", file=sys.stderr)


def prune_packages(built):
    """Drop package folders that are no longer generated. A folder holding a returned image is kept."""
    orphans = []
    for meta in sorted(PACKAGES.rglob(PKG_META)):
        d = meta.parent
        if d.resolve() in built:
            continue
        if pkg_returned(d):
            orphans.append(d)
            continue
        for p in sorted(d.rglob("*"), reverse=True):
            p.unlink() if p.is_file() else p.rmdir()
        d.rmdir()
    for legacy in PACKAGES.glob("*.chatgpt.md"):          # the flat layout this tree replaced
        legacy.unlink()
    for d in sorted(PACKAGES.rglob("*"), reverse=True):   # tidy the empty shells left behind
        if d.is_dir() and not any(d.iterdir()):
            d.rmdir()
    return orphans


def write_packages_readme(index, orphans, failures, notes, every, scenes, shared, all_maps, lead_map,
                          cast):
    rel = lambda d: str(d.relative_to(PACKAGES))
    cmd = lambda d: f"`ingest {rel(d)}`"
    rows = lambda g: [r for r in index if r["group"] == g]

    def state(r):
        return r.get("blocked") or pkg_state(r["dir"], r["meta"])[0] if r["ok"] or r.get("blocked") \
            else "_could not be built, see the command output_"

    L = ["# story/packages — the art tray", "",
         "Generated by `./story_prompt.py packages`. Every folder below is one ChatGPT generation.",
         "Nothing here is edited by hand; rerun the command after any change to a scene file,",
         "`characters.md`, `story/field/*.md` or `STYLE.md`. Rerunning never touches an image you saved.", "",
         "## The loop", "",
         "1. Open a folder and read `prompt.md`: it lists the files to attach, in order, and the prompt to paste.",
         "2. Generate in ChatGPT, checking the result against the checklist at the end of `prompt.md`.",
         f"3. Save the image into that same folder as `{RETURNED}.png`.",
         "4. From the repository root: `./story_prompt.py ingest` — it finds every waiting image, cuts each",
         "   one into its panels, tiles, sprites or portrait, and says what it wrote.",
         "5. `./fast_reload.sh` to see it on the phone.", "",
         "Work down this page: the short path first, then everything else — reference sheets (every later",
         "prompt attaches them), walk sprites, the field art for each map, the scene shot sheets.", "",
         f"Scenes come from `story/playlist.md`" + ("" if every else " — a scene file no chapter list names "
                                                    "gets no package") + f"; {len(scenes)} scene(s) selected.", ""]

    # ── the short path: the fewest generations that put something on the phone ──
    short, seen = [], set()
    def step(r, what):
        if r and r["dir"] not in seen:
            seen.add(r["dir"])
            short.append((r, what))
    todo_first = lambda rs: sorted(rs, key=lambda r: pkg_state(r["dir"], r["meta"])[0] == "done")
    for r in todo_first(rows("refsheet")):       # what is left to do first, then the ones already cut
        step(r, f"{r['who']}'s reference sheet — the portrait, the walker and every panel come from it")
    for r in todo_first([x for x in rows("walker") if not x.get("blocked") and x["meta"].get("handle") in cast]):
        step(r, f"{r['who']}'s walk sprite — the figure walking the map")
    for g, what in (("tiles", "the ground and walls of {m}"), ("props", "everything standing in {m}")):
        for r in [x for x in index if x["group"] == g and x["map"] == lead_map]:
            step(r, what.format(m=f"`{lead_map}`"))
    for r in [x for x in rows("scene") if x["map"] == lead_map]:
        step(r, f"the shot sheet for `{r['scene']['stem']}` — the first scene the game plays")
    L += [f"## 1. The short path to something on the phone", "",
          "The fewest generations that put the first scene and the first map on screen, in the order they",
          "unblock each other. A checked box is already cut; everything below this section is the long tail.", ""]
    for n, (r, what) in enumerate(short, 1):
        label, have, total = pkg_state(r["dir"], r["meta"])
        box = "x" if label == "done" else " "
        L.append(f"- [{box}] **{n}.** {what}  \n      "
                 f"[`{rel(r['dir'])}`]({rel(r['dir'])}/prompt.md) · {label} · {cmd(r['dir'])}")
    if not short:
        L.append("- (nothing to generate: no cast and no field art in the selected scenes)")
    L += ["", f"Then `./story_prompt.py ingest` and `./fast_reload.sh`.", ""]

    L += ["## 2. Character reference sheets", "",
          "Do these first: the portrait beside the dialogue box, the walk sprite, and every scene panel",
          "are all drawn from this one image.", "",
          "| character | folder | status | after downloading |", "| --- | --- | --- | --- |"]
    for r in rows("refsheet"):
        L.append(f"| {r['who']} | [`{rel(r['dir'])}`]({rel(r['dir'])}/prompt.md) | {state(r)} | {cmd(r['dir'])} |")
    if not rows("refsheet"):
        L.append("| — | | no cast in the selected scenes | |")

    L += ["", "## 3. Walk sprites", "",
          "The 4x4 sheet the field renderer animates (rows S, W, E, N). A character's package is blocked",
          "until their reference sheet exists, because the walker must match it.", "",
          "| who | folder | status | after downloading |", "| --- | --- | --- | --- |"]
    for r in rows("walker"):
        link = f"[`{rel(r['dir'])}`]({rel(r['dir'])}/prompt.md)" if r["ok"] else f"`{rel(r['dir'])}`"
        after = "rerun `packages` once the sheet is in" if r.get("blocked") else cmd(r["dir"])
        L.append(f"| {r['who']} | {link} | {state(r)} | {after} |")
    if not rows("walker"):
        L.append("| — | | | |")

    field = [r for r in index if r["group"] in ("tiles", "props")]
    L += ["", "## 4. Field art, by chapter and map", "",
          "Tiles are the big surfaces (ground, floors, walls); everything else in the world is a prop",
          f"sprite. A map named `{COMMON_MAP}` means the entry carries no `- map:` line yet.", ""]
    for mp, ch in sorted(all_maps.items(), key=lambda kv: (kv[1], kv[0])):
        mine = [x for x in field if x["map"] == mp]
        borrowed = {k: shared.get((mp, k), []) for k in ("tiles", "props")}
        if not mine and not any(borrowed.values()):
            continue                                  # a map with no field art of its own yet
        mapfile = FIELD / "maps" / f"{mp}.map"
        L += [f"### {chapter_label(ch)} — {mp}" + ("" if mapfile.exists() else "  (no map file yet)"), "",
              "| package | makes | status | after downloading |", "| --- | --- | --- | --- |"]
        for r in mine:
            L.append(f"| [`{rel(r['dir'])}`]({rel(r['dir'])}/prompt.md) | {r['meta']['makes']} | "
                     f"{state(r)} | {cmd(r['dir'])} |")
        if not mine:
            L.append("| — | nothing in tiles.md or props.md calls this map home | | |")
        L.append("")
        for kind, ids in borrowed.items():
            if ids:
                L += [f"Shared {kind}, drawn with another map so one id is never drawn twice: "
                      + ", ".join(f"`{i}` (with `{home}`)" for i, home in sorted(ids)), ""]
    if not field:
        L += ["(nothing in `story/field/tiles.md` or `story/field/props.md` yet)", ""]

    scene_rows = rows("scene")
    L += ["## 5. Scene shot sheets, by chapter and map", "",
          "One sheet per panel scene: all of its panels as separate white-bordered rectangles on black,",
          "cut apart into `story/panels/`. Talk and narration scenes need no art and are not listed.", ""]
    for key in sorted({(r["chapter"], r["map"]) for r in scene_rows}):
        ch, mp = key
        L += [f"### {chapter_label(ch)} — {mp}", "",
              "| scene | folder | panels | status | after downloading |", "| --- | --- | --- | --- | --- |"]
        for r in [x for x in scene_rows if (x["chapter"], x["map"]) == key]:
            have, total = panel_art(r["scene"])
            link = f"[`{rel(r['dir'])}`]({rel(r['dir'])}/prompt.md)" if r["ok"] else f"`{rel(r['dir'])}`"
            L.append(f"| `{r['scene']['stem']}` | {link} | {total} | {state(r)} | {cmd(r['dir'])} |")
        L.append("")
    if not scene_rows:
        L += ["(no panel scenes in the selection yet)", ""]

    if notes:
        L += ["## Notes", ""] + [f"- {n}" for n in notes] + [""]
    if orphans:
        L += ["## Kept, but no longer generated", "",
              "These folders left the playlist while holding an image you generated. Nothing deletes them;",
              "move the image somewhere safe and delete the folder by hand when you are done with it.", ""]
        L += [f"- `{rel(d)}`" for d in orphans] + [""]
    if failures:
        L += ["## Could not be built", "",
              "Each of these is a validation error in a scene file or a field description, not a missing",
              "image. Fix the source file and rerun `packages`.", ""]
        L += [f"- `{what}` — {why}" for what, why in failures] + [""]
    (PACKAGES / "README.md").write_text("\n".join(L))


# ───────────────────────── Ingest: one command for every returned image ─────────────────────────

def to_png(src, dst):
    """Copy a returned image to dst as a PNG, converting through sips when it is not one already."""
    dst.parent.mkdir(parents=True, exist_ok=True)
    if src.read_bytes()[:8] == b"\x89PNG\r\n\x1a\n":
        dst.write_bytes(src.read_bytes())
        return
    try:
        subprocess.run(["sips", "-s", "format", "png", str(src), "--out", str(dst)],
                       check=True, capture_output=True)
    except (OSError, subprocess.CalledProcessError):
        die(f"{src.name} is not a PNG and sips could not convert it.")


def archive_returned(d, returned):
    """Keep the raw generation in story/sheets/, which is where raw generations live and is tracked.

    The package folder's own returned.png is gitignored scratch: it can be overwritten by the next
    attempt and thrown away with the folder. The archive copy is named for the package's path so a
    sheet can always be traced back to what asked for it, and re-cut years later."""
    rel = d.relative_to(PACKAGES) if d.is_relative_to(PACKAGES) else Path(d.name)
    out = SHEETS / (str(rel).replace("/", "-") + ".png")
    SHEETS.mkdir(exist_ok=True)
    if out.exists() and out.read_bytes() == returned.read_bytes():
        return None                                          # same image, already kept
    to_png(returned, out)
    return out


def ingest_one(d, meta, returned):
    """Run the right cutter for one package. (ok, message)."""
    kind = meta.get("kind")
    if kind == "sheet":
        return run_quiet(cmd_slice, [str(d / "sheet.json"), str(returned)])
    if kind in FIELD_KINDS:
        return run_quiet(cmd_cut, [str(d / "sheet.json"), str(returned)])
    if kind == "refsheet":
        handle = meta["handle"]
        try:
            to_png(returned, ROOT.parent / meta["ref"])          # the sheet itself is the deliverable
            cast = load_cast()
            if handle not in cast:
                return True, f"saved {meta['ref']}; no characters.md entry, so no portrait"
            ok, msg = portrait_from_ref(handle, cast[handle])    # then the dialogue-box portrait out of it
        except SystemExit as e:
            return False, squash(str(e.code))
        except Exception as e:
            return False, f"{type(e).__name__}: {e}"
        return (True, f"saved {meta['ref']}; {msg}") if ok else (False, f"saved {meta['ref']}, but {msg}")
    return False, f"unknown package kind '{kind}'"


def cmd_ingest(args):
    """Cut every image the owner has saved into story/packages/ since the last run."""
    force = "--force" in args
    roots = [Path(a).expanduser().resolve() for a in args if not a.startswith("--")] or [PACKAGES]
    metas = []
    for r in roots:
        if not r.exists():
            die(f"{r} not found. Give a package folder under {PACKAGES.relative_to(ROOT.parent)}/, "
                f"or no path at all to do them all.")
        metas += [r / PKG_META] if (r / PKG_META).exists() else sorted(r.rglob(PKG_META))
    if not metas:
        die(f"no package folders under {', '.join(str(r) for r in roots)}. Run: ./story_prompt.py packages")

    done, failed, waiting, fresh = 0, 0, 0, 0
    for mf in metas:
        d, meta = mf.parent, json.loads(mf.read_text())
        rel = d.relative_to(ROOT.parent) if d.is_relative_to(ROOT.parent) else d
        returned = pkg_returned(d)
        if not returned:
            waiting += 1
            continue
        status, have, total = pkg_state(d, meta)
        if not force and status == "done":
            fresh += 1
            continue
        kept = archive_returned(d, returned)
        ok, msg = ingest_one(d, meta, returned)
        line = squash(msg) if msg else ""
        if ok:
            done += 1
            after, have, total = pkg_state(d, meta)
            print(f"ok    {rel}  ({returned.name} -> {have}/{total} file(s)){'  ' + line if line else ''}"
                  + (f"  [kept {kept.relative_to(ROOT.parent)}]" if kept else ""))
        else:
            failed += 1
            print(f"FAIL  {rel}  {line}", file=sys.stderr)
    print(f"\n{done} cut, {failed} failed, {fresh} already up to date, {waiting} still waiting for an image "
          f"({len(metas)} package(s))")
    if done:
        print("The game picks the new art up on the next ./fast_reload.sh (it runs portraits and export first).")
    if failed:
        sys.exit(f"story_prompt: {failed} package(s) failed. The returned images are untouched; fix and rerun.")


# ───────────────────────── Export to the game ─────────────────────────

GAME_HEADER = ROOT.parent / "src" / "cutscene_data.h"


def c_str(text):
    for a, b in (("‘", "'"), ("’", "'"), ("“", '"'), ("”", '"'), ("—", " - "), ("…", "...")):
        text = text.replace(a, b)
    text = text.encode("ascii", "replace").decode()              # the game font covers Basic Latin
    return '"' + text.replace("\\", "\\\\").replace('"', '\\"') + '"'


def cmd_export(args):
    style, cast = load_style(), load_cast()
    stems = [stem for stem, _ in playlist_lists().get("intro", [])]
    if not stems:
        die("playlist.md: no scenes listed under '## intro'")
    out = ["// GENERATED by ./story_prompt.py export. Do not edit: change story/scenes/*.md or story/playlist.md.",
           "#pragma once", "",
           "enum CsMood { " + ", ".join(f"CS_{m.upper()}" for m in MOODS) + ", CS_MOOD_COUNT };",
           "enum CsKind { " + ", ".join(f"CS_{k.upper()}" for k in SCENE_KINDS) + " };   // panels = manga page, "
           "narration = text over black, talk = dialogue box over black or a dimmed backdrop",
           "enum CsSide { CS_LEFT, CS_RIGHT };                   // which end of the dialogue box a portrait sits at",
           "struct CsRect { float x, y, w, h; };                 // percent of the stage",
           "struct CsPanel { const char *file; int page; CsRect land, port; };   // a new page clears the screen",
           "struct CsLine { const char *speaker; const char *text; int reveal; CsMood mood;  // reveal 0 = none",
           "                const char *portrait; CsSide side; };  // portrait file, or nullptr when the speaker has none",
           f'#define CS_NARRATOR "{NARRATOR.title()}"              '
           "// speaker of an unattributed box: no name drawn, never a portrait",
           "struct CsScene { const char *id; const char *title; bool narration;   // narration == (kind == CS_NARRATION)",
           "                 CsKind kind; const char *backdrop;   // backdrop: talk scenes only, may be nullptr",
           "                 const CsPanel *panels; int panel_count; const CsLine *lines; int line_count; };", ""]
    table, mood, emitted = [], "tense", set()
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
        if not scene["dialogue"]:                               # the game advances on dialogue lines
            print(f"skipped {stem}: no dialogue lines, so the game could never advance past it", file=sys.stderr)
            continue
        missing = [panel_file(scene, n, sid).name for n, (sid, _) in enumerate(scene["panels"], 1)
                   if not panel_file(scene, n, sid).exists()]
        if scene["panels"] and len(missing) == len(scene["panels"]):
            print(f"skipped {stem}: no panel art generated yet", file=sys.stderr)
            continue
        if missing:                                            # the game draws a placeholder box for these
            print(f"{stem}: {len(missing)} of {len(scene['panels'])} panels have no art yet, shown as placeholders "
                  f"({', '.join(m.split('_', 3)[-1].removesuffix('.png') for m in missing)})", file=sys.stderr)
        ident = re.sub(r"\W", "_", stem)
        if ident in emitted:                                      # a playlist may play the same scene twice
            print(f"{stem}: listed more than once; the second entry reuses the first one's tables", file=sys.stderr)
        else:
            emitted.add(ident)
            if scene["panels"]:
                land, port = page_layout(scene, style, "land"), page_layout(scene, style, "port")
                out.append(f"static const CsPanel CS_{ident}_PANELS[] = {{")
                for a, b in zip(land, port):
                    rect = lambda r: "{" + ", ".join(f"{float(r[k]):.1f}f" for k in "xywh") + "}"
                    out.append(f'    {{ "{a["file"]}", {a["page"]}, {rect(a)}, {rect(b)} }},')
                out.append("};")
            sides = speaker_sides(scene, cast)                    # first speaker left, second right, then alternating
            out.append(f"static const CsLine CS_{ident}_LINES[] = {{")
            for (who, text), reveal, tag in zip(scene["dialogue"], reveal_plan(scene), scene["moods"]):
                mood = tag or mood                                # an untagged line keeps the current mood
                pf = portrait_file(who, cast)                     # an alias uses its handle's portrait
                face = c_str(f"portrait_{pf.name}") if pf else "nullptr"
                side = "CS_RIGHT" if sides[who.strip().lower()] else "CS_LEFT"
                out.append(f"    {{ {c_str(who)}, {c_str(text)}, {reveal}, CS_{mood.upper()}, {face}, {side} }},")
            out += ["};", ""]
        title = re.sub(r"^(Scene \S+|Prologue):\s*", "", scene["title"])
        panels = f"CS_{ident}_PANELS, {len(scene['panels'])}" if scene["panels"] else "nullptr, 0"
        bd = backdrop_panel(scene) if scene["talk"] else None
        backdrop = c_str(bd[2].name) if bd and bd[2] and bd[2].exists() else "nullptr"
        table.append(f'    {{ "{stem}", {c_str(title)}, {"true" if scene["narration"] else "false"}, '
                     f'CS_{scene["kind"].upper()}, {backdrop}, '
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
    elif cmd == "portraits":
        cmd_portraits(args[1:])
    elif cmd == "slice":
        cmd_slice(args[1:])
    elif cmd == "tiles":
        cmd_tiles(args[1:])
    elif cmd == "props":
        cmd_props(args[1:])
    elif cmd == "walker":
        cmd_walker(args[1:])
    elif cmd == "cut":
        cmd_cut(args[1:])
    elif cmd == "preview":
        cmd_preview(args[1:])
    elif cmd == "export":
        cmd_export(args[1:])
    elif cmd == "stats":
        cmd_stats(args[1:])
    elif cmd == "packages":
        cmd_packages(args[1:])
    elif cmd == "ingest":
        cmd_ingest(args[1:])
    elif cmd == "names":
        cmd_names(args[1:])
    else:
        print(__doc__.strip())
        sys.exit(0 if cmd in ("", "-h", "--help", "help") else 2)


if __name__ == "__main__":
    main()
