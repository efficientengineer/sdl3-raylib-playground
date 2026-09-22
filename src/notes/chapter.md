# The chapter, and what survives a reload

> Split out of `src/VOXFIELD_NOTES.md` (2026-09-21) when that file passed a thousand lines. Nothing
> was rewritten; the sections are exactly as they were. `src/ENGINE.md` is the map of `src/`, and
> `src/notes/` is the long-form record behind it.

Covers: what `src/chapter01.h`'s script drives through the field, and the hot-reload blob.
The chapter's RUNTIME is `src/chapter.cpp`; its DATA is `src/chapter01.h`.

### What the chapter script drives (`src/chapter01.h`)

`voxfield.h`'s bottom block. The field owns the world; the chapter owns the story; neither knows what
the other's nouns are.

| call | what it does |
|---|---|
| `vx_current_map` | the map the party is standing on |
| `vx_goto(map, x, z, facing)` | load and place. Triggers the party is standing in are marked already-entered, so a teleport never fires one. |
| `vx_set_light(table, level)` | re-points the colormap row, re-runs `vx_sun_defaults` for that table and **re-renders the shadow map**. A step-change call, not a per-frame one. |
| `vx_set_party_lamp(radius, level, on)` | the lantern that follows the leader — see below |
| `vx_set_night_sight(add)` | adds to the **effective ambient** (one place: `vx_set_common`), and makes `nightsight` triggers live |
| `vx_set_party(count)` | how many are following |
| `vx_disable_trigger(arg_id)` | matches on `arg` **or** `arg2`; cleared by a map load |
| `vx_say_id(id)` | a `story/field/text.md` id in the field's own box, without reporting it back |
| `vx_busy()` | a box is open, or a map change is fading |
| `vx_freeze(on)` | input ignored and the party stops walking; the world keeps rendering |

**The dynamic party lamp** is the only real rendering work here. The `.tmap`'s static `lamp:` lines
are **baked per-vertex into `VxVert.warm` at mesh time**, so a moving light cannot use that path at
all. Instead there is **exactly one** dynamic point light, as shader uniforms:

- `uniform vec4 u_dlamp` (world xyz, radius) and `uniform float u_dlev` in the world fragment shader,
  added to the warm term right where the baked `v_light.y` is read. It is a **uniform branch** — one
  value for the whole draw — so no wavefront diverges on it and the discard-free shader split is
  untouched.
- On the CPU, `vx_dyn_lamp_at` adds the same falloff inside `vx_light_at_foot`, so sprites light with
  it too. It is deliberately **not** in `vx_lamp_at`, which is what the mesh bake calls.
- It is looked up in the **`lamp` colormap row (`cmap_lamp_row0`)** exactly as the baked lamps are, so
  the lantern pool stays warm inside a blue night.
- It rides the leader, updated once a tick before the world draw reads the uniform.

Two capture-only switches, so the look can be judged before `chapter01` exists:
`VOX_LAMP="radius,level"` is `vx_set_party_lamp`, `VOX_NIGHTSIGHT=<add>` is `vx_set_night_sight`.
Neither is a setting anybody is meant to find.

## The reload blob

`RELOAD_MAGIC` went `STR9` → `STRA` → `STRC` → **`STRD`** (free movement: `VxSave` now carries a
FLOAT position and the five movement tunables; the party always comes back on the ground, never
mid-jump, and a blob written before free movement falls back to the cell). `VxSave` carries the map,
the party's position and facings, the
step count and every look knob (camera, HD-2D, light, res %), and `vx_restore` validates all of it —
a cell that is off the map or no longer standable is dropped rather than stranding the party in a
wall. The look knobs are applied **before** `vx_load_map`, the light table and ambient after, so what
the owner was tuning wins over the map's own `light:` line.
