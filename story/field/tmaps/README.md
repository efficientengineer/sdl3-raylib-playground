# story/field/tmaps

One `<map>.tmap` per map, in the plain-text format `TILES.md` fixes. The engine loads these; agents and
humans edit them by hand.

- `./story_prompt.py tmap check <map>` — or `--all`, which `./story_prompt.py check --all` also runs.
  It checks the legend against the map's tileset, every stamp's footprint and its `+` cells, stamps
  that overlap, the spawn and every exit or door target standing on a walkable tile, every `message`
  and `npc` text id against `story/field/text.md`, and every exit's target map.
- `./story_prompt.py tmap preview <map>` — renders the map from its tileset's `atlas.png` to
  `story/out/<map>.tmap.png`, drawing a flat colour for any tile whose atlas cell is still empty. That
  is how a map is judged on the Mac before any art exists.

## todo

Maps an exit is allowed to point at before they are written. Delete a name once its `.tmap` exists.

- `west_road`
