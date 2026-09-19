# halm / town — painted map — save every image here

One chat, one message per image: the map, then the walkable mask. `prompt.md` in this folder has the
prompts in order and each one says which name to save under.

1. `returned.png` — the top-down map
2. `returned_walk.png` — the walkable mask (green on black)
3. `returned_over.png` — the overhead mask, **only** if `prompt.md` has a third message

`ingest` needs every image `prompt.md` asks for and refuses until they are all here. Then, from the repository root:

```
./story_prompt.py ingest story/packages/ch01/halm/screens/town
```

Add `--debug` for a picture of what the tool understood. That writes:

- `story/field/screens/halm_town_paint.png`
- `story/field/screens/halm_town_walk.png`
- `story/field/screens/halm_town.screen`

Nothing here is ever deleted, so a bad cut is redone by fixing and rerunning, and a regeneration is
just saving the new image over the old one and running `ingest` again.
