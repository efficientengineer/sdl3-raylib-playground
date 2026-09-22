# Rendering — camera, sprites, the HD-2D pass, and performance

> Split out of `src/VOXFIELD_NOTES.md` (2026-09-21) when that file passed a thousand lines. Nothing
> was rewritten; the sections are exactly as they were. `src/ENGINE.md` is the map of `src/`, and
> `src/notes/` is the long-form record behind it.

Covers: why the camera is perspective, sprites, the HD-2D post pass, occlusion, the numbers,
and the whole performance record — instrumentation, HUD, benchmark, what was optimised and
measured, the spike recorder, and whether a deferred pass would pay.

## Camera, and why perspective

Fixed yaw, never rotating. **Perspective by default**, Octopath-style: pitch 42°, FOV 32°, about 11
blocks of vertical view, which puts the party at roughly a fifth of the screen. The camera stands
**south of the party looking north**, so east is on the right and north at the top — the tile field's
orientation, which is what the walker sheets were drawn for. (Standing north of the party and looking
south mirrors the world; that was the first build's other bug.) Ortho is a Dev toggle and a real
alternative; pitch, FOV and view height are sliders.

A **far fog** toward the horizon colour makes the far map edge fade instead of ending.

The **sky** is a full-screen gradient drawn before the world: a horizon colour up to a deeper zenith,
both palette indices through the colormap, with a slow procedural cloud band in the top half tinted
the lightest step of the plaster ramp (looked up by NAME — no colour is typed in). The fog colour and
the horizon are the same colour by construction.

## Sprites

- Characters are **billboards that lean back** toward the camera by `tilt` x pitch (0.68 by default),
  feet pinned to the ground, so an upright sprite is not foreshortened to 70% by a 42° camera.
- The **existing 9-frame walker sheets** (`story/field/walkers/<id>.png` + `.json`, rows S/side/N,
  columns stand/A/B, side row mirrored for east), decoded, **indexed on the master palette** and
  uploaded `GL_R8` — the same currency the blocks are drawn in, so a sprite and a wall can never
  disagree about a colour. `GL_NEAREST` both ways: the owner's pixels stay pixels.
- **Alpha-tested with depth write**, so they sort against the voxels for free. No sorting pass.
- A **soft blob shadow** under each, and **detail billboards** — the tileset's `decals/*.png` —
  hash-scattered over grass, about one in six cells, with a live density slider.
- **Lamps at night** get an additive soft glow billboard; the bloom does the rest. Off in full day.

## HD-2D post pass

One full-screen composite over three half-resolution passes, all of it behind a master **HD-2D**
toggle with a strength slider each:

1. **Tilt-shift** — the depth texture is read, linearised (perspective or ortho), and the blurred
   half-res copy mixed in by how far the fragment is from the party's own depth. A sharp band follows
   the party.
2. **Bloom** — the blurred copy blurred again and thresholded at 0.66 luma: lamps, water glints, night
   windows, sunlit plaster.
3. **Vignette** and 4. a **gentle grade** (warm highlights, cool shadows).

The pixel detail stays crisp inside the focus band: nothing in the pass smooths a texel, it only mixes
in a blurred copy outside the band.

## Occlusion

A **cutaway**: in the world fragment shader, anything inside a screen-space circle around the party
and nearer to the camera than they are is discarded, with a dithered edge. One `if`, no geometry, no
per-object bookkeeping — the robust option.

## Numbers

Measured on the Mac at 1920x1080 with `glFinish` around the world and post draws:

| | ms/frame |
|---|---|
| world only (`VOX_HD2D=0`) | **2.42** |
| world + HD-2D | **3.48** |

The post pass costs about **1.06 ms** at 1080p. Against the whole-cell build (1.96 / 2.90) the half-size
voxels plus the shadow map cost about **0.46 ms** of world time for 9x the triangles — the extra
shadow fetch set is four taps a fragment and the geometry is vertex-bound, not fill-bound. **On the
phone that split will not hold**: four `sampler2DShadow` taps at 1080p on a mobile GPU is the thing to
measure first, and the `res %` slider is the lever.

halm: 9 chunks, **73 810 triangles**, 25 draw calls, 330 sprites, 305 detail billboards, **2 207 shaped
voxels, 12 buildings, 2 ramps**, 29 ms to mesh, 2.3 ms to shadow, 0 px off-palette.
hart_yard: 15 748 tris, 290 shaped, 2 buildings. west_road: 19 992 tris, 349 shaped, 13 ramps.

**The phone has not been measured.** The device at 192.168.1.217:5555 was offline for this round, so
no `fast_reload.sh` run and no device `SELFCHECK`/frame line. The budget lever is there: the Dev
panel's **res %** slider renders the 3D at a fraction of the drawable and upscales, and it rides the
reload blob. Both `voxfield.cpp` and `star_logic.cpp` were compiled for `aarch64-none-linux-android24`
with the NDK and `-DIMGUI_IMPL_OPENGL_ES3` to prove the GLES 3 path builds.

## Performance

The owner, 2026-09-19: *"It looks amazing! It does feel slow. I need better tools to analyse what is
making it slow."* `src/vxperf.h` is those tools — header-only, included by `voxfield.cpp` alone, so
nothing in `CMakeLists.txt` or `fast_reload.sh` had to change to carry it.

**The first thing it found was a `glFinish()` in the frame path.** It had been left in to make a
CPU-side timer around `vx_render` + `vx_post` mean something, and it cost the phone a third of its
frame rate: on a tile-based GPU it flushes and waits inside every frame, so the CPU and the GPU never
overlap. Removing it took halm from **~47 fps / 21 ms to a vsync-locked 60 fps**. Per-pass GPU cost
now comes from timer queries, which do not stall. **Do not put a `glFinish` back on that line.** The
benchmark is the one caller allowed to stall, and only when the timer queries are unavailable.

### The instrumentation

- **Named scopes**, flat by construction: `tick/logic`, `mesh/upload`, `shadow map`, `sky`,
  `world opaque`, `sprites+detail`, `post: blur`, `post: bloom`, `post: composite`, `ui/dialogue`.
  Each one is timed on the CPU with `SDL_GetPerformanceCounter` and on the GPU with a timer query.
  Water is **not** a scope: water voxels live in the chunk meshes and are shaded by the world program,
  so their cost is inside `world opaque`. Saying so beats a scope that would always read 0.
- **GPU timers**: `GL_EXT_disjoint_timer_query` on GLES, `GL_TIME_ELAPSED` on desktop GL. Every entry
  point is fetched through `SDL_GL_GetProcAddress` (EXT name first, then core), because macOS's
  `gl3.h` does not declare `glGetQueryObjectui64v` and the GLES3 headers declare none of the EXT
  ones. Results are read **three frames late** out of a four-deep ring and the AVAILABLE flag is
  checked before the value is taken, so a query never blocks. `GL_GPU_DISJOINT` is read only when a
  slot actually holds a query and its samples are thrown away. The queries are **armed only when
  somebody is reading them** — `vxp_want_gpu(perf_hud > 0 || bench_on)`, or `VXPERF_GPU=1`. If anything is missing it falls back to CPU only and the HUD says
  `gpu timers unavailable`. **`GL_TIME_ELAPSED` cannot nest**: a scope opened inside another is timed
  on the CPU alone and its GPU column reads `-`.
- **TOTAL is wall clock, tick entry to tick entry** — the honest frame interval including the host's
  swap. That needed no hook in `host.cpp`, so none of this costs a `./deploy.sh`.
- **Counters**: draw calls, triangles after frustum culling, chunks drawn/total, sprites, detail
  billboards, FBO and drawable size, shadow size, and a GPU memory estimate (an estimate, and
  labelled one — GLES has no query for it).
- **Frame pacing**: the display's refresh rate from `SDL_GetCurrentDisplayMode`, the swap interval,
  and a histogram of frame intervals in vsync buckets (1x, 2x, 3x, 4x+, off-grid). A run that is
  missing vsync is visible as weight in the 2x bucket rather than as a worse average.

### The HUD

Dev panel → **Perf HUD: off / compact / full**. It rides the reload blob (`VxSave.perf_hud`), so a hot
reload keeps it up. Compact is fps, frame ms average / 1% worst / max and a 120-sample graph with the
16.7 and 33.3 ms lines on it. Full adds the per-scope CPU and GPU table sorted by cost, the counters,
the refresh rate and swap interval, the vsync buckets and the timer-query note. It is drawn into the
foreground draw list with no windows and one `snprintf` a line.

### The benchmark

Dev panel → **Run benchmark**, or a `perf.flag` in the pref dir (its contents name the map). It saves
the owner's settings, parks the party on a fixed view, and runs **20 configurations** for 30 warm-up
plus 240 measured frames at **two views** (halm: the square and the stream), then puts the settings
back. Configurations: baseline, HD-2D master, DoF, bloom, grade+vignette, shadows, shadow PCF 1 tap,
procedural pattern detail, AO, cutaway, detail sprites, water animation, sky clouds, fog, resolution
scale 100/85/75/66/50 %, and everything off. Output is a `PERF` line per configuration in the log and
`perf_report.md` / `perf_report.csv` in the pref dir, with the device and GL strings, the resolution
and the date, the per-scope table and the counters.

**Wall clock is vsync-bound and cannot separate two configurations that both beat the refresh period.
The column that can is `gpu ms`.** Read that one.

The `u_qpattern`, `u_qpcf`, `u_qwater` uniforms and the `q_*` fields exist **only** so the benchmark
can turn one thing off at a time. All three are uniform branches — one value for the whole draw — and
all three sit at the shipping look unless a benchmark moved them. They are not settings.

```
./perf.sh [map]        the phone: writes perf.flag, waits for `PERF done`, pulls the report, prints it
./perf.sh --desktop    the same benchmark in the desktop build
./perf.sh --watch      tail the periodic `voxfield perf:` lines from the phone
```

The periodic log line (every 300 frames) is now wall-clock ms with the 1% worst and the max, the GPU
total, the CPU total, and the three most expensive scopes, so a `logcat` tail alone says where the
time went.

### What was optimised, and what was measured

Measured on the phone (Adreno 650, 2400x1080, 60 Hz), halm, at the square:

| | GPU ms | wall ms | fps |
|---|---|---|---|
| before | 12.62 | 17.98 | 55.6 |
| after | see `build_desktop/perf/` | | |

1. **The `glFinish` is gone** from the non-capture path. The single biggest win, and it cost nothing.
2. **The sky is drawn after the opaque world, depth-rejected against it.** It used to be drawn first
   with the depth test off, shading all 2.59 M pixels — two octaves of value noise, eight hash
   evaluations each — and then being painted over by the town. The benchmark put `sky clouds off` at
   **-1.71 ms**, which is what that pass was worth. Now `VX_QUAD_VS` emits the quad at the far plane
   (`z = 1.0`) and the sky is drawn with `GL_LEQUAL` and no depth write, so every pixel the world
   already covers is rejected before the fragment shader runs. The pixels that survive are exactly
   the ones that were visible before, shaded by the same code. It is drawn **after the opaque pass and
   before the blended one** — the only order that is also correct, since a lamp glow over open sky
   would be overwritten if the sky came last.
3. **The cutaway `discard` is the first thing in the world fragment shader**, not the last. It used to
   sit after the whole pattern had been evaluated, so every discarded fragment paid for ten hash
   evaluations and a LUT fetch first. Same pixels out.
4. **A shadow strength of 0 now really skips the taps** in both the world and the sprite shader, which
   is what makes the benchmark's `shadows off` row mean anything.
5. **Uniform locations are cached** (`vx_uni`). The render path made about forty
   `glGetUniformLocation` string lookups a frame and they are the same forty every frame. The cache is
   keyed by program id and the name's address and is **reset in `vx_gl_init`**, because a fresh
   program can be handed a recycled id.

6. **The world shader is two programs, and only the chunks that need it get the one with `discard`.**
   A fragment shader that contains `discard` anywhere is one the GPU cannot assume writes depth at the
   rasterized value, so Adreno turns **LRZ — its early-Z / hidden-surface removal — off for the whole
   draw**. The cutaway's `discard` sat under a uniform branch in the one world shader, so the entire
   town was shaded with no early-Z: ten hash evaluations and four shadow taps a fragment, for pixels a
   nearer wall went on to cover. `VX_FS_HEAD` + `VX_FS_BODY` is now compiled twice, once with
   `VX_FS_CUT` between them and once without, and `vx_chunk_needs_cut` picks per chunk: a chunk
   entirely at or behind the party, or whose screen box misses the cutaway circle, has no fragment the
   block could discard and gets the discard-free program. On halm that is **6 of 7 visible chunks at
   the square and 2 of 5 at the stream**. The test is conservative everywhere it is unsure (a corner
   across the near plane → use the cutaway variant). Verified by forcing every chunk to the cutaway
   program and diffing five views: four byte-identical, one with 210 differing bytes of 8.29 M at a
   maximum of 1/255 — post-pass rounding from a different draw order, not a selection error.
7. **The visible chunks are drawn front to back.** An insertion sort over at most `VX_CHUNKS` boxes by
   view-space distance, which is free; without it early-Z has nothing to work with. Measured on the
   overdraw probe: square **1.747 → 1.645** writes per covered pixel, stream **1.939 → 1.910**.
   `VOX_CHUNKSORT=0` (index order) and `=-1` (back to front) exist only so that number can be taken.
8. **The GPU timer queries are only armed when somebody is reading them** (`vxp_want_gpu`, called with
   `perf_hud > 0 || bench_on`; `VXPERF_GPU=1` forces them on). Left on all the time they cost a
   `glGetIntegerv(GL_GPU_DISJOINT_EXT)` and up to `VXP_COUNT` `glGetQueryObjectuiv` a frame, and on
   Adreno a `glGet*` can flush the command stream — the instrumentation would be measuring itself.
   The CPU scopes and the spike recorder stay on always; they are two counter reads.
9. **The sprite vertex data is uploaded once a frame, not once a texture.** The kind-0 pass called
   `glBufferData` per texture — four walker sheets, the atlas and ten decals, fourteen orphan-and-
   upload round trips, each one a driver allocation the GPU may still be reading from. The whole array
   is now built first, uploaded once, and drawn with fourteen `glDrawArrays` offsets into it.
10. **Per-frame odds and ends that were pure overhead**: `SDL_GetCurrentDisplayMode` was called every
    frame for a refresh rate that never changes (now cached for a second); the GPU memory estimate
    looped over every decal every frame for a HUD that was closed (now only when it is open);
    `map.flag` and `perf.flag` were opened 2.5 and 2 times a second on the render thread (now once a
    second each, half a period out of phase so they can never charge the same frame).

None of these changes the look; the captures (`--vox halm`, `--viewh 24`, `--light night`) were
compared before and after. **No default visual quality or resolution scale was changed.** The
resolution rows in the report are there so the owner can make that call with numbers.

### The spike recorder

`p99` on the phone sat at 33-45 ms in almost every configuration — regular missed vsyncs — while the
averages were fine, and no average can tell you what happened in the one frame that went long. So:
when the frame interval exceeds `vxp.spike_ms` (25 ms), one `VXSPIKE` line is logged naming **every
scope's CPU time for that frame**, plus the total those scopes account for and what is left over.
It is rate-limited to one a second and says how many it swallowed. Read it like this:

- scopes add up to roughly the frame → the game did it, and the line names which pass;
- `unaccounted` is nearly the whole frame → the game did **not** do it. The time went somewhere the
  render thread was not running: the scheduler, a GPU queue the driver blocked on, or another process.

`cpu_ms` is this frame's, **zero included**, precisely so a stale number (`mesh/upload`'s 70 ms from
load time) can never be read as a cause forever after.

### Overdraw, and whether a deferred pass would pay

`vx_measure_overdraw` replays the visible chunks **in the order the real pass just drew them** with a
constant-colour probe blended `ONE/ONE` against a fresh depth buffer, then reads back a 1-in-4 row
sample. The byte in a pixel is the number of times the opaque world wrote it; the mean is over the
covered pixels, so the sky's empty half of the screen does not flatter it. It is a full pipeline
stall, so it runs only in the capture path (`VOX_OVERDRAW=1`), once a configuration in the benchmark
(the `overdraw` column), and at most once a second in the Dev panel's **Overdraw view**.

On halm at 1920x1080: **square 1.65, stream 1.91**. So a perfect depth prepass or a deferred pass
could save at most a third to a half of the world's shading — and would pay for it with a G-buffer
write and read of every pixel, which on a tile-based mobile GPU is the expensive half. **It does not
justify a deferred renderer.** The win was never the overdraw count: it was that `discard` had
switched early-Z off altogether, so the GPU was shading every rasterized fragment rather than the
1.65 that survive. Items 6 and 7 above are that fix. Re-measure on the phone before revisiting this.
