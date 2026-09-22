// voxfield.h — the voxel + sprite field. src/VOXFIELD_NOTES.md is the contract.
//
// The owner's pivot (2026-09-19): "a voxel and sprite approach. World mostly rendered like Minecraft,
// but we have sprites for characters and detail." The tile field (tilefield.cpp) stays, parked behind
// a Dev button; SCR_FIELD opens this.
//
// The world is a 3D grid of block ids, one block to a walk cell, built at load from the SAME
// story/field/tmaps/*.tmap files the tile field reads — nothing new has to be authored. Blocks are
// flat master-palette colours with procedural pixel detail in the fragment shader; characters, props
// and detail are camera-facing billboards from the existing walker sheets, the tileset atlas and the
// tileset's decal PNGs.
#pragma once
#include <stdint.h>

struct VoxField;

enum VxEventKind {
    VXE_NONE = 0,
    VXE_SCENE,                       // ev.arg is a cutscene id; the game plays it and comes back
    VXE_ZONE,                        // ev.arg is an encounter table name
    // ── chapter one (src/chapter01.h) ──────────────────────────────────────────────────────────
    // The field reports what the player did; the chapter script decides what it means. The field
    // itself knows nothing about flags, goals or steps.
    VXE_TEXT,                        // ev.arg is the field-text id just shown (message/npc/trap).
                                     // This is how a mandatory examine is observed: the chapter
                                     // gates on it, the field just draws the box as it always did.
    VXE_FIGHT,                       // ev.arg is an encounter id (post, arm, swing, lid, burr,
                                     // lantern, fleece, klee). The game runs the fallback fight.
    VXE_PICKUP,                      // ev.arg is an item id, ev.arg2 the field-text id to show
    VXE_GOAL,                        // ev.arg is a goal line, underscores already spaces
    VXE_MAP,                         // ev.arg is the map just loaded (an exit was taken)
};

struct VxEvent { int kind; char arg[64], arg2[64]; };

#define VX_PARTY 4

// Everything a hot reload needs to put the party back where it stood, plus the look knobs the owner
// was tuning. POD, memcpy'd into star_logic.cpp's reload blob; every field is validated on the way in.
struct VxSave {
    char map[32];
    int32_t tx[VX_PARTY], ty[VX_PARTY], facing[VX_PARTY];
    int32_t party, steps;
    int32_t dbg_solid, dbg_trig, dbg_coord, noclip;
    int32_t ortho, hd2d, cutaway;
    float pitch, fov, view_h, tilt, ao, detail;
    float amb;
    int32_t light_table;
    float fog, dof, bloom, vignette, grade;
    int32_t motes, res_scale_pct;
    float sun_az, sun_el, sh_str, sh_soft;
    int32_t sh_snap;
    int32_t perf_hud;                 // the Perf HUD mode: 0 off, 1 compact, 2 full
    // Free movement (VOXFIELD_NOTES.md "Movement"). The position is a FLOAT now; facing survives,
    // and the body is always put back on the ground — a reload never lands you mid-jump.
    float px[VX_PARTY], pz[VX_PARTY];
    float agent_r, sp_walk, sp_run, jump_apex, gravity;
    int32_t nav_view;
};

VoxField *vx_create();
void vx_destroy(VoxField *v);

bool vx_load_map(VoxField *v, const char *name);
void vx_tick(VoxField *v, int w, int h, float dt, bool ui_blocked, VxEvent *ev);
bool vx_dev_ui(VoxField *v, char *out, int cap);
void vx_message(VoxField *v, const char *text);

// Desktop capture (capture.sh --vox): renders one frame at `w`x`h` into a PNG, no phone involved.
void vx_capture_to(VoxField *v, const char *map, int w, int h, const char *out_path);
bool vx_capture_done(VoxField *v);

// The desktop self-test (capture.sh --vox-selftest): loads every map once and fails loudly.
int vx_selftest(VoxField *v);

// The movement bot (capture.sh --vox-walktest [map|all]): drives the real movement code against
// walls, path-walks to every exit/door/NPC and jumps 200 times. Non-zero means a real failure.
int vx_walktest(VoxField *v, const char *one_map);

// ───────────────────────── the play-test bot's hands ─────────────────────────
// The dialogue box's own measurement with nothing drawn: pages needed, and whether any page would
// spill. `overflow` may be null. Needs an ImGui frame to be open (it reads the live font).
int vx_msg_measure(const char *who, const char *text, int w, int h, int *overflow);

// src/star_logic.cpp's --chapter-playtest drives the REAL game through these: a stick and two
// buttons, which land on exactly the variables the touch stick and the Act and Jump buttons land
// on, plus the A* the bot needs to steer. Everything else — triggers firing, boxes opening, events
// reaching the chapter, map changes — happens inside vx_tick exactly as it does for a thumb.
// Nothing here sets a flag or fires an event.
void vx_bot(VoxField *v, int on);
void vx_bot_stick(VoxField *v, float mx, float mz, int run);
void vx_bot_interact(VoxField *v);
void vx_bot_jump(VoxField *v);
void vx_bot_where(VoxField *v, float *x, float *z, int *airborne);
void vx_bot_cell(VoxField *v, int *cx, int *cz);
// Walk edges only: can the player get from one cell to another WITHOUT jumping?
bool vx_bot_can_walk(VoxField *v, int fx, int fz, int tx, int tz);
// Standable, but not walk-reachable — the engine's own definition of a jump-only place.
bool vx_bot_jump_only(VoxField *v, int cx, int cz);
// A* from where the party stands to a cell. Waypoints are NAV VOXELS; 0 means no walking route.
int vx_bot_path(VoxField *v, int tx, int tz, short *out_x, short *out_z, int cap);
void vx_bot_waypoint(VoxField *v, int wx, int wz, float *x, float *z);
// The id the interact button would act on from where the party stands, or "".
const char *vx_bot_examinable(VoxField *v);
// The centre cell of the trigger (or NPC) carrying this id. False = this map does not have it,
// which is what a flag with no trigger looks like from the play-test's side.
bool vx_find_trigger(VoxField *v, const char *id, short *cx, short *cz);
// Every trigger on this map that walking cannot reach but a jump or a drop can. Reported, never
// failed; chapter01.h's CH_JUMP_ONLY says which of them are the design.
int vx_bot_jump_targets(VoxField *v, const char **ids, short *xs, short *zs, int cap);

void vx_save(VoxField *v, VxSave *s);
void vx_restore(VoxField *v, const VxSave *s);

int vx_map_count();
const char *vx_map_name_at(int i);

// ───────────────────────── what the chapter script drives ─────────────────────────
// src/chapter01.h owns the story; the field owns the world. These are the only knobs between them.
// None of them knows what a flag or a step is.

// Which map the party is standing on right now.
const char *vx_current_map(VoxField *v);

// Load `map` and put the party on `x,z` facing `facing` ("S"/"W"/"E"/"N"). Used by a chapter step
// to place the player, not by the player walking through an exit. A NEGATIVE x or z means "use the
// map's own `spawn:` line", which is what a chapter step that does not care about the exact doorway
// passes. Standing inside a trigger after this never fires it: the party did not walk in.
void vx_goto(VoxField *v, const char *map, int x, int z, const char *facing);

// Time of day for this step (PALETTE.md's colormap tables: day, dusk, night, lamp, ...). Re-points
// the colormap row, moves the sun to that table's defaults and re-renders the shadow map. Cheap
// enough to call on a step change; NOT a per-frame call.
void vx_set_light(VoxField *v, const char *table, float level);

// A lamp that follows the party leader — the lantern on the high pasture. Looked up in the `lamp`
// colormap table, so it stays warm inside a blue night. radius in cells; level 0..1; on = 0 puts it
// out. Unlike the .tmap's static `lamp:` lines this one is NOT baked into the mesh.
void vx_set_party_lamp(VoxField *v, float radius, float level, int on);

// Distel's night sight: lifts the ambient a step and makes things flagged night-sight-only visible
// (the lens shelf). add is added to the map's own ambient; 0 turns it off.
void vx_set_night_sight(VoxField *v, float add);

// How many of the party are following (1..VX_PARTY).
void vx_set_party(VoxField *v, int count);

// Stop a trigger firing again once the chapter has consumed it (a mandatory examine, a pickup that
// has been taken). Matches on the trigger's arg id. Cleared by loading a map, so the chapter
// re-applies what it needs after a step change.
void vx_disable_trigger(VoxField *v, const char *arg_id);

// The same switch, both ways, and SILENT — the chapter's step/flag condition table (chapter01.h's
// CH_TRIG_COND) calls it for every conditioned trigger on every step change, and most of those are
// for some other map. Returns how many triggers matched.
int vx_set_trigger(VoxField *v, const char *arg_id, int on);

// Show a story/field/text.md id in the field's own box, from the game side.
void vx_say_id(VoxField *v, const char *id);

// True while the field is busy with something the chapter must not interrupt: a message box is
// open, or a map change is fading.
bool vx_busy(VoxField *v);

// Freeze the player where they stand (a scripted beat, a battle starting). Input is ignored and the
// party stops walking; the world keeps rendering.
void vx_freeze(VoxField *v, int on);

// Diagnostics for the play-test: what the field is showing, and whether a map change is in flight.
const char *vx_bot_boxtext(VoxField *v);
int vx_bot_fading(VoxField *v);
