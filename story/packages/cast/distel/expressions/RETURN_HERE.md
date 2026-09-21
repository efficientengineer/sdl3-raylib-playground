# Distel — expression sheet — save the image here

1. Open `prompt.md` in this folder. Attach the files it lists, in that order, then paste its prompt.
2. Save what ChatGPT gives back **into this folder**, named `returned.png`.
   The whole path is `story/packages/cast/distel/expressions/returned.png`. A `.jpg` or `.webp` works too; the tool converts it.
3. From the repository root:

```
./story_prompt.py ingest story/packages/cast/distel/expressions
```

That cuts the image into:

- `story/portraits/distel_neutral.png`
- `story/portraits/distel_smile.png`
- `story/portraits/distel_laugh.png`
- `story/portraits/distel_biglaugh.png`
- `story/portraits/distel_concern.png`
- `story/portraits/distel_sorrow.png`
- `story/portraits/distel_annoyed.png`
- `story/portraits/distel_angry.png`
- `story/portraits/distel_shock.png`
- `story/portraits/distel_resolve.png`

`returned.png` is never deleted, so a bad cut can be redone after a fix, and a regeneration is just
saving the new image over it and running `ingest` again. `./story_prompt.py ingest` with no path does
every package in `story/packages/` that has a new image waiting.
