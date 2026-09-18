# stair_shrine — 6 props — save the image here

1. Open `prompt.md` in this folder. Attach the files it lists, in that order, then paste its prompt.
2. Save what ChatGPT gives back **into this folder**, named `returned.png`.
   The whole path is `story/packages/ch01/stair_shrine/props/returned.png`. A `.jpg` or `.webp` works too; the tool converts it.
3. From the repository root:

```
./story_prompt.py ingest story/packages/ch01/stair_shrine/props
```

That cuts the image into:

- `story/field/props/rubbing_stall.png`
- `story/field/props/shrine_house.png`
- `story/field/props/sitter.png`
- `story/field/props/step_rope.png`
- `story/field/props/stone_shelf.png`
- `story/field/props/water_jar.png`

`returned.png` is never deleted, so a bad cut can be redone after a fix, and a regeneration is just
saving the new image over it and running `ingest` again. `./story_prompt.py ingest` with no path does
every package in `story/packages/` that has a new image waiting.
