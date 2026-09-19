# valley — buildings_2 tile sheet (2 of 3) — save the image here

1. Open `prompt.md` in this folder. Attach the files it lists, in that order, then paste its prompt.
2. Save what ChatGPT gives back **into this folder**, named `returned.png`.
   The whole path is `story/packages/tilesets/valley/buildings_2/returned.png`. A `.jpg` or `.webp` works too; the tool converts it.
3. From the repository root:

```
./story_prompt.py ingest story/packages/tilesets/valley/buildings_2
```

That cuts the image into:

- `story/field/tilesets/valley/atlas.png`
- `story/field/tilesets/valley/cut/buildings_2.png`

`returned.png` is never deleted, so a bad cut can be redone after a fix, and a regeneration is just
saving the new image over it and running `ingest` again. `./story_prompt.py ingest` with no path does
every package in `story/packages/` that has a new image waiting.
