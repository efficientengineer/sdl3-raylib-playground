"""The command line: parse argv, hand off to a cmd_* function, print the usage text.

Owns nothing but the dispatch table and this tool's usage documentation (which is what
`./story_prompt.py` with no argument prints). It must never contain logic: a new command
is a cmd_* in the module that owns the subject, named here and nowhere else.

Public: main(). Imports: every module that defines a cmd_*, and nothing else.
"""
import sys

from .check import cmd_brief, cmd_check, cmd_shots
from .cutcmd import cmd_cut, cmd_slice
from .expressions import cmd_expressions
from .export import cmd_export
from .ingest import cmd_ingest
from .names import cmd_names
from .packages import cmd_packages
from .palette_cmd import cmd_palette
from .portraits import cmd_portraits
from .preview import cmd_preview
from .script import cmd_script
from .sheet import cmd_refsheet, cmd_sheet
from .sprites import cmd_sprites
from .stats import cmd_stats
from .tmapview import cmd_tmap
from .walkers import cmd_walker

USAGE = """Story scene -> image prompt builder, and the art tray. Enforces story/STYLE.md.

  ./story_prompt.py shots                      list the shot menu
  ./story_prompt.py check  story/scenes/*.md | --all
                                               validate scenes; --all also checks story/field/text.md,
                                               every .tmap and the palette
  ./story_prompt.py brief  ["next beat"]       print a brief for an LLM to write the next scene file
  ./story_prompt.py names                      the {{TOKEN}} table from story/v3/NAMES.md, and every token
                                               used in the story that has no row in it (exit 1 if any)
  ./story_prompt.py stats [--all]              per chapter: scenes by type, panels, lines, panel art
  ./story_prompt.py script [chapter]           rebuild story/v3/chapterNN_script.md: the chapter read as
                                               a play, each picture described where it appears

  THE LOOP: ./story_prompt.py packages -> open a folder -> generate -> save returned.png -> ingest.
  ./story_prompt.py packages [--all]           (re)build story/packages/, the art tray: five kinds and
                                               nothing else — cast/<name>/refsheet|expressions|walker,
                                               chNN/sprites, chNN/scenes/<scene>. Each folder holds
                                               prompt.md, sheet.json, template.png and RETURN_HERE.md,
                                               and README.md is the ordered to-do with a status a row
  ./story_prompt.py ingest [folder] [--force] [--fringe N] [--debug]
                                               cut every returned.png waiting in the tray, and stamp
                                               each package with the inputs it was cut from, so a later
                                               edit to a `look` line or a scene shows the art as STALE

  The pieces underneath, for driving one package by hand:
  ./story_prompt.py refsheet Falke             a character reference sheet package
  ./story_prompt.py expressions Falke [more]   ten numbered head-and-shoulders slots (neutral, smile,
                                               laugh, biglaugh, concern, sorrow, annoyed, angry, shock,
                                               resolve) -> story/portraits/<name>_<id>.png, shipped as
                                               portrait_<name>_<id>.png. A line picks one with
                                               '- Falke (biglaugh): text'; the line may be textless
  ./story_prompt.py portraits                  cut each reference sheet's middle panel to story/portraits/
  ./story_prompt.py walker Falke Distel [more] the 9-frame walk sheet (rows S, side, N; columns stand,
                                               step-A, step-B) at 128x192; up to 3 characters a sheet
  ./story_prompt.py walker compact [<id>...]   an old 16-frame sheet -> the 9-frame one
  ./story_prompt.py sprites burr klee [more]   THE VOXEL WORLD'S BILLBOARDS (WORLD.md): slots sized by
                                               each entry's footprint in story/field/sprites.md, one slot
                                               a frame -> story/field/sprites/<id>.png (+ .json when it
                                               has frames). The ground, walls and houses need no art
  ./story_prompt.py sheet  story/scenes/001_x.md [more scenes]
                                               a shot sheet package (max 6 panels)
  ./story_prompt.py slice  <sheet.json> downloaded.png [--trim N] [--boxes "x,y,w,h;..."]
                                               cut a shot sheet into story/panels/<scene>_pN_<shot>.png
  ./story_prompt.py cut    <sheet.json> downloaded.png [--fringe N]
                                               cut a TEMPLATE sheet: key the magenta, localise each slot,
                                               write the files ('slice' redirects here when it is one)
  ./story_prompt.py preview story/scenes/003_x.md
                                               story/out/<scene>.preview.html: the panels layered manga
                                               style and revealed line by line (placeholders if not cut)

  ./story_prompt.py tmap check   halm | --all  validate story/field/tmaps/<map>.tmap against its tileset
  ./story_prompt.py tmap preview halm          render it from the atlas -> story/out/<map>.tmap.png

  ./story_prompt.py export                     write src/cutscene_data.h from story/playlist.md (every
                                               '## chapterNN' list; a panel scene with no art exports as
                                               placeholder boxes carrying each panel's description), and
                                               src/field_text.h from story/field/text.md

  Palette (PALETTE.md, D19) — 256 colours, one byte a pixel, and lighting as table rows.
  ./story_prompt.py palette build              fit story/palette/master.* to the raw returns in
                                               story/sheets + story/refs (cutscene panels excluded:
                                               they get their own palette per scene at slice time),
                                               then the colormap and cycles.md
  ./story_prompt.py palette apply <files> [--palette master|<hex>]     convert by hand
  ./story_prompt.py palette check              every shipped image is indexed and holds only its own
                                               palette (also run by `check --all`)
  ./story_prompt.py palette colormap           story/palette/colormap.png + .json

  stats and packages consider the scenes story/playlist.md names; --all takes story/scenes/ as it stands.

Scene types (the '- type:' meta line): panels (default, manga page), narration (text over
black), talk (dialogue box over black or a dimmed backdrop panel, with speaker portraits).

Style text, shots, and shapes are parsed from story/STYLE.md; character looks from
story/characters.md. Nothing stylistic is hard-coded here except the rule thresholds
below, which mirror the "Composition rules" section of STYLE.md. Keep the two in sync.

Names are tokens (DECISIONS.md D13): the story text says {{HERO}}, story/v3/NAMES.md says what
{{HERO}} is called this week, and every command substitutes before it uses the text, so the game
and ChatGPT only ever see real names and a rename is one line. The files stay tokenised.

The retired systems (tiles, props, buildings, painted screens, painted views, the tileset package
generator, the single-page `build` mode) went on 2026-09-21 with the field designs that needed them:
DECISIONS.md D24, and the `legacy-final` git tag. The tileset CUTTER stays, so the valley atlas can
still be re-cut from story/sheets/ if the palette is refitted.
Stdlib only. The tool lives in storytool/ — storytool/README.md is the module map.
"""

__doc_usage__ = USAGE


def main():
    args = sys.argv[1:]
    cmd = args[0] if args else ""
    if cmd == "shots":
        cmd_shots()
    elif cmd == "check":
        cmd_check(args[1:])
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
    elif cmd == "sprites":
        cmd_sprites(args[1:])
    elif cmd == "walker":
        cmd_walker(args[1:])
    elif cmd == "expressions":
        cmd_expressions(args[1:])
    elif cmd == "tmap":
        cmd_tmap(args[1:])
    elif cmd == "cut":
        cmd_cut(args[1:])
    elif cmd == "preview":
        cmd_preview(args[1:])
    elif cmd == "export":
        cmd_export(args[1:])
    elif cmd == "script":
        cmd_script(args[1:])
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
        print(USAGE.strip())
        sys.exit(0 if cmd in ("", "-h", "--help", "help") else 2)


if __name__ == "__main__":
    main()
