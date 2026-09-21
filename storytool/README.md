# storytool/ — the story and art pipeline, one responsibility a module

`./story_prompt.py` at the repository root is a shim; everything is here. It is **stdlib
only** — there is no Pillow, no YAML, no third-party anything on this machine, and that is
deliberate: the tool has to run from a clean checkout with nothing installed.

Every module starts with a docstring saying what it owns, what it must never do, its
public functions and what it may import. Read that before the code.

## Where do I look to change X

| I want to… | Touch |
| --- | --- |
| add a command | `cli.py` (dispatch + usage text) and the module that owns the subject |
| change a composition rule threshold | `rules.py` **and** `story/STYLE.md` — the two are one rule written twice |
| change prompt wording | `story/STYLE.md`. Never a module: `style.py` quotes it verbatim |
| add a scene meta tag or dialogue tag | `scenes.py` (parse + validate), `export.py` (into `src/cutscene_data.h`), `preview.py` if it shows, and the consumer in `src/star_logic.cpp` |
| change the reveal rule | `scenes.reveal_plan` — the validator, `preview.py`, `script.py` and the game all read it |
| add a package kind | `packages.py` (build the folder), `ingest.py` (route the return), `cutcmd.py` (pick the cutter), and a `<kind>.py` + `<kind>_cut.py` pair |
| change where a file is written | `paths.py`, and nowhere else |
| change how a returned sheet is cut apart | `cutter.py` (find the art) and `keying.py` (make the background transparent) |
| change image reading, writing or resampling | `png.py` |
| change a palette, a light table or a colour cycle | `palette.py`, `palette_fit.py`, `colormap.py`, and `PALETTE.md` |
| change the tileset atlas or a mask | `tset.py` (where and how big), `masks.py` (the boundary), `atlas.py` (the cut and the cell) |
| change a map rule | `tmap.py` (validate), `tmapview.py` (render), and `TILES.md` |
| change what examine text may say | `fieldtext.py`, and the `BAN:` lists in `story/v3/STYLE.md` / `SMELLS.md`, which it reads |
| change the generated headers | `export.py` and the struct in `src/` that reads them |

## The import graph

It is acyclic and layered. A module may import anything above it in this list and nothing
below it. Sizes are lines including the docstring.

| module | lines | imports |
| --- | ---: | --- |
| `paths.py` | 53 | — |
| `rules.py` | 125 | — |
| `oklab.py` | 79 | — |
| `md.py` | 67 | paths |
| `names.py` | 180 | md, paths |
| `style.py` | 47 | md, paths, rules |
| `cast.py` | 106 | md, names, paths, rules |
| `scenes.py` | 372 | cast, md, names, paths, rules |
| `playlist.py` | 116 | md, names, paths, scenes |
| `png.py` | 202 | md, paths |
| `palette.py` | 295 | md, oklab, paths, png |
| `field.py` | 213 | md, names, paths, rules |
| `keying.py` | 162 | field |
| `canvas.py` | 140 | field, md |
| `cutter.py` | 285 | field, md, rules |
| `seamless.py` | 284 | md, oklab |
| `package_io.py` | 110 | md, paths |
| `layout.py` | 72 | md |
| `manifest.py` | 145 | field, package_io, paths, png |
| `field2.py` | 122 | canvas, field, md, package_io, palette, paths, png |
| `fieldtext.py` | 128 | field, md, names, paths |
| `tset.py` | 231 | field, md, paths |
| `terrain.py` | 97 | md, tset |
| `tileset.py` | 161 | field, md, names, paths, terrain, tset |
| `masks.py` | 161 | md, palette, tset |
| `atlas.py` | 374 | cutter, field, keying, md, oklab, palette, paths, png, seamless, tileset, tset |
| `tset_cut.py` | 114 | atlas, cutter, field, keying, masks, palette, paths, png, seamless, tileset, tset |
| `tmap.py` | 527 | field, fieldtext, md, palette, paths, playlist, png, tileset, tset |
| `tmapview.py` | 287 | masks, md, paths, png, terrain, tileset, tmap, tset |
| `colormap.py` | 169 | oklab, palette, paths, png |
| `palette_fit.py` | 390 | colormap, md, oklab, palette, paths, png |
| `palette_cmd.py` | 233 | colormap, md, palette, palette_fit, paths, png |
| `preview.py` | 158 | cast, paths, png, rules, scenes, style |
| `script.py` | 69 | md, paths, scenes |
| `stats.py` | 128 | cast, md, names, paths, playlist, rules |
| `portraits.py` | 65 | cast, cutter, md, palette, paths, png, rules |
| `expressions_cut.py` | 85 | cutter, keying, palette, paths, png, rules |
| `walkers_cut.py` | 222 | cutter, field, keying, manifest, md, oklab, palette, paths, png |
| `sprites.py` | 149 | canvas, field, field2, manifest, md, package_io, paths, style |
| `sprites_cut.py` | 78 | cutter, field, keying, palette, paths, png, sprites |
| `expressions.py` | 151 | canvas, cast, field, field2, manifest, md, package_io, paths, rules, style |
| `walkers.py` | 198 | canvas, cast, field, field2, manifest, md, package_io, paths, style, walkers_cut |
| `sheet.py` | 176 | cast, field2, layout, md, package_io, paths, rules, scenes, style |
| `cutcmd.py` | 133 | atlas, cutter, expressions_cut, field, manifest, md, palette_cmd, paths, png, rules, sprites_cut, tset_cut, walkers_cut |
| `check.py` | 145 | cast, field, fieldtext, md, names, palette_cmd, paths, scenes, style, tmap, tset |
| `export.py` | 182 | cast, field, fieldtext, md, paths, playlist, preview, rules, scenes, style |
| `packages.py` | 636 | canvas, cast, expressions, field, manifest, md, package_io, paths, playlist, rules, scenes, sheet, sprites, stats, walkers |
| `ingest.py` | 152 | cast, cutcmd, field, keying, md, package_io, packages, palette, paths, playlist, portraits, stats |
| `cli.py` | 168 | every module that defines a `cmd_*` |

Three shapes recur:

- **`<kind>.py` builds a package, `<kind>_cut.py` cuts what comes back.** They are kept
  apart on purpose: the cutter must not have to import the builder, or a returned sheet
  could not be re-cut after the builder changed. `walkers`/`walkers_cut`,
  `sprites`/`sprites_cut`, `expressions`/`expressions_cut`, `tileset`/`tset_cut`.
- **`field.py` / `field2.py`** are the field-art data file and sizes, then the package
  assembly that every template command ends with. `canvas.py` between them is the whole
  drawing library.
- **`palette.py` / `palette_fit.py` / `colormap.py` / `palette_cmd.py`** are the contract,
  fitting it to the art, lighting as table rows, and the commands.

Two globals are **rebound** at run time and so may never be imported by value:
`package_io.PKG_DIR` (which is why `package_label` lives in `package_io.py`, not
`manifest.py`), and the lazily-loaded caches `names._NAMES` and `palette._MASTER`, each of
which is only ever read through its own module's accessor.

## Testing a change

`tools/script/golden.py` is the contract. It runs every command in a throwaway copy of the
repo and records the exit code, the normalised output and a hash of every generated file.

```
tools/script/golden.py record /tmp/before.json --legacy <a pre-change story_prompt.py>
tools/script/golden.py record /tmp/after.json
tools/script/golden.py diff  /tmp/before.json /tmp/after.json
```

For a refactor the answer must be `IDENTICAL`. Take the pair back to back: other agents
edit `story/` while you work, and a report taken yesterday is a report of yesterday's
story. `--legacy` points at any older single-file or shimmed copy of the tool
(`git show <rev>:story_prompt.py > /tmp/old.py`), so both halves run over the same files.
