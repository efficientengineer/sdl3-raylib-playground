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

## `## height` — authored elevation (NEW, parser pending)

`hill_path` and `high_pasture` carry an optional `## height` section. It is a **grid exactly like
`## ground`**: `size:` rows of `size:` characters, one per walk cell, giving that cell's surface
height in **VOXELS** (half a walk cell, the unit `VOXFIELD_NOTES.md` builds in).

```
## height            # optional; W chars x H rows, base36 per cell: 0-9 = 0..9, a-z = 10..35
```

- A character is read base36, so `0`..`9` then `a`..`z`, range **0..35 voxels** (0..17.5 cells).
- **`.` means "unauthored"**: that cell keeps the deterministic terrain height the builder already
  generates. A map with no `## height` section behaves exactly as it does today.
- An authored cell is **authoritative**: no procedural height, and it is **exempt from the
  neighbour-clamp pass** — the two-voxel clamp is what a cliff has to be allowed to break.
- Stamps and trigger rectangles are still flattened, to the height of their own top-left cell.
- Walk connectivity is unchanged (`<= 1` voxel is a walk edge, more is a ledge), which is what the
  two maps are authored against: every path climbs at most one voxel a cell, and every cliff, the
  two-cell drop on `hill_path` and the jump-only shelf on `high_pasture` are deliberate ledges.

## todo

Maps an exit is allowed to point at before they are written. Delete a name once its `.tmap` exists.

- (none — every exit target has a `.tmap`)
