# Ottilie — expression sheet — save the image here

1. Open `prompt.md` in this folder. Attach the files it lists, in that order, then paste its prompt.
2. Save what ChatGPT gives back **into this folder**, named `returned.png`.
   The whole path is `story/packages/cast/ottilie/expressions/returned.png`. A `.jpg` or `.webp` works too; the tool converts it.
3. From the repository root:

```
./story_prompt.py ingest story/packages/cast/ottilie/expressions
```

That cuts the image into:

- `story/portraits/lyra_neutral.png`
- `story/portraits/lyra_smile.png`
- `story/portraits/lyra_laugh.png`
- `story/portraits/lyra_biglaugh.png`
- `story/portraits/lyra_concern.png`
- `story/portraits/lyra_sorrow.png`
- `story/portraits/lyra_annoyed.png`
- `story/portraits/lyra_angry.png`
- `story/portraits/lyra_shock.png`
- `story/portraits/lyra_resolve.png`

`returned.png` is never deleted, so a bad cut can be redone after a fix, and a regeneration is just
saving the new image over it and running `ingest` again. `./story_prompt.py ingest` with no path does
every package in `story/packages/` that has a new image waiting.
