"""The `check`, `shots` and `brief` commands.

check runs the scene validator over the files it is given; `--all` widens it to the field
text, every .tmap and the palette, which is why this module sits high in the graph and
imports the validators rather than containing them. brief prints the writer's brief.

Must never do: implement a rule. Every rule it reports belongs to scenes, fieldtext,
tmap or palette_cmd.
Public: cmd_check, cmd_shots, cmd_brief. Imports: cast, field, fieldtext, md, names,
palette_cmd, paths, scenes, style, tset.
"""
import re
import sys
from .cast import load_cast
from .field import TODO_RE
from .fieldtext import check_field_text, load_field_text
from .md import die, squash
from .names import names_table
from .palette_cmd import cmd_palette_check
from .paths import CAST, FIELD_TEXT, ROOT, SCENES, STATE_FILES
from .scenes import parse_scene, scene_paths, validate
from .style import load_style
from .tmap import check_tmap, tmap_path
from .tset import all_tmaps



def cmd_check(args):
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
        pg = len(set(scene["pages"])) if scene["panels"] else 0
        what = f"{len(scene['panels'])} panels" if scene["kind"] == "panels" else \
               f"{scene['kind']}, {len(scene['dialogue'])} lines"
        print(f"{path.name}: ok ({what}" + (f", {pg} pages)" if pg > 1 else ")"))
    if "--all" in args:                    # the field's examine text answers to the same rules
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
    plot = "\n\n".join(f"## From {p.relative_to(ROOT.parent)}\n\n{p.read_text().strip()}"
                        for p in STATE_FILES if p.exists())
    beat = " ".join(args).strip()
    if not beat:
        m = re.search(r"^#+ Next beat\s*\n(.+?)(?=^#+ |\Z)", plot, flags=re.M | re.S)
        beat = squash(m.group(1)) if m else ""
    if not beat:
        die("no beat given, and no '## Next beat' heading in "
            + " or ".join(str(p.relative_to(ROOT.parent)) for p in STATE_FILES)
            + ". Pass the beat as an argument: ./story_prompt.py brief \"<what happens>\"")
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

After the scene validates with `./story_prompt.py check story/scenes/<file>`, add it to its chapter
list in story/playlist.md, revise the open threads in story/v3/THREADS.md, and update any changed
character status in story/characters.md.""")
