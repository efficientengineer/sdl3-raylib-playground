# tool agent — dialogue-box expressions (owner's request, 2026-09-21)

Ten faces a speaking character, cut from one template sheet, plus the no-weapons rule on every
character sheet. Everything the owner does runs from the art tray.

## The command

```
./story_prompt.py expressions <Name> [<Name> ...]
```

One package a character — the tool draws the template (2 rows of 5 numbered white-bordered slots,
flat mid-grey inside each, dark canvas outside so a file from the wrong package is still caught by
the margin check), ChatGPT fills it, `ingest` cuts it. The reference sheet is **required**: it is
attached, and the command exits non-zero without one, exactly as `walker` does.

Slot layout on a 1536x1024 canvas: slots 267x400 (inner 261x394), 2:3 — the dialogue portrait's own
shape — packed at 71% of the canvas so a ~1.5 MP return still gives each face ~190x290 real pixels.

## The ten ids, in slot order (never reorder or rename — the cut goes by slot number)

`neutral smile laugh biglaugh concern sorrow annoyed angry shock resolve`

They live in `EXPRESSIONS` at the top of `story_prompt.py` with their one-line acting description,
and are mirrored in `story/STYLE.md` → "Expression sheet".

## Files, and what must ship

The cut writes, per character:

```
story/portraits/<handle>_<expr>.png        192x288, indexed on the master palette, opaque
```

**These have to ship.** They go to the phone under the same convention as the existing portrait:
`portrait_<handle>_<expr>.png`. Checked while writing this: the engine agent has already changed
`fast_reload.sh` (line ~105) and `deploy.sh` (line ~25) to glob **every** `story/portraits/*.png`
and ship it as `portrait_<basename>.png`, so the expression variants ship with no further change.
Note that `./story_prompt.py portraits`, which both scripts run first, only ever rewrites
`<name>.png` — it never touches or deletes an `<name>_<expr>.png`.

The main portrait `story/portraits/<handle>.png` is **left alone** by default (it is still the one
cut from the reference sheet). `ingest ... --neutral-main` replaces it with slot 1 instead.

## The export contract (fixed; the engine side implements the other half)

A dialogue line may carry the expression in parentheses right after the speaker, before `[n]` and
`{mood}`, and may be textless when it does:

```
- Ottilie (biglaugh) [2] {hope}: text
- Ottilie (biglaugh):
```

`src/cutscene_data.h` gains, at the END of the line struct so old initialisers stay valid:

```c
struct CsLine { const char *speaker; const char *text; int reveal; CsMood mood;
                const char *portrait; CsSide side;
                const char *expr; };                  // "" when untagged
#define CS_HAS_EXPR 1
```

The game looks for `portrait_<name>_<expr>.png` and falls back to `portrait_<name>.png`.

Validation: unknown id = error; a tag on `Narrator` = error; an empty line with no expression =
error; a tag on a speaker with no reference sheet, or whose expression sheet has not been generated
yet = warning.

## Art tray

`packages` writes `story/packages/cast/<name>/expressions/` for every cast member the selected
scenes have **speak** (a dialogue speaker, resolved through aliases), listed in the README as
section 4 — after that character's reference sheet, before the walk sheet — and in the short path
between them. Blocked with "needs the reference sheet first" until `story/refs/<name>.png` exists.
Frozen once a `returned.png` is in the folder, like every other package. ArtTray needed **no
change**: it is driven by `package.json`'s `kind` and the `prompt.md` attach list. `--selftest`
passes (52 packages).

Falke's package: `story/packages/cast/falke/expressions/` (handle `bron`, so the portraits are
`story/portraits/bron_<expr>.png`).

## Synthetic round trip

Ten flat coloured "faces" painted into the real template, scaled to 0.82x, saved as the package's
`returned.png`, `ingest` run: 10/10 cut to 192x288 indexed PNGs, no white-border pixels left on any
edge (the slot is shaved 2 px × the return's scale before the border scrub — a portrait is opaque,
so a surviving border sliver would have shipped as a bright line beside the face). A deliberately
broken sheet (slot 5 left blank, slot 3 a copy of slot 2) produced exactly two warnings and no false
positives.

## For the orchestrator

- `characters.md` and `story/scenes/` were being edited while this ran. My `./story_prompt.py
  packages` run also **pruned** `story/packages/cast/shrinewoman/walker/` (no returned image, and
  shrinewoman is no longer a speaker in the current playlist) and rewrote several unrelated
  `prompt.md`/`sheet.json` files. Rerun `packages` once the writer is done and take that run's tree
  as the truth.
- `src/cutscene_data.h` and `src/field_text.h` were exported to test and then reverted with `git
  checkout`. **Re-export** after the writer finishes; the header will then carry the `expr` field.
- `./story_prompt.py check --all` passes.
