# valley — the ground swatches (9 terrains) — save the image here

1. Open `prompt.md` in this folder. Attach the files it lists, in that order, then paste its prompt.
2. Save what ChatGPT gives back **into this folder**, named `returned.png`.
   The whole path is `story/packages/tilesets/valley/swatches/returned.png`. A `.jpg` or `.webp` works too; the tool converts it.
3. From the repository root:

```
./story_prompt.py ingest story/packages/tilesets/valley/swatches
```

That cuts the image into:

- `story/field/tilesets/valley/swatches/grass.png`
- `story/field/tilesets/valley/swatches/grass_dry.png`
- `story/field/tilesets/valley/swatches/crop.png`
- `story/field/tilesets/valley/swatches/mud.png`
- `story/field/tilesets/valley/swatches/dirt.png`
- `story/field/tilesets/valley/swatches/gravel.png`
- `story/field/tilesets/valley/swatches/paving.png`
- `story/field/tilesets/valley/swatches/bridge_deck.png`
- `story/field/tilesets/valley/swatches/water.png`

`returned.png` is never deleted, so a bad cut can be redone after a fix, and a regeneration is just
saving the new image over it and running `ingest` again. `./story_prompt.py ingest` with no path does
every package in `story/packages/` that has a new image waiting.
