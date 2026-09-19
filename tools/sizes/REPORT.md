# Asset size study (2026-09-19)

How big does each kind of art need to be? Scripts and raw numbers are in this folder (`out/*.json`,
strips in `out/strip_*.png`, nine of them pushed to the phone album `size_study`).

## The answer

Panels and the tile atlas are already the right size. **Portraits are about 4x too big in each dimension
and walker frames 2x too big**, and walk sheets carry four duplicate stand frames plus a side row that is
a mirror of the other.

| Class | measured art-pixel pitch | shown on phone (2400x1080) | shown at 1440p | stored before | **target stored size** |
|---|---|---|---|---|---|
| Walker frame | 3 px | 96x144 | 128x192 | 256x384 | **128x192** (4x logical) |
| Portrait | 5–6 px | ~133x206 | ~178x275 | ~550x850 | **~190x288** (288 tall, keep aspect) |
| Atlas tile / stamp | 3 px | 96 px per tile | 128 | 128 per tile | **128 per tile (keep)**; 64 is visibly mushy |
| Panel | 3 px | its rect on a 1693x1058 stage | x1.33 | ≈ phone size | **keep**: about 16.9 px per percent of stage width |

Rules that came out of it:
- **Store at the largest target display size, never below it and not far above it.** A rung that lands
  exactly on the display size loses nothing the display path was not already discarding.
- **Do not snap to the art-pixel grid** for anything with a face or fine detail: ChatGPT's fat pixels are
  not on a clean lattice (edge-phase lift 0.02–0.11 where a true grid scores ~1), and one-pixel-per-art-pixel
  versions break eyes, mouths and thin ties.
- ΔE thresholds are a sanity rail only: on AI-generated, already-resampled pixel art the metric is dominated
  by noise phase. Knees were set from display geometry and confirmed by eye.
- The cutscene player's textures use `GL_NEAREST` for minification; large sources shown small alias. Stored
  sizes near display size plus a linear/mipmapped min filter fix that.

## Generation guidance

- **Walkers**: 128x192 frames. With the 9-frame layout (rows S, side, N; columns stand, step-A, step-B) a
  sheet is 384x576, so **up to six characters fit one ~1.5 MP generation** (three with 16 frames).
- **Portraits**: cut from the reference sheet as now, resized on the way out; generation unchanged.
- **Stamps**: a 3x3 house is 384x384; about eight fit a sheet. Tiles: ~90 single cells per sheet.
- **Panels**: a wide panel needs ~881x401; a six-panel sheet gives each ~250k px, so a scene with a wide
  establishing shot should be one sheet per page.

## Redundancy (walkers)

Two frames that are the same drawing still differ by ΔE 0.06–0.10 (ChatGPT redraws its noise); that is the
floor.
- Columns 0 and 2 (the two stand frames) are at the floor for every row of every character: **4 of 16
  frames are duplicates**.
- The E row is a mirror of the W row for Ottilie and both villagers. **Falke is the exception**: shoulder
  plate and bracers swap sides when mirrored (owner accepted this trade-off).
- The walk cycle is not phase-offset between rows.
- Front/back step mirroring does **not** hold (2.5–5.4x the floor): the 6-frame layout is rejected.

| layout | frames | sheet at 128x192 | characters per generation | indexed bytes per character |
|---|---|---|---|---|
| old 4x4 at 256x384 | 16 | 1024x1536 | 1 | ~304 KB |
| **3x3, side mirrored at draw** | 9 | 384x576 | 6 | **~75 KB** |
| 4x3 for asymmetric characters | 12 | 512x576 | 4 | ~100 KB |

Other redundancy: `grass`/`grass_tuft`/`grass_flower` and `paving`/`paving_worn` are the same base drawn
twice (pairwise ΔE at the same-drawing floor) — one base plus decals. No panel repeats another; reference
sheets are not shipped.

## Projected shipped size

Panels are ~64% of what remains and are correctly sized, so the remaining lever is palettisation, not
resolution. Portraits 0.53 → 0.17 MB indexed; four walkers 1.21 → 0.30 MB indexed with the 9-frame layout.
