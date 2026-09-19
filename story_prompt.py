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

  Field art (parked paths; the tile field below is the live one): the tool draws the sheet's layout
  itself as a template PNG, ChatGPT only fills
  the numbered slots, and cutting needs no border detection because the boxes are in the package JSON.
  ./story_prompt.py tiles  grass dirt water    story/out/tiles_....template.png + .chatgpt.md + .sheet.json
  ./story_prompt.py props  well cart sign      slots sized by each prop's footprint in map cells
  ./story_prompt.py walker Bron [more names]   the 9-frame walk sheet (rows S, side, N; columns stand,
                                               step-A, step-B) at 128x192; up to 3 characters a sheet
  ./story_prompt.py walker compact [<id>...]   an old 16-frame sheet -> the 9-frame one, and the dE
                                               between its E row and a mirror of its W row
  ./story_prompt.py building house_a           three slots a building: the front wall, a wall sample, a
                                               roof sample -> story/field/buildings/<id>_{front,side,roof}.png
  ./story_prompt.py screen halm square         (parked, see the tile field below) ChatGPT paints a map screen
                                               from story/field/screens.md and the game is fitted to it —
                                               one painting, two binary masks, one chat, three messages ->
                                               story/field/screens/<map>_<zone>.screen for the engine
  ./story_prompt.py view   halm square         paint the engine's block-out of one camera zone (the older path):
                                               story/field/views/<map>_<zone>.png -> ..._paint.png
  Tile field (TILES.md, D18/D20) — THE FIELD: 32x32 tiles, an atlas, grid walking. Under TILES2 a
  terrain is one seamless SWATCH and its edges are GENERATED 1-bit masks, not art; a DECAL is a
  cut-out the game hash-scatters over the ground. `tileset` writes masks.png and the swatch, decal
  and stamp packages; `tmap preview` renders a map that way on the Mac.
  ./story_prompt.py tileset valley             read story/field/tilesets/valley/tiles.md, assign any missing
                                               atlas index, and write one template sheet package per sheet
                                               under story/packages/tilesets/valley/<sheet>/ (slots at 4x:
                                               a tile is a 128-px slot, a 4x3 stamp a 512x384 one)
  ./story_prompt.py tmap check   halm | --all  validate story/field/tmaps/<map>.tmap against its tileset
  ./story_prompt.py tmap preview halm          render it from the atlas -> story/out/<map>.tmap.png

  ./story_prompt.py cut    story/out/<name>.sheet.json downloaded.png [--fringe N]
                                               key the magenta (--fringe N shaves N more pixels off the
                                               edge), cut the slots, write story/field/tiles|props|walkers/
                                               and regenerate story/field/manifest.md ('slice' redirects here).
                                               A tileset sheet takes --palette N (quantise the sheet) and
                                               --no-heal (leave a ground tile's wrap alone).

  ./story_prompt.py preview story/scenes/003_x.md
                                               write story/out/<scene>.preview.html: panels layered manga-style and
                                               revealed line by line with a dialogue box (placeholders if not sliced yet)

  ./story_prompt.py export                     write src/cutscene_data.h from story/playlist.md, and
                                               src/field_text.h from story/field/text.md, for the game
                                               (scenes whose panels are not all generated yet are skipped)

  Palette (PALETTE.md, D19) — 256 colours, one byte a pixel, and lighting as table rows.
  ./story_prompt.py palette build              fit story/palette/master.* to the raw returns in
                                               story/sheets + story/refs (cutscene panels excluded:
                                               they get their own palette per scene at slice time),
                                               then the colormap and cycles.md
  ./story_prompt.py palette apply <files> [--palette master|<hex>]
                                               convert anything by hand to an indexed PNG
  ./story_prompt.py palette check              every shipped image is indexed and holds only its own
                                               palette (also run by `check --all`)
  ./story_prompt.py palette colormap           story/palette/colormap.png + .json: day/dusk/night/
                                               flash/poison/stone x 32 light levels

  ./story_prompt.py names                    the {{TOKEN}} table from story/v3/NAMES.md, and every token
                                               used in the story that has no row in it (exit 1 if any)

  ./story_prompt.py stats [--all]              per chapter: scenes by type, panels, dialogue lines, optional
                                               scenes, panel art; then totals and the one-off NPC speakers
  ./story_prompt.py packages [--all]           build story/packages/ as a folder tree, by chapter and map:
                                               cast/<name>/refsheet|walker, chNN/<map>/tiles|props|scenes/<scene>,
                                               each with prompt.md, sheet.json, template.png, RETURN_HERE.md
                                               and a README index (one failing scene does not stop the run)
  ./story_prompt.py ingest [folder] [--force] [--fringe N] [--debug]
                                               cut every image saved as returned.png in story/packages/:
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
import bisect
import json
import math
import random
import re
import struct
import subprocess
import sys
import time
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
PORTRAIT_H = 288              # a portrait ships this tall, aspect kept (tools/sizes/REPORT.md: the
                              # dialogue box draws it about 206 px tall on the phone and 275 at
                              # 1440p, so a ~850-px source was four times the size it is ever shown)
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
    out = (sorted(SCENES.glob("*.md")) + [CAST, PLOT, ROOT / "playlist.md", FIELD_TEXT]
           + list(FIELD_DOCS.values()))
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
    if not build and "--all" in args:                    # the field's examine text answers to the same rules
        err, warn = check_field_text()
        for w in warn:
            print(f"warning: {w}", file=sys.stderr)
        for e in err:
            print(f"ERROR: {e}", file=sys.stderr)
        n = len(load_field_text())
        todo = sum(1 for e in load_field_text().values() if TODO_RE.match(e["raw"]))
        print(f"{FIELD_TEXT.relative_to(ROOT.parent)}: " +
              (f"{len(err)} error(s)" if err else f"ok ({n} line(s), {todo} still a placeholder)"))
        failed += 1 if err else 0
        for mp in all_tmaps():                           # the tile maps answer to their tileset
            terr, twarn = check_tmap(mp)
            for wmsg in twarn:
                print(f"warning: {wmsg}", file=sys.stderr)
            for e in terr:
                print(f"ERROR: {e}", file=sys.stderr)
            print(f"{tmap_path(mp).name}: " + (f"{len(terr)} error(s)" if terr else "ok"))
            failed += 1 if terr else 0
        perr = cmd_palette_check([])                      # D19: every shipped image is indexed art
        failed += 1 if perr else 0
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
    pos, idat, w, plte, trns = 8, b"", 0, b"", b""
    while pos < len(data):
        n, kind = struct.unpack(">I4s", data[pos:pos + 8])
        body = data[pos + 8:pos + 8 + n]
        pos += 12 + n
        if kind == b"IHDR":
            w, h, depth, ctype, _, _, interlace = struct.unpack(">IIBBBBB", body)
            if depth != 8 or ctype not in (0, 2, 3, 6) or interlace:
                die(f"{path.name}: unsupported PNG (need 8-bit grey/RGB/RGBA/indexed, non-interlaced). "
                    f"Fix with: sips -s format png -s formatOptions default {path.name} --out fixed.png")
            ch = {0: 1, 2: 3, 3: 1, 6: 4}[ctype]
        elif kind == b"PLTE":
            plte = body
        elif kind == b"tRNS":
            trns = body
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
    if ctype == 3:                                   # an indexed PNG the palette step wrote: expand it
        alpha = list(trns) + [255] * (len(plte) // 3 - len(trns))
        out = []
        for line in rows:
            o = bytearray()
            for i in line:
                o += plte[i * 3:i * 3 + 3] + bytes((alpha[i],))
            out.append(o)
        return w, h, 4, out
    if ctype == 0:
        return w, h, 3, [bytearray(b for v in line for b in (v, v, v)) for line in rows]
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
        return cmd_cut(args)                             # --trim and --boxes mean nothing there, --fringe does
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
    for scene in sorted({p["scene"] for p in want}):      # D19: a scene's panels get their own palette
        print("  " + palettise_scene(scene))


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
    cw, chh = x1 - x0, y1 - y0
    px = [rows[y][x0 * ch:x1 * ch] for y in range(y0, y1)]
    nw, nh = cw, chh
    if chh > PORTRAIT_H:                      # area-average down to the size it is actually drawn at
        nh = PORTRAIT_H
        nw = max(1, round(cw * PORTRAIT_H / chh))
        px = resample(px, cw, chh, ch, nw, nh)
    ship_png(out, nw, nh, ch, px)
    return True, (f"wrote {out.relative_to(ROOT.parent)}  {nw}x{nh}"
                  + (f"  (from {cw}x{chh})" if (nw, nh) != (cw, chh) else "")
                  + f"  (ships as portrait_{key}.png)")


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
FIELD_DIRS = {"tile": FIELD / "tiles", "prop": FIELD / "props", "walker": FIELD / "walkers",
              "building": FIELD / "buildings"}
FIELD_DOCS = {"tile": FIELD / "tiles.md", "prop": FIELD / "props.md", "walker": FIELD / "walkers.md",
              "building": FIELD / "buildings.md"}
FIELD_MANIFEST = FIELD / "manifest.md"
FIELD_TEXT = FIELD / "text.md"        # examine text: '## <map>.<id>' -> one or two sentences
VIEWS = FIELD / "views"               # <map>_<zone>.png = the engine's block-out; _paint.png = the painting
VIEW_SIZES = ((1280, 720), (640, 360))   # what the engine captures; anything else is a warning
VIEW_ATTACH_MAX = 6                   # prop and building cuts attached alongside the capture
V3_STYLE, SMELLS = ROOT / "v3" / "STYLE.md", ROOT / "v3" / "SMELLS.md"   # the BAN: lists
TEXT_MAX_SENTENCES = 2                # it is a box on a phone that a thumb dismisses
TODO_RE = re.compile(r"^\[[\w.]+:\s*TODO\]$")   # a placeholder the writer has not filled yet
FIELD_NAME_MAX = 24                   # '- name:' is drawn over the box, like a speaker name
FIELD_KINDS = ("tiles", "props", "walker", "building")   # the four commands
KIND_DIR = {"tiles": "tile", "props": "prop", "walker": "walker", "building": "building"}

CELL_PX = 64                          # a map cell is 64 internal pixels (FIELD.md, "Art contract")
TILE_PX = 64                          # every tile is 64x64 and seamless

# Buildings are map geometry with three textures on them, not sprites. The sizes and the mapping are
# the engine's contract (src/FIELD_NOTES.md, "Face-texture contract"); change them there first.
BUILDING_FACES = ("front", "side", "roof")
FACE_PX = {"front": (128, 128), "side": (128, 128), "roof": (64, 64)}
ATLAS_CELL = 128                      # ...but the atlas keeps the art at the 4x the sheet was drawn
                                      # at. The owner compared the sheets with the cut and preferred
                                      # the full-resolution art: nothing is thrown away here any
                                      # more. --snap32 still writes the old 32-px atlas beside it.
EXTRUDE_PX = 2                        # opaque colour bled outward under the transparent edge, so
                                      # linear filtering never pulls black or magenta into a sprite
WALK_W, WALK_H = 32, 48               # one walker frame in LOGICAL px (one tile wide, 1.5 tall)
WALK_OUT_W, WALK_OUT_H = 128, 192     # ...and as stored: 4x logical, which the size study
                                      # (tools/sizes/REPORT.md) measured as exactly the phone's
                                      # display size at 1440p. 256x384 was twice that in each axis.
WALK_FOOT = 4                         # the lowest opaque row sits this far above the frame's bottom
# The 9-frame layout (owner, 2026-09-19): rows S, side, N x columns stand, step-A, step-B. The engine
# MIRRORS the side row for the other direction, so 16 frames became 9 and the four duplicate stand
# frames went away. A character whose design is not symmetric (a sword on one hip, an eyepatch) takes
# `- asymmetric: yes` in characters.md and keeps both side rows: 12 frames, 4 rows.
WALK_ROWS = ("S", "side", "N")
WALK_ROWS_ASYM = ("S", "W", "E", "N")
WALK_STEPS = ("stand", "step-A", "step-B")                   # column order
WALK_FACINGS = ("S", "W", "E", "N")   # the old 4x4 row order, still read by `walker compact`
WALK_OLD_STEPS = ("stand", "step-left", "stand", "step-right")
WALK_MIN_SCALE = 4                    # frames drawn at least 4x up so ChatGPT has pixels to work with
WALK_MAX_PER_SHEET = 3                # characters on one walk sheet. The frames would fit six, but
                                      # each needs its reference sheet attached and past three
                                      # attachments ChatGPT starts blending the designs.
TILE_KINDS = ("ground", "wall")

MAGENTA, BLACK, WHITE = (255, 0, 255), (0, 0, 0), (255, 255, 255)
KEY_HARD = 56                         # RGB distance to magenta: at or under this the pixel is background
#   Distance to magenta is a bad measure of how much magenta is *in* a pixel. A black outline blended
#   half and half with the background lands at rgb(126,0,128) — 181 away from pure magenta, further
#   than a mid grey is — so any distance threshold either keeps it (a purple outline on the phone,
#   which is what the first real cut showed) or eats real colour. What does measure it is the magenta
#   cast, min(r, b) - g: magenta is the one colour with red and blue at the top and green at the
#   bottom, so for a blend with a roughly neutral foreground the cast is about 255 * the magenta
#   fraction, and 255 - cast is the alpha. That estimate only gets applied near the background, so an
#   object may be any colour it likes in its own interior.
KEY_EDGE_REACH = 3                    # how many pixels in from the background the anti-aliasing reaches
KEY_CAST_MIN = 16                     # a smaller cast than this is the artist's colour, not the background
KEY_CAST_CLEAN = 24                   # cast still left after un-matting: the pixel needs a neighbour's colour
KEY_MIN_ALPHA = 64                    # under this the engine's 0.5 alpha test drops the pixel anyway
KEY_CLEAN_RADIUS = 4                  # how far to look for a clean opaque neighbour to borrow a colour from
SLOT_BORDER = 3                       # white border drawn inside each slot rectangle
SLOT_MARGIN, SLOT_GUTTER = 52, 48     # canvas margin, gap between slots (the numbers live in the gap)
SLOT_MIN = 96                         # an inner slot smaller than this is not worth asking for
CUT_PAD = 2                           # extra pixels shaved off each side when cutting, for soft borders
DIGIT_SCALE, DIGIT_GAP = 6, 8         # 3x5 digits at 6x are 18x30: they survive a regeneration
TEMPLATE_CANVASES = ((1536, 1024, "landscape, 1536x1024"), (1024, 1536, "portrait, 1024x1536"))

# Expected props and their box in map cells (W across, H tall). props.md's own '- footprint:' line wins;
# this table is the fallback, and an id in neither gets 1x1 and a warning.
PROP_FOOTPRINTS = {                   # houses and halls are not here: a building is geometry, see buildings.md
    "well": (1, 2), "cart": (2, 2), "barrel": (1, 1), "practice_post": (1, 2), "fence": (2, 1),
    "tree_a": (2, 3), "tree_b": (1, 2), "sign": (1, 2), "milestone": (1, 1),
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


def draw_template(W, H, bg, slots, digit=None):
    """The template: a white border round each slot and its number in the gutter above it.

    `digit` is (scale, gap); the tile sheets pass a smaller one because their gutters are tight."""
    scale, gap = digit or (DIGIT_SCALE, DIGIT_GAP)
    rows = new_canvas(W, H, bg)
    for s in slots:
        x, y, w, h = s["box"]
        stroke_rect(rows, x, y, w, h, SLOT_BORDER, WHITE)
        draw_digits(rows, str(s["n"]), x, y - gap - 5 * scale, scale, WHITE)
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


def inner_box(box):
    x, y, w, h = box
    t = SLOT_BORDER
    return (x + t, y + t, w - 2 * t, h - 2 * t)


# ── the package ──

def field_rendering(blocks):
    """The rendering block for field art: `field_rendering` if STYLE.md has one, else `rendering`.

    They differ in one sentence. The cutscene block asks for checkerboard dithering in skies, walls
    and shadows, which was right when a panel was an RGB image of its own; under D19 every field
    image is one 256-colour palette and flat cel tones, and a dithered wall becomes two colours
    fighting on a tile that then repeats across a map. The panel blocks are untouched."""
    return blocks.get("field_rendering") or blocks["rendering"]


def field_attachments(style, template, ref, warn):
    """[(path, note)] in attach order: the template, the style reference, then a character sheet."""
    out = [(template, "TEMPLATE. Redraw this exact image with every numbered slot filled in and "
                      "everything else left untouched. It is the canvas, not a reference.")]
    # The field's style reference is OUR OWN approved test sheet, not the third-party screenshot the
    # cutscene path uses: the owner picked that look (flat luminous colour, big simple shapes, soft
    # painted edges, sparse detail, cool blue-violet shadows) off a sheet we generated, so the safest
    # thing to hand the generator is the sheet it already agreed to.
    sp = existing(style["refs"].get("style_map")) or existing(style["refs"].get("style"))
    if sp:
        out.append((sp, "STYLE reference. " + (style["refs"].get("style_note_map")
                                               or style["refs"].get("style_note", ""))))
    else:
        warn.append(f"no style reference at {style['refs'].get('style')}; the prompt text carries the style alone")
    if ref:
        out.append(ref)
    pa = palette_attachment(warn)
    if pa:
        out.append(pa)
    return out


PALETTE_NOTE = ("the game's COLOUR PALETTE, one material ramp a row: use only these colours.")


def palette_attachment(warn=None):
    """The master swatch, attached LAST to every package whose art shares the field's palette (D19).

    Last on purpose: it is not a reference for what to draw, it is the set of colours to draw it in,
    and a generator takes the last image as the most recent instruction. A package drawn before this
    existed keeps working — the slots do not move, only the prompt gains a line."""
    if not MASTER_SWATCH.exists():
        if warn is not None:
            warn.append(f"no palette swatch at {MASTER_SWATCH.relative_to(ROOT.parent)}; "
                        f"run: ./story_prompt.py palette build")
        return None
    return (MASTER_SWATCH, PALETTE_NOTE)


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
                        status=(), digit=None):
    OUT.mkdir(exist_ok=True)
    template = pkg_path(name, "template.png")
    write_png(template, W, H, 3, draw_template(W, H, bg, slots, digit))
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
    L += ["", f"RENDERING: {field_rendering(b)}", "",
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
    L += ["", f"RENDERING: {field_rendering(b)}", "",
          f"AVOID: {b['negative']}, drawing outside a slot, moving or covering a slot number, ground or "
          f"grass or paving under an object, a cast shadow on the magenta, a base plate or pedestal, a "
          f"scene or background inside a slot, two objects in one slot, a soft blurred or glowing edge "
          f"where an object meets the magenta, changing the size of the image"]
    prompt = "\n".join(L)
    template, manifest, md = write_field_package(
        "props", name, label, W, H, MAGENTA, slots, prompt, attach, field_after(manifest_path(name), [
            f"- [ ] All {len(ids)} slots filled, every border and number still exactly where it was",
            "- [ ] The magenta is untouched outside the slots, and comes right up to each object",
            "- [ ] The edge of every object is hard against the magenta, not soft, blurred or glowing",
            "- [ ] No ground, shadow, base plate or scenery under or behind an object",
            "- [ ] Every object stands on the bottom edge of its slot and shares one camera angle",
            "- [ ] One scale across the sheet: the big buildings really are bigger than the barrel", "",
            "If one prop fails, reply in the same chat: \"Redraw only slot 2 and keep every other slot and "
            "the whole template exactly as it is. <what was wrong>\"."],
            makes=f"the {len(ids)} prop sprite(s) listed above"), status=field_status("props", slots))
    report_field(kind_rows("prop", slots, package_label(name)), warn, md, template, f"{len(ids)} props, {label}")


def layout_walker(blocks):
    """The frame grid for `blocks` = [rows per character]. Three columns a character, side by side.

    Nine frames instead of sixteen (owner, 2026-09-19) and a frame stored at 128x192 instead of
    256x384 (tools/sizes/REPORT.md) means a walk sheet is 384x576 — small enough that several
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


def walker_ident(written):
    return re.sub(r"[^a-z0-9_]", "", written.lower())


def walker_meta_path(ident):
    return FIELD_DIRS["walker"] / f"{ident}.json"


def write_walker_meta(ident, rows, cols=None):
    """The sheet's own layout, beside the sheet. The engine reads this, not a hard-coded 4."""
    p = walker_meta_path(ident)
    p.parent.mkdir(parents=True, exist_ok=True)
    p.write_text(json.dumps({
        "id": ident, "rows": list(rows), "cols": list(cols or WALK_STEPS),
        "frame": [WALK_OUT_W, WALK_OUT_H],
        "sheet": [WALK_OUT_W * len(cols or WALK_STEPS), WALK_OUT_H * len(rows)],
        "mirror_side": "side" in rows,
        "note": "row 'side' is drawn facing WEST and MIRRORED for east. A sheet with rows "
                "S, W, E, N is a character whose design is not symmetric and is never mirrored.",
    }, indent=2) + "\n")
    return p


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


def cmd_building(args):
    """Three slots per building: the whole front wall, a wall sample, a roof sample.

    A building is not a sprite. The engine raises the box from the map and stretches the front across
    the front wall, so the door and the windows are painted into that one square and land wherever the
    artist put them; the side and the roof are seamless material, no openings and no landmark."""
    ids = [a.lower() for a in args if not a.startswith("--")]
    if not ids:
        die("usage: building <id> [<id>...]   (ids come from story/field/buildings.md)")
    entries, style, warn = field_entries(FIELD_DOCS["building"], "building"), load_style(), []
    ids = dedupe_ids(ids, warn)
    missing = [i for i in ids if i not in entries]
    if missing:
        die(f"no entry in story/field/buildings.md for: {', '.join(missing)}. Add a '## <id>' with a "
            f"one-line description and a '- map:' line.")
    for i in ids:
        if not entries[i]["desc"]:
            die(f"story/field/buildings.md: '## {i}' has no description line")
        for key in ("desc", "wall", "roof"):
            if entries[i].get(key):
                check_description(entries[i][key], f"{i} ({key})", "story/field/buildings.md")

    W, H, label, u, boxes = layout_cells([(1, 1)] * (len(ids) * 3), ids)
    slots, n = [], 0
    for i in ids:
        for face in BUILDING_FACES:
            n += 1
            slots.append({"n": n, "id": i, "face": face, "box": list(boxes[n - 1]),
                          "inner": list(inner_box(boxes[n - 1])), "target": list(FACE_PX[face]),
                          "out": str((FIELD_DIRS["building"] / f"{i}_{face}.png").relative_to(ROOT.parent))})
    name = package_name("buildings", ids)
    b = style["blocks"]
    attach = field_attachments(style, pkg_path(name, "template.png"), None, warn)
    L = [f"Create ONE image: the attached template with all {len(slots)} numbered slots filled in. "
         f"Canvas: {label}, the same size as the template.", b["header"], "",
         "ATTACHED REFERENCE IMAGES, in the order I attached them:"]
    L += [f"Image {n}: {note}" for n, (_, note) in enumerate(attach, 1)]
    L += ["", TEMPLATE_RULES, "",
          "WHAT THESE ARE: wall and roof textures for the buildings of a 2.5D field map. The buildings "
          "themselves are built by the game out of flat walls and roof slopes; these slots are what "
          "gets painted onto them. Each building has three slots in a row: its front wall, its wall "
          "material, its roof material. Every slot is filled edge to edge, right up to the white "
          "border, with no sky, no ground, no scenery, no frame and no empty corner.", "",
          "THE FRONT SLOT is the whole front wall of that one building, seen square on and flat, as if "
          "you stood in front of it and looked straight at the wall: no perspective, no corner of the "
          "building turning away, nothing in front of it. The door and the windows are painted into it "
          "where they belong on that wall, at the size they would really be, with the door standing on "
          "the very bottom edge of the slot. The game stretches this square across the real width of "
          "the wall, so draw the whole wall and nothing else: no roof above it, no ground below it, no "
          "sky, and no margin around it.", "",
          "THE SIDE AND ROOF SLOTS are material samples, not walls: a patch of the same stuff, filled "
          "edge to edge, with NO door, NO window, NO opening and no single large feature the eye can "
          "count across a building. Both are SEAMLESS: the right edge continues into the left and the "
          "bottom into the top, because the game repeats them. The side sample is seen square on and "
          "flat like the front; the roof sample is seen straight down from above.", "",
          f"In game the front and side are {FACE_PX['front'][0]}x{FACE_PX['front'][1]} pixels and the "
          f"roof is {FACE_PX['roof'][0]}x{FACE_PX['roof'][1]}, so keep the pixels large and the shapes "
          f"simple. All of these buildings stand in one valley town and share one palette.", "",
          "SLOTS:"]
    for s in slots:
        e = entries[s["id"]]
        px = f"{s['inner'][2]}x{s['inner'][3]} px in the template"
        if s["face"] == "front":
            L.append(f"Slot {s['n']} ({s['id']} front), {px}: the whole front wall of this building, "
                     f"square on and flat, door and windows included, filling the slot edge to edge. "
                     f"The building is: {e['desc'].rstrip('.')}.")
        elif s["face"] == "side":
            what = e.get("wall") or e["desc"]
            L.append(f"Slot {s['n']} ({s['id']} side), {px}: a seamless sample of this building's wall "
                     f"material, square on and flat, with no door and no window: {what.rstrip('.')}.")
        else:
            what = e.get("roof") or e["desc"]
            L.append(f"Slot {s['n']} ({s['id']} roof), {px}: a seamless sample of this building's roof "
                     f"seen straight down from above: {what.rstrip('.')}.")
    L += ["", f"RENDERING: {field_rendering(b)}", "",
          f"AVOID: {b['negative']}, drawing outside a slot, moving or covering a slot number, sky or "
          f"ground or scenery in a slot, a three-quarter or perspective view of a building, a roof "
          f"drawn over a front wall, a door or window in a side or roof slot, a visible seam in a side "
          f"or roof sample, a frame or margin inside a slot, changing the size of the image"]
    prompt = "\n".join(L)
    template, manifest, md = write_field_package(
        "building", name, label, W, H, BLACK, slots, prompt, attach, field_after(manifest_path(name), [
            f"- [ ] All {len(slots)} slots filled, every border and number still exactly where it was",
            "- [ ] Each front is one flat wall seen square on: no corner turning away, no roof, no ground",
            "- [ ] The door stands on the bottom edge of its front slot and is the right size for the wall",
            "- [ ] No door, window or opening anywhere in a side or roof slot",
            "- [ ] Side and roof samples tile: left edge would meet right, top would meet bottom",
            "- [ ] Every slot is filled to its border, with no frame, no margin and no empty corner", "",
            "If one slot fails, reply in the same chat: \"Redraw only slot 4 and keep every other slot "
            "and the whole template exactly as it is. <what was wrong>\"."],
            makes=f"the {len(slots)} face texture(s) listed above"),
        status=field_status("building", slots))
    report_field(kind_rows("building", slots, package_label(name)), warn, md, template,
                 f"{len(ids)} buildings, {len(slots)} faces, {label}")


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
        target = (" / ".join(f"{f} {FACE_PX[f][0]}" for f in BUILDING_FACES) if kind == "building"
                  else f"{s['target'][0]}x{s['target'][1]}")
        rows.append({"id": s["id"], "kind": kind, "target": target, "package": package})
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


def field_art_exists(kind, ident):
    """Is there a file on disk for this id? Buildings have three, and any one of them counts."""
    d = FIELD_DIRS.get(kind)
    if d is None:
        return False
    if kind == "building":
        return any((d / f"{ident}_{f}.png").exists() for f in BUILDING_FACES)
    return (d / f"{ident}.png").exists()


def write_field_manifest(new_rows=()):
    """Regenerate story/field/manifest.md: every id ever requested, plus every id the docs declare."""
    rows = read_manifest_rows()
    declared = []
    tiles = field_entries(FIELD_DOCS["tile"], "tile") if FIELD_DOCS["tile"].exists() else {}
    props = field_entries(FIELD_DOCS["prop"], "prop") if FIELD_DOCS["prop"].exists() else {}
    walkers = field_entries(FIELD_DOCS["walker"], "walker") if FIELD_DOCS["walker"].exists() else {}
    builds = field_entries(FIELD_DOCS["building"], "building") if FIELD_DOCS["building"].exists() else {}
    for i in tiles:
        declared.append({"id": i, "kind": "tile", "target": f"{TILE_PX}x{TILE_PX}", "package": "-"})
    for i, e in props.items():
        cw, chh = footprint_of(i, e, [])
        declared.append({"id": i, "kind": "prop", "target": f"{cw * CELL_PX}x{chh * CELL_PX}", "package": "-"})
    for i in walkers:
        declared.append({"id": i, "kind": "walker", "target": f"{WALK_OUT_W * 4}x{WALK_OUT_H * 4}", "package": "-"})
    for i in builds:
        declared.append({"id": i, "kind": "building", "package": "-",
                         "target": " / ".join(f"{f} {FACE_PX[f][0]}" for f in BUILDING_FACES)})
    for r in declared:                                   # a declared id keeps whatever package asked for it
        key = (r["kind"], r["id"])
        r["package"] = rows.get(key, {}).get("package", "-")
        rows[key] = r
    for r in new_rows:
        rows[(r["kind"], r["id"])] = dict(r)
    live = {(r["kind"], r["id"]) for r in declared} | {(r["kind"], r["id"]) for r in new_rows}
    for key in [k for k in rows if k not in live and not field_art_exists(*k)]:
        del rows[key]                                    # an id its doc has dropped, with nothing on disk
                                                         # (house_a was a prop before it became a building)

    L = ["# story/field/manifest.md — every field art id",
         "",
         "Generated by `./story_prompt.py tiles|props|walker|building|cut`. The engine does not read it;",
         "people and agents do. `package` is the last template sheet that asked for the id, `-` if none has",
         "yet. Sizes are the target the pipeline writes; the last column is what is actually on disk.",
         ""]
    for kind, title, note in (("tile", "Tiles", f"{TILE_PX}x{TILE_PX}, opaque, seamless on all four edges"),
                              ("prop", "Props", f"alpha, {CELL_PX} px to a map cell, standing on the bottom row"),
                              ("walker", "Walkers", f"alpha, 4 rows (S, W, E, N) x 4 columns of "
                                                    f"{WALK_OUT_W}x{WALK_OUT_H} frames"),
                              ("building", "Buildings", "opaque faces on map geometry: front stretched once "
                                                        "across the front wall, side and roof seamless")):
        group = sorted((r for (k, _), r in rows.items() if k == kind), key=lambda r: r["id"])
        L += [f"## {title} — {note}", "", "| id | kind | target | package | file |", "| --- | --- | --- | --- | --- |"]
        for r in group:
            if kind == "building":                       # three files to an id, so say which of them exist
                have = [x for x in BUILDING_FACES if (FIELD_DIRS[kind] / f"{r['id']}_{x}.png").exists()]
                state = ", ".join(have) if have else "missing"
            else:
                f = FIELD_DIRS[kind] / f"{r['id']}.png"
                size = png_size(f) if f.exists() else None
                state = f"yes, {size[0]}x{size[1]}" if size else ("yes" if f.exists() else "missing")
            L.append(f"| `{r['id']}` | {r['kind']} | {r['target']} | `{r['package']}` | {state} |")
        if not group:
            L.append("| — | | | | |")
        L.append("")
    FIELD.mkdir(parents=True, exist_ok=True)
    FIELD_MANIFEST.write_text("\n".join(L))


# ── field text: what the world says when you look at it ──

def load_bans():
    """[(phrase, whole word?, file)] for every quoted phrase on a 'BAN:' line in the v3 lists.

    Those lists are written to be grepped — each rule is one `BAN:` line and the greppable part of it
    is in quotes — so this reads them rather than copying them. A rule with nothing quoted is a
    structural one ("ALL-CAPS dialogue") and is left to a human."""
    out, seen = [], set()
    for path in (V3_STYLE, SMELLS):
        if not path.exists():
            continue
        for line in path.read_text().splitlines():
            m = re.match(r"^BAN:\s*(.+)$", line.strip())
            if not m:
                continue
            rest = re.split(r"\s+[\u2014-]\s+", m.group(1))[0]     # the advice after the dash is not the rule
            words = re.match(r"^the words?\s+(.*)$", rest, flags=re.I)
            quotes = re.findall(r'"([^"]{2,})"', words.group(1) if words else rest)
            bare = re.fullmatch(r'\s*(?:"[^"]+"[\s,/]*(?:and\s*)?)+\s*(?:\([^)]*\))?\s*', rest)
            for phrase in quotes:
                one = re.fullmatch(r"[\w']+", phrase) is not None
                if one and not (words or bare):
                    continue                             # a common word quoted inside a prose rule
                if phrase.lower() not in seen:
                    seen.add(phrase.lower())
                    out.append((phrase, one, path.relative_to(ROOT.parent)))
    return out


def banned_hits(text):
    """[(phrase, which list)] for every banned phrase in a piece of text."""
    hits = []
    for phrase, word, where in load_bans():
        pat = rf"\b{re.escape(phrase)}\b" if word else re.escape(phrase)
        if re.search(pat, text, flags=re.I):
            hits.append((phrase, where))
    return hits


def sentence_count(text):
    return len([s for s in re.split(r"[.!?]+(?:\s|$)", text.strip()) if s.strip()])


def load_field_text():
    """{id: {...}} from story/field/text.md: the line itself, and the speaker name if it carries one.

    `- name:` is the name the game prints over the box — an NPC's display name is story, the same as
    the line is — and takes tokens like everything else. `- what:` is the writer's own note and is
    never exported."""
    out = {}
    if not FIELD_TEXT.exists():
        return out
    for ident, body in h2_sections(FIELD_TEXT.read_text()).items():
        if "." not in ident:
            continue                                     # the file's own prose headings
        raw = next((squash(l) for l in body.splitlines()
                    if l.strip() and not re.match(r"^\s*- [\w ]+?:", l)), "")
        kv = kv_lines(body)
        unknown, unnamed = [], []
        out[ident] = {**kv, "raw": raw, "text": detok(raw, unknown, unnamed),
                      "name_raw": kv.get("name", ""), "name": detok(kv.get("name", ""), unknown, unnamed),
                      "unknown": unknown, "unnamed": unnamed}
    return out


def check_field_text():
    """(errors, warnings) for story/field/text.md: ids, length, tokens, and the banned lists."""
    err, warn = [], []
    if not FIELD_TEXT.exists():
        return err, [f"{FIELD_TEXT.relative_to(ROOT.parent)} not found; the field has no examine text"]
    maps = {p.stem for p in (FIELD / "maps").glob("*.map")}
    entries = load_field_text()
    if not entries:
        warn.append(f"{FIELD_TEXT.name}: no '## <map>.<id>' entries")
    for ident, e in sorted(entries.items()):
        where = f"{FIELD_TEXT.name}: {ident}"
        if not re.fullmatch(r"[a-z0-9_]+\.[a-z0-9_]+", ident):
            err.append(f"{where}: an id is '<map>.<thing>', lower case, digits and underscores only")
            continue
        mp = ident.split(".")[0]
        if maps and mp not in maps:
            warn.append(f"{where}: no story/field/maps/{mp}.map, so nothing can trigger this line")
        if not e["raw"]:
            err.append(f"{where}: no text under the heading")
            continue
        for tok in e["unknown"]:
            err.append(f"{where}: unknown name token {{{{{tok}}}}}: it has no row in "
                       f"{NAMES_FILE.relative_to(ROOT.parent)}")
        if e["name"]:                                    # a display name is a name, not a sentence
            if len(e["name"]) > FIELD_NAME_MAX:
                warn.append(f"{where}: '- name: {e['name']}' is {len(e['name'])} characters; the box "
                            f"draws about {FIELD_NAME_MAX}")
            m = DOUBLE_ARTICLE.search(e["name"])
            if m:
                err.append(f"{where}: '- name:' reads '{m.group(0)}' after name substitution")
        if TODO_RE.match(e["raw"]):
            warn.append(f"{where}: still a placeholder; the game will show it in brackets")
            continue                                     # the rest is for text somebody has written
        n = sentence_count(e["text"])
        if n > TEXT_MAX_SENTENCES:
            err.append(f"{where}: {n} sentences, max {TEXT_MAX_SENTENCES}")
        for phrase, src in banned_hits(e["text"]):
            err.append(f"{where}: '{phrase}' is on the banned list in {src}")
        m = DOUBLE_ARTICLE.search(e["text"])
        if m:
            err.append(f"{where}: '{m.group(0)}' after name substitution: a token's value never carries "
                       f"its own article")
    return err, warn


# ── painted views: a finished background over the engine's block-out ──
#
# The owner's direction is the Final Fantasy VIII arrangement: the game keeps the 3D block-out for
# collision, depth and camera, and what you see is a painting laid over it. So the capture is not a
# reference for the painter to interpret — it is the layout, and nothing in it may move.

def capture_path(mp, zone):
    return VIEWS / f"{mp}_{zone}.png"


def paint_path(mp, zone):
    return VIEWS / f"{mp}_{zone}_paint.png"


def captures():
    """[(map, zone, path)] for every block-out the engine has written, paintings excluded."""
    out = []
    for p in sorted(VIEWS.glob("*.png")) if VIEWS.is_dir() else []:
        if p.stem.endswith("_paint"):
            continue
        maps = sorted((FIELD / "maps").glob("*.map")) if (FIELD / "maps").is_dir() else []
        mp = next((m.stem for m in maps if p.stem.startswith(m.stem + "_")), p.stem.split("_")[0])
        out.append((mp, p.stem[len(mp) + 1:], p))
    return out


def map_meta(mp):
    """The map's '## meta' block: plain 'key: value' lines, not the '- key:' the story files use."""
    f = FIELD / "maps" / f"{mp}.map"
    if not f.exists():
        return {}
    body = h2_sections(detok(f.read_text())).get("meta", "")
    return {m.group(1).strip().lower(): m.group(2).strip()
            for m in re.finditer(r"^\s*([\w ]+?):\s*(.+)$", body, flags=re.M)}


def map_places(mp):
    """{'props': [id...], 'buildings': [id...]} actually placed on a map, most-used first."""
    f = FIELD / "maps" / f"{mp}.map"
    out = {"props": [], "buildings": []}
    if not f.exists():
        return out
    sec = None
    for line in f.read_text().splitlines():
        if line.startswith("## "):
            sec = line[3:].strip().lower()
            continue
        if sec not in ("props", "buildings") or not line.strip() or line.lstrip().startswith("#"):
            continue
        bits = line.split()
        ident = next((b for b in bits if re.fullmatch(r"[a-z][a-z0-9_]*", b)), None)
        if ident:
            out[sec].append(ident)
    for k in out:
        seen = {}
        for i in out[k]:
            seen[i] = seen.get(i, 0) + 1
        out[k] = sorted(seen, key=lambda i: (-seen[i], i))
    return out


def view_attachments(style, cap, mp, warn):
    """The capture first — it is the layout — then the style, then what the map puts in the scene."""
    out = [(cap, "THE BLOCK-OUT. This is the scene to paint, from the game's own camera. It is the "
                 "layout, not a suggestion: every object, edge, road, wall and building stays exactly "
                 "where it is and exactly the size it is.")]
    sp = existing(style["refs"].get("style"))
    if sp:
        out.append((sp, "STYLE reference. " + style["refs"].get("style_note", "")))
    else:
        warn.append(f"no style reference at {style['refs'].get('style')}; the prompt text carries the style alone")
    placed = map_places(mp)
    for kind, ids in (("buildings", placed["buildings"]), ("props", placed["props"])):
        for i in ids:
            if len(out) >= VIEW_ATTACH_MAX + 2:
                break
            f = (FIELD_DIRS["building"] / f"{i}_front.png") if kind == "buildings" else \
                (FIELD_DIRS["prop"] / f"{i}.png")
            if f.exists():
                out.append((f, f"The {i.replace('_', ' ')} as it is drawn in this game. The map puts it "
                               f"in this scene; keep its design, colours and proportions."))
    return out


def map_description(mp, warn):
    """What this place is, in the words the field docs already use for it."""
    meta, placed = map_meta(mp), map_places(mp)
    lines = []
    if meta.get("landmark"):
        lines.append(f"THE PLACE: {meta['landmark'].rstrip('.')}.")
    docs = {k: (field_entries(FIELD_DOCS[k], k) if FIELD_DOCS[k].exists() else {})
            for k in ("tile", "prop", "building")}
    on_map = lambda e: mp in [slug(m) for m in (e.get("map") or "").split(",")]
    ground = [f"{i} ({e['desc'].rstrip('.')})" for i, e in docs["tile"].items()
              if on_map(e) and e.get("kind") == "ground"]
    if ground:
        lines.append("THE GROUND is made of: " + "; ".join(ground) + ".")
    for i in placed["buildings"]:
        e = docs["building"].get(i)
        if e:
            lines.append(f"BUILDING {i.replace('_', ' ')}: {e['desc'].rstrip('.')}.")
    props = [f"{i.replace('_', ' ')} ({docs['prop'][i]['desc'].rstrip('.')})"
             for i in placed["props"] if i in docs["prop"]]
    if props:
        lines.append("THE THINGS STANDING IN IT: " + "; ".join(props) + ".")
    if not lines:
        warn.append(f"no description for '{mp}': no story/field/maps/{mp}.map with a 'landmark:' line, "
                    f"and nothing in tiles.md, props.md or buildings.md names the map")
    elif len(lines) > 1:
        lines.append("That list is what this town is made of, so a block in the picture can be read for "
                     "what it is. Only what is actually in the block-out belongs in the painting: do not "
                     "add a building, a tree or an object because it is named here.")
    return lines


def cmd_view(args):
    """One painted background per camera zone, over the capture the engine wrote."""
    pos = [a for a in args if not a.startswith("--")]
    if len(pos) != 2:
        die("usage: view <map> <zone>   (the engine writes story/field/views/<map>_<zone>.png)")
    mp, zone = slug(pos[0]), slug(pos[1])
    cap = capture_path(mp, zone)
    if not cap.exists():
        die(f"no block-out at {cap.relative_to(ROOT.parent)}. The engine captures it from the game — "
            f"stand in the zone, use the Dev panel's capture, and pull the file — and this command paints "
            f"over what it captured, so there is nothing to do until it exists.")
    size = png_size(cap)
    if not size:
        die(f"{cap.relative_to(ROOT.parent)} is not a PNG the tool can read")
    style, warn = load_style(), []
    if tuple(size) not in VIEW_SIZES:
        warn.append(f"{cap.name} is {size[0]}x{size[1]}, not one of "
                    f"{' or '.join(f'{w}x{h}' for w, h in VIEW_SIZES)}; the painting is fitted back to "
                    f"{size[0]}x{size[1]} whatever it is")
    attach = view_attachments(style, cap, mp, warn)
    b = style["blocks"]
    L = ["Paint this exact scene. The attached image is a rough 3D block-out from the game's camera: "
         "keep every object, edge, road, wall and building EXACTLY where it is and the same size — the "
         "game uses this layout for collision and depth, so nothing may move — and repaint it as a "
         "finished background.", "", b["header"], "",
         "ATTACHED REFERENCE IMAGES, in the order I attached them:"]
    L += [f"Image {n}: {note}" for n, (_, note) in enumerate(attach, 1)]
    L += [""] + map_description(mp, warn)
    L += ["",
          "WHAT TO CHANGE: only the surface. Flat block colours become real materials, bare boxes become "
          "buildings with their own walls and roofs, the ground gets its grass, dirt, paving and ruts, "
          "and the light and shadow of one time of day fall across all of it consistently. Paint in "
          "everything a finished picture of this place would have that the block-out is too crude to "
          "show: the grain of a wall, the wear along a road, a shutter, a hinge, a rope, a weed at the "
          "foot of a post.", "",
          "WHAT NOT TO CHANGE: the camera, the perspective, the frame, and the position, footprint and "
          "height of every single thing in it. Do not move a building a few pixels to compose the shot, "
          "do not widen a road, do not add or remove a structure, and do not crop or zoom. A player "
          "walks this layout: anything that moves in the painting is a wall they will walk through and a "
          "gap they will bump into.", "",
          "NO PEOPLE, no characters, no animals: the game draws those on top as sprites, and a painted "
          "one would stand still forever. No text, no signs with writing, no labels, no numbers, no "
          "watermark, no user interface, no frame, no border, no vignette and no letterboxing.", "",
          f"THE WHOLE FRAME is painted, edge to edge, 16:9, at the same size as the block-out "
          f"({size[0]}x{size[1]}) if you can; a little smaller is fine and the tool fits it back.", "",
          f"CAMERA AND FRAMING: {b['framing']}", "",
          f"RENDERING: {b['rendering']}", "",
          f"AVOID: {b['negative']}, moving anything from where the block-out puts it, changing the "
          f"camera or the crop, people or animals, text or signs with writing, a user interface, a "
          f"border or vignette, empty unpainted areas"]
    prompt = "\n".join(L)
    name = f"view_{mp}_{zone}"
    out_file = str(paint_path(mp, zone).relative_to(ROOT.parent))
    OUT.mkdir(exist_ok=True)
    manifest = pkg_path(name, "sheet.json")
    manifest.write_text(json.dumps({
        "view": name, "kind": "view", "map": mp, "zone": zone,
        "capture": str(cap.relative_to(ROOT.parent)), "size": list(size), "out": out_file,
        "prompt": prompt,
    }, indent=2) + "\n")
    md = pkg_path(name, "chatgpt.md")
    md.write_text(package(f"painted view {mp} / {zone}", attach, prompt, [
        "Check the result against the block-out before accepting it — hold them side by side.", "",
        "- [ ] Every building, wall, road edge and object is in the same place and the same size",
        "- [ ] The camera, the crop and the horizon are unchanged",
        "- [ ] No people, no animals, no text, no interface, no border",
        "- [ ] Painted edge to edge, nothing left flat or unfinished",
        "- [ ] It looks like the same world as the panels and the tiles", "",
        "If one thing has drifted, reply in the same chat: \"The well has moved left and grown. Put it "
        "back exactly where the block-out has it, at the same size, and keep everything else.\"", "",
        *(return_lines(f"`{out_file}`, fitted back to {size[0]}x{size[1]}") or [
            "When it passes, download it and fit it to the capture:", "", "```",
            f"./story_prompt.py cut {manifest.relative_to(ROOT.parent)} ~/Downloads/<file>.png", "```"])],
        status=[f"- block-out `{cap.relative_to(ROOT.parent)}` — **exists**, {size[0]}x{size[1]}",
                f"- painting `{out_file}` — " + ("**exists**" if (ROOT.parent / out_file).exists()
                                                 else "missing")]))
    for w in warn:
        print(f"warning: {w}", file=sys.stderr)
    print(f"wrote {md.relative_to(ROOT.parent)}  (block-out {size[0]}x{size[1]}, {len(attach)} attachment(s))")


def resize_box(rows, w, h, ch, nw, nh):
    """Area-average downscale. See cut_view for why this rather than nearest."""
    out = []
    for y in range(nh):
        y0, y1 = y * h // nh, max(y * h // nh + 1, (y + 1) * h // nh)
        dst = bytearray(nw * ch)
        for x in range(nw):
            x0, x1 = x * w // nw, max(x * w // nw + 1, (x + 1) * w // nw)
            n = (x1 - x0) * (y1 - y0)
            for c in range(ch):
                s = 0
                for yy in range(y0, y1):
                    row = rows[yy]
                    for xx in range(x0, x1):
                        s += row[xx * ch + c]
                dst[x * ch + c] = s // n
        out.append(dst)
    return out


def resize_bilinear(rows, w, h, ch, nw, nh):
    """Bilinear enlargement. Used where the return came back a little smaller than the target."""
    out = []
    for y in range(nh):
        fy = (y + 0.5) * h / nh - 0.5
        y0 = max(0, min(h - 1, int(fy // 1)))
        y1 = min(h - 1, y0 + 1)
        ty = max(0.0, fy - y0)
        r0, r1 = rows[y0], rows[y1]
        dst = bytearray(nw * ch)
        for x in range(nw):
            fx = (x + 0.5) * w / nw - 0.5
            x0 = max(0, min(w - 1, int(fx // 1)))
            x1 = min(w - 1, x0 + 1)
            tx = max(0.0, fx - x0)
            for c in range(ch):
                a = r0[x0 * ch + c] * (1 - tx) + r0[x1 * ch + c] * tx
                b = r1[x0 * ch + c] * (1 - tx) + r1[x1 * ch + c] * tx
                dst[x * ch + c] = int(a * (1 - ty) + b * ty + 0.5)
        out.append(dst)
    return out


def resample(rows, w, h, ch, nw, nh):
    """To an exact size: area-average going down, bilinear going up. Never nearest.

    A returned sheet is never an exact scale of the template, so every slot's art area has to be
    brought onto the grid. Nearest at a non-integer ratio drops whole columns and tears the straight
    edges; this keeps them."""
    if (w, h) == (nw, nh):
        return [bytearray(r) for r in rows]
    if nw <= w and nh <= h:
        return resize_box(rows, w, h, ch, nw, nh)
    return resize_bilinear(rows, w, h, ch, nw, nh)


def extrude_edges(px, w, h, n=EXTRUDE_PX):
    """Bleed the colour of opaque pixels outward under the transparent ones. Alpha is not changed.

    The engine filters the atlas linearly, so a texel just outside a sprite is still sampled at the
    silhouette; if it is black or leftover magenta, that is what shows up as a dark or pink halo.
    Giving it the colour of the art it borders makes the interpolation invisible."""
    have = [[bool(px[y][x * 4 + 3]) for x in range(w)] for y in range(h)]
    for _ in range(n):
        add = []
        for y in range(h):
            for x in range(w):
                if have[y][x]:
                    continue
                acc, k = [0, 0, 0], 0
                for nx, ny in ((x + 1, y), (x - 1, y), (x, y + 1), (x, y - 1)):
                    if 0 <= nx < w and 0 <= ny < h and have[ny][nx]:
                        for c in range(3):
                            acc[c] += px[ny][nx * 4 + c]
                        k += 1
                if k:
                    add.append((x, y, bytes(v // k for v in acc)))
        for x, y, c in add:
            px[y][x * 4:x * 4 + 3] = c
            have[y][x] = True
    return px


def cut_view(data, image, nearest=False):
    """The returned painting, fitted to the block-out's exact size. (printed, written)

    Box filter by default, not nearest. The painting comes back at about 1.5 MP whatever was asked
    for, so this is a small non-integer reduction — 1620x912 to 1280x720 is 1.27x — and nearest at a
    non-integer ratio throws away every fourth column and row. On a straight road edge or a wall that
    has to line up with the block-out underneath, that shows as a torn staircase; averaging keeps the
    line where the painter put it. `--nearest` is there for a painting that came back at an exact
    integer multiple, where nearest is the crisper answer."""
    cap = ROOT.parent / data["capture"]
    tw, th = data["size"]
    w, h, ch, rows = read_png(image)
    if abs((w / h) / (tw / th) - 1) > 0.06:
        die(f"{image.name} is {w}x{h}, the block-out is {tw}x{th}: a different shape, so the painting "
            f"would not line up with the layout. Ask for it again at 16:9. Nothing written.")
    px = (resize_nn(rows, w, h, ch, tw, th) if nearest else resize_box(rows, w, h, ch, tw, th))
    if ch == 4:                                          # a background is opaque
        px = [bytearray(b for i, b in enumerate(r) if i % 4 != 3) for r in px]
    out = ROOT.parent / data["out"]
    out.parent.mkdir(parents=True, exist_ok=True)
    write_png(out, tw, th, 3, px)
    print(f"{image.name}: {w}x{h} -> {out.relative_to(ROOT.parent)}  {tw}x{th} "
          f"({'nearest' if nearest else 'box filter'}, block-out {cap.name})")


# ── cutting a returned template ──

def magenta_cast(r, g, b):
    """How much magenta is mixed into a pixel, 0-255. See the note on KEY_HARD."""
    return min(r, b) - g


def key_magenta(rows, w, h, ch, fringe=0):
    """RGBA rows with the magenta keyed out: flat magenta transparent, the anti-aliased edge cleaned.

    Three passes, because the engine alpha-tests at 0.5 and draws whatever survives at full strength,
    so a surviving pixel has to carry a clean colour — a half-magenta pixel kept at alpha 1 is the
    purple outline the phone showed.

    1. Flat magenta goes transparent, and every pixel within KEY_EDGE_REACH of it is marked as edge.
       Only edge pixels are ever touched again: an object's interior may be any colour it likes.
    2. Each edge pixel's alpha comes from its magenta cast (255 - cast), and the magenta is taken back
       out of its colour at that alpha. Under KEY_MIN_ALPHA it is background.
    3. Any edge pixel still carrying a cast after that — the un-matte only corrects what the estimate
       got right — takes the colour of the nearest clean opaque pixel and keeps its own alpha. With no
       clean pixel within reach (a one-pixel-wide rope) the cast is subtracted off instead.

    `fringe` then erodes the alpha by that many pixels, for a sheet that stays dirty anyway."""
    px = [bytearray(w * 4) for _ in range(h)]
    for y in range(h):
        src, dst = rows[y], px[y]
        for x in range(w):
            r, g, b = src[x * ch], src[x * ch + 1], src[x * ch + 2]
            if ((r - 255) ** 2 + g * g + (b - 255) ** 2) ** 0.5 <= KEY_HARD:
                continue                                        # flat background: leave it at 0,0,0,0
            dst[x * 4:x * 4 + 4] = bytes((r, g, b, 255))

    # The edge is grown from the background rather than measured off it: flood outward through every
    # pixel that still carries a magenta cast, however deep the ramp goes, then take KEY_EDGE_REACH
    # more rings for the colour clamp. A fixed band missed the middle of a soft edge and left the
    # worst pixels — the ones nearest pure magenta — sitting at full alpha. An object that is itself
    # magenta and touches the background would be eaten by this, which is the bargain the contract
    # already makes: on these sheets the magenta is empty space, never paint.
    edge = [[False] * w for _ in range(h)]
    front = [(x, y) for y in range(h) for x in range(w) if not px[y][x * 4 + 3]]
    while front:
        nxt = []
        for x, y in front:
            for nx, ny in ((x + 1, y), (x - 1, y), (x, y + 1), (x, y - 1)):
                if 0 <= nx < w and 0 <= ny < h and px[ny][nx * 4 + 3] and not edge[ny][nx] \
                        and magenta_cast(px[ny][nx * 4], px[ny][nx * 4 + 1], px[ny][nx * 4 + 2]) >= KEY_CAST_MIN:
                    edge[ny][nx] = True
                    nxt.append((nx, ny))
        front = nxt
    front = [(x, y) for y in range(h) for x in range(w) if edge[y][x] or not px[y][x * 4 + 3]]
    for _ in range(KEY_EDGE_REACH):
        nxt = []
        for x, y in front:
            for nx, ny in ((x + 1, y), (x - 1, y), (x, y + 1), (x, y - 1)):
                if 0 <= nx < w and 0 <= ny < h and px[ny][nx * 4 + 3] and not edge[ny][nx]:
                    edge[ny][nx] = True
                    nxt.append((nx, ny))
        front = nxt

    for y in range(h):                                          # 2. alpha from the cast, then un-matte
        row = px[y]
        for x in range(w):
            if not edge[y][x]:
                continue
            r, g, b = row[x * 4], row[x * 4 + 1], row[x * 4 + 2]
            cast = magenta_cast(r, g, b)
            if cast < KEY_CAST_MIN:
                continue
            a = max(0, 255 - cast)
            if a < KEY_MIN_ALPHA:
                row[x * 4:x * 4 + 4] = b"\0\0\0\0"
                continue
            f = a / 255.0                                       # observed = f*colour + (1-f)*magenta
            row[x * 4:x * 4 + 4] = bytes((min(255, max(0, int((r - 255 * (1 - f)) / f))),
                                          min(255, max(0, int(g / f))),
                                          min(255, max(0, int((b - 255 * (1 - f)) / f))), a))

    clean = [[px[y][x * 4 + 3] == 255 and not edge[y][x] for x in range(w)] for y in range(h)]
    for y in range(h):                                          # 3. whatever is still purple
        row = px[y]
        for x in range(w):
            if not edge[y][x] or not row[x * 4 + 3]:
                continue
            r, g, b, a = row[x * 4:x * 4 + 4]
            if magenta_cast(r, g, b) <= KEY_CAST_CLEAN:
                continue
            got = nearest_clean(px, clean, w, h, x, y)
            row[x * 4:x * 4 + 4] = bytes((*got, a)) if got else \
                bytes((max(0, r - magenta_cast(r, g, b)), g, max(0, b - magenta_cast(r, g, b)), a))

    return erode_alpha(px, w, h, fringe) if fringe else px


def nearest_clean(px, clean, w, h, x, y):
    """The colour of the nearest fully opaque pixel that the background never touched, or None."""
    for rad in range(1, KEY_CLEAN_RADIUS + 1):
        best, bd = None, None
        for dy in range(-rad, rad + 1):
            for dx in range(-rad, rad + 1):
                if max(abs(dx), abs(dy)) != rad:
                    continue
                nx, ny = x + dx, y + dy
                if 0 <= nx < w and 0 <= ny < h and clean[ny][nx]:
                    d = dx * dx + dy * dy
                    if bd is None or d < bd:
                        best, bd = px[ny][nx * 4:nx * 4 + 3], d
        if best:
            return tuple(best)
    return None


def erode_alpha(px, w, h, n):
    """Shave n pixels off the alpha, for a sheet whose edge stays dirty however it is keyed."""
    for _ in range(n):
        gone = [(x, y) for y in range(h) for x in range(w) if px[y][x * 4 + 3]
                and any(not (0 <= x + dx < w and 0 <= y + dy < h) or not px[y + dy][(x + dx) * 4 + 3]
                        for dx, dy in ((1, 0), (-1, 0), (0, 1), (0, -1)))]
        for x, y in gone:
            px[y][x * 4:x * 4 + 4] = b"\0\0\0\0"
    return px


LOUD = []           # warnings that have to survive run_quiet's swallowed output; `ingest` prints them


def check_alpha(rows, w, h, out):
    """A prop or a walker must come out with real transparency, or the game draws a black rectangle.

    `write_png` writes colour type 6 whenever it is handed 4 channels, and `key_magenta` always hands
    it 4, so every prop and walker file is RGBA by construction — that part cannot go wrong. What a
    real cut can still get wrong is the *content*: a generator that painted over every last pixel of
    magenta leaves a sprite with nothing keyed out, and the engine draws it as a solid rectangle. Say
    so here, loudly, instead of leaving it to be found on the phone. A sprite that is honestly a
    rectangle trips this too, and should: it will look like one in game."""
    clear = sum(1 for r in rows for i in range(3, len(r), 4) if r[i] < 128)
    if clear == 0:
        LOUD.append(f"{out.name} came out with no transparent pixel anywhere, so the game will draw it "
                    f"as a solid rectangle. Either the magenta was painted over, or the object really "
                    f"does fill its slot. Ask for that slot again with the background left untouched "
                    f"right up to the edge of the object.")
        print(f"WARNING: {LOUD[-1]}", file=sys.stderr)
        return "   <-- NO TRANSPARENCY, see the warning"
    return f"  ({100 * clear // (w * h)}% transparent)"


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


# ── slot localisation: finding the border ChatGPT actually drew ──
#
# The returned sheet is never an exact scale of the template — it comes back at whatever size the app
# felt like (about 1.5 megapixels), and the model redraws each slot's white border a few pixels off
# where the scaled box says it is. Cutting on the scaled box therefore leaves a sliver of the white
# border inside the art: thin white lines along tile edges in the atlas, and a seam metric that reads
# the white row rather than the art. So every side of every slot is searched for the border line it
# actually has, and the art area starts just inside it.

NEAR_WHITE = 235                      # r,g,b all above this is "the template's white border"
BORDER_RUN = 0.55                     # ...over this much of the side's length
BORDER_RUN_LOW = 0.3                  # ...or this much, on a second pass, where the art overlaps it
BORDER_REACH = 0.12                   # how far either side of the scaled edge the border is hunted
BORDER_SKIRT = 200                    # the border's anti-aliased edge: bright, but not white
SKIRT_FRAC, SKIRT_MAX = 0.3, 3        # how much of a line is skirt, and how far in to chase it


def _bright_frac(rows, w, h, ch, pos, lo, hi, vertical, level=NEAR_WHITE):
    """How much of a column (vertical) or row between lo and hi is brighter than `level`."""
    n = hi - lo
    if n <= 0 or pos < 0 or pos >= (w if vertical else h):
        return 0.0
    if vertical:
        it = (rows[y][pos * ch:pos * ch + 3] for y in range(lo, hi))
    else:
        r = rows[pos]
        it = (r[x * ch:x * ch + 3] for x in range(lo, hi))
    return sum(1 for p in it if p[0] > level and p[1] > level and p[2] > level) / n


def _white_frac(rows, w, h, ch, pos, lo, hi, vertical):
    return _bright_frac(rows, w, h, ch, pos, lo, hi, vertical)


def localise_slot(rows, w, h, ch, box):
    """Refine a scaled slot box (x0, y0, x1, y1 exclusive) onto the border the sheet really has.

    Each side is searched +-3% of the slot's size (at least 6 px) for the white border line, and the
    art is taken to start just inside it. For a left or top edge the INNERMOST candidate wins, for a
    right or bottom edge likewise, so a neighbouring slot's border across the gutter can never be
    mistaken for this one's. A side with no border found falls back to the scaled edge inset by 2 px.
    Returns (box, sides not found)."""
    x0, y0, x1, y1 = box
    rx, ry = max(24, int(round(BORDER_REACH * (x1 - x0)))), max(24, int(round(BORDER_REACH * (y1 - y0))))
    ylo, yhi = y0 + (y1 - y0) // 10, y1 - (y1 - y0) // 10
    xlo, xhi = x0 + (x1 - x0) // 10, x1 - (x1 - x0) // 10
    miss = []

    def side(name, cands, vertical, lo, hi, inner, default):
        # The innermost candidate always wins, so searching generously outward can never pick up a
        # neighbour's border across the gutter, and searching generously inward is what finds the one
        # ChatGPT redrew forty pixels off. A second, slacker pass catches a border the art overlaps.
        for level in (BORDER_RUN, BORDER_RUN_LOW):
            found = [p for p in cands
                     if _white_frac(rows, w, h, ch, p, lo, hi, vertical) >= level]
            if found:
                return inner(found)
        miss.append(name)
        return default

    def skirt(pos, step, vertical, lo, hi):
        """Step further in past the border's anti-aliased edge: bright, but never white.

        Without this the cut keeps a half-lit row of the border, which the mode filter promotes to a
        full art pixel on a sheet that came back at about 1:1 — the dashed white outlines that were
        left around the trees and the barn when only the white line itself was skipped."""
        for _ in range(SKIRT_MAX):
            if _bright_frac(rows, w, h, ch, pos, lo, hi, vertical, BORDER_SKIRT) < SKIRT_FRAC:
                break
            pos += step
        return pos

    nx0 = side("left", range(x0 - rx, x0 + rx + 1), True, ylo, yhi, lambda f: max(f) + 1, x0 + 2)
    nx1 = side("right", range(x1 - rx, x1 + rx + 1), True, ylo, yhi, min, x1 - 2)
    ny0 = side("top", range(y0 - ry, y0 + ry + 1), False, xlo, xhi, lambda f: max(f) + 1, y0 + 2)
    ny1 = side("bottom", range(y1 - ry, y1 + ry + 1), False, xlo, xhi, min, y1 - 2)
    nx0 = skirt(nx0, 1, True, ylo, yhi)
    nx1 = skirt(nx1 - 1, -1, True, ylo, yhi) + 1
    ny0 = skirt(ny0, 1, False, xlo, xhi)
    ny1 = skirt(ny1 - 1, -1, False, xlo, xhi) + 1
    if nx1 - nx0 < 8 or ny1 - ny0 < 8:                   # nonsense: keep the scaled box, inset
        return (x0 + 2, y0 + 2, x1 - 2, y1 - 2), ["left", "right", "top", "bottom"]
    return (max(0, nx0), max(0, ny0), min(w, nx1), min(h, ny1)), miss


def localise_slots(rows, w, h, ch, slots, sx, sy, label=""):
    """Refine every slot of a template package in place, as `_art`. One warning per sheet."""
    missed = 0
    for s in slots:
        ix, iy, iw, ih = s["inner"]
        box = (int(round(ix * sx)), int(round(iy * sy)),
               int(round((ix + iw) * sx)), int(round((iy + ih) * sy)))
        if box[0] < 0 or box[1] < 0 or box[2] > w or box[3] > h:
            die(f"slot {s['n']} falls outside the image; is this the right file for this package?")
        s["_art"], miss = localise_slot(rows, w, h, ch, box)
        missed += len(miss)
    if missed:
        print(f"  note: {missed} slot side(s) had no white border where one was expected; "
              f"the scaled edge inset by 2 px was used there{label}", file=sys.stderr)
    return missed


def scrub_border(px, w, h, ch, keyed, ring=1):
    """Wipe what is left of the template's white border from a cut image's outermost pixels.

    Two things are residue, and nothing else is:

    * a near-white STRAIGHT RUN along an edge, 60% of that edge's length or more — a border line the
      cut kept whole; and
    * a near-white edge pixel with nothing bright behind it — the DASHED remains of a border line on
      a sheet that came back at about 1:1, where the mode filter promoted the border in some blocks
      and not others. A border line is one pixel thin by construction; a headband, a flower, a
      plaster wall or the white water foam is a body of white with more white just inside it, so the
      support test leaves all of them alone.

    Nothing in the interior is ever touched either way."""
    def white(x, y, level=NEAR_WHITE):
        r = px[y]
        return r[x * ch] > level and r[x * ch + 1] > level and r[x * ch + 2] > level

    def bright(x, y):
        r = px[y]
        return (r[x * ch] > BORDER_SKIRT and r[x * ch + 1] > BORDER_SKIRT
                and r[x * ch + 2] > BORDER_SKIRT and (ch < 4 or r[x * ch + 3]))

    def longest(flags):
        best = run = 0
        for f in flags:
            run = run + 1 if f else 0
            best = max(best, run)
        return best

    n = 0
    for r in range(min(ring, (min(w, h) - 1) // 2), -1, -1):
        for edge in ("top", "bottom", "left", "right"):
            if edge in ("top", "bottom"):
                y = r if edge == "top" else h - 1 - r
                sy = y + 1 if edge == "top" else y - 1
                cells = [(x, y, x, sy) for x in range(w)]
            else:
                x = r if edge == "left" else w - 1 - r
                sx_ = x + 1 if edge == "left" else x - 1
                cells = [(x, y, sx_, y) for y in range(h)]
            # A whole white line has to be white; a dash of one only has to be pale, because a border
            # that the keying caught half of comes back as a half-transparent pale pixel, and that is
            # still a line on the phone. Either way it only goes if the art behind it is not pale too.
            flags = [white(cx, cy) for cx, cy, _, _ in cells]
            whole = longest(flags) >= BORDER_RUN * len(cells)
            pale = [white(cx, cy, BORDER_SKIRT) for cx, cy, _, _ in cells]
            for (cx, cy, ax, ay), f, pl in zip(cells, flags, pale):
                if not ((f and whole) or (pl and not bright(ax, ay))):
                    continue
                if keyed:
                    px[cy][cx * ch:cx * ch + ch] = b"\0" * ch
                else:
                    px[cy][cx * ch:(cx + 1) * ch] = px[ay][ax * ch:(ax + 1) * ch]
                n += 1
    return n


def scaled_inner(slot, sx, sy, w, h):
    """A slot's inner box in the returned image, with a couple of pixels shaved off for soft borders."""
    if slot.get("_art"):
        x0, y0, x1, y1 = slot["_art"]
        return x0, y0, x1 - x0, y1 - y0
    ix, iy, iw, ih = slot["inner"]
    x0 = int(round(ix * sx)) + CUT_PAD
    y0 = int(round(iy * sy)) + CUT_PAD
    x1 = int(round((ix + iw) * sx)) - CUT_PAD
    y1 = int(round((iy + ih) * sy)) - CUT_PAD
    if x0 < 0 or y0 < 0 or x1 > w or y1 > h or x1 - x0 < 8 or y1 - y0 < 8:
        die(f"slot {slot['n']} falls outside the image; is this the right file for this package?")
    return x0, y0, x1 - x0, y1 - y0


def body_bounds(px, w, h, alpha=200, least=3):
    """The silhouette's box, ignoring dust: a row or column counts only once `least` pixels in it are
    solid. With a soft-keyed frame a single half-transparent speck in a corner would otherwise make
    every frame's box the whole frame, and the anchoring would have nothing to work from."""
    xs = [x for x in range(w) if sum(1 for y in range(h) if px[y][x * 4 + 3] >= alpha) >= least]
    ys = [y for y in range(h) if sum(1 for x in range(w) if px[y][x * 4 + 3] >= alpha) >= least]
    if not xs or not ys:
        return None
    return xs[0], ys[0], xs[-1] - xs[0] + 1, ys[-1] - ys[0] + 1


def shift_frame(px, w, h, dx, dy):
    """Move a frame's art by (dx, dy) inside its own box; what falls off the box is gone."""
    out = [bytearray(w * 4) for _ in range(h)]
    for y in range(h):
        sy = y - dy
        if not (0 <= sy < h):
            continue
        row, dst = px[sy], out[y]
        if dx >= 0:
            dst[dx * 4:w * 4] = row[0:(w - dx) * 4]
        else:
            dst[0:(w + dx) * 4] = row[-dx * 4:w * 4]
    return out


def cut_walker(data, slots, rows, w, h, ch, sx, sy, fringe=0):
    """A returned walk sheet: one localised frame per slot, keyed, ANCHORED, and one sheet a character.

    Every frame is placed rather than merely cut: ChatGPT draws each pose where it likes inside its
    slot, so without this the character slid sideways and bobbed as the cycle played. Each frame is
    centred horizontally on its own silhouette, and each ROW takes one vertical offset measured from
    its stand frame, so a step may lift a foot without the whole body hopping."""
    FIELD_DIRS["walker"].mkdir(parents=True, exist_ok=True)
    OW, OH = WALK_OUT_W, WALK_OUT_H
    ncol = max(s["col"] for s in slots) + 1               # 3 now; 4 on a sheet drawn before D20
    if data.get("who"):
        who = {c["id"]: c["rows"] for c in data["who"]}
    else:                                                 # one character, rows named by their facing
        ident = slots[0].get("who") or Path(slots[0]["out"]).stem
        rows_named, seen = [], set()
        for s in sorted(slots, key=lambda s: s["row"]):
            if s["row"] not in seen:
                seen.add(s["row"])
                rows_named.append(s["facing"])
        who = {ident: rows_named}
    frames, boxes = {}, {}
    for s in slots:
        x, y, bw, bh = scaled_inner(s, sx, sy, w, h)
        art = crop(rows, ch, x, y, bw, bh)
        if (bw, bh) != (OW, OH):
            art = resample(art, bw, bh, ch, OW, OH)      # area-average down: the size study's rule
        px = key_magenta(art, OW, OH, ch, fringe)
        scrub_border(px, OW, OH, 4, True, EXTRUDE_PX * 2)
        ident = s.get("who") or Path(s["out"]).stem
        frames[(ident, s["row"], s["col"])] = px
        boxes[(ident, s["row"], s["col"])] = body_bounds(px, OW, OH)

    if ncol == 4:
        # A sheet drawn before D20: four columns (stand, step-left, stand, step-right) and four rows
        # (S W E N). Columns 1 and 3 were asked for as the same pose and the E row is the W row
        # mirrored, so the shipping layout is taken straight out of it — cols 0, 1, 3 and rows S, W,
        # N — rather than writing a 16-frame sheet nothing reads any more. `walker compact` reports
        # how true the mirror actually is; this only picks the frames.
        pick_c, pick_r = (0, 1, 3), (0, 1, 3)
        new_frames, new_boxes, new_who = {}, {}, {}
        for ident, rownames in who.items():
            keep = [r for r in pick_r if r < len(rownames)]
            new_who[ident] = [("side" if rownames[r] == "W" else rownames[r]) for r in keep]
            for nr, r in enumerate(keep):
                for nc, c in enumerate(pick_c):
                    if (ident, r, c) in frames:
                        new_frames[(ident, nr, nc)] = frames[(ident, r, c)]
                        new_boxes[(ident, nr, nc)] = boxes[(ident, r, c)]
        frames, boxes, who, ncol = new_frames, new_boxes, new_who, 3
        print(f"  a pre-D20 4x4 sheet: taking columns 1, 2, 4 and rows S, W, N as the 9-frame layout")

    pal, pidx = master_palette(), PalIndex(master_palette())
    report = []
    for ident, rownames in who.items():
        for r in range(len(rownames)):
            stand = boxes.get((ident, r, 0))
            dy = (OH - WALK_FOOT) - (stand[1] + stand[3]) if stand else 0
            for c in range(ncol):
                b = boxes.get((ident, r, c))
                if not b:
                    report.append(f"    {ident} {rownames[r]} frame {c} is empty")
                    continue
                dx = (OW - b[2]) // 2 - b[0]
                frames[(ident, r, c)] = shift_frame(frames[(ident, r, c)], OW, OH, dx, dy)
                nb = body_bounds(frames[(ident, r, c)], OW, OH)
                step = WALK_STEPS[c] if c < len(WALK_STEPS) else WALK_OLD_STEPS[c]
                report.append(f"    {ident} {rownames[r]} {step}: bbox {b[2]}x{b[3]} at "
                              f"{b[0]},{b[1]} -> {nb[0]},{nb[1]} (dx {dx:+d}, dy {dy:+d})")
        SW, SH = OW * ncol, OH * len(rownames)
        sheet = [bytearray(SW * 4) for _ in range(SH)]
        for r in range(len(rownames)):
            for c in range(ncol):
                px = frames.get((ident, r, c))
                if not px:
                    continue
                for k in range(OH):
                    sheet[r * OH + k][c * OW * 4:(c + 1) * OW * 4] = px[k]
        out = FIELD_DIRS["walker"] / f"{ident}.png"
        ship_png(out, SW, SH, 4, sheet, pal, pidx)
        meta = write_walker_meta(ident, rownames, WALK_STEPS[:ncol])
        print(f"  {len(rownames) * ncol} frames of {OW}x{OH} -> {out.relative_to(ROOT.parent)}  "
              f"{SW}x{SH} (rows {', '.join(rownames)}; {meta.name})"
              f"{check_alpha(sheet, SW, SH, out)}")
    for l in report:
        print(l)


def cmd_walker_compact(args):
    """`walker compact <id>...` — an old 16-frame sheet becomes the 9-frame one, at 128x192.

    Three things happen and each is checked. The four duplicate stand frames go (columns 1 and 3 of
    the old sheet were asked for as identical). The E row goes, because the engine mirrors the W row
    — and this REPORTS the Oklab dE between E and mirror(W) before it does, so a design that is not
    actually symmetric is caught rather than quietly flipped. And every frame is area-averaged down
    to 128x192, which the size study measured as the phone's own display size."""
    ids = [walker_ident(a) for a in args if not a.startswith("--")]
    force = "--force" in args
    if not ids:
        ids = sorted(p.stem for p in FIELD_DIRS["walker"].glob("*.png"))
        if not ids:
            die("usage: walker compact <id> [<id>...]   (converts story/field/walkers/<id>.png)")
    pal, pidx = master_palette(), PalIndex(master_palette())
    rows_out = []
    for ident in ids:
        src = FIELD_DIRS["walker"] / f"{ident}.png"
        if not src.exists():
            die(f"{src.relative_to(ROOT.parent)} not found")
        w, h, ch, px = read_png(src)
        if w == len(WALK_STEPS) * WALK_OUT_W and not force:      # already the 9- (or 12-) frame sheet
            meta = walker_meta_path(ident)
            rows_named = WALK_ROWS if h == len(WALK_ROWS) * WALK_OUT_H else WALK_ROWS_ASYM
            print(f"skip  {ident}: already {w}x{h}, rows {', '.join(rows_named)}"
                  + ("" if meta.exists() else "  (wrote its .json)"))
            if not meta.exists():
                write_walker_meta(ident, rows_named)
            continue
        if w % 4 or h % 4:
            die(f"{src.name} is {w}x{h}, which is not the 4x4 grid this converts from")
        fw, fh = w // 4, h // 4
        SHEETS.mkdir(exist_ok=True)
        keep = SHEETS / f"walker16-{ident}.png"           # the 16-frame sheet is archived, not lost
        if not keep.exists():
            keep.write_bytes(src.read_bytes())
        grab = lambda r, c: crop(px, ch, c * fw, r * fh, fw, fh)
        # How far is the E row from a mirror of the W row? Measured both ways round the gait, because
        # a mirrored stride is the OTHER column: a generator that drew the east row independently
        # usually picked the opposite phase, and that is not a reason to call the design asymmetric.
        def mirror_score(pairs):
            de_sum, de_n, shape = 0.0, 0, 0
            for ca, cb in pairs:
                a, b = grab(1, ca), grab(2, cb)
                for y in range(0, fh, 3):
                    for x in range(0, fw, 3):
                        i, j = x * ch, (fw - 1 - x) * ch
                        oa = ch != 4 or a[y][i + 3] >= 128
                        ob = ch != 4 or b[y][j + 3] >= 128
                        if not oa or not ob:
                            shape += 1 if oa != ob else 0
                            continue
                        p1 = _to_oklab(a[y][i], a[y][i + 1], a[y][i + 2])
                        p2 = _to_oklab(b[y][j], b[y][j + 1], b[y][j + 2])
                        de_sum += ((p1[0] - p2[0]) ** 2 + (p1[1] - p2[1]) ** 2
                                   + (p1[2] - p2[2]) ** 2) ** 0.5
                        de_n += 1
            n = de_n + shape
            return de_sum / max(1, de_n), 100.0 * shape / max(1, n)
        same = mirror_score(((0, 0), (1, 1), (3, 3)))
        swap = mirror_score(((0, 0), (1, 3), (3, 1)))
        (mirror_de, shape_pct), phase = (same, "same phase") if same[0] <= swap[0] else \
                                        (swap, "strides swapped")
        OW, OH = WALK_OUT_W, WALK_OUT_H
        sheet = [bytearray(OW * 3 * 4) for _ in range(OH * 3)]
        for r_out, r_in in enumerate((0, 1, 3)):                  # S, side (the old W row), N
            for c_out, c_in in enumerate((0, 1, 3)):              # stand, step-A, step-B
                f = to_rgba(grab(r_in, c_in), fw, fh, ch)
                f = resample(f, fw, fh, 4, OW, OH)
                for y in range(OH):
                    sheet[r_out * OH + y][c_out * OW * 4:(c_out + 1) * OW * 4] = f[y]
        ship_png(src, OW * 3, OH * 3, 4, sheet, pal, pidx)
        write_walker_meta(ident, WALK_ROWS)
        verdict = ("E is a clean mirror of W" if mirror_de < 0.045 and shape_pct < 12 else
                   "E is NOT a mirror of W — this design is asymmetric, so it wants "
                   "'- asymmetric: yes' in characters.md and a 12-frame sheet")
        print(f"ok    {ident}: {w}x{h} (16 frames of {fw}x{fh}) -> {OW * 3}x{OH * 3} "
              f"(9 frames of {OW}x{OH}); mirror dE {mirror_de:.4f}, silhouette "
              f"{shape_pct:.0f}% off ({phase}) — {verdict}"
              f"  [kept {keep.relative_to(ROOT.parent)}]")
        rows_out.append({"id": ident, "kind": "walker", "target": f"{OW * 3}x{OH * 3}",
                         "package": "compacted"})
    if rows_out:
        write_field_manifest(rows_out)


def cmd_cut(args):
    pos, fringe, palette, it = [], 0, 0, iter(args)
    for a in it:
        if a == "--fringe":
            fringe = int(next(it, "0"))
        elif a == "--palette":
            palette = int(next(it, "0"))
        elif not a.startswith("--"):
            pos.append(a)
    heal, snap = "--no-heal" not in args, "--snap32" in args
    if len(pos) != 2:
        die("usage: cut story/out/<name>.sheet.json <image> [--fringe N] [--nearest] "
            "[--palette N] [--no-heal] [--snap32]")
    data = json.loads(Path(pos[0]).read_text())
    image = Path(pos[1]).expanduser()
    kind = data.get("kind")
    if kind == "view":                                   # a painting is fitted, not cut into slots
        if not image.exists():
            die(f"{image} not found")
        return cut_view(data, image, "--nearest" in args)
    if kind not in FIELD_KINDS + ("tileset", "swatch", "decal"):
        die(f"{pos[0]} is not a template package (kind '{kind}'). For a shot sheet use: story_prompt.py slice")
    if not image.exists():
        die(f"{image} not found")
    slots, (tw, th) = data["slots"], data["canvas"]
    if kind == "walker" and len(slots) % 3 and len(slots) != 16:
        die(f"{pos[0]} has {len(slots)} slots; a walk sheet is 3 columns x 3 or 4 rows a character "
            f"(or the 4x4 grid of a sheet drawn before D20). Nothing written.")
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
    localise_slots(rows, w, h, ch, slots, sx, sy, f" ({image.name})")

    if kind == "tileset":                                 # a tile sheet: snap, key, pack into the atlas
        return cut_tileset(data, slots, rows, w, h, ch, sx, sy, palette, heal, snap)
    if kind == "swatch":                                  # D20: flatten, make it wrap, palettise
        return cut_swatches(data, slots, rows, w, h, ch, sx, sy)
    if kind == "decal":                                   # D20: key, trim to the object, palettise
        return cut_decals(data, slots, rows, w, h, ch, sx, sy, fringe)
    if kind in ("tiles", "building"):                     # opaque faces: resize to the target, drop any alpha
        FIELD_DIRS[KIND_DIR[kind]].mkdir(parents=True, exist_ok=True)
        for s in slots:
            x, y, bw, bh = scaled_inner(s, sx, sy, w, h)
            tw_, th_ = s["target"]
            px = resize_nn(crop(rows, ch, x, y, bw, bh), bw, bh, ch, tw_, th_)
            scrub_border(px, tw_, th_, ch, False)
            if ch == 4:
                px = [bytearray(b for i, b in enumerate(r) if i % 4 != 3) for r in px]
            out = ROOT.parent / s["out"]
            ship_png(out, tw_, th_, 3, px)
            what = f"{s['id']} {s['face']}" if s.get("face") else s["id"]
            print(f"  slot {s['n']} {what}: {bw}x{bh} -> {out.relative_to(ROOT.parent)}  {tw_}x{th_}")
    elif kind == "props":
        FIELD_DIRS["prop"].mkdir(parents=True, exist_ok=True)
        for s in slots:
            x, y, bw, bh = scaled_inner(s, sx, sy, w, h)
            px = key_magenta(crop(rows, ch, x, y, bw, bh), bw, bh, ch, fringe)
            scrub_border(px, bw, bh, 4, True, max(2, int(round(2 * sx))))
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
            ship_png(out, nw, nh, 4, px)
            print(f"  slot {s['n']} {s['id']}: {bw}x{bh} -> trimmed {cw}x{chh} -> "
                  f"{out.relative_to(ROOT.parent)}  {nw}x{nh}{check_alpha(px, nw, nh, out)}")
    else:
        cut_walker(data, slots, rows, w, h, ch, sx, sy, fringe)

    rows_out = ([{"id": data["template"].split("_", 1)[1], "kind": "walker",
                  "target": f"{WALK_OUT_W * 4}x{WALK_OUT_H * 4}", "package": data["template"]}]
                if kind == "walker" else kind_rows(KIND_DIR[kind], slots, data["template"]))
    write_field_manifest(rows_out)
    print(f"{FIELD_MANIFEST.relative_to(ROOT.parent)} updated")


# ───────────────────────── Tile field (D18): tilesets, atlases and tmaps ─────────────────────────
#
# Owner's call, 2026-09-19, final: "Sega style, top down, using tile sheets, grid walking. No more
# screen navmesh." TILES.md is the contract. The engine owns `story/field/tilesets/<set>/tiles.md`
# (the names, the sizes, the solid rows, the layers); this half owns everything that turns that list
# into art and back into an atlas.
#
#   tileset <set>   read tiles.md, assign any missing atlas index, lay every tile and stamp out on
#                   template sheets at EXACTLY 4x (a 32-px tile is a 128-px slot), write one package
#                   per sheet under story/packages/tilesets/<set>/<sheet>/
#   ingest          cut a returned sheet: snap to the 4x4 art-pixel grid by mode filter, downsample
#                   4:1, key the magenta on anything that is not opaque ground, hard-threshold the
#                   alpha (the engine alpha-tests; there is no partial alpha in a tile), then pack
#                   every tile into story/field/tilesets/<set>/atlas.png at its own index
#   tmap check|preview   validate a .tmap against its tileset, or render it from the atlas
#
# Two things are load-bearing and easy to get wrong:
#
# 1. **Nothing ever moves in the atlas.** An index that tiles.md already carries is never reassigned,
#    and a cut only ever writes the cells its own entries own. That is what lets the engine hard-code
#    indices and lets one sheet be recut years later.
# 2. **The 4x grid.** ChatGPT does not draw on a pixel grid, it draws a picture that looks pixelated.
#    Downsampling that by averaging turns every hard tile edge into mush and the seams stop meeting.
#    So the cut resizes each slot to exactly 4x the tile, then takes the MODE colour of each 4x4
#    block — the colour the artist meant that art pixel to be — and that is the tile.

TILESETS = FIELD / "tilesets"
TMAPS = FIELD / "tmaps"
TSET_TILE = 32                        # a tile is 32x32 map cells' worth of design (TILES.md)
TSET_SCALE = 4                        # ...drawn at 4x on the sheet, so one art pixel is a 4x4 block
TSET_SLOT = TSET_TILE * TSET_SCALE    # 128: one tile's slot on a template sheet
ATLAS_COLS = 16                       # the atlas is 16 tiles wide; index 0 is the empty tile
TSET_CANVAS = (1536, 1024, "landscape, 1536x1024")   # one sheet, never bigger
TSET_GREY = (56, 56, 60)              # the ground sheet's background: neutral, dark, not magenta
TSET_LAYERS = ("ground", "object", "over")
FRINGE_KINDS = ("edge", "corner_out", "corner_in")   # TILES.md: three fringe tiles a terrain, rotated
FRINGE_DEPTH = 10                     # how far the bordering terrain creeps in, in the tile's 32 px
SEAM_WARN = 24.0                      # mean per-channel difference across a ground tile's wrap: warn above
TSET_ALPHA_CUT = 128                  # the engine alpha-tests at 0.5; a tile pixel is on or off
ATLAS_JSON = "atlas.json"

# ── ground families: variants of one material, meant to be MIXED on the map ──
#
# A `.tmap` scatters `paving_worn` through `paving` and `grass_tuft` through `grass` so the ground
# does not repeat. That only works if the variants differ in DETAIL. ChatGPT draws each slot on its
# own and gives one a lighter cast than the next, and the map then reads as a checkerboard of pale
# and dark squares — which is exactly what the first Halm capture showed on the square. So every
# variant is pulled to the FIRST member's mean lightness and mean chroma in Oklab (a perceptual
# space: shifting L there does not swing the hue the way scaling RGB does). Detail, texture and
# local contrast are untouched; only the overall tone is matched.
GROUND_FAMILIES = (
    ("grass", "grass_tuft", "grass_flower"),
    ("paving", "paving_worn"),
    ("dirt", "dirt_rut"),
)

# ── how full a sheet is packed ──
#
# A tile slot is fixed at 4x (128 px) and cannot be shrunk to fit, so the only way to ask for fewer
# generations is to put more slots on each canvas. The first cut of this laid one sheet out per
# category, one shelf-row at a time, and left most of nine canvases empty: 12 ground tiles took a
# whole 1536x1024 sheet, the guild hall took another with a quarter of the canvas used. These
# numbers and the skyline packer below fill the canvas instead — big things first, small stamps
# tucked into the space beside and under them.
TSET_MARGIN = 20                      # canvas edge to the first slot border
TSET_GUTTER_X, TSET_GUTTER_Y = 20, 28 # between slot borders; the vertical one holds the slot number
TSET_DIGIT = (4, 4)                   # (scale, gap): a 3x5 digit at 4x is 12x20, inside GUTTER_Y
TSET_MAX_SLOTS = 26                   # more than this on one sheet and the detail per slot suffers
TSET_MAX_FILL = 0.85                  # ...and no more than this much of the canvas is slot area

# Which sheet an entry lands on when it carries no '- sheet:' line. The keyword decides the *family*;
# the packer below then merges families onto shared canvases, because a family rarely fills one.
TSET_SHEET_WORDS = (
    ("buildings", ("house", "roof", "wall", "door", "window", "shop", "inn", "gate", "chimney",
                   "eave", "porch", "hut", "tower", "shrine", "steps", "stair")),
    ("nature", ("tree", "bush", "rock", "stump", "log", "flower", "grass_tuft", "reed", "cliff",
                "boulder", "hedge", "crop", "vine", "water_", "fall")),
)
# Families that share a canvas. Ground and its fringes go together (they must share one palette —
# a fringe is the same material as its ground, drawn ragged), which means one background for both:
# magenta, with the ground slots painted edge to edge so nothing of it survives to be keyed.
TSET_FAMILY = {"ground": "terrain", "fringes": "terrain"}
TSET_OBJECT_FAMILIES = ("buildings", "nature", "props")


def tileset_dir(name):
    return TILESETS / slug(name)


def tileset_doc(name):
    return tileset_dir(name) / "tiles.md"


def atlas_path(name):
    return tileset_dir(name) / "atlas.png"


def cut_sheet_path(name, sheet):
    return tileset_dir(name) / "cut" / f"{sheet}.png"


def is_fringe(name):
    return any(name.endswith("_" + k) for k in FRINGE_KINDS)


def fringe_kind(name):
    return next((k for k in FRINGE_KINDS if name.endswith("_" + k)), "")


def sheet_guess(name, entry):
    """The sheet an entry belongs on with no '- sheet:' line of its own."""
    if entry.get("sheet"):
        return slug(entry["sheet"])
    if is_fringe(name):
        return "fringes"
    if entry["layer"] == "ground":
        return "ground"
    for sheet, words in TSET_SHEET_WORDS:
        if any(w in name for w in words):
            return sheet
    return "props"


def parse_wh(text, where):
    m = re.fullmatch(r"(\d+)\s*[xX]\s*(\d+)", (text or "").strip())
    if not m:
        die(f"{where}: '{text}' is not a WxH size")
    w, h = int(m.group(1)), int(m.group(2))
    if not (1 <= w <= ATLAS_COLS and 1 <= h <= 16):
        die(f"{where}: {w}x{h} is not a usable stamp size (1..{ATLAS_COLS} across, 1..16 down)")
    return w, h


def tileset_entries(name):
    """Ordered {id: entry} from story/field/tilesets/<set>/tiles.md. The engine owns this file."""
    path = tileset_doc(name)
    if not path.exists():
        die(f"{path.relative_to(ROOT.parent)} not found. The engine writes it: one '## <id>' per tile "
            f"or stamp, with '- layer:', '- solid:' and '- desc:' (see TILES.md).")
    out, unknown = {}, []
    for ident, body in h2_sections(detok(path.read_text(), unknown)).items():
        if not re.fullmatch(r"[a-z0-9_]+", ident):
            die(f"{path.name}: '## {ident}' is not a usable tile id (lower case, digits, underscores)")
        kv = kv_lines(body)
        desc = kv.get("desc") or next((squash(l) for l in body.splitlines()
                                       if l.strip() and not re.match(r"^\s*- [\w ]+?:", l)), "")
        layer = (kv.get("layer") or "ground").lower()
        if layer == "fringe":                       # a friendlier synonym the engine may use
            layer = "ground"
        if layer not in TSET_LAYERS:
            die(f"{path.name}: '## {ident}' has '- layer: {layer}'; use one of: {', '.join(TSET_LAYERS)}")
        idx, w, h = None, 1, 1
        if "index" in kv:
            m = re.fullmatch(r"(\d+)(?:\s+(\d+\s*[xX]\s*\d+))?", kv["index"].strip())
            if not m:
                die(f"{path.name}: '## {ident}' has '- index: {kv['index']}'; "
                    f"write 'index: 12' or 'index: 12 4x3' for a stamp")
            idx = int(m.group(1))
            if m.group(2):
                w, h = parse_wh(m.group(2), f"{path.name} '{ident}'")
        elif kv.get("size") or kv.get("footprint"):
            w, h = parse_wh(kv.get("size") or kv["footprint"], f"{path.name} '{ident}'")
        frames = int(kv["frames"]) if re.fullmatch(r"\d+", (kv.get("frames") or "").strip()) else 1
        if frames < 1 or frames > ATLAS_COLS:
            die(f"{path.name}: '## {ident}' has '- frames: {kv.get('frames')}'; 1..{ATLAS_COLS}")
        e = {"id": ident, "desc": desc, "layer": layer, "index": idx, "w": w, "h": h,
             "frames": frames, "solid": (kv.get("solid") or "no").strip(),
             "sheet": kv.get("sheet"), "raw": kv}
        # TILES2 (D20): an entry is a terrain (a swatch and a computed edge), a decal (a scattered
        # cut-out) or a plain atlas tile/stamp. Anything with no '- kind:' is a tile, which is what
        # every entry written before D20 is.
        e["kind"] = (kv.get("kind") or ("terrain" if kv.get("terrain", "").lower().startswith("y")
                                        else "decal" if kv.get("decal", "").lower().startswith("y")
                                        else "tile")).strip().lower()
        if e["kind"] not in TERRAIN_KINDS:
            die(f"{path.name}: '## {ident}' has '- kind: {e['kind']}'; "
                f"use one of: {', '.join(TERRAIN_KINDS)}")
        e["pass"] = parse_passmask(kv.get("pass"), ident, path.name)
        e["tag"] = (kv.get("tag") or "").strip().lower() or None
        e["over_rows"] = int(kv["over"]) if re.fullmatch(r"\d+", (kv.get("over") or "").strip()) else 0
        e["flip"] = "h" in (kv.get("flip") or "").lower()
        if e["kind"] == "terrain":
            parse_terrain(ident, kv, e, path.name)
        elif e["kind"] == "decal":
            parse_decal(ident, kv, e, path.name)
        e["opaque"] = e["layer"] == "ground" and not is_fringe(ident)
        e["cells"] = (w * frames, h)                 # what it occupies in the atlas and on the sheet
        e["foot"] = (w, h)                           # what it occupies on a map: frames are alternatives
        e["sheet"] = sheet_guess(ident, e)
        if not desc:
            die(f"{path.name}: '## {ident}' has no '- desc:' line; the art pipeline has nothing to ask for")
        check_description(desc, ident, str(path.relative_to(ROOT.parent)))
        out[ident] = e
    if not out:
        die(f"{path.name}: no '## <id>' entries")
    if unknown:
        die(f"{path.name}: unknown name token(s) {', '.join('{{%s}}' % t for t in unknown)}")
    return out


def atlas_cells(idx, w, h):
    return [idx + ATLAS_COLS * r + c for r in range(h) for c in range(w)]


def assign_indices(name, entries):
    """Give every entry that has none an atlas index, and write just those lines back. [(id, index)]."""
    used, path = {}, tileset_doc(name)
    for e in entries.values():
        if e["index"] is None:
            continue
        w, h = e["cells"]
        if e["index"] < 1:
            die(f"{path.name}: '## {e['id']}' has index {e['index']}; index 0 is the empty tile")
        if e["index"] % ATLAS_COLS + w > ATLAS_COLS:
            die(f"{path.name}: '## {e['id']}' at index {e['index']} is {w} cells wide and would wrap "
                f"past column {ATLAS_COLS}. A stamp is one rectangle in the atlas; move it to a row start.")
        for c in atlas_cells(e["index"], w, h):
            if c in used:
                die(f"{path.name}: '## {e['id']}' at index {e['index']} overlaps '## {used[c]}' at "
                    f"atlas cell {c}. Indices already written are never moved by this tool; fix tiles.md.")
            used[c] = e["id"]
    fresh = []
    for e in entries.values():                       # in file order, so a fresh file reads in order
        if e["index"] is not None or e["kind"] == "terrain":
            continue                                 # a terrain is a swatch sampled in world space;
                                                     # it has no atlas cell. A DECAL does: the engine
                                                     # draws it from the atlas like anything else.
        w, h = e["cells"]
        idx = 1
        while True:
            if idx % ATLAS_COLS + w <= ATLAS_COLS and all(c not in used for c in atlas_cells(idx, w, h)):
                break
            idx += 1
        e["index"] = idx
        for c in atlas_cells(idx, w, h):
            used[c] = e["id"]
        fresh.append((e["id"], idx, e["w"], e["h"], e["frames"]))
    if fresh:
        write_indices(path, fresh)
    return fresh


def write_indices(path, fresh):
    """Insert a '- index:' line into each named entry, leaving every other byte of the file alone."""
    lines = path.read_text().splitlines(keepends=True)
    want = {ident: (idx, w, h) for ident, idx, w, h, _ in fresh}
    starts = [i for i, l in enumerate(lines) if re.match(r"^## (?!#)", l)]
    edits = []
    for n, i in enumerate(starts):
        ident = lines[i][3:].strip().lower()
        if ident not in want:
            continue
        end = starts[n + 1] if n + 1 < len(starts) else len(lines)
        idx, w, h = want[ident]
        text = f"- index: {idx}" + (f" {w}x{h}" if w * h > 1 else "") + "\n"
        at = next((j for j in range(i + 1, end) if re.match(r"^- [\w ]+?:", lines[j])), None)
        if at is None:                               # no metadata at all: after the last non-blank line
            at = max([j for j in range(i + 1, end) if lines[j].strip()], default=i) + 1
        edits.append((at, text))
    for at, text in sorted(edits, reverse=True):
        lines.insert(at, text)
    path.write_text("".join(lines))


# ── laying the sheets out ──
#
# Two jobs here: which entries share a canvas, and where on that canvas each one goes. The first cut
# of this did one sheet per category and one shelf-row at a time, and left most of nine canvases
# empty — twelve ground tiles had a whole 1536x1024 to themselves, the guild hall used a quarter of
# another. The owner's question ("can't we generate more tiles per image?") is answered here.
#
# GROUPING. A category is rarely a canvas's worth on its own, so they are merged into two pools:
# `terrain` (ground plus its fringes — they are the same materials and must share one palette) and
# `objects` (buildings, nature and props together). A `- sheet:` line in tiles.md still wins outright.
#
# PLACEMENT. A skyline packer, biggest first: each slot is dropped at the lowest, then leftmost,
# place it fits. That is what stands three 1x1 props up the side of an 8x6 guild hall instead of
# opening a ninth sheet for them. A sheet stops at TSET_MAX_SLOTS slots or TSET_MAX_FILL of the
# canvas, whichever comes first; slots are numbered afterwards, in reading order.

def tset_slot_box(cells):
    """(w, h) of the white rectangle for an entry of (cw, ch) cells: the art area plus its border."""
    return (cells[0] * TSET_SLOT + 2 * SLOT_BORDER, cells[1] * TSET_SLOT + 2 * SLOT_BORDER)


class TsetSheet:
    """One canvas being filled. `place` drops a slot at the lowest then leftmost spot it fits."""

    def __init__(self, W=None, H=None):
        W = W or TSET_CANVAS[0]
        H = H or TSET_CANVAS[1]
        scale, gap = TSET_DIGIT
        self.top = TSET_MARGIN + 5 * scale + gap      # the first row's numbers live above it
        self.avail_w = W - 2 * TSET_MARGIN
        self.avail_h = H - TSET_MARGIN - self.top
        self.sky = [0] * self.avail_w                 # the used height at each x, from `self.top`
        self.boxes, self.area, self.canvas = [], 0, W * H

    def _spots(self):
        """Every x where the skyline steps — the only x worth trying."""
        return [0] + [x for x in range(1, self.avail_w) if self.sky[x] != self.sky[x - 1]]

    def place(self, cells, commit=True):
        """The box this entry would take, or None if it will not fit or would break a cap."""
        bw, bh = tset_slot_box(cells)
        if bw > self.avail_w or bh > self.avail_h:
            return None
        if commit and (len(self.boxes) >= TSET_MAX_SLOTS
                       or (self.area + bw * bh) / self.canvas > TSET_MAX_FILL):
            return None
        pw = bw + TSET_GUTTER_X                       # the gutter is carried on the right and below
        best = None
        for x in self._spots():
            if x + bw > self.avail_w:
                break
            y = max(self.sky[x:x + pw])
            if y + bh <= self.avail_h and (best is None or (y, x) < best):
                best = (y, x)
        if best is None:
            return None
        y, x = best
        box = (TSET_MARGIN + x, self.top + y, bw, bh)
        if commit:
            hi = y + bh + TSET_GUTTER_Y
            for xx in range(x, min(self.avail_w, x + pw)):
                if self.sky[xx] < hi:
                    self.sky[xx] = hi
            self.area += bw * bh
            self.boxes.append(box)
        return box

    def fill(self):
        return self.area / self.canvas

    def used(self):
        """The height the content actually needs, margin included."""
        return max((y + h for _, y, _, h in self.boxes), default=self.top) + TSET_MARGIN


def tset_canvas_for(ids, entries):
    """(W, H, boxes) — the smallest canvas this group packs onto, not the biggest.

    ChatGPT caps what it returns at about 1.5 megapixels whatever canvas it is given, so half a blank
    sheet is half the pixels thrown away: the twelve ground tiles on a 1536x1024 template came back
    with a quarter of the detail they could have had. So every width from the widest down is tried and
    the canvas is trimmed to the content; the smallest sensibly-shaped one wins, and a sheet that
    really does need the full width (an 8x6 hall) simply keeps it."""
    MAXW, MAXH, _ = TSET_CANVAS
    sizes = [entries[i]["cells"] for i in ids]
    best = None
    for W in range(640, MAXW + 1, 64):
        sheet = TsetSheet(W, MAXH)
        boxes = [sheet.place(c, commit=True) for c in sizes]
        if any(b is None for b in boxes):
            continue
        H = min(MAXH, sheet.used())
        if not (0.6 <= W / H <= 2.0) and W != MAXW:
            continue
        if best is None or W * H < best[0] * best[1]:
            best = (W, H, boxes)
    if best is None:                                     # nothing shapely fits: the full sheet, as before
        sheet = TsetSheet()
        boxes = [sheet.place(c) for c in sizes]
        if any(b is None for b in boxes):
            return None
        best = (MAXW, min(MAXH, sheet.used()), boxes)
    return best


def tileset_sheets(entries):
    """{pool: [ids]} in file order. Families are merged; a '- sheet:' line keeps its own pool."""
    by_pool = {}
    for ident, e in entries.items():
        if e["kind"] != "tile":                       # terrains and decals have sheets of their own
            continue                                  # (a decal is in the atlas, but not on these)
        if e["raw"].get("sheet"):                     # the author named a sheet: leave it alone
            pool = slug(e["raw"]["sheet"])
        else:
            pool = TSET_FAMILY.get(e["sheet"], "objects")
        by_pool.setdefault(pool, []).append(ident)
    # Within a pool, the whole tiles before the fringes, each in file order: that is what numbers a
    # terrain sheet's ground tiles 1-12 and its fringes 13-24, so the prompt can say so in one line.
    return {k: sorted(v, key=lambda i: (is_fringe(i), v.index(i))) for k, v in by_pool.items()}


def tileset_pack(ids, entries):
    """[[ids]] split into canvas-fuls: biggest first, then the single tiles filling the gaps.

    Each group comes back in the order it was placed, which is the order `tileset_slots` replays."""
    # Tallest first, then widest, then the order the author wrote them in. Height leads because that
    # is what the skyline has to fit round; it also leaves a sheet of same-height tiles (a terrain
    # sheet) in file order, so the ground tiles are numbered before their fringes.
    cells = lambda i: entries[i]["cells"]
    order = sorted(range(len(ids)), key=lambda n: (-cells(ids[n])[1], -cells(ids[n])[0], n))
    big = [ids[n] for n in order if entries[ids[n]]["cells"] != (1, 1)]
    small = [ids[n] for n in order if entries[ids[n]]["cells"] == (1, 1)]
    groups = []
    while big or small:
        sheet, cur = TsetSheet(), []
        for pool in (big, small):                     # the big things first, then fill round them
            for i in list(pool):
                if sheet.place(entries[i]["cells"]) is not None:
                    cur.append(i)
                    pool.remove(i)
        if not cur:
            i = (big or small)[0]
            cw, chh = entries[i]["cells"]
            W, H, _ = TSET_CANVAS
            die(f"'{i}' is {cw}x{chh} tiles ({cw * TSET_SLOT}x{chh * TSET_SLOT} px at {TSET_SCALE}x) "
                f"and does not fit a {W}x{H} sheet on its own. Split the stamp or shrink it in "
                f"tiles.md.")
        groups.append(cur)
    return groups


def tileset_slots(ids, entries):
    """(W, H, label, slots) for one sheet: the same skyline pack, trimmed, numbered in reading order."""
    got = tset_canvas_for(ids, entries)
    if got is None:
        die(f"{len(ids)} slots do not fit a {TSET_CANVAS[0]}x{TSET_CANVAS[1]} sheet: "
            + ", ".join(ids))
    W, H, boxes = got
    label = f"{'landscape' if W >= H else 'portrait'}, {W}x{H}"
    placed = list(zip(ids, boxes))
    placed.sort(key=lambda p: (p[1][1], p[1][0]))     # number them left to right, top to bottom
    slots = []
    for n, (i, box) in enumerate(placed, 1):
        # The white border goes OUTSIDE the art, unlike the older field templates: a tile slot has to
        # be exactly 4x the tile (128 px) so the returned image can be cropped and read back on the
        # 4x4 art-pixel grid with no resampling at all.
        inner = (box[0] + SLOT_BORDER, box[1] + SLOT_BORDER,
                 box[2] - 2 * SLOT_BORDER, box[3] - 2 * SLOT_BORDER)
        e = entries[i]
        slots.append({"n": n, "id": i, "box": list(box), "inner": list(inner),
                      "cells": list(e["cells"]), "index": e["index"], "layer": e["layer"],
                      "frames": e["frames"], "opaque": e["opaque"],
                      "kind": ("ground" if e["opaque"] else "fringe" if is_fringe(i) else "stamp"),
                      "target": [e["cells"][0] * ATLAS_CELL, e["cells"][1] * ATLAS_CELL]})
    return W, H, label, slots


def num_ranges(nums):
    """[1,2,3,7,9,10] -> '1-3, 7, 9-10' — how the prompt names a group of slots."""
    out, nums = [], sorted(nums)
    for n in nums:
        if out and out[-1][1] == n - 1:
            out[-1][1] = n
        else:
            out.append([n, n])
    return ", ".join(str(a) if a == b else f"{a}-{b}" for a, b in out)


FRINGE_RULE = (
    "These are the ids ending in _edge, _corner_out and _corner_in, and they are not whole tiles: "
    "each one is a ragged scrap of its own terrain drawn over the empty background, which the game lays on top "
    "of the neighbouring ground so the boundary between two terrains is not a straight line. Draw "
    f"only the terrain, never the neighbour, and let the background show through the rest:\n"
    f"  - _edge: a band of the terrain along the NORTH edge of the slot, solid and full width at the "
    f"very top, reaching about {FRINGE_DEPTH} of the tile's 32 pixels down at its deepest, with a "
    f"ragged, organic, uneven southern boundary — clumps and gaps, never a straight line and never a "
    f"repeating scallop. The bottom two thirds of the slot is background.\n"
    f"  - _corner_out: the same terrain filling only the NORTH-EAST corner, reaching about "
    f"{FRINGE_DEPTH} pixels in from the north edge and {FRINGE_DEPTH} pixels in from the east edge, a "
    f"ragged quarter-round. Everything else is background.\n"
    f"  - _corner_in: the exact inverse — the terrain fills the whole slot EXCEPT a ragged bite out of "
    f"the SOUTH-WEST corner about {FRINGE_DEPTH} pixels deep from the south edge and {FRINGE_DEPTH} "
    f"pixels in from the west edge. Only that bite is background.\n"
    "A slot's own description below gives the MATERIAL only — what the terrain is made of. Where it "
    "also mentions an edge or a corner, the three shapes above win: draw the shape described here, in "
    "that material.\n"
    "The north / north-east / south-west wording is fixed: the game rotates each of these three by 90, 180 and "
    "270 degrees to cover the other sides and corners, so all three must be drawn at the orientation "
    "described and the terrain must meet the slot's edges at full strength wherever it touches them."
)


KIND_TITLE = {"ground": "GROUND TILES", "fringe": "FRINGE TILES", "stamp": "STAMPS (objects)"}


def ground_rule(on_magenta):
    """What a ground slot is. On a mixed sheet the magenta must not show through one."""
    return ("GROUND TILES are flat terrain seen from straight above, and each one is FILLED EDGE TO "
            "EDGE with its material, right up to the inside of its white border: opaque, no frame, no "
            "vignette, no empty corner"
            + (", AND NO MAGENTA ANYWHERE INSIDE THE SLOT — the background colour must not show "
               "through a ground tile at all, not even in a corner or as a tint along an edge. "
               if on_magenta else ". ")
            + "Each one is SEAMLESS on all four edges: the right edge continues into the left edge "
              "and the bottom edge into the top, so a floor tiled with copies of it shows no seam. "
              "Keep the detail even and small — no single large feature the eye can count across a "
              "field, no object standing on the ground, no character, and no shadow cast by anything "
              "outside the tile.")


STAMP_RULE = (
    "STAMPS are objects that stand on the ground: a house, a tree, a barrel. Each one is drawn to "
    "fill its slot's width and STANDS ON THE SLOT'S BOTTOM EDGE — its base touches the bottom inside "
    "edge of the border, centred left to right — and everything else in the slot is left as the flat "
    "background, which the tool keys out. No ground under it (the map draws its own), no cast shadow "
    "beyond a contact shadow no more than one tile high tucked under its south face. Every stamp on "
    "this sheet is drawn to ONE COMMON SCALE: a tile is a stride, a person would be one tile wide and "
    "about one and a half tiles tall, and a door is one tile wide and about 1.3 tiles tall, centred on "
    "a tile column so it lines up with the grid."
)


def tileset_package_lines(setname, sheet, ids, entries, slots, style, label, opaque_sheet, attach):
    b = style["blocks"]
    on_magenta = not opaque_sheet
    by_kind = {}
    for s in slots:
        by_kind.setdefault(s["kind"], []).append(s)
    L = [f"Create ONE image: the attached template with all {len(slots)} numbered slots filled in. "
         f"Canvas: {label}, exactly the same size and proportions as the template.", "",
         f"PIXEL GRID — THE MOST IMPORTANT RULE ON THIS SHEET. Everything is drawn at {TSET_SCALE}x. "
         f"Every art pixel is a {TSET_SCALE}x{TSET_SCALE} block of identical colour, aligned to the "
         f"slot: the blocks start at the slot's top-left inside corner and run in an even grid across "
         f"and down it, so a {TSET_SLOT}-pixel slot is a {TSET_TILE}x{TSET_TILE} pixel-art tile blown "
         f"up {TSET_SCALE} times. No block is half a colour, no edge falls between blocks, no detail "
         f"is finer than one block, and no two objects on the sheet use different block sizes. This "
         f"is pixel art at 4x, not a smooth drawing shrunk down.", "",
         b["map_header"], "",
         "ATTACHED REFERENCE IMAGES, in the order I attached them:"]
    L += [f"Image {n}: {note}" for n, (_, note) in enumerate(attach, 1)]
    L += ["", TEMPLATE_RULES, "",
          f"WHAT THIS SHEET IS: the '{sheet}' tile sheet for the '{setname}' tileset of a top-down "
          f"16-bit JRPG field map. Every slot is one tile or one multi-tile object, and in game each "
          f"tile is exactly {TSET_TILE}x{TSET_TILE} pixels, so a {TSET_SLOT}-pixel slot is one tile "
          f"and a {2 * TSET_SLOT}x{3 * TSET_SLOT} slot is a 2x3-tile object.", "",
          "THIS SHEET HOLDS " + " AND ".join(
              f"{len(v)} {k} tile(s) in slot(s) {num_ranges([s['n'] for s in v])}"
              for k, v in by_kind.items()) + ". The rules for each kind:", ""]
    for kind in ("ground", "fringe", "stamp"):
        if kind not in by_kind:
            continue
        nums = num_ranges([s["n"] for s in by_kind[kind]])
        head = f"{KIND_TITLE[kind]} — SLOT(S) {nums}. "
        L += [head + (ground_rule(on_magenta) if kind == "ground" else
                      FRINGE_RULE if kind == "fringe" else STAMP_RULE), ""]
    if on_magenta:
        L += ["BACKGROUND: the flat magenta around and between the objects is empty space and is keyed "
              "out by the tool. Leave it completely untouched right up to the edge of what you draw: "
              "no magenta tint on an object, no soft halo, no gradient, no drop shadow lying on the "
              "magenta."
              + (" The ground slots listed above are the exception and the one thing to get right: "
                 "they are painted over completely, so no magenta survives inside them."
                 if "ground" in by_kind else ""), ""]
    else:
        L += ["BACKGROUND: the flat dark grey around the slots is the template and is thrown away. "
              "Every slot on this sheet is painted over it edge to edge.", ""]
    L += ["PROJECTION — top-down oblique, the Phantasy Star IV field view, the same for every slot:", "",
          "- The ground is seen from straight above: a patch of grass, dirt, paving or water is a flat "
          "texture with no thickness and no side visible.",
          "- An object standing on the ground is seen from slightly in front of straight above: you see "
          "its top or roof AND its south-facing front, and nothing of its north, east or west sides.",
          "- Every vertical edge in the world — a wall corner, a tree trunk, a post — runs straight up "
          "the image. Nothing leans, nothing converges, nothing is foreshortened.",
          "- No perspective, no vanishing point, no horizon, no sky. Nothing gets smaller because it is "
          "further away: a thing the same size in the world is the same size in pixels wherever it sits.",
          "- One light direction for the whole sheet, from the upper left. Every object's shading and "
          "every contact shadow agrees with it."]
    if "stamp" in by_kind:
        L += ["- The slots are different sizes because the things in them are different sizes. Nothing "
              "is scaled to fill a bigger slot: a barrel in a 1-tile slot and a barn in a 4x3 slot are "
              "drawn at the same number of pixels to the foot."]
    L += [""]
    L += [f"SLOTS — {len(slots)} of them, grouped by kind:"]
    for kind in ("ground", "fringe", "stamp"):
        if kind not in by_kind:
            continue
        L += ["", f"{KIND_TITLE[kind]}:"]
        for s in by_kind[kind]:
            e = entries[s["id"]]
            cw, chh = e["cells"]
            size = (f"{s['inner'][2]}x{s['inner'][3]} px = {cw}x{chh} tile"
                    + ("s" if cw * chh > 1 else ""))
            what = []
            if e["frames"] > 1:
                what.append(f"{e['frames']} animation frames of the same tile side by side, left to "
                            f"right, each one {e['w']}x{e['h']} tile"
                            + ("s" if e["w"] * e["h"] > 1 else "")
                            + ", the same subject with only the moving part changed, the motion "
                              "looping from the last frame back to the first, and every frame obeying "
                              "the rules above on its own")
            if kind == "fringe":
                what.append(f"drawn as the _{fringe_kind(s['id'])} fringe shape described above, in "
                            f"this material")
            elif kind == "ground":
                what.append("seen straight down from directly above, seamless on all four edges, "
                            "filled edge to edge, opaque")
            else:
                what.append("standing on the slot's bottom edge, everything else left as background")
            L.append(f"Slot {s['n']} ({s['id']}), {size}: {e['desc'].rstrip('.')}. " + ". ".join(
                w[0].upper() + w[1:] for w in what) + ".")
    L += ["", f"RENDERING: {b['map_rendering']}", "",
          f"AVOID: {b['map_negative']}, drawing outside a slot, moving or covering a slot number, a "
          f"border or frame inside a slot, pixels not aligned to the {TSET_SCALE}x{TSET_SCALE} block "
          f"grid, anti-aliased or soft edges, a visible seam at a ground tile's edge, background "
          f"colour showing inside a ground tile, changing the size of the image"]
    return "\n".join(L)


def check_by_kind(slots):
    """The 'afterwards' line for each kind of slot on this sheet — they are checked differently."""
    nums = lambda k: num_ranges([s["n"] for s in slots if s["kind"] == k])
    out, kinds = [], {s["kind"] for s in slots}
    if "ground" in kinds:
        out.append(f"- [ ] Each ground tile ({nums('ground')}) fills its slot right to the border with "
                   f"no background colour showing through anywhere in it, and its left edge would meet "
                   f"its right edge without a seam")
    if "fringe" in kinds:
        out.append(f"- [ ] Each fringe ({nums('fringe')}) is its own terrain only, ragged, at the "
                   f"orientation asked for (north band / north-east corner / bitten south-west corner), "
                   f"reaching the slot edges at full strength where it touches them, background "
                   f"everywhere else")
    if "stamp" in kinds:
        out.append(f"- [ ] Each object ({nums('stamp')}) stands on its slot's bottom edge, with clean "
                   f"background right up against it and no magenta tint on the object itself")
    return out


def build_tileset_sheet(setname, sheet, ids, entries=None, style=None):
    """One sheet's package: template.png, sheet.json, prompt.md."""
    entries = entries or tileset_entries(setname)
    style, warn = style or load_style(), []
    missing = [i for i in ids if i not in entries]
    if missing:
        die(f"{tileset_doc(setname).name}: no entry for {', '.join(missing)}")
    W, H, label, slots = tileset_slots(ids, entries)
    opaque_sheet = all(entries[i]["opaque"] for i in ids)
    bg = TSET_GREY if opaque_sheet else MAGENTA
    name = f"tileset_{setname}_{sheet}"
    attach = field_attachments(style, pkg_path(name, "template.png"), None, warn)
    prompt = tileset_package_lines(setname, sheet, ids, entries, slots, style, label, opaque_sheet, attach)
    atlas = str(atlas_path(setname).relative_to(ROOT.parent))
    cutfile = str(cut_sheet_path(setname, sheet).relative_to(ROOT.parent))
    status = [f"- `{atlas}` — " + ("**exists**" if atlas_path(setname).exists() else "not started yet"),
              f"- `{cutfile}` — " + ("**cut already**" if cut_sheet_path(setname, sheet).exists()
                                     else "not cut yet"), "",
              f"This sheet carries {len(ids)} of the tileset's entries: " + ", ".join(f"`{i}`" for i in ids)]
    after = field_after(pkg_path(name, "sheet.json"), [
        f"- [ ] All {len(slots)} slots filled; every border and every number exactly where it was",
        f"- [ ] Every art pixel is a {TSET_SCALE}x{TSET_SCALE} block on one grid — zoom in and check a "
        f"diagonal edge is a clean staircase of blocks, not a soft ramp",
        "- [ ] Nothing drawn in the gutters; the background between slots is still flat and untouched",
        *check_by_kind(slots),
        "- [ ] One light direction, upper left, across the whole sheet",
        "- [ ] No text, labels, numbers of your own, swatches or signature", "",
        "If one slot fails, reply in the same chat: \"Redraw only slot 4 and keep every other slot and "
        "the whole template exactly as it is. <what was wrong>\"."],
        makes=f"the {len(ids)} tile(s) into `{atlas}`")
    template, manifest, md = write_field_package(
        "tileset", name, label, W, H, bg, slots, prompt, attach, after,
        out_file=atlas, status=status, digit=TSET_DIGIT)
    data = json.loads(manifest.read_text())
    data.update({"set": setname, "sheet": sheet, "cut": cutfile, "opaque_sheet": opaque_sheet,
                 "tile": TSET_TILE, "scale": TSET_SCALE, "cols": ATLAS_COLS})
    manifest.write_text(json.dumps(data, indent=2) + "\n")
    for w in warn:
        print(f"warning: {w}", file=sys.stderr)
    kinds = sorted({s["kind"] for s in slots})
    print(f"wrote {md.relative_to(ROOT.parent)}  ({len(slots)} slots, {'+'.join(kinds)}, "
          f"{100 * sum(s['box'][2] * s['box'][3] for s in slots) / (W * H):.0f}% of {label}, "
          f"{'opaque ground' if opaque_sheet else 'magenta key'})")
    return slots


def tileset_plan(setname, entries=None):
    """[(folder, meta, builder args)] for one tileset, freezing any sheet that has been drawn."""
    entries = entries or tileset_entries(setname)
    here = PACKAGES / "tilesets" / setname
    plan = []
    terr, decs = set_terrains(entries), set_decals(entries)
    if terr:                                              # D20: one sheet for every terrain's swatch
        plan.append((here / "swatches",
                     {"kind": "swatch", "set": setname, "ids": terr,
                      "title": f"{setname} — the ground swatches ({len(terr)} terrains)",
                      "makes": f"{len(terr)} seamless swatch(es) and the generated masks",
                      "outputs": [str(swatch_path(setname, t).relative_to(ROOT.parent)) for t in terr]},
                     [setname, "--swatches"]))
    if decs:                                              # ...and one for the whole biome's decals
        plan.append((here / "decals",
                     {"kind": "decal", "set": setname, "ids": decs,
                      "title": f"{setname} — the decals ({len(decs)} cut-outs)",
                      "makes": f"{len(decs)} decal sprite(s)",
                      "outputs": [str(decal_path(setname, i).relative_to(ROOT.parent)) for i in decs]},
                     [setname, "--decals"]))
    for sheet, ids in tileset_sheets(entries).items():
        done = frozen_packages(here, sheet, "tileset", ids)
        taken = set()
        for _, keep in done:
            taken |= set(keep)
        rest = [i for i in ids if i not in taken]
        groups = tileset_pack(rest, entries) if rest else []
        laid = list(done)
        if not done and len(groups) == 1:
            laid.append((sheet, groups[0]))
        else:
            for g in groups:
                laid.append((next_package_name(sheet, {n for n, _ in laid}), g))
        for n, (folder, group) in enumerate(laid, 1):
            outs = [str(atlas_path(setname).relative_to(ROOT.parent)),
                    str(cut_sheet_path(setname, folder).relative_to(ROOT.parent))]
            plan.append((here / folder,
                         {"kind": "tileset", "set": setname, "sheet": folder, "ids": group,
                          "title": f"{setname} — {folder} tile sheet"
                                   + (f" ({n} of {len(laid)})" if len(laid) > 1 else ""),
                          "makes": f"{len(group)} tile(s) packed into `{outs[0]}`",
                          "outputs": outs},
                         [setname, "--sheet", folder, *group]))
    return plan


TILESETS_README = """\
# story/field/tilesets — the tile art contract

Generated by `./story_prompt.py tileset <set>`; do not edit by hand. `TILES.md` is the decision, this
file is the part the art pipeline fixes so the engine can match it.

## What a set is

`story/field/tilesets/<set>/`

- `tiles.md` — **the engine's file.** One `## <id>` per tile or stamp, with `- layer:`, `- solid:` and
  `- desc:`. The art pipeline never changes a name, a size or a solid row. The one thing it writes
  back is a `- index:` line for an entry that has none, so the atlas position is recorded in the file
  the engine reads. An index already written is never moved.
- `atlas.png` — {cols} cells across, **{cell}x{cell} each**, an **indexed PNG** on
  `story/palette/master.hex` (PALETTE.md, D19): one byte a pixel, index 0 transparent. Index 0 is the empty tile. A stamp of
  `WxH` occupies a `WxH` rectangle of cells whose top-left is its index, row-major, and never wraps
  past the last column. A tile with `- frames: n` occupies `n` copies side by side starting at its
  index.
- `atlas.json` — the same thing for humans and for tools: name to index, size and frame count. Its
  `cell` field is the atlas cell size in pixels; read it rather than assuming one.
- `atlas32.png` — only when a sheet was cut with `--snap32`: the same atlas mode-snapped down to
  {tile}x{tile} a cell, for comparing against the pixel-art reading of a sheet. Not shipped.
- `masks.png` / `masks.json` — the transition masks, **generated by the tool**, never drawn. See
  `TILES.md`, "Terrains, masks and decals".
- `swatches/<terrain>.png` — one seamless swatch a terrain, sampled in world space. Not in the atlas.
- `decals/<id>.png` — the loose cut-out, for the preview and for looking at one. The **game reads the
  decal out of `atlas.png`**, one cell each, art in the cell's top-left, size under `px` in
  `atlas.json`.
- `cut/<sheet>.png` — what one returned sheet cut down to, kept so a sheet can be eyeballed.

## Sizes — the art is kept, not reduced

A tile is **{tile}x{tile}** as a unit of map design: that is what a cell of a `.tmap` means. The art
is another matter. A sheet draws a tile at **{scale}x**, so one slot is **{slot}x{slot}** and a `4x3`
stamp is a `{stamp_w}x{stamp_h}` slot — and the atlas keeps it at exactly that size, **{cell} px a
cell**. The cut used to mode-snap each {scale}x{scale} block down to one art pixel; the owner compared
the returns with the result and preferred the art as drawn, so nothing is thrown away here any more.
`--snap32` still produces the old reading, into `atlas32.png` beside the atlas.

What the cut does to a slot, in order:

1. **Localise.** A return is never an exact scale of the template — it comes back at about 1.5
   megapixels whatever was asked for, and the model redraws each white slot border a few pixels, or
   a few tens of pixels, off. Every side is searched for the border that was actually drawn and the
   art starts just inside it, past its anti-aliased skirt. Without this a sliver of white border is
   cut into the tile: thin white lines all over the atlas, and a seam metric reading the white row.
2. **Resample** the art area to exactly `cells x {cell}` — area-average going down, bilinear going
   up. Never nearest: at a non-integer ratio nearest drops whole columns and tears straight edges.
3. **Key** the magenta by its cast, on a keyed entry only, un-matting the colour so no pink shows
   through the edge. Alpha is **hard** again (PALETTE.md, D19): the keying measures it softly, and
   the palette step at the end turns anything under 0.5 into index 0 and anything above it into its
   own nearest colour. An opaque ground tile is never keyed; background showing inside one is a hole
   and is filled from the nearest painted pixel.
4. **Scrub** what is left of the border: a near-white edge run, or a near-white edge pixel with no
   bright pixel behind it. Nothing in the interior is touched, so white plaster and water foam stay.
   There is **no edge extrusion** any more. It existed so linear filtering at a silhouette would
   pull clean colour rather than black or magenta; with an indexed atlas there is no colour under a
   transparent texel to bleed, and the tile shader blends the four neighbours' *colormap colours*
   with premultiplied alpha instead, index 0 counting as zero. The cells still have to come out
   clean, which is what the scrub is for.
5. **Make a single-cell opaque ground tile seamless** — on by default, `--no-heal` to skip. Not by
   blending: the tile is *rolled* along each axis in turn, so its first and last line were adjacent
   lines of the drawing and the wrap is continuous by construction. The split is the **min-error**
   one of the middle third, so the join lands where the art is quietest rather than across a mortar
   course. The roll puts the original's own discontinuity in the middle of the tile, and that is
   covered by a narrow strip of the tile's own interior (least-error source, 3 px held and 16 px of
   feather) cut in with a **hashed dither** — each output pixel taken whole from one side or the
   other, never averaged, so nothing is blurred. The old `--heal-seams` cross-blend is gone: it
   removed the wrap error and left a soft ribbon at every border, which is what read on the phone as
   a faint grid over grass, dirt and gravel.
6. **Match the tone of a ground family.** `paving`/`paving_worn`, `grass`/`grass_tuft`/
   `grass_flower`, `dirt`/`dirt_rut` are scattered through each other by the maps, so they must
   differ in detail and not in overall tone. Every variant after the first is shifted to the first's
   **mean Oklab lightness** and scaled to its **mean chroma** — an offset and a gain, so each pixel
   keeps its own distance from the mean and the drawing is untouched. Without it a 0.05 gap in L
   between `paving` and `paving_worn` made the square read as a checkerboard of pale and dark
   squares.
7. **Palettise, last.** Every step above happens in full colour — the seamless roll, the tone match,
   the un-matting — and only then is the whole atlas converted to `story/palette/master.hex`: exact
   nearest in Oklab, **no dithering**, alpha hard at 0.5. Change the palette and nothing needs
   redrawing: `./story_prompt.py palette build && ./story_prompt.py ingest --force` re-derives every
   tile from the sheet archived in `story/sheets/`.

Two numbers are printed for every ground tile. `wrap h/v` is the mean difference across the tile's
own edges, and `border/interior gradient` is that compared with the texture's mean gradient on the
same axis: **above 1.15 is a line** (something is still discontinuous) and **near 0 is a band**
(something was averaged). A tile made seamless by construction sits below 1, because the split was
chosen where the art is quiet.

## Sheets — as few generations as possible

A slot is fixed at 4x and cannot be shrunk, so the only way to ask the owner for fewer images is to
put more on each one. Entries are therefore grouped into two pools, not one per category:

- `terrain` — every `layer: ground` entry **and its fringes**, on one sheet. They are the same
  materials and have to share a palette, and a sheet has one background colour, so the background is
  **magenta** and the ground slots are painted **edge to edge** over it: nothing of the background
  survives inside a ground tile, and the cut keys the fringes only.
- `objects` — buildings, nature and props together, biggest first, each sheet then filled up with
  the single-tile stamps, so the barrels and fence posts stand in the space beside the guild hall.

An explicit `- sheet:` line in `tiles.md` still wins and keeps its entries in a pool of their own.

Packing is a **skyline**: each slot is dropped at the lowest, then leftmost, place it fits, with a
20 px gutter (28 px vertically, which is where the slot number goes) and a 20 px margin. A sheet
stops at **26 slots** or **85% of the canvas**, whichever comes first, and slots are numbered
afterwards in reading order. The canvas is then **trimmed to the content**: ChatGPT returns about
1.5 megapixels whatever it is given, so a half-empty 1536x1024 template throws half the detail away.
1536x1024 is the largest a sheet gets.

A sheet that has been generated is **frozen**: its slot boxes never change, so the image can always be
recut. New entries go into a fresh `<sheet>_2` beside it.

## The fringe convention — this is the part the engine has to match

Each terrain that borders another has three fringe tiles, and the engine **rotates** them by 90, 180
and 270 degrees to cover all four sides and all four corners. A fringe tile carries **only its own
terrain**, ragged, over transparency, and is drawn **on top of the neighbouring tile** so the boundary
is not a straight line.

| id | what is drawn, at 0 degrees | what the engine does with it |
| --- | --- | --- |
| `<terrain>_edge` | a band of the terrain along the **north** edge, full width and solid at the very top, about {depth} of the tile's {tile} px deep at its deepest, ragged and uneven along its southern boundary; the rest transparent | laid on the neighbour tile that sits **south** of the terrain; rotate 90/180/270 for the other three sides |
| `<terrain>_corner_out` | the terrain filling only the **north-east** corner, about {depth} px in from the north edge and {depth} px in from the east edge, a ragged quarter-round; the rest transparent | laid on the neighbour diagonally off an **outside (convex)** corner; rotate for the other three |
| `<terrain>_corner_in` | the terrain filling the whole tile **except** a ragged bite out of the **south-west** corner, about {depth} px deep from the south edge and {depth} px in from the west edge | laid on the neighbour at an **inside (concave)** corner; rotate for the other three |

`corner_in` is exactly the inverse of `corner_out` rotated 180 degrees, which is why three tiles cover
a terrain instead of the forty-seven a full autotile needs.

Alpha in a fringe is **hard** — one bit, index 0 or a colour — and the colour under the edge is clean,
un-matted at the alpha the keying measured before it was hardened. A fringe's ragged boundary reads
soft in game all the same, because the shader interpolates between four neighbouring texels'
colormap colours with index 0 as transparent, rather than between the stored bytes.
"""


def write_tilesets_readme():
    TILESETS.mkdir(parents=True, exist_ok=True)
    (TILESETS / "README.md").write_text(TILESETS_README.format(
        cols=ATLAS_COLS, tile=TSET_TILE, cell=ATLAS_CELL, scale=TSET_SCALE, slot=TSET_SLOT,
        depth=FRINGE_DEPTH,
        stamp_w=4 * TSET_SLOT, stamp_h=3 * TSET_SLOT))


def cmd_tileset(args):
    pos = []
    sheet, what, it = None, None, iter(args)
    for a in it:
        if a == "--sheet":
            sheet = next(it, None)
        elif a in ("--swatches", "--decals"):
            what = a[2:]
        elif not a.startswith("--"):
            pos.append(a)
    if not pos:
        die("usage: tileset <set> [--sheet <name> <id>...]   "
            "(reads story/field/tilesets/<set>/tiles.md)")
    setname = slug(pos[0])
    entries = tileset_entries(setname)
    fresh = assign_indices(setname, entries)
    if what == "swatches":                           # called by `packages` as a builder
        return build_swatch_sheet(setname, set_terrains(entries), entries)
    if what == "decals":
        return build_decal_sheet(setname, set_decals(entries), entries)
    if sheet:                                        # one sheet, called by `packages` as a builder
        return build_tileset_sheet(setname, sheet, pos[1:], entries)
    write_tilesets_readme()
    if set_terrains(entries):                        # the masks are generated, never drawn: do it now
        mp, n = write_masks(setname, [entries[t]["edge_style"] for t in set_terrains(entries)])
        print(f"wrote {mp.relative_to(ROOT.parent)}  ({n} masks, generated from the edge styles)")
    for ident, idx, w, h, fr in fresh:
        print(f"assigned index {idx} to '{ident}'" + (f" ({w}x{h})" if w * h > 1 else "")
              + (f" x{fr} frames" if fr > 1 else ""))
    plan, style = tileset_plan(setname, entries), load_style()
    built = set()
    for d, meta, bargs in plan:
        ok, msg = write_package(d, meta, cmd_tileset, bargs)
        built.add(d.resolve())
        print(f"{'ok  ' if ok else 'FAIL'}  {d.relative_to(ROOT.parent)}  "
              f"{len(meta['ids'])} slot(s)" + (f"  {msg}" if msg else ""))
    write_atlas_json(setname, entries)
    print(f"\n{len(plan)} sheet(s) for tileset '{setname}', {len(entries)} entries, "
          f"atlas {atlas_path(setname).relative_to(ROOT.parent)}")
    print(f"convention: {(TILESETS / 'README.md').relative_to(ROOT.parent)}")


# ───────────────────── TILES2 (D20): terrains, masks, swatches, decals ─────────────────────
#
# TILES2_PROPOSAL.md is the research; TILES.md, "Terrains, masks and decals", is the contract.
# The short version: drawn transition tiles are gone. A terrain is ONE seamless swatch sampled in
# world space, and the boundary between two terrains is a 1-bit mask the TOOL computes — never art.
# Every mask boundary crosses a tile edge at that edge's midpoint, perpendicular, which is the whole
# reason any shape meets any other shape in any rotation with no break.

TSET_MASKS = "masks.png"
TSET_MASKS_JSON = "masks.json"
TSET_SWATCHES = "swatches"            # <set>/swatches/<terrain>.png
MASK_PX = ATLAS_CELL                  # a mask is one display tile at the atlas's own cell size
MASK_VARIANTS = 3                     # per (shape, style); hash(i, j, terrain) picks one
MASK_SHAPES = ("corner", "edge", "diag", "inv", "full")
EDGE_STYLES = {                       # amp/freq of two octaves of value noise displacing the boundary
    "ragged": (0.105, 5.5, 0.045, 13.0),      # grass, dirt, crop, mud
    "smooth": (0.022, 3.0, 0.010, 7.0),       # paving, plank, any laid floor
    "bank":   (0.060, 3.5, 0.022, 9.0),       # water: a hard Phantasy Star bank, not a soft shore
}
MASK_WINDOW = 0.16                    # the displacement is windowed to zero this close to the border,
                                      # so the pinned midpoints survive whatever the noise does
SWATCH_MIN, SWATCH_MAX = 2, 4         # tiles a side
DECAL_MIN_SLOT = 96                   # a decal slot smaller than this is not worth asking for
TERRAIN_KINDS = ("terrain", "decal", "tile")


def h32(*v):
    """FNV-1a. The engine computes the same hash for the same (i, j, terrain), so the tool's preview
    and the phone scatter identically — keep the constants if this is ever ported."""
    n = 2166136261
    for x in v:
        n = ((n ^ (int(x) & 0xffffffff)) * 16777619) & 0xffffffff
    return n


def vnoise(x, y, seed):
    """Value noise on the unit lattice, smoothstepped. Range about -1..1."""
    xi, yi = math.floor(x), math.floor(y)
    tx, ty = x - xi, y - yi
    tx = tx * tx * (3 - 2 * tx)
    ty = ty * ty * (3 - 2 * ty)
    r = lambda a, b: (h32(a, b, seed) & 0xffff) / 65535.0 * 2 - 1
    a = r(xi, yi) * (1 - tx) + r(xi + 1, yi) * tx
    b = r(xi, yi + 1) * (1 - tx) + r(xi + 1, yi + 1) * tx
    return a * (1 - ty) + b * ty


def mask_sdf(shape, x, y):
    """Signed distance over the unit display tile, positive inside the terrain.

    Radius 0.5 is not taste. It is what puts every boundary through the midpoint of the tile edge it
    crosses, which is the invariant that lets any shape meet any other shape, in any rotation, in any
    variant, with no break. Change it and the set stops composing."""
    if shape == "full":
        return 1.0
    if shape == "edge":
        return 0.5 - y                                             # terrain in the north half
    if shape == "corner":
        return 0.5 - math.hypot(x, y)                              # quarter disc at NW
    if shape == "diag":                                            # two quarter discs TOUCHING at the
        return max(0.5 - math.hypot(x, y),                         # centre: the saddle is drawn
                   0.5 - math.hypot(x - 1, y - 1))                 # connected, by convention
    if shape == "inv":
        return math.hypot(x - 1, y - 1) - 0.5                      # all but a bite at SE
    die(f"unknown mask shape '{shape}'")


def bake_mask(shape, style, variant, px=MASK_PX):
    """One mask as rows of 0/1. Rotation is not baked: the engine swizzles the UV."""
    a1, f1, a2, f2 = EDGE_STYLES[style]
    seed = h32(MASK_SHAPES.index(shape), sorted(EDGE_STYLES).index(style), variant)
    rows = []
    for py in range(px):
        y = (py + 0.5) / px
        line = bytearray(px)
        for x_ in range(px):
            x = (x_ + 0.5) / px
            d = mask_sdf(shape, x, y)
            if shape != "full":
                win = min(1.0, min(x, y, 1 - x, 1 - y) / MASK_WINDOW)
                d += win * (a1 * vnoise(x * f1, y * f1, seed)
                            + a2 * vnoise(x * f2, y * f2, seed + 7))
            line[x_] = 1 if d > 0 else 0
        rows.append(line)
    return rows


def bits_rot(b):
    """One clockwise 90-degree step of the corner bit word (1 NW, 2 NE, 4 SE, 8 SW)."""
    return ((b << 1) | (b >> 3)) & 15


def dual_grid_cases():
    """{corner bits: (shape, rotation)} for all 16 patterns, built from four canonical ones.

    The 16 corner masks fall into exactly six classes under rotation — empty, corner (4), edge (4),
    diag (2), inv (4), full — and every class is already closed under reflection, which is the
    research finding that says mirroring buys nothing for transitions (TILES2_PROPOSAL §3.1)."""
    cases = {0: None, 15: ("full", 0)}
    for shape, b in (("corner", 1), ("edge", 3), ("diag", 5), ("inv", 11)):
        c = b
        for r in range(4):
            cases.setdefault(c, (shape, r))
            c = bits_rot(c)
    if len(cases) != 16:
        die(f"the dual-grid case table came out with {len(cases)} entries, not 16")
    return cases


DUAL_CASES = dual_grid_cases()
_MASK_CACHE = {}


def tile_mask(shape, style, variant, rot=0):
    """A baked mask, rotated clockwise `rot` quarter turns. Cached; the preview asks for these a lot."""
    key = (shape, style, variant, rot)
    if key not in _MASK_CACHE:
        m = bake_mask(shape, style, variant)
        for _ in range(rot):
            n = len(m)
            m = [bytearray(m[n - 1 - x][y] for x in range(n)) for y in range(n)]
        _MASK_CACHE[key] = m
    return _MASK_CACHE[key]


def write_masks(setname, styles=None):
    """<set>/masks.png + masks.json — every mask the set's terrains can need.

    An indexed PNG like everything else the game ships: index 1 (black) outside the terrain, index 2
    (white) inside, so `palette check` covers it and the engine reads one byte a pixel."""
    styles = sorted(set(styles or EDGE_STYLES))
    for s in styles:
        if s not in EDGE_STYLES:
            die(f"unknown edge_style '{s}'; use one of {', '.join(sorted(EDGE_STYLES))}")
    cols = len(MASK_SHAPES) * MASK_VARIANTS
    W, H = cols * MASK_PX, len(styles) * MASK_PX
    idx = [bytearray(W) for _ in range(H)]
    slots = []
    for r, style in enumerate(styles):
        for si, shape in enumerate(MASK_SHAPES):
            for v in range(MASK_VARIANTS):
                c = si * MASK_VARIANTS + v
                m = tile_mask(shape, style, v)
                for y in range(MASK_PX):
                    row, src = idx[r * MASK_PX + y], m[y]
                    for x in range(MASK_PX):
                        row[c * MASK_PX + x] = PAL_WHITE if src[x] else PAL_BLACK
                slots.append({"style": style, "shape": shape, "variant": v,
                              "x": c * MASK_PX, "y": r * MASK_PX})
    d = tileset_dir(setname)
    d.mkdir(parents=True, exist_ok=True)
    write_indexed_png(d / TSET_MASKS, W, H, idx, master_palette())
    (d / TSET_MASKS_JSON).write_text(json.dumps({
        "set": setname, "cell": MASK_PX, "width": W, "height": H,
        "styles": styles, "shapes": list(MASK_SHAPES), "variants": MASK_VARIANTS,
        "inside": PAL_WHITE, "outside": PAL_BLACK,
        "note": "1-bit masks, GENERATED — never art. A display tile's four corners are four world "
                "cells; bits 1 NW, 2 NE, 4 SE, 8 SW pick a shape and a clockwise rotation from "
                "'cases'. Rotation is a UV swizzle, so no mask is stored twice. "
                f"variant = hash(i, j, terrain) % {MASK_VARIANTS}. Every boundary crosses a tile "
                "edge at that edge's midpoint, which is what makes any two shapes meet cleanly.",
        "cases": {str(b): (list(c) if c else None) for b, c in sorted(DUAL_CASES.items())},
        "slots": slots,
    }, indent=2) + "\n")
    return d / TSET_MASKS, len(slots)


# ── the new tiles.md keys ──

def parse_terrain(ident, kv, e, where):
    """`- kind: terrain`: a swatch, a priority, an edge style, optionally a border band."""
    e["priority"] = int(kv.get("priority", kv.get("fringe", 0)))          # `fringe:` was the old name
    style = (kv.get("edge_style") or "ragged").strip().lower()
    if style == "shore":
        style = "bank"                                                   # the proposal's older word
    if style not in EDGE_STYLES:
        die(f"{where}: '## {ident}' has '- edge_style: {style}'; "
            f"use one of {', '.join(sorted(EDGE_STYLES))}")
    e["edge_style"] = style
    sw = (kv.get("swatch") or "3x3").strip()
    w, h = parse_wh(sw, f"{where} '{ident}' swatch")
    if not (SWATCH_MIN <= w <= SWATCH_MAX and SWATCH_MIN <= h <= SWATCH_MAX):
        die(f"{where}: '## {ident}' has '- swatch: {sw}'; each side is {SWATCH_MIN}..{SWATCH_MAX} tiles")
    e["swatch"] = (w, h)
    e["border"] = None
    if kv.get("border"):
        f = kv["border"].split()
        if len(f) != 2:
            die(f"{where}: '## {ident}' has '- border: {kv['border']}'; "
                f"write 'border: <terrain-or-colour> <width in tiles>'")
        try:
            e["border"] = (f[0], float(f[1]))
        except ValueError:
            die(f"{where}: '## {ident}' border width '{f[1]}' is not a number")
    e["drift"] = float(kv.get("drift", 0.0))
    e["cycle"] = (kv.get("cycle") or "").strip() or None
    return e


def parse_decal(ident, kv, e, where):
    """`- kind: decal`: what it may sit on, and how the hash scatter is shaped."""
    on = [s.strip() for s in (kv.get("on") or "").split(",") if s.strip()]
    if not on:
        die(f"{where}: '## {ident}' is a decal with no '- on: <terrain>[, ...]' line")
    e["on"] = on
    e["density"] = float(kv.get("density", 0.4))
    e["cluster"] = float(kv.get("cluster", 4))
    e["sizes"] = [float(v) for v in (kv.get("sizes") or "0.8 1.0 1.25").split()]
    e["flip"] = "h" in (kv.get("flip") or "").lower()
    e["edge_bias"] = {}
    for part in (kv.get("edge_bias") or "").split(","):
        f = part.split()
        if len(f) == 2:
            try:
                e["edge_bias"][f[0]] = float(f[1])
            except ValueError:
                die(f"{where}: '## {ident}' has edge_bias '{part.strip()}', not '<terrain> <factor>'")
        elif part.strip():
            die(f"{where}: '## {ident}' has edge_bias '{part.strip()}', not '<terrain> <factor>'")
    e["tiles"] = float(kv.get("size_tiles", 0.75))
    return e


def parse_passmask(text, ident, where):
    """`- pass: NESW` — the sides of a tile that may be walked THROUGH, as the open letters.

    A counter you talk across, a ledge you drop off one way, a wall you walk along: `- solid:` says
    whether the tile blocks at all, this says which of its four sides do."""
    text = (text or "").strip().upper()
    if not text or text in ("ALL", "NESW"):
        return "NESW"
    if text in ("NONE", "-"):
        return ""
    if not re.fullmatch(r"[NESW]+", text) or len(set(text)) != len(text):
        die(f"{where}: '## {ident}' has '- pass: {text}'; write the open sides as letters from "
            f"N, E, S, W (each at most once), or 'none'")
    return "".join(c for c in "NESW" if c in text)


def set_terrains(entries):
    """The set's terrains in draw order: priority low first, then by name. Ties never overlay."""
    t = [i for i, e in entries.items() if e.get("kind") == "terrain"]
    return sorted(t, key=lambda i: (entries[i]["priority"], i))


def set_decals(entries):
    return sorted(i for i, e in entries.items() if e.get("kind") == "decal")


def swatch_path(setname, terrain):
    return tileset_dir(setname) / TSET_SWATCHES / f"{terrain}.png"


# ── the swatch sheet: one slot per terrain, and nothing else on it ──

SWATCH_RULE = (
    "WHAT THESE ARE: ground textures, one per slot, each one a piece cut from the middle of an "
    "endless field of that material. Each slot is filled edge to edge, right up to its white border, "
    "and is SEAMLESS on all four edges: the right edge continues into the left and the bottom into "
    "the top, so a whole map tiled with copies of it shows no join and no landmark. "
    "Draw them FLAT. No border of any kind, no frame, no vignette, no darker or lighter side, no "
    "light falling across the slot, no shadow of anything outside it, no grass creeping in at an "
    "edge, no object, no path, no water's edge, no single feature a player could point at and "
    "remember. Calm and low in contrast: 3 or 4 flat tones of the material, close together, with "
    "the detail even across the whole slot. This texture will have flowers, stones and tufts "
    "scattered over it by the game, so it must be quiet enough to sit underneath them."
)


def build_swatch_sheet(setname, terrains, entries=None, style=None):
    """One package: every terrain's seamless swatch on one sheet."""
    entries = entries or tileset_entries(setname)
    style, warn = style or load_style(), []
    missing = [t for t in terrains if t not in entries]
    if missing:
        die(f"{tileset_doc(setname).name}: no entry for {', '.join(missing)}")
    sizes = [entries[t]["swatch"] for t in terrains]
    W, H, label, u, boxes = layout_cells(sizes, list(terrains))
    slots = []
    for n, (t, box) in enumerate(zip(terrains, boxes), 1):
        e = entries[t]
        slots.append({"n": n, "id": t, "kind": "swatch", "box": list(box),
                      "inner": list(inner_box(box)), "tiles": list(e["swatch"]),
                      "target": [e["swatch"][0] * ATLAS_CELL, e["swatch"][1] * ATLAS_CELL],
                      "edge_style": e["edge_style"],
                      "out": str(swatch_path(setname, t).relative_to(ROOT.parent))})
    name = f"swatch_{setname}"
    b = style["blocks"]
    attach = field_attachments(style, pkg_path(name, "template.png"), None, warn)
    L = [f"Create ONE image: the attached template with all {len(slots)} numbered slots filled in. "
         f"Canvas: {label}, exactly the same size and proportions as the template.", "",
         b["map_header"], "",
         "ATTACHED REFERENCE IMAGES, in the order I attached them:"]
    L += [f"Image {n}: {note}" for n, (_, note) in enumerate(attach, 1)]
    L += ["", TEMPLATE_RULES, "", SWATCH_RULE, "", "SLOTS:"]
    for s in slots:
        e = entries[s["id"]]
        L.append(f"Slot {s['n']} ({s['id']}), {s['tiles'][0]}x{s['tiles'][1]} tiles of ground, "
                 f"{s['inner'][2]}x{s['inner'][3]} px in the template: {e['desc']}")
    L += ["", f"RENDERING: {field_rendering(b)}", "",
          f"AVOID: {b['map_negative']}, a border or frame inside a slot, a lit or shaded side, a "
          f"vignette, a visible seam at a slot edge, one big feature in the middle of a slot, an "
          f"object, a path, a shoreline, a character, a shadow, noisy high-contrast texture, "
          f"drawing outside a slot, moving or covering a slot number, changing the size of the image"]
    after = field_after(pkg_path(name, "sheet.json"), [
        f"- [ ] All {len(slots)} slots filled to the border, every number still where it was",
        "- [ ] Each slot is FLAT: no lit side, no vignette, no border, no shadow",
        "- [ ] Calm and even: nothing in a slot the eye goes to first",
        "- [ ] Nothing that reads as an edge of the material — no grass rim, no shoreline, no path",
        "- [ ] Tiling test: the top edge of a slot would meet its bottom edge with no join", "",
        "If one slot fails, reply in the same chat: \"Redraw only slot 2 and keep every other slot "
        "and the whole template exactly as it is. <what was wrong>\"."],
        makes=f"the {len(slots)} seamless swatch(es) listed above")
    status = [f"- `{s['out']}` — " + ("**exists**" if (ROOT.parent / s["out"]).exists() else "missing")
              for s in slots]
    template, manifest, md = write_field_package(
        "swatch", name, label, W, H, TSET_GREY, slots, "\n".join(L), attach, after, status=status)
    data = json.loads(manifest.read_text())
    data.update({"set": setname, "cell": ATLAS_CELL})
    manifest.write_text(json.dumps(data, indent=2) + "\n")
    for w in warn:
        print(f"warning: {w}", file=sys.stderr)
    print(f"wrote {md.relative_to(ROOT.parent)}  ({len(slots)} terrain swatch(es), {label})")
    return slots


def cut_swatches(data, slots, rows, w, h, ch, sx, sy):
    """Cut, flatten the drawing's own lighting, make it wrap, palettise. The order matters.

    A swatch is the one piece of art in the game that is sampled in WORLD space, so its only job is
    to tile: the flattening and the seam healing are not polish, they are what make it usable at
    all. Both happen in full colour, and the palette step is last, as everywhere else."""
    setname = data["set"]
    out_dir = tileset_dir(setname) / TSET_SWATCHES
    out_dir.mkdir(parents=True, exist_ok=True)
    pal, pidx = master_palette(), PalIndex(master_palette())
    lines = []
    for s in slots:
        x, y, bw, bh = exact_inner(s, sx, sy, w, h)
        tw, th = s["target"]
        px = to_rgba(resample(crop(rows, ch, x, y, bw, bh), bw, bh, ch, tw, th), tw, th, ch)
        scrub_border(px, tw, th, 4, False, max(1, EXTRUDE_PX * 2))
        flat, dev = flatten_lighting(px, tw, th)
        make_seamless(px, tw, th)
        band_check(px, tw, th, s["id"])
        calm, contrast = swatch_is_calm(px, tw, th)
        hz, vt = seam_error(px, tw, th)
        out = swatch_path(setname, s["id"])
        ship_png(out, tw, th, 4, px, pal, pidx)
        note = (f"  wrap h{hz:.0f}/v{vt:.0f}, contrast {contrast:.3f}, "
                f"lighting {dev:.3f}" + (f" -> flattened {flat:.3f}" if flat else " (flat already)"))
        if not calm:
            LOUD.append(f"{s['id']}'s swatch is busy (contrast {contrast:.3f}): it will fight the "
                        f"decals and show its own repeat. Ask for that slot again, calmer — three "
                        f"or four flat tones, close together, nothing the eye goes to.")
            print(f"WARNING: {LOUD[-1]}", file=sys.stderr)
            note += "  <-- BUSY"
        lines.append(f"  slot {s['n']} {s['id']}: {bw}x{bh} -> {out.relative_to(ROOT.parent)}  "
                     f"{tw}x{th} ({s['tiles'][0]}x{s['tiles'][1]} tiles){note}")
    for l in lines:
        print(l)
    mp, n = write_masks(setname, [e["edge_style"] for e in tileset_entries(setname).values()
                                  if e.get("kind") == "terrain"])
    print(f"  -> {mp.relative_to(ROOT.parent)}  ({n} masks, generated — no art)")


FLATTEN_FLOOR = 0.010                 # low-frequency deviation under this is not a lit side
FLATTEN_FULL = 0.045                  # ...and at this much, take all of it out


def flatten_lighting(px, w, h, strength=0.9):
    """Subtract the drawing's own low-frequency lighting, so the swatch can tile. (how much moved)

    A swatch with a bright side or a grassy rim bands wherever it repeats, and the model draws one
    whatever the prompt says. This is a wrapped box blur's deviation from the image mean. The blur
    MUST wrap: a clamped one leaves a bright rim, and the seam roll then puts that rim through the
    middle of the tile, where it reads as a pale cross at every repeat.

    It is also **measured before it is applied**. Art that is already flat — which is what the owner's
    new style returns — has nothing to take out, and subtracting 90% of a deviation that is only
    noise adds noise. The strength ramps from zero at a deviation of 1% of range to full at 4.5%, so
    a flat return is left alone and a lit one is still fixed."""
    r = max(8, w // 6)
    sums = [[[0, 0, 0] for _ in range(w + 1)] for _ in range(h + 1)]
    for y in range(h):
        run = [0, 0, 0]
        for x in range(w):
            i = x * 4
            for k in range(3):
                run[k] += px[y][i + k]
                sums[y + 1][x + 1][k] = sums[y][x + 1][k] + run[k]
    tot = [sums[h][w][k] / (w * h) for k in range(3)]

    def box(x0, y0, x1, y1, k):                       # inclusive-exclusive, already clamped
        return sums[y1][x1][k] - sums[y0][x1][k] - sums[y1][x0][k] + sums[y0][x0][k]

    def wrapped(cx, cy, k):                           # sum over a wrapped window, as up to 4 boxes
        total, n = 0.0, 0
        for y0, y1 in _wrap_spans(cy - r, cy + r + 1, h):
            for x0, x1 in _wrap_spans(cx - r, cx + r + 1, w):
                total += box(x0, y0, x1, y1, k)
                n += (x1 - x0) * (y1 - y0)
        return total / max(1, n)

    # 1. how much low-frequency deviation is there at all? (sampled, on the green-ish mean)
    dev, n = 0.0, 0
    step = max(1, w // 48)
    for y in range(0, h, step):
        for x in range(0, w, step):
            dev += abs(sum(wrapped(x, y, k) - tot[k] for k in range(3)) / 3.0)
            n += 1
    dev /= max(1, n) * 255.0
    if dev <= FLATTEN_FLOOR:                          # already flat: touching it can only add noise
        return 0.0, dev
    k_ = min(1.0, (dev - FLATTEN_FLOOR) / (FLATTEN_FULL - FLATTEN_FLOOR))
    strength *= k_

    moved = 0.0
    for y in range(h):
        for x in range(w):
            i = x * 4
            for k in range(3):
                m = wrapped(x, y, k)
                v = px[y][i + k] - strength * (m - tot[k])
                moved += abs(strength * (m - tot[k]))
                px[y][i + k] = 0 if v < 0 else (255 if v > 255 else int(v))
    return moved / (w * h * 3 * 255), dev


def band_check(px, w, h, ident):
    """A finished swatch may hold NO pale or transparent band. Loud, and it fails the cut.

    The white cross that got through was a whole row and column band of saturated pixels, and every
    check we had looked at single pixels or at edges. This looks at the thing that was actually
    wrong: a row (or column) whose mean lightness is far from the swatch's own median, or which has
    gone transparent. A real texture's row means sit inside a few percent of each other."""
    def means(vertical):
        out = []
        for i in range(h if not vertical else w):
            s_, a_, n_ = 0, 0, 0
            for j in range(0, (w if not vertical else h), 3):
                y, x = (i, j) if not vertical else (j, i)
                o = x * 4
                s_ += px[y][o] + px[y][o + 1] + px[y][o + 2]
                a_ += px[y][o + 3]
                n_ += 1
            out.append((s_ / (3 * n_), a_ / n_))
        return out

    bad = []
    for vertical, what in ((False, "row"), (True, "column")):
        ms = means(vertical)
        vals = sorted(m for m, _ in ms)
        med = vals[len(vals) // 2]
        spread = max(1.0, (vals[int(len(vals) * 0.9)] - vals[int(len(vals) * 0.1)]))
        for i, (m, a) in enumerate(ms):
            if a < 250:
                bad.append(f"{what} {i} is transparent (mean alpha {a:.0f})")
            elif abs(m - med) > max(26.0, 6.0 * spread):
                bad.append(f"{what} {i} has mean {m:.0f} against the swatch's median {med:.0f}")
            if len(bad) > 4:
                break
    if bad:
        die(f"{ident}: the finished swatch has a band in it — {'; '.join(bad[:5])}"
            + (f" and {len(bad) - 5} more" if len(bad) > 5 else "")
            + ". A swatch is one material edge to edge, so a whole row or column that is pale, dark "
              "or transparent is a bug in the cut, not art. Nothing was written.")


def _wrap_spans(lo, hi, n):
    """[lo, hi) on a ring of n, as up to two half-open spans inside [0, n).

    The width is taken BEFORE lo is folded into the ring. Computing it afterwards — `hi = lo + (hi -
    lo)` with lo already reassigned — silently returns a backwards span like (90, 11), and a
    backwards span through a prefix-sum table is not a small error: it is a garbage window mean, and
    at strength 0.9 that saturates a band of pixels to white. That was the white cross through every
    swatch, and it only showed at the image border, which is exactly where the seam roll then put
    it."""
    width = hi - lo
    if width >= n:
        return [(0, n)]
    lo %= n
    hi = lo + width
    return [(lo, n), (0, hi - n)] if hi > n else [(lo, hi)]


def swatch_is_calm(px, w, h, limit=0.115):
    """(calm, contrast). Contrast is the mean |L - mean L| in Oklab, which is what the eye reads."""
    n, s, vals = 0, 0.0, []
    step = max(1, w // 160)
    for y in range(0, h, step):
        for x in range(0, w, step):
            i = x * 4
            L = _to_oklab(px[y][i], px[y][i + 1], px[y][i + 2])[0]
            vals.append(L)
            s += L
            n += 1
    m = s / max(1, n)
    c = sum(abs(v - m) for v in vals) / max(1, n)
    return c <= limit, c


# ── the decal sheet: ~20 cut-outs of nothing but the object ──

DECAL_RULE = (
    "WHAT THESE ARE: small loose objects that the game scatters over its ground by the hundred — "
    "a tuft of grass here, three pebbles there. Each slot holds ONE object, drawn against the flat "
    "magenta and NOTHING else. The magenta is empty space, not a backdrop: it comes right up to the "
    "object on every side, and the edge where they meet is hard, not soft, blurred or glowing. "
    "There is NO ground under the object — no patch of grass, no earth, no stone, no dirt, no "
    "shadow, no base, no plate, no circle of darker colour. An object that is drawn standing on "
    "something is unusable, because the game puts it on a hundred different grounds. "
    "Each object fills most of its slot, seen from straight above as the ground is, with the same "
    "light from the upper left across the whole sheet."
)


def build_decal_sheet(setname, ids, entries=None, style=None):
    """One package: a biome's decals, ~20 slots on magenta."""
    entries = entries or tileset_entries(setname)
    style, warn = style or load_style(), []
    missing = [i for i in ids if i not in entries]
    if missing:
        die(f"{tileset_doc(setname).name}: no entry for {', '.join(missing)}")
    sizes = [(1, 1)] * len(ids)
    W, H, label, u, boxes = layout_cells(sizes, list(ids))
    slots = []
    for n, (i, box) in enumerate(zip(ids, boxes), 1):
        e = entries[i]
        t = max(16, int(round(e["tiles"] * ATLAS_CELL)))
        slots.append({"n": n, "id": i, "kind": "decal", "box": list(box),
                      "inner": list(inner_box(box)), "target": [t, t],
                      "out": str((decal_path(setname, i)).relative_to(ROOT.parent))})
    name = f"decal_{setname}"
    b = style["blocks"]
    attach = field_attachments(style, pkg_path(name, "template.png"), None, warn)
    L = [f"Create ONE image: the attached template with all {len(slots)} numbered slots filled in. "
         f"Canvas: {label}, exactly the same size and proportions as the template.", "",
         b["map_header"], "",
         "ATTACHED REFERENCE IMAGES, in the order I attached them:"]
    L += [f"Image {n}: {note}" for n, (_, note) in enumerate(attach, 1)]
    L += ["", TEMPLATE_RULES, "", DECAL_RULE, "", "SLOTS:"]
    for s in slots:
        e = entries[s["id"]]
        L.append(f"Slot {s['n']} ({s['id']}), on {', '.join(e['on'])}, "
                 f"{s['inner'][2]}x{s['inner'][3]} px in the template: {e['desc']}")
    L += ["", f"RENDERING: {field_rendering(b)}", "",
          f"AVOID: {b['map_negative']}, ground or a patch of earth under an object, a shadow, a "
          f"base plate, a frame, a soft blurred or glowing edge against the magenta, two objects in "
          f"one slot, drawing outside a slot, moving or covering a slot number, changing the size "
          f"of the image"]
    after = field_after(pkg_path(name, "sheet.json"), [
        f"- [ ] All {len(slots)} slots filled, every border and number still exactly where it was",
        "- [ ] NOTHING under any object: no ground, no patch, no shadow, no base",
        "- [ ] The magenta comes right up to every object, with a hard edge",
        "- [ ] One object a slot, seen from straight above, lit from the upper left", "",
        "If one decal fails, reply in the same chat: \"Redraw only slot 7 and keep every other slot "
        "and the whole template exactly as it is. <what was wrong>\"."],
        makes=f"the {len(slots)} decal sprite(s) listed above")
    status = [f"- `{s['out']}` — " + ("**exists**" if (ROOT.parent / s["out"]).exists() else "missing")
              for s in slots]
    template, manifest, md = write_field_package(
        "decal", name, label, W, H, MAGENTA, slots, "\n".join(L), attach, after, status=status)
    data = json.loads(manifest.read_text())
    data.update({"set": setname})
    manifest.write_text(json.dumps(data, indent=2) + "\n")
    for w in warn:
        print(f"warning: {w}", file=sys.stderr)
    print(f"wrote {md.relative_to(ROOT.parent)}  ({len(slots)} decals, {label})")
    return slots


def decal_path(setname, ident):
    """The loose cut-out. It is kept for the preview and for eyeballing; the GAME reads the atlas."""
    return tileset_dir(setname) / "decals" / f"{ident}.png"


def decal_px(e):
    """A decal's size in atlas pixels — `size_tiles` of a cell, at the atlas cell size."""
    n = max(2, int(round(e["tiles"] * ATLAS_CELL)))
    return [min(n, ATLAS_CELL), min(n, ATLAS_CELL)]


def cut_decals(data, slots, rows, w, h, ch, sx, sy, fringe=0):
    """Key the magenta, trim to the object, scale, palettise, and pack into the ATLAS.

    A decal is drawn from `atlas.png` like every other sprite — one cell each, the art in the cell's
    top-left corner and its real pixel size in `atlas.json` — because the engine has one tile shader
    with one atlas bound and a second texture of loose files would be a second batch for the smallest
    thing on screen. The loose `decals/<id>.png` is still written: the preview uses it, and it is how
    you look at one."""
    setname = data["set"]
    entries = tileset_entries(setname)
    pal, pidx = master_palette(), PalIndex(master_palette())
    need = max((entries[s["id"]]["index"] // ATLAS_COLS) + 1 for s in slots if entries.get(s["id"]))
    atlas = load_atlas(setname, need)
    for s in slots:
        x, y, bw, bh = scaled_inner(s, sx, sy, w, h)
        px = key_magenta(crop(rows, ch, x, y, bw, bh), bw, bh, ch, fringe)
        scrub_border(px, bw, bh, 4, True, max(2, int(round(2 * sx))))
        bounds = opaque_bounds(px, bw, bh)
        if not bounds:
            print(f"  slot {s['n']} {s['id']}: nothing but background in the slot; not written",
                  file=sys.stderr)
            continue
        cx, cy, cw, chh = bounds
        px = crop(px, 4, cx, cy, cw, chh)
        k = s["target"][0] / max(bw, bh)
        nw, nh = max(2, round(cw * k)), max(2, round(chh * k))
        px = resample(px, cw, chh, 4, nw, nh)
        out = ROOT.parent / s["out"]
        ship_png(out, nw, nh, 4, px, pal, pidx)
        e = entries.get(s["id"])
        where = ""
        if e and e["index"] is not None:                  # into its own atlas cell, top-left
            cell = [bytearray(ATLAS_CELL * 4) for _ in range(ATLAS_CELL)]
            for y in range(min(nh, ATLAS_CELL)):
                cell[y][:min(nw, ATLAS_CELL) * 4] = px[y][:min(nw, ATLAS_CELL) * 4]
            place_in_atlas(atlas, e["index"], cell, 1, 1)
            where = f", atlas index {e['index']}"
        print(f"  slot {s['n']} {s['id']}: {bw}x{bh} -> trimmed {cw}x{chh} -> "
              f"{out.relative_to(ROOT.parent)}  {nw}x{nh}{where}"
              f"{check_alpha(px, nw, nh, out)}")
    ap = atlas_path(setname)
    ap.parent.mkdir(parents=True, exist_ok=True)
    ship_png(ap, ATLAS_COLS * ATLAS_CELL, len(atlas), 4, atlas, pal, pidx)
    write_atlas_json(setname, entries)
    print(f"  -> {ap.relative_to(ROOT.parent)}  {ATLAS_COLS * ATLAS_CELL}x{len(atlas)} "
          f"({len(slots)} decal(s) packed), atlas.json updated")


# ── cutting a returned tile sheet ──

def mode_region(rows, ch, x0, y0, bw, bh, ow, oh):
    """Downsample a crop to ow x oh by taking each output pixel's MODE colour in the source.

    This is the whole reason a ChatGPT sheet can be used as pixel art. The model draws something that
    looks blocky but is not: a block's edge lands a pixel or two off, and its interior carries a faint
    gradient. Averaging turns every hard tile edge into mush and two tiles stop meeting cleanly. The
    mode is the colour the artist meant that art pixel to be, and it is one of the colours that is
    actually there, so the palette never grows.

    The region is computed straight from the crop rather than by resizing to 4x first, because the
    sheet comes back at whatever size the app felt like (it caps at about 1.5 megapixels), so the
    ratio is never a whole number. Resizing first would put the 4x4 art blocks half a pixel out of
    step and the tile's last row would be its neighbour's."""
    out = []
    for y in range(oh):
        sy0, sy1 = y0 + y * bh // oh, y0 + max(y * bh // oh + 1, (y + 1) * bh // oh)
        dst = bytearray(ow * ch)
        for x in range(ow):
            sx0, sx1 = x0 + x * bw // ow, x0 + max(x * bw // ow + 1, (x + 1) * bw // ow)
            counts, best, bn = {}, None, 0
            for yy in range(sy0, sy1):
                row = rows[yy]
                for xx in range(sx0, sx1):
                    pxl = bytes(row[xx * ch:xx * ch + ch])
                    n = counts.get(pxl, 0) + 1
                    counts[pxl] = n
                    if n > bn:
                        best, bn = pxl, n
            if bn < 2:                               # a flat tie: take the middle of the region
                mid = rows[(sy0 + sy1) // 2]
                cx = (sx0 + sx1) // 2
                best = bytes(mid[cx * ch:cx * ch + ch])
            dst[x * ch:(x + 1) * ch] = best
        out.append(dst)
    return out


def to_rgba(rows, w, h, ch, alpha=255):
    if ch == 4:
        return [bytearray(r) for r in rows]
    out = []
    for r in rows:
        dst = bytearray(w * 4)
        for x in range(w):
            dst[x * 4:x * 4 + 3] = r[x * 3:x * 3 + 3]
            dst[x * 4 + 3] = alpha
        out.append(dst)
    return out


def harden_alpha(px, w, h):
    """A tile pixel is on or off: the engine alpha-tests at 0.5 and there is no blending in a tile."""
    for y in range(h):
        row = px[y]
        for x in range(w):
            if row[x * 4 + 3] < TSET_ALPHA_CUT:
                row[x * 4:x * 4 + 4] = b"\0\0\0\0"
            else:
                row[x * 4 + 3] = 255
    return px


def seam_error(px, w, h):
    """(horizontal, vertical) mean per-channel difference across a tile's wrap. 0 = perfectly seamless."""
    hz = sum(abs(px[y][(w - 1) * 4 + c] - px[y][c]) for y in range(h) for c in range(3)) / (h * 3)
    vt = sum(abs(px[h - 1][x * 4 + c] - px[0][x * 4 + c]) for x in range(w) for c in range(3)) / (w * 3)
    return hz, vt


def _hash01(x, y, salt):
    """A cheap hashed [0,1) per pixel — the dither threshold. Deterministic, so a recut is identical."""
    n = (x * 374761393 + y * 668265263 + salt * 2246822519) & 0xFFFFFFFF
    n = ((n ^ (n >> 13)) * 1274126177) & 0xFFFFFFFF
    return ((n ^ (n >> 16)) & 0xFFFF) / 65535.0


def _smoothstep(t):
    t = 0.0 if t < 0 else (1.0 if t > 1 else t)
    return t * t * (3 - 2 * t)


def border_gradient(px, w, h):
    """((across-x, interior-x), (across-y, interior-y)) mean gradients — the band / line test.

    The wrap error on its own cannot tell a healed tile from a blurred one: an averaged edge scores
    zero and looks like a crease. What separates them is the gradient *across* the tile border
    compared with the texture's own mean gradient on that axis. Much lower than 1 and the border is
    a soft band (averaged pixels); much higher than 1 and it is a line (a real discontinuity). A
    make-seamless by construction puts real adjacent pixels there, so it sits at or just under 1 —
    under, because the split is chosen where the art happens to be quietest."""
    def d(ax, ay, bx, by):
        return sum(abs(px[ay][ax * 4 + c] - px[by][bx * 4 + c]) for c in range(3)) / 3.0
    xs, ys = list(range(0, w - 1, 3)), list(range(0, h - 1, 3))
    cx = sum(d(0, y, w - 1, y) for y in range(h)) / h
    ix = sum(d(x, y, x + 1, y) for y in range(h) for x in xs) / (h * len(xs))
    cy = sum(d(x, 0, x, h - 1) for x in range(w)) / w
    iy = sum(d(x, y, x, y + 1) for x in range(w) for y in ys) / (w * len(ys))
    return (cx, ix), (cy, iy)


def make_seamless(px, w, h, feather=16, hold=3):
    """Make an opaque ground tile tile perfectly, *by construction*, keeping its detail crisp.

    The old `--heal-seams` cross-blended a band at each edge: it removed the wrap error and put a
    soft, blurred ribbon in its place. That ribbon is exactly the faint grid the phone showed on
    grass, dirt and gravel — every tile border was a line of averaged pixels, and averaged pixels
    read as a crease whatever the art either side of them is.

    Nothing is averaged here. The construction is an offset, one axis at a time, so each axis is
    exact and no pixel of the output is anything but one real pixel of the returned art:

    1. **Roll the tile** along the axis by `k`, wrapping. The output's first and last line were
       *adjacent* lines of the original, so the wrap on that axis is now continuous by construction.
       `k` is not h/2 but the **min-error split**: of the middle third of the possible splits, the
       one whose two lines are most alike, so the join is not merely legal but invisible. (At a flat
       h/2 the split can land on a mortar course or a furrow and read as a line even though it
       wraps.)
    2. The roll has moved the original's own discontinuity into the middle, as a line across the
       tile. **Cover it with a strip of the tile's own interior**, copied as whole lines of the
       *other* axis, so the (already exact) continuity of that other axis is carried along with it.
       The source offset is picked from a dozen candidates by least mean error against what it lands
       on, and the strip is narrow — 3 px held, 16 px of feather.
    3. The strip's own two boundaries are cut with a **hashed dither** against the feather ramp, not
       an alpha average: each pixel is taken whole from one source or the other, chosen by a hash of
       its position. Detail stays crisp and the join reads as texture rather than as an edge.
    4. Repeat on the other axis. A roll along x permutes columns only, so step 1's y-continuity
       survives it exactly, and a whole-column patch carries its own y-continuity with it.

    The result: the tile's border pixels *are* interior pixels, so the border is neither a line
    (nothing was left discontinuous) nor a band (nothing was blurred)."""
    R = hold + feather

    def alpha(dist):
        return _smoothstep((R - dist) / float(feather))

    for axis in (0, 1):                                  # 0 = roll in y, 1 = roll in x
        n = h if axis == 0 else w                        # the length along the rolled axis
        m = w if axis == 0 else h                        # the length of one line
        if n < 4 * R:
            continue

        # 1. the min-error split, over the middle third, so the old seam lands clear of both edges
        lo, hi = max(R + 4, n // 3), min(n - R - 4, 2 * n // 3)
        best_k, best_e = n // 2, None
        for kk in range(lo, hi + 1):
            if axis == 0:
                a, b = px[kk - 1], px[kk]
                e = sum(abs(a[t * 4 + c] - b[t * 4 + c]) for t in range(0, m, 3) for c in range(3))
            else:
                e = sum(abs(px[t][kk * 4 + c] - px[t][(kk - 1) * 4 + c])
                        for t in range(0, m, 3) for c in range(3))
            if best_e is None or e < best_e:
                best_k, best_e = kk, e
        k = best_k
        if axis == 0:
            rows = [bytearray(px[(y + k) % h]) for y in range(h)]
            line = lambda i: rows[i]
        else:
            c = k * 4
            rows = [bytearray(px[y][c:] + px[y][:c]) for y in range(h)]
            line = lambda i: bytes(bb for y in range(h) for bb in rows[y][i * 4:i * 4 + 4])

        # 2. the old seam now sits between n-k-1 and n-k; patch a strip of interior over it
        mid = n - k
        band = [i for i in range(n) if abs(i - mid) <= R and 0 <= i < n]
        cands = [o for o in range(R + 6, n - R - 5, 5)]  # never puts the seam back inside the band
        src_off, berr = cands[0] if cands else n // 2, None
        for o in cands:
            err = 0
            for i in band[::3]:
                a, b = line(i), line((i + o) % n)
                for t in range(0, m, 7):
                    err += abs(a[t * 4] - b[t * 4]) + abs(a[t * 4 + 1] - b[t * 4 + 1]) \
                         + abs(a[t * 4 + 2] - b[t * 4 + 2])
            if berr is None or err < berr:
                src_off, berr = o, err
        patch = {i: bytes(line((i + src_off) % n)) for i in band}
        # 3. hashed dither across the feather — a whole pixel from one side or the other, never a mix
        for i in band:
            a = alpha(abs(i - mid))
            if a <= 0:
                continue
            s = patch[i]
            for j in range(m):
                if _hash01(i, j, axis + 1) >= a:
                    continue
                if axis == 0:
                    rows[i][j * 4:j * 4 + 3] = s[j * 4:j * 4 + 3]
                else:
                    rows[j][i * 4:i * 4 + 3] = s[j * 4:j * 4 + 3]
        px[:] = rows
    return px


_SRGB_LIN = [((v / 255.0 / 12.92) if v / 255.0 <= 0.04045
              else (((v / 255.0 + 0.055) / 1.055) ** 2.4)) for v in range(256)]


def _to_oklab(r, g, b):
    R, G, B = _SRGB_LIN[r], _SRGB_LIN[g], _SRGB_LIN[b]
    l = (0.4122214708 * R + 0.5363325363 * G + 0.0514459929 * B) ** (1 / 3.0)
    m = (0.2119034982 * R + 0.6806995451 * G + 0.1073969566 * B) ** (1 / 3.0)
    s = (0.0883024619 * R + 0.2817188376 * G + 0.6299787005 * B) ** (1 / 3.0)
    return (0.2104542553 * l + 0.7936177850 * m - 0.0040720468 * s,
            1.9779984951 * l - 2.4285922050 * m + 0.4505937099 * s,
            0.0259040371 * l + 0.7827717662 * m - 0.8086757660 * s)


def _from_oklab(L, A, B_):
    l = (L + 0.3963377774 * A + 0.2158037573 * B_) ** 3
    m = (L - 0.1055613458 * A - 0.0638541728 * B_) ** 3
    s = (L - 0.0894841775 * A - 1.2914855480 * B_) ** 3
    out = []
    for lin in (+4.0767416621 * l - 3.3077115913 * m + 0.2309699292 * s,
                -1.2684380046 * l + 2.6097574011 * m - 0.3413193965 * s,
                -0.0041960863 * l - 0.7034186147 * m + 1.7076147010 * s):
        lin = 0.0 if lin < 0 else (1.0 if lin > 1 else lin)
        v = 12.92 * lin if lin <= 0.0031308 else 1.055 * lin ** (1 / 2.4) - 0.055
        out.append(max(0, min(255, int(round(v * 255)))))
    return out


def oklab_means(px, w, h):
    """(mean L, mean chroma) of an opaque tile, in Oklab."""
    n, sL, sC = 0, 0.0, 0.0
    for y in range(h):
        row = px[y]
        for x in range(w):
            L, A, B = _to_oklab(row[x * 4], row[x * 4 + 1], row[x * 4 + 2])
            sL += L
            sC += (A * A + B * B) ** 0.5
            n += 1
    return (sL / n, sC / n) if n else (0.0, 0.0)


def match_tone(px, w, h, want_L, want_C):
    """Shift a variant's mean lightness and scale its mean chroma to the family's first member.

    An offset on L and a gain on chroma: every pixel keeps its own distance from the mean, so the
    tile's detail, its local contrast and its hue relationships are all left exactly as drawn. Only
    the tone the eye reads from three tiles away moves."""
    have_L, have_C = oklab_means(px, w, h)
    dL = want_L - have_L
    gC = (want_C / have_C) if have_C > 1e-6 else 1.0
    gC = max(0.5, min(2.0, gC))
    if abs(dL) < 0.002 and abs(gC - 1) < 0.02:
        return have_L, have_C, have_L, have_C
    for y in range(h):
        row = px[y]
        for x in range(w):
            L, A, B = _to_oklab(row[x * 4], row[x * 4 + 1], row[x * 4 + 2])
            row[x * 4:x * 4 + 3] = bytes(_from_oklab(max(0.0, L + dL), A * gC, B * gC))
    now_L, now_C = oklab_means(px, w, h)
    return have_L, have_C, now_L, now_C


def normalise_families(cut):
    """Pull every ground variant on this sheet onto the tone of the first tile of its family."""
    by_id = {s["id"]: (px, w, h) for px, w, h, s in cut if s["opaque"] and tuple(s["cells"]) == (1, 1)}
    notes = []
    for fam in GROUND_FAMILIES:
        head = next((n for n in fam if n in by_id), None)
        if head is None:
            continue
        want = oklab_means(*by_id[head])
        notes.append(f"  tone {head}: L {want[0]:.3f} chroma {want[1]:.3f} (the family's reference)")
        for name in fam:
            if name == head or name not in by_id:
                continue
            wasL, wasC, nowL, nowC = match_tone(*by_id[name], want[0], want[1])
            notes.append(f"  tone {name}: L {wasL:.3f} -> {nowL:.3f}, "
                         f"chroma {wasC:.3f} -> {nowC:.3f}")
    return notes


def median_cut(colours, n):
    """A palette of at most n colours from [((r,g,b), count)]. Plain median cut, stdlib only."""
    spread = lambda box: max(max(c[0][i] for c in box) - min(c[0][i] for c in box) for i in range(3))
    boxes = [list(colours)]
    while len(boxes) < n:
        can = [b for b in boxes if len(b) > 1 and spread(b) > 0]
        if not can:
            break
        big = max(can, key=lambda b: spread(b) * sum(c[1] for c in b))
        i = max(range(3), key=lambda c: max(x[0][c] for x in big) - min(x[0][c] for x in big))
        big.sort(key=lambda x: x[0][i])
        half, run, target = 1, 0, sum(c[1] for c in big) / 2
        for k, c in enumerate(big):
            run += c[1]
            if run >= target:
                half = max(1, min(len(big) - 1, k))
                break
        boxes.remove(big)
        boxes += [big[:half], big[half:]]
    out = []
    for b in boxes:
        tot = sum(c[1] for c in b) or 1
        out.append(tuple(sum(c[0][i] * c[1] for c in b) // tot for i in range(3)))
    return out


def apply_palette(tiles, n):
    """Quantise every cut tile on one sheet to a shared palette of n colours."""
    counts = {}
    for px, w, h, _ in tiles:
        for y in range(h):
            for x in range(w):
                if px[y][x * 4 + 3]:
                    c = tuple(px[y][x * 4:x * 4 + 3])
                    counts[c] = counts.get(c, 0) + 1
    if len(counts) <= n:
        return len(counts), len(counts)
    pal = median_cut([(c, k) for c, k in counts.items()], n)
    cache = {}
    for px, w, h, _ in tiles:
        for y in range(h):
            for x in range(w):
                if not px[y][x * 4 + 3]:
                    continue
                c = tuple(px[y][x * 4:x * 4 + 3])
                if c not in cache:
                    cache[c] = min(pal, key=lambda p: sum((a - b) ** 2 for a, b in zip(p, c)))
                px[y][x * 4:x * 4 + 3] = bytes(cache[c])
    return len(counts), len(pal)


def load_atlas(setname, need_rows, cell=None):
    """The atlas as RGBA rows, grown to need_rows cells tall. Existing cells are never touched."""
    cell = cell or ATLAS_CELL
    p = atlas_path(setname) if cell == ATLAS_CELL else atlas_path(setname).with_name("atlas32.png")
    W = ATLAS_COLS * cell
    rows = []
    if p.exists():
        w, h, ch, px = read_png(p)
        if w != W:
            die(f"{p.relative_to(ROOT.parent)} is {w}px wide, expected {W} "
                f"({ATLAS_COLS} columns of {cell}). Delete it and recut every sheet.")
        rows = to_rgba(px, w, h, ch)
    while len(rows) < need_rows * cell:
        rows.append(bytearray(W * 4))
    return rows


def place_in_atlas(atlas, index, px, cw, chh, cell=None):
    """Write one entry's cells into its own place in the atlas, and nobody else's."""
    cell = cell or ATLAS_CELL
    for r in range(chh):
        for c in range(cw):
            n = index + ATLAS_COLS * r + c
            ax, ay = (n % ATLAS_COLS) * cell, (n // ATLAS_COLS) * cell
            for y in range(cell):
                sy = r * cell + y
                atlas[ay + y][ax * 4:(ax + cell) * 4] = \
                    px[sy][c * cell * 4:(c + 1) * cell * 4]


def write_atlas_json(setname, entries):
    d = tileset_dir(setname)
    d.mkdir(parents=True, exist_ok=True)
    top = max((e["index"] or 0) + ATLAS_COLS * (e["cells"][1] - 1) + e["cells"][0] - 1
              for e in entries.values())
    (d / ATLAS_JSON).write_text(json.dumps({
        "set": setname, "tile": TSET_TILE, "cell": ATLAS_CELL, "cols": ATLAS_COLS,
        "rows": top // ATLAS_COLS + 1,
        "note": "generated by story_prompt.py tileset; tiles.md is the source of truth",
        "tiles": {i: {"index": e["index"], "w": e["w"], "h": e["h"], "frames": e["frames"],
                      "layer": e["layer"], "solid": e["solid"], "sheet": e["sheet"],
                      "kind": e["kind"],
                      **({"pass": e["pass"]} if e.get("pass", "NESW") != "NESW" else {}),
                      **({"tag": e["tag"]} if e.get("tag") else {}),
                      **({"px": decal_px(e), "on": e["on"], "density": e["density"],
                          "cluster": e["cluster"], "edge_bias": e["edge_bias"],
                          "sizes": e["sizes"], "flip": e["flip"]}
                         if e["kind"] == "decal" else {})}
                  for i, e in entries.items() if e["index"] is not None},
        "terrains": {i: {"priority": e["priority"], "edge_style": e["edge_style"],
                         "swatch": list(e["swatch"]), "border": list(e["border"]) if e["border"] else None,
                         "drift": e["drift"], "cycle": e["cycle"], "solid": e["solid"]}
                     for i, e in entries.items() if e["kind"] == "terrain"},
    }, indent=2) + "\n")


def exact_inner(slot, sx, sy, w, h):
    """A tile slot's art area in the returned image, to the pixel — no padding shaved off.

    The other template kinds shave CUT_PAD off each side to lose a soft border. A tile cannot afford
    that: the art area is exactly TSET_SCALE times the tile, and shaving two pixels would put every
    4x4 art block half a pixel out of step with the grid the cut reads it back on. The border is
    drawn outside the art area instead, so there is nothing to shave."""
    if slot.get("_art"):                                 # localised against the border really drawn
        x0, y0, x1, y1 = slot["_art"]
        return x0, y0, x1 - x0, y1 - y0
    ix, iy, iw, ih = slot["inner"]
    x0, y0 = int(round(ix * sx)), int(round(iy * sy))
    x1, y1 = int(round((ix + iw) * sx)), int(round((iy + ih) * sy))
    if x0 < 0 or y0 < 0 or x1 > w or y1 > h or x1 - x0 < 8 or y1 - y0 < 8:
        die(f"slot {slot['n']} falls outside the image; is this the right file for this package?")
    return x0, y0, x1 - x0, y1 - y0


def fill_ground_magenta(px, w, h):
    """An opaque tile on a magenta sheet: any background the artist left showing is a hole. Fill it.

    A sheet has one background colour, and the ground tiles now share theirs with the fringes, so a
    corner left unpainted would go into the atlas as a magenta square. Nothing is keyed on a ground
    tile — instead every pixel still carrying a magenta cast takes the colour of the nearest painted
    pixel, growing in from the clean part. Returns (repaired, repaired away from the tile's edge):
    a hole in the outermost ring is what a non-integer return does to a slot's border and is repaired
    without a word, while one further in is the artist leaving the background showing."""
    # Only the background colour itself counts, and then a blend that is more than half background:
    # a ground tile may legitimately be any colour it likes, purples included, and a cast threshold
    # as loose as the keying's would call a heather-coloured moor a hole.
    def hole(x, y):
        r, g, b = px[y][x * 4], px[y][x * 4 + 1], px[y][x * 4 + 2]
        return ((r - 255) ** 2 + g * g + (b - 255) ** 2) ** 0.5 <= KEY_HARD

    def blend(x, y):
        return magenta_cast(px[y][x * 4], px[y][x * 4 + 1], px[y][x * 4 + 2]) >= 128

    dirty = [[hole(x, y) for x in range(w)] for y in range(h)]
    for y in range(h):                                   # the soft edge of a hole, but only there
        for x in range(w):
            if not dirty[y][x] and blend(x, y) and any(
                    0 <= nx < w and 0 <= ny < h and dirty[ny][nx]
                    for nx, ny in ((x + 1, y), (x - 1, y), (x, y + 1), (x, y - 1))):
                dirty[y][x] = True
    n = sum(r.count(True) for r in dirty)
    inner = sum(1 for y in range(1, h - 1) for x in range(1, w - 1) if dirty[y][x])
    if not n or n == w * h:
        return n, inner
    front = [(x, y) for y in range(h) for x in range(w) if not dirty[y][x]]
    while front:
        nxt = []
        for x, y in front:
            for nx, ny in ((x + 1, y), (x - 1, y), (x, y + 1), (x, y - 1)):
                if 0 <= nx < w and 0 <= ny < h and dirty[ny][nx]:
                    dirty[ny][nx] = False
                    px[ny][nx * 4:nx * 4 + 3] = px[y][x * 4:x * 4 + 3]
                    nxt.append((nx, ny))
        front = nxt
    return n, inner


def snap32(px, w, h):
    """The old 32-px tile: the mode colour of each 4x4 art block of the full-resolution one.

    Kept behind --snap32 so the pixel-art reading of a sheet can still be produced and compared; the
    atlas the game ships is the full-resolution one."""
    k = ATLAS_CELL // TSET_TILE
    ow, oh = w // k, h // k
    out = []
    for y in range(oh):
        dst = bytearray(ow * 4)
        for x in range(ow):
            counts, best, bn = {}, None, 0
            for yy in range(y * k, y * k + k):
                for xx in range(x * k, x * k + k):
                    q = bytes(px[yy][xx * 4:xx * 4 + 4])
                    n = counts.get(q, 0) + 1
                    counts[q] = n
                    if n > bn:
                        best, bn = q, n
            dst[x * 4:x * 4 + 4] = best
        out.append(dst)
    return out


def cut_tileset(data, slots, rows, w, h, ch, sx, sy, palette=0, heal=True, snap=False):
    """A returned tile sheet: localise, resample onto the atlas grid, key, pack into the atlas.

    Nothing is thrown away here. The sheet is drawn at TSET_SCALE and the atlas keeps it at that
    scale, so a slot's art is resampled to exactly its cells x ATLAS_CELL and no further: the owner
    compared the returns with the old mode-snapped 32-px cut and preferred the art as drawn. The
    keying therefore leaves alpha SOFT (the engine filters linearly), un-mattes the edge colour so no
    pink shows through it, and the silhouette is extruded outward so linear sampling has clean colour
    to pull from.

    A sheet carries one background colour but its entries are not all of one kind: a terrain sheet
    holds opaque ground tiles AND keyed fringes over the same magenta. So the keying is decided per
    ENTRY, from its own `opaque` flag, never from the sheet."""
    setname = data["set"]
    entries = tileset_entries(setname)
    keyed_bg = list(data.get("background") or MAGENTA) == list(MAGENTA)
    ring = max(1, EXTRUDE_PX * 2)                        # the border residue at ATLAS_CELL, in pixels
    cut, lines, scrubbed = [], [], 0
    for s in slots:
        x, y, bw, bh = exact_inner(s, sx, sy, w, h)
        cw, chh = s["cells"]
        tw, th = cw * ATLAS_CELL, chh * ATLAS_CELL
        art = resample(crop(rows, ch, x, y, bw, bh), bw, bh, ch, tw, th)
        if s["opaque"]:
            px = to_rgba(art, tw, th, ch)
            for r in px:
                for i in range(3, len(r), 4):
                    r[i] = 255
            scrubbed += scrub_border(px, tw, th, 4, False, ring)
            if keyed_bg:                               # ground on a magenta sheet: never keyed
                holes, inner = fill_ground_magenta(px, tw, th)
                if inner > tw:                         # a ring's worth is the border, not a hole
                    LOUD.append(f"{s['id']} is an opaque ground tile but came back with {holes} "
                                f"background-coloured pixel(s) in it ({100 * holes // (tw * th)}%); "
                                f"they were filled from the nearest painted pixel. Ask for that slot "
                                f"again, painted edge to edge, if it shows.")
                    print(f"WARNING: {LOUD[-1]}", file=sys.stderr)
        else:
            px = key_magenta(art, tw, th, ch)
            scrubbed += scrub_border(px, tw, th, 4, True, ring)
            # No edge extrusion any more (D19): alpha is hard and the atlas holds indices, so there
            # is no colour under a transparent texel to bleed into. What the bleed was for — linear
            # filtering pulling background through the silhouette — the tile shader now does by
            # blending the four neighbours' colormap COLOURS with premultiplied alpha, index 0 being
            # zero. The cells still have to come out clean, which is what the scrub above is for.
        cut.append((px, tw, th, s))
    if scrubbed:
        print(f"  border scrub: {scrubbed} white edge-run pixel(s) removed")
    for n in normalise_families(cut):
        print(n)
    if palette:
        was, now = apply_palette(cut, palette)
        print(f"  palette: {was} colours -> {now}")

    atlas_rows = max((s["index"] + ATLAS_COLS * (s["cells"][1] - 1)) // ATLAS_COLS + 1 for s in slots)
    atlas = load_atlas(setname, atlas_rows)
    small = load_atlas(setname, atlas_rows, ATLAS_CELL) if snap else None
    for px, tw, th, s in cut:
        cw, chh = s["cells"]
        note = ""
        if s["opaque"] and cw == chh == 1:
            if heal:
                make_seamless(px, tw, th)
            hz, vt = seam_error(px, tw, th)
            (cx, ix), (cy, iy) = border_gradient(px, tw, th)
            rx, ry = (cx / ix if ix else 0.0), (cy / iy if iy else 0.0)
            note = (f"  wrap h{hz:.0f}/v{vt:.0f}  border/interior gradient "
                    f"h{rx:.2f} v{ry:.2f}")
            if max(rx, ry) > 1.15:                       # a real discontinuity is left on some edge
                LOUD.append(f"{s['id']} does not tile cleanly: the gradient across its border is "
                            f"{max(rx, ry):.2f}x its own interior gradient — a line. Recut without "
                            f"--no-heal, or ask for that slot again.")
                print(f"WARNING: {LOUD[-1]}", file=sys.stderr)
                note += "  <-- LINE"
            elif min(rx, ry) < 0.08:                     # nothing real is that flat: it was averaged
                LOUD.append(f"{s['id']}'s border is far flatter than its interior "
                            f"(h{rx:.2f} v{ry:.2f}) — a soft band, not a join.")
                print(f"WARNING: {LOUD[-1]}", file=sys.stderr)
                note += "  <-- BAND"
        elif not s["opaque"]:
            clear = sum(1 for r in px for i in range(3, len(r), 4) if not r[i])
            note = f"  ({100 * clear // (tw * th)}% transparent)"
            if not clear:
                LOUD.append(f"{s['id']} came out with no transparent pixel anywhere; the magenta was "
                            f"painted over, so the game will draw a solid rectangle.")
                print(f"WARNING: {LOUD[-1]}", file=sys.stderr)
        place_in_atlas(atlas, s["index"], px, cw, chh)
        if small is not None:
            place_in_atlas(small, s["index"], snap32(px, tw, th), cw, chh, TSET_TILE)
        lines.append(f"  slot {s['n']} {s['id']}: index {s['index']}, {cw}x{chh} cell(s) "
                     f"at {tw}x{th}{note}")

    ap = atlas_path(setname)
    ap.parent.mkdir(parents=True, exist_ok=True)
    pal, pidx = master_palette(), PalIndex(master_palette())
    ship_png(ap, ATLAS_COLS * ATLAS_CELL, len(atlas), 4, atlas, pal, pidx)
    strip = write_cut_sheet(setname, data["sheet"], cut)
    for l in lines:
        print(l)
    print(f"  -> {ap.relative_to(ROOT.parent)}  {ATLAS_COLS * ATLAS_CELL}x{len(atlas)} "
          f"({len(slots)} entries placed), {strip.relative_to(ROOT.parent)}")
    if small is not None:
        sp = ap.with_name("atlas32.png")
        ship_png(sp, ATLAS_COLS * TSET_TILE, len(small), 4, small, pal, pidx)
        print(f"  -> {sp.relative_to(ROOT.parent)}  {ATLAS_COLS * TSET_TILE}x{len(small)} (--snap32)")
    write_atlas_json(setname, entries)


def write_cut_sheet(setname, sheet, cut):
    """Every tile this sheet produced, laid out at 1:1 — the receipt the package's status reads."""
    MAXW = ATLAS_COLS * ATLAS_CELL
    lanes, x, lane = [[]], 0, 0
    for px, tw, th, s in cut:
        if x and x + tw > MAXW:
            lanes.append([])
            lane, x = lane + 1, 0
        lanes[lane].append((px, x, tw, th))
        x += tw
    W = max((t[1] + t[2] for L in lanes for t in L), default=ATLAS_CELL)
    out = []
    for L in lanes:
        if not L:
            continue
        lh = max(t[3] for t in L)
        band = [bytearray(W * 4) for _ in range(lh)]
        for px, lx, tw, th in L:
            for r in range(th):
                band[r][lx * 4:(lx + tw) * 4] = px[r]
        out += band
    p = cut_sheet_path(setname, sheet)
    p.parent.mkdir(parents=True, exist_ok=True)
    ship_png(p, W, len(out), 4, out)                     # the receipt ships too, so it is checked too
    return p


# ── tmaps ──

TRIGGER_KINDS = {"exit": 4, "door": 4, "message": 1, "npc": 3, "zone": 1, "trap": 1, "light": 2}
# 'light <radius> <level> [flicker]' is a point light as a trigger box; the same thing written as a
# 'lamp: x y radius level [flicker]' meta line is what the maps actually use. Both are checked, and
# both are looked up in the colormap's point-light table (`lamp`), not in the map's ambient table.


def tmap_path(mp):
    return TMAPS / f"{slug(mp)}.tmap"


def strip_comment(line):
    return re.sub(r"\s{2,}#.*$", "", line).rstrip()


def parse_tmap(mp):
    path = tmap_path(mp)
    if not path.exists():
        die(f"{path.relative_to(ROOT.parent)} not found")
    secs = h2_sections(path.read_text())
    for need in ("meta", "legend", "ground"):
        if need not in secs:
            die(f"{path.name}: no '## {need}' section (see TILES.md, 'Maps')")
    meta, metas = {}, {}
    for l in secs["meta"].splitlines():
        l = strip_comment(l)
        if ":" in l:
            k, v = l.split(":", 1)
            k, v = k.strip().lower(), v.strip()
            meta[k] = v
            metas.setdefault(k, []).append(v)       # 'lamp:' is written once per point light
    legend = {}
    for l in secs["legend"].splitlines():
        l = strip_comment(l)
        if not l.strip():
            continue
        ch, rest = l[0], l[1:].strip().split()
        if not rest:
            die(f"{path.name}: legend line '{l}' names no tile")
        legend[ch] = rest[0]
    grid = lambda key: [l.rstrip("\n") for l in secs.get(key, "").splitlines()
                        if l.strip() and not l.lstrip().startswith("#")]
    lines_of = lambda key: [strip_comment(l).strip().split()
                            for l in secs.get(key, "").splitlines()
                            if strip_comment(l).strip() and not l.lstrip().startswith("#")]
    trig = lines_of("triggers")
    return {"path": path, "meta": meta, "metas": metas, "legend": legend, "ground": grid("ground"),
            "objects": grid("objects"), "triggers": trig,
            # TILES2 (D20). Both are optional and both are LINE LISTS, never grids: a grid of the
            # same width as the map is a thing an author has to keep aligned, and neither of these
            # is dense enough to be worth that. `## decals` places one by hand where the scatter
            # cannot be trusted to (the flowers on a grave); `## flips` names the top-left cell of
            # a stamp placement that is drawn mirrored. A map with neither reads exactly as before.
            "decals": lines_of("decals"), "flips": lines_of("flips")}


def tmap_todo():
    """Map names that are allowed to be missing: '## todo' in story/field/tmaps/README.md."""
    p = TMAPS / "README.md"
    if not p.exists():
        return set()
    body = h2_sections(p.read_text()).get("todo", "")
    return {slug(w) for l in body.splitlines() for w in re.findall(r"`([a-z0-9_]+)`", l)}


def tmap_solid_grid(m, entries, err):
    """(w, h, solid[y][x], stamp tops) after laying the ground and every stamp down."""
    w, h = (int(x) for x in (m["meta"].get("size") or "0 0").split()[:2]) if m["meta"].get("size") \
        else (0, 0)
    solid = [[False] * w for _ in range(h)]
    tops = []
    for y, line in enumerate(m["ground"][:h]):
        for x, c in enumerate(line[:w]):
            name = m["legend"].get(c)
            e = entries.get(name)
            if e and e["solid"].lower() in ("yes", "true", "1"):
                solid[y][x] = True
    claimed = {}
    for y, line in enumerate(m["objects"][:h]):
        for x, c in enumerate(line[:w]):
            if c in (".", "+", " "):
                continue
            name = m["legend"].get(c)
            e = entries.get(name)
            if not e:
                continue
            cw, chh = e["foot"]
            if x + cw > w or y + chh > h:
                err.append(f"stamp '{name}' at {x},{y} is {cw}x{chh} and runs off the map")
                continue
            rows = re.split(r"[\s/]+", e["solid"].strip()) if set(e["solid"]) <= set("#./ ") \
                and "#" in e["solid"] else []
            hit, notplus = [], []                # one message a stamp, not one a cell
            for r in range(chh):
                for cc in range(cw):
                    key = (x + cc, y + r)
                    if key in claimed:
                        hit.append(claimed[key])
                    claimed[key] = name
                    if (r or cc) and m["objects"][y + r][x + cc] != "+":
                        notplus.append(f"{x + cc},{y + r}")
                    blocked = e["solid"].lower() in ("yes", "true", "1")
                    if rows and r < len(rows) and cc < len(rows[r]):
                        blocked = rows[r][cc] == "#"
                    if blocked:
                        solid[y + r][x + cc] = True
            if hit:
                err.append(f"stamp '{name}' at {x},{y} ({cw}x{chh}) overlaps {', '.join(sorted(set(hit)))}")
            if notplus:
                err.append(f"stamp '{name}' at {x},{y} is {cw}x{chh}, so cell(s) "
                           f"{', '.join(notplus[:6])}{' ...' if len(notplus) > 6 else ''} must be '+' "
                           f"in '## objects'")
            tops.append((x, y, name, e))
    orphan = [f"{x},{y}" for y, line in enumerate(m["objects"][:h])
              for x, c in enumerate(line[:w]) if c == "+" and (x, y) not in claimed]
    if orphan:
        err.append(f"{len(orphan)} cell(s) marked '+' that no stamp's footprint covers: "
                   f"{', '.join(orphan[:8])}{' ...' if len(orphan) > 8 else ''}")
    return w, h, solid, tops


def colormap_table_names():
    """The tables story/palette/colormap.json holds, so a map's `light:` can be checked against them."""
    if not COLORMAP_JSON.exists():
        return []
    try:
        return [t["table"] for t in json.loads(COLORMAP_JSON.read_text()).get("tables", [])]
    except (ValueError, KeyError):
        return []


def check_map_light(m, name, w, h):
    """`light: <table> <level>` and every `lamp: x y radius level [flicker]` in a map's '## meta'.

    The engine (src/TILEFIELD_NOTES.md, "Light, as implemented") reads these; the tool used to ignore
    any meta key it did not know, so a table name with a typo or a lamp standing outside the map went
    to the phone and showed up as a black screen or nothing at all. Light is a map's *ambient plus its
    point lights*, both looked up in `story/palette/colormap.png`, so this is the one place that can
    tell the two files are still talking about the same tables."""
    err, tables = [], colormap_table_names()
    lit = (m["meta"].get("light") or "").split()
    if lit:
        if lit[0] not in tables and tables:
            err.append(f"{name}: 'light: {' '.join(lit)}' names table '{lit[0]}', which "
                       f"{COLORMAP_JSON.relative_to(ROOT.parent)} does not hold "
                       f"({', '.join(tables)}). Rebuild it, or fix the line.")
        try:
            lv = float(lit[1]) if len(lit) > 1 else 1.0
            if not 0.0 <= lv <= 1.0:
                err.append(f"{name}: 'light:' level {lv} is outside 0..1 (1.0 is full light)")
        except ValueError:
            err.append(f"{name}: 'light: {' '.join(lit)}' — the level is not a number")
    for raw in m.get("metas", {}).get("lamp", []):
        f = raw.split()
        if len(f) < 4:
            err.append(f"{name}: 'lamp: {raw}' is not 'x y radius level [flicker]'")
            continue
        try:
            x, y, r, lv = (float(v) for v in f[:4])
        except ValueError:
            err.append(f"{name}: 'lamp: {raw}' has a non-numeric field")
            continue
        if not (0 <= x < w and 0 <= y < h):
            err.append(f"{name}: lamp at {f[0]},{f[1]} is outside the {w}x{h} map")
        if r <= 0:
            err.append(f"{name}: lamp at {f[0]},{f[1]} has radius {r}; it would light nothing")
        if not 0.0 <= lv <= 1.0:
            err.append(f"{name}: lamp at {f[0]},{f[1]} has level {lv}, outside 0..1")
        if len(f) > 4 and f[4] != "flicker":
            err.append(f"{name}: 'lamp: {raw}' — the only word after the level is 'flicker'")
    return err


def check_map_decals(m, name, w, h, entries):
    """`## decals` (x y id [flip]) and `## flips` (x y) — the two optional D20 sections."""
    err = []
    for f in m.get("decals", []):
        if len(f) < 3:
            err.append(f"{name}: '## decals' line '{' '.join(f)}' is not 'x y <id> [flip]'")
            continue
        try:
            x, y = int(f[0]), int(f[1])
        except ValueError:
            err.append(f"{name}: '## decals' line '{' '.join(f)}' has a non-numeric cell")
            continue
        if not (0 <= x < w and 0 <= y < h):
            err.append(f"{name}: decal '{f[2]}' at {x},{y} is outside the {w}x{h} map")
        e = entries.get(f[2])
        if not e:
            err.append(f"{name}: '## decals' names '{f[2]}', which has no entry in "
                       f"{tileset_doc(slug(m['meta'].get('tileset', ''))).name}")
        elif e["kind"] != "decal":
            err.append(f"{name}: '## decals' names '{f[2]}', which is a {e['kind']}, not a decal")
        if len(f) > 3 and f[3] != "flip":
            err.append(f"{name}: '## decals' line '{' '.join(f)}' — the only word after the id is 'flip'")
    objs = m.get("objects") or []
    for f in m.get("flips", []):
        if len(f) != 2:
            err.append(f"{name}: '## flips' line '{' '.join(f)}' is not 'x y' (a stamp's top-left cell)")
            continue
        try:
            x, y = int(f[0]), int(f[1])
        except ValueError:
            err.append(f"{name}: '## flips' line '{' '.join(f)}' has a non-numeric cell")
            continue
        if not (0 <= x < w and 0 <= y < h) or y >= len(objs) or x >= len(objs[y]):
            err.append(f"{name}: '## flips' names {x},{y}, which is outside the {w}x{h} map")
            continue
        ch = objs[y][x]
        ident = m["legend"].get(ch)
        if ch in (".", "+", " ") or not ident:
            err.append(f"{name}: '## flips' names {x},{y}, where '## objects' has no stamp")
            continue
        e = entries.get(ident)
        if e and not e.get("flip"):
            err.append(f"{name}: '## flips' mirrors '{ident}' at {x},{y}, and that entry has no "
                       f"'- flip: h' line. A building must never be mirrored: every roof and wall "
                       f"face in the set is lit from the upper left, so a mirrored one is lit from "
                       f"the wrong side. Mirror nature, not architecture.")
    return err


def stamp_fill_warnings(m, name, entries, setname):
    """Warn when a solid stamp's art leaves too much of a solid tile see-through (owner's rule).

    A solid tile a player can see grass through looks walkable and is not, which is the single most
    annoying thing a tile map can do. Only checked where the art exists: a placeholder is exempt."""
    warn, seen = [], set()
    objs = m.get("objects") or []
    atlas = atlas_path(setname)
    if not atlas.exists():
        return warn
    try:
        aw, ah, ach, arows = read_png(atlas)
    except SystemExit:
        return warn
    cell = ATLAS_CELL
    for y, row in enumerate(objs):
        for x, ch in enumerate(row):
            ident = m["legend"].get(ch)
            e = entries.get(ident or "")
            if not e or e["kind"] != "tile" or ident in seen or e["solid"] == "no":
                continue
            seen.add(ident)
            rows_solid = tile_solid_rows(e)
            over = e.get("over_rows", 0)
            worst = (0.0, None)
            for r in range(e["h"]):
                if r < over:                                  # an `over` row is meant to be see-through
                    continue
                for c in range(e["w"]):
                    if not rows_solid[r][c]:
                        continue
                    n = e["index"] + ATLAS_COLS * r + c
                    ax, ay = (n % ATLAS_COLS) * cell, (n // ATLAS_COLS) * cell
                    if ay + cell > ah or ax + cell > aw or ach != 4:
                        continue
                    clear = sum(1 for yy in range(ay, ay + cell)
                                for xx in range(ax, ax + cell) if arows[yy][xx * 4 + 3] < 128)
                    pct = 100 * clear / (cell * cell)
                    if pct > worst[0]:
                        worst = (pct, (c, r))
            if worst[0] > 15:                                 # one line an entry, its worst tile
                c, r = worst[1]
                warn.append(f"{name}: '{ident}' is solid but its art leaves tile {c},{r} "
                            f"{worst[0]:.0f}% transparent — the player sees ground inside a tile "
                            f"they cannot walk on. Redraw that slot filling the footprint, or make "
                            f"the stamp smaller in tiles.")
    return warn


def tile_solid_rows(e):
    """[[bool]] per tile of a stamp, from `- solid: yes|no|##/.#`."""
    v = (e["solid"] or "no").strip()
    if v in ("yes", "true"):
        return [[True] * e["w"] for _ in range(e["h"])]
    if v in ("no", "false", ""):
        return [[False] * e["w"] for _ in range(e["h"])]
    rows = [r.strip() for r in v.split("/")]
    out = []
    for r in range(e["h"]):
        line = rows[r] if r < len(rows) else ""
        out.append([(c < len(line) and line[c] == "#") for c in range(e["w"])])
    return out


def check_tmap(mp):
    """(errors, warnings) for one .tmap against its tileset."""
    m, err, warn = parse_tmap(mp), [], []
    name = m["path"].name
    setname = slug(m["meta"].get("tileset", ""))
    if not setname:
        return [f"{name}: '## meta' has no 'tileset:' line"], []
    if not tileset_doc(setname).exists():
        return [f"{name}: tileset '{setname}' has no {tileset_doc(setname).relative_to(ROOT.parent)}"], []
    entries = tileset_entries(setname)
    if not m["meta"].get("size"):
        return [f"{name}: '## meta' has no 'size: W H' line"], []
    try:
        w, h = (int(x) for x in m["meta"]["size"].split()[:2])
    except ValueError:
        return [f"{name}: 'size: {m['meta']['size']}' is not 'W H'"], []
    for ch, tile in sorted(m["legend"].items()):
        if tile not in entries:
            err.append(f"{name}: legend '{ch}' is '{tile}', which has no entry in "
                       f"{tileset_doc(setname).relative_to(ROOT.parent)}")
    for key in ("ground", "objects"):
        rows = m[key]
        if key == "objects" and not rows:
            continue
        if len(rows) != h:
            err.append(f"{name}: '## {key}' has {len(rows)} rows, 'size:' says {h}")
        for y, line in enumerate(rows):
            if len(line) != w:
                err.append(f"{name}: '## {key}' row {y} is {len(line)} chars, 'size:' says {w}")
            for x, c in enumerate(line):
                if c in (".", "+", " ") and key == "objects":
                    continue
                if c not in m["legend"]:
                    err.append(f"{name}: '## {key}' cell {x},{y} is '{c}', which the legend does not name")
                elif key == "ground" and entries.get(m["legend"][c], {}).get("layer") != "ground":
                    warn.append(f"{name}: ground cell {x},{y} uses '{m['legend'][c]}', "
                                f"which is layer '{entries[m['legend'][c]]['layer']}'")
    hard = []
    w2, h2, solid, tops = tmap_solid_grid(m, entries, hard)
    err += [f"{name}: {e}" for e in hard]
    walkable = lambda x, y, what: (
        err.append(f"{name}: {what} at {x},{y} is outside the map") if not (0 <= x < w and 0 <= y < h)
        else err.append(f"{name}: {what} at {x},{y} stands on a solid tile") if solid[y][x] else None)
    sp = (m["meta"].get("spawn") or "").split()
    if len(sp) < 2:
        err.append(f"{name}: '## meta' has no 'spawn: x y [facing]' line")
    else:
        walkable(int(sp[0]), int(sp[1]), "spawn")
    err += check_map_light(m, name, w, h)
    err += check_map_decals(m, name, w, h, entries)
    warn += stamp_fill_warnings(m, name, entries, setname)
    text_ids = set(load_field_text())
    todo = tmap_todo()
    for t in m["triggers"]:
        if len(t) < 5:
            err.append(f"{name}: trigger '{' '.join(t)}' is not 'x y w h kind ...'")
            continue
        try:
            tx, ty, tw, th = (int(v) for v in t[:4])
        except ValueError:
            err.append(f"{name}: trigger '{' '.join(t)}' has a non-numeric box")
            continue
        kind, rest = t[4], t[5:]
        if kind not in TRIGGER_KINDS:
            err.append(f"{name}: trigger kind '{kind}' is not one of {', '.join(sorted(TRIGGER_KINDS))}")
            continue
        if len(rest) < TRIGGER_KINDS[kind]:
            err.append(f"{name}: '{kind}' trigger at {tx},{ty} wants {TRIGGER_KINDS[kind]} argument(s), "
                       f"got {len(rest)}")
            continue
        if not (0 <= tx < w and 0 <= ty < h and tx + tw <= w and ty + th <= h):
            err.append(f"{name}: trigger box {tx},{ty} {tw}x{th} runs off the {w}x{h} map")
        if kind in ("exit", "door"):
            target = slug(rest[0])
            if not tmap_path(target).exists() and target not in todo:
                err.append(f"{name}: {kind} points at map '{target}', which has no "
                           f"{tmap_path(target).relative_to(ROOT.parent)} and is not listed under "
                           f"'## todo' in {(TMAPS / 'README.md').relative_to(ROOT.parent)}")
        elif kind == "message" and rest[0] not in text_ids:
            err.append(f"{name}: message id '{rest[0]}' has no '## {rest[0]}' entry in "
                       f"{FIELD_TEXT.relative_to(ROOT.parent)}")
        elif kind == "npc":
            if rest[2] not in text_ids:
                err.append(f"{name}: npc text id '{rest[2]}' has no entry in "
                           f"{FIELD_TEXT.relative_to(ROOT.parent)}")
            sprite = FIELD_DIRS["walker"] / f"{slug(rest[0])}.png"
            if not sprite.exists():
                warn.append(f"{name}: npc walker '{rest[0]}' has no {sprite.relative_to(ROOT.parent)} yet")
    return err, warn


def placeholder_rgb(name):
    """A stable muted colour for a tile with no atlas cell yet, so a preview is still readable."""
    v = zlib.crc32(name.encode())
    return (90 + (v & 63), 90 + ((v >> 6) & 63), 90 + ((v >> 12) & 63))


PREVIEW_TILE = 32                     # the preview draws a tile this big; the maps are 48x36


TERRAIN_STANDIN = {                   # a readable flat colour until the swatch is drawn
    "grass": (104, 132, 78), "grass_dry": (150, 148, 96), "crop": (128, 140, 70),
    "mud": (86, 72, 58), "dirt": (150, 126, 94), "gravel": (146, 142, 134),
    "paving": (150, 148, 142), "bridge_deck": (134, 104, 70), "plank": (134, 104, 70),
    "water": (70, 104, 140), "sand": (204, 184, 140), "snow": (220, 224, 232),
}


def terrain_colour(e):
    """A flat stand-in for a terrain with no swatch yet.

    Named where the name is one we use, hashed otherwise — a preview whose dirt is bright purple
    tells you nothing about whether the map reads."""
    if e["id"] in TERRAIN_STANDIN:
        return TERRAIN_STANDIN[e["id"]]
    for k, v in TERRAIN_STANDIN.items():
        if k in e["id"]:
            return v
    return placeholder_rgb(e["id"])


def load_swatch(setname, e, px):
    """(w, h, rows RGBA) of a terrain's swatch, scaled to `px` per tile. A flat colour if unpainted."""
    p = swatch_path(setname, e["id"])
    tw, th = e["swatch"]
    W, H = tw * px, th * px
    if p.exists():
        w, h, ch, rows = read_png(p)
        rows = to_rgba(rows, w, h, ch)
        if (w, h) != (W, H):
            rows = resize_box(rows, w, h, 4, W, H)
        return W, H, rows
    c = bytes(terrain_colour(e)) + b"\xff"
    return W, H, [bytearray(c * W) for _ in range(H)]


def preview_tmap(mp):
    """Render a map the way the engine will (D20): dual-grid masks over world-space swatches.

    This exists so a map can be judged on the Mac before the engine lands, and so the masks can be
    judged at all — they are generated, so nobody ever sees them until something draws them. Terrains
    with no swatch yet come out as flat palette colours, which is still the right SHAPE."""
    m = parse_tmap(mp)
    setname = slug(m["meta"].get("tileset", ""))
    entries = tileset_entries(setname)
    w, h = (int(x) for x in m["meta"]["size"].split()[:2])
    T = PREVIEW_TILE
    W, H = w * T, h * T
    img = [bytearray(b"\x20\x20\x28\xff" * W) for _ in range(H)]

    # ── the ground: one pass per terrain, low priority first, through the dual grid ──
    terrains = set_terrains(entries)
    tidx = {t: n for n, t in enumerate(terrains)}
    ground = [[None] * w for _ in range(h)]
    for y, line in enumerate(m["ground"][:h]):
        for x, c in enumerate(line[:w]):
            ident = m["legend"].get(c)
            if ident in tidx:
                ground[y][x] = ident
    base = terrains[0] if terrains else None
    at = lambda x, y: (ground[y][x] if 0 <= x < w and 0 <= y < h else base) or base
    sw = {t: load_swatch(setname, entries[t], T) for t in terrains}
    masks = {}
    for n, t in enumerate(terrains):
        e = entries[t]
        style = e["edge_style"]
        sww, swh, swpx = sw[t]
        if n == 0:                                             # the lowest terrain floods the map
            for py in range(H):
                src = swpx[py % swh]
                row = img[py]
                for px in range(W):
                    o, so = px * 4, (px % sww) * 4
                    row[o:o + 4] = src[so:so + 4]
            continue
        for j in range(h + 1):
            for i in range(w + 1):
                b = 0                                          # 1 NW, 2 NE, 4 SE, 8 SW
                if at(i - 1, j - 1) == t: b |= 1
                if at(i, j - 1) == t: b |= 2
                if at(i, j) == t: b |= 4
                if at(i - 1, j) == t: b |= 8
                case = DUAL_CASES[b]
                if case is None:
                    continue
                shape, rot = case
                v = h32(i, j, tidx[t]) % MASK_VARIANTS
                key = (shape, style, v, rot)
                if key not in masks:
                    mk = tile_mask(shape, style, v, rot)
                    n_ = len(mk)
                    masks[key] = [bytearray(mk[y * n_ // T][x * n_ // T] for x in range(T))
                                  for y in range(T)]
                mk = masks[key]
                ox, oy = i * T - T // 2, j * T - T // 2
                for py in range(T):
                    cy = oy + py
                    if not (0 <= cy < H):
                        continue
                    mrow, row, src = mk[py], img[cy], swpx[cy % swh]
                    for px in range(T):
                        cx = ox + px
                        if 0 <= cx < W and mrow[px]:
                            o, so = cx * 4, (cx % sww) * 4
                            row[o:o + 4] = src[so:so + 4]

    # ── the decals: the same hash scatter the engine will run, so the density is judged here ──
    decals = [entries[i] for i in set_decals(entries)]
    art = {}
    for e in decals:
        p = decal_path(setname, e["id"])
        if p.exists():
            dw, dh, dch, dpx = read_png(p)
            art[e["id"]] = (dw, dh, to_rgba(dpx, dw, dh, dch))
    placed = 0
    for y in range(h):
        for x in range(w):
            t = at(x, y)
            here = [e for e in decals if t in e["on"]]
            if not here:
                continue
            touch = {at(x + dx, y + dy) for dx, dy in ((1, 0), (-1, 0), (0, 1), (0, -1))}
            for e in here:
                bias = max([e["edge_bias"].get(n, 1.0) for n in touch] or [1.0])
                field = 0.5 + 0.5 * vnoise(x / e["cluster"], y / e["cluster"],
                                           h32(zlib.crc32(e["id"].encode())))
                want = e["density"] * bias * field
                hh = h32(x, y, zlib.crc32(e["id"].encode()))
                if (hh & 0xffff) / 65535.0 > want:
                    continue
                size = e["sizes"][(hh >> 17) % len(e["sizes"])]
                px_ = int(e["tiles"] * size * T)
                if e["id"] not in art:                       # no art yet: a dot, so density reads
                    r = max(1, px_ // 3)
                    cx, cy = x * T + (hh >> 5) % T, y * T + (hh >> 11) % T
                    for yy in range(-r, r + 1):
                        for xx in range(-r, r + 1):
                            if xx * xx + yy * yy > r * r:
                                continue
                            ax, ay = cx + xx, cy + yy
                            if 0 <= ax < W and 0 <= ay < H:
                                o = ax * 4
                                img[ay][o:o + 3] = bytes(max(0, v - 28) for v in img[ay][o:o + 3])
                    placed += 1
                    continue
                dw, dh, dpx = art[e["id"]]
                nw, nh = max(1, round(px_ * dw / max(dw, dh))), max(1, round(px_ * dh / max(dw, dh)))
                sc = resize_box(dpx, dw, dh, 4, nw, nh)
                flip = e["flip"] and (hh >> 3) & 1
                ox, oy = x * T + (hh >> 5) % max(1, T - nw // 2), y * T + (hh >> 11) % max(1, T - nh // 2)
                for yy in range(nh):
                    ay = oy + yy
                    if not (0 <= ay < H):
                        continue
                    for xx in range(nw):
                        ax = ox + xx
                        sx_ = (nw - 1 - xx) if flip else xx
                        if 0 <= ax < W and sc[yy][sx_ * 4 + 3] >= 128:
                            img[ay][ax * 4:ax * 4 + 4] = sc[yy][sx_ * 4:sx_ * 4 + 4]
                placed += 1

    # ── the stamps: straight out of the atlas, exactly as before ──
    atlas, arows = None, 0
    if atlas_path(setname).exists():
        aw, ah, ach, apx = read_png(atlas_path(setname))
        apx = to_rgba(apx, aw, ah, ach)
        if aw != ATLAS_COLS * T:
            k = ATLAS_COLS * T
            ah = max(1, round(ah * k / aw))
            apx, aw = resize_box(apx, aw, len(apx), 4, k, ah), k
        atlas, arows = apx, ah
    flips = {(int(f[0]), int(f[1])) for f in m.get("flips", []) if len(f) == 2}

    def blit(index, cw, chh, px, py, mirror=False):
        if px + cw * T > W or py + chh * T > H:
            return True
        for r in range(chh):
            for c in range(cw):
                cell = index + ATLAS_COLS * r + (cw - 1 - c if mirror else c)
                ax, ay = (cell % ATLAS_COLS) * T, (cell // ATLAS_COLS) * T
                if atlas is None or ay + T > arows:
                    return False
                for y in range(T):
                    src, dst = atlas[ay + y], img[py + r * T + y]
                    for x in range(T):
                        sx_ = (T - 1 - x) if mirror else x
                        if src[(ax + sx_) * 4 + 3] >= 128:
                            o = (px + c * T + x) * 4
                            dst[o:o + 4] = src[(ax + sx_) * 4:(ax + sx_) * 4 + 4]
        return True

    def flat(name, px, py, cw, chh):
        rgb = bytes(placeholder_rgb(name)) + b"\xff"
        cw = min(cw, (W - px) // T)
        chh = min(chh, (H - py) // T)
        for y in range(chh * T):
            row = img[py + y]
            for x in range(cw * T):
                edge = x < 1 or y < 1 or x >= cw * T - 1 or y >= chh * T - 1
                row[(px + x) * 4:(px + x) * 4 + 4] = b"\x18\x18\x18\xff" if edge else rgb

    miss = set()
    for y, line in enumerate(m["objects"][:h]):
        for x, c in enumerate(line[:w]):
            if c in (".", "+", " "):
                continue
            e = entries.get(m["legend"].get(c, ""))
            if not e or e["kind"] != "tile":
                continue
            cw, chh = e["foot"]
            if not blit(e["index"], cw, chh, x * T, y * T, (x, y) in flips):
                flat(e["id"], x * T, y * T, cw, chh)
                miss.add(e["id"])
    # an authored decal beats the scatter, so it is drawn last
    for f in m.get("decals", []):
        if len(f) >= 3 and f[2] in art:
            dw, dh, dpx = art[f[2]]
            x, y = int(f[0]) * T, int(f[1]) * T
            for yy in range(min(dh, H - y)):
                for xx in range(min(dw, W - x)):
                    sx_ = (dw - 1 - xx) if len(f) > 3 else xx
                    if dpx[yy][sx_ * 4 + 3] >= 128:
                        img[y + yy][(x + xx) * 4:(x + xx) * 4 + 4] = dpx[yy][sx_ * 4:sx_ * 4 + 4]

    OUT.mkdir(exist_ok=True)
    out = OUT / f"{slug(mp)}.tmap.png"
    write_png(out, W, H, 4, img)
    painted = sum(1 for t in terrains if swatch_path(setname, t).exists())
    print(f"{m['path'].name}: {w}x{h} tiles -> {out.relative_to(ROOT.parent)}  {W}x{H}   "
          f"{len(terrains)} terrain(s) ({painted} with a swatch), {placed} decal(s) scattered"
          + (f", {len(miss)} stamp(s) still flat placeholders: {', '.join(sorted(miss))}" if miss else ""))


def all_tmaps():
    return sorted(p.stem for p in TMAPS.glob("*.tmap")) if TMAPS.exists() else []


def cmd_tmap(args):
    sub = args[0] if args else ""
    names = [a for a in args[1:] if not a.startswith("--")]
    if sub not in ("check", "preview"):
        die("usage: tmap check <map>|--all   |   tmap preview <map>")
    names = names or (all_tmaps() if "--all" in args or not names else [])
    if not names:
        die(f"no .tmap files in {TMAPS.relative_to(ROOT.parent)}")
    bad = 0
    for mp in names:
        if sub == "preview":
            preview_tmap(mp)
            continue
        err, warn = check_tmap(mp)
        for wmsg in warn:
            print(f"warning: {wmsg}", file=sys.stderr)
        for e in err:
            print(f"ERROR: {e}", file=sys.stderr)
        print(f"{tmap_path(mp).name}: " + (f"{len(err)} error(s)" if err else "ok"))
        bad += 1 if err else 0
    if bad:
        sys.exit(f"story_prompt: {bad} map(s) rejected")


# ───────────────────────── Painted screens: a top-down map and its walkable mask ─────────────────────────
#
# Owner's direction, 2026-09-19: **the field is Phantasy Star IV, not Final Fantasy VIII.** The
# angled three-quarter experiment is dropped — with an angled camera every object hides ground, the
# walkable mask had to guess what was behind a statue, and the walker needed a per-screen scale. A
# top-down 16-bit map has none of those problems: nothing is walked behind, one scale holds edge to
# edge, and what the player can walk on is simply what the painter left as ground.
#
# So a screen is ONE PAINTING and ONE MASK, in one chat, one message each:
#
#   returned.png       the top-down map        -> story/field/screens/<map>_<zone>_paint.png
#   returned_walk.png  green = walkable ground -> ..._walk.png, the `nav` polygons and the `door` lines
#
# and, only for a screen whose entry says `- overhead: yes`, a third:
#
#   returned_over.png  white = drawn OVER the player (an archway top, a bridge deck, a balcony)
#                                              -> ..._over.png, painting pixels with alpha, `over` line
#
# Everything the engine needs is derived into story/field/screens/<map>_<zone>.screen in SCREEN
# PIXELS. The painting is the world here: there is no block-out to agree with and nothing to line up,
# which is why this path is the main one and `view` is the older one.

SCREENS_DOC = FIELD / "screens.md"            # one '## <map>.<zone>' entry per painted screen
SCREENS_DIR = FIELD / "screens"               # the paintings, the cleaned masks, the .screen files
SCREEN_MAX_W = 1536                           # everything is fitted to the painting, capped to this
SCREEN_DEFAULT_SIZE = (1536, 1024)            # what the prompt asks for when the entry says nothing
SCREEN_EDGES = ("left", "right", "top", "bottom")
SCREEN_WALKER = 64                            # a person's height in painting pixels, the scale of everything
DOOR_FACTOR = 1.15                            # a door is about this much taller than the person
PATH_PERSONS = 3                              # a path is at least this many people wide (FIELD.md 10b)
WALK_OPEN = 3                                 # erode then dilate: kills traced outlines up to 2*3 px wide
WALK_CLOSE = 2                                # then dilate then erode: shuts pinholes and hairline seams
MASK_SPECK = 40                               # a mask blob smaller than this is the model's noise
NAV_MIN_AREA = 600                            # a walkable island smaller than this is never kept
NAV_RETRY_POLYS = 120                         # more than this and the outline is retraced coarser
NAV_MAX_POLYS = 400                           # more than this and the mask is too fiddly to trust
NAV_SIMPLIFY = 3.0                            # Douglas-Peucker tolerance, pixels
NAV_COVER_SLACK = 0.03                        # a coarser outline is only taken if it still covers this well
NAV_MAX_VERTS = 8                             # convex polygons stay at the size FIELD.md's nav uses
EXIT_MIN_RUN = 12                             # a walkable run at the frame edge shorter than this is noise
DOOR_MAX_WIDE = 1.5                           # a threshold notch is at most this many walkers wide
DOOR_POCKET = 0.75                            # ...and has building within this much of a walker either side
DOOR_MIN_AREA = 40                            # a notch smaller than this is mask noise, not a door

WALK_MASK_PROMPT = """\
Make a WALKABLE MASK of the map you just generated. This is a technical image for a game engine, not \
an illustration.

Output: the exact same image size and framing as the map, aligned pixel for pixel, so that it can be \
laid over the map and every edge lines up. Do not re-imagine, re-compose, crop, zoom or shift \
anything. Trace the map.

Use exactly TWO flat colours and nothing else:
- Pure green #00FF00 = ground a person could stand or walk on.
- Pure black #000000 = everything else.

No outlines, no line art, no shading, no gradients, no texture, no anti-aliasing, no third colour, no \
text. Every pixel is either pure green or pure black. Edges are hard.

GREEN (walkable): open ground, grass, dirt, paving, streets, paths and yards; steps and stairs (paint \
a whole flight as one solid green shape, not tread by tread); ramps; bridges and plank walkways. \
Walkable areas that connect on the map must connect in the mask: a path joins the square it runs \
into, a bridge joins the bank at both ends. A doorway a person can enter gets a small green notch at \
its threshold, in the wall, the width of the door.

BLACK (not walkable): every object exactly as it is drawn, its whole drawn area — buildings and their \
roofs, walls, fences, hedges, gates, wells, troughs, barrels, crates, carts, market stalls, signposts, \
statues, trees, bushes, crops, rocks, cliffs, water and streams. If it is drawn on the map and it is \
not ground, it is black, over the whole shape the painting gives it.

Keep paths at least as wide as they are on the map. When unsure whether a person could walk there, \
paint it black."""

OVER_MASK_PROMPT = """\
Now make an OVERHEAD MASK of the same map: the parts of the picture that are drawn ABOVE a person \
walking there, rather than behind them. Same rules as the last one: exact same size and framing, \
aligned pixel for pixel, traced from the map.

Use exactly TWO flat colours: pure white #FFFFFF and pure black #000000. No outlines, no shading, no \
gradients, no anti-aliasing, no text.

WHITE = anything a person walks UNDERNEATH: the top of an archway or a gate, a bridge deck with a \
path running under it, an overhanging balcony or awning with ground beneath it, the branches of a \
tree whose trunk the path passes under, a roof that a walkway goes below.

BLACK = everything else, including all the ordinary ground, and every object a person walks AROUND \
rather than under.

If nothing on this map is walked underneath, the whole image is black, and that is a correct answer."""

# The locked blocks in STYLE.md that apply to a top-down field map. `layout`, `framing`, `acting`,
# `character_design`, `dialogue_box` and the sheet blocks are all about a manga page — panels,
# borders, crops, people acting — and a field map is none of those, so they are deliberately left out
# and the package says so.
SCREEN_BLOCKS = ("map_header", "map_rendering", "map_negative")


def screen_key(mp, zone):
    return f"{mp}.{zone}"


def screen_entries():
    """{'<map>.<zone>': {desc, exits, landmark, spawn, size, walker, overhead}} from screens.md."""
    if not SCREENS_DOC.exists():
        die(f"{SCREENS_DOC.relative_to(ROOT.parent)} not found; it holds one '## <map>.<zone>' entry "
            f"per painted screen")
    out, unknown = {}, []
    for name, body in h2_sections(detok(SCREENS_DOC.read_text(), unknown)).items():
        if not re.fullmatch(r"[a-z0-9_]+\.[a-z0-9_]+", name):
            continue                                     # the prose at the top of the file
        kv = kv_lines(body)
        desc = squash(" ".join(l for l in body.splitlines()
                               if l.strip() and not re.match(r"^\s*- [\w ]+?:", l)))
        if not desc:
            die(f"screens.md: '## {name}' has no description paragraph")
        check_description(desc, name, "screens.md")
        exits = {}
        for part in (kv.get("exits") or "").replace("→", "->").split(","):
            if not part.strip():
                continue
            bits = [b.strip().lower() for b in part.split("->")]
            if len(bits) != 2 or bits[0] not in SCREEN_EDGES or not slug(bits[1]):
                die(f"screens.md: '## {name}' exit '{squash(part)}' is not '<edge> -> <map>'; "
                    f"edges are {', '.join(SCREEN_EDGES)}")
            exits[bits[0]] = slug(bits[1])
        size = SCREEN_DEFAULT_SIZE
        if kv.get("size"):
            m = re.fullmatch(r"(\d+)\s*[x×]\s*(\d+)", kv["size"].strip())
            if not m:
                die(f"screens.md: '## {name}' size '{kv['size']}' is not WxH")
            size = (int(m.group(1)), int(m.group(2)))
        spawn = None
        if kv.get("spawn"):
            bits = kv["spawn"].split()
            if len(bits) != 2 or not all(b.lstrip("-").isdigit() for b in bits):
                die(f"screens.md: '## {name}' spawn '{kv['spawn']}' is not 'x y' in screen pixels")
            spawn = (int(bits[0]), int(bits[1]))
        walker = SCREEN_WALKER
        if kv.get("walker"):
            if not kv["walker"].strip().isdigit():
                die(f"screens.md: '## {name}' walker '{kv['walker']}' is not a height in pixels")
            walker = int(kv["walker"])
        mp, zone = name.split(".")
        out[name] = {"key": name, "map": mp, "zone": zone, "desc": desc, "exits": exits,
                     "landmark": squash(kv.get("landmark", "")), "spawn": spawn, "size": size,
                     "walker": walker,
                     "overhead": (kv.get("overhead", "no").strip().lower() in ("yes", "true", "1"))}
    if unknown:
        die(f"screens.md: unknown name token(s) {', '.join('{{%s}}' % t for t in unknown)}. "
            f"Every token needs a row in {NAMES_FILE.relative_to(ROOT.parent)}.")
    if not out:
        die("screens.md: no '## <map>.<zone>' entries")
    return out


def screen_paths(mp, zone):
    """Every file one screen owns."""
    base = str(SCREENS_DIR / f"{mp}_{zone}")
    return {"paint": Path(base + "_paint.png"), "walk": Path(base + "_walk.png"),
            "over": Path(base + "_over.png"), "screen": Path(base + ".screen"),
            "debug": Path(base + "_debug.png")}


def screen_outputs(mp, zone, overhead=False):
    p = screen_paths(mp, zone)
    keys = ["paint", "walk"] + (["over"] if overhead else []) + ["screen"]
    return [str(p[k].relative_to(ROOT.parent)) for k in keys]


def screen_attachments(style, mp, warn):
    """A map needs no block-out. The style sheet, then whatever this map already has drawn."""
    out = []
    sp = existing(style["refs"].get("style"))
    if sp:
        out.append((sp, "STYLE reference. " + style["refs"].get("style_note", "")))
    else:
        warn.append(f"no style reference at {style['refs'].get('style')}; the prompt text carries the style alone")
    placed = map_places(mp)
    for kind, ids in (("buildings", placed["buildings"]), ("props", placed["props"])):
        for i in ids:
            if len(out) >= VIEW_ATTACH_MAX:
                break
            f = (FIELD_DIRS["building"] / f"{i}_front.png") if kind == "buildings" else \
                (FIELD_DIRS["prop"] / f"{i}.png")
            if f.exists():
                out.append((f, f"The {i.replace('_', ' ')} as it is drawn in this game. It belongs in "
                               f"this place; keep its design, colours and proportions."))
    return out


def screen_paint_prompt(e, style, warn):
    """Prompt A: the map itself, in the top-down oblique a 16-bit JRPG town is drawn in."""
    b = style["blocks"]
    missing = [k for k in SCREEN_BLOCKS if k not in b]
    if missing:
        die(f"STYLE.md: a map prompt needs the locked block(s) {', '.join(missing)}. They are the "
            f"field-map half of the style: the panel blocks talk about skin, hair and skies, which a "
            f"map has none of.")
    w, h = e["size"]
    P = e["walker"]
    D = int(round(P * DOOR_FACTOR))
    L = ["Paint one complete top-down map for a 16-bit JRPG: the whole place the player walks around "
         "in, as one finished picture. This is a field map, not a comic page and not a scene — no "
         "panels, no borders, no frame, one single image filling the canvas edge to edge.", "",
         b["map_header"], "",
         "THE PROJECTION, and it is the most important instruction here: the classic top-down oblique "
         "those games are drawn in. The ground is seen from straight above, flat, as if the map were "
         "laid on a table. Buildings and objects show their roof and their front — their south — face "
         "only, and every vertical edge runs straight up the screen. There is NO perspective, NO "
         "vanishing point, NO horizon and no sky. Nothing gets smaller further up the picture: one "
         "uniform scale from edge to edge, the same size at the top as at the bottom. Nothing leans, "
         "nothing is foreshortened, no side or three-quarter faces on anything.", ""]
    if e["landmark"]:
        L += [f"THE LANDMARK, which must be clearly on the map: {e['landmark'].rstrip('.')}.", ""]
    L += ["THE PLACE:", e["desc"], "",
          f"THE SCALE, which everything is drawn to: a person is about {P} pixels tall on this map and "
          f"a door about {D} pixels tall, at {w}x{h}. A house is a few people wide, a well is about one "
          f"person across, and every path, street and gap a person has to walk through is at least "
          f"{PATH_PERSONS * P} pixels wide — {PATH_PERSONS} people abreast. Draw no people; the scale "
          f"is there so the buildings and the gaps between them come out the right size.", "",
          "THE GROUND A PERSON CAN WALK ON reads at a glance: open ground, paving, dirt and grass, with "
          "a clear edge where it stops and a clear line where every object stands on it. Paths connect "
          "to each other and to the places they lead; nothing walkable is left as an island that "
          "cannot be reached.", ""]
    if e["exits"]:
        runs = "; ".join(f"the {edge} edge, the way to {t.replace('_', ' ')}"
                         for edge, t in sorted(e["exits"].items()))
        L += [f"THE WAYS OUT run off the edge of the map as roads or paths: {runs}. Each one reaches "
              f"the very edge of the canvas, at least {PATH_PERSONS * P} pixels wide, not stopping "
              f"short of it and not blocked by anything.", ""]
    L += ["HOW THE PLACE IS SHAPED — people grew this, they did not lay it out. It spread from one "
          "reason, so the oldest and densest part is round that and it thins toward the edges. Lanes "
          "bend, fork, pinch and widen, and some end in a yard. Nothing is in a row: buildings are "
          "staggered along the lanes at irregular spacings and in clearly different sizes and shapes, "
          "set forward and back from the path rather than lined up, so no two edges run together for "
          "long. No grid, no city blocks, no repeated spacing, no mirror symmetry — those belong to "
          "the ancients and this is not one of their places. The map does not stop at a clean line: "
          "the edges dissolve into orchard, hedge, wall, shed, rock and field.", "",
          "NO PEOPLE, no characters, no animals: the game draws those on top as moving sprites, and a "
          "painted one would stand still forever. No text, no letters, no numbers, no writing on signs "
          "or boards, no labels, no watermark, no user interface, no map legend, no compass, no frame, "
          "no border, no vignette and no letterboxing.", "",
          f"RENDERING: {b['map_rendering']}", "",
          f"SIZE: one landscape image, {w}x{h}, painted edge to edge, the map filling the whole frame "
          f"like a piece cut out of a larger world.", "",
          f"AVOID: {b['map_negative']}, empty unpainted areas, writing of any kind on signs or boards"]
    return "\n".join(L)


def cmd_screen(args):
    """One painted top-down map per screen: the map and its walkable mask, asked for in one chat."""
    pos = [a for a in args if not a.startswith("--")]
    if len(pos) == 1 and "." in pos[0]:
        pos = pos[0].split(".", 1)
    if len(pos) != 2:
        die("usage: screen <map> <zone>   (the entry lives in story/field/screens.md)")
    mp, zone = slug(pos[0]), slug(pos[1])
    entries = screen_entries()
    e = entries.get(screen_key(mp, zone))
    if not e:
        die(f"no '## {mp}.{zone}' in {SCREENS_DOC.relative_to(ROOT.parent)}. It has: "
            f"{', '.join(sorted(entries)) or '(nothing)'}")
    style, warn = load_style(), []
    attach = screen_attachments(style, mp, warn)
    paint = screen_paint_prompt(e, style, warn)
    w, h = e["size"]
    over = e["overhead"]
    p, name = screen_paths(mp, zone), f"screen_{mp}_{zone}"
    outs = screen_outputs(mp, zone, over)
    steps = [{"n": 1, "what": "paint", "save": f"{RETURNED}.png", "prompt": paint},
             {"n": 2, "what": "walk", "save": f"{RETURNED}_walk.png", "prompt": WALK_MASK_PROMPT}]
    if over:
        steps.append({"n": 3, "what": "over", "save": f"{RETURNED}_over.png", "prompt": OVER_MASK_PROMPT})

    manifest = pkg_path(name, "sheet.json")
    manifest.parent.mkdir(parents=True, exist_ok=True)
    manifest.write_text(json.dumps({
        "screen": name, "kind": "screen", "map": mp, "zone": zone, "size": [w, h],
        "landmark": e["landmark"], "exits": e["exits"], "spawn": list(e["spawn"]) if e["spawn"] else None,
        "walker": e["walker"], "overhead": over, "outputs": outs,
        "returns": {s["what"]: s["save"] for s in steps},
        "steps": [{"n": s["n"], "what": s["what"], "save": s["save"]} for s in steps],
        "prompts": {s["what"]: s["prompt"] for s in steps},
    }, indent=2) + "\n")

    rel = lambda q: q.relative_to(ROOT.parent)
    here = PKG_DIR.relative_to(ROOT.parent) if PKG_DIR is not None else Path("story/out")
    has = lambda o: "**exists**" if (ROOT.parent / o).exists() else "missing"
    L = [f"# ChatGPT package: {PKG_TITLE or f'{mp} / {zone} — painted map'}", "",
         f"**One chat, {len(steps)} messages.** The map first, then the mask" +
         (", then the overhead mask" if over else "") + ", each in its own message in the same chat, so",
         "ChatGPT is tracing a picture it can see. Do not start a new chat for the mask, and do not ask",
         "for the map and the mask in one message — the mask must be traced from the finished map.", "",
         "## What exists already", ""]
    L += [f"- `{o}` — {has(o)}" for o in outs]
    L += ["", f"Style blocks used from `story/STYLE.md`: {', '.join(SCREEN_BLOCKS)} (`negative` minus "
              f"its comic-page complaints). The panel blocks — `layout`, `framing`, `acting`, "
              f"`character_design`, `dialogue_box`, `sheet_layout` — are deliberately **not** used: "
              f"this is a top-down field map, not a manga page.", "",
          "## 1. Start a new chat and attach these files, in this order", ""]
    L += [f"{n}. `{rel(q)}`" for n, (q, _) in enumerate(attach, 1)] or \
         ["(nothing to attach yet — no style reference and no cut sprites for this map. The prompt "
          "carries the style on its own.)"]
    if attach:
        L += [""] + [f"- Image {n}: {note}" for n, (_, note) in enumerate(attach, 1)]
    titles = {"paint": f"the map ({w}x{h})", "walk": "the walkable mask",
              "over": "the overhead mask (what is drawn above the player)"}
    for s in steps:
        L += ["", f"## {s['n'] + 1}. Message {s['n']} — paste this and get {titles[s['what']]}", "",
              "````", s["prompt"], "````", "",
              f"Save it in **this folder** as `{s['save']}`" +
              (f" (the whole path is `{here}/{s['save']}`). A .jpg or .webp works too; the tool "
               f"converts it." if s["n"] == 1 else ".")]
    L += ["", f"## {len(steps) + 2}. Check before cutting", "",
          "- [ ] Top-down: the ground is seen from straight above, objects show only roof and front face",
          "- [ ] No perspective, no vanishing point, no horizon, no sky, nothing shrinking with distance",
          f"- [ ] One scale everywhere: a door about {D_of(e)} px tall, paths {PATH_PERSONS * e['walker']} px wide",
          "- [ ] Lanes bend; buildings are staggered, different sizes, never in rows; no grid",
          "- [ ] Every way out runs off its edge of the frame"
          + (f" ({', '.join(sorted(e['exits']))})" if e["exits"] else ""),
          "- [ ] No people, no animals, no text, no interface, no border",
          "- [ ] The mask is the same size and framing as the map, pixel for pixel",
          "- [ ] The mask is two flat colours only — no outlines, no shading, no third colour",
          "- [ ] Green is only ground: every object, roof, tree, wall and stretch of water is black",
          "- [ ] Every enterable door has a small green notch at its threshold", "",
          "If a mask drifts, reply in the same chat: \"Trace the map exactly — same size and framing,",
          "two flat colours only, no outlines.\"", "",
          f"## {len(steps) + 3}. Then cut", "", "```", f"./story_prompt.py ingest {here}", "```", "",
          "That fits everything to one size and writes " + ", ".join(f"`{o}`" for o in outs) + " — the",
          "size, the nav polygons, the doors, the exits, the spawn and the walker height, all in screen",
          "pixels, which is what the engine loads.",
          f"Add `--debug` to also write `{rel(p['debug'])}`: the map with the nav polygons outlined and",
          "the doors and exits marked, so what the tool understood can be seen at a glance.", ""]
    pkg_path(name, "chatgpt.md").write_text("\n".join(L))
    for x in warn:
        print(f"warning: {x}", file=sys.stderr)
    print(f"wrote {pkg_path(name, 'chatgpt.md').relative_to(ROOT.parent)}  "
          f"({w}x{h}, walker {e['walker']} px, {len(steps)} step(s), {len(e['exits'])} exit(s), "
          f"{len(attach)} attachment(s))")


def D_of(e):
    return int(round(e["walker"] * DOOR_FACTOR))

# ── binary masks, as one big integer a row ──
#
# Erode and dilate a 1536x1024 mask pixel by pixel in Python and a screen takes minutes. A row is a
# W-bit integer instead, so a dilation is five shifts and an or per row and the whole pass is a
# thousand big-integer operations. The structuring element is the 4-neighbour plus, applied n times,
# which is a diamond of radius n.

def mask_of(rows, w, h, ch, test):
    """A flat bytearray of 0/1, one byte a pixel, from a pixel test."""
    g = bytearray(w * h)
    for y in range(h):
        line, o = rows[y], y * w
        for x in range(w):
            i = x * ch
            if test(line[i], line[i + 1], line[i + 2]):
                g[o + x] = 1
    return g


def walk_test(r, g, b):
    return g > 128 and r < 128 and b < 128


def fg_test(r, g, b):
    return (r * 299 + g * 587 + b * 114) > 128000


def bits_of(g, w, h):
    return [int("".join("1" if v else "0" for v in g[y * w:(y + 1) * w]), 2) for y in range(h)]


def grid_of(bits, w, h):
    g = bytearray(w * h)
    for y in range(h):
        o = y * w
        for x, cch in enumerate(format(bits[y], "b").rjust(w, "0")[-w:]):
            if cch == "1":
                g[o + x] = 1
    return g


def dilate_bits(bits, w, n):
    m = (1 << w) - 1
    for _ in range(n):
        out = []
        for y, r in enumerate(bits):
            v = r | (r << 1) | (r >> 1)
            if y:
                v |= bits[y - 1]
            if y + 1 < len(bits):
                v |= bits[y + 1]
            out.append(v & m)
        bits = out
    return bits


def erode_bits(bits, w, n):
    """Erode = the complement of the dilated complement, with everything outside the frame empty."""
    m = (1 << w) - 1
    for _ in range(n):
        comp = [m & ~r for r in bits]
        grown = []
        for y, r in enumerate(comp):
            v = r | (r << 1) | (r >> 1) | (~m)           # outside the side edges is empty
            v |= comp[y - 1] if y else m                 # outside the top and bottom too
            v |= comp[y + 1] if y + 1 < len(comp) else m
            grown.append(v & m)
        bits = [b & (m & ~gq) for b, gq in zip(bits, grown)]
    return bits


def morph(g, w, h, opening=0, closing=0):
    """Open by `opening` px (kill lines), then close by `closing` px (shut pinholes)."""
    bits = bits_of(g, w, h)
    if opening:
        bits = dilate_bits(erode_bits(bits, w, opening), w, opening)
    if closing:
        bits = erode_bits(dilate_bits(bits, w, closing), w, closing)
    return grid_of(bits, w, h)


def grid_components(g, w, h, value=1):
    """[[pixel index, ...]] for each 4-connected run of `value`. Iterative, so a big blob is safe."""
    seen, out = bytearray(w * h), []
    for start in range(w * h):
        if g[start] != value or seen[start]:
            continue
        seen[start] = 1
        stack, comp = [start], []
        while stack:
            i = stack.pop()
            comp.append(i)
            x, y = i % w, i // w
            if x and g[i - 1] == value and not seen[i - 1]:
                seen[i - 1] = 1
                stack.append(i - 1)
            if x + 1 < w and g[i + 1] == value and not seen[i + 1]:
                seen[i + 1] = 1
                stack.append(i + 1)
            if y and g[i - w] == value and not seen[i - w]:
                seen[i - w] = 1
                stack.append(i - w)
            if y + 1 < h and g[i + w] == value and not seen[i + w]:
                seen[i + w] = 1
                stack.append(i + w)
        out.append(comp)
    return out


def fill_pinholes(g, w, h, speck=MASK_SPECK):
    filled = 0
    for comp in grid_components(g, w, h, 0):
        if len(comp) >= speck or any(i % w in (0, w - 1) or i // w in (0, h - 1) for i in comp):
            continue
        filled += 1
        for i in comp:
            g[i] = 1
    return filled


def comp_box(comp, w):
    xs = [i % w for i in comp]
    ys = [i // w for i in comp]
    return min(xs), min(ys), max(xs) - min(xs) + 1, max(ys) - min(ys) + 1


def reachable_walk(g, w, h, spawn, declared, report, debug=False):
    """Only the ground the player can actually stand on: their own island, plus the ways out.

    The first real mask painted green on aqueduct arches on the skyline, on windows, on the inside of
    archways and on roof terraces — all of it unreachable, none of it connected to the plaza. So one
    component survives (the one the spawn point is in, or the biggest), together with any component
    that reaches a declared exit edge, and everything else is named in the report and dropped."""
    comps = grid_components(g, w, h, 1)
    if not comps:
        return g, []
    edge_of = {}
    for n, comp in enumerate(comps):
        edges = set()
        for i in comp:
            x, y = i % w, i // w
            if x == 0:
                edges.add("left")
            if x == w - 1:
                edges.add("right")
            if y == 0:
                edges.add("top")
            if y == h - 1:
                edges.add("bottom")
        edge_of[n] = edges
    home = None
    if spawn:
        sx, sy = int(spawn[0]), int(spawn[1])
        if 0 <= sx < w and 0 <= sy < h:
            idx = sy * w + sx
            home = next((n for n, comp in enumerate(comps) if idx in set(comp)), None)
    if home is None:
        home = max(range(len(comps)), key=lambda n: len(comps[n]))
    keep = {home}
    for n, comp in enumerate(comps):
        if len(comp) >= NAV_MIN_AREA and edge_of[n] & set(declared):
            keep.add(n)
    dropped = []
    for n, comp in enumerate(comps):
        if n in keep:
            continue
        for i in comp:
            g[i] = 0
        if len(comp) >= NAV_MIN_AREA:
            dropped.append(comp)
    # One line per island is dozens of lines on a painted screen and says nothing the summary does
    # not; the detail is there under --debug when an island is actually being hunted.
    if dropped and debug:
        for comp in dropped:
            x, y, bw, bh = comp_box(comp, w)
            report.append(f"dropped an unreachable walkable island of {len(comp)} px at "
                          f"{x},{y} {bw}x{bh} (nothing connects it to the ground the player stands on)")
    elif dropped:
        report.append(f"dropped {len(dropped)} unreachable walkable island(s), "
                      f"{sum(len(c) for c in dropped)} px in all (nothing connects them to the ground "
                      f"the player stands on); rerun with --debug for each one's box")
    return g, sorted(keep)


# ── from a binary grid to convex polygons ──

def trace_loops(g, w, h):
    """Closed loops along the pixel boundary of a binary grid, interior on the right.

    Every filled cell contributes the four unit edges its empty neighbours leave exposed; shared
    edges cancel by never being emitted. Chaining them gives one loop per outline, positive shoelace
    for an outer boundary and negative for a hole. At a pinch — two cells touching only at a corner —
    a point has two ways out, and taking the sharpest right turn splits the figure the way the eye
    does instead of knotting the two loops together."""
    edges = {}
    def add(a, b):
        edges.setdefault(a, []).append(b)
    for y in range(h):
        o = y * w
        for x in range(w):
            if not g[o + x]:
                continue
            if y == 0 or not g[o + x - w]:
                add((x, y), (x + 1, y))
            if x + 1 >= w or not g[o + x + 1]:
                add((x + 1, y), (x + 1, y + 1))
            if y + 1 >= h or not g[o + x + w]:
                add((x + 1, y + 1), (x, y + 1))
            if x == 0 or not g[o + x - 1]:
                add((x, y + 1), (x, y))
    loops = []
    while edges:
        start = next(iter(edges))
        loop, cur, d = [], start, None
        while True:
            outs = edges.get(cur)
            if not outs:
                break
            if len(outs) == 1 or d is None:
                nxt = outs.pop()
            else:                                        # sharpest right turn first
                pick = 0
                for want in ((-d[1], d[0]), d, (d[1], -d[0]), (-d[0], -d[1])):
                    cand = (cur[0] + want[0], cur[1] + want[1])
                    if cand in outs:
                        pick = outs.index(cand)
                        break
                nxt = outs.pop(pick)
            if not outs:
                del edges[cur]
            loop.append(cur)
            d = (nxt[0] - cur[0], nxt[1] - cur[1])
            cur = nxt
            if cur == start:
                break
        if len(loop) >= 4:
            loops.append(loop)
    return loops


def shoelace(pts):
    s = 0.0
    for i in range(len(pts)):
        x0, y0 = pts[i]
        x1, y1 = pts[(i + 1) % len(pts)]
        s += x0 * y1 - x1 * y0
    return s / 2.0


def drop_collinear(pts, eps=1e-9):
    out, n = [], len(pts)
    for i in range(n):
        a, b, c = pts[i - 1], pts[i], pts[(i + 1) % n]
        if abs((b[0] - a[0]) * (c[1] - a[1]) - (b[1] - a[1]) * (c[0] - a[0])) > eps:
            out.append(b)
    return out or pts


def dp_simplify(chain, eps):
    """Douglas-Peucker on an open chain of points."""
    if len(chain) < 3:
        return list(chain)
    keep = [False] * len(chain)
    keep[0] = keep[-1] = True
    stack = [(0, len(chain) - 1)]
    while stack:
        a, b = stack.pop()
        if b <= a + 1:
            continue
        ax, ay = chain[a]
        bx, by = chain[b]
        dx, dy = bx - ax, by - ay
        norm = (dx * dx + dy * dy) ** 0.5
        best, bi = -1.0, a
        for i in range(a + 1, b):
            px, py = chain[i]
            d = (abs(dx * (ay - py) - dy * (ax - px)) / norm) if norm else \
                ((px - ax) ** 2 + (py - ay) ** 2) ** 0.5
            if d > best:
                best, bi = d, i
        if best > eps:
            keep[bi] = True
            stack += [(a, bi), (bi, b)]
    return [p for p, k in zip(chain, keep) if k]


def simplify_loop(loop, eps):
    """Douglas-Peucker on a closed loop: split it at two far-apart points first."""
    pts = drop_collinear([(float(x), float(y)) for x, y in loop])
    if len(pts) < 4:
        return pts
    cx = sum(p[0] for p in pts) / len(pts)
    cy = sum(p[1] for p in pts) / len(pts)
    a = max(range(len(pts)), key=lambda i: (pts[i][0] - cx) ** 2 + (pts[i][1] - cy) ** 2)
    b = max(range(len(pts)), key=lambda i: (pts[i][0] - pts[a][0]) ** 2 + (pts[i][1] - pts[a][1]) ** 2)
    if a > b:
        a, b = b, a
    out = dp_simplify(pts[a:b + 1], eps)[:-1] + dp_simplify(pts[b:] + pts[:a + 1], eps)[:-1]
    return drop_collinear(out) if len(out) >= 3 else pts


def point_in_poly(p, poly):
    x, y, inside = p[0], p[1], False
    for i in range(len(poly)):
        x0, y0 = poly[i]
        x1, y1 = poly[(i + 1) % len(poly)]
        if (y0 > y) != (y1 > y) and x < (x1 - x0) * (y - y0) / (y1 - y0) + x0:
            inside = not inside
    return inside


def bridge_holes(outer, holes):
    """Splice each hole into the outer ring with a two-way bridge, so one ring is left."""
    ring = list(outer)
    for hole in sorted(holes, key=lambda hh: -abs(shoelace(hh))):
        if len(hole) < 3:
            continue
        best = None
        for i, a in enumerate(ring):
            for j, b in enumerate(hole):
                d = (a[0] - b[0]) ** 2 + (a[1] - b[1]) ** 2
                if best is None or d < best[0]:
                    best = (d, i, j)
        _, i, j = best
        ring = ring[:i + 1] + hole[j:] + hole[:j + 1] + ring[i:]
    return ring


def ear_clip(ring):
    """Triangles for a simple polygon, in positive (shoelace > 0) orientation."""
    pts = list(ring)
    if shoelace(pts) < 0:
        pts.reverse()
    idx, tris, guard = list(range(len(pts))), [], 0
    def cross(a, b, c):
        return (pts[b][0] - pts[a][0]) * (pts[c][1] - pts[a][1]) - \
               (pts[b][1] - pts[a][1]) * (pts[c][0] - pts[a][0])
    while len(idx) > 3 and guard < 4 * len(ring) + 64:
        guard += 1
        cut = None
        for k in range(len(idx)):
            a, b, c = idx[k - 1], idx[k], idx[(k + 1) % len(idx)]
            if cross(a, b, c) <= 1e-9:
                continue
            bad = False
            for m in idx:
                if m in (a, b, c):
                    continue
                px, py = pts[m]
                d1 = (pts[b][0] - pts[a][0]) * (py - pts[a][1]) - (pts[b][1] - pts[a][1]) * (px - pts[a][0])
                d2 = (pts[c][0] - pts[b][0]) * (py - pts[b][1]) - (pts[c][1] - pts[b][1]) * (px - pts[b][0])
                d3 = (pts[a][0] - pts[c][0]) * (py - pts[c][1]) - (pts[a][1] - pts[c][1]) * (px - pts[c][0])
                if d1 > 0 and d2 > 0 and d3 > 0:     # strictly inside: a bridge's doubled vertex
                    bad = True                       # sits exactly on the ear and must not block it
                    break
            if not bad:
                cut = k
                break
        if cut is None:                                  # a bridge slit or a rounding knot: force progress
            cut = max(range(len(idx)), key=lambda k: cross(idx[k - 1], idx[k], idx[(k + 1) % len(idx)]))
            if cross(idx[cut - 1], idx[cut], idx[(cut + 1) % len(idx)]) <= 0:
                idx.pop(cut)
                continue
        a, b, c = idx[cut - 1], idx[cut], idx[(cut + 1) % len(idx)]
        tris.append([pts[a], pts[b], pts[c]])
        idx.pop(cut)
    if len(idx) == 3:
        tris.append([pts[i] for i in idx])
    return [t for t in tris if abs(shoelace(t)) > 0.5]


def is_convex(poly, eps=1e-6):
    if len(poly) < 3:
        return False
    for i in range(len(poly)):
        a, b, c = poly[i - 1], poly[i], poly[(i + 1) % len(poly)]
        if (b[0] - a[0]) * (c[1] - a[1]) - (b[1] - a[1]) * (c[0] - a[0]) < -eps:
            return False
    return True


def merge_convex(tris, max_verts=NAV_MAX_VERTS):
    """Hertel-Mehlhorn: drop a shared diagonal whenever what is left is still convex."""
    polys, changed = [list(t) for t in tris], True
    while changed:
        changed = False
        edges = {}
        for pi, poly in enumerate(polys):
            for k in range(len(poly)):
                a, b = poly[k], poly[(k + 1) % len(poly)]
                edges.setdefault(tuple(sorted((a, b))), []).append((pi, k))
        cands = []                                       # longest diagonal first: fewer slivers
        for key, uses in edges.items():
            if len(uses) == 2:
                (ax, ay), (bx, by) = key
                cands.append((-((ax - bx) ** 2 + (ay - by) ** 2), uses))
        for _d, uses in sorted(cands, key=lambda c: c[0]):
            (pi, ki), (qi, kj) = uses
            if pi == qi:
                continue
            p, q = polys[pi], polys[qi]
            P = p[ki + 1:] + p[:ki + 1]                  # starts at the far end of the shared edge
            Q = q[kj + 1:] + q[:kj + 1]
            if P[0] != Q[-1] or P[-1] != Q[0]:
                continue
            merged = drop_collinear(P + Q[1:-1])
            if len(merged) > max_verts or len(set(merged)) != len(merged):
                continue
            if shoelace(merged) <= 0 or not is_convex(merged):
                continue
            polys[pi] = merged
            polys.pop(qi)
            changed = True
            break
    return polys


def polys_from_grid(g, w, h, eps):
    loops = trace_loops(g, w, h)
    outers = [l for l in loops if shoelace(l) > 0]
    holes = [l for l in loops if shoelace(l) < 0]
    out = []
    for outer in outers:
        ring = simplify_loop(outer, eps)
        if len(ring) < 3:
            continue
        mine = [simplify_loop(hl, eps) for hl in holes
                if abs(shoelace(hl)) > eps * eps and point_in_poly(hl[0], ring)]
        out += merge_convex(ear_clip(bridge_holes(ring, [hl for hl in mine if len(hl) >= 3])))
    return out


def nav_polygons(g, w, h, report):
    """Convex polygons in screen pixels covering the walkable ground."""
    green = max(1, sum(g))
    eps, tried = NAV_SIMPLIFY, []
    while True:                                          # coarser until it is few enough, or too loose
        polys = polys_from_grid(g, w, h, eps)
        cover = sum(abs(shoelace(q)) for q in polys) / green
        tried.append((eps, polys, cover))
        if len(polys) <= NAV_RETRY_POLYS or eps >= NAV_SIMPLIFY * 16:
            break
        eps *= 2
    fits = [t for t in tried if abs(t[2] - 1) <= NAV_COVER_SLACK and all(is_convex(q) for q in t[1])]
    eps, polys, cover = (min(fits, key=lambda t: len(t[1])) if fits else
                         min(tried, key=lambda t: abs(t[2] - 1)))
    if len(tried) > 1:
        report.append("the walkable outline came apart into a lot of convex polygons (an edge with "
                      "trees and bushes cut into it): " +
                      ", ".join(f"{n} at {e:.0f} px covering {100 * c:.0f}%"
                                for e, ps, c in tried for n in [len(ps)]) +
                      f" — kept {len(polys)} at {eps:.0f} px, the fewest that still cover the ground")
    if len(polys) > NAV_MAX_POLYS:
        polys.sort(key=lambda p: -abs(shoelace(p)))
        report.append(f"still {len(polys)} polygons; keeping the {NAV_MAX_POLYS} biggest, which loses "
                      f"walkable ground. The mask is probably speckled or has line art left in it.")
        polys = polys[:NAV_MAX_POLYS]
    return polys, eps


# ── doors, exits, the .screen file ──

def row_spans(g, w, y, value=1):
    """[(x0, x1)] horizontal runs of `value` in one row."""
    out, start, o = [], None, y * w
    for x in range(w + 1):
        v = (g[o + x] == value) if x < w else False
        if v and start is None:
            start = x
        elif not v and start is not None:
            out.append((start, x - 1))
            start = None
    return out


def nearest_flags(line, n):
    """(before, after): for each index, the index of the nearest set element at or before / after it."""
    before, after, last = [-1] * n, [n] * n, -1
    for i in range(n):
        if line[i]:
            last = i
        before[i] = last
    last = n
    for i in range(n - 1, -1, -1):
        if line[i]:
            last = i
        after[i] = last
    return before, after


def doors(walk, w, h, walker, report):
    """Green notches poked into a building: the thresholds the mask prompt asks for.

    A doorway is a pocket. On its own row it has non-walkable close by on both sides, and there is
    non-walkable close above it too, because it is cut into the front wall of something. Anything
    wider than about one and a half people is a gap between buildings, not a door."""
    reach = max(4, int(DOOR_POCKET * walker))
    wide = max(6, int(DOOR_MAX_WIDE * walker))
    pocket = bytearray(w * h)
    for y in range(h):
        o = y * w
        solid = bytearray(0 if walk[o + x] else 1 for x in range(w))
        before, after = nearest_flags(solid, w)
        for x0, x1 in row_spans(walk, w, y):
            if x1 - x0 + 1 > wide:
                continue
            l, r = before[x0], after[x1]
            if l < 0 or r >= w or x0 - l > reach or r - x1 > reach:
                continue
            for x in range(x0, x1 + 1):
                pocket[o + x] = 1
    for x in range(w):                                   # and something solid overhead: a wall, not a lane
        col = bytearray(0 if walk[y * w + x] else 1 for y in range(h))
        before, _ = nearest_flags(col, h)
        for y in range(h):
            if pocket[y * w + x] and (before[y] < 0 or y - before[y] > reach):
                pocket[y * w + x] = 0
    out = []
    for comp in grid_components(pocket, w, h, 1):
        if len(comp) < DOOR_MIN_AREA:
            continue
        x, y, bw, bh = comp_box(comp, w)
        if bw > wide or bh > wide:
            continue
        out.append((x, y, bw, bh))
    out.sort(key=lambda d: (d[1], d[0]))
    if out:
        report.append(f"{len(out)} doorway notch(es) found in the walkable mask; they are `door` lines "
                      f"in the .screen and nothing is bound to them yet")
    return out


def edge_runs(g, w, h, edge):
    """[(x0, y0, x1, y1)] walkable runs along one frame edge, in screen pixels."""
    if edge in ("left", "right"):
        x = 0 if edge == "left" else w - 1
        line = [g[y * w + x] for y in range(h)]
    else:
        y = 0 if edge == "top" else h - 1
        line = [g[y * w + x] for x in range(w)]
    runs, start = [], None
    for i, v in enumerate(list(line) + [0]):
        if v and start is None:
            start = i
        elif not v and start is not None:
            if i - start >= EXIT_MIN_RUN:
                runs.append((start, i - 1))
            start = None
    if edge in ("left", "right"):
        x = 0 if edge == "left" else w - 1
        return [(x, a, x, b) for a, b in runs]
    y = 0 if edge == "top" else h - 1
    return [(a, y, b, y) for a, b in runs]


def screen_exits(walk, w, h, declared, report):
    out = []
    for edge in SCREEN_EDGES:
        runs = edge_runs(walk, w, h, edge)
        target = declared.get(edge)
        if target and not runs:
            report.append(f"screens.md declares an exit on the {edge} edge to '{target}', but no "
                          f"walkable ground reaches that edge — the map did not run the road off the "
                          f"frame, or the mask cut it off. The engine gets no {edge} exit.")
        for r in runs:
            if not target:
                report.append(f"walkable ground touches the {edge} edge ({r[0]},{r[1]} to {r[2]},{r[3]}) "
                              f"but screens.md declares no exit there; written with target '-'")
            out.append((edge, r, target or "-"))
    return out


def poly_centroid(poly):
    a = shoelace(poly)
    if abs(a) < 1e-6:
        return (sum(p[0] for p in poly) / len(poly), sum(p[1] for p in poly) / len(poly))
    cx = cy = 0.0
    for i in range(len(poly)):
        x0, y0 = poly[i]
        x1, y1 = poly[(i + 1) % len(poly)]
        f = x0 * y1 - x1 * y0
        cx += (x0 + x1) * f
        cy += (y0 + y1) * f
    return (cx / (6 * a), cy / (6 * a))


def fmt_pt(p):
    return f"{int(round(p[0]))} {int(round(p[1]))}"


def write_screen_file(path, meta, polys, door_list, exits, spawn):
    L = [f"# {path.name} — generated by ./story_prompt.py ingest. Never edited by hand.",
         "# A top-down 16-bit map. Everything is in SCREEN PIXELS of the painting, which is the world.",
         f"map {meta['map']}", f"zone {meta['zone']}",
         f"size {meta['w']} {meta['h']}",
         f"paint {meta['paint']}", f"walk {meta['walk']}"]
    if meta.get("over"):
        L += ["# over: the painting's pixels that are drawn ABOVE the walker (an arch top, a bridge "
              "deck), with alpha everywhere else. Drawn after the walker, at the same position.",
              f"over {meta['over']}"]
    L += [f"# walker: how tall the player's sprite is drawn on this map, in screen pixels.",
          f"walker {meta['walker']}",
          f"spawn {fmt_pt(spawn)}",
          "# top-down: one scale over the whole map, so the walker never changes size.",
          "scale_near 1.000 at " + str(meta["h"] - 1), "scale_far 1.000 at 0", "",
          f"# nav: {len(polys)} convex polygon(s), vertices in order, walkable ground only"]
    for n, poly in enumerate(polys, 1):
        L.append(f"poly {n}: " + ", ".join(fmt_pt(p) for p in poly))
    L += ["", "# door: a threshold notch cut into a building. Nothing is bound to it yet.",
          *(f"door {x} {y} {bw} {bh}" for x, y, bw, bh in door_list),
          "", "# exit: edge x0 y0 x1 y1 target_map"]
    for edge, r, target in exits:
        L.append(f"exit {edge} {r[0]} {r[1]} {r[2]} {r[3]} {target}")
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text("\n".join(L) + "\n")


def draw_line(rows, x0, y0, x1, y1, rgb, thick=1):
    dx, dy = abs(x1 - x0), -abs(y1 - y0)
    sx, sy = (1 if x0 < x1 else -1), (1 if y0 < y1 else -1)
    err = dx + dy
    while True:
        fill_rect(rows, x0 - thick // 2, y0 - thick // 2, thick, thick, rgb)
        if x0 == x1 and y0 == y1:
            break
        e2 = 2 * err
        if e2 >= dy:
            err += dy
            x0 += sx
        if e2 <= dx:
            err += dx
            y0 += sy


def screen_debug_image(paint, w, h, polys, door_list, exits, spawn, over, out):
    """The map with the nav polygons outlined, the doors and exits marked, the overhead tinted."""
    rows = [bytearray(r) for r in paint]
    if over:
        for y in range(h):
            line, o = rows[y], y * w
            for x in range(w):
                if over[o + x]:
                    i = x * 3
                    line[i] = (line[i] + 255) // 2
                    line[i + 1] = (line[i + 1] + 180) // 2
                    line[i + 2] = (line[i + 2] + 255) // 2
    for poly in polys:
        for i in range(len(poly)):
            a, b = poly[i], poly[(i + 1) % len(poly)]
            draw_line(rows, int(a[0]), int(a[1]), int(b[0]), int(b[1]), (0, 255, 255), 2)
    for x, y, bw, bh in door_list:
        stroke_rect(rows, x - 2, y - 2, bw + 4, bh + 4, 2, (255, 200, 0))
    for _edge, r, _t in exits:
        draw_line(rows, r[0], r[1], r[2], r[3], (255, 32, 32), 9)
    sx, sy = int(spawn[0]), int(spawn[1])
    draw_line(rows, sx - 14, sy, sx + 14, sy, (255, 255, 255), 3)
    draw_line(rows, sx, sy - 14, sx, sy + 14, (255, 255, 255), 3)
    write_png(out, w, h, 3, rows)


def flat_rows(g, w, h, rgb):
    on, off = bytes(rgb), b"\x00\x00\x00"
    return [bytearray(b"".join(on if g[y * w + x] else off for x in range(w))) for y in range(h)]


def fit_to(rows, w, h, ch, tw, th, nearest):
    if (w, h) == (tw, th):
        return rows
    return resize_nn(rows, w, h, ch, tw, th) if nearest else resize_box(rows, w, h, ch, tw, th)


def screen_returns(data, folder):
    """The images the owner saved, one per step. A missing one is a hard stop, nothing is written."""
    want = {}
    for what, fname in data["returns"].items():
        f = folder / fname
        if not f.exists():
            alt = [p for p in folder.iterdir()
                   if p.stem == f.stem and p.suffix.lower() in RETURN_SUFFIXES]
            if not alt:
                die(f"`{fname}` is missing from this package. It needs "
                    f"{', '.join('`%s`' % v for v in data['returns'].values())}, one per message in "
                    f"prompt.md. Nothing written.")
            f = max(alt, key=lambda p: p.stat().st_mtime)
        want[what] = f
    return want


SCREEN_SUMMARY = []      # the one line `ingest` prints for a screen, since it swallows the rest


def cut_screen(data, folder, debug=False):
    """The map and its walkable mask -> the cleaned files and the engine's .screen."""
    SCREEN_SUMMARY.clear()
    mp, zone = data["map"], data["zone"]
    want, report = screen_returns(data, folder), []
    walker = int(data.get("walker") or SCREEN_WALKER)
    p = screen_paths(mp, zone)
    SCREENS_DIR.mkdir(parents=True, exist_ok=True)

    pw, ph, pch, prows = read_png(want["paint"])
    W = min(pw, SCREEN_MAX_W)
    H = max(1, int(round(ph * W / pw)))
    prows = fit_to(prows, pw, ph, pch, W, H, nearest=False)
    if pch == 4:
        prows = [bytearray(b for i, b in enumerate(r) if i % 4 != 3) for r in prows]
    write_png(p["paint"], W, H, 3, prows)
    print(f"map {want['paint'].name}: {pw}x{ph} -> {p['paint'].relative_to(ROOT.parent)} {W}x{H}")

    def read_mask(what, test, label):
        mw, mh, mch, mrows = read_png(want[what])
        if abs((mw / mh) / (W / H) - 1) > 0.06:
            die(f"{want[what].name} is {mw}x{mh}, the map is {W}x{H}: a different shape, so the mask "
                f"cannot be laid over it. Ask for it again, same size and framing. Nothing written.")
        mrows = fit_to(mrows, mw, mh, mch, W, H, nearest=True)
        g = mask_of(mrows, W, H, mch, test)
        print(f"{label} {want[what].name}: {mw}x{mh} -> {W}x{H}, {sum(g)} px raw")
        return g

    walk = read_mask("walk", walk_test, "walkable mask")
    if not sum(walk):
        die(f"{want['walk'].name} has no pure green in it at all. Is this the right file? Nothing written.")
    walk = morph(walk, W, H, opening=WALK_OPEN, closing=WALK_CLOSE)
    filled = fill_pinholes(walk, W, H)
    print(f"  cleaned: open {WALK_OPEN}, close {WALK_CLOSE}, {filled} pinhole(s) filled -> {sum(walk)} px")
    walk, kept = reachable_walk(walk, W, H, data.get("spawn"), data.get("exits") or {}, report, debug)
    print(f"  reachable: {sum(walk)} px in {len(kept)} island(s)")
    if not sum(walk):
        die("nothing walkable survived the cleaning: the green was all outlines and unreachable "
            "islands. Check the walkable mask. Nothing more written.")
    write_png(p["walk"], W, H, 3, flat_rows(walk, W, H, (0, 255, 0)))

    over = None
    if data.get("overhead") and "over" in want:
        over = morph(read_mask("over", fg_test, "overhead mask"), W, H, closing=1)
        if not sum(over):
            report.append("the overhead mask is entirely black: nothing on this map is walked "
                          "underneath, so no overhead layer is written")
            over = None
        else:
            cut = []
            for y in range(H):
                line, o = bytearray(W * 4), y * W
                for x in range(W):
                    if over[o + x]:
                        line[x * 4:x * 4 + 4] = bytes(prows[y][x * 3:x * 3 + 3]) + b"\xff"
                cut.append(line)
            write_png(p["over"], W, H, 4, cut)
            print(f"  overhead: {sum(over)} px -> {p['over'].relative_to(ROOT.parent)}")

    polys, eps = nav_polygons(walk, W, H, report)
    green, covered = sum(walk), sum(abs(shoelace(q)) for q in polys)
    bad = [n for n, q in enumerate(polys, 1) if not is_convex(q)]
    if bad:
        report.append(f"polygon(s) {bad} came out non-convex; the engine will not like them")
    print(f"nav: {len(polys)} convex polygon(s) at {eps:.0f} px tolerance, covering {covered:.0f} of "
          f"{green} walkable px ({100 * covered / max(1, green):.1f}%)")

    door_list = doors(walk, W, H, walker, report)
    print(f"doors: {len(door_list)}" + (": " + ", ".join(f"{x},{y} {bw}x{bh}"
                                                         for x, y, bw, bh in door_list[:8])
                                        if door_list else ""))
    exits = screen_exits(walk, W, H, data.get("exits") or {}, report)
    print("exits: " + (", ".join(f"{e} -> {t}" for e, _r, t in exits) or "none found"))

    spawn = data.get("spawn")
    if spawn:
        k = W / (data["size"][0] or W)
        spawn = (spawn[0] * k, spawn[1] * k)
        sx, sy = int(spawn[0]), int(spawn[1])
        if not (0 <= sx < W and 0 <= sy < H and walk[sy * W + sx]):
            report.append(f"screens.md puts the spawn at {sx},{sy}, which is not walkable ground on "
                          f"this map; using the middle of the biggest walkable polygon instead")
            spawn = None
    if not spawn:
        spawn = poly_centroid(max(polys, key=lambda q: abs(shoelace(q)))) if polys else (W / 2, H - 1)
    rel = lambda q: str(q.relative_to(ROOT.parent))
    meta = {"map": mp, "zone": zone, "w": W, "h": H, "walker": walker,
            "paint": rel(p["paint"]), "walk": rel(p["walk"]),
            "over": rel(p["over"]) if over else None}
    write_screen_file(p["screen"], meta, polys, door_list, exits, spawn)
    print(f"wrote {p['screen'].relative_to(ROOT.parent)}  ({len(polys)} poly, {len(door_list)} door, "
          f"{len(exits)} exit, spawn {fmt_pt(spawn)}, walker {walker} px)")
    SCREEN_SUMMARY.append(f"{W}x{H}, {len(polys)} nav polygon(s) covering "
                          f"{100 * covered / max(1, green):.1f}% of the walkable ground, "
                          f"{len(door_list)} door(s), {len(exits)} exit(s), spawn {fmt_pt(spawn)}")
    if debug:
        screen_debug_image(prows, W, H, polys, door_list, exits, spawn, over, p["debug"])
        print(f"wrote {p['debug'].relative_to(ROOT.parent)}  (nav polygons outlined, doors boxed, "
              f"exits and spawn marked)")
    for x in report:
        LOUD.append(x)
        print(f"warning: {x}", file=sys.stderr)


def cmd_screen_cut(args):
    """`ingest`'s half of a screen package: <sheet.json> <package folder> [--debug]."""
    pos = [a for a in args if not a.startswith("--")]
    if len(pos) != 2:
        die("usage: (internal) screen-cut <sheet.json> <package folder> [--debug]")
    data = json.loads(Path(pos[0]).read_text())
    if data.get("kind") != "screen":
        die(f"{pos[0]} is not a screen package (kind '{data.get('kind')}')")
    cut_screen(data, Path(pos[1]), debug="--debug" in args)


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
    """Group ids into sheets that lay out with usable slots. ([[id...]], [(id, why it never fits)]).

    An id may want several slots — a building wants three — so an entry in `sizes` is either one
    (w, h) box or a list of them, and a group is never split through the middle of an id."""
    boxes = [s if isinstance(s, list) else [s] for s in sizes]
    groups, bad, cur, cur_boxes = [], [], [], []
    for i, bs in zip(ids, boxes):
        best, why = try_layout_cells(bs)
        if best is None:
            bad.append((i, why))
            continue
        if cur and try_layout_cells(cur_boxes + bs)[0] is None:
            groups.append(cur)
            cur, cur_boxes = [], []
        cur.append(i)
        cur_boxes += bs
    if cur:
        groups.append(cur)
    return groups, bad


def frozen_packages(here, dirname, kind, ids):
    """[(folder, ids)] for packages of this kind that have already been drawn, so their slots are fixed.

    Once the owner has generated a sheet, its template and its slot boxes are the only thing that can
    cut the image they got back. Adding an id to `tiles.md` must therefore never re-shuffle that
    package: the drawn ones keep exactly the ids they were built with, and everything new goes into a
    fresh `tiles_2`. A package counts as drawn when a returned image is sitting in it, or when every
    file it makes is already on disk."""
    out = []
    for d in sorted(here.glob(f"{dirname}*")) if here.is_dir() else []:
        mf = d / PKG_META
        if not mf.exists():
            continue
        try:
            meta = json.loads(mf.read_text())
        except ValueError:
            continue
        was = meta.get("ids") or []
        if meta.get("kind") != kind or not was or any(i not in ids for i in was):
            continue                                     # not ours, or an id it drew has since gone
        if pkg_returned(d) or (meta.get("outputs") and
                               all((ROOT.parent / o).exists() for o in meta["outputs"])):
            out.append((d.name, was))
    return out


def next_package_name(dirname, used):
    if dirname not in used:
        return dirname
    n = 2
    while f"{dirname}_{n}" in used:
        n += 1
    return f"{dirname}_{n}"


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
    if meta.get("kind") == "screen":
        (d / "RETURN_HERE.md").write_text(f"""# {meta['title']} — save every image here

One chat, one message per image: the map, then the walkable mask. `prompt.md` in this folder has the
prompts in order and each one says which name to save under.

1. `{RETURNED}.png` — the top-down map
2. `{RETURNED}_walk.png` — the walkable mask (green on black)
3. `{RETURNED}_over.png` — the overhead mask, **only** if `prompt.md` has a third message

`ingest` needs every image `prompt.md` asks for and refuses until they are all here. Then, from the repository root:

```
./story_prompt.py ingest {rel}
```

Add `--debug` for a picture of what the tool understood. That writes:

{outs}

Nothing here is ever deleted, so a bad cut is redone by fixing and rerunning, and a regeneration is
just saving the new image over the old one and running `ingest` again.
""")
        return
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
    """Build one package into its folder, then drop the machine index and the return note.

    A folder that already holds a returned image is FROZEN: its `template.png` and `sheet.json` are
    put back exactly as they were after the rebuild, so the image the owner generated can always be
    cut again. Only the prose — prompt.md, RETURN_HERE.md — is refreshed. Without this, a change to a
    template's layout (the walk sheet going from 16 frames to 9) would silently invalidate every
    image already sitting in the tray."""
    d.mkdir(parents=True, exist_ok=True)
    frozen = {}
    if pkg_returned(d):
        for f in ("template.png", "sheet.json"):
            if (d / f).exists():
                frozen[f] = (d / f).read_bytes()
    with in_package(d, meta.get("title")):
        ok, msg = run_quiet(builder, args)
    for f, blob in frozen.items():
        (d / f).write_bytes(blob)
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

    # ── the tilesets: the tile field (D18), and the first thing on the page ──
    # These come first because a tmap is walkable the moment its atlas has tiles in it, with no cast,
    # no scenes and no maps drawn: it is the shortest route from nothing to a game you can walk around.
    for setname in sorted({q.parent.name for q in TILESETS.glob("*/tiles.md")}):
        try:
            tset = tileset_entries(setname)
            assign_indices(setname, tset)
            plan = tileset_plan(setname, tset)
        except SystemExit as exc:
            failures.append((f"tileset {setname}", squash(str(exc.code))))
            continue
        write_tilesets_readme()
        if set_terrains(tset):                       # generated, and regenerated whenever this runs
            write_masks(setname, [tset[t]["edge_style"] for t in set_terrains(tset)])
        for d, meta, bargs in plan:
            add(d, meta, cmd_tileset, bargs, group="tileset", tileset=setname)
        write_atlas_json(setname, tset)

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
    houses = field_entries(FIELD_DOCS["building"], "building") if FIELD_DOCS["building"].exists() else {}
    panel_scenes = [(ch, sc) for ch, sc in scenes if sc["kind"] == "panels" and sc["panels"]]
    map_chapter, default_ch = {}, min([ch for ch, _ in scenes], default=1)
    for ch, sc in scenes:
        map_chapter.setdefault(first_map(sc["map"]), ch)
    chap_of_map = lambda m: map_chapter.get(m, default_ch)

    shared = {}                    # (map, kind) -> [(id, the map whose package draws it)]
    builder = {"tiles": cmd_tiles, "props": cmd_props, "building": cmd_building}
    for kind, entries, dirname in (("tiles", tiles, "tiles"), ("props", props, "props"),
                                   ("building", houses, "buildings")):
        by_map = {}
        for i, e in entries.items():
            maps = [slug(m) for m in (e.get("map") or "").split(",") if slug(m)] or [COMMON_MAP]
            by_map.setdefault(maps[0], []).append(i)
            for other in maps[1:]:            # drawn once, used on several maps: say so, never redraw it
                shared.setdefault((other, kind), []).append((i, maps[0]))
        box_of = lambda i: ([(1, 1)] if kind == "tiles" else
                            [(1, 1)] * len(BUILDING_FACES) if kind == "building" else
                            [footprint_of(i, entries[i], [])])
        for mp in sorted(by_map):
            ids = sorted(by_map[mp])
            here = PACKAGES / chapter_label(chap_of_map(mp)).split()[0] / mp
            done, taken = frozen_packages(here, dirname, kind, ids), set()
            for _, keep in done:
                taken |= set(keep)
            rest = [i for i in ids if i not in taken]
            groups, bad = split_field_ids(rest, [box_of(i)[0] if kind != "building" else box_of(i)
                                                 for i in rest])
            for i, why in bad:
                failures.append((f"{kind} {i}", why))
            plan = list(done)
            if not done and len(groups) == 1:
                plan.append((dirname, groups[0]))
            else:
                for g in groups:                         # a new id never disturbs a sheet already drawn
                    plan.append((next_package_name(dirname, {n for n, _ in plan}), g))
            for n, (folder, group) in enumerate(plan, 1):
                d = here / folder
                outs = ([str((FIELD_DIRS["building"] / f"{i}_{f}.png").relative_to(ROOT.parent))
                         for i in group for f in BUILDING_FACES] if kind == "building" else
                        [str((FIELD_DIRS[KIND_DIR[kind]] / f"{i}.png").relative_to(ROOT.parent)) for i in group])
                what = "buildings" if kind == "building" else kind
                add(d, {"kind": kind, "map": mp, "ids": group,
                        "title": f"{mp} — {len(group)} {what}" + (f" ({n} of {len(plan)})" if len(plan) > 1 else ""),
                        "makes": (", ".join(f"`{i}`" for i in group) + " — front, side and roof each"
                                  if kind == "building" else ", ".join(f"`{o}`" for o in outs)),
                        "outputs": outs},
                    builder[kind], list(group), group=kind, map=mp, chapter=chap_of_map(mp))

    # ── the painted screens: the main field path, so they come first under each map ──
    ok_screens, why = True, ""
    try:
        screens = screen_entries() if SCREENS_DOC.exists() else {}
    except SystemExit as exc:
        ok_screens, screens, why = False, {}, squash(str(exc.code))
    if not ok_screens:
        failures.append((str(SCREENS_DOC.relative_to(ROOT.parent)), why))
    for key in sorted(screens):
        e = screens[key]
        mp, zone = e["map"], e["zone"]
        d = PACKAGES / chapter_label(chap_of_map(mp)).split()[0] / mp / "screens" / zone
        outs = screen_outputs(mp, zone, e["overhead"])
        add(d, {"kind": "screen", "map": mp, "zone": zone, "overhead": e["overhead"],
                "title": f"{mp} / {zone} — painted map",
                "makes": f"the top-down map, its cleaned walkable mask and `{outs[-1]}`",
                "outputs": outs},
            cmd_screen, [mp, zone], group="screen", map=mp, chapter=chap_of_map(mp), zone=zone)

    # ── one painted background per camera zone the engine has captured ──
    for mp, zone, cap in captures():
        d = PACKAGES / chapter_label(chap_of_map(mp)).split()[0] / mp / "views" / zone
        out = str(paint_path(mp, zone).relative_to(ROOT.parent))
        add(d, {"kind": "view", "map": mp, "zone": zone, "title": f"{mp} / {zone} — painted view",
                "makes": f"`{out}`, painted over `{cap.relative_to(ROOT.parent)}`", "outputs": [out]},
            cmd_view, [mp, zone], group="view", map=mp, chapter=chap_of_map(mp), zone=zone)

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
    for g, label in (("tileset", "tile sheets (tilesets)"), ("refsheet", "reference sheets"), ("walker", "walk sheets"),
                     ("screen", "painted screens"), ("tiles", "tile sheets"),
                     ("props", "prop sheets"), ("building", "building face sheets"),
                     ("view", "painted views"), ("scene", "scene shot sheets")):
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

    # ── the tilesets, first on the page: this is the field now (TILES.md, D18) ──
    tsets = [r for r in index if r["group"] == "tileset"]
    L += ["## 1. Tilesets — the tile field", "",
          "The field is tile sheets and grid walking (`TILES.md`). **Start with the two sheets at the",
          "top of this table.** Under TILES2 (D20) a terrain is ONE seamless **swatch** and its edges",
          "are computed, so the swatch sheet is the whole ground of the world in one generation; the",
          "**decal** sheet is the variety scattered over it — tufts, flowers, pebbles, reeds — and",
          "between them they replace every transition tile that used to have to match its neighbours.",
          "The stamp sheets after them are unchanged: drawn at 4x, a 32x32 tile in a 128-px slot,",
          "packed into the set's `atlas.png` at the index `tiles.md` gives it. A sheet that has been",
          "generated is frozen; new ids go into a fresh `<sheet>_2` beside it.", "",
          "The masks are **generated by the tool** into `<set>/masks.png` — there is nothing to draw",
          "and nothing to review. The conventions are in `story/field/tilesets/README.md` and the",
          "contract is `TILES.md`.", "",
          "| set | sheet | slots | folder | status | after downloading |",
          "| --- | --- | --- | --- | --- | --- |"]
    order = {"swatch": 0, "decal": 1, "tileset": 2}
    for r in sorted(tsets, key=lambda r: (order.get(r["meta"].get("kind"), 3), r["dir"])):
        m = r["meta"]
        what = m.get("sheet") or {"swatch": "swatches (the ground)",
                                  "decal": "decals (the variety)"}.get(m.get("kind"), m.get("kind"))
        L.append(f"| `{r['tileset']}` | `{what}` | {len(m['ids'])} | "
                 f"[`{rel(r['dir'])}`]({rel(r['dir'])}/prompt.md) | {state(r)} | {cmd(r['dir'])} |")
    if not tsets:
        L.append("| — | | | | no `story/field/tilesets/<set>/tiles.md` yet | |")
    L.append("")

    # ── the short path: the fewest generations that put something on the phone ──
    short, seen = [], set()
    def step(r, what):
        if r and r["dir"] not in seen:
            seen.add(r["dir"])
            short.append((r, what))
    todo_first = lambda rs: sorted(rs, key=lambda r: pkg_state(r["dir"], r["meta"])[0] == "done")
    for r in todo_first(sorted(rows("tileset"),
                               key=lambda r: order.get(r["meta"].get("kind"), 3))):
        m = r["meta"]
        if m.get("kind") == "swatch":
            step(r, f"the `{r['tileset']}` ground swatches — {len(m['ids'])} terrains in one image, "
                    f"and every edge between them is computed from it. This is the whole ground of "
                    f"the world.")
        elif m.get("kind") == "decal":
            step(r, f"the `{r['tileset']}` decals — {len(m['ids'])} cut-outs the game scatters over "
                    f"the ground, which is what stops it looking like a sheet of one colour")
        else:
            step(r, f"the `{m['sheet']}` sheet of the `{r['tileset']}` tileset — "
                    f"{len(m['ids'])} tile(s) into that set's atlas, which the map is built from")
    for r in todo_first(rows("refsheet")):       # what is left to do first, then the ones already cut
        step(r, f"{r['who']}'s reference sheet — the portrait, the walker and every panel come from it")
    for r in todo_first([x for x in rows("walker") if not x.get("blocked") and x["meta"].get("handle") in cast]):
        step(r, f"{r['who']}'s walk sprite — the figure walking the map")
    for r in [x for x in rows("screen") if x["map"] == lead_map]:
        step(r, f"the top-down map `{r['zone']}` of `{lead_map}` — the whole place in one picture, "
                f"which is what the player walks around in")
    for g, what in (("tiles", "the ground and walls of {m}"), ("props", "everything standing in {m}"),
                    ("building", "the front, wall and roof of every building in {m}")):
        for r in [x for x in index if x["group"] == g and x["map"] == lead_map]:
            part = re.search(r"\((\d+) of (\d+)\)", r["meta"].get("title", ""))
            step(r, what.format(m=f"`{lead_map}`") + (f", sheet {part.group(1)} of {part.group(2)}"
                                                      if part else ""))
    for r in [x for x in rows("scene") if x["map"] == lead_map]:
        step(r, f"the shot sheet for `{r['scene']['stem']}` — the first scene the game plays")
    L += [f"## 2. The short path to something on the phone", "",
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

    L += ["## 3. Character reference sheets", "",
          "Do these first: the portrait beside the dialogue box, the walk sprite, and every scene panel",
          "are all drawn from this one image.", "",
          "| character | folder | status | after downloading |", "| --- | --- | --- | --- |"]
    for r in rows("refsheet"):
        L.append(f"| {r['who']} | [`{rel(r['dir'])}`]({rel(r['dir'])}/prompt.md) | {state(r)} | {cmd(r['dir'])} |")
    if not rows("refsheet"):
        L.append("| — | | no cast in the selected scenes | |")

    L += ["", "## 4. Walk sprites", "",
          "The 4x4 sheet the field renderer animates (rows S, W, E, N). A character's package is blocked",
          "until their reference sheet exists, because the walker must match it.", "",
          "| who | folder | status | after downloading |", "| --- | --- | --- | --- |"]
    for r in rows("walker"):
        link = f"[`{rel(r['dir'])}`]({rel(r['dir'])}/prompt.md)" if r["ok"] else f"`{rel(r['dir'])}`"
        after = "rerun `packages` once the sheet is in" if r.get("blocked") else cmd(r["dir"])
        L.append(f"| {r['who']} | {link} | {state(r)} | {after} |")
    if not rows("walker"):
        L.append("| — | | | |")

    field = [r for r in index if r["group"] in ("tiles", "props", "building")]
    L += ["", "## 5. Field art, by chapter and map", "",
          "A **screen** is the main path: one top-down painting of the whole map plus its walkable mask,",
          "and the game is fitted to it. Tiles, props and buildings are the older sprite path — tiles are the big surfaces",
          "the ground is made of; a building is map geometry wearing three textures (the front wall, a wall",
          "sample, a roof sample); everything else is a prop sprite.",
          f"A map named `{COMMON_MAP}` means the entry carries no `- map:` line yet.", ""]
    for mp, ch in sorted(all_maps.items(), key=lambda kv: (kv[1], kv[0])):
        mine = [x for x in field if x["map"] == mp]
        borrowed = {k: shared.get((mp, k), []) for k in ("tiles", "props", "building")}
        screens = [x for x in index if x["group"] == "screen" and x["map"] == mp]
        if not mine and not screens and not any(borrowed.values()) \
                and not any(x["group"] == "view" and x["map"] == mp for x in index):
            continue                                  # a map with no field art of its own yet
        mapfile = FIELD / "maps" / f"{mp}.map"
        L += [f"### {chapter_label(ch)} — {mp}" + ("" if mapfile.exists() else "  (no map file yet)"), ""]
        if screens:
            L += ["**Screens — the main path.** ChatGPT paints the whole map, top-down and 16-bit, from",
                  "its entry in `story/field/screens.md`, then traces a green-on-black walkable mask over it",
                  "in the same chat, and the game is fitted to the painting: the mask becomes the nav",
                  "polygons, the doorway notches and the exits. One more mask, only for a map with something",
                  "the player walks underneath.", "",
                  "| zone | folder | status | after downloading |", "| --- | --- | --- | --- |"]
            for r in screens:
                L.append(f"| `{r['zone']}` | [`{rel(r['dir'])}`]({rel(r['dir'])}/prompt.md) | "
                         f"{state(r)} | {cmd(r['dir'])} |")
            L.append("")
        L += ["Tiles, props and buildings below are the sprite path, which a painted screen does not need.",
              "" ] if screens and mine else []
        L += ["| package | makes | status | after downloading |", "| --- | --- | --- | --- |"]
        for r in mine:
            L.append(f"| [`{rel(r['dir'])}`]({rel(r['dir'])}/prompt.md) | {r['meta']['makes']} | "
                     f"{state(r)} | {cmd(r['dir'])} |")
        if not mine:
            L.append("| — | nothing in tiles.md or props.md calls this map home | | |")
        L.append("")
        views = [x for x in index if x["group"] == "view" and x["map"] == mp]
        if views:
            L += ["**Painted views** — the block-out the engine captured, repainted as a finished "
                  "background and laid back over it. Nothing in the picture may move.", "",
                  "| zone | folder | status | after downloading |", "| --- | --- | --- | --- |"]
            for r in views:
                L.append(f"| `{r['zone']}` | [`{rel(r['dir'])}`]({rel(r['dir'])}/prompt.md) | "
                         f"{state(r)} | {cmd(r['dir'])} |")
            L.append("")
        for kind, ids in borrowed.items():
            if ids:
                L += [f"Shared {'buildings' if kind == 'building' else kind}, drawn with another map "
                      f"so one id is never drawn twice: "
                      + ", ".join(f"`{i}` (with `{home}`)" for i, home in sorted(ids)), ""]
    if not field:
        L += ["(nothing in `story/field/tiles.md` or `story/field/props.md` yet)", ""]

    scene_rows = rows("scene")
    L += ["## 6. Scene shot sheets, by chapter and map", "",
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


# ───────────────────────── Palette (D19): 256 colours, one byte a pixel ─────────────────────────
#
# PALETTE.md is the contract. Everything the game ships is an indexed PNG (colour type 3) drawn from
# one of two palettes: the master, shared by everything that can be on screen together in the field,
# and a per-scene palette for cutscene panels, which are never lit and need their own skies.
#
# Index 0 is transparent, 1 is pure black, 2 is pure white, and the other 253 are material ramps
# (variable length) plus the corpus colours the ramps serve worst. A ramp does not spend slots on
# near-black or near-white — that is what 1 and 2 are for — its shadows rotate toward blue-violet and
# its highlights toward yellow, because a ramp that is a straight line through Oklab goes muddy the
# moment the colormap darkens it.

PALETTE_DIR = ROOT / "palette"
MASTER_HEX = PALETTE_DIR / "master.hex"
MASTER_JSON = PALETTE_DIR / "master.json"
MASTER_SWATCH = PALETTE_DIR / "master_swatch.png"
MASTER_PAL_PNG = PALETTE_DIR / "master.pal.png"
COLORMAP_PNG = PALETTE_DIR / "colormap.png"
COLORMAP_JSON = PALETTE_DIR / "colormap.json"
CYCLES_MD = PALETTE_DIR / "cycles.md"
PALETTE_VERSION = 2           # v2 (2026-09-19): refitted to the owner's flat luminous field style,
                              # and the first version to reserve contiguous ramps for palette cycling
PAL_SIZE = 256
PAL_TRANSPARENT, PAL_BLACK, PAL_WHITE = 0, 1, 2
PAL_FREE = PAL_SIZE - 3               # 253 colours the build gets to choose
MERGE_DE = 0.015                      # two palette entries this close in Oklab are one colour
LEVELS = 32                           # light levels a colormap table carries

# The material families. (name, hue anchor in Oklab degrees or None for a neutral, class) — the class
# sets how many slots the family may have, and the corpus's own pixel share distributes the class
# budget inside those bounds. "long" is for the materials a whole map is made of, where a missing
# step reads as a band; "short" is for a hue that turns up on one object.
RAMP_CLASSES = {"long": (16, 24, 104), "med": (10, 12, 50), "short": (6, 8, 24)}
RAMP_FAMILIES = [
    ("roof red / brick",             32,   "long"),
    ("orange / terracotta (hair)",   45,   "med"),
    ("skin",                         57,   "long"),
    ("wood",                         68,   "long"),
    ("earth / dirt",                 78,   "long"),
    ("gold / blond",                 90,   "med"),
    ("olive / dry grass",            108,  "short"),
    ("foliage green (warm)",         135,  "long"),
    ("foliage green (cool)",         158,  "long"),
    ("teal / shallow water",         205,  "short"),
    ("sky / cyan",                   238,  "med"),
    ("water blue / metal",           262,  "med"),
    ("purple / shadow",              300,  "short"),
    ("neutral warm (plaster, cloth)", None, "med"),
    ("neutral cool (stone, steel)",   None, "long"),
]
# Palette cycling needs its indices CONTIGUOUS and its own: the engine rewrites those columns of the
# colormap every frame, so an index shared with a roof tile would make the roof flicker. Reserved
# immediately after black and white, before any ramp, and recorded in master.json and cycles.md.
# (name, family the colours come from, length, fps, lightness window, what it is)
CYCLE_RAMPS = (
    ("water",        "water blue / metal",        8, 6,  (0.34, 0.66),
     "the light moving on running water"),
    ("fire_lamp",    "orange / terracotta (hair)", 8, 10, (0.46, 0.86),
     "a fire, a lantern, a forge: the warm end flickering"),
    ("foliage_wind", "foliage green (warm)",       6, 4,  (0.38, 0.64),
     "leaves turning in the wind, a slow shimmer through a canopy"),
    ("sparkle",      "sky / cyan",                 4, 12, (0.72, 0.95),
     "a glint on water or metal, and anything that has to twinkle"),
)
NEUTRAL_C = 0.030                     # chroma under this is a neutral, not a hue
RAMP_LO, RAMP_HI = 0.16, 0.93         # a ramp's lightness range: black and white are indices 1 and 2
SHADOW_HUE, HILITE_HUE = 295.0, 95.0  # shadows rotate toward blue-violet, highlights toward yellow

# Where shipped art lives. `palette check` walks these; everything in them must be an indexed PNG.
SHIPPED_MASTER = ("field/tilesets", "field/walkers", "field/props", "field/tiles",
                  "field/buildings", "portraits")
SHIPPED_SCENE = "panels"


# ── the hex file ──

def read_hex(path):
    """[(r, g, b)] from a '# version N' hex file. The version line is the palette's identity."""
    if not Path(path).exists():
        die(f"{Path(path)} not found. Build it with: ./story_prompt.py palette build")
    cols, ver = [], None
    for line in Path(path).read_text().splitlines():
        line = line.strip()
        if line.startswith("#") and not re.fullmatch(r"#[0-9a-fA-F]{6}", line):
            m = re.match(r"#\s*version\s+(\d+)", line)
            if m:
                ver = int(m.group(1))
            continue
        if not line:
            continue
        if not re.fullmatch(r"#[0-9a-fA-F]{6}", line):
            die(f"{Path(path).name}: '{line}' is not a #rrggbb colour")
        cols.append(tuple(int(line[i:i + 2], 16) for i in (1, 3, 5)))
    if ver is None:
        die(f"{Path(path).name} has no '# version N' first line; it is not a palette this tool wrote")
    if len(cols) != PAL_SIZE:
        die(f"{Path(path).name} holds {len(cols)} colours, not {PAL_SIZE}")
    return cols


def write_hex(path, pal, version=PALETTE_VERSION):
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(f"# version {version}\n" + "".join("#%02x%02x%02x\n" % tuple(c) for c in pal))


_MASTER = None


def master_palette():
    global _MASTER
    if _MASTER is None:
        _MASTER = read_hex(MASTER_HEX)
    return _MASTER


def scene_hex(scene):
    return PANELS / f"{scene}.hex"


# ── exact nearest colour in Oklab ──

class PalIndex:
    """Nearest palette index for an sRGB colour, exact, in Oklab.

    Exact and fast enough to be the last step of every cut: the palette is sorted by lightness, and
    |dL| is a lower bound on the distance, so a scan outward from the query's lightness stops as soon
    as the lightness gap alone exceeds the best distance so far. Two dozen candidates out of 253 is
    typical. Every unique colour in an image is looked up once and cached — a 1.5 MP sheet holds a
    few hundred thousand of them, so the cache is what keeps this in seconds rather than minutes."""

    def __init__(self, pal):
        self.pal = list(pal)
        self.labs = [_to_oklab(*c) for c in self.pal]
        self.order = sorted(range(1, len(self.pal)), key=lambda i: self.labs[i][0])
        self.Ls = [self.labs[i][0] for i in self.order]
        self.cache = {}
        self.probes = 0
        self.lookups = 0

    def of(self, r, g, b):
        k = (r << 16) | (g << 8) | b
        got = self.cache.get(k)
        if got is not None:
            return got
        L, A, B = _to_oklab(r, g, b)
        order, Ls, labs, n = self.order, self.Ls, self.labs, len(self.order)
        lo = bisect.bisect_left(Ls, L)
        hi, best, bd = lo, order[lo if lo < n else n - 1], 1e9
        while True:
            moved = False
            if hi < n:
                d = Ls[hi] - L
                if d * d < bd:
                    i = order[hi]
                    q = labs[i]
                    dd = (L - q[0]) ** 2 + (A - q[1]) ** 2 + (B - q[2]) ** 2
                    if dd < bd:
                        bd, best = dd, i
                    hi += 1
                    moved = True
                    self.probes += 1
            if lo > 0:
                d = L - Ls[lo - 1]
                if d * d < bd:
                    lo -= 1
                    i = order[lo]
                    q = labs[i]
                    dd = (L - q[0]) ** 2 + (A - q[1]) ** 2 + (B - q[2]) ** 2
                    if dd < bd:
                        bd, best = dd, i
                    moved = True
                    self.probes += 1
            if not moved:
                break
        self.lookups += 1
        self.cache[k] = best
        return best


def palettise(rows, w, h, ch, pal, idx=None):
    """RGB(A) rows -> one byte a pixel. Alpha is HARD: under 0.5 is index 0, nothing else is.

    Un-matting has already happened in the keying, so an edge pixel that survives carries its own
    colour and takes its own nearest entry. No dithering, anywhere: every dither mode tested made
    this cel-shaded art worse, and an ordered pattern is exactly what a palette-cycled colormap
    turns into a crawling texture."""
    idx = idx or PalIndex(pal)
    out = []
    of = idx.of
    for y in range(h):
        row, o = rows[y], bytearray(w)
        if ch == 4:
            for x in range(w):
                i = x * 4
                if row[i + 3] >= 128:
                    o[x] = of(row[i], row[i + 1], row[i + 2])
        else:
            for x in range(w):
                i = x * 3
                o[x] = of(row[i], row[i + 1], row[i + 2])
        out.append(o)
    return out


def write_indexed_png(path, w, h, index_rows, pal):
    """Colour type 3: PLTE + a tRNS of one byte, so index 0 alone is transparent."""
    def chunk(kind, body):
        return struct.pack(">I", len(body)) + kind + body + struct.pack(">I", zlib.crc32(kind + body))
    plte = b"".join(bytes(c) for c in pal) + b"\x00" * 3 * (PAL_SIZE - len(pal))
    raw = b"".join(b"\x00" + bytes(r) for r in index_rows)
    Path(path).parent.mkdir(parents=True, exist_ok=True)
    Path(path).write_bytes(
        b"\x89PNG\r\n\x1a\n"
        + chunk(b"IHDR", struct.pack(">IIBBBBB", w, h, 8, 3, 0, 0, 0))
        + chunk(b"PLTE", plte) + chunk(b"tRNS", b"\x00")
        + chunk(b"IDAT", zlib.compress(raw, 9)) + chunk(b"IEND", b""))


PALETTE_OFF = False       # tools/palette/proof.py: write the cut in full colour beside the shipped
                          # file, as <name>.raw.png, so the two can be looked at side by side


def ship_png(path, w, h, ch, rows, pal=None, idx=None, note=True):
    """The last step of every cutter: palettise and write the indexed PNG the game ships."""
    if PALETTE_OFF:
        write_png(Path(path).with_suffix(".raw.png"), w, h, ch, rows)
        return None
    pal = pal or master_palette()
    t0 = time.time()
    index_rows = palettise(rows, w, h, ch, pal, idx)
    write_indexed_png(path, w, h, index_rows, pal)
    if note:
        PAL_TIMING.append((Path(path).name, w * h, time.time() - t0))
    return index_rows


PAL_TIMING = []


# ── building the master from the corpus ──

# What the field is made of NOW. The v1 corpus was every sheet in the tray at equal weight, which
# meant the old noisy tile style — the one the owner replaced — set most of the palette, and the flat
# luminous greens of the new swatches had almost nothing near them. v2 weights by what the screen is
# actually going to be made of: the approved style test and the new ground art lead, the cast comes
# next (a face is on screen as much as the ground is), and the old stamps that are still in the atlas
# are kept at a low weight — they have to stay drawable, but they must not choose the greens.
CORPUS_WEIGHTS = (
    (("tilesets-valley-decals.png",), 6.0, "the decals: small, saturated, and few pixels each"),
    (("tests/clean_style_test_v1.png", "tilesets-valley-swatches.png"), 3.0,
     "the new flat style: the approved style test and the ground swatches"),
    (("cast-", "refs/"), 2.0, "the cast: walk sheets, reference sheets, portraits"),
    (("tilesets-valley-objects", "ch01-"), 0.5, "the stamps and props already in the atlas"),
)


def palette_corpus():
    """[(path, weight)] — the raw returns the master is fitted to, and how much each one counts.

    Cutscene panels are never in it: they get their own palette per scene (PALETTE.md), so letting
    their skies and their lamplight in would spend the field's slots on colours the field never uses.
    Screens, walk masks and the retired terrain sheet are out too."""
    files = sorted(SHEETS.rglob("*.png")) + sorted((ROOT / "refs").glob("*.png"))
    out = []
    for f in files:
        rel = str(f.relative_to(SHEETS)) if f.is_relative_to(SHEETS) else "refs/" + f.name
        if f.name.startswith("_") or f.stem.endswith("_walk") or f.stem.endswith("_prev"):
            continue
        if "-scenes-" in f.name or f.stem.startswith("003_the_warning") or "-screens-" in f.name:
            continue
        if f.name == "tilesets-valley-terrain.png":     # the retired noisy terrain sheet: D20 killed it
            continue
        if f.name.startswith("walker16-"):              # archived 16-frame sheets duplicate the cast
            continue
        if f.parent.name == "refs" and f.stem == "style":   # third-party screenshot, not our art
            continue
        w = next((wt for keys, wt, _ in CORPUS_WEIGHTS if any(k in rel for k in keys)), 1.0)
        out.append((f, w))
    return out


def corpus_keep(r, g, b):
    """Drop the template's magenta, its white borders and numbers, and the pure sheet background."""
    if min(r, b) - g > 40 and r > 90 and b > 90:
        return False
    if r > 236 and g > 236 and b > 236:
        return False
    if r < 12 and g < 12 and b < 12:
        return False
    return True


def palette_thumb(path, width=260):
    """A sips-downscaled copy, cached. The corpus is 60 MP of PNG; the histogram does not need it."""
    cache = OUT / "_palcache"
    cache.mkdir(parents=True, exist_ok=True)
    dst = cache / (path.parent.name + "__" + path.name)
    if not dst.exists() or dst.stat().st_mtime < path.stat().st_mtime:
        subprocess.run(["sips", "--resampleWidth", str(width), str(path), "--out", str(dst)],
                       check=True, capture_output=True)
    return dst


def palette_histogram(paths, thumbs=True, keep=corpus_keep, drop_bg=False):
    """[(L, a, b, weight, r, g, b)] over 5-bit RGB buckets, each bucket's true mean colour.

    `drop_bg` throws away each sheet's own background before counting: a template's flat grey or the
    neutral card behind a reference sheet is a third of its pixels and none of its art, and left in
    it buys the plaster ramp a dozen slots to describe one colour nobody ever draws."""
    h = {}
    for entry in paths:
        p, weight = entry if isinstance(entry, tuple) else (entry, 1.0)
        w, ht, ch, rows = read_png(palette_thumb(p) if thumbs else p)
        one = {}
        for y in range(ht):
            row = rows[y]
            for x in range(0, w * ch, ch):
                r, g, b = row[x], row[x + 1], row[x + 2]
                if ch == 4 and row[x + 3] < 128:
                    continue
                if keep and not keep(r, g, b):
                    continue
                k = (r >> 3) << 10 | (g >> 3) << 5 | (b >> 3)
                e = one.get(k)
                if e is None:
                    one[k] = [1, r, g, b]
                else:
                    e[0] += 1
                    e[1] += r
                    e[2] += g
                    e[3] += b
        if drop_bg and one:
            tot = sum(e[0] for e in one.values())
            top = max(one, key=lambda k: one[k][0])
            e = one[top]
            lab = _to_oklab(*(int(v / e[0]) for v in e[1:]))
            if e[0] > 0.18 * tot and math.hypot(lab[1], lab[2]) < 0.045:
                for k in [k for k in one
                          if ((lambda q: (q[0] - lab[0]) ** 2 + (q[1] - lab[1]) ** 2
                               + (q[2] - lab[2]) ** 2)(_to_oklab(*(int(v / one[k][0])
                                                                   for v in one[k][1:])))) < 0.0004]:
                    del one[k]
        for k, e in one.items():
            e.append(e[0] * weight)                      # [pixels, sr, sg, sb, weighted pixels]
            g0 = h.get(k)
            if g0 is None:
                h[k] = e
            else:
                for i in range(5):
                    g0[i] += e[i]
    pts = []
    for n, sr, sg, sb, wn in h.values():
        # the colour is the bucket's true mean over its real pixels; the WEIGHT is what k-means, the
        # ramp budget and the refill see, so a corpus weight moves attention without moving colour.
        r, g, b = sr / n, sg / n, sb / n
        L, A, B = _to_oklab(int(r), int(g), int(b))
        pts.append((L, A, B, wn, r, g, b))
    return pts


def _lerp_ang(a, b, t):
    d = math.atan2(math.sin(b - a), math.cos(b - a))
    return a + d * t


def hue_families(pts):
    """Circular k-means on Oklab hue, seeded on the material anchors, for the chromatic buckets."""
    seeds = [f for f in RAMP_FAMILIES if f[1] is not None]
    cents = [math.radians(f[1]) for f in seeds]
    groups = [[] for _ in cents]
    for _ in range(12):
        groups = [[] for _ in cents]
        for p in pts:
            a = math.atan2(p[2], p[1])
            best, bd = 0, 9.0
            for j, c in enumerate(cents):
                d = abs(math.atan2(math.sin(a - c), math.cos(a - c)))
                if d < bd:
                    bd, best = d, j
            groups[best].append(p)
        for j, g in enumerate(groups):
            if not g:
                continue
            sx = sum(math.cos(math.atan2(q[2], q[1])) * q[3] for q in g)
            sy = sum(math.sin(math.atan2(q[2], q[1])) * q[3] for q in g)
            if sx or sy:
                cents[j] = math.atan2(sy, sx)
    return {seeds[i][0]: g for i, g in enumerate(groups)}


def _thirds(g):
    """Weighted (L, chroma, hue) of a family's dark, mid and light thirds — the ramp's spine."""
    g = sorted(g, key=lambda p: p[0])
    tot = sum(p[3] for p in g) or 1
    parts, acc, part = [], 0.0, []
    for p in g:
        part.append(p)
        acc += p[3]
        if acc >= tot / 3 and len(parts) < 2:
            parts.append(part)
            part, acc = [], 0.0
    parts.append(part)
    while len(parts) < 3:
        parts.append(parts[-1] or [g[0]])
    res = []
    for part in parts:
        part = part or [g[0]]
        wsum = sum(p[3] for p in part) or 1
        L = sum(p[0] * p[3] for p in part) / wsum
        ax = sum(p[1] * p[3] for p in part) / wsum
        by = sum(p[2] * p[3] for p in part) / wsum
        res.append((L, math.hypot(ax, by), math.atan2(by, ax)))
    return res


def build_ramp(g, n, lo=RAMP_LO, hi=RAMP_HI):
    """n steps through one family's own colours, shadows blue-violet, highlights yellow."""
    t3 = _thirds(g)
    Ls = [t3[0][0], t3[1][0], t3[2][0]]
    sh, hl = math.radians(SHADOW_HUE), math.radians(HILITE_HUE)
    out = []
    for i in range(n):
        u = i / max(1, n - 1)
        L = lo + (hi - lo) * u
        if L <= Ls[0]:
            C, H = t3[0][1], t3[0][2]
        elif L <= Ls[1]:
            t = (L - Ls[0]) / max(1e-6, Ls[1] - Ls[0])
            C = t3[0][1] + (t3[1][1] - t3[0][1]) * t
            H = _lerp_ang(t3[0][2], t3[1][2], t)
        elif L <= Ls[2]:
            t = (L - Ls[1]) / max(1e-6, Ls[2] - Ls[1])
            C = t3[1][1] + (t3[2][1] - t3[1][1]) * t
            H = _lerp_ang(t3[1][2], t3[2][2], t)
        else:
            C, H = t3[2][1], t3[2][2]
        mid = (Ls[0] + Ls[2]) / 2
        if L < mid:
            k = min(1.0, (mid - L) / max(1e-6, mid - lo))
            H = _lerp_ang(H, sh, 0.22 * k)
            C *= 1.0 - 0.45 * k * k
        else:
            k = min(1.0, (L - mid) / max(1e-6, hi - mid))
            H = _lerp_ang(H, hl, 0.18 * k)
            C *= 1.0 - 0.55 * k * k
        out.append(tuple(_from_oklab(L, C * math.cos(H), C * math.sin(H))))
    return out


def ramp_lengths(groups):
    """Each class's slot budget, split between its families by pixel share, inside the class bounds."""
    lens = {}
    for cls, (lo, hi, budget) in RAMP_CLASSES.items():
        fams = [f for f in RAMP_FAMILIES if f[2] == cls]
        share = {f[0]: max(1.0, sum(p[3] for p in groups.get(f[0], []))) for f in fams}
        root = {k: math.sqrt(v) for k, v in share.items()}
        tot = sum(root.values()) or 1.0
        raw = {k: budget * v / tot for k, v in root.items()}
        got = {k: max(lo, min(hi, int(round(v)))) for k, v in raw.items()}
        # hand back or take up the rounding and the clamping, largest share first
        order = sorted(fams, key=lambda f: -share[f[0]])
        while sum(got.values()) != budget:
            step = 1 if sum(got.values()) < budget else -1
            moved = False
            for f in (order if step > 0 else order[::-1]):
                if lo <= got[f[0]] + step <= hi:
                    got[f[0]] += step
                    moved = True
                    break
            if not moved:
                break
        lens.update(got)
    return lens


def cmd_palette_build(args):
    """story/palette/master.* — the 256 colours everything in the field is drawn from."""
    t0 = time.time()
    files = palette_corpus()
    if not files:
        die(f"no raw returns in {SHEETS.relative_to(ROOT.parent)}; there is nothing to fit a palette to")
    by_w = {}
    for _, wt in files:
        by_w[wt] = by_w.get(wt, 0) + 1
    print(f"corpus: {len(files)} sheet(s), weighted "
          + ", ".join(f"{n} at x{w:g}" for w, n in sorted(by_w.items(), reverse=True))
          + " (cutscene sheets excluded: panels get their own palette per scene)")
    pts = palette_histogram(files, drop_bg=True)
    total = sum(p[3] for p in pts)
    chromatic = [p for p in pts if math.hypot(p[1], p[2]) >= NEUTRAL_C]
    neutral = [p for p in pts if math.hypot(p[1], p[2]) < NEUTRAL_C]
    groups = hue_families(chromatic)
    groups["neutral warm (plaster, cloth)"] = [p for p in neutral if p[2] >= 0]
    groups["neutral cool (stone, steel)"] = [p for p in neutral if p[2] < 0]
    lens = ramp_lengths(groups)

    ramps, meta = [], []
    for name, _, cls in RAMP_FAMILIES:
        g = groups.get(name) or pts
        n = lens[name]
        ramps.append((name, build_ramp(g, n), cls, sum(p[3] for p in groups.get(name, []))))
    # the reserved three, then the CYCLE blocks, then the ramps, merging what is too close to tell
    # apart. The cycle blocks go first because they are the only entries whose *index* matters.
    pal, labs, spans, dropped = [(0, 0, 0), (0, 0, 0), (255, 255, 255)], [], [], 0
    for c in pal[1:]:
        labs.append(_to_oklab(*c))
    cycles = []
    for cname, fam, n, fps, (lo, hi), what in CYCLE_RAMPS:
        g = groups.get(fam) or pts
        start = len(pal)
        for c in build_ramp(g, n, lo, hi):
            pal.append(tuple(c))
            labs.append(_to_oklab(*c))
        cycles.append({"name": cname, "family": fam, "start": start, "len": n, "fps": fps,
                       "what": what})
        print(f"  cycle {cname:13s} {n} indices {start}-{start + n - 1} at {fps} fps, from {fam}")
    for name, ramp, cls, px in ramps:
        # A ramp keeps every step it asked for, but a step within MERGE_DE of a colour the palette
        # already holds SHARES that index instead of spending a new one. Skin, wood and earth really
        # are the same browns in this corpus: saying so costs nothing and frees the slot for a colour
        # nothing else serves. So a ramp is a list of indices, not a range, and two ramps may overlap.
        got, own = [], 0
        for c in ramp:
            lab = _to_oklab(*c)
            hit = next((j for j, q in enumerate(labs)
                        if ((lab[0] - q[0]) ** 2 + (lab[1] - q[1]) ** 2 + (lab[2] - q[2]) ** 2) ** 0.5
                        < MERGE_DE), None)
            if hit is not None:
                got.append(hit + 1)                     # labs[0] is index 1: index 0 is transparent
                dropped += 1
                continue
            got.append(len(pal))
            pal.append(tuple(c))
            labs.append(lab)
            own += 1
        spans.append({"name": name, "class": cls, "indices": got, "len": len(got), "own": own,
                      "corpus_px": int(px), "share": round(px / max(1, total), 4)})
        print(f"  {name:32s} {cls:5s} {len(ramp):2d} steps, {own:2d} of its own  "
              f"{100 * px / max(1, total):5.1f}% of the corpus")
    print(f"  {dropped} near-duplicate(s) merged (dE < {MERGE_DE}); "
          f"{PAL_SIZE - len(pal)} slot(s) go to the colours the ramps serve worst")

    # refill: the corpus colours furthest from anything the ramps offer, weighted by how much of the
    # corpus they are. This is what keeps a ramp palette honest against a purely statistical one.
    extra_start = len(pal)
    # each bucket keeps its own distance to the nearest palette entry, updated against the one colour
    # just added, so this is one pass over the corpus a slot instead of a full re-scan.
    near = [min((p[0] - q[0]) ** 2 + (p[1] - q[1]) ** 2 + (p[2] - q[2]) ** 2 for q in labs) for p in pts]
    weight = [math.log1p(p[3]) for p in pts]
    while len(pal) < PAL_SIZE and pts:
        k = max(range(len(pts)), key=lambda i: near[i] * weight[i])
        best = pts[k]
        c = (int(round(best[4])), int(round(best[5])), int(round(best[6])))
        pal.append(c)
        q = _to_oklab(*c)
        labs.append(q)
        for i, p in enumerate(pts):
            d = (p[0] - q[0]) ** 2 + (p[1] - q[1]) ** 2 + (p[2] - q[2]) ** 2
            if d < near[i]:
                near[i] = d
    while len(pal) < PAL_SIZE:
        pal.append((0, 0, 0))
    spans.append({"name": "worst-served corpus colours", "class": "extra",
                  "indices": list(range(extra_start, PAL_SIZE)), "len": PAL_SIZE - extra_start,
                  "own": PAL_SIZE - extra_start, "corpus_px": 0, "share": 0.0})

    PALETTE_DIR.mkdir(parents=True, exist_ok=True)
    write_hex(MASTER_HEX, pal)
    write_png(MASTER_PAL_PNG, PAL_SIZE, 1, 4,
              [bytearray(v for i, c in enumerate(pal)
                         for v in (c[0], c[1], c[2], 0 if i == 0 else 255))])
    write_swatch(MASTER_SWATCH, pal, spans, cycles)
    MASTER_JSON.write_text(json.dumps({
        "version": PALETTE_VERSION, "size": PAL_SIZE,
        "reserved": {"0": "transparent", "1": "black", "2": "white"},
        "cycles": cycles,
        "corpus_weights": [{"match": list(k), "weight": w, "what": t} for k, w, t in CORPUS_WEIGHTS],
        "ramp_lightness": [RAMP_LO, RAMP_HI],
        "shadow_hue": SHADOW_HUE, "highlight_hue": HILITE_HUE,
        "merge_dE": MERGE_DE, "merged": dropped,
        "corpus": [{"file": str(f.relative_to(ROOT.parent)), "weight": wt} for f, wt in files],
        "ramps": spans,
    }, indent=2) + "\n")
    print(f"wrote {MASTER_HEX.relative_to(ROOT.parent)}, master.pal.png, master_swatch.png, master.json "
          f"(version {PALETTE_VERSION}, {time.time()-t0:.1f}s)")
    cmd_palette_colormap([])
    write_cycles(pal, cycles)


def write_swatch(path, pal, spans, cycles=(), cell=24):
    """One material ramp a row, left-aligned, reserved three on the top row. No labels: it is a
    reference image for ChatGPT, and any text on it comes back drawn into the art."""
    rows_of = [[pal[1], pal[2]]]
    for c in cycles:                                    # the reserved cycle blocks get their own rows
        rows_of.append([pal[c["start"] + i] for i in range(c["len"])])
    for s in spans:
        cols = [pal[i] for i in s["indices"]]
        if s["class"] == "extra":                       # the leftovers wrap, or one row is 100 wide
            rows_of += [cols[i:i + 16] for i in range(0, len(cols), 16)]
        else:
            rows_of.append(cols)
    W = max(len(r) for r in rows_of) * cell
    out = []
    for r in rows_of:
        line = bytearray()
        for c in r:
            line += bytes(c) * cell
        line += bytes((16, 16, 18)) * (W // cell - len(r)) * cell
        for _ in range(cell):
            out.append(bytearray(line))
    write_png(path, W, len(out), 3, out)


# ── the colormap: light, time of day and effects, as table rows ──

COLORMAP_TABLES = ("day", "dusk", "night", "lamp", "lantern", "flash", "poison", "stone")
SHADE_FLOOR = 0.15                    # a colour that started above this lightness never falls below it
AMBIENT_SHADOW_HUE = 285.0            # where a colour drifts as the ambient darkens. NOT the ramps'
                                      # 295: 295 is violet enough to read as magenta once a whole
                                      # screen is at level 24, which is exactly what the first night
                                      # render looked like — a pink village.
NIGHT_CHROMA = 0.35                   # what a colour keeps of its own chroma at night
NIGHT_TINT = (-0.004, -0.042)         # the moonlight itself, added in Oklab coordinates
LAMP_TINT = (0.005, 0.022)            # firelight, added the same way: red-yellow, not a rotation


def shade_colour(lab, s, table):
    """One palette colour at light level s (1.0 full light, 0.0 darkest), in the named table."""
    L0, a, b = lab
    C0, H0 = math.hypot(a, b), math.atan2(b, a)
    C, H = C0, H0
    L = L0 * s
    if table not in ("night", "lamp", "lantern"):        # those two set their own direction outright
        C *= 0.30 + 0.70 * s
        H = _lerp_ang(H, math.radians(AMBIENT_SHADOW_HUE), 0.22 * (1 - s))
    if table == "night":
        # Moonlight, not a magenta filter, and NOT a hue rotation. Rotating every hue toward blue is
        # what made the first two night tables pink: the short way round from a red roof or an orange
        # jerkin to blue runs straight through magenta, so the warmest things on screen came out the
        # loudest colour on screen. What night actually does is take the colour out and lay a blue
        # over what is left — so chroma drops to a third, each colour keeps its own direction, and
        # one flat blue-violet cast is added in Oklab coordinates. Greens land as dark teal-blue,
        # reds as dark maroon, neutrals as blue, and skin stays a face.
        L = L0 * s * 0.78
        C = C0 * NIGHT_CHROMA
        a, b = C * math.cos(H0) + NIGHT_TINT[0], C * math.sin(H0) + NIGHT_TINT[1]
    elif table in ("lamp", "lantern"):
        # The pool under a lantern or a fire. The engine looks a point light up in this table and
        # mixes it in by how far the light is above ambient, so level 0 has to be full-strength warm
        # light — brighter and warmer than day, never bluer — and the dim end is the edge of the pool.
        # Warm by ADDITION, for the same reason night is blue by addition: rotating a blue roof tile
        # toward orange takes it through magenta and a purple water trough is not firelight.
        L = L0 * (0.12 + 0.88 * s) + 0.05 * s
        C = C0 * 1.10 * (0.45 + 0.55 * s)
        a, b = C * math.cos(H0) + LAMP_TINT[0] * s, C * math.sin(H0) + LAMP_TINT[1] * s
    elif table == "dusk":
        H = _lerp_ang(H, math.radians(55), 0.32 * (1 - s) + 0.12)
        C = C * 1.05 + 0.018 * (1 - s)
        L, a, b = L * 0.95, C * math.cos(H), C * math.sin(H) + 0.010
    elif table == "flash":                              # a hit, a spell, lightning: washed toward white
        f = 0.75 * s
        a, b = C * math.cos(H) * (1 - f), C * math.sin(H) * (1 - f)
        L = L + (1.0 - L) * f
    elif table == "poison":
        H = _lerp_ang(H, math.radians(140), 0.55)
        C = C * 0.75 + 0.030
        a, b = C * math.cos(H), C * math.sin(H)
    elif table == "stone":                              # petrified, or a statue: all the colour out
        a, b = C * math.cos(H) * 0.10, C * math.sin(H) * 0.10
        L = L * 0.92 + 0.04
    else:
        a, b = C * math.cos(H), C * math.sin(H)
    if L0 > SHADE_FLOOR:                                # the floor PALETTE.md asks for: it still reads
        L = max(L, SHADE_FLOOR)
    return (L, a, b)


def build_colormap(pal):
    """[(table, [row of 256 palette indices] per light level)] — row 0 is full light."""
    idx = PalIndex(pal)
    out = []
    for table in COLORMAP_TABLES:
        rows = []
        for lv in range(LEVELS):
            s = 1.0 - 0.95 * (lv / (LEVELS - 1))
            row = bytearray(PAL_SIZE)
            for i, c in enumerate(pal):
                if i == PAL_TRANSPARENT:
                    continue
                L, a, b = shade_colour(_to_oklab(*c), s, table)
                row[i] = idx.of(*_from_oklab(L, a, b))
            rows.append(row)
        out.append((table, rows))
        print(f"  {table}: {LEVELS} light levels")
    return out


def cmd_palette_colormap(args):
    """story/palette/colormap.png — every table at every light level, as colours the engine can use."""
    pal = read_hex(args[0]) if args and not args[0].startswith("--") else master_palette()
    t0 = time.time()
    tables = build_colormap(pal)
    rows, meta = [], []
    for table, trows in tables:
        meta.append({"table": table, "row0": len(rows), "levels": LEVELS,
                     **({"alias_of": "lamp"} if table == "lantern" else {})})
        for r in trows:
            # RGBA, and column 0 is transparent in EVERY row. Index 0 is the transparent index, so a
            # colormap that wrote it as opaque black handed the engine a black texel to blend at
            # every sprite edge; it was patching that on load. There is nothing to patch now.
            line = bytearray()
            for i in range(PAL_SIZE):
                line += bytes(pal[r[i]]) + bytes((0 if i == PAL_TRANSPARENT else 255,))
            rows.append(line)
    PALETTE_DIR.mkdir(parents=True, exist_ok=True)
    write_png(COLORMAP_PNG, PAL_SIZE, len(rows), 4, rows)
    COLORMAP_JSON.write_text(json.dumps({
        "palette": "master.hex", "version": PALETTE_VERSION,
        "width": PAL_SIZE, "height": len(rows), "levels": LEVELS, "channels": "RGBA",
        "note": "row (table.row0 + level) column i = the colour palette index i takes at that light. "
                "Level 0 is full light, level 31 the darkest. Each entry is the RGB of the nearest "
                "MASTER palette index, so the engine may use the colormap directly as colour. "
                "Column 0 is the transparent index: RGBA (0,0,0,0) in every row, so the engine needs "
                "no patch on load. Every other column is opaque.",
        "floor": SHADE_FLOOR,
        "point_light_table": "lamp",
        "tables": meta,
    }, indent=2) + "\n")
    print(f"wrote {COLORMAP_PNG.relative_to(ROOT.parent)}  {PAL_SIZE}x{len(rows)} "
          f"({len(tables)} tables x {LEVELS} levels) and colormap.json ({time.time()-t0:.1f}s)")


def write_cycles(pal, cycles):
    """story/palette/cycles.md, from the blocks `palette build` reserved. Generated, not hand-written."""
    lines = ["# Palette cycles (D19/D20)", "",
             "One line a cycle: `name: index index index… @ fps`. The engine rewrites those columns of",
             "the colormap every frame, rotating the listed indices through each other's colours, so a",
             "river moves and a lamp flickers with no second frame of art.", "",
             "These index ranges are **reserved**: `palette build` lays them down immediately after",
             "black and white, before any material ramp, and nothing else in the palette shares them.",
             "That is the point — an index shared with a roof tile would make the roof flicker too.",
             "Regenerated by `./story_prompt.py palette build`; do not edit by hand.", ""]
    for c in cycles:
        idx = list(range(c["start"], c["start"] + c["len"]))
        lines.append(f"{c['name']}: {' '.join(str(i) for i in idx)} @ {c['fps']}"
                     f"   # {c['what']}")
    lines.append("")
    CYCLES_MD.write_text("\n".join(lines))
    print(f"wrote {CYCLES_MD.relative_to(ROOT.parent)}  "
          + ", ".join(f"{c['name']} {c['start']}-{c['start'] + c['len'] - 1}" for c in cycles))


# ── per-scene palettes for cutscene panels ──

def kmeans_palette(pts, k, iters=8, seed=7):
    """k-means++ in Oklab, weighted by pixel count. Returns sRGB means — statistical, no ramps.

    Panels are never lit and never share the screen with the field, so nothing here has to shade
    gracefully; what matters is that a sky with forty blues keeps forty blues."""
    rnd = random.Random(seed)
    if len(pts) <= k:
        return [(int(round(p[4])), int(round(p[5])), int(round(p[6]))) for p in pts]
    first = max(pts, key=lambda p: p[3] * rnd.random())
    cents = [(first[0], first[1], first[2])]
    d2 = [((p[0] - cents[0][0]) ** 2 + (p[1] - cents[0][1]) ** 2 + (p[2] - cents[0][2]) ** 2) * p[3]
          for p in pts]
    while len(cents) < k:
        tot = sum(d2)
        if tot <= 0:
            cents.append(pts[rnd.randrange(len(pts))][0:3])
            continue
        t, acc, pick = rnd.random() * tot, 0.0, len(pts) - 1
        for i, v in enumerate(d2):
            acc += v
            if acc >= t:
                pick = i
                break
        c = (pts[pick][0], pts[pick][1], pts[pick][2])
        cents.append(c)
        for i, p in enumerate(pts):
            nd = ((p[0] - c[0]) ** 2 + (p[1] - c[1]) ** 2 + (p[2] - c[2]) ** 2) * p[3]
            if nd < d2[i]:
                d2[i] = nd
    for _ in range(iters):
        sums = [[0.0] * 7 for _ in range(k)]
        for p in pts:
            best, bd = 0, 1e9
            for j, c in enumerate(cents):
                d = (p[0] - c[0]) ** 2 + (p[1] - c[1]) ** 2 + (p[2] - c[2]) ** 2
                if d < bd:
                    bd, best = d, j
            s = sums[best]
            for n, v in enumerate((p[0] * p[3], p[1] * p[3], p[2] * p[3], p[3],
                                   p[4] * p[3], p[5] * p[3], p[6] * p[3])):
                s[n] += v
        for j in range(k):
            if sums[j][3] > 0:
                cents[j] = (sums[j][0] / sums[j][3], sums[j][1] / sums[j][3], sums[j][2] / sums[j][3])
    out, sums = [], [[0.0] * 4 for _ in range(k)]
    for p in pts:
        best, bd = 0, 1e9
        for j, c in enumerate(cents):
            d = (p[0] - c[0]) ** 2 + (p[1] - c[1]) ** 2 + (p[2] - c[2]) ** 2
            if d < bd:
                bd, best = d, j
        s = sums[best]
        s[0] += p[3]
        s[1] += p[4] * p[3]
        s[2] += p[5] * p[3]
        s[3] += p[6] * p[3]
    for s in sums:
        if s[0] > 0:
            out.append(tuple(int(round(s[i] / s[0])) for i in (1, 2, 3)))
    return out


def scene_panels(scene):
    return sorted(PANELS.glob(f"{scene}_p*.png"))


def palettise_scene(scene):
    """Fit a palette to one scene's own panels and convert them all. (message)."""
    files = scene_panels(scene)
    if not files:
        return f"{scene}: no panels to palettise"
    pts = palette_histogram(files, thumbs=True, keep=None)
    cols = kmeans_palette(pts, PAL_SIZE - 1)
    pal = [(0, 0, 0)] + cols[:PAL_SIZE - 1]
    while len(pal) < PAL_SIZE:
        pal.append((0, 0, 0))
    write_hex(scene_hex(scene), pal)
    idx = PalIndex(pal)
    for f in files:
        w, h, ch, rows = read_png(f)
        ship_png(f, w, h, ch, rows, pal, idx)
    return (f"{scene}: {len(files)} panel(s) -> {scene_hex(scene).name} "
            f"({len(cols)} colours + transparent)")


# ── apply, check ──

def palette_for(arg):
    if not arg or arg == "master":
        return master_palette()
    return read_hex(Path(arg))


def cmd_palette_apply(args):
    pal_arg, files = None, []
    it = iter(args)
    for a in it:
        if a == "--palette":
            pal_arg = next(it, "master")
        elif not a.startswith("--"):
            files.append(a)
    if not files:
        die("usage: palette apply <file.png> [more files] [--palette master|<hex file>]")
    pal = palette_for(pal_arg)
    idx = PalIndex(pal)
    for f in files:
        p = Path(f).expanduser()
        if not p.exists():
            die(f"{p} not found")
        w, h, ch, rows = read_png(p)
        was = p.stat().st_size
        t0 = time.time()
        ship_png(p, w, h, ch, rows, pal, idx, note=False)
        print(f"{p.name}: {w}x{h} -> indexed, {was} -> {p.stat().st_size} bytes ({time.time()-t0:.1f}s)")


def png_palette(path):
    """(colour type, PLTE as [(r,g,b)], tRNS bytes) without expanding the image."""
    data = Path(path).read_bytes()
    if data[:8] != b"\x89PNG\r\n\x1a\n":
        return None, [], b""
    pos, ctype, plte, trns = 8, None, [], b""
    while pos < len(data):
        n, kind = struct.unpack(">I4s", data[pos:pos + 8])
        body = data[pos + 8:pos + 8 + n]
        pos += 12 + n
        if kind == b"IHDR":
            ctype = body[9]
        elif kind == b"PLTE":
            plte = [tuple(body[i:i + 3]) for i in range(0, len(body), 3)]
        elif kind == b"tRNS":
            trns = body
        elif kind == b"IDAT":
            break
    return ctype, plte, trns


def shipped_images():
    """[(path, palette, what)] — every image the game ships, with the palette it must be drawn from."""
    out = []
    for rel in SHIPPED_MASTER:
        d = ROOT / rel
        if not d.exists():
            continue
        for p in sorted(d.rglob("*.png")):
            out.append((p, "master"))
    for p in sorted(PANELS.glob("*.png")):
        scene = re.sub(r"_p\d+_.*$", "", p.stem)
        out.append((p, scene))
    return out


def cmd_palette_check(args, quiet=False):
    """Every shipped image is an indexed PNG holding nothing but its own palette. (errors)."""
    err, seen, pals = [], 0, {}
    for p, which in shipped_images():
        seen += 1
        rel = p.relative_to(ROOT.parent)
        if which not in pals:
            hexf = MASTER_HEX if which == "master" else scene_hex(which)
            if not Path(hexf).exists():
                err.append(f"{rel}: no palette at {Path(hexf).relative_to(ROOT.parent)}; "
                           f"run ./story_prompt.py ingest --force on the package that made it")
                pals[which] = None
            else:
                pals[which] = read_hex(hexf)
        pal = pals[which]
        if pal is None:
            continue
        ctype, plte, trns = png_palette(p)
        if ctype != 3:
            err.append(f"{rel}: colour type {ctype}, not an indexed PNG (PALETTE.md: everything ships "
                       f"at one byte a pixel). Recut it, or: ./story_prompt.py palette apply {rel}")
            continue
        if plte[:len(pal)] != [tuple(c) for c in pal]:
            bad = next((i for i in range(min(len(plte), len(pal))) if tuple(plte[i]) != tuple(pal[i])), -1)
            err.append(f"{rel}: its PLTE is not the {which} palette (first difference at index {bad}). "
                       f"The palette changed since this was cut: ./story_prompt.py ingest --force")
            continue
        if list(trns) != [0]:
            err.append(f"{rel}: tRNS is {list(trns)}; only index 0 may be transparent")
    if not quiet:
        for e in err:
            print(f"ERROR: {e}", file=sys.stderr)
        print(f"palette check: {seen} shipped image(s), {len(err)} problem(s)")
    return err


def cmd_palette(args):
    sub = args[0] if args else ""
    if sub == "build":
        cmd_palette_build(args[1:])
    elif sub == "apply":
        cmd_palette_apply(args[1:])
    elif sub == "check":
        if cmd_palette_check(args[1:]):
            sys.exit("story_prompt: shipped art does not match its palette.")
    elif sub == "colormap":
        cmd_palette_colormap(args[1:])
    elif sub == "scene":
        for s in args[1:]:
            print(palettise_scene(s))
    else:
        die("usage: palette build | apply <files> [--palette master|<hex>] | check | colormap | "
            "scene <scene stem>")


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
    for other in sorted(d.glob(f"{RETURNED}_*")):         # a screen returns three images, not one
        if other.suffix.lower() in RETURN_SUFFIXES:
            to_png(other, SHEETS / (str(rel).replace("/", "-") + other.stem[len(RETURNED):] + ".png"))
    return out


def ingest_one(d, meta, returned, extra=()):
    """Run the right cutter for one package. (ok, message)."""
    kind = meta.get("kind")
    if kind == "sheet":
        return run_quiet(cmd_slice, [str(d / "sheet.json"), str(returned)])
    if kind == "screen":                                     # three images in, the engine's .screen out
        ok, msg = run_quiet(cmd_screen_cut, [str(d / "sheet.json"), str(d),
                                             *[a for a in extra if a == "--debug"]])
        return ok, (SCREEN_SUMMARY[0] if ok and SCREEN_SUMMARY else msg)
    if kind in FIELD_KINDS or kind in ("view", "tileset", "swatch", "decal"):
        return run_quiet(cmd_cut, [str(d / "sheet.json"), str(returned), *extra])
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
    force, extra, skip = "--force" in args, [], False
    for a in args:                                       # --fringe N is handed to the cutter as it stands
        if skip:
            extra.append(a)
            skip = False
        elif a in ("--fringe", "--palette"):
            extra.append(a)
            skip = True
        elif a in ("--nearest", "--debug", "--no-heal"):
            extra.append(a)
    roots = [Path(a).expanduser().resolve() for a in args
             if not a.startswith("--") and a not in extra] or [PACKAGES]
    metas = []
    for r in roots:
        if not r.exists():
            die(f"{r} not found. Give a package folder under {PACKAGES.relative_to(ROOT.parent)}/, "
                f"or no path at all to do them all.")
        metas += [r / PKG_META] if (r / PKG_META).exists() else sorted(r.rglob(PKG_META))
    if not metas:
        die(f"no package folders under {', '.join(str(r) for r in roots)}. Run: ./story_prompt.py packages")

    done, failed, waiting, fresh, loud = 0, 0, 0, 0, 0
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
        LOUD.clear()
        ok, msg = ingest_one(d, meta, returned, extra)
        line = squash(msg) if msg else ""
        if ok:
            done += 1
            after, have, total = pkg_state(d, meta)
            print(f"ok    {rel}  ({returned.name} -> {have}/{total} file(s)){'  ' + line if line else ''}"
                  + (f"  [kept {kept.relative_to(ROOT.parent)}]" if kept else ""))
        else:
            failed += 1
            print(f"FAIL  {rel}  {line}", file=sys.stderr)
        for w in LOUD:                                   # never let a cutter's warning vanish into the capture
            print(f"      ! {w}", file=sys.stderr)
        loud += len(LOUD)
    if PAL_TIMING:
        secs = sum(t for _, _, t in PAL_TIMING)
        worst = max(PAL_TIMING, key=lambda r: r[2])
        print(f"\npalette: {len(PAL_TIMING)} file(s) converted to indexed art in {secs:.1f}s "
              f"(slowest {worst[0]}, {worst[1] / 1e6:.2f} MP in {worst[2]:.1f}s)")
    print(f"\n{done} cut, {failed} failed, {fresh} already up to date, {waiting} still waiting for an image "
          f"({len(metas)} package(s))" + (f", {loud} warning(s) above" if loud else ""))
    if done:
        print("The game picks the new art up on the next ./fast_reload.sh (it runs portraits and export first).")
    if failed:
        sys.exit(f"story_prompt: {failed} package(s) failed. The returned images are untouched; fix and rerun.")


# ───────────────────────── Export to the game ─────────────────────────

GAME_HEADER = ROOT.parent / "src" / "cutscene_data.h"
GAME_FIELD_TEXT = ROOT.parent / "src" / "field_text.h"


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
    export_field_text()


def export_field_text():
    """story/field/text.md -> src/field_text.h: the examine lines, names substituted, sorted by id."""
    err, _ = check_field_text()
    if err:
        for e in err:
            print(f"ERROR: {e}", file=sys.stderr)
        sys.exit("story_prompt: field text rejected. Fix story/field/text.md; do not bypass the rules.")
    entries = load_field_text()
    out = ["// GENERATED by ./story_prompt.py export. Do not edit: change story/field/text.md.",
           "#pragma once", "",
           "// What the world says when you look at it. A map's `message` trigger names an id; the",
           "// engine looks it up here. Sorted by id, so a binary search is safe.",
           "// name is the display name to draw over the box, or \"\" when the line has no speaker.",
           "struct FieldText { const char *id; const char *text; const char *name; };",
           "static const FieldText FIELD_TEXT[] = {"]
    for ident in sorted(entries):
        e = entries[ident]
        out.append(f"    {{ {c_str(ident)}, {c_str(e['text'])}, {c_str(e['name'])} }},")
    out += ["};", f"#define FIELD_TEXT_COUNT {len(entries)}", ""]
    GAME_FIELD_TEXT.write_text("\n".join(out))
    todo = sum(1 for e in entries.values() if TODO_RE.match(e["raw"]))
    print(f"wrote {GAME_FIELD_TEXT.relative_to(ROOT.parent)}  ({len(entries)} line(s)"
          + (f", {todo} still a placeholder)" if todo else ")"))


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
    elif cmd == "building":
        cmd_building(args[1:])
    elif cmd == "view":
        cmd_view(args[1:])
    elif cmd == "tileset":
        cmd_tileset(args[1:])
    elif cmd == "tmap":
        cmd_tmap(args[1:])
    elif cmd == "screen":
        cmd_screen(args[1:])
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
    elif cmd == "palette":
        cmd_palette(args[1:])
    elif cmd == "names":
        cmd_names(args[1:])
    else:
        print(__doc__.strip())
        sys.exit(0 if cmd in ("", "-h", "--help", "help") else 2)


if __name__ == "__main__":
    main()
