# Stolz — expression sheet — save the image here

1. Open `prompt.md` in this folder. Attach the files it lists, in that order, then paste its prompt.
2. Save what ChatGPT gives back **into this folder**, named `returned.png`.
   The whole path is `story/packages/cast/stolz/expressions/returned.png`. A `.jpg` or `.webp` works too; the tool converts it.
3. From the repository root:

```
./story_prompt.py ingest story/packages/cast/stolz/expressions
```

That cuts the image into:

- `story/portraits/stolz_neutral.png`
- `story/portraits/stolz_smile.png`
- `story/portraits/stolz_laugh.png`
- `story/portraits/stolz_biglaugh.png`
- `story/portraits/stolz_concern.png`
- `story/portraits/stolz_sorrow.png`
- `story/portraits/stolz_annoyed.png`
- `story/portraits/stolz_angry.png`
- `story/portraits/stolz_shock.png`
- `story/portraits/stolz_resolve.png`

`returned.png` is never deleted, so a bad cut can be redone after a fix, and a regeneration is just
saving the new image over it and running `ingest` again. `./story_prompt.py ingest` with no path does
every package in `story/packages/` that has a new image waiting.
