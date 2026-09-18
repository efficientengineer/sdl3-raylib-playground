# halm — 13 props — save the image here

1. Open `prompt.md` in this folder. Attach the files it lists, in that order, then paste its prompt.
2. Save what ChatGPT gives back **into this folder**, named `returned.png`.
   The whole path is `story/packages/ch01/halm/props/returned.png`. A `.jpg` or `.webp` works too; the tool converts it.
3. From the repository root:

```
./story_prompt.py ingest story/packages/ch01/halm/props
```

That cuts the image into:

- `story/field/props/barrel.png`
- `story/field/props/cart.png`
- `story/field/props/fence.png`
- `story/field/props/grain_shed.png`
- `story/field/props/guild_hall.png`
- `story/field/props/house_a.png`
- `story/field/props/house_b.png`
- `story/field/props/scale_bench.png`
- `story/field/props/sign.png`
- `story/field/props/tree_a.png`
- `story/field/props/tree_b.png`
- `story/field/props/well.png`
- `story/field/props/yard_wall.png`

`returned.png` is never deleted, so a bad cut can be redone after a fix, and a regeneration is just
saving the new image over it and running `ingest` again. `./story_prompt.py ingest` with no path does
every package in `story/packages/` that has a new image waiting.
