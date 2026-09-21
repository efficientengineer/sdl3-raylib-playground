# Guildclerk — expression sheet — save the image here

1. Open `prompt.md` in this folder. Attach the files it lists, in that order, then paste its prompt.
2. Save what ChatGPT gives back **into this folder**, named `returned.png`.
   The whole path is `story/packages/cast/guildclerk/expressions/returned.png`. A `.jpg` or `.webp` works too; the tool converts it.
3. From the repository root:

```
./story_prompt.py ingest story/packages/cast/guildclerk/expressions
```

That cuts the image into:

- `story/portraits/guildclerk_neutral.png`
- `story/portraits/guildclerk_smile.png`
- `story/portraits/guildclerk_laugh.png`
- `story/portraits/guildclerk_biglaugh.png`
- `story/portraits/guildclerk_concern.png`
- `story/portraits/guildclerk_sorrow.png`
- `story/portraits/guildclerk_annoyed.png`
- `story/portraits/guildclerk_angry.png`
- `story/portraits/guildclerk_shock.png`
- `story/portraits/guildclerk_resolve.png`

`returned.png` is never deleted, so a bad cut can be redone after a fix, and a regeneration is just
saving the new image over it and running `ingest` again. `./story_prompt.py ingest` with no path does
every package in `story/packages/` that has a new image waiting.
