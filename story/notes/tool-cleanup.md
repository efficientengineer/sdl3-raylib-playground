# tool-cleanup — clearing the art tray and the legacy (2026-09-21)

The tool agent's report on D24. `story/DECISIONS.md` D24 is the ruling; this is the working detail.

## What the tray looks like now

27 generated packages, 7 frozen. The queue, in `story/packages/README.md` order:

1. **Reference sheets** (7) — Falke *stale*, Ottilie *exists*, Hart / Guildclerk / Stolz *done*,
   Distel / Garbe *to do*.
2. **Expression sheets** (7) — five open, Distel and Garbe blocked on their reference sheets.
3. **Walk sprites** (10) — Falke and Ottilie *stale*, Hart / Guildclerk / Stolz / linde *to do*,
   villager_a / villager_b *done*, Distel and Garbe blocked.
4. **Field sprites** (3 sheets, 11 billboards) — all to do.
5. **Scene shot sheets** (4) — the four panel scenes of chapter one, all to do.

**25 to do (3 of them redraws), 6 drawn and finished.**

## The three stale rulings, and why they are rulings and not computations

Fingerprints did not exist when these were cut, so there is no honest way to compute their status.
They are hard-coded in `LEGACY_STATUS` at the top of the tray section of `story_prompt.py`:

| package | ruling | why |
|---|---|---|
| `cast/falke/refsheet` | **stale** | the sheet on disk is the old red-haired armoured design; `characters.md`'s `look` is now green-haired in a quilted ochre training vest with a practice stick |
| `cast/falke/walker` | **stale** | cut from that old reference sheet |
| `cast/ottilie/walker` | **stale** | cut from the reference sheet as it was before the weapon came off her `look` line |
| `cast/ottilie/refsheet` | **exists** | good as it is. Redraw ONLY to get the mace out of the full-body panel; the portrait is unaffected |

Everything cut from here on records a `cut_fingerprint` in its `package.json`, so `stale` is
computed from then on: change a `look` line, a scene's panels or a sprite's slot list and the row
says so, with the reason.

## Do NOT redo

- **`story/packages/tilesets/valley/**`** — seven sheets, all drawn. The voxel field reads that
  atlas, its decals and its swatches every frame. They are frozen: never regenerated, never pruned,
  listed under "Frozen — already drawn, do not redo" and never on the queue. The archived returns in
  `story/sheets/tilesets-valley-*.png` are kept with them so the atlas can be re-cut if the palette
  is refitted — verified: `ingest --force` on the decals package reproduced byte-identical output.
- **Hart, Guildclerk and Stolz reference sheets**, and the **villager_a / villager_b walkers**.
- The **stale Falke and Ottilie walkers are kept on disk** even though they show old designs, so the
  demo has sprites to walk until they are redrawn.

## Open question for the owner

The retired `003_the_warning` panels and their raw sheets were deleted with the rest of the Fair
Copy. An earlier memory records that art as canon, and the scene it belongs to no longer exists.
`git checkout legacy-final -- story/panels story/sheets` brings it all back if that was wrong.

## Notes for whoever works on the tool next

- The sprite cutter trims the frames of one sprite to **one shared box**. Do not "improve" it to
  trim each frame to its own silhouette: a creature that opens its mouth is a pixel wider in frame
  two, and the flip then reads as the whole animal twitching sideways.
- `SPRITE_SLOTS_MAX = 8`, and `cap_sheets` takes the sheet count first and divides evenly rather
  than filling greedily — greedy left a two-slot runt sheet with one creature drawn enormous.
- A `- frames: N` entry needs `- frame2:`/`- frame3:` lines saying what *moves*. Without them the
  prompt describes frame two in the same words as frame one and the generator draws the same
  picture twice. The tool warns.
- `ArtTray.swift`'s selftest still has fixtures for the retired `props` and `screen` package kinds.
  They run against a temp directory and exercise the multi-step prompt parsing, so they were left
  alone; they are not evidence that those kinds still exist.
- Dead code was removed by reachability from `main()`, not by hand, and the pass was run until it
  found nothing. If you delete a command, run that pass again rather than hunting helpers.
