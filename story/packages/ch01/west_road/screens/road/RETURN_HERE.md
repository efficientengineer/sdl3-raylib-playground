# west_road / road — painted screen — save THREE images here

One chat, three messages: the painting, then the walkable mask, then the foreground mask. `prompt.md`
in this folder has all three prompts in order, and each one says which name to save under.

1. `returned.png` — the painting
2. `returned_walk.png` — the walkable mask (green on black)
3. `returned_fg.png` — the foreground mask (white on black)

`ingest` needs all three and refuses until they are all here. Then, from the repository root:

```
./story_prompt.py ingest story/packages/ch01/west_road/screens/road
```

Add `--debug` for a picture of what the tool understood. That writes:

- `story/field/screens/west_road_road_paint.png`
- `story/field/screens/west_road_road_walk.png`
- `story/field/screens/west_road_road_fg.png`
- `story/field/screens/west_road_road_base.png`
- `story/field/screens/west_road_road.screen`

Nothing here is ever deleted, so a bad cut is redone by fixing and rerunning, and a regeneration is
just saving the new image over the old one and running `ingest` again.
