# valley — the decals (20 cut-outs) — save the image here

1. Open `prompt.md` in this folder. Attach the files it lists, in that order, then paste its prompt.
2. Save what ChatGPT gives back **into this folder**, named `returned.png`.
   The whole path is `story/packages/tilesets/valley/decals/returned.png`. A `.jpg` or `.webp` works too; the tool converts it.
3. From the repository root:

```
./story_prompt.py ingest story/packages/tilesets/valley/decals
```

That cuts the image into:

- `story/field/tilesets/valley/decals/clover.png`
- `story/field/tilesets/valley/decals/crack.png`
- `story/field/tilesets/valley/decals/crack_moss.png`
- `story/field/tilesets/valley/decals/daisy_patch.png`
- `story/field/tilesets/valley/decals/flower_red.png`
- `story/field/tilesets/valley/decals/flower_white.png`
- `story/field/tilesets/valley/decals/grass_sprout.png`
- `story/field/tilesets/valley/decals/leaf_fall.png`
- `story/field/tilesets/valley/decals/lily_pad.png`
- `story/field/tilesets/valley/decals/mushroom.png`
- `story/field/tilesets/valley/decals/mushroom_pair.png`
- `story/field/tilesets/valley/decals/pebble.png`
- `story/field/tilesets/valley/decals/pebbles.png`
- `story/field/tilesets/valley/decals/reed.png`
- `story/field/tilesets/valley/decals/reed_clump.png`
- `story/field/tilesets/valley/decals/stone_flat.png`
- `story/field/tilesets/valley/decals/straw.png`
- `story/field/tilesets/valley/decals/tuft.png`
- `story/field/tilesets/valley/decals/tuft_tall.png`
- `story/field/tilesets/valley/decals/twig.png`

`returned.png` is never deleted, so a bad cut can be redone after a fix, and a regeneration is just
saving the new image over it and running `ingest` again. `./story_prompt.py ingest` with no path does
every package in `story/packages/` that has a new image waiting.
