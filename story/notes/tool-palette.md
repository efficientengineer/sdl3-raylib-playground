# tool-palette — the green lantern pool (2026-09-21)

The tool agent's report on the bug the engine agent found: the party's lantern pool on the
night pasture came out bright green. There were **three** faults, on both sides.

## 1. The engine's sample was off by one column (not a data fault)

The report read "grass index 24, raw (67,144,82)". The master palette says:

| index | colour |
| --- | --- |
| 23 | (67, 144, 82) |
| 24 | (88, 160, 79) |
| 25 | (54, 180, 219)  ← cyan, the head of the `sparkle` ramp |

So the raw colour quoted is index **23**, and the colormap column read was **24**. The same
shift explains the second, scarier symptom exactly: "index 25, raw (88,160,79) → day level 0
= (54,180,219), cyan" is raw at 24 against column 25. Column 25 is cyan because *index 25 is
cyan*. Checked directly: **`day` level 0 matches the palette for all 255 opaque indices, before
and after this change.** There is no misalignment in the file. Whatever reads the colormap on
the engine side is one column high — worth finding, because it silently recolours everything.

## 2. The colormap was refitted to palette indices (real, fixed)

`build_colormap` ended each entry with `row[i] = idx.of(*_from_oklab(...))` — a nearest-master
snap. The colormap is RGBA output; only *art* is palettised, so this was a quantiser on the one
product that never needed one.

| table | worst snap error (Oklab dE) | hue breaks between adjacent levels — before → after |
| --- | ---: | --- |
| day | 0.104 | 373 → 0 |
| dusk | 0.096 | 373 → 4 |
| night | 0.075 | 258 → 1 |
| lamp / lantern | 0.101 | 433 → 0 |
| flash | 0.058 | 462 → 0 |
| poison | 0.066 | 731 → 6 |
| stone | 0.054 | 4 → 0 |

A just-noticeable difference is about 0.01–0.02, so the worst entries were five to ten times
off. Worse than the size is the *kind* of error: the nearest palette entry to a lit colour is
often a step of a different ramp, so levels jumped families. Index 188 lit to (15,19,112), a
deep blue, and was stored as (3,34,59), a dark teal. Index 24 under `lamp` snapped from its
true (111,176,62) all the way out to index **105** — a different ramp entirely. Those jumps are
the "different hue" the report saw, and they staircase as the light level slides.

The few remaining breaks (dusk 4, night 1, poison 6) are near-neutral colours crossing the hue
circle at the very dark end, where chroma is almost nil. They are not visible.

## 3. Firelight was saturating instead of bleaching (real, fixed)

`lamp` used `C = C0 * 1.10 * (0.45 + 0.55*s)` — a **gain** — with `LAMP_TINT = (0.005, 0.022)`,
a cast far too weak to register against a green it had just boosted. So the pool under a lantern
was the most saturated green on a night screen. PALETTE.md already said lamp "takes chroma out";
the code did the opposite.

Now `LAMP_CHROMA = 0.70`, `C = C0 * 0.70 * (0.72 + 0.28*s)`, `LAMP_TINT = (0.012, 0.075)`.

| index | raw | lamp L0 before | lamp L0 after |
| --- | --- | --- | --- |
| 23 | (67,144,82) | (90,160,72) green | **(135,149,35)** warm olive |
| 24 | (88,160,79) | (106,170,70) green | **(153,165,29)** straw |

`NIGHT_TINT` also went from `(-0.004, -0.042)` to `(+0.004, -0.042)`: PALETTE.md says the cast
is blue-**violet**, and a negative `a` is a hair green, which made a moonlit neutral read cold
teal. One step, not a redesign.

## 4. Ordinary art sits on the reserved cycle indices — reported, not fixed

`cycles.md` reserves 3–28 and says "nothing else in the palette shares them… an index shared
with a roof tile would make the roof flicker too." That is not what shipped. Every reserved
index is in heavy use by ordinary art, because `palette build` puts real ramp colours in those
slots and the cutter's nearest-match then finds them like any other colour.

**No refit this round** — a palette version bump re-derives every image, which is its own job.

Pixels on reserved indices, across `story/field/tilesets`, `walkers`, `props`, `portraits`,
`panels` (indexed PNGs only):

| cycle | indices | total pixels on them |
| --- | --- | ---: |
| `water` | 3–10 | 121,175 |
| `fire_lamp` | 11–18 | 83,766 |
| `foliage_wind` | 19–24 | 19,593 |
| `sparkle` | 25–28 | 78,366 |

Worst files:

| file | reserved px | share of file | cycles hit |
| --- | ---: | ---: | --- |
| `tilesets/valley/swatches/water.png` | 76,622 | 52.0% | sparkle |
| `tilesets/valley/atlas.png` | 33,863 | 0.5% | water, fire_lamp, foliage_wind, sparkle |
| `tilesets/valley/cut/objects_3.png` | 14,751 | 1.0% | foliage_wind, water, fire_lamp |
| `tilesets/valley/swatches/mud.png` | 7,781 | 5.3% | water |
| `portraits/garbe.png` | 6,719 | 13.9% | fire_lamp |
| `portraits/hart*.png` (11 files) | ~5.8–6.7k each | 10.5–14.4% | water, fire_lamp |
| `walkers/villager_a.png` | 5,430 | 2.5% | water, fire_lamp |
| `walkers/ottilie.png` | 4,269 | 1.9% | water |
| `walkers/falke.png` | 3,744 | 1.7% | water |
| `portraits/distel*.png` (10 files) | ~2.7–2.9k each | 4.9–5.4% | water, fire_lamp |

Read that as: **the moment the engine starts cycling, Hart's face strobes.** A tenth of every
Hart portrait is on `water` and `fire_lamp` columns; `water.png`'s swatch is half on `sparkle`
(the 12 fps twinkle), which is a happy accident for water and a disaster for anything else
drawn from those slots. `foliage_wind` (19–24) is the grass ramp, which is why the engine
picked index 23/24 for pasture in the first place — those *are* ordinary grass colours today.

Two ways out, for the orchestrator to rule on:

1. **Give the cycles their own colours.** `palette build` lays 3–28 down from a family and then
   lets the ramp keep serving the corpus. Exclude 3–28 from `PalIndex` so the cutter can never
   choose them, bump the palette version, `ingest --force`. Costs 26 of 253 slots outright.
2. **Drop the reservation** and declare cycling dead for now. Nothing in the engine cycles yet,
   and `cycles.md` would become a description of colours rather than a promise about indices.

Option 1 is what `cycles.md` already claims is true, so it is the smaller lie to correct.

## Files changed

- `storytool/colormap.py` — no refit; `day` L0 is the identity row; lamp and night constants.
- `PALETTE.md` — the three rules the bug broke, written down.
- `story/palette/colormap.png`, `colormap.json` — regenerated.

`palette check` (87 images, 0 problems) and `check --all` pass. The golden report differs from
before the change in exactly `story/palette/colormap.png` and `colormap.json`, and nothing else.
