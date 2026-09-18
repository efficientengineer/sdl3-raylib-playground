# halm — 5 buildings — save the image here

1. Open `prompt.md` in this folder. Attach the files it lists, in that order, then paste its prompt.
2. Save what ChatGPT gives back **into this folder**, named `returned.png`.
   The whole path is `story/packages/ch01/halm/buildings/returned.png`. A `.jpg` or `.webp` works too; the tool converts it.
3. From the repository root:

```
./story_prompt.py ingest story/packages/ch01/halm/buildings
```

That cuts the image into:

- `story/field/buildings/grain_shed_front.png`
- `story/field/buildings/grain_shed_side.png`
- `story/field/buildings/grain_shed_roof.png`
- `story/field/buildings/guild_hall_front.png`
- `story/field/buildings/guild_hall_side.png`
- `story/field/buildings/guild_hall_roof.png`
- `story/field/buildings/house_a_front.png`
- `story/field/buildings/house_a_side.png`
- `story/field/buildings/house_a_roof.png`
- `story/field/buildings/house_b_front.png`
- `story/field/buildings/house_b_side.png`
- `story/field/buildings/house_b_roof.png`
- `story/field/buildings/ladder_house_front.png`
- `story/field/buildings/ladder_house_side.png`
- `story/field/buildings/ladder_house_roof.png`

`returned.png` is never deleted, so a bad cut can be redone after a fix, and a regeneration is just
saving the new image over it and running `ingest` again. `./story_prompt.py ingest` with no path does
every package in `story/packages/` that has a new image waiting.
