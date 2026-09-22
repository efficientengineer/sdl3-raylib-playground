// vox_internal.h — the voxel field's internal contract, shared by src/vox_*.cpp ONLY.
// OWNS: the world's constants and dimensions, the block/shape tables, every plain-old-data struct
//       the field keeps (triggers, NPCs, art, sprites, chunks) and `struct VoxField` itself, plus
//       the small pure helpers (trim, hash, lerp, Oklab) each module wants a copy of.
// NEVER: allocates, touches GL, reads a file at include time, or is included from outside src/vox_*.
//        Nothing here is part of the game's API — src/voxfield.h is. The game never sees this file.
// EXPOSES: struct VoxField, the VX_* constants, and (at the bottom) every function that crosses a
//        module seam. A function used by one module only stays `static` in that module's .cpp.
// See src/ENGINE.md for the module map and src/notes/ for what each system actually does.
#pragma once
#include <SDL3/SDL.h>
#include <math.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#if defined(__ANDROID__)
#include <GLES3/gl3.h>
#else
#include <OpenGL/gl3.h>
#endif
#include "imgui.h"
#include "stb_image.h"          // implementations live in star_logic.cpp
#include "stb_image_write.h"
#include "voxfield.h"
#include "dialogue.h"
#include "field_text.h"
#include "vxperf.h"             // named CPU/GPU scopes, the HUD and the benchmark's numbers

#define PREF_ORG "com.playground"
#define PREF_APP "questglory"

// A WALK CELL is one world unit and never changes: the maps, triggers, characters, camera and the
// 16-texel pattern grid are all in walk cells. A VOXEL is HALF a walk cell on every axis (the owner,
// 2026-09-19: "4 blocks should be able to fit into the smallest block size now… more detail, feel
// different than Minecraft"), so the block grid is 2x2x2 voxels to the cell and everything structural
// is authored in voxels. VX_V* are voxel counts, VX_MAX* are cell counts.
#define VX_MAXW  96              // map cells east
#define VX_MAXD  96              // map cells south
#define VX_VPC   2               // voxels per walk cell, per axis
#define VOX_S    0.5f            // one voxel, in world units
#define VX_VW    (VX_MAXW * VX_VPC)   // 192 voxels east
#define VX_VD    (VX_MAXD * VX_VPC)   // 192 voxels south
#define VX_VY    64                   // voxels up (32 walk cells) — a `## height` map may climb
#define VX_CHV   32              // chunk footprint, in voxels (= 16 walk cells, as before)
#define VX_CHX   (VX_VW / VX_CHV)
#define VX_CHZ   (VX_VD / VX_CHV)
#define VX_CHUNKS (VX_CHX * VX_CHZ)
#define VX_TRIGS 128
// The tallest a `## height` cell may be, in VOXELS above the map base, so base + h + 1 fits VX_VY.
#define VX_HMAX  (VX_VY - 6)
#define VX_EVQ   16              // events a tick may produce; drained one a tick, never dropped
#define VX_NPCS  48
#define VX_ART   16
#define VX_LIGHTS 16
#define VX_DEFS  192             // tiles.md entries
#define VX_PLACE 768             // stamps placed on a map
#define VX_SPRITES 4096          // billboards in one frame
#define VX_LEVELS 32

#define WALK_STEP 0.16f
#define RUN_STEP  0.10f
#define TURN_HOLD 0.07f
#define FADE_FRAMES 8
#define VX_TYPE_CPS 42.0f

// ── free movement on a generated navmesh (VOXFIELD_NOTES.md "Movement") ──
// The owner, 2026-09-20: "we need to use a navmesh for movement… 8 directional movement instead of
// the 4 we have now, and jumping", then "fully free is better for touch". So the party is a POINT
// with a radius, not a cell, and the nav data is generated from the voxel grid at load.
//
// VX_AGENT_R is the one number everything else is proved against. A nav voxel is VOX_S (0.5 cells)
// across, so the centre of one is 0.25 cells from each of its own edges. Keeping the radius UNDER
// 0.25 means the circle at any free nav voxel's centre can never reach a blocked voxel, and the
// straight segment between two 4-adjacent free centres runs down the middle of a 0.5-wide strip —
// so walk connectivity on the nav grid ALWAYS survives erosion, and the reachability proof and the
// collision can never disagree. That is the whole reason for the value; do not raise the default
// above 0.24 without re-reading "Movement" in the notes.
#define VX_AGENT_R    0.24f      // walk cells
#define VX_SP_WALK    4.4f       // cells/second
#define VX_SP_RUN     7.2f
#define VX_GRAVITY    40.0f      // cells/second^2
#define VX_JUMP_APEX  1.25f      // cells (= one walk-cell height and a quarter)
#define VX_COYOTE     0.080f
#define VX_JUMPBUF    0.100f
#define VX_AIR_CTRL   0.40f      // of the ground acceleration
#define VX_BODY_H     1.6f       // the party sprite's height, in cells
#define VX_STEP_UP    (VOX_S + 0.001f)   // one voxel of rise is a free, smooth step
// How far up or down the interact button can reach, in world units. Two voxels: you can examine
// something a step above or below you and nothing on a roof. See the exam cone in vx_tick.
#define VX_EXAM_REACH (VOX_S * 2.0f + 0.001f)
#define VX_EXAM_Q     6          // triggers one cell may deliver from a single press
// How bright the party's lantern is allowed to make a surface, as a light level into the `lamp`
// colormap row. The row's top is paper white; a pool that reaches it reads as a torch in a cave
// rather than a lantern in a field. Tuned by eye on high_pasture at night (owner: warm straw, not
// neon). The world shader needs it as GLSL text, hence the pair.
#define VX_LAMP_CAP   0.62f
#define VX_LAMP_CAP_S "0.62"
#define VX_FACE_HYST  55.0f      // degrees off the current facing's axis before it flips
#define VX_TRAIL      512        // breadcrumbs the followers walk
#define VX_TRAIL_DS   0.06f      // cells between breadcrumbs
#define VX_FOLLOW_D   1.15f      // cells behind, per party slot
#define VX_NPC_R      0.28f      // an NPC's soft push-out circle
#define VX_MAX_SUBSTEP (VOX_S * 0.5f)    // never tunnel: half a nav voxel a sub-step

static const int DX[4] = { 0, -1, 1, 0 }, DZ[4] = { 1, 0, 0, -1 };
static const char *FACE_NAME[4] = { "S", "W", "E", "N" };
static int facing_of(const char *s) { return s[0] == 'W' ? 1 : s[0] == 'E' ? 2 : s[0] == 'N' ? 3 : 0; }

// VX_MAPS and the two accessors live in voxfield.cpp: one definition, one copy.

static const char *vx_pref() {
    static char path[512];
    static bool got = false;
    if (!got) { const char *p = SDL_GetPrefPath(PREF_ORG, PREF_APP); snprintf(path, sizeof(path), "%s", p ? p : ""); got = true; }
    return path;
}
// Phone pref path first (fast_reload.sh pushes there), then the APK assets, then the repo on desktop.
static void *vx_read(const char *rel, size_t *size) {
    char path[768];
    snprintf(path, sizeof(path), "%s%s", vx_pref(), rel);
    void *d = SDL_LoadFile(path, size);
    if (!d) d = SDL_LoadFile(rel, size);
    if (!d) { snprintf(path, sizeof(path), "story/%s", rel); d = SDL_LoadFile(path, size); }
    return d;
}

static char *vx_trim(char *s) {
    while (*s == ' ' || *s == '\t') s++;
    char *e = s + strlen(s);
    while (e > s && (e[-1] == ' ' || e[-1] == '\t' || e[-1] == '\r')) *--e = 0;
    return s;
}

static uint32_t vx_h32(int a, int b, int c) {         // FNV-1a over three ints, the tool's hash
    uint32_t h = 2166136261u;
    int vals[3] = { a, b, c };
    for (int i = 0; i < 3; i++) {
        uint32_t x = (uint32_t)vals[i];
        h = (h ^ (x & 255)) * 16777619u; h = (h ^ ((x >> 8) & 255)) * 16777619u;
        h = (h ^ ((x >> 16) & 255)) * 16777619u; h = (h ^ ((x >> 24) & 255)) * 16777619u;
    }
    return h;
}
static float vx_rnd(int a, int b, int c) { return (float)(vx_h32(a, b, c) & 0xFFFF) / 65535.0f; }
static float vx_lerp(float a, float b, float k) { return a + (b - a) * k; }

// ───────────────────────── Oklab, exactly the tool's transform ─────────────────────────

static float srgb_lin(float c) { return c <= 0.04045f ? c / 12.92f : powf((c + 0.055f) / 1.055f, 2.4f); }
static float lin_srgb(float c) {
    c = c < 0 ? 0 : c > 1 ? 1 : c;
    return c <= 0.0031308f ? c * 12.92f : 1.055f * powf(c, 1.0f / 2.4f) - 0.055f;
}
static void rgb_to_oklab(float r, float g, float b, float *L, float *A, float *B) {
    float lr = srgb_lin(r), lg = srgb_lin(g), lb = srgb_lin(b);
    float l = cbrtf(0.4122214708f * lr + 0.5363325363f * lg + 0.0514459929f * lb);
    float m = cbrtf(0.2119034982f * lr + 0.6806995451f * lg + 0.1073969566f * lb);
    float s = cbrtf(0.0883024619f * lr + 0.2817188376f * lg + 0.6299787005f * lb);
    *L = 0.2104542553f * l + 0.7936177850f * m - 0.0040720468f * s;
    *A = 1.9779984951f * l - 2.4285922050f * m + 0.4505937099f * s;
    *B = 0.0259040371f * l + 0.7827717662f * m - 0.8086757660f * s;
}

// ───────────────────────── blocks ─────────────────────────
// A block type is two palette ramps (top and side), six steps of each, and a pattern the fragment
// shader draws with. No textures anywhere in the world: PALETTE.md's 256 colours are the whole
// material library, and the detail is arithmetic on a world position quantised to 1/16 of a block.

enum {
    B_AIR = 0, B_GRASS, B_GRASS_DRY, B_DIRT, B_MUD, B_GRAVEL, B_PAVING, B_STONE,
    B_PLASTER, B_TIMBER, B_PLANK, B_ROOF, B_THATCH, B_WATER, B_WATERBED, B_CROP,
    B_LEAVES, B_TRUNK, B_DOOR, B_WINDOW, B_HEDGE, B_WETSTONE, B_SLATE, B_COUNT
};
// patterns
enum { P_MOTTLE = 0, P_GRASS, P_COBBLE, P_PLANK, P_ROOFTILE, P_WATER, P_THATCH, P_CROP,
       P_LEAVES, P_BARK, P_FLAT, P_STONEWALL, P_PLASTER };

struct VxBlockDef {
    const char *name;
    const char *top_ramp; float top_lo, top_hi;
    const char *side_ramp; float side_lo, side_hi;
    int pattern;
    bool solid;            // blocks walking and sight
    bool water;
};

// lo/hi are fractions along the ramp, dark -> light. Six steps are laid between them.
static const VxBlockDef BLOCKS[B_COUNT] = {
    { "air",       "",                              0,0,    "",                              0,0,    P_FLAT,      false, false },
    { "grass",     "foliage green (warm)",       0.30f,0.72f, "earth / dirt",              0.28f,0.58f, P_GRASS,     true,  false },
    { "grass_dry", "olive / dry grass",          0.28f,0.80f, "earth / dirt",              0.30f,0.60f, P_GRASS,     true,  false },
    { "dirt",      "earth / dirt",               0.42f,0.74f, "earth / dirt",              0.26f,0.56f, P_MOTTLE,    true,  false },
    { "mud",       "earth / dirt",               0.16f,0.44f, "earth / dirt",              0.12f,0.38f, P_MOTTLE,    true,  false },
    { "gravel",    "neutral cool (stone, steel)",0.36f,0.72f, "neutral cool (stone, steel)",0.28f,0.56f, P_MOTTLE,    true,  false },
    { "paving",    "neutral cool (stone, steel)",0.40f,0.78f, "neutral cool (stone, steel)",0.30f,0.60f, P_COBBLE,    true,  false },
    { "stone",     "neutral cool (stone, steel)",0.34f,0.70f, "neutral cool (stone, steel)",0.26f,0.66f, P_STONEWALL, true,  false },
    { "plaster",   "neutral warm (plaster, cloth)",0.52f,0.92f,"neutral warm (plaster, cloth)",0.44f,0.88f,P_PLASTER,  true,  false },
    { "timber",    "wood",                       0.24f,0.54f, "wood",                      0.20f,0.50f, P_BARK,      true,  false },
    { "plank",     "wood",                       0.40f,0.76f, "wood",                      0.34f,0.66f, P_PLANK,     true,  false },
    { "roof",      "roof red / brick",           0.34f,0.76f, "roof red / brick",          0.26f,0.62f, P_ROOFTILE,  true,  false },
    { "thatch",    "olive / dry grass",          0.40f,0.86f, "olive / dry grass",         0.30f,0.70f, P_THATCH,    true,  false },
    { "water",     "teal / shallow water",       0.30f,0.95f, "water blue / metal",        0.20f,0.60f, P_WATER,     false, true  },
    { "waterbed",  "earth / dirt",               0.14f,0.40f, "earth / dirt",              0.12f,0.34f, P_MOTTLE,    true,  false },
    { "crop",      "olive / dry grass",          0.24f,0.70f, "earth / dirt",              0.26f,0.52f, P_CROP,      true,  false },
    { "leaves",    "foliage green (warm)",       0.22f,0.72f, "foliage green (warm)",      0.16f,0.62f, P_LEAVES,    true,  false },
    { "trunk",     "wood",                       0.20f,0.48f, "wood",                      0.16f,0.46f, P_BARK,      true,  false },
    { "door",      "wood",                       0.14f,0.38f, "wood",                      0.12f,0.34f, P_PLANK,     true,  false },
    { "window",    "water blue / metal",         0.08f,0.55f, "water blue / metal",        0.08f,0.55f, P_FLAT,      true,  false },
    { "hedge",     "foliage green (cool)",       0.20f,0.62f, "foliage green (cool)",      0.16f,0.56f, P_LEAVES,    true,  false },
    { "wetstone",  "neutral cool (stone, steel)",0.12f,0.42f, "neutral cool (stone, steel)",0.08f,0.38f, P_STONEWALL, true,  false },
    { "slate",     "neutral cool (stone, steel)",0.20f,0.56f, "neutral cool (stone, steel)",0.16f,0.50f, P_ROOFTILE,  true,  false },
};

static bool blk_solid(unsigned char b) { return b != B_AIR && BLOCKS[b].solid; }
static bool blk_opaque(unsigned char b) { return b != B_AIR && b != B_WATER; }   // water's neighbours still draw

// ───────────────────────── shapes ─────────────────────────
// A parallel uint8 grid gives every voxel a SHAPE and an ORIENTATION: shp = shape * 8 + orient, so
// SH_CUBE with orient 0 is 0 and an untouched grid is all cubes. Orientations that name a compass
// direction use D_* below, which index DIRV[] (the horizontal unit vector) — never a literal.
//
// At half-voxel scale slab/post/pane are smaller than they were, and the call (owner asked) is that
// they all still earn their place: a slab is a quarter of a walk cell and makes eaves, sills and
// ridges; a post is a fence upright with rails; a pane is glass, a shutter and a railing infill.
enum { SH_CUBE = 0, SH_SLOPE, SH_SLOPE_OUT, SH_SLOPE_IN, SH_SLAB, SH_STAIRS, SH_POST, SH_PANE, SH_COUNT };
enum { D_S = 0, D_N, D_E, D_W };                    // +z, -z, +x, -x
static const int DIRV[4][2] = { {0,1}, {0,-1}, {1,0}, {-1,0} };   // x, z
// The six faces, in the order shape_covers() and the mesher both use. Declared here rather than in
// the meshing section because the navmesh asks "does this shape cover its top?" long before then.
enum { F_TOP = 0, F_BOT, F_SOUTH, F_NORTH, F_EAST, F_WEST };
static int dir_cw(int d)  { return d == D_S ? D_W : d == D_W ? D_N : d == D_N ? D_E : D_S; }
static int dir_opp(int d) { return d == D_S ? D_N : d == D_N ? D_S : d == D_E ? D_W : D_E; }

#define SHP(shape, orient) ((unsigned char)((shape) * 8 + (orient)))
#define SH_OF(s)  ((s) >> 3)
#define OR_OF(s)  ((s) & 7)
enum { SL_BOT = 0, SL_TOP = 1 };                                  // SH_SLAB orientations
enum { PN_S = 0, PN_N, PN_E, PN_W, PN_MIDX, PN_MIDZ };            // SH_PANE orientations

// Does this shape fill the whole of that face of its cell? A face may only be culled against a
// neighbour that covers the shared boundary — when in doubt this says false and the face is drawn.
static bool shape_covers(unsigned char s, int face) {              // face: F_TOP..F_WEST ordering
    int sh = SH_OF(s), o = OR_OF(s);
    if (sh == SH_CUBE) return true;
    if (face == 1 /*F_BOT*/) return sh == SH_SLOPE || sh == SH_SLOPE_OUT || sh == SH_SLOPE_IN
                                 || sh == SH_STAIRS || (sh == SH_SLAB && o == SL_BOT);
    if (face == 0 /*F_TOP*/) return sh == SH_SLAB && o == SL_TOP;
    int d = face - 2;                                              // F_SOUTH..F_WEST -> D_S..D_W
    if (d < 0 || d > 3) return false;
    if (sh == SH_SLOPE || sh == SH_STAIRS) return o == d;          // the high end is a full square
    if (sh == SH_SLOPE_IN) return o == d || dir_cw(o) == d;
    return false;                                                  // slab sides, out-corner, post, pane
}

// The top surface height (0..1 within the voxel) at each of the four plan corners, in the order
// (x0,z0), (x1,z0), (x1,z1), (x0,z1). Only the ramp shapes use it.
static void shape_corner_h(unsigned char s, float h[4]) {
    int sh = SH_OF(s), o = OR_OF(s);
    static const int CX[4] = { 0, 1, 1, 0 }, CZ[4] = { 0, 0, 1, 1 };
    for (int i = 0; i < 4; i++) h[i] = 1.0f;
    if (sh == SH_SLOPE || sh == SH_STAIRS) {
        for (int i = 0; i < 4; i++) {
            int on = (DIRV[o][0] ? (CX[i] == (DIRV[o][0] > 0 ? 1 : 0)) : (CZ[i] == (DIRV[o][1] > 0 ? 1 : 0)));
            h[i] = on ? 1.0f : 0.0f;
        }
    } else if (sh == SH_SLOPE_OUT || sh == SH_SLOPE_IN) {
        int a = o, b = dir_cw(o);
        for (int i = 0; i < 4; i++) {
            bool ona = DIRV[a][0] ? (CX[i] == (DIRV[a][0] > 0 ? 1 : 0)) : (CZ[i] == (DIRV[a][1] > 0 ? 1 : 0));
            bool onb = DIRV[b][0] ? (CX[i] == (DIRV[b][0] > 0 ? 1 : 0)) : (CZ[i] == (DIRV[b][1] > 0 ? 1 : 0));
            h[i] = (sh == SH_SLOPE_OUT) ? ((ona && onb) ? 1.0f : 0.0f) : ((ona || onb) ? 1.0f : 0.0f);
        }
    }
}

static bool shape_is_ramp(unsigned char s) {
    int sh = SH_OF(s);
    return sh == SH_SLOPE || sh == SH_STAIRS || sh == SH_SLOPE_OUT || sh == SH_SLOPE_IN;
}
// The walkable surface height of a ramp voxel at a fractional plan position, 0..1 inside the voxel.
static float shape_height_at(unsigned char s, float fu, float fw) {
    if (!shape_is_ramp(s)) return 1.0f;
    float h[4]; shape_corner_h(s, h);
    float a = h[0] + (h[1] - h[0]) * fu, b = h[3] + (h[2] - h[3]) * fu;
    return a + (b - a) * fw;
}

// ───────────────────────── data ─────────────────────────

enum { TG_MESSAGE = 0, TG_EXIT, TG_DOOR, TG_ZONE, TG_TRAP, TG_SCENE,
       TG_FIGHT, TG_PICKUP, TG_GOAL, TG_SPRITE };
// A trigger walking cannot reach but a jump or a drop can. See vx_nav_check_targets.
#define VX_JUMPTGT 32
struct VxJumpTgt { short x, z, kind; char arg[96]; };
#define TGM(k) (1u << (k))

// `arg2` is the second argument a `pickup` carries (its field-text id). `off` is set by
// vx_disable_trigger (the chapter consumed it) and cleared by a map load; `taken` is a pickup
// that has been taken; `night` is the `nightsight` flag — hidden until vx_set_night_sight is on.
struct VxTrig { short x, z, w, d; int kind; char arg[96], arg2[64], map[32]; short ax, az, af;
                bool inside; unsigned char off, taken, night; short sprart; };
struct VxNpc {
    short hx, hz, tx, tz, px, pz;
    float t, wait, y, py_;
    int facing, wander, art, parity;
    char walker[24], text[48];
    float x, z, gx, gz, phase;         // free movement: world position and the current wander goal
};
struct VxArt {                                  // a walker sheet, indexed on the master palette
    char id[24];
    GLuint tex;
    int w, h, fw, fh, nrows, ncols;
    int row_of[4]; bool flip[4];
    int col_stand, col_a, col_b;
};
// An actor is a POINT now. `x/z/y` are world units and are the truth; `tx/tz` are floor(x),floor(z)
// and exist only so the cell-based things (triggers, the Dev readout, the reload blob) keep working.
// `px/pz/y0` are what is left of the grid walk and are kept so a saved blob still loads.
struct VxActor { short tx, tz, px, pz; int facing; float y, y0; float x, z, ang, phase; };

// One breadcrumb of the leader's path. The followers walk this, so they reproduce a jump at exactly
// the spot the leader jumped, with no second simulation to get wrong.
struct VxCrumb { float x, z, y, s; unsigned char air; };
struct VxLight { float x, z, y, r, level; int flicker; };

// One tiles.md entry, only what the voxel builder needs: how big a stamp is and what it is.
// A FIELD SPRITE (VOXFIELD_NOTES.md "Field sprites"): story/field/sprites/<id>.png, an indexed PNG
// on the master palette, index 0 transparent, anchored BOTTOM-CENTRE, 64 px to the map cell. The
// optional sidecar <id>.json gives frames/frame/sheet/footprint/fps/loop; `frame` is READ, never
// derived by dividing. `tex` == 0 means the art is not on disk yet and a placeholder is drawn.
#define VX_SPRART 32
struct VxSprArt {
    char id[32];
    GLuint tex;
    int w, h;                       // the sheet, in pixels
    int fw, fh;                     // one frame, in pixels (from the json, or the whole sheet)
    int frames, fps, pingpong;
    float cw, ch;                   // the billboard, in map cells (footprint, or frame / 64)
};

struct VxDef { char name[24]; short index, w, h; unsigned char solid[8]; unsigned char kind; };
enum { DK_TILE = 0, DK_TERRAIN, DK_DECAL };
struct VxPlace { short def, x, z; };

// A billboard, in world units. `tex` is an indexed R8 texture; `lit`/`warm` are its light at the foot.
struct VxSpr {
    float x, y, z, w, h;               // foot centre and size in blocks
    float u0, v0, u1, v1;
    GLuint tex;
    float lit, warm, tilt, alpha;
    int kind;                          // 0 sprite, 1 blob shadow, 2 additive glow
};

struct VxVert {                        // 32 bytes
    float x, y, z;                         // world position
    float u, w;                            // the pattern's own 2D basis, in world units (16 texels each)
    unsigned char type, face, ao, spare;   // attr 2: integer. spare = per-vertex extra (water shallows)
    unsigned char lit, warm, pad[2];       // attr 3: normalised. lit = the face term, warm = lamp
    signed char nx, ny, nz, npad;          // attr 4: the real normal, for the shadow map's offset
};

struct VxChunk { GLuint vbo; int verts, cap; float lo[3], hi[3]; };

struct VoxField {
    // map
    char map_name[32], set[24], music[24];
    int mw, md;                                  // cells
    int vw, vd;                                  // voxels  (= mw * VX_VPC, md * VX_VPC)
    unsigned char blk[VX_VY][VX_VD][VX_VW];
    unsigned char shp[VX_VY][VX_VD][VX_VW];      // shape * 8 + orientation; 0 = a plain cube
    unsigned char hgt[VX_MAXD][VX_MAXW];         // walkable top, IN VOXELS: the voxel you stand ON
    unsigned char walk[VX_MAXD][VX_MAXW];        // 1 = a body may stand here
    unsigned char ramp[VX_MAXD][VX_MAXW];        // 1 = this cell's surface is a ramp, crossed smoothly
    unsigned char tsolid[VX_MAXD][VX_MAXW];      // the tile map's own solidity, for the check
    short ground[VX_MAXD][VX_MAXW];              // tiles.md def of the ground terrain
    VxDef defs[VX_DEFS]; int def_count;
    VxPlace places[VX_PLACE]; int place_count;
    short place_at[VX_MAXD][VX_MAXW];
    VxTrig trigs[VX_TRIGS]; int trig_count;
    VxNpc npcs[VX_NPCS]; int npc_count;
    VxLight lights[VX_LIGHTS]; int light_count;
    int spawn_x, spawn_z, spawn_f;
    // The authored terrain height (the `## height` section). hset marks the cells the map spoke for;
    // everywhere else the procedural noise still runs, exactly as before. hmap is in WALK CELLS above
    // the map base; base_v is the base itself, in voxels (meta `base:`, default H_FLAT).
    unsigned char hmap[VX_MAXD][VX_MAXW], hset[VX_MAXD][VX_MAXW];
    int base_v, has_height;

    // palette
    unsigned char pal[256][3];
    float pal_lab[256][3];
    bool pal_ok;
    char ramp_name[24][40];
    unsigned char ramp_idx[24][80];
    int ramp_len[24], ramp_count;
    GLuint cmap, lut;
    int cmap_levels, cmap_tables, cmap_h;
    char cmap_tname[8][16];
    int cmap_row0[8], cmap_lamp_row0;
    unsigned char *cmap_px;
    int light_table;
    float amb;
    long off_pal;

    // world geometry
    VxChunk chunks[VX_CHUNKS];
    int tris, vert_total;
    double mesh_ms;
    bool built;

    // party
    VxActor act[VX_PARTY];
    int party, steps, art_party[VX_PARTY];
    bool moving; float move_t, step_len;
    int want_dir, last_axis, hold_t_dir; float hold_t;
    int step_parity;
    bool run;

    // ── the navmesh, generated from the voxel grid at load (VOXFIELD_NOTES.md "Movement") ──
    // One entry per NAV VOXEL, which is one voxel = half a walk cell, so 2x2 to the cell.
    unsigned char nav_ok[VX_VD][VX_VW];     // 1 = a body's centre may be here
    unsigned char nav_reg[VX_VD][VX_VW];    // 0 none, 1 walk-reachable from spawn, 2.. jump-only
    unsigned char nav_clr[VX_VD][VX_VW];    // clearance to the nearest blocked voxel, 1/32 cell, saturating
    float nav_h[VX_VD][VX_VW];              // the walking surface at the voxel's centre, world y
    int nav_regions, nav_jump_vox, nav_widened, nav_bad;
    VxJumpTgt nav_jump_tgt[VX_JUMPTGT]; int nav_jump_tgt_n;   // targets behind a jump, by design or not
    int nav_reg_size[16];
    double nav_ms;

    // the leader, as a point
    float pvx, pvz, pvy;                    // velocity, cells/second
    int airborne;
    float coyote, jump_buf, tko_x, tko_z, tko_y;
    float move_in_x, move_in_z, move_mag;   // this frame's desired direction and 0..1 magnitude
    int want_jump, jump_held;
    float speed_now;
    int invalid_logged, land_fails;
    // tunables (Dev sliders; they ride the reload blob)
    float agent_r, sp_walk, sp_run, jump_apex, gravity;
    int nav_view;

    // breadcrumbs: the leader's path, which the followers walk
    VxCrumb trail[VX_TRAIL]; int trail_n, trail_head; float trail_s;
    float follow_warp_t[VX_PARTY], follow_stuck[VX_PARTY];

    // the walktest bot (capture.sh --vox-walktest): drives the same movement code with no input
    int bot_on, bot_jump, bot_run, bot_tap, headless; float bot_mx, bot_mz;
    float cam_y;                            // the camera's own softened height, so a jump does not bob it
    bool cam_y_init;
    // the Nav view overlay (Dev): its own flat-colour program and a VBO rebuilt when the map changes
    GLuint nav_prog, nav_vao, nav_vbo; int nav_verts, nav_cap; float *nav_buf; bool nav_dirty;

    // art
    VxArt art[VX_ART]; int art_count;
    VxSprArt sprart[VX_SPRART]; int sprart_count;
    GLuint atlas; int atlas_w, atlas_h, atlas_cell;
    GLuint decal_tex[24]; int decal_w[24], decal_h[24]; char decal_id[24][24]; int decal_count;
    // scattered detail billboards, built once per map
    struct { float x, z, y; unsigned char d, size; } det[3072];
    int det_count;

    // gl
    bool gl_ready;
    GLuint prog, vao, spr_prog, spr_vao, spr_vbo, post_prog, blur_prog, quad_vao, quad_vbo;
    GLuint prog_cut;                  // the world shader WITH the cutaway's discard (see VX_FS_CUT)
    GLuint od_prog;                   // the overdraw probe: one constant step a depth-passing fragment
    GLuint fbo, fbo_tex, fbo_depth; int fbo_w, fbo_h;
    GLuint half_fbo[3], half_tex[3]; int half_w, half_h;
    GLuint out_fbo, out_tex;
    GLuint cap_fbo, cap_tex, cap_depth; int cap_w, cap_h;
    VxSpr *spr; int spr_count;

    // shadow map of the static world
    GLuint sky_prog, shadow_prog, shadow_fbo, shadow_tex;
    int shadow_dim;
    float lmvp[16], sundir[3];
    float sun_az, sun_el, sh_str, sh_soft;
    int sh_snap;
    bool shadow_dirty;
    double shadow_ms;
    // camera / look
    int ortho, hd2d, cutaway;
    float pitch, fov, view_h, tilt, ao_str, detail;
    float fog, dof, bloom, vignette, grade;
    int motes, res_pct;
    float cam_eye[3], cam_tgt[3], mvp[16], view[16];
    float player_screen[3];
    int chunks_drawn;

    // ui / flow
    char msg[512], msg_who[64];
    float msg_t; int msg_page; bool msg_typing, msg_more, tapped;
    // ── what the chapter script drives (voxfield.h, "what the chapter script drives") ──
    // The event QUEUE. vx_tick reports one event a tick and the chapter gates on every one of them,
    // so a tick that produced two (a pickup's text and the pickup itself) must not throw one away.
    VxEvent evq[VX_EVQ]; int evq_n;
    float ns_add;                     // night sight: added to the effective ambient, 0 = off
    int frozen;                       // vx_freeze: input ignored, the party stops walking
    // The ONE dynamic point light (the lantern that follows the leader). Not baked into the mesh —
    // it is a uniform in the world shader and a term in vx_light_at_foot, looked up in the `lamp`
    // colormap row exactly as the .tmap's static lamps are.
    int dlamp_on; float dlamp_r, dlamp_level, dlamp_x, dlamp_y, dlamp_z;
    int exam_trig, exam_npc;
    // MORE THAN ONE TRIGGER ON A CELL. A `.tmap` cell may carry a message AND a pickup AND a door
    // (hart_yard 6,7 is a message and a pickup: the bench, and the part on it). The examine used to
    // take the FIRST one the trigger list happened to hold and the others could never be reached at
    // all, which is why `hart_yard.bench_part` read its line forever and the part was never taken.
    // So an examine resolves the whole cell into this queue, in a defined order — every message,
    // then the pickup, then the door — and each box closing acts on the next. See trig_collect.
    int exam_q[VX_EXAM_Q], exam_qn, exam_cx, exam_cz;
    unsigned exam_mask;
    char to_map[32]; int to_x, to_z, to_f, fade, fade_dir;
    float anim_t, map_poll;
    int dbg_solid, dbg_trig, dbg_coord, noclip;
    bool stick_on, act_on; SDL_FingerID stick_id, act_id, jump_id;
    float stick_x, stick_y, stick_ox, stick_oy, act_t;

    // capture / selfcheck
    char cap_out[320]; int cap_req, cap_wi, cap_hi; bool cap_ok;
    bool selfchecked;
    double frame_ms_sum; int frame_n;
    int reach_missing, shaped, houses, ramps;

    // ── performance (VOXFIELD_NOTES.md "Performance") ──
    // perf_hud: 0 off, 1 compact, 2 full. It rides the reload blob so a hot reload keeps it up.
    int perf_hud;
    // The quality knobs below exist so the BENCHMARK can turn one thing off at a time. Every default
    // is the shipping look; nothing here is a setting the owner is meant to find.
    float q_pattern;      // 1 = the procedural block patterns, 0 = a flat mid step
    float q_pcf;          // shadow taps: 4 (default) or 1
    float q_water;        // 1 = water animates, 0 = frozen
    float q_cloud;        // sky cloud strength multiplier
    // The benchmark state machine. It only ever runs when the owner asks for it.
    int bench_on, bench_cfg, bench_frame;
    double bench_sum, bench_gpu[VXP_COUNT];
    double bench_hist[256]; int bench_hist_n;
    char bench_map[32];
    VxSave bench_saved; bool bench_have_saved;
    char bench_rows[64][320]; int bench_row_n;
    double bench_base_ms;
    float bench_poll;
    // Overdraw (the orchestrator's question: would a deferred pass pay for itself?). od_view turns
    // the Dev panel's false-colour debug view on; od_avg is the last measurement — the mean number of
    // times a pixel of the opaque world was written, i.e. how much shading a perfect depth prepass
    // would have saved. It is measured by a readback, so it happens in the capture and benchmark
    // paths and (at most once a second) in the debug view. Never in normal play.
    int od_view;
    double od_avg, od_cut_frac;
    int od_chunks_cut;
    // The order vx_render last drew the visible chunks in. The probe replays exactly that, so the
    // number it reports is the real pass's overdraw and not some other order's.
    VxChunk *od_order[VX_CHUNKS]; int od_nord;
};

extern int vx_max_tex;          // defined in voxfield.cpp
void vx_probe_limits();

// ── shared across a seam: the stamp-kind table's shape (vox_world.cpp owns the table itself) ──
struct NameBlock { const char *name; unsigned char blk; };
enum { OB_BILLBOARD = 0, OB_HOUSE, OB_TREE, OB_LOWWALL, OB_POST };
struct ObjKind { const char *name; int kind; unsigned char wall, roof; };

// ── the A/B benchmark's shape. vox_dev.cpp runs it; voxfield.cpp's vx_tick has to know the
//    frame counts and the configuration names to drive and label it. ──
#define VXB_WARM 30
#define VXB_MEAS 240
enum { VXB_BASE = 0, VXB_HD2D, VXB_DOF, VXB_BLOOM, VXB_GRADE, VXB_SHADOW, VXB_PCF1, VXB_PATTERN,
       VXB_AO, VXB_CUT, VXB_DETAIL, VXB_WATER, VXB_CLOUD, VXB_FOG,
       VXB_R100, VXB_R85, VXB_R75, VXB_R66, VXB_R50, VXB_ALLOFF, VXB_COUNT };
extern const char *VXB_NAME[VXB_COUNT];

// ───────────────────────── what crosses a module seam ─────────────────────────
// Each of these is defined in exactly one src/vox_*.cpp and called from at least one other.
int def_by_name(VoxField *v, const char *name);
void draw_msg_box(VoxField *v, int w, int h);
void draw_touch_ui(VoxField *v, int w, int h);
unsigned char get_blk(VoxField *v, int x, int y, int z);
unsigned char get_shp(VoxField *v, int x, int y, int z);
int npc_near(VoxField *v, float x, float z, float r);
void npc_step(VoxField *v, float dt);
const ObjKind *obj_kind(const char *name);
void on_enter_cell(VoxField *v, bool grounded);
void set_blk(VoxField *v, int x, int y, int z, unsigned char b);
unsigned char terr_block(const char *name);
int trig_at(VoxField *v, int x, int z, unsigned mask);
int trig_collect(VoxField *v, int x, int z, unsigned mask, int *out, int cap);
void trig_drain(VoxField *v);
bool trig_live(VoxField *v, const VxTrig *g);
int vx_art_get(VoxField *v, const char *id);
void vx_bench_frame(VoxField *v);
void vx_bind_sprites(VoxField *v);
void vx_btn_act(int w, int h, float *x, float *y, float *r);
void vx_btn_jump(int w, int h, float *x, float *y, float *r);
void vx_build_colormap(VoxField *v);
void vx_build_lut(VoxField *v);
void vx_build_nav(VoxField *v);
void vx_build_world(VoxField *v);
bool vx_can_stand(VoxField *v, int x, int z);
bool vx_cell_reachable(VoxField *v, int cx, int cz);
bool vx_cell_standable(VoxField *v, int cx, int cz);
void vx_collect_sprites(VoxField *v);
void vx_cpu_colour(VoxField *v, int idx, float light, float out[3]);
int vx_face_pick(int cur, float mx, float mz);
void vx_fbo_size(VoxField *v, int w, int h);
void vx_fire(VoxField *v, int kind, const char *arg, const char *arg2);
void vx_followers(VoxField *v, float dt);
void vx_gl_init(VoxField *v);
bool vx_ground_block(VoxField *v, int ix, int iz, float feet);
float vx_gy(VoxField *v, int x, int z);
void vx_input(VoxField *v, int w, int h, bool blocked, float dt);
void vx_lamp_at(VoxField *v, float x, float y, float z, float *lamp);
void vx_leader_move(VoxField *v, int w, int h, float dt);
void vx_load_atlas(VoxField *v);
void vx_load_decals(VoxField *v);
bool vx_load_palette(VoxField *v);
void vx_load_ramps(VoxField *v);
bool vx_load_tileset(VoxField *v, const char *set);
double vx_measure_overdraw(VoxField *v, int w, int h);
void vx_mesh_all(VoxField *v);
void vx_move_slide(VoxField *v, float *x, float *z, float dx, float dz, float feet, bool air, float r);
bool vx_nav_at(VoxField *v, float x, float z);
bool vx_nav_snap(VoxField *v, float *x, float *z, float limit);
float vx_nav_y(VoxField *v, float x, float z);
void vx_overdraw_pass(VoxField *v, int w, int h, float step);
void vx_parse_tmap(VoxField *v, char *text);
void vx_place_party(VoxField *v, int x, int z, int facing);
void vx_place_party_at(VoxField *v, float wx, float wz, int facing);
void vx_poll_map_flag(VoxField *v, float dt);
void vx_poll_perf_flag(VoxField *v, float dt);
GLuint vx_post(VoxField *v, int w, int h);
void vx_probe_limits();
int vx_ramp_by_name(VoxField *v, const char *want);
double vx_refresh_hz(void);
void vx_render(VoxField *v, int w, int h);
void vx_render_shadow(VoxField *v);
void vx_say(VoxField *v, const char *who, const char *id);
void vx_say2(VoxField *v, const char *who, const char *id, bool report);
void vx_scatter_detail(VoxField *v);
void vx_selfcheck(VoxField *v, int dw, int dh, int draws, int sprites);
void vx_sun_defaults(VoxField *v);
int vx_table_by_name(VoxField *v, const char *name);
GLuint vx_tex_indexed(VoxField *v, const char *label, const unsigned char *rgba, int w, int h);
double vx_tex_mb(VoxField *v, int rw, int rh);
void vx_trail_push(VoxField *v, float x, float z, float y, bool air);
int vx_verify_reach(VoxField *v);
float vx_vnoise(float x, float z, uint32_t seed);
void vx_walk(VoxField *v, int w, int h, float dt);

