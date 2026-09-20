// voxfield.cpp — the voxel + sprite field. src/VOXFIELD_NOTES.md says what this actually does.
// Part 1: platform glue, the master palette, the colormap, and the block table.
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
#define VX_VY    48                   // voxels up (24 walk cells)
#define VX_CHV   32              // chunk footprint, in voxels (= 16 walk cells, as before)
#define VX_CHX   (VX_VW / VX_CHV)
#define VX_CHZ   (VX_VD / VX_CHV)
#define VX_CHUNKS (VX_CHX * VX_CHZ)
#define VX_TRIGS 128
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

static const int DX[4] = { 0, -1, 1, 0 }, DZ[4] = { 1, 0, 0, -1 };
static const char *FACE_NAME[4] = { "S", "W", "E", "N" };
static int facing_of(const char *s) { return s[0] == 'W' ? 1 : s[0] == 'E' ? 2 : s[0] == 'N' ? 3 : 0; }

static const char *VX_MAPS[] = { "halm", "hart_yard", "west_road" };
int vx_map_count() { return (int)(sizeof(VX_MAPS) / sizeof(VX_MAPS[0])); }
const char *vx_map_name_at(int i) { return (i >= 0 && i < vx_map_count()) ? VX_MAPS[i] : "halm"; }

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

enum { TG_MESSAGE = 0, TG_EXIT, TG_DOOR, TG_ZONE, TG_TRAP, TG_SCENE };

struct VxTrig { short x, z, w, d; int kind; char arg[96], map[32]; short ax, az, af; bool inside; };
struct VxNpc {
    short hx, hz, tx, tz, px, pz;
    float t, wait, y, py_;
    int facing, wander, art, parity;
    char walker[24], text[48];
};
struct VxArt {                                  // a walker sheet, indexed on the master palette
    char id[24];
    GLuint tex;
    int w, h, fw, fh, nrows, ncols;
    int row_of[4]; bool flip[4];
    int col_stand, col_a, col_b;
};
struct VxActor { short tx, tz, px, pz; int facing; float y, y0; };
struct VxLight { float x, z, y, r, level; int flicker; };

// One tiles.md entry, only what the voxel builder needs: how big a stamp is and what it is.
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

    // art
    VxArt art[VX_ART]; int art_count;
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
    int exam_trig, exam_npc;
    char to_map[32]; int to_x, to_z, to_f, fade, fade_dir;
    float anim_t, map_poll;
    int dbg_solid, dbg_trig, dbg_coord, noclip;
    bool stick_on, act_on; SDL_FingerID stick_id, act_id;
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

static int vx_max_tex = 0;
static void vx_probe_limits() {
    if (!vx_max_tex) { GLint m = 0; glGetIntegerv(GL_MAX_TEXTURE_SIZE, &m); vx_max_tex = m > 0 ? m : 2048; }
}

// ───────────────────────── palette + ramps + colormap ─────────────────────────

static bool vx_load_palette(VoxField *v) {
    size_t sz = 0;
    char *text = (char *)vx_read("palette/master.hex", &sz);
    if (!text) { SDL_Log("voxfield: no story/palette/master.hex — falling back to built-in colours"); return false; }
    int n = 0;
    for (char *c = text; *c && n < 256; c++) {
        if (*c != '#') continue;
        unsigned x = 0;
        if (sscanf(c + 1, "%6x", &x) != 1) continue;
        v->pal[n][0] = (unsigned char)(x >> 16); v->pal[n][1] = (unsigned char)((x >> 8) & 255);
        v->pal[n][2] = (unsigned char)(x & 255);
        n++; c += 6;
    }
    SDL_free(text);
    if (n < 16) { SDL_Log("voxfield: master.hex has %d colours — ignoring", n); return false; }
    for (int i = n; i < 256; i++) { v->pal[i][0] = v->pal[i][1] = v->pal[i][2] = 0; }
    for (int i = 0; i < 256; i++)
        rgb_to_oklab(v->pal[i][0] / 255.0f, v->pal[i][1] / 255.0f, v->pal[i][2] / 255.0f,
                     &v->pal_lab[i][0], &v->pal_lab[i][1], &v->pal_lab[i][2]);
    v->pal_ok = true;
    SDL_Log("voxfield: palette master.hex — %d colours", n);
    return true;
}

// master.json's ramps, by name. A block's colours are steps along one of these; nothing in this file
// ever writes a hex value (PALETTE.md).
static void vx_load_ramps(VoxField *v) {
    size_t sz = 0;
    char *js = (char *)vx_read("palette/master.json", &sz);
    if (!js) { SDL_Log("voxfield: no master.json — block colours will be flat greys"); return; }
    for (const char *c = strstr(js, "\"ramps\""); c; ) {
        c = strstr(c, "\"name\"");
        if (!c || v->ramp_count >= 24) break;
        const char *q = strchr(c + 6, '"'); const char *e = q ? strchr(q + 1, '"') : nullptr;
        if (!q || !e) break;
        int len = (int)(e - q - 1); if (len > 39) len = 39;
        memcpy(v->ramp_name[v->ramp_count], q + 1, (size_t)len);
        v->ramp_name[v->ramp_count][len] = 0;
        const char *ix = strstr(e, "\"indices\"");
        if (!ix) break;
        const char *b = strchr(ix, '[');
        int k = 0;
        for (const char *p = b; p && *p && *p != ']' && k < 80; p++) {
            if ((*p >= '0' && *p <= '9') && (p == b || !(p[-1] >= '0' && p[-1] <= '9'))) {
                int val = atoi(p);
                if (val >= 0 && val < 256) v->ramp_idx[v->ramp_count][k++] = (unsigned char)val;
            }
        }
        v->ramp_len[v->ramp_count] = k;
        if (k) v->ramp_count++;
        c = strchr(ix, ']');
        if (!c) break;
    }
    SDL_free(js);
    SDL_Log("voxfield: %d palette ramps", v->ramp_count);
}

static int vx_ramp_by_name(VoxField *v, const char *want) {
    if (!want || !want[0]) return -1;
    for (int i = 0; i < v->ramp_count; i++) if (!strcmp(v->ramp_name[i], want)) return i;
    for (int i = 0; i < v->ramp_count; i++) if (strstr(v->ramp_name[i], want)) return i;
    return -1;
}

// The block colour LUT: 16 wide, B_COUNT tall, R8. Columns 0..5 are the top's six steps (dark to
// light), 6..11 the sides', 12 the pattern id, 13 the solid flag. One texelFetch in the shader.
static void vx_build_lut(VoxField *v) {
    unsigned char px[16 * B_COUNT];
    memset(px, 0, sizeof(px));
    for (int b = 0; b < B_COUNT; b++) {
        const VxBlockDef *d = &BLOCKS[b];
        int rt = vx_ramp_by_name(v, d->top_ramp), rs = vx_ramp_by_name(v, d->side_ramp);
        for (int s = 0; s < 6; s++) {
            float f = s / 5.0f;
            int ti = 1, si = 1;
            if (rt >= 0 && v->ramp_len[rt] > 1) {
                float g = vx_lerp(d->top_lo, d->top_hi, f) * (v->ramp_len[rt] - 1);
                ti = v->ramp_idx[rt][(int)(g + 0.5f)];
            }
            if (rs >= 0 && v->ramp_len[rs] > 1) {
                float g = vx_lerp(d->side_lo, d->side_hi, f) * (v->ramp_len[rs] - 1);
                si = v->ramp_idx[rs][(int)(g + 0.5f)];
            }
            px[b * 16 + s] = (unsigned char)ti;
            px[b * 16 + 6 + s] = (unsigned char)si;
        }
        px[b * 16 + 12] = (unsigned char)d->pattern;
        px[b * 16 + 13] = d->solid ? 1 : 0;
    }
    if (!v->lut) glGenTextures(1, &v->lut);
    glBindTexture(GL_TEXTURE_2D, v->lut);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_R8, 16, B_COUNT, 0, GL_RED, GL_UNSIGNED_BYTE, px);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
}

// story/palette/colormap.png as it stands: 256 wide, one row per (table, level). Row 0 of a table is
// full light. With no file, the rows are synthesised so night still works.
static void vx_build_colormap(VoxField *v) {
    static const char *NAMES[6] = { "day", "dusk", "night", "flash", "poison", "stone" };
    v->cmap_levels = VX_LEVELS; v->cmap_tables = 6; v->cmap_h = 256;
    for (int i = 0; i < 6; i++) { snprintf(v->cmap_tname[i], 16, "%s", NAMES[i]); v->cmap_row0[i] = i * 32; }
    v->cmap_lamp_row0 = 0;

    size_t isz = 0;
    void *data = vx_read("palette/colormap.png", &isz);
    unsigned char *file = nullptr;
    int fw = 0, fh = 0, ch = 0;
    if (data) { file = stbi_load_from_memory((const unsigned char *)data, (int)isz, &fw, &fh, &ch, 4); SDL_free(data); }
    if (file && fw != 256) { stbi_image_free(file); file = nullptr; }
    if (file) {
        size_t jsz = 0;
        char *js = (char *)vx_read("palette/colormap.json", &jsz);
        if (js) {
            const char *lv = strstr(js, "\"levels\"");
            int n = 0;
            if (lv && sscanf(lv + 8, " : %d", &n) == 1 && n >= 2 && n <= 64) v->cmap_levels = n;
            int k = 0;
            for (const char *c = strstr(js, "\"table\""); c && k < 8; c = strstr(c + 1, "\"table\"")) {
                const char *q = strchr(c + 7, '"'), *e = q ? strchr(q + 1, '"') : nullptr;
                if (!q || !e) break;
                int len = (int)(e - q - 1); if (len > 15) len = 15;
                char nm[16]; memcpy(nm, q + 1, (size_t)len); nm[len] = 0;
                const char *r0 = strstr(e, "\"row0\"");
                int row0 = 0;
                if (!r0 || sscanf(r0 + 6, " : %d", &row0) != 1) break;
                snprintf(v->cmap_tname[k], 16, "%s", nm);
                v->cmap_row0[k] = row0;
                if (!strcmp(nm, "lamp")) v->cmap_lamp_row0 = row0;
                k++;
            }
            if (k) v->cmap_tables = k;
            SDL_free(js);
        }
        v->cmap_h = fh;
        if (!v->cmap) glGenTextures(1, &v->cmap);
        glBindTexture(GL_TEXTURE_2D, v->cmap);
        glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 256, fh, 0, GL_RGBA, GL_UNSIGNED_BYTE, file);
        free(v->cmap_px);
        v->cmap_px = (unsigned char *)malloc((size_t)256 * fh * 4);
        if (v->cmap_px) memcpy(v->cmap_px, file, (size_t)256 * fh * 4);
        stbi_image_free(file);
        SDL_Log("voxfield: colormap.png 256x%d, %d tables, %d levels", fh, v->cmap_tables, v->cmap_levels);
    } else {
        // Synthesised: darken toward the table's cast. Good enough that a checkout with no colormap
        // still has a night; the file replaces it the moment it is there.
        static const float CAST[6][3] = { {1,1,1}, {1.05f,0.92f,0.80f}, {0.62f,0.72f,1.0f},
                                          {1.2f,1.2f,1.1f}, {0.8f,1.1f,0.8f}, {0.9f,0.9f,0.95f} };
        int h = 6 * VX_LEVELS;
        unsigned char *px = (unsigned char *)calloc((size_t)256 * h, 4);
        if (!px) return;
        for (int tb = 0; tb < 6; tb++) for (int lv = 0; lv < VX_LEVELS; lv++) {
            float k = 1.0f - (float)lv / (VX_LEVELS - 1) * 0.85f;
            unsigned char *row = px + ((size_t)(tb * VX_LEVELS + lv) * 256) * 4;
            for (int i = 1; i < 256; i++) {
                for (int c = 0; c < 3; c++) {
                    float x = v->pal[i][c] / 255.0f * k * CAST[tb][c];
                    row[i * 4 + c] = (unsigned char)(255.0f * (x < 0 ? 0 : x > 1 ? 1 : x) + 0.5f);
                }
                row[i * 4 + 3] = 255;
            }
        }
        v->cmap_h = h;
        if (!v->cmap) glGenTextures(1, &v->cmap);
        glBindTexture(GL_TEXTURE_2D, v->cmap);
        glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 256, h, 0, GL_RGBA, GL_UNSIGNED_BYTE, px);
        free(v->cmap_px); v->cmap_px = px;
        SDL_Log("voxfield: colormap synthesised on the CPU (no palette/colormap.png)");
    }
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
}

static int vx_table_by_name(VoxField *v, const char *name) {
    for (int i = 0; i < v->cmap_tables && i < 8; i++) if (!SDL_strcasecmp(v->cmap_tname[i], name)) return i;
    return 0;
}

// The colour a palette index takes under the current light — for the fog and the sky, which are
// solved on the CPU because they are one colour a frame, not one a pixel.
static void vx_cpu_colour(VoxField *v, int idx, float light, float out[3]) {
    out[0] = out[1] = out[2] = 0.3f;
    if (!v->cmap_px) return;
    float f = (1.0f - (light < 0 ? 0 : light > 1 ? 1 : light)) * (v->cmap_levels - 1);
    int row = v->cmap_row0[v->light_table < v->cmap_tables ? v->light_table : 0] + (int)(f + 0.5f);
    if (row < 0) row = 0;
    if (row >= v->cmap_h) row = v->cmap_h - 1;
    const unsigned char *p = v->cmap_px + ((size_t)row * 256 + (idx & 255)) * 4;
    for (int c = 0; c < 3; c++) out[c] = p[c] / 255.0f;
}

// ───────────────────────── indexing art on the palette ─────────────────────────

#define VXQ_SLOTS 65536
struct VxQCache { uint32_t key[VXQ_SLOTS]; unsigned char val[VXQ_SLOTS]; int colours; };

static unsigned char vx_quant(VoxField *v, VxQCache *q, uint32_t rgba) {
    if ((rgba >> 24) < 128) return 0;
    uint32_t k = rgba | 0xFF000000u;
    uint32_t s = ((k * 2654435761u) >> 12) & (VXQ_SLOTS - 1);
    int slot = -1;
    for (int probe = 0; probe < 64; probe++, s = (s + 1) & (VXQ_SLOTS - 1)) {
        if (q->key[s] == k) return q->val[s];
        if (!q->key[s]) { slot = (int)s; break; }
    }
    int r = (int)(k & 255), g = (int)((k >> 8) & 255), b = (int)((k >> 16) & 255);
    int best = 1;
    for (int i = 1; i < 256; i++)
        if (v->pal[i][0] == r && v->pal[i][1] == g && v->pal[i][2] == b) { best = i; goto done; }
    {
        float L, A, B2, bd = 1e9f;
        rgb_to_oklab(r / 255.0f, g / 255.0f, b / 255.0f, &L, &A, &B2);
        for (int i = 1; i < 256; i++) {
            float d = (L - v->pal_lab[i][0]) * (L - v->pal_lab[i][0])
                    + (A - v->pal_lab[i][1]) * (A - v->pal_lab[i][1])
                    + (B2 - v->pal_lab[i][2]) * (B2 - v->pal_lab[i][2]);
            if (d < bd) { bd = d; best = i; }
        }
        v->off_pal++;
    }
done:
    if (slot >= 0) { q->key[slot] = k; q->val[slot] = (unsigned char)best; q->colours++; }
    return (unsigned char)best;
}

// RGBA -> an R8 texture of palette indices, the same currency the world is drawn in, so a sprite and
// a block light through one colormap row and never disagree about a colour.
static GLuint vx_tex_indexed(VoxField *v, const char *label, const unsigned char *rgba, int w, int h) {
    GLuint tex = 0;
    glGenTextures(1, &tex);
    glBindTexture(GL_TEXTURE_2D, tex);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    if (!v->pal_ok) {
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, w, h, 0, GL_RGBA, GL_UNSIGNED_BYTE, rgba);
    } else {
        unsigned char *idx = (unsigned char *)malloc((size_t)w * h);
        VxQCache *q = (VxQCache *)calloc(1, sizeof(VxQCache));
        if (!idx || !q) { free(idx); free(q); return tex; }
        for (size_t i = 0; i < (size_t)w * h; i++) idx[i] = vx_quant(v, q, ((const uint32_t *)rgba)[i]);
        SDL_Log("voxfield: %s indexed — %dx%d, %d colours", label, w, h, q->colours);
        free(q);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_R8, w, h, 0, GL_RED, GL_UNSIGNED_BYTE, idx);
        free(idx);
    }
    // NEAREST both ways: the owner's pixels stay pixels (the HD-2D pass must not smooth texels).
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    return tex;
}

// ───────────────────────── tiles.md: only the shape of each entry ─────────────────────────
// The voxel world needs to know how big a stamp is and what it is called. Colours, atlas indices,
// masks, swatches and decal scatter rules all belong to the tile field; none of it is read here.

static int def_by_name(VoxField *v, const char *name) {
    for (int i = 0; i < v->def_count; i++) if (!strcmp(v->defs[i].name, name)) return i;
    return -1;
}

static void parse_solid_rows(VxDef *d, const char *val) {
    memset(d->solid, 0, sizeof(d->solid));
    if (!strcmp(val, "yes")) { for (int r = 0; r < d->h && r < 8; r++) d->solid[r] = (unsigned char)((1 << d->w) - 1); return; }
    if (!strcmp(val, "no")) return;
    int r = 0, c = 0;
    for (const char *p = val; *p && r < 8; p++) {
        if (*p == '/') { r++; c = 0; continue; }
        if (*p == '#' && c < 8) d->solid[r] |= (unsigned char)(1 << c);
        if (*p == '#' || *p == '.') c++;
    }
}

static bool vx_load_tileset(VoxField *v, const char *set) {
    if (!strcmp(v->set, set) && v->def_count) return true;
    snprintf(v->set, sizeof(v->set), "%s", set);
    v->def_count = 0;
    char rel[128];
    snprintf(rel, sizeof(rel), "field/tilesets/%s/tiles.md", set);
    size_t sz = 0;
    char *text = (char *)vx_read(rel, &sz);
    if (!text) { SDL_Log("voxfield: no %s — stamps will be 1x1", rel); return false; }
    VxDef *d = nullptr;
    char *save = nullptr;
    for (char *line = SDL_strtok_r(text, "\n", &save); line; line = SDL_strtok_r(nullptr, "\n", &save)) {
        char *s = vx_trim(line);
        if (s[0] == '#' && s[1] == '#' && s[2] != '#') {
            if (v->def_count >= VX_DEFS) { d = nullptr; continue; }
            d = &v->defs[v->def_count++];
            memset(d, 0, sizeof(*d));
            snprintf(d->name, sizeof(d->name), "%s", vx_trim(s + 2));
            d->w = d->h = 1; d->index = -1; d->kind = DK_TILE;
            continue;
        }
        if (!d || s[0] != '-') continue;
        char *val = strchr(s, ':');
        if (!val) continue;
        *val = 0;
        char *key = vx_trim(s + 1);
        val = vx_trim(val + 1);
        if (!strcmp(key, "index")) {
            int i = -1, w = 1, h = 1;
            int n = sscanf(val, "%d %dx%d", &i, &w, &h);
            d->index = (short)i;
            if (n >= 3) { d->w = (short)w; d->h = (short)h; }
        } else if (!strcmp(key, "kind")) {
            d->kind = !strcmp(val, "terrain") ? DK_TERRAIN : !strcmp(val, "decal") ? DK_DECAL : DK_TILE;
        } else if (!strcmp(key, "solid")) parse_solid_rows(d, val);
    }
    SDL_free(text);
    // `- solid: yes` may come before `- index: N WxH`, so re-expand it now the size is known.
    for (int i = 0; i < v->def_count; i++) {
        VxDef *e = &v->defs[i];
        bool any = false;
        for (int r = 0; r < 8; r++) if (e->solid[r]) any = true;
        if (any && e->solid[0] == 1 && e->w > 1) for (int r = 0; r < e->h && r < 8; r++) e->solid[r] = (unsigned char)((1 << e->w) - 1);
    }
    SDL_Log("voxfield: tileset %s — %d entries", set, v->def_count);
    return true;
}

// ───────────────────────── what a tile NAME becomes, in blocks ─────────────────────────

struct NameBlock { const char *name; unsigned char blk; };
static const NameBlock TERR_BLOCK[] = {
    { "grass", B_GRASS }, { "grass_dry", B_GRASS_DRY }, { "crop", B_CROP }, { "mud", B_MUD },
    { "dirt", B_DIRT }, { "gravel", B_GRAVEL }, { "paving", B_PAVING }, { "bridge_deck", B_PLANK },
    { "water", B_WATER }, { "sand", B_GRAVEL },
};
static unsigned char terr_block(const char *name) {
    for (size_t i = 0; i < sizeof(TERR_BLOCK) / sizeof(TERR_BLOCK[0]); i++)
        if (!strcmp(TERR_BLOCK[i].name, name)) return TERR_BLOCK[i].blk;
    return B_GRASS;
}

// What kind of thing a stamp is. Names come from valley's tiles.md; anything unrecognised falls
// through to a billboard, which is the safe answer — it never blocks a lane it should not.
enum { OB_BILLBOARD = 0, OB_HOUSE, OB_TREE, OB_LOWWALL, OB_POST };
struct ObjKind { const char *name; int kind; unsigned char wall, roof; };
static const ObjKind OBJ_KINDS[] = {
    { "house_a", OB_HOUSE, B_PLASTER, B_ROOF },   { "house_b", OB_HOUSE, B_TIMBER,  B_ROOF },
    { "house_c", OB_HOUSE, B_PLASTER, B_ROOF },   { "house_d", OB_HOUSE, B_PLASTER, B_ROOF },
    { "house_e", OB_HOUSE, B_PLASTER, B_ROOF },   { "hut",     OB_HOUSE, B_PLASTER, B_THATCH },
    { "shed",    OB_HOUSE, B_PLANK,   B_PLANK },  { "stable",  OB_HOUSE, B_PLANK,   B_THATCH },
    { "barn",    OB_HOUSE, B_PLANK,   B_ROOF },   { "grain_shed", OB_HOUSE, B_PLANK, B_THATCH },
    { "ladder_house", OB_HOUSE, B_PLASTER, B_ROOF }, { "guild_hall", OB_HOUSE, B_STONE, B_ROOF },
    { "tree", OB_TREE, 0, 0 }, { "tree_2", OB_TREE, 0, 0 }, { "tree_bare", OB_TREE, 0, 0 },
    { "orchard_tree", OB_TREE, 0, 0 },
    { "fence_ew", OB_LOWWALL, B_TIMBER, 0 }, { "fence_ns", OB_LOWWALL, B_TIMBER, 0 },
    { "fence_corner", OB_LOWWALL, B_TIMBER, 0 }, { "hedge", OB_LOWWALL, B_HEDGE, 0 },
    { "wall_ew", OB_LOWWALL, B_STONE, 0 }, { "wall_ns", OB_LOWWALL, B_STONE, 0 },
    { "wall_corner", OB_LOWWALL, B_STONE, 0 },
    { "practice_post", OB_POST, B_TIMBER, 0 }, { "gate_post", OB_POST, B_TIMBER, 0 },
    { "signpost", OB_POST, B_TIMBER, 0 }, { "stump", OB_POST, B_TRUNK, 0 },
};
static const ObjKind *obj_kind(const char *name) {
    for (size_t i = 0; i < sizeof(OBJ_KINDS) / sizeof(OBJ_KINDS[0]); i++)
        if (!strcmp(OBJ_KINDS[i].name, name)) return &OBJ_KINDS[i];
    return nullptr;
}

// ───────────────────────── the .tmap, read exactly as the tile field reads it ─────────────────────────

static int split_words(char *s, char **w, int max) {
    int n = 0;
    for (char *c = s; *c && n < max;) {
        while (*c == ' ' || *c == '\t') c++;
        if (!*c) break;
        w[n++] = c;
        while (*c && *c != ' ' && *c != '\t') c++;
        if (*c) *c++ = 0;
    }
    return n;
}

static void vx_parse_tmap(VoxField *v, char *text) {
    char legend[128][24];
    memset(legend, 0, sizeof(legend));
    int sec = -1, row = 0;
    char *save = nullptr;
    for (char *line = SDL_strtok_r(text, "\n", &save); line; line = SDL_strtok_r(nullptr, "\n", &save)) {
        char *e = line + strlen(line);
        while (e > line && (e[-1] == '\r' || e[-1] == ' ')) *--e = 0;
        if (line[0] == '#' && line[1] == '#') {
            char *s = vx_trim(line + 2);
            sec = !strcmp(s, "meta") ? 0 : !strcmp(s, "legend") ? 1 : !strcmp(s, "ground") ? 2
                : !strcmp(s, "objects") ? 3 : !strcmp(s, "triggers") ? 4 : -1;
            row = 0;
            continue;
        }
        if (sec < 0 || !line[0]) continue;
        if (sec == 0) {
            char *val = strchr(line, ':');
            if (!val) continue;
            *val = 0;
            char *key = vx_trim(line);
            val = vx_trim(val + 1);
            if (!strcmp(key, "name")) snprintf(v->map_name, sizeof(v->map_name), "%s", val);
            else if (!strcmp(key, "tileset")) vx_load_tileset(v, val);
            else if (!strcmp(key, "music")) snprintf(v->music, sizeof(v->music), "%s", val);
            else if (!strcmp(key, "size")) {
                int w = 0, h = 0;
                sscanf(val, "%d %d", &w, &h);
                v->mw = w < 1 ? 1 : w > VX_MAXW ? VX_MAXW : w;
                v->md = h < 1 ? 1 : h > VX_MAXD ? VX_MAXD : h;
            } else if (!strcmp(key, "light")) {
                char tb[16] = "day"; float lv = 1.0f;
                sscanf(val, "%15s %f", tb, &lv);
                v->light_table = vx_table_by_name(v, tb);
                v->amb = lv < 0 ? 0 : lv > 1 ? 1 : lv;
            } else if (!strcmp(key, "lamp")) {
                float x = 0, z = 0, r = 3, lv = 1; char fl[16] = "";
                if (sscanf(val, "%f %f %f %f %15s", &x, &z, &r, &lv, fl) >= 4 && v->light_count < VX_LIGHTS) {
                    VxLight *l = &v->lights[v->light_count++];
                    l->x = x + 0.5f; l->z = z + 0.5f; l->y = 2.2f;
                    l->r = r < 0.5f ? 0.5f : r;
                    l->level = lv < 0 ? 0 : lv > 1 ? 1 : lv;
                    l->flicker = !strcmp(fl, "flicker");
                }
            } else if (!strcmp(key, "spawn")) {
                char f[8] = "S";
                sscanf(val, "%d %d %7s", &v->spawn_x, &v->spawn_z, f);
                v->spawn_f = facing_of(f);
            }
        } else if (sec == 1) {
            char c = line[0];
            char *nm = vx_trim(line + 1);
            if (c && nm[0]) snprintf(legend[(unsigned char)c & 127], 24, "%s", nm);
        } else if (sec == 2 || sec == 3) {
            if (row >= v->md) { row++; continue; }
            for (int x = 0; x < v->mw; x++) {
                char c = line[x];
                if (!c) break;
                if (c == '+') continue;
                if (c == '.' && sec == 3) continue;
                const char *nm = legend[(unsigned char)c & 127];
                if (!nm[0]) continue;
                int def = def_by_name(v, nm);
                if (def < 0) continue;
                if (sec == 2) v->ground[row][x] = (short)def;
                else {
                    VxDef *d = &v->defs[def];
                    bool clash = false;
                    for (int r = 0; r < d->h; r++) for (int cc = 0; cc < d->w; cc++) {
                        int mx = x + cc, mz = row + r;
                        if (mx >= v->mw || mz >= v->md || v->place_at[mz][mx]) clash = true;
                    }
                    if (clash || v->place_count >= VX_PLACE) continue;
                    VxPlace *p = &v->places[v->place_count++];
                    p->def = (short)def; p->x = (short)x; p->z = (short)row;
                    for (int r = 0; r < d->h; r++) for (int cc = 0; cc < d->w; cc++) {
                        v->place_at[row + r][x + cc] = (short)v->place_count;
                        if (d->solid[r] & (1 << cc)) v->tsolid[row + r][x + cc] = 1;
                    }
                }
            }
            row++;
        } else if (sec == 4) {
            char work[256];
            snprintf(work, sizeof(work), "%s", line);
            char *w[12];
            int n = split_words(work, w, 12);
            if (n < 5) continue;
            int gx = atoi(w[0]), gz = atoi(w[1]), gw = atoi(w[2]) < 1 ? 1 : atoi(w[2]), gd = atoi(w[3]) < 1 ? 1 : atoi(w[3]);
            const char *k = w[4];
            if (!strcmp(k, "npc")) {
                if (v->npc_count >= VX_NPCS || n < 8) continue;
                VxNpc *np = &v->npcs[v->npc_count++];
                memset(np, 0, sizeof(*np));
                np->tx = np->px = np->hx = (short)gx; np->tz = np->pz = np->hz = (short)gz;
                snprintf(np->walker, sizeof(np->walker), "%s", w[5]);
                np->facing = facing_of(w[6]);
                snprintf(np->text, sizeof(np->text), "%s", w[7]);
                if (n >= 10 && !strcmp(w[8], "wander")) np->wander = atoi(w[9]);
                np->wait = 1.0f + vx_rnd(v->npc_count, 3, 7) * 2.0f;
                continue;
            }
            if (!strcmp(k, "light")) continue;
            if (v->trig_count >= VX_TRIGS) continue;
            VxTrig *g = &v->trigs[v->trig_count];
            memset(g, 0, sizeof(*g));
            g->x = (short)gx; g->z = (short)gz; g->w = (short)gw; g->d = (short)gd;
            if (!strcmp(k, "message")) { g->kind = TG_MESSAGE; snprintf(g->arg, sizeof(g->arg), "%s", n > 5 ? w[5] : ""); }
            else if (!strcmp(k, "zone")) { g->kind = TG_ZONE; snprintf(g->arg, sizeof(g->arg), "%s", n > 5 ? w[5] : ""); }
            else if (!strcmp(k, "trap")) { g->kind = TG_TRAP; snprintf(g->arg, sizeof(g->arg), "%s", n > 5 ? w[5] : ""); }
            else if (!strcmp(k, "scene")) { g->kind = TG_SCENE; snprintf(g->arg, sizeof(g->arg), "%s", n > 5 ? w[5] : ""); }
            else if (!strcmp(k, "exit") || !strcmp(k, "door")) {
                if (n < 9) continue;
                g->kind = !strcmp(k, "exit") ? TG_EXIT : TG_DOOR;
                snprintf(g->map, sizeof(g->map), "%s", w[5]);
                g->ax = (short)atoi(w[6]); g->az = (short)atoi(w[7]); g->af = (short)facing_of(w[8]);
            } else continue;
            v->trig_count++;
        }
    }
}

// ───────────────────────── building the voxel world from the .tmap ─────────────────────────
// Nothing new is authored: the same map the tile field walks becomes a column of blocks per cell,
// buildings are extruded from the stamp footprints, and the triggers, spawn, NPCs and lamps carry
// over untouched. The one thing invented here is a gentle terrain height, and it is FORCED FLAT
// wherever the tile map has something that has to stay walkable — then checked by flood fill.

static float vx_vnoise(float x, float z, uint32_t seed) {
    int xi = (int)floorf(x), zi = (int)floorf(z);
    float fx = x - xi, fz = z - zi;
    fx = fx * fx * (3 - 2 * fx); fz = fz * fz * (3 - 2 * fz);
    float a = vx_rnd(xi, zi, (int)seed), b = vx_rnd(xi + 1, zi, (int)seed);
    float c = vx_rnd(xi, zi + 1, (int)seed), d = vx_rnd(xi + 1, zi + 1, (int)seed);
    return vx_lerp(vx_lerp(a, b, fx), vx_lerp(c, d, fx), fz);
}

// Everything below here is in VOXELS: x,z run 0..vw/vd, y runs 0..VX_VY, and one voxel is VOX_S of a
// world unit. A walk cell (cx,cz) owns the 2x2 voxel columns at (cx*2, cz*2).
static void set_blk(VoxField *v, int x, int y, int z, unsigned char b) {
    if (x < 0 || z < 0 || y < 0 || x >= v->vw || z >= v->vd || y >= VX_VY) return;
    v->blk[y][z][x] = b;
    v->shp[y][z][x] = 0;
}
static void set_shaped(VoxField *v, int x, int y, int z, unsigned char b, unsigned char shape) {
    if (x < 0 || z < 0 || y < 0 || x >= v->vw || z >= v->vd || y >= VX_VY) return;
    v->blk[y][z][x] = b;
    v->shp[y][z][x] = shape;
}
static unsigned char get_blk(VoxField *v, int x, int y, int z) {
    if (x < 0 || z < 0 || y < 0 || x >= v->vw || z >= v->vd || y >= VX_VY) return B_AIR;
    return v->blk[y][z][x];
}
static unsigned char get_shp(VoxField *v, int x, int y, int z) {
    if (x < 0 || z < 0 || y < 0 || x >= v->vw || z >= v->vd || y >= VX_VY) return 0;
    return v->shp[y][z][x];
}
// Fill a whole walk cell's 2x2 column of voxels at voxel level y.
static void set_cell(VoxField *v, int cx, int y, int cz, unsigned char b) {
    for (int dz = 0; dz < VX_VPC; dz++) for (int dx = 0; dx < VX_VPC; dx++)
        set_blk(v, cx * VX_VPC + dx, y, cz * VX_VPC + dz, b);
}

static bool trig_covers(VoxField *v, int x, int z) {
    for (int i = 0; i < v->trig_count; i++) {
        VxTrig *g = &v->trigs[i];
        if (x >= g->x && z >= g->z && x < g->x + g->w && z < g->z + g->d) return true;
    }
    return false;
}

// The rectangle a building's front door should sit in: a door or message trigger on or beside the
// footprint. Returns the x of its centre, or -1.
static int door_x_for(VoxField *v, int x0, int z0, int w, int d) {
    for (int i = 0; i < v->trig_count; i++) {
        VxTrig *g = &v->trigs[i];
        if (g->kind != TG_DOOR && g->kind != TG_MESSAGE) continue;
        int cx = g->x + g->w / 2, cz = g->z + g->d / 2;
        if (cx >= x0 - 1 && cx <= x0 + w && cz >= z0 - 1 && cz <= z0 + d + 1) return cx < x0 ? x0 : cx > x0 + w - 1 ? x0 + w - 1 : cx;
    }
    return -1;
}

// ───────────────────────── the rule-based house generator ─────────────────────────
// No wave function collapse: a short list of rules, applied in a fixed order, seeded per building.
// Everything is in VOXELS (half a walk cell), which is what buys the framing, sills, recesses and
// eaves — at the old scale every one of those would have been a whole cell.
//
//   foundation course -> walls (+ timber framing for plaster) -> roof chosen by footprint
//   -> gable ends -> door recess under a lintel -> windows on a rhythm -> chimney -> extras
//
// The footprint is still filled solid as far as walking is concerned: walk[] is cleared by the
// caller for every cell of the stamp, exactly as before.

enum { HS_PLASTER = 0, HS_STONE, HS_TIMBER };

struct HouseIn {
    int cx0, cz0, cw, cd;        // footprint, in walk cells
    int base;                    // top solid ground voxel under it
    int style;
    unsigned char wall, roof;
    int door_side, door_cell;    // D_*, and the cell index along that side (-1 = centre)
    uint32_t seed;
    bool grand;
    int depth;                   // recursion guard for the L-wing
};

static int vx_houses_built = 0;

// Everything in the shell of a mass of `b` that has air above and air on exactly one horizontal side
// becomes a slope leaning that way. It is what turns a stepped blob into something rounded, and it is
// used for tree crowns and for thatch.
static void vx_round_shell(VoxField *v, int x0, int y0, int z0, int x1, int y1, int z1, unsigned char b) {
    for (int y = y0; y <= y1; y++) for (int z = z0; z <= z1; z++) for (int x = x0; x <= x1; x++) {
        if (get_blk(v, x, y, z) != b || get_shp(v, x, y, z) != 0) continue;
        if (get_blk(v, x, y + 1, z) != B_AIR) continue;
        int open = 0, d_open = -1;
        for (int d = 0; d < 4; d++)
            if (get_blk(v, x + DIRV[d][0], y, z + DIRV[d][1]) == B_AIR) { open++; d_open = d; }
        if (open == 1) set_shaped(v, x, y, z, b, SHP(SH_SLOPE, dir_opp(d_open)));
        else if (open == 2) {
            int a = -1, c = -1;
            for (int d = 0; d < 4; d++) if (get_blk(v, x + DIRV[d][0], y, z + DIRV[d][1]) == B_AIR) { if (a < 0) a = d; else c = d; }
            if (a >= 0 && c >= 0 && dir_opp(a) != c) {
                int hi = (dir_cw(dir_opp(a)) == dir_opp(c)) ? dir_opp(a) : dir_opp(c);
                set_shaped(v, x, y, z, b, SHP(SH_SLOPE_OUT, hi));
            }
        }
    }
}

static void vx_build_house(VoxField *v, const HouseIn *in);

// A pitched roof over a voxel rectangle, laid course by course from the eaves up. Slope voxels at
// voxel pitch (45 degrees), a slab ridge, and a half-voxel eave that oversails the wall by one voxel
// — always at least four voxels above the ground, so it can never block a walking cell.
// A roof is a HEIGHT MAP, not a shell. For every column of the plan the surface level is decided
// first — distance to the nearer eave for a gable, the lesser of the two distances for a hip — then
// the column is filled solid up to it and the top voxel is given the slope (or, where the two
// directions tie, the outer corner) that makes the pitch. A shell of 45-degree slopes with nothing
// under it reads as a hole at this camera; this cannot.
static void vx_roof(VoxField *v, int x0, int z0, int W, int D, int yr, unsigned char roof, bool hip) {
    bool ridge_x = (W >= D);
    int span = ridge_x ? D : W, len = ridge_x ? W : D;
    int half = span / 2;
    // the eave: one voxel of oversail all the way round, its top flush with the roof's outer edge
    for (int z = z0 - 1; z < z0 + D + 1; z++) for (int x = x0 - 1; x < x0 + W + 1; x++) {
        bool out = (x < x0 || x >= x0 + W || z < z0 || z >= z0 + D);
        if (out && get_blk(v, x, yr - 1, z) == B_AIR) set_shaped(v, x, yr - 1, z, roof, SHP(SH_SLAB, SL_TOP));
    }
    for (int o = 0; o < span; o++) for (int a = 0; a < len; a++) {
        int x = ridge_x ? x0 + a : x0 + o, z = ridge_x ? z0 + o : z0 + a;
        int d_off = o < span - 1 - o ? o : span - 1 - o;
        int d_alg = a < len - 1 - a ? a : len - 1 - a;
        int dir_off = ridge_x ? (o < span - 1 - o ? D_S : D_N) : (o < span - 1 - o ? D_E : D_W);
        int dir_alg = ridge_x ? (a < len - 1 - a ? D_E : D_W) : (a < len - 1 - a ? D_S : D_N);
        int rise = hip ? (d_off < d_alg ? d_off : d_alg) : d_off;
        unsigned char shape;
        if (hip && d_off == d_alg) {
            int o1 = (dir_cw(dir_off) == dir_alg) ? dir_off : dir_alg;
            shape = SHP(SH_SLOPE_OUT, o1);
        } else {
            shape = SHP(SH_SLOPE, (hip && d_alg < d_off) ? dir_alg : dir_off);
        }
        for (int y = yr; y < yr + rise; y++) set_blk(v, x, y, z, roof);
        set_shaped(v, x, yr + rise, z, roof, shape);
    }
    // the ridge: a slab cap over the two rows that meet at the top, so the pitches meet in a line
    int y = yr + half;
    for (int a = 0; a < len; a++) {
        if (hip) {
            int d_alg = a < len - 1 - a ? a : len - 1 - a;
            if (d_alg < half - 1) continue;
        }
        for (int t = 0; t < 2; t++) {
            int o = half - 1 + t;
            if (o >= span) continue;
            int x = ridge_x ? x0 + a : x0 + o, z = ridge_x ? z0 + o : z0 + a;
            set_shaped(v, x, y, z, roof, SHP(SH_SLAB, SL_BOT));
        }
    }
}

static void vx_build_house(VoxField *v, const HouseIn *in) {
    if (in->cw < 1 || in->cd < 1 || in->depth > 1) return;
    vx_houses_built++;
    uint32_t sd = in->seed;
    int x0 = in->cx0 * VX_VPC, z0 = in->cz0 * VX_VPC;
    int W = in->cw * VX_VPC, D = in->cd * VX_VPC;
    int y0 = in->base + 1;                                  // the first voxel above the ground
    unsigned char wall = in->wall, roof = in->roof;
    unsigned char frame = (in->style == HS_STONE) ? B_STONE : B_TIMBER;
    int wh = in->grand ? 8 : (in->cw >= 3 && in->cd >= 3 ? 7 : 6);   // wall height in voxels
    int yb = y0 + 1, yr = yb + wh;

    // 1. the foundation course: a plinth of stone one voxel proud of the ground under the whole thing
    for (int z = z0; z < z0 + D; z++) for (int x = x0; x < x0 + W; x++)
        set_blk(v, x, y0, z, in->style == HS_TIMBER ? B_STONE : B_STONE);
    // 2. the walls
    for (int z = z0; z < z0 + D; z++) for (int x = x0; x < x0 + W; x++) {
        bool edge = (x == x0 || x == x0 + W - 1 || z == z0 || z == z0 + D - 1);
        if (!edge) continue;
        for (int y = yb; y < yr; y++) set_blk(v, x, y, z, wall);
    }
    // 3. timber framing: corner posts, a post every three voxels, a mid rail and a top plate
    if (in->style != HS_STONE) {
        for (int z = z0; z < z0 + D; z++) for (int x = x0; x < x0 + W; x++) {
            bool edge = (x == x0 || x == x0 + W - 1 || z == z0 || z == z0 + D - 1);
            if (!edge) continue;
            bool corner = (x == x0 || x == x0 + W - 1) && (z == z0 || z == z0 + D - 1);
            int along = (z == z0 || z == z0 + D - 1) ? x - x0 : z - z0;
            bool post = corner || (along % 3 == 0);
            for (int y = yb; y < yr; y++) {
                if (post || y == yr - 1 || y == yb + wh / 2) set_blk(v, x, y, z, frame);
            }
        }
    }
    // 4. a jettied upper storey on a wide plan: the storey above the mid rail oversails by one voxel
    //    all the way round. It is built BEFORE the roof, because the roof has to cover it — a roof
    //    laid on the original footprint over jettied walls leaves an open tray, which is what the
    //    first build of this did.
    bool jetty = (in->cw >= 4 && in->cd >= 3 && in->style != HS_STONE && vx_rnd((int)sd, 5, 1) > 0.45f);
    if (jetty) {
        for (int z = z0 - 1; z < z0 + D + 1; z++) for (int x = x0 - 1; x < x0 + W + 1; x++) {
            if (x >= x0 && x < x0 + W && z >= z0 && z < z0 + D) continue;
            bool corner_out = (x < x0 || x >= x0 + W) && (z < z0 || z >= z0 + D);
            if (corner_out) continue;
            for (int y = yb + wh / 2 + 1; y < yr; y++) set_blk(v, x, y, z, wall);
            set_shaped(v, x, yb + wh / 2, z, frame, SHP(SH_SLOPE, D_S));
        }
    }
    int rx0 = jetty ? x0 - 1 : x0, rz0 = jetty ? z0 - 1 : z0;
    int RW = jetty ? W + 2 : W, RD = jetty ? D + 2 : D;
    // 5. the roof: gable along the long axis, hipped when the plan is nearly square
    bool hip = (in->cw * 4 >= in->cd * 3) && (in->cd * 4 >= in->cw * 3) && in->cw >= 2 && in->cd >= 2;
    vx_roof(v, rx0, rz0, RW, RD, yr, roof, hip);
    // 6. the gable end walls, filled up to the roofline
    if (!hip) {
        bool ridge_x = (RW >= RD);
        int span = ridge_x ? RD : RW;
        for (int s = 0; s < 2; s++) {
            int fixed = s ? (ridge_x ? RW - 1 : RD - 1) : 0;
            for (int o = 0; o < span; o++) {
                int rise = o < span - 1 - o ? o : span - 1 - o;
                int x = ridge_x ? rx0 + fixed : rx0 + o, z = ridge_x ? rz0 + o : rz0 + fixed;
                for (int y = yr; y <= yr + rise; y++) if (get_blk(v, x, y, z) == B_AIR) set_blk(v, x, y, z, wall);
            }
        }
    }
    // 7. the door: a one-voxel recess with the leaf set back, a lintel over it and a step outside
    int ds = in->door_side;
    int len = (ds == D_S || ds == D_N) ? W : D;
    int dc = in->door_cell < 0 ? len / 2 - 1 : in->door_cell * VX_VPC;
    if (dc < 1) dc = 1;
    if (dc > len - 3) dc = len - 3;
    if (dc < 0) dc = 0;
    {
        int inx = -DIRV[ds][0], inz = -DIRV[ds][1];
        for (int t = 0; t < 2 && dc + t < len; t++) {
            int x, z;
            if (ds == D_S)      { x = x0 + dc + t; z = z0 + D - 1; }
            else if (ds == D_N) { x = x0 + dc + t; z = z0; }
            else if (ds == D_E) { x = x0 + W - 1;  z = z0 + dc + t; }
            else                { x = x0;          z = z0 + dc + t; }
            for (int y = yb; y < yb + 4; y++) set_blk(v, x, y, z, B_AIR);          // the recess
            for (int y = yb; y < yb + 4; y++) set_blk(v, x + inx, y, z + inz, B_DOOR);
            set_blk(v, x, yb + 4, z, frame);                                        // the lintel
            set_shaped(v, x + inx, yb - 1, z + inz, B_STONE, SHP(SH_SLAB, SL_TOP)); // the threshold
        }
    }
    // 8. windows on a rhythm: a recessed pane, a timber surround and a slate sill that oversails
    for (int side = 0; side < 4; side++) {
        int slen = (side == D_S || side == D_N) ? W : D;
        int inx = -DIRV[side][0], inz = -DIRV[side][1];
        int rows = in->grand ? 2 : 1;
        for (int i = 1; i < slen - 1; i++) {
            if (i % 3 != 2) continue;
            for (int r = 0; r < rows; r++) {
                int wy = yb + 3 + r * 3;
                if (wy + 1 >= yr) continue;
                int x, z;
                if (side == D_S)      { x = x0 + i; z = z0 + D - 1; }
                else if (side == D_N) { x = x0 + i; z = z0; }
                else if (side == D_E) { x = x0 + W - 1;  z = z0 + i; }
                else                  { x = x0;          z = z0 + i; }
                if (side == ds && i >= dc - 1 && i <= dc + 2 && r == 0) continue;
                if (get_blk(v, x, wy, z) != wall && get_blk(v, x, wy, z) != frame) continue;
                for (int y = wy; y < wy + 2; y++) {
                    set_shaped(v, x, y, z, B_WINDOW, SHP(SH_PANE, side == D_S ? PN_N : side == D_N ? PN_S : side == D_E ? PN_W : PN_E));
                }
                set_blk(v, x, wy - 1, z, frame);
                set_blk(v, x, wy + 2, z, frame);
                set_shaped(v, x - inx, wy - 1, z - inz, B_SLATE, SHP(SH_SLAB, SL_TOP));   // the sill
                (void)inx; (void)inz;
            }
        }
    }
    // 9. a chimney on a seeded spot: one cell square, three voxels clear of the ridge
    {
        bool ridge_x = (W >= D);
        int a = 1 + (int)(vx_rnd((int)sd, 11, 3) * (float)((ridge_x ? W : D) - 3));
        int ridge_y = yr + (ridge_x ? D : W) / 2;
        int top = ridge_y + 2;
        int cxv = ridge_x ? x0 + a : x0 + W / 2 - 1, czv = ridge_x ? z0 + D / 2 - 1 : z0 + a;
        for (int t = 0; t < 2; t++) for (int u = 0; u < 2; u++)
            for (int y = yr - 1; y <= top; y++) set_blk(v, cxv + t, y, czv + u, B_STONE);
        for (int t = 0; t < 2; t++) for (int u = 0; u < 2; u++)
            set_shaped(v, cxv + t, top + 1, czv + u, B_SLATE, SHP(SH_SLAB, SL_BOT));
    }
    // 10. a porch on a grand building, hung on brackets.
    if (in->grand) {
        int px, pz, pw, pd;
        if (ds == D_S)      { px = x0 + dc - 1; pz = z0 + D;     pw = 4; pd = 2; }
        else if (ds == D_N) { px = x0 + dc - 1; pz = z0 - 2;     pw = 4; pd = 2; }
        else if (ds == D_E) { px = x0 + W;      pz = z0 + dc - 1; pw = 2; pd = 4; }
        else                { px = x0 - 2;      pz = z0 + dc - 1; pw = 2; pd = 4; }
        // brackets, not posts: a porch that reached the ground would close the lane to its own door
        for (int t = 0; t < 4; t++) {
            int x = px + (t & 1 ? pw - 1 : 0), z = pz + (t & 2 ? pd - 1 : 0);
            for (int y = yb + 5; y < yb + 7; y++) if (get_blk(v, x, y, z) == B_AIR) set_blk(v, x, y, z, frame);
        }
        vx_roof(v, px, pz, pw, pd, yb + 7, roof, false);
    }
}

// ───────────────────────── trees ─────────────────────────
// Three silhouettes, seeded. A crown may hang over a walking cell, but only above head height, and the
// cutaway takes it away when the party walks under it.
static void vx_build_tree(VoxField *v, int cx, int cz, int base, uint32_t sd, const char *name) {
    int x0 = cx * VX_VPC, z0 = cz * VX_VPC;
    int y0 = base + 1;
    int species = (int)(vx_rnd((int)sd, 3, 5) * 3.0f);
    if (species > 2) species = 2;
    if (!strcmp(name, "orchard_tree")) species = 2;
    if (!strcmp(name, "tree_2")) species = 1;
    // The crown may oversail a walking cell, but never below this: three voxels of clear headroom
    // plus the slab course, which is the same rule vx_can_stand enforces.
    const int CROWN_FLOOR = 4;
    int th = species == 1 ? 7 : species == 2 ? 5 : 5 + (int)(vx_rnd((int)sd, 7, 2) * 2.0f);
    for (int y = y0; y < y0 + th; y++) for (int t = 0; t < 2; t++) for (int u = 0; u < 2; u++)
        set_blk(v, x0 + t, y, z0 + u, B_TRUNK);
    int tx = x0 + 1, tz = z0 + 1;                              // the trunk's plan centre
    int ly = y0 + th;
    if (species == 1) {                                        // a tall stepped cone
        for (int L = 0; L < 6; L++) {
            float rad = 4.2f - L * 0.64f;
            int y = ly - 3 + L;
            if (y < y0 + CROWN_FLOOR) continue;
            for (int dz = -5; dz <= 5; dz++) for (int dx = -5; dx <= 5; dx++) {
                float d = sqrtf((dx + 0.5f) * (dx + 0.5f) + (dz + 0.5f) * (dz + 0.5f));
                if (d > rad) continue;
                if (d > rad - 0.9f && vx_rnd(x0 + dx, z0 + dz, y) > 0.58f) continue;
                if (get_blk(v, tx + dx, y, tz + dz) == B_AIR) set_blk(v, tx + dx, y, tz + dz, B_LEAVES);
            }
        }
    } else {                                                   // a rounded broadleaf crown
        float rx = species == 2 ? 3.0f : 4.6f, ry = species == 2 ? 2.4f : 3.6f;
        for (int dy = -3; dy <= 6; dy++) for (int dz = -6; dz <= 6; dz++) for (int dx = -6; dx <= 6; dx++) {
            int yy = ly + dy;
            if (yy < y0 + CROWN_FLOOR) continue;
            float u = (dx + 0.5f) / rx, w = (dz + 0.5f) / rx, q = (dy - ry * 0.35f) / ry;
            float r2 = u * u + w * w + q * q;
            if (r2 > 1.0f) continue;
            if (r2 > 0.74f && vx_rnd(x0 + dx, z0 + dz + 31, yy) > 0.66f) continue;
            if (get_blk(v, tx + dx, yy, tz + dz) == B_AIR) set_blk(v, tx + dx, yy, tz + dz, B_LEAVES);
        }
    }
    vx_round_shell(v, x0 - 7, y0 + CROWN_FLOOR, z0 - 7, x0 + 8, ly + 8, z0 + 8, B_LEAVES);
}

// ───────────────────────── the world ─────────────────────────

static void vx_build_world(VoxField *v) {
    memset(v->blk, 0, sizeof(v->blk));
    memset(v->shp, 0, sizeof(v->shp));
    memset(v->ramp, 0, sizeof(v->ramp));
    v->vw = v->mw * VX_VPC; v->vd = v->md * VX_VPC;
    vx_houses_built = 0;

    // 1. the height field, in VOXELS — half steps now, so the roll is gentler and reads as ground
    //    rather than as terracing. Forced flat everywhere the tile map needs a guaranteed lane.
    unsigned char flat[VX_MAXD][VX_MAXW];
    memset(flat, 0, sizeof(flat));
    for (int z = 0; z < v->md; z++) for (int x = 0; x < v->mw; x++) {
        int def = v->ground[z][x];
        const char *nm = (def >= 0 && def < v->def_count) ? v->defs[def].name : "grass";
        unsigned char b = terr_block(nm);
        bool rolls = (b == B_GRASS || b == B_GRASS_DRY || b == B_CROP);
        if (!rolls || v->place_at[z][x] || v->tsolid[z][x] || trig_covers(v, x, z)) flat[z][x] = 1;
    }
    for (int z = 0; z < v->md; z++) for (int x = 0; x < v->mw; x++) {          // one cell of margin
        if (!flat[z][x]) continue;
        for (int dz = -1; dz <= 1; dz++) for (int dx = -1; dx <= 1; dx++) {
            int nx = x + dx, nz = z + dz;
            if (nx >= 0 && nz >= 0 && nx < v->mw && nz < v->md && !flat[nz][nx]) flat[nz][nx] = 2;
        }
    }
    const int H_FLAT = 4;                       // solid voxels under a flat cell
    static unsigned char hh[VX_MAXD][VX_MAXW];
    for (int z = 0; z < v->md; z++) for (int x = 0; x < v->mw; x++) {
        if (flat[z][x]) { hh[z][x] = H_FLAT; continue; }
        float n = vx_vnoise(x / 9.0f, z / 9.0f, 7717u) * 0.72f + vx_vnoise(x / 4.0f, z / 4.0f, 313u) * 0.28f;
        int h = H_FLAT + (int)(n * 5.2f);
        hh[z][x] = (unsigned char)(h < H_FLAT ? H_FLAT : h > H_FLAT + 4 ? H_FLAT + 4 : h);
    }
    // No neighbour may differ by more than two voxels — one voxel is a free smooth step, two is a hop.
    for (int pass = 0; pass < 10; pass++) {
        bool changed = false;
        for (int z = 0; z < v->md; z++) for (int x = 0; x < v->mw; x++) {
            int h = hh[z][x];
            for (int d = 0; d < 4; d++) {
                int nx = x + DX[d], nz = z + DZ[d];
                if (nx < 0 || nz < 0 || nx >= v->mw || nz >= v->md) continue;
                if (h > hh[nz][nx] + 2) { h = hh[nz][nx] + 2; changed = true; }
            }
            hh[z][x] = (unsigned char)h;
        }
        if (!changed) break;
    }

    // 2. the ground columns. The TOP voxel's material is chosen per voxel, not per cell, and may be
    //    borrowed from a neighbouring cell on a hash — which is what makes grass, dirt and paving meet
    //    on a ragged line instead of on the cell edge. Height stays per cell, so walking is unaffected.
    static unsigned char topb[VX_MAXD][VX_MAXW];
    static unsigned char iswater[VX_MAXD][VX_MAXW];
    for (int z = 0; z < v->md; z++) for (int x = 0; x < v->mw; x++) {
        int def = v->ground[z][x];
        const char *nm = (def >= 0 && def < v->def_count) ? v->defs[def].name : "grass";
        topb[z][x] = terr_block(nm);
        iswater[z][x] = topb[z][x] == B_WATER;
    }
    for (int cz = 0; cz < v->md; cz++) for (int cx = 0; cx < v->mw; cx++) {
        int H = hh[cz][cx];
        if (iswater[cz][cx]) {
            // A channel with a real bed: the bed is three voxels down, the surface 1.2 voxels below
            // the land, and the banks are wet stone.
            for (int y = 0; y <= H - 4; y++) set_cell(v, cx, y, cz, y == H - 4 ? B_WATERBED : B_STONE);
            set_cell(v, cx, H - 3, cz, B_WATER);
            set_cell(v, cx, H - 2, cz, B_WATER);
            v->walk[cz][cx] = 0;
            v->hgt[cz][cx] = (unsigned char)(H - 4 < 0 ? 0 : H - 4);
            continue;
        }
        bool near_water = false;
        for (int dz = -1; dz <= 1 && !near_water; dz++) for (int dx = -1; dx <= 1; dx++) {
            int nx = cx + dx, nz = cz + dz;
            if (nx >= 0 && nz >= 0 && nx < v->mw && nz < v->md && iswater[nz][nx]) { near_water = true; break; }
        }
        for (int dz = 0; dz < VX_VPC; dz++) for (int dx = 0; dx < VX_VPC; dx++) {
            int vx = cx * VX_VPC + dx, vz = cz * VX_VPC + dz;
            unsigned char top = topb[cz][cx];
            // ragged boundary: this voxel may take the neighbouring cell's ground instead
            int nxc = dx ? cx + 1 : cx - 1, nzc = dz ? cz + 1 : cz - 1;
            float r = vx_rnd(vx, vz, 4177);
            if (r < 0.24f && nxc >= 0 && nxc < v->mw && !iswater[cz][nxc] && hh[cz][nxc] == H) top = topb[cz][nxc];
            else if (r < 0.46f && nzc >= 0 && nzc < v->md && !iswater[nzc][cx] && hh[nzc][cx] == H) top = topb[nzc][cx];
            for (int y = 0; y < H; y++) {
                unsigned char b = (y == H - 1) ? top
                                : (near_water && y >= H - 3) ? B_WETSTONE
                                : (y >= H - 3) ? B_DIRT : B_STONE;
                set_blk(v, vx, y, vz, b);
            }
        }
        v->walk[cz][cx] = v->tsolid[cz][cx] ? 0 : 1;
        v->hgt[cz][cx] = (unsigned char)(H - 1);
    }
    // an irregular shoreline: wet stone shoulders pushed out into the water from the bank
    for (int cz = 0; cz < v->md; cz++) for (int cx = 0; cx < v->mw; cx++) {
        if (!iswater[cz][cx]) continue;
        int H = hh[cz][cx];
        for (int dz = 0; dz < VX_VPC; dz++) for (int dx = 0; dx < VX_VPC; dx++) {
            int vx = cx * VX_VPC + dx, vz = cz * VX_VPC + dz;
            bool touches = false;
            for (int d = 0; d < 4; d++) {
                int nx = (vx + DIRV[d][0]) / VX_VPC, nz = (vz + DIRV[d][1]) / VX_VPC;
                if (nx >= 0 && nz >= 0 && nx < v->mw && nz < v->md && !iswater[nz][nx]) touches = true;
            }
            if (!touches) continue;
            if (vx_rnd(vx, vz, 913) < 0.45f) {
                set_blk(v, vx, H - 3, vz, B_WETSTONE);
                if (vx_rnd(vx, vz, 914) < 0.4f) set_shaped(v, vx, H - 2, vz, B_WETSTONE, SHP(SH_SLAB, SL_BOT));
            }
        }
    }

    // 3. the objects
    for (int i = 0; i < v->place_count; i++) {
        VxPlace *p = &v->places[i];
        VxDef *d = &v->defs[p->def];
        const ObjKind *k = obj_kind(d->name);
        if (!k) continue;
        int x0 = p->x, z0 = p->z, W = d->w, D = d->h;
        int base = 0;
        for (int z = z0; z < z0 + D; z++) for (int x = x0; x < x0 + W; x++)
            if (x < v->mw && z < v->md && (int)v->hgt[z][x] > base) base = v->hgt[z][x];
        if (k->kind == OB_HOUSE) {
            HouseIn in;
            memset(&in, 0, sizeof(in));
            in.cx0 = x0; in.cz0 = z0; in.cw = W; in.cd = D; in.base = base;
            in.wall = k->wall; in.roof = k->roof;
            in.style = k->wall == B_STONE ? HS_STONE : k->wall == B_PLASTER ? HS_PLASTER : HS_TIMBER;
            in.seed = vx_h32(x0, z0, 77);
            in.grand = (!strcmp(d->name, "guild_hall") || !strcmp(d->name, "barn") || (W >= 4 && D >= 4));
            int dxd = door_x_for(v, x0, z0, W, D);
            in.door_side = D_S;
            in.door_cell = dxd < 0 ? -1 : dxd - x0;
            vx_build_house(v, &in);
            // an L-wing on a big plan: a lower mass along one side, gabled the other way
            if (W >= 5 && D >= 4 && vx_rnd((int)in.seed, 2, 9) > 0.5f) {
                HouseIn wing = in;
                wing.cw = W / 2; wing.cd = 2; wing.cz0 = z0 + D;
                wing.grand = false; wing.depth = 1; wing.door_cell = -1;
                bool room = wing.cz0 + wing.cd <= v->md;
                for (int z = wing.cz0; z < wing.cz0 + wing.cd && room; z++)
                    for (int x = wing.cx0; x < wing.cx0 + wing.cw; x++)
                        if (x >= v->mw || !v->tsolid[z][x]) room = false;
                if (room) vx_build_house(v, &wing);
            }
            for (int z = z0; z < z0 + D; z++) for (int x = x0; x < x0 + W; x++)
                if (x < v->mw && z < v->md) v->walk[z][x] = 0;
        } else if (k->kind == OB_TREE) {
            int cx = x0 + W / 2, cz = z0 + D - 1;
            vx_build_tree(v, cx, cz, base, vx_h32(cx, cz, 5), d->name);
            for (int z = z0; z < z0 + D; z++) for (int x = x0; x < x0 + W; x++)
                if (x < v->mw && z < v->md && (d->solid[z - z0] & (1 << (x - x0)))) v->walk[z][x] = 0;
        } else {
            // fences, walls, hedges and posts, now with real uprights and rails at voxel thickness
            bool fence = (k->wall == B_TIMBER && k->kind == OB_LOWWALL);
            for (int z = z0; z < z0 + D; z++) for (int x = x0; x < x0 + W; x++) {
                if (x >= v->mw || z >= v->md) continue;
                int b2 = v->hgt[z][x] + 1;
                if (k->kind == OB_POST) {
                    for (int y = b2; y < b2 + 5; y++) set_shaped(v, x * VX_VPC, y, z * VX_VPC, k->wall, SHP(SH_POST, 0));
                } else if (fence) {
                    for (int y = b2; y < b2 + 3; y++)
                        set_shaped(v, x * VX_VPC, y, z * VX_VPC, k->wall, SHP(SH_POST, 0));
                } else {
                    for (int y = b2; y < b2 + 3; y++) set_cell(v, x, y, z, k->wall);
                    if (k->wall == B_HEDGE) vx_round_shell(v, x * VX_VPC, b2, z * VX_VPC, x * VX_VPC + 1, b2 + 2, z * VX_VPC + 1, B_HEDGE);
                }
                v->walk[z][x] = 0;
            }
        }
    }
    // Anything the tile map calls solid stays solid, billboard or not.
    for (int z = 0; z < v->md; z++) for (int x = 0; x < v->mw; x++) if (v->tsolid[z][x]) v->walk[z][x] = 0;
    for (int i = 0; i < v->npc_count; i++) {
        VxNpc *np = &v->npcs[i];
        if (np->tx >= 0 && np->tz >= 0 && np->tx < v->mw && np->tz < v->md) v->walk[np->tz][np->tx] = 0;
    }

    // 4. ramps: where a walkable cell steps two voxels up to exactly one neighbour and is level with
    //    the one opposite, its surface becomes a 45-degree slope instead of a lip to hop over.
    for (int z = 0; z < v->md; z++) for (int x = 0; x < v->mw; x++) {
        if (!v->walk[z][x] || v->place_at[z][x]) continue;
        int h = v->hgt[z][x], up = -1, ups = 0;
        for (int d = 0; d < 4; d++) {
            int nx = x + DX[d], nz = z + DZ[d];
            if (nx < 0 || nz < 0 || nx >= v->mw || nz >= v->md || !v->walk[nz][nx]) continue;
            if ((int)v->hgt[nz][nx] == h + 2) { up = d; ups++; }
        }
        if (ups != 1) continue;
        int bx = -DX[up], bz = -DZ[up];
        int lx = x + bx, lz = z + bz;
        if (lx < 0 || lz < 0 || lx >= v->mw || lz >= v->md || !v->walk[lz][lx] || (int)v->hgt[lz][lx] != h) continue;
        unsigned char mat = get_blk(v, x * VX_VPC, h, z * VX_VPC);
        if (mat == B_AIR) continue;
        // the ramp climbs across the cell: the near voxel row one voxel up, the far row two
        int d_up = (DX[up] > 0) ? D_E : (DX[up] < 0) ? D_W : (DZ[up] > 0) ? D_S : D_N;
        for (int dz = 0; dz < VX_VPC; dz++) for (int dx = 0; dx < VX_VPC; dx++) {
            int vx = x * VX_VPC + dx, vz = z * VX_VPC + dz;
            int along = (DX[up] ? dx : dz);
            if (DX[up] < 0 || DZ[up] < 0) along = 1 - along;
            set_blk(v, vx, h + 1, vz, along ? mat : B_AIR);
            set_shaped(v, vx, h + 1 + along, vz, mat, SHP(SH_SLOPE, d_up));
        }
        v->ramp[z][x] = (unsigned char)(1 + d_up);
    }
    v->shaped = 0;
    for (int y = 0; y < VX_VY; y++) for (int z = 0; z < v->vd; z++) for (int x = 0; x < v->vw; x++)
        if (v->shp[y][z][x]) v->shaped++;
    v->houses = vx_houses_built;
    v->ramps = 0;
    for (int z = 0; z < v->md; z++) for (int x = 0; x < v->mw; x++) if (v->ramp[z][x]) v->ramps++;
}

static void vx_sun_defaults(VoxField *v);

// ───────────────────────── walkability, and the check that it survived the height field ─────────
// A step is legal when the target cell is walkable, its top is within one block, and there are two
// blocks of headroom. Every route the .tmap allows must still exist; the flood fill says so at load.

static bool vx_can_stand(VoxField *v, int x, int z) {
    if (x < 0 || z < 0 || x >= v->mw || z >= v->md) return false;
    if (!v->walk[z][x]) return false;
    int y = v->hgt[z][x];                             // the voxel the feet stand on
    // Headroom. The party is 1.6 walk cells, so three voxels (1.5 cells) must be wholly clear and
    // the fourth may hold only a top slab — which is what lets a sill, an eave or a rail oversail a
    // lane without closing it. A ramp's own slope voxels are what you walk on and never count.
    for (int dz = 0; dz < VX_VPC; dz++) for (int dx = 0; dx < VX_VPC; dx++)
        for (int k = 1; k <= 4; k++) {
            int vx = x * VX_VPC + dx, vy = y + k, vz = z * VX_VPC + dz;
            if (get_blk(v, vx, vy, vz) == B_AIR) continue;
            unsigned char sp = get_shp(v, vx, vy, vz);
            if (v->ramp[z][x] && k <= 2 && shape_is_ramp(sp)) continue;
            if (k == 4 && SH_OF(sp) == SH_SLAB && OR_OF(sp) == SL_TOP) continue;
            return false;
        }
    return true;
}
// One voxel of rise is a free, smooth step; two is a hop, and only a ramp cell makes a two-voxel
// climb smooth. Anything more is a wall.
static bool vx_can_step(VoxField *v, int fx, int fz, int tx, int tz) {
    if (!vx_can_stand(v, tx, tz)) return false;
    int a = v->hgt[fz][fx], b = v->hgt[tz][tx];
    int dd = a > b ? a - b : b - a;
    if (dd <= 1) return true;
    if (dd == 2) return true;                          // a hop, as one old block used to be
    // a ramp cell reaches two voxels above its own foot, so it can meet the shelf it climbs to
    if (v->ramp[fz][fx] && b == a + 2) return true;
    if (v->ramp[tz][tx] && a == b + 2) return true;
    return false;
}

// The walking surface at a fractional position inside a cell, in VOXELS. A ramp is crossed smoothly:
// the height is the slope's own plane at that fraction, so there is no hop at either end.
static float vx_surface_v(VoxField *v, int cx, int cz, float fx, float fz) {
    if (cx < 0 || cz < 0 || cx >= v->mw || cz >= v->md) return 0;
    float y = (float)v->hgt[cz][cx] + 1.0f;
    int r = v->ramp[cz][cx];
    if (!r) return y;
    int d = r - 1;
    float t = DIRV[d][0] ? (DIRV[d][0] > 0 ? fx : 1.0f - fx) : (DIRV[d][1] > 0 ? fz : 1.0f - fz);
    return y + t * 2.0f;
}

static int npc_home_at(VoxField *v, int x, int z) {
    for (int i = 0; i < v->npc_count; i++) if (v->npcs[i].tx == x && v->npcs[i].tz == z) return i;
    return -1;
}

static void flood(VoxField *v, unsigned char *out, int sx, int sz, bool vox) {
    static short qx[VX_MAXW * VX_MAXD], qz[VX_MAXW * VX_MAXD];
    int head = 0, tail = 0;
    memset(out, 0, (size_t)VX_MAXW * VX_MAXD);
    if (sx < 0 || sz < 0 || sx >= v->mw || sz >= v->md) return;
    out[sz * VX_MAXW + sx] = 1;
    qx[tail] = (short)sx; qz[tail++] = (short)sz;
    while (head < tail) {
        int x = qx[head], z = qz[head]; head++;
        for (int d = 0; d < 4; d++) {
            int nx = x + DX[d], nz = z + DZ[d];
            if (nx < 0 || nz < 0 || nx >= v->mw || nz >= v->md) continue;
            if (out[nz * VX_MAXW + nx]) continue;
            bool ok;
            if (vox) ok = vx_can_step(v, x, z, nx, nz);
            else {
                int def = v->ground[nz][nx];
                const char *nm = (def >= 0 && def < v->def_count) ? v->defs[def].name : "grass";
                ok = !v->tsolid[nz][nx] && terr_block(nm) != B_WATER && npc_home_at(v, nx, nz) < 0;
            }
            if (!ok) continue;
            out[nz * VX_MAXW + nx] = 1;
            qx[tail] = (short)nx; qz[tail++] = (short)nz;
        }
    }
}

// Flatten until the voxel world is at least as connected as the tile map. In practice the flatten
// set above makes this pass first time; it exists so an edit to the noise can never strand a door.
static int vx_verify_reach(VoxField *v) {
    static unsigned char tile_r[VX_MAXW * VX_MAXD], vox_r[VX_MAXW * VX_MAXD];
    int missing = 0;
    for (int round = 0; round < 6; round++) {
        flood(v, tile_r, v->spawn_x, v->spawn_z, false);
        flood(v, vox_r, v->spawn_x, v->spawn_z, true);
        missing = 0;
        for (int z = 0; z < v->md; z++) for (int x = 0; x < v->mw; x++)
            if (tile_r[z * VX_MAXW + x] && !vox_r[z * VX_MAXW + x]) missing++;
        if (!missing) return 0;
        // Flatten every unreachable cell and its neighbours back to the flat level and rebuild them.
        for (int z = 0; z < v->md; z++) for (int x = 0; x < v->mw; x++) {
            if (!tile_r[z * VX_MAXW + x] || vox_r[z * VX_MAXW + x]) continue;
            for (int dz = -1; dz <= 1; dz++) for (int dx = -1; dx <= 1; dx++) {
                int nx = x + dx, nz = z + dz;
                if (nx < 0 || nz < 0 || nx >= v->mw || nz >= v->md) continue;
                if (v->place_at[nz][nx]) continue;
                int def = v->ground[nz][nx];
                const char *nm = (def >= 0 && def < v->def_count) ? v->defs[def].name : "grass";
                unsigned char top = terr_block(nm);
                if (top == B_WATER) continue;
                v->ramp[nz][nx] = 0;
                const int H = 4;
                for (int dzv = 0; dzv < VX_VPC; dzv++) for (int dxv = 0; dxv < VX_VPC; dxv++) {
                    int vx = nx * VX_VPC + dxv, vz = nz * VX_VPC + dzv;
                    for (int y = 0; y < VX_VY; y++)
                        set_blk(v, vx, y, vz, y < H - 1 ? (y >= H - 3 ? B_DIRT : B_STONE) : y == H - 1 ? top : B_AIR);
                }
                v->hgt[nz][nx] = (unsigned char)(H - 1);
            }
        }
        SDL_Log("voxfield: %d cells unreachable after the height field — flattened, retrying", missing);
    }
    return missing;
}

// The world y a body's feet rest at, on a cell's flat surface.
static float vx_gy(VoxField *v, int x, int z) {
    if (x < 0 || z < 0 || x >= v->mw || z >= v->md) return 0;
    return ((float)v->hgt[z][x] + 1.0f) * VOX_S;
}

// ───────────────────────── meshing ─────────────────────────
// Once, at load, into one static VBO per 16x16 chunk. Hidden faces are culled; every vertex carries
// its baked ambient occlusion, the sun term for its face, whether a column shadows it, and the lamp
// light at that corner. Nothing about the light is computed per frame (CLAUDE.md: no realtime
// lighting) — the ambient level and the AO strength stay live because they are applied in the shader
// to numbers that were baked.

enum { F_TOP = 0, F_BOT, F_SOUTH, F_NORTH, F_EAST, F_WEST };
static const int FN[6][3] = { {0,1,0}, {0,-1,0}, {0,0,1}, {0,0,-1}, {1,0,0}, {-1,0,0} };
static const int F_OPP[6] = { F_BOT, F_TOP, F_NORTH, F_SOUTH, F_WEST, F_EAST };

// The face term, from the REAL normal, so a slope is lit between the two faces it lies between
// rather than snapping to one of them. The six axis normals reproduce the old FACE_L table exactly:
// top 1.00, bottom 0.42, south 0.90, north 0.56, east 0.78, west 0.66.
static float vx_face_light(float nx, float ny, float nz) {
    float horiz = 0.725f + 0.17f * nz + 0.06f * nx;
    float vert = ny > 0 ? 1.00f : 0.42f;
    float k = ny < 0 ? -ny : ny;
    return horiz + (vert - horiz) * k;
}

static VxVert *vx_tmp = nullptr;
static int vx_tmp_cap = 0, vx_tmp_n = 0;
static void tmp_push(const VxVert *v6) {
    if (vx_tmp_n + 6 > vx_tmp_cap) {
        vx_tmp_cap = vx_tmp_cap ? vx_tmp_cap * 2 : 262144;
        vx_tmp = (VxVert *)realloc(vx_tmp, sizeof(VxVert) * (size_t)vx_tmp_cap);
    }
    if (!vx_tmp) return;
    memcpy(vx_tmp + vx_tmp_n, v6, sizeof(VxVert) * 6);
    vx_tmp_n += 6;
}

static bool vx_opaque_at(VoxField *v, int x, int y, int z) { return blk_opaque(get_blk(v, x, y, z)); }
// For ambient occlusion only a full opaque cube occludes; a slope, slab, post or pane does not, which
// keeps the corners under a roof from going black.
static bool vx_ao_solid(VoxField *v, int x, int y, int z) {
    return blk_opaque(get_blk(v, x, y, z)) && SH_OF(get_shp(v, x, y, z)) == SH_CUBE;
}
// A face may be culled only against a neighbour that FILLS the shared boundary. When in doubt this
// says no and the face is drawn — the rule that stops shaped neighbours punching holes in the world.
static bool vx_covered(VoxField *v, int x, int y, int z, int face) {
    int nx = x + FN[face][0], ny = y + FN[face][1], nz = z + FN[face][2];
    unsigned char nb = get_blk(v, nx, ny, nz);
    if (!blk_opaque(nb)) return false;
    return shape_covers(get_shp(v, nx, ny, nz), F_OPP[face]);
}

static void vx_lamp_at(VoxField *v, float x, float y, float z, float *lamp) {
    float s = 0;
    for (int i = 0; i < v->light_count; i++) {
        VxLight *l = &v->lights[i];
        float dx = x - l->x, dy = y - l->y, dz = z - l->z;
        float d = sqrtf(dx * dx + dy * dy + dz * dz);
        float f = 1.0f - d / (l->r > 0.001f ? l->r : 0.001f);
        if (f > 0) s += l->level * f * f;
    }
    *lamp = s > 1 ? 1 : s;
}

// How near a water voxel is to its bank, 0 (mid-channel) .. 1 (touching), and whether the bank it is
// nearest is the NORTH one — the camera-facing bank, which wants a dark band under it.
static void vx_water_edge(VoxField *v, int x, int z, float *shallow, float *north) {
    int best = 99, bn = 0;
    for (int r = 1; r <= 6; r++) {
        for (int d = 0; d < 4; d++) {
            int cx = (x + DIRV[d][0] * r) / VX_VPC, cz = (z + DIRV[d][1] * r) / VX_VPC;
            if (cx < 0 || cz < 0 || cx >= v->mw || cz >= v->md) continue;
            if (get_blk(v, x + DIRV[d][0] * r, 0, z + DIRV[d][1] * r) == B_AIR) continue;
            bool water = false;
            for (int y = 0; y < VX_VY; y++) if (get_blk(v, x + DIRV[d][0] * r, y, z + DIRV[d][1] * r) == B_WATER) water = true;
            if (water) continue;
            if (r < best) { best = r; bn = (d == D_N); }
        }
        if (best <= r) break;
    }
    *shallow = best > 6 ? 0.0f : 1.0f - (float)(best - 1) / 6.0f;
    *north = (best <= 2 && bn) ? 1.0f : 0.0f;
}

// One quad (or, with c[3] == c[2], one triangle) of one voxel. Corners are in VOXEL-LOCAL 0..1
// coordinates; `cullface` is the cell boundary the quad lies in, or -1 for an interior surface.
struct VxQuadCtx { VoxField *v; int x, y, z; unsigned char b; };
static void vx_emit_quad(VxQuadCtx *q, const float lc[4][3], const float n[3], int cullface) {
    VoxField *v = q->v;
    if (cullface >= 0 && vx_covered(v, q->x, q->y, q->z, cullface)) return;
    float c[4][3];
    for (int i = 0; i < 4; i++) {
        c[i][0] = ((float)q->x + lc[i][0]) * VOX_S;
        c[i][1] = ((float)q->y + lc[i][1]) * VOX_S;
        c[i][2] = ((float)q->z + lc[i][2]) * VOX_S;
    }
    // Winding is DERIVED, never typed: the cross product of the first two edges must agree with the
    // quad's own normal, else it is flipped. This is why no face can vanish under back-face culling.
    {
        float e1[3] = { c[1][0]-c[0][0], c[1][1]-c[0][1], c[1][2]-c[0][2] };
        float e2[3] = { c[2][0]-c[0][0], c[2][1]-c[0][1], c[2][2]-c[0][2] };
        float ax = e1[1]*e2[2]-e1[2]*e2[1], ay = e1[2]*e2[0]-e1[0]*e2[2], az = e1[0]*e2[1]-e1[1]*e2[0];
        if (ax*n[0] + ay*n[1] + az*n[2] < 0) {
            for (int k = 0; k < 3; k++) { float t = c[1][k]; c[1][k] = c[3][k]; c[3][k] = t; }
        }
    }
    // the face id: the dominant axis of the normal. It picks the top or side palette ramp and, for a
    // wall, the fraction up the block that the grass lip uses.
    int face;
    {
        float ax = n[0] < 0 ? -n[0] : n[0], ay = n[1] < 0 ? -n[1] : n[1], az = n[2] < 0 ? -n[2] : n[2];
        if (ay >= ax && ay >= az) face = n[1] > 0 ? F_TOP : F_BOT;
        else if (az >= ax)        face = n[2] > 0 ? F_SOUTH : F_NORTH;
        else                      face = n[0] > 0 ? F_EAST : F_WEST;
    }
    bool axis = (n[0]*n[0] + n[1]*n[1] + n[2]*n[2]) > 0.999f &&
                ((n[0] == 0 && n[1] == 0) || (n[0] == 0 && n[2] == 0) || (n[1] == 0 && n[2] == 0));
    // the tangent basis for AO
    float tu[3], tw[3];
    if (n[1] > 0.9f || n[1] < -0.9f) { tu[0]=1;tu[1]=0;tu[2]=0; }
    else { float l = sqrtf(n[2]*n[2] + n[0]*n[0]); tu[0]= n[2]/l; tu[1]=0; tu[2]= -n[0]/l; }
    tw[0] = n[1]*tu[2] - n[2]*tu[1]; tw[1] = n[2]*tu[0] - n[0]*tu[2]; tw[2] = n[0]*tu[1] - n[1]*tu[0];
    float ctr[3] = { (c[0][0]+c[1][0]+c[2][0]+c[3][0]) * 0.25f,
                     (c[0][1]+c[1][1]+c[2][1]+c[3][1]) * 0.25f,
                     (c[0][2]+c[1][2]+c[2][2]+c[3][2]) * 0.25f };
    float shade = vx_face_light(n[0], n[1], n[2]);
    float shallow = 0, north = 0;
    if (q->b == B_WATER && face == F_TOP) vx_water_edge(v, q->x, q->z, &shallow, &north);
    VxVert corner[4];
    for (int i = 0; i < 4; i++) {
        // the pattern basis. The six axis faces keep exactly the mapping they had, so nothing that
        // already looked right moved; a sloped face gets its own basis, with the run measured as arc
        // length so a roof course is the same width on the pitch as it is on the flat.
        float U, W;
        if (axis && (face == F_TOP || face == F_BOT))      { U = c[i][0]; W = c[i][2]; }
        else if (axis && (face == F_SOUTH || face == F_NORTH)) { U = c[i][0]; W = -c[i][1]; }
        else if (axis)                                     { U = c[i][2]; W = -c[i][1]; }
        else {
            float hl = sqrtf(n[0]*n[0] + n[2]*n[2]);
            float rx = hl > 0.001f ? n[0]/hl : 0.0f, rz = hl > 0.001f ? n[2]/hl : 1.0f;
            U = c[i][0]*rz - c[i][2]*rx;
            if (n[1] > 0.3f) W = (c[i][0]*rx + c[i][2]*rz) / (n[1] < 0.2f ? 0.2f : n[1]);
            else W = -c[i][1];
        }
        // classic three-neighbour ambient occlusion, generalised: sample the two edge neighbours and
        // the diagonal of the cell just outside the surface at this corner.
        float base[3] = { c[i][0]/VOX_S + n[0]*0.5f, c[i][1]/VOX_S + n[1]*0.5f, c[i][2]/VOX_S + n[2]*0.5f };
        float dv[3] = { c[i][0]-ctr[0], c[i][1]-ctr[1], c[i][2]-ctr[2] };
        float su = (dv[0]*tu[0]+dv[1]*tu[1]+dv[2]*tu[2]) < 0 ? -0.6f : 0.6f;
        float sw = (dv[0]*tw[0]+dv[1]*tw[1]+dv[2]*tw[2]) < 0 ? -0.6f : 0.6f;
        bool s1 = vx_ao_solid(v, (int)floorf(base[0]+tu[0]*su), (int)floorf(base[1]+tu[1]*su), (int)floorf(base[2]+tu[2]*su));
        bool s2 = vx_ao_solid(v, (int)floorf(base[0]+tw[0]*sw), (int)floorf(base[1]+tw[1]*sw), (int)floorf(base[2]+tw[2]*sw));
        bool cc = vx_ao_solid(v, (int)floorf(base[0]+tu[0]*su+tw[0]*sw), (int)floorf(base[1]+tu[1]*su+tw[1]*sw), (int)floorf(base[2]+tu[2]*su+tw[2]*sw));
        int ao = (s1 && s2) ? 0 : 3 - ((s1?1:0) + (s2?1:0) + (cc?1:0));
        float lamp = 0;
        vx_lamp_at(v, c[i][0], c[i][1], c[i][2], &lamp);
        if (q->b == B_WINDOW) lamp = 0.9f;             // lit from inside; the bloom catches it at night
        corner[i].x = c[i][0]; corner[i].y = c[i][1]; corner[i].z = c[i][2];
        corner[i].u = U; corner[i].w = W;
        corner[i].type = q->b; corner[i].face = (unsigned char)face;
        corner[i].lit = (unsigned char)(shade * 255.0f + 0.5f);
        corner[i].warm = (unsigned char)(lamp * 255.0f + 0.5f);
        corner[i].ao = (unsigned char)(ao * 85);
        corner[i].spare = (unsigned char)((int)(shallow * 127.0f) | (north > 0.5f ? 128 : 0));
        corner[i].nx = (signed char)(n[0] * 127.0f); corner[i].ny = (signed char)(n[1] * 127.0f);
        corner[i].nz = (signed char)(n[2] * 127.0f); corner[i].npad = 0;
    }
    VxVert q6[6];
    q6[0] = corner[0]; q6[1] = corner[1]; q6[2] = corner[2];
    q6[3] = corner[0]; q6[4] = corner[2]; q6[5] = corner[3];
    tmp_push(q6);
}

// An axis-aligned box inside one voxel, in local 0..1 coordinates. A face flush with the voxel
// boundary is cullable against the neighbour; a face inside the voxel never is.
static void vx_emit_box(VxQuadCtx *q, float x0, float y0, float z0, float x1, float y1, float z1) {
    const float EPS = 0.0005f;
    for (int f = 0; f < 6; f++) {
        float c[4][3]; float n[3] = { (float)FN[f][0], (float)FN[f][1], (float)FN[f][2] };
        int cull = -1;
        switch (f) {
            case F_TOP:   if (y1 > 1 - EPS) cull = f;
                c[0][0]=x0;c[0][1]=y1;c[0][2]=z0; c[1][0]=x1;c[1][1]=y1;c[1][2]=z0;
                c[2][0]=x1;c[2][1]=y1;c[2][2]=z1; c[3][0]=x0;c[3][1]=y1;c[3][2]=z1; break;
            case F_BOT:   if (y0 < EPS) cull = f;
                c[0][0]=x0;c[0][1]=y0;c[0][2]=z1; c[1][0]=x1;c[1][1]=y0;c[1][2]=z1;
                c[2][0]=x1;c[2][1]=y0;c[2][2]=z0; c[3][0]=x0;c[3][1]=y0;c[3][2]=z0; break;
            case F_SOUTH: if (z1 > 1 - EPS) cull = f;
                c[0][0]=x0;c[0][1]=y0;c[0][2]=z1; c[1][0]=x1;c[1][1]=y0;c[1][2]=z1;
                c[2][0]=x1;c[2][1]=y1;c[2][2]=z1; c[3][0]=x0;c[3][1]=y1;c[3][2]=z1; break;
            case F_NORTH: if (z0 < EPS) cull = f;
                c[0][0]=x1;c[0][1]=y0;c[0][2]=z0; c[1][0]=x0;c[1][1]=y0;c[1][2]=z0;
                c[2][0]=x0;c[2][1]=y1;c[2][2]=z0; c[3][0]=x1;c[3][1]=y1;c[3][2]=z0; break;
            case F_EAST:  if (x1 > 1 - EPS) cull = f;
                c[0][0]=x1;c[0][1]=y0;c[0][2]=z1; c[1][0]=x1;c[1][1]=y0;c[1][2]=z0;
                c[2][0]=x1;c[2][1]=y1;c[2][2]=z0; c[3][0]=x1;c[3][1]=y1;c[3][2]=z1; break;
            default:      if (x0 < EPS) cull = f;
                c[0][0]=x0;c[0][1]=y0;c[0][2]=z0; c[1][0]=x0;c[1][1]=y0;c[1][2]=z1;
                c[2][0]=x0;c[2][1]=y1;c[2][2]=z1; c[3][0]=x0;c[3][1]=y1;c[3][2]=z0; break;
        }
        vx_emit_quad(q, c, n, cull);
    }
}

// The ramp family — slope, outer corner, inner corner — as one prism defined by its four corner
// heights: two triangles for the top surface, a trapezoid per side, and a bottom.
static void vx_emit_prism(VxQuadCtx *q, const float h[4]) {
    static const float PX[4] = { 0, 1, 1, 0 }, PZ[4] = { 0, 0, 1, 1 };
    // the top, as two triangles, each with its own normal
    static const int TRI[2][3] = { {0,1,2}, {0,2,3} };
    for (int t = 0; t < 2; t++) {
        float c[4][3];
        for (int i = 0; i < 3; i++) { int k = TRI[t][i]; c[i][0]=PX[k]; c[i][1]=h[k]; c[i][2]=PZ[k]; }
        c[3][0]=c[2][0]; c[3][1]=c[2][1]; c[3][2]=c[2][2];
        float e1[3] = { c[1][0]-c[0][0], c[1][1]-c[0][1], c[1][2]-c[0][2] };
        float e2[3] = { c[2][0]-c[0][0], c[2][1]-c[0][1], c[2][2]-c[0][2] };
        float n[3] = { e1[1]*e2[2]-e1[2]*e2[1], e1[2]*e2[0]-e1[0]*e2[2], e1[0]*e2[1]-e1[1]*e2[0] };
        float l = sqrtf(n[0]*n[0]+n[1]*n[1]+n[2]*n[2]);
        if (l < 1e-6f) continue;
        n[0]/=l; n[1]/=l; n[2]/=l;
        if (n[1] < 0) { n[0]=-n[0]; n[1]=-n[1]; n[2]=-n[2]; }
        vx_emit_quad(q, c, n, -1);
    }
    // the four sides; each is a trapezoid from y=0 to the two corner heights on that edge
    static const int SIDE[4][2] = { {3,2}, {0,1}, {1,2}, {0,3} };   // S, N, E, W  (in F_* order - 2)
    for (int s = 0; s < 4; s++) {
        int a = SIDE[s][0], b = SIDE[s][1];
        if (h[a] < 0.001f && h[b] < 0.001f) continue;
        int face = s + 2;
        float n[3] = { (float)FN[face][0], (float)FN[face][1], (float)FN[face][2] };
        float c[4][3];
        c[0][0]=PX[a]; c[0][1]=0;    c[0][2]=PZ[a];
        c[1][0]=PX[b]; c[1][1]=0;    c[1][2]=PZ[b];
        c[2][0]=PX[b]; c[2][1]=h[b]; c[2][2]=PZ[b];
        c[3][0]=PX[a]; c[3][1]=h[a]; c[3][2]=PZ[a];
        vx_emit_quad(q, c, n, (h[a] > 0.999f && h[b] > 0.999f) ? face : -1);
    }
    float c[4][3] = { {0,0,1}, {1,0,1}, {1,0,0}, {0,0,0} };
    float n[3] = { 0, -1, 0 };
    vx_emit_quad(q, c, n, F_BOT);
}

static void vx_emit_shape(VxQuadCtx *q, unsigned char s) {
    int sh = SH_OF(s), o = OR_OF(s);
    switch (sh) {
        case SH_CUBE: vx_emit_box(q, 0,0,0, 1,1,1); break;
        case SH_SLAB: if (o == SL_BOT) vx_emit_box(q, 0,0,0, 1,0.5f,1); else vx_emit_box(q, 0,0.5f,0, 1,1,1); break;
        case SH_SLOPE: case SH_SLOPE_OUT: case SH_SLOPE_IN: { float h[4]; shape_corner_h(s, h);
vx_emit_prism(q, h); } break;
        case SH_STAIRS: {
            for (int i = 0; i < 3; i++) {
                float t0 = (float)i / 3.0f, hgt = (float)(i + 1) / 3.0f;
                if (o == D_S)      vx_emit_box(q, 0, 0, t0, 1, hgt, 1);
                else if (o == D_N) vx_emit_box(q, 0, 0, 0, 1, hgt, 1.0f - t0);
                else if (o == D_E) vx_emit_box(q, t0, 0, 0, 1, hgt, 1);
                else               vx_emit_box(q, 0, 0, 0, 1.0f - t0, hgt, 1);
            }
        } break;
        case SH_POST: {
            vx_emit_box(q, 0.3f, 0, 0.3f, 0.7f, 1, 0.7f);
            // rails, reaching out to any neighbouring fence or solid cell
            for (int d = 0; d < 4; d++) {
                unsigned char nb = get_blk(q->v, q->x + DIRV[d][0], q->y, q->z + DIRV[d][1]);
                if (!blk_opaque(nb)) continue;
                for (int r = 0; r < 2; r++) {
                    float y0 = r ? 0.62f : 0.26f, y1 = y0 + 0.16f;
                    if (d == D_S)      vx_emit_box(q, 0.38f, y0, 0.6f, 0.62f, y1, 1.0f);
                    else if (d == D_N) vx_emit_box(q, 0.38f, y0, 0.0f, 0.62f, y1, 0.4f);
                    else if (d == D_E) vx_emit_box(q, 0.6f, y0, 0.38f, 1.0f, y1, 0.62f);
                    else               vx_emit_box(q, 0.0f, y0, 0.38f, 0.4f, y1, 0.62f);
                }
            }
        } break;
        case SH_PANE: {
            if (o == PN_S)      vx_emit_box(q, 0, 0, 0.82f, 1, 1, 1);
            else if (o == PN_N) vx_emit_box(q, 0, 0, 0, 1, 1, 0.18f);
            else if (o == PN_E) vx_emit_box(q, 0.82f, 0, 0, 1, 1, 1);
            else if (o == PN_W) vx_emit_box(q, 0, 0, 0, 0.18f, 1, 1);
            else if (o == PN_MIDX) vx_emit_box(q, 0, 0, 0.41f, 1, 1, 0.59f);
            else                   vx_emit_box(q, 0.41f, 0, 0, 0.59f, 1, 1);
        } break;
        default: vx_emit_box(q, 0,0,0, 1,1,1); break;
    }
}

static void vx_mesh_chunk(VoxField *v, int cx, int cz) {
    vx_tmp_n = 0;
    int x0 = cx * VX_CHV, z0 = cz * VX_CHV;
    int x1 = x0 + VX_CHV > v->vw ? v->vw : x0 + VX_CHV;
    int z1 = z0 + VX_CHV > v->vd ? v->vd : z0 + VX_CHV;
    float lo[3] = { 1e9f, 1e9f, 1e9f }, hi[3] = { -1e9f, -1e9f, -1e9f };
    for (int z = z0; z < z1; z++) for (int x = x0; x < x1; x++) for (int y = 0; y < VX_VY; y++) {
        unsigned char b = v->blk[y][z][x];
        if (b == B_AIR) continue;
        VxQuadCtx q = { v, x, y, z, b };
        if (BLOCKS[b].water) {
            // water draws only its surface, and only where there is nothing above it
            if (get_blk(v, x, y + 1, z) != B_AIR) continue;
            float c[4][3] = { {0,0.8f,0}, {1,0.8f,0}, {1,0.8f,1}, {0,0.8f,1} };
            float n[3] = { 0, 1, 0 };
            vx_emit_quad(&q, c, n, -1);
        } else {
            vx_emit_shape(&q, v->shp[y][z][x]);
        }
        for (int k = 0; k < 3; k++) {
            float p = (k == 0 ? x : k == 1 ? y : z) * VOX_S;
            if (p < lo[k]) lo[k] = p;
            if (p + VOX_S > hi[k]) hi[k] = p + VOX_S;
        }
    }
    VxChunk *ch = &v->chunks[cz * VX_CHX + cx];
    if (!ch->vbo) glGenBuffers(1, &ch->vbo);
    glBindBuffer(GL_ARRAY_BUFFER, ch->vbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof(VxVert) * (size_t)vx_tmp_n, vx_tmp, GL_STATIC_DRAW);
    ch->verts = vx_tmp_n;
    for (int k = 0; k < 3; k++) { ch->lo[k] = lo[k]; ch->hi[k] = hi[k]; }
    v->vert_total += vx_tmp_n;
    v->tris += vx_tmp_n / 3;
}

static void vx_mesh_all(VoxField *v) {
    uint64_t t0 = SDL_GetTicksNS();
    v->tris = 0; v->vert_total = 0;
    for (int i = 0; i < VX_CHUNKS; i++) v->chunks[i].verts = 0;
    for (int cz = 0; cz * VX_CHV < v->vd; cz++) for (int cx = 0; cx * VX_CHV < v->vw; cx++) vx_mesh_chunk(v, cx, cz);
    v->mesh_ms = (double)(SDL_GetTicksNS() - t0) / 1e6;
    v->built = true;
    // Load-time cost, reported once in the HUD's mesh/upload row and in the benchmark header.
    vxp.s[VXP_MESH].cpu_ms = vxp.s[VXP_MESH].cpu_avg = v->mesh_ms;
    vxp.c.vbo_mb = (double)v->vert_total * sizeof(VxVert) / (1024.0 * 1024.0);
}

// ───────────────────────── shaders ─────────────────────────
// Three programs: the world, the billboards, and the HD-2D post pass. No loops anywhere in GLSL —
// a small `for` was miscompiled by the Mac driver once in this repo and the habit stays.

#if defined(__ANDROID__)
static const char *VX_PREFIX = "#version 300 es\nprecision highp float;\nprecision highp sampler2D;\n";
#else
static const char *VX_PREFIX = "#version 330 core\n";
#endif

// Depth-only, for the sun's view of the static world. Rendered at map load and again only when the
// sun moves; it is the whole of the cast shadow.
// Uniform locations, cached. glGetUniformLocation is a driver-side string lookup and the render path
// made about forty of them a frame; they are the same forty every frame. Keyed by program id and by
// the ADDRESS of the name, which works because every name here is a string literal in this file — and
// if two identical literals are not pooled we simply get two entries holding the same answer, so the
// result is correct either way. Reset whenever the programs are rebuilt (a hot reload re-inits GL),
// because a fresh program can be handed a recycled id.
#define VX_UNI_CACHE 256
static struct { GLuint prog; const char *name; GLint loc; } vx_uni_c[VX_UNI_CACHE];
static int vx_uni_n = 0;
static void vx_uni_reset(void) { vx_uni_n = 0; }
static GLint vx_uni(GLuint prog, const char *name) {
    for (int i = 0; i < vx_uni_n; i++)
        if (vx_uni_c[i].prog == prog && vx_uni_c[i].name == name) return vx_uni_c[i].loc;
    GLint l = glGetUniformLocation(prog, name);
    if (vx_uni_n < VX_UNI_CACHE) { vx_uni_c[vx_uni_n].prog = prog; vx_uni_c[vx_uni_n].name = name;
                                   vx_uni_c[vx_uni_n].loc = l; vx_uni_n++; }
    return l;
}

static const char *VX_SHADOW_VS =
    "layout(location=0) in vec3 a_pos;\n"
    "uniform mat4 u_lmvp;\n"
    "void main(){ gl_Position = u_lmvp * vec4(a_pos, 1.0); }\n";
static const char *VX_SHADOW_FS = "void main(){}\n";

static const char *VX_VS =
    "layout(location=0) in vec3 a_pos;\n"
    "layout(location=1) in vec2 a_uw;\n"             // the pattern basis, in world units
    "layout(location=2) in uvec4 a_meta;\n"          // type, face, ao(0..255), spare
    "layout(location=3) in vec2 a_light;\n"          // face term, baked lamp
    "layout(location=4) in vec3 a_nrm;\n"
    "uniform mat4 u_mvp;\n"
    "uniform mat4 u_view;\n"
    "uniform mat4 u_lmvp;\n"
    "uniform float u_snap, u_noff;\n"
    "out vec3 v_world;\n"
    "flat out uint v_type;\n"
    "flat out uint v_face;\n"
    "out float v_ao;\n"
    "out vec2 v_light;\n"
    "out vec2 v_uw;\n"
    "out float v_spare;\n"
    "out vec3 v_nrm;\n"
    "out vec3 v_lpos;\n"
    "out float v_viewz;\n"
    "void main(){\n"
    "  v_world = a_pos;\n"
    "  v_uw = a_uw;\n"
    "  v_type = a_meta.x; v_face = a_meta.y; v_ao = float(a_meta.z) / 255.0;\n"
    "  v_spare = float(a_meta.w);\n"
    "  v_light = a_light;\n"
    "  vec3 n = normalize(a_nrm);\n"
    "  v_nrm = n;\n"
    // Snapped: the shadow is looked up at the 1/16 texel grid, so its edge is a pixel-art staircase
    // that belongs to the same grid the patterns are drawn on. Smooth: the true position.
    "  vec3 sp = mix(a_pos, (floor(a_pos * 16.0) + 0.5) / 16.0, u_snap);\n"
    "  vec4 lp = u_lmvp * vec4(sp + n * u_noff, 1.0);\n"
    "  v_lpos = lp.xyz / lp.w * 0.5 + 0.5;\n"
    "  vec4 vp = u_view * vec4(a_pos, 1.0);\n"
    "  v_viewz = -vp.z;\n"
    "  gl_Position = u_mvp * vec4(a_pos, 1.0);\n"
    "}\n";

static const char *VX_FS_HEAD =
    "in vec3 v_world;\n"
    "flat in uint v_type;\n"
    "flat in uint v_face;\n"
    "in float v_ao;\n"
    "in vec2 v_light;\n"
    "in vec2 v_uw;\n"
    "in float v_spare;\n"
    "in vec3 v_nrm;\n"
    "in vec3 v_lpos;\n"
    "in float v_viewz;\n"
    "uniform sampler2D u_lut;\n"
    "uniform sampler2D u_cmap;\n"
    "uniform sampler2DShadow u_shadow;\n"
    "uniform float u_amb, u_ao, u_levels, u_cmaph, u_rowa, u_rowb, u_time;\n"
    "uniform float u_fog, u_fognear, u_fogfar;\n"
    "uniform float u_shstr, u_shsoft, u_shtexel;\n"
    "uniform vec3 u_sundir;\n"
    "uniform vec3 u_sky;\n"
    "uniform vec4 u_player;\n"                 // screen x, y, view z, cutaway radius in px
    // The benchmark's knobs. All three are uniform branches — one value for the whole draw, so the
    // wavefront never diverges — and all three sit at the shipping look unless a benchmark moved them.
    "uniform float u_qpattern, u_qpcf, u_qwater;\n"
    "out vec4 o;\n"
    // Four rotated-poisson taps, UNROLLED (this repo has been bitten by a driver miscompiling a
    // GLSL loop), each one hardware-PCF'd, so the edge is soft without being mush.
    "float shadow_at(vec3 lp, float soft){\n"
    "  if (lp.z > 1.0 || lp.x < 0.0 || lp.x > 1.0 || lp.y < 0.0 || lp.y > 1.0) return 1.0;\n"
    "  float r = u_shtexel * soft;\n"
    "  if (u_qpcf < 1.5) return texture(u_shadow, vec3(lp.xy, lp.z));\n"
    "  float s = texture(u_shadow, vec3(lp.xy + vec2( 0.94, 0.34) * r, lp.z));\n"
    "  s += texture(u_shadow, vec3(lp.xy + vec2(-0.85, 0.52) * r, lp.z));\n"
    "  s += texture(u_shadow, vec3(lp.xy + vec2(-0.20,-0.98) * r, lp.z));\n"
    "  s += texture(u_shadow, vec3(lp.xy + vec2( 0.40,-0.30) * r * 0.4, lp.z));\n"
    "  return s * 0.25;\n"
    "}\n"
    "float h21(vec2 p){ p = fract(p * vec2(127.11, 311.7)); p += dot(p, p + 34.77); return fract(p.x * p.y); }\n"
    "float vn(vec2 p){ vec2 i = floor(p), f = fract(p); f = f*f*(3.0-2.0*f);\n"
    "  float a = h21(i), b = h21(i+vec2(1,0)), c = h21(i+vec2(0,1)), d = h21(i+vec2(1,1));\n"
    "  return mix(mix(a,b,f.x), mix(c,d,f.x), f.y); }\n"
    "vec3 look(float idx, float row){ return texture(u_cmap, vec2((idx+0.5)/256.0, (row+0.5)/u_cmaph)).rgb; }\n"
    "void main(){\n";

// The cutaway, and it is a SEPARATE PROGRAM. It used to sit inside the one world shader under a
// uniform branch — but a fragment shader that contains `discard` anywhere is a shader the GPU cannot
// assume writes depth at the rasterized value, so Adreno turns LRZ (its early-Z / hidden-surface
// removal) OFF for the whole draw. Every chunk of the town was then shaded in full — ten hash
// evaluations and four shadow taps a fragment — for pixels a nearer wall went on to cover.
//
// So: two programs from the same source, one with this block and one without, and the cutaway one is
// used only for the chunks that could actually hold the fragments it discards (nearer than the party
// and overlapping the cutaway circle on screen — vx_chunk_needs_cut). Everything else runs the
// discard-free variant and keeps early-Z. The pixels that come out are identical either way, because
// a chunk that fails that test has no fragment the block would have discarded.
static const char *VX_FS_CUT =
    "  if (u_player.w > 0.5 && v_viewz < u_player.z - 0.6) {\n"
    "    float dsc = length(gl_FragCoord.xy - u_player.xy) / u_player.w;\n"
    "    if (dsc < 1.0) { if (dsc < 0.76 || h21(floor(gl_FragCoord.xy / 3.0)) > (dsc - 0.76) / 0.24) discard; }\n"
    "  }\n";

static const char *VX_FS_BODY =
    "  int ty = int(v_type);\n"
    "  int pat = int(texelFetch(u_lut, ivec2(12, ty), 0).r * 255.0 + 0.5);\n"
    "  if (u_qpattern < 0.5) pat = 99;\n"
    "  bool top = (v_face == 0u || v_face == 1u);\n"
    "  vec2 P = v_uw;\n"
    "  float vfrac = top ? 1.0 : fract(v_world.y * 2.0 + 0.0001);\n"
    "  vec2 T = floor(P * 16.0);\n"              // the block's own 16x16 pixel grid, in world space
    "  float s = 0.5; bool use_top = top; float glint = 0.0;\n"
    "  if (pat == 0) {\n"                        // mottle: earth, gravel, mud
    "    float n = vn(T / 3.1) * 0.6 + h21(T * 0.71) * 0.4;\n"
    "    s = n < 0.30 ? 0.22 : n < 0.52 ? 0.44 : n < 0.74 ? 0.62 : n < 0.92 ? 0.80 : 1.0;\n"
    "  } else if (pat == 1) {\n"                 // grass: clumps on top, a lip of turf over the sides
    "    float n = vn(T / 2.6) * 0.65 + h21(T * 0.37) * 0.35;\n"
    "    if (!top && vfrac < 0.80) {\n"
    "      use_top = false;\n"
    "      float m = vn(T / 3.0) * 0.6 + h21(T * 0.53) * 0.4;\n"
    "      s = m < 0.34 ? 0.24 : m < 0.60 ? 0.46 : m < 0.84 ? 0.66 : 0.86;\n"
    "    } else {\n"
    "      use_top = true;\n"
    "      float edge = top ? 1.0 : smoothstep(0.80, 0.86, vfrac + h21(T * 0.91) * 0.05);\n"
    "      s = n < 0.26 ? 0.18 : n < 0.48 ? 0.42 : n < 0.70 ? 0.62 : n < 0.88 ? 0.82 : 1.0;\n"
    "      s = mix(0.45, s, edge);\n"
    "    }\n"
    "  } else if (pat == 2) {\n"                 // cobble: a warped grid, dark mortar, a lit top lip
    "    vec2 Q = T + vec2(vn(T / 5.0) * 1.7, vn(T / 6.0 + 4.0) * 1.3);\n"
    "    vec2 cs = vec2(6.0, 4.0);\n"
    "    float row = floor(Q.y / cs.y);\n"
    "    float ox = (h21(vec2(row, 2.0)) * 0.7 + mod(row, 2.0) * 0.4) * cs.x;\n"
    "    vec2 cell = vec2(floor((Q.x + ox) / cs.x), row);\n"
    "    vec2 lp = vec2(mod(Q.x + ox, cs.x), mod(Q.y, cs.y));\n"
    "    s = 0.56 + (h21(cell) - 0.5) * 0.45;\n"
    "    if (lp.x < 1.0 || lp.y < 1.0) s = 0.10;\n"
    "    else if (lp.y < 2.0) s += 0.18;\n"
    "  } else if (pat == 3) {\n"                 // planks
    "    float pw = 4.0;\n"
    "    float pi = floor((top ? T.y : T.x) / pw), lp = mod((top ? T.y : T.x), pw);\n"
    "    s = 0.55 + (h21(vec2(pi, 3.0)) - 0.5) * 0.36;\n"
    "    if (lp < 1.0) s = 0.14;\n"
    "    float along = top ? T.x : T.y;\n"
    "    if (mod(along + h21(vec2(pi, floor(along / 22.0))) * 12.0, 22.0) < 1.0) s = 0.20;\n"
    "  } else if (pat == 4) {\n"                 // roof tiles: short courses, each with a lit lip
    "    float ch2 = 3.0;\n"
    "    float row = floor(T.y / ch2), lp = mod(T.y, ch2);\n"
    "    float ox = mod(row, 2.0) * 2.0;\n"
    "    float cell = floor((T.x + ox) / 4.0);\n"
    "    s = 0.55 + (h21(vec2(cell, row)) - 0.5) * 0.30;\n"
    "    if (lp < 1.0) s = 0.12;\n"
    "    else if (lp < 1.8) s += 0.22;\n"
    "    if (mod(T.x + ox, 4.0) < 1.0) s -= 0.16;\n"
    "  } else if (pat == 5) {\n"                 // water: bands quantised to 6 fps, with hard glints
    "    float tq = floor(u_time * 6.0) / 6.0 * u_qwater;\n"
    "    float a = vn(vec2(P.x * 1.4 - tq * 0.7, P.y * 0.8));\n"
    "    float b = vn(vec2(P.x * 3.1 - tq * 1.3, P.y * 1.9 + tq * 0.2));\n"
    "    s = 0.18;\n"
    "    if (a > 0.46) s = 0.42;\n"
    "    if (b > 0.60) s = 0.70;\n"
    "    float sp = h21(T + floor(tq * 6.0) * 7.0);\n"
    "    if (sp > 0.992 && b > 0.5) { s = 1.0; glint = 1.0; }\n"
    "  } else if (pat == 6) {\n"                 // thatch: long combed straw
    "    float n = vn(vec2(T.x * 0.8, T.y * 0.12));\n"
    "    s = n < 0.34 ? 0.28 : n < 0.58 ? 0.52 : n < 0.82 ? 0.72 : 0.92;\n"
    "    if (mod(T.y, 5.0) < 1.0) s -= 0.22;\n"
    "  } else if (pat == 7) {\n"                 // crop rows
    "    float lp = mod(T.y, 4.0);\n"
    "    s = 0.34 + vn(T / 7.0) * 0.10;\n"
    "    if (lp < 1.5) s = 0.66;\n"
    "    if (lp < 2.5 && h21(vec2(floor(T.x / 2.0), floor(T.y / 4.0))) > 0.45) s = 0.90;\n"
    "  } else if (pat == 8) {\n"                 // leaves: chunky blobs, a little sky through them
    "    float n = vn(T / 3.4) * 0.7 + h21(T * 0.29) * 0.3;\n"
    "    s = n < 0.30 ? 0.12 : n < 0.50 ? 0.38 : n < 0.70 ? 0.60 : n < 0.88 ? 0.82 : 1.0;\n"
    "  } else if (pat == 9) {\n"                 // bark
    "    float n = vn(vec2(T.x * 1.6, T.y * 0.20));\n"
    "    s = n < 0.36 ? 0.18 : n < 0.62 ? 0.46 : n < 0.85 ? 0.70 : 0.92;\n"
    "  } else if (pat == 11) {\n"                // dressed stone: big courses
    "    vec2 cs = vec2(8.0, 5.0);\n"
    "    float row = floor(T.y / cs.y);\n"
    "    float ox = mod(row, 2.0) * 4.0;\n"
    "    vec2 cell = vec2(floor((T.x + ox) / cs.x), row);\n"
    "    vec2 lp = vec2(mod(T.x + ox, cs.x), mod(T.y, cs.y));\n"
    "    s = 0.56 + (h21(cell) - 0.5) * 0.30;\n"
    "    if (lp.x < 1.0 || lp.y < 1.0) s = 0.14;\n"
    "    else if (lp.y < 2.0) s += 0.16;\n"
    "  } else if (pat == 12) {\n"                // plaster: nearly flat, a few worn patches
    "    float n = vn(T / 6.0);\n"
    "    s = 0.76 + (n - 0.5) * 0.34;\n"
    "    if (h21(floor(T / 7.0) + 3.3) > 0.90) s -= 0.26;\n"
    "  } else {\n"
    "    s = 0.62;\n"
    "  }\n"
    // the shallows: a lighter band of water near the bank, and a dark one under the north bank, both
    // from a per-vertex distance-to-bank baked at mesh time
    "  if (pat == 5) {\n"
    "    float shal = mod(v_spare, 128.0) / 127.0;\n"
    "    float nb = step(127.5, v_spare);\n"
    "    s = clamp(s + shal * 0.30 - nb * 0.34, 0.0, 1.0);\n"
    "  }\n"
    "  int step6 = int(clamp(floor(s * 5.0 + 0.5), 0.0, 5.0));\n"
    "  float idx = texelFetch(u_lut, ivec2(step6 + (use_top ? 0 : 6), ty), 0).r * 255.0;\n"
    "  float ao = 1.0 - (1.0 - v_ao) * u_ao;\n"
    "  float lamp = v_light.y;\n"
    // The cast shadow is a LIGHT LEVEL, not a multiply toward black: it pulls the colormap lookup
    // down the table, so shade stays a cool palette colour (PALETTE.md / D19).
    "  float ndl = clamp(dot(v_nrm, u_sundir), 0.0, 1.0);\n"
    "  float sh = (ndl > 0.02 && u_shstr > 0.002) ? shadow_at(v_lpos, u_shsoft) : 1.0;\n"
    "  float shk = 1.0 - (1.0 - sh) * u_shstr * 0.55;\n"
    "  float lv = clamp(u_amb * v_light.x * ao * shk, 0.0, 1.0);\n"
    "  float warm = clamp((lamp - u_amb) * 1.8, 0.0, 1.0);\n"
    "  lv = max(lv, lamp);\n"
    "  float f = (1.0 - lv) * (u_levels - 1.0);\n"
    "  float r0 = floor(f), fr = f - r0, r1 = min(r0 + 1.0, u_levels - 1.0);\n"
    "  vec3 cA = mix(look(idx, u_rowa + r0), look(idx, u_rowa + r1), fr);\n"
    "  vec3 col = warm > 0.002 ? mix(cA, mix(look(idx, u_rowb + r0), look(idx, u_rowb + r1), fr), warm) : cA;\n"
    "  if (glint > 0.5) col = mix(col, vec3(1.0), 0.45);\n"
    "  float fg = u_fog * smoothstep(u_fognear, u_fogfar, v_viewz);\n"
    "  col = mix(col, u_sky, clamp(fg, 0.0, 0.92));\n"
    "  o = vec4(col, 1.0);\n"
    "}\n";

static const char *VX_SPR_VS =
    "layout(location=0) in vec3 a_pos;\n"
    "layout(location=1) in vec2 a_uv;\n"
    "layout(location=2) in vec4 a_par;\n"          // lit, warm, alpha, kind
    "uniform mat4 u_mvp;\n"
    "uniform mat4 u_view;\n"
    "uniform mat4 u_lmvp;\n"
    "out vec2 v_uv; out vec4 v_par; out float v_viewz; out vec3 v_lpos;\n"
    "void main(){ v_uv = a_uv; v_par = a_par;\n"
    "  vec4 vp = u_view * vec4(a_pos, 1.0); v_viewz = -vp.z;\n"
    "  vec4 lp = u_lmvp * vec4(a_pos + vec3(0.0, 0.12, 0.0), 1.0);\n"
    "  v_lpos = lp.xyz / lp.w * 0.5 + 0.5;\n"
    "  gl_Position = u_mvp * vec4(a_pos, 1.0); }\n";

static const char *VX_SPR_FS =
    "in vec2 v_uv; in vec4 v_par; in float v_viewz; in vec3 v_lpos;\n"
    "uniform sampler2D u_tex; uniform sampler2D u_cmap;\n"
    "uniform sampler2DShadow u_shadow;\n"
    "uniform float u_amb, u_levels, u_cmaph, u_rowa, u_rowb, u_indexed;\n"
    "uniform float u_shstr, u_shsoft, u_shtexel;\n"
    "uniform float u_fog, u_fognear, u_fogfar; uniform vec3 u_sky;\n"
    "out vec4 o;\n"
    "vec3 look(float idx, float row){ return texture(u_cmap, vec2((idx+0.5)/256.0, (row+0.5)/u_cmaph)).rgb; }\n"
    // A character walking into a roof's or a tree's shadow darkens with it: the same map, sampled a
    // little above the feet so the ground they stand on is what decides.
    "float spr_shadow(vec3 lp){\n"
    "  if (lp.z > 1.0 || lp.x < 0.0 || lp.x > 1.0 || lp.y < 0.0 || lp.y > 1.0) return 1.0;\n"
    "  float r = u_shtexel * max(u_shsoft, 1.5);\n"
    "  float s = texture(u_shadow, vec3(lp.xy + vec2( 0.94, 0.34) * r, lp.z));\n"
    "  s += texture(u_shadow, vec3(lp.xy + vec2(-0.85, 0.52) * r, lp.z));\n"
    "  s += texture(u_shadow, vec3(lp.xy + vec2(-0.20,-0.98) * r, lp.z));\n"
    "  s += texture(u_shadow, vec3(lp.xy, lp.z));\n"
    "  return s * 0.25;\n"
    "}\n"
    "void main(){\n"
    "  int kind = int(v_par.w + 0.5);\n"
    "  if (kind == 1) {\n"                        // a blob shadow: no texture, a soft disc
    "    float d = length(v_uv - 0.5) * 2.0;\n"
    "    float a = (1.0 - smoothstep(0.55, 1.0, d)) * v_par.z;\n"
    "    if (a < 0.02) discard;\n"
    "    o = vec4(0.0, 0.0, 0.0, a); return; }\n"
    "  if (kind == 2) {\n"                        // an additive lamp glow
    "    float d = length(v_uv - 0.5) * 2.0;\n"
    "    float a = pow(clamp(1.0 - d, 0.0, 1.0), 2.2) * v_par.z;\n"
    "    if (a < 0.004) discard;\n"
    "    o = vec4(vec3(1.0, 0.86, 0.60) * a, 0.0); return; }\n"
    "  vec4 t = texture(u_tex, v_uv);\n"
    "  vec3 col;\n"
    "  if (u_indexed > 0.5) {\n"
    "    float idx = t.r * 255.0;\n"
    "    if (idx < 0.5) discard;\n"
    "    float lamp = v_par.y;\n"
    "    float sh = u_shstr > 0.002 ? spr_shadow(v_lpos) : 1.0;\n"
    "    float shk = 1.0 - (1.0 - sh) * u_shstr * 0.55;\n"
    "    float lv = max(clamp(u_amb * v_par.x * shk, 0.0, 1.0), lamp);\n"
    "    float warm = clamp((lamp - u_amb) * 1.8, 0.0, 1.0);\n"
    "    float f = (1.0 - lv) * (u_levels - 1.0);\n"
    "    float r0 = floor(f), fr = f - r0, r1 = min(r0 + 1.0, u_levels - 1.0);\n"
    "    vec3 cA = mix(look(idx, u_rowa + r0), look(idx, u_rowa + r1), fr);\n"
    "    col = warm > 0.002 ? mix(cA, mix(look(idx, u_rowb + r0), look(idx, u_rowb + r1), fr), warm) : cA;\n"
    "  } else { if (t.a < 0.5) discard; col = t.rgb * v_par.x; }\n"
    "  float fg = u_fog * smoothstep(u_fognear, u_fogfar, v_viewz);\n"
    "  col = mix(col, u_sky, clamp(fg, 0.0, 0.92));\n"
    "  o = vec4(col, 1.0);\n"
    "}\n";

// The HD-2D pass. One full-screen triangle; the blur is a separable 5-tap at half resolution, so the
// expensive part runs on a quarter of the pixels. Every term has its own strength and can be zero.
static const char *VX_QUAD_VS =
    "out vec2 v_uv;\n"
    "void main(){\n"
    "  vec2 p = vec2((gl_VertexID == 2) ? 3.0 : -1.0, (gl_VertexID == 1) ? 3.0 : -1.0);\n"
    "  v_uv = (p + 1.0) * 0.5;\n"
    "  gl_Position = vec4(p, 1.0, 1.0); }\n";

static const char *VX_BLUR_FS =
    "in vec2 v_uv;\n"
    "uniform sampler2D u_tex; uniform vec2 u_dir; uniform float u_thresh;\n"
    "out vec4 o;\n"
    "void main(){\n"
    "  vec4 c0 = texture(u_tex, v_uv);\n"
    "  vec4 c1 = texture(u_tex, v_uv + u_dir);\n"
    "  vec4 c2 = texture(u_tex, v_uv - u_dir);\n"
    "  vec4 c3 = texture(u_tex, v_uv + u_dir * 2.4);\n"
    "  vec4 c4 = texture(u_tex, v_uv - u_dir * 2.4);\n"
    "  vec4 c = c0 * 0.30 + (c1 + c2) * 0.245 + (c3 + c4) * 0.105;\n"
    "  if (u_thresh > 0.0) {\n"
    "    float l = dot(c.rgb, vec3(0.299, 0.587, 0.114));\n"
    "    c.rgb *= smoothstep(u_thresh, u_thresh + 0.22, l); }\n"
    "  o = c; }\n";

static const char *VX_POST_FS =
    "in vec2 v_uv;\n"
    "uniform sampler2D u_scene; uniform sampler2D u_blur; uniform sampler2D u_bloom;\n"
    "uniform sampler2D u_depth;\n"
    "uniform float u_dof, u_bloom_k, u_vig, u_grade, u_pdepth, u_near, u_far, u_band, u_ortho;\n"
    "out vec4 o;\n"
    "float lin(float d){ if (u_ortho > 0.5) return u_near + d * (u_far - u_near);\n"
    "  float z = d * 2.0 - 1.0; return (2.0 * u_near * u_far) / (u_far + u_near - z * (u_far - u_near)); }\n"
    "void main(){\n"
    "  vec3 c = texture(u_scene, v_uv).rgb;\n"
    "  if (u_dof > 0.0) {\n"
    // Tilt shift: sharp in a band around the party's own depth, blurred away from it. The depth
    // texture is the honest version; the band is in world units, so the focus follows the player.
    "    float d = lin(texture(u_depth, v_uv).r);\n"
    "    float k = clamp((abs(d - u_pdepth) - u_band) / (u_band * 2.2), 0.0, 1.0);\n"
    "    c = mix(c, texture(u_blur, v_uv).rgb, k * u_dof); }\n"
    "  if (u_bloom_k > 0.0) c += texture(u_bloom, v_uv).rgb * u_bloom_k;\n"
    "  if (u_grade > 0.0) {\n"
    "    float l = dot(c, vec3(0.299, 0.587, 0.114));\n"
    "    vec3 warm = c * vec3(1.06, 1.01, 0.93), cool = c * vec3(0.92, 0.97, 1.10);\n"
    "    c = mix(c, mix(cool, warm, smoothstep(0.30, 0.80, l)), u_grade); }\n"
    "  if (u_vig > 0.0) {\n"
    "    vec2 q = (v_uv - 0.5) * vec2(1.0, 0.86);\n"
    "    c *= mix(1.0, clamp(1.12 - dot(q, q) * 1.9, 0.0, 1.0), u_vig); }\n"
    "  o = vec4(c, 1.0); }\n";

// The sky: a vertical gradient between two palette indices through the colormap, with a slow band of
// cloud across the top half. Drawn as one full-screen triangle before the world, which is also why
// the fog colour and the horizon are by construction the same colour.
static const char *VX_SKY_FS =
    "in vec2 v_uv;\n"
    "uniform sampler2D u_cmap;\n"
    "uniform float u_levels, u_cmaph, u_rowa, u_amb, u_time, u_horizon, u_cloud;\n"
    "uniform float u_iz, u_ih, u_ic;\n"                  // zenith, horizon, cloud palette indices
    "out vec4 o;\n"
    "float h21(vec2 p){ p = fract(p * vec2(127.11, 311.7)); p += dot(p, p + 34.77); return fract(p.x * p.y); }\n"
    "float vn(vec2 p){ vec2 i = floor(p), f = fract(p); f = f*f*(3.0-2.0*f);\n"
    "  float a = h21(i), b = h21(i+vec2(1,0)), c = h21(i+vec2(0,1)), d = h21(i+vec2(1,1));\n"
    "  return mix(mix(a,b,f.x), mix(c,d,f.x), f.y); }\n"
    "vec3 look(float idx){ float f = (1.0 - clamp(u_amb, 0.0, 1.0)) * (u_levels - 1.0);\n"
    "  float r0 = floor(f), fr = f - r0, r1 = min(r0 + 1.0, u_levels - 1.0);\n"
    "  vec3 a = texture(u_cmap, vec2((idx+0.5)/256.0, (u_rowa + r0 + 0.5)/u_cmaph)).rgb;\n"
    "  vec3 b = texture(u_cmap, vec2((idx+0.5)/256.0, (u_rowa + r1 + 0.5)/u_cmaph)).rgb;\n"
    "  return mix(a, b, fr); }\n"
    "void main(){\n"
    "  float t = clamp((v_uv.y - u_horizon) / max(1.0 - u_horizon, 0.001), 0.0, 1.0);\n"
    "  vec3 c = mix(look(u_ih), look(u_iz), t * t);\n"
    "  if (u_cloud > 0.0) {\n"
    "    vec2 p = vec2(v_uv.x * 5.0 + u_time * 0.012, (v_uv.y - u_horizon) * 9.0);\n"
    "    float n = vn(p) * 0.6 + vn(p * 2.3 + 7.0) * 0.4;\n"
    "    float band = smoothstep(0.12, 0.45, t) * (1.0 - smoothstep(0.55, 1.0, t));\n"
    "    float k = smoothstep(0.55, 0.72, n) * band * u_cloud;\n"
    "    c = mix(c, look(u_ic), k); }\n"
    "  o = vec4(c, 1.0); }\n";

// The overdraw probe. One constant 1/255 a fragment, blended ONE/ONE with the ordinary depth test
// and depth write, so what accumulates in a pixel is the number of times the opaque world WROTE
// that pixel — which is exactly the shading a perfect depth prepass (or a deferred pass) would have
// saved. It shares VX_VS, so it rasterizes the same triangles in the same places; it has no discard,
// so it does not itself defeat early-Z.
// u_step is 1/255 when the number is being measured (so the byte in the pixel IS the write count)
// and something visible when the debug view is being looked at.
static const char *VX_OD_FS =
    "uniform float u_step;\n"
    "out vec4 o;\n"
    "void main(){ o = vec4(u_step, u_step * 0.55, u_step * 0.25, 1.0); }\n";

static GLuint vx_shader_n(GLenum type, const char **src, int n, const char *what) {
    GLuint s = glCreateShader(type);
    const char *parts[6] = { VX_PREFIX };
    if (n > 5) n = 5;
    for (int i = 0; i < n; i++) parts[i + 1] = src[i];
    glShaderSource(s, n + 1, parts, nullptr);
    glCompileShader(s);
    GLint ok = 0;
    glGetShaderiv(s, GL_COMPILE_STATUS, &ok);
    if (!ok) { char log[2048] = ""; glGetShaderInfoLog(s, sizeof(log) - 1, nullptr, log); SDL_Log("voxfield %s shader: %s", what, log); }
    return s;
}

static GLuint vx_shader(GLenum type, const char *src, const char *what) {
    GLuint s = glCreateShader(type);
    const char *parts[2] = { VX_PREFIX, src };
    glShaderSource(s, 2, parts, nullptr);
    glCompileShader(s);
    GLint ok = 0;
    glGetShaderiv(s, GL_COMPILE_STATUS, &ok);
    if (!ok) { char log[2048] = ""; glGetShaderInfoLog(s, sizeof(log) - 1, nullptr, log); SDL_Log("voxfield %s shader: %s", what, log); }
    return s;
}
static GLuint vx_program(const char *vs_src, const char *fs_src, const char *what) {
    GLuint vs = vx_shader(GL_VERTEX_SHADER, vs_src, what), fs = vx_shader(GL_FRAGMENT_SHADER, fs_src, what);
    GLuint p = glCreateProgram();
    glAttachShader(p, vs); glAttachShader(p, fs);
    glLinkProgram(p);
    GLint ok = 0;
    glGetProgramiv(p, GL_LINK_STATUS, &ok);
    if (!ok) { char log[2048] = ""; glGetProgramInfoLog(p, sizeof(log) - 1, nullptr, log); SDL_Log("voxfield %s link: %s", what, log); }
    glDeleteShader(vs); glDeleteShader(fs);
    return p;
}

// Same, with the fragment source assembled from several pieces — which is how the world's two
// variants (with and without the cutaway's `discard`) are built from one body of code.
static GLuint vx_program_n(const char *vs_src, const char **fs, int n, const char *what) {
    GLuint v_ = vx_shader(GL_VERTEX_SHADER, vs_src, what), f_ = vx_shader_n(GL_FRAGMENT_SHADER, fs, n, what);
    GLuint p = glCreateProgram();
    glAttachShader(p, v_); glAttachShader(p, f_);
    glLinkProgram(p);
    GLint ok = 0;
    glGetProgramiv(p, GL_LINK_STATUS, &ok);
    if (!ok) { char log[2048] = ""; glGetProgramInfoLog(p, sizeof(log) - 1, nullptr, log); SDL_Log("voxfield %s link: %s", what, log); }
    glDeleteShader(v_); glDeleteShader(f_);
    return p;
}

// ───────────────────────── GL objects ─────────────────────────

static void vx_gl_init(VoxField *v) {
    vx_probe_limits();
    vx_uni_reset();          // fresh programs may be handed recycled ids: the cache must not survive
    {   // the world, twice: without the cutaway's discard (early-Z lives) and with it
        const char *plain[2] = { VX_FS_HEAD, VX_FS_BODY };
        const char *cut[3]   = { VX_FS_HEAD, VX_FS_CUT, VX_FS_BODY };
        v->prog     = vx_program_n(VX_VS, plain, 2, "world");
        v->prog_cut = vx_program_n(VX_VS, cut,   3, "world cutaway");
    }
    v->od_prog = vx_program(VX_VS, VX_OD_FS, "overdraw");
    v->spr_prog = vx_program(VX_SPR_VS, VX_SPR_FS, "sprite");
    v->blur_prog = vx_program(VX_QUAD_VS, VX_BLUR_FS, "blur");
    v->post_prog = vx_program(VX_QUAD_VS, VX_POST_FS, "post");
    v->sky_prog = vx_program(VX_QUAD_VS, VX_SKY_FS, "sky");
    v->shadow_prog = vx_program(VX_SHADOW_VS, VX_SHADOW_FS, "shadow");
    // The shadow map of the static world: one depth texture, rendered at load and whenever the sun
    // moves, sampled with hardware PCF.
    v->shadow_dim = vx_max_tex >= 4096 ? 2048 : 1024;
    glGenFramebuffers(1, &v->shadow_fbo);
    glGenTextures(1, &v->shadow_tex);
    glBindTexture(GL_TEXTURE_2D, v->shadow_tex);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT24, v->shadow_dim, v->shadow_dim, 0,
                 GL_DEPTH_COMPONENT, GL_UNSIGNED_INT, nullptr);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_COMPARE_MODE, GL_COMPARE_REF_TO_TEXTURE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_COMPARE_FUNC, GL_LEQUAL);
    glBindFramebuffer(GL_FRAMEBUFFER, v->shadow_fbo);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, v->shadow_tex, 0);
#if !defined(__ANDROID__)
    glDrawBuffer(GL_NONE); glReadBuffer(GL_NONE);
#endif
    {
        GLenum st = glCheckFramebufferStatus(GL_FRAMEBUFFER);
        if (st != GL_FRAMEBUFFER_COMPLETE) SDL_Log("voxfield: shadow FBO incomplete 0x%x", st);
    }
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glGenVertexArrays(1, &v->vao);
    glGenVertexArrays(1, &v->spr_vao);
    glGenVertexArrays(1, &v->quad_vao);
    glGenBuffers(1, &v->spr_vbo);
    glGenTextures(1, &v->fbo_tex);
    glGenTextures(1, &v->fbo_depth);
    glGenFramebuffers(1, &v->fbo);
    v->spr = (VxSpr *)calloc(VX_SPRITES, sizeof(VxSpr));
    vx_load_palette(v);
    vx_load_ramps(v);
    vx_build_colormap(v);
    vx_build_lut(v);
    v->gl_ready = true;
    SDL_Log("voxfield: GL ready, GL_MAX_TEXTURE_SIZE %d", vx_max_tex);
}

static void vx_attach_colour(GLuint fbo, GLuint tex, int w, int h) {
    glBindTexture(GL_TEXTURE_2D, tex);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, w, h, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glBindFramebuffer(GL_FRAMEBUFFER, fbo);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, tex, 0);
}

static void vx_fbo_size(VoxField *v, int w, int h) {
    if (v->fbo_w == w && v->fbo_h == h) return;
    vx_attach_colour(v->fbo, v->fbo_tex, w, h);
    // A depth TEXTURE, not a renderbuffer: the tilt-shift reads it. GLES3 and GL 3.3 both have it.
    glBindTexture(GL_TEXTURE_2D, v->fbo_depth);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT24, w, h, 0, GL_DEPTH_COMPONENT, GL_UNSIGNED_INT, nullptr);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glBindFramebuffer(GL_FRAMEBUFFER, v->fbo);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, v->fbo_depth, 0);
    GLenum st = glCheckFramebufferStatus(GL_FRAMEBUFFER);
    if (st != GL_FRAMEBUFFER_COMPLETE) SDL_Log("voxfield: scene FBO incomplete 0x%x", st);
    v->fbo_w = w; v->fbo_h = h;
    // The half-res pair the blur ping-pongs through.
    int hw = w / 2 < 1 ? 1 : w / 2, hh = h / 2 < 1 ? 1 : h / 2;
    for (int i = 0; i < 3; i++) {
        if (!v->half_fbo[i]) { glGenFramebuffers(1, &v->half_fbo[i]); glGenTextures(1, &v->half_tex[i]); }
        vx_attach_colour(v->half_fbo[i], v->half_tex[i], hw, hh);
    }
    if (!v->out_fbo) { glGenFramebuffers(1, &v->out_fbo); glGenTextures(1, &v->out_tex); }
    vx_attach_colour(v->out_fbo, v->out_tex, w, h);
    v->half_w = hw; v->half_h = hh;
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    SDL_Log("voxfield: scene FBO %dx%d (half %dx%d)", w, h, hw, hh);
}

// ───────────────────────── matrices ─────────────────────────

static void mat_ident(float *m) { memset(m, 0, 64); m[0] = m[5] = m[10] = m[15] = 1; }
static void mat_mul(const float *a, const float *b, float *o) {   // o = a * b, column major
    float r[16];
    for (int c = 0; c < 4; c++) for (int i = 0; i < 4; i++)
        r[c * 4 + i] = a[0 * 4 + i] * b[c * 4 + 0] + a[1 * 4 + i] * b[c * 4 + 1]
                     + a[2 * 4 + i] * b[c * 4 + 2] + a[3 * 4 + i] * b[c * 4 + 3];
    memcpy(o, r, 64);
}
static void mat_persp(float *m, float fovy_deg, float aspect, float zn, float zf) {
    float f = 1.0f / tanf(fovy_deg * 0.5f * 3.14159265f / 180.0f);
    memset(m, 0, 64);
    m[0] = f / aspect; m[5] = f; m[10] = (zf + zn) / (zn - zf); m[11] = -1;
    m[14] = (2 * zf * zn) / (zn - zf);
}
static void mat_ortho(float *m, float hw, float hh, float zn, float zf) {
    memset(m, 0, 64);
    m[0] = 1 / hw; m[5] = 1 / hh; m[10] = -2 / (zf - zn); m[14] = -(zf + zn) / (zf - zn); m[15] = 1;
}
static void mat_look(float *m, const float *eye, const float *ctr) {
    float f[3] = { ctr[0] - eye[0], ctr[1] - eye[1], ctr[2] - eye[2] };
    float fl = sqrtf(f[0] * f[0] + f[1] * f[1] + f[2] * f[2]);
    if (fl < 1e-6f) fl = 1;
    f[0] /= fl; f[1] /= fl; f[2] /= fl;
    float up[3] = { 0, 1, 0 };
    float s[3] = { f[1] * up[2] - f[2] * up[1], f[2] * up[0] - f[0] * up[2], f[0] * up[1] - f[1] * up[0] };
    float sl = sqrtf(s[0] * s[0] + s[1] * s[1] + s[2] * s[2]);
    if (sl < 1e-6f) sl = 1;
    s[0] /= sl; s[1] /= sl; s[2] /= sl;
    float u[3] = { s[1] * f[2] - s[2] * f[1], s[2] * f[0] - s[0] * f[2], s[0] * f[1] - s[1] * f[0] };
    m[0] = s[0]; m[4] = s[1]; m[8]  = s[2];  m[12] = -(s[0] * eye[0] + s[1] * eye[1] + s[2] * eye[2]);
    m[1] = u[0]; m[5] = u[1]; m[9]  = u[2];  m[13] = -(u[0] * eye[0] + u[1] * eye[1] + u[2] * eye[2]);
    m[2] = -f[0]; m[6] = -f[1]; m[10] = -f[2]; m[14] = (f[0] * eye[0] + f[1] * eye[1] + f[2] * eye[2]);
    m[3] = m[7] = m[11] = 0; m[15] = 1;
}

// ───────────────────────── the walker sheets and the atlas ─────────────────────────

static int vx_art_get(VoxField *v, const char *id) {
    for (int i = 0; i < v->art_count; i++) if (!strcmp(v->art[i].id, id)) return i;
    if (v->art_count >= VX_ART) return -1;
    VxArt *a = &v->art[v->art_count];
    memset(a, 0, sizeof(*a));
    snprintf(a->id, sizeof(a->id), "%s", id);
    char rel[128];
    snprintf(rel, sizeof(rel), "field/walkers/%s.png", id);
    size_t sz = 0;
    void *data = vx_read(rel, &sz);
    if (!data) { SDL_Log("voxfield: walker \"%s\" has no sheet — no sprite", id); return -1; }
    int w = 0, h = 0, c = 0;
    unsigned char *px = stbi_load_from_memory((const unsigned char *)data, (int)sz, &w, &h, &c, 4);
    SDL_free(data);
    if (!px || w < 4 || h < 4) { if (px) stbi_image_free(px); SDL_Log("voxfield: walker \"%s\" unreadable", id); return -1; }
    char label[64];
    snprintf(label, sizeof(label), "walker %s", id);
    a->tex = vx_tex_indexed(v, label, px, w, h);
    stbi_image_free(px);
    a->w = w; a->h = h;
    // Layout: the same rule the tile field uses — a 2:3 sheet is the 3-column contract, else 4x4.
    a->ncols = 4; a->nrows = 4; a->fw = w / 4; a->fh = h / 4;
    a->col_stand = 0; a->col_a = 1; a->col_b = 3;
    for (int i = 0; i < 4; i++) { a->row_of[i] = i; a->flip[i] = false; }
    if (w * 3 == h * 2) {
        a->ncols = 3; a->nrows = 4; a->fw = w / 3; a->fh = h / 4;
        a->col_stand = 0; a->col_a = 1; a->col_b = 2;
    }
    char jrel[128];
    snprintf(jrel, sizeof(jrel), "field/walkers/%s.json", id);
    size_t jsz = 0;
    char *js = (char *)vx_read(jrel, &jsz);
    if (js) {
        char rows[8][8] = {{0}};
        int nr = 0, fw = 0, fh = 0, nc = 0;
        const char *r = strstr(js, "\"rows\"");
        if (r) for (const char *cc = strchr(r, '['); cc && *cc && *cc != ']' && nr < 8; cc++)
            if (*cc == '"') { const char *e = strchr(cc + 1, '"'); if (!e) break;
                              int n2 = (int)(e - cc - 1); if (n2 > 7) n2 = 7;
                              memcpy(rows[nr], cc + 1, (size_t)n2); rows[nr][n2] = 0; nr++; cc = e; }
        const char *cl = strstr(js, "\"cols\"");
        if (cl) for (const char *cc = strchr(cl, '['); cc && *cc && *cc != ']'; cc++) if (*cc == '"') { nc++; cc = strchr(cc + 1, '"'); if (!cc) break; }
        const char *fr = strstr(js, "\"frame\"");
        if (fr) sscanf(strchr(fr, '[') + 1, "%d , %d", &fw, &fh);
        bool mirror = strstr(js, "\"mirror_side\"") && strstr(strstr(js, "\"mirror_side\""), "true");
        if (nr > 0) {
            a->nrows = nr;
            for (int i = 0; i < 4; i++) { a->row_of[i] = 0; a->flip[i] = false; }
            for (int i = 0; i < nr; i++) {
                char rc = rows[i][0];
                if (rc == 'S') a->row_of[0] = i;
                else if (rc == 'W') { a->row_of[1] = i; if (mirror) { a->row_of[2] = i; a->flip[2] = true; } }
                else if (rc == 'E') { a->row_of[2] = i; a->flip[2] = false; }
                else if (rc == 'N') a->row_of[3] = i;
            }
        }
        if (nc > 0) { a->ncols = nc; a->col_stand = 0; a->col_a = 1; a->col_b = nc > 2 ? 2 : 1; }
        if (fw > 0 && fh > 0) { a->fw = fw; a->fh = fh; }
        else { a->fw = w / (a->ncols > 0 ? a->ncols : 1); a->fh = h / (a->nrows > 0 ? a->nrows : 1); }
        SDL_free(js);
    }
    SDL_Log("voxfield: walker \"%s\" %dx%d, %dx%d grid, frame %dx%d", id, w, h, a->ncols, a->nrows, a->fw, a->fh);
    return v->art_count++;
}

static void vx_load_atlas(VoxField *v) {
    if (v->atlas) return;
    char rel[160];
    snprintf(rel, sizeof(rel), "field/tilesets/%s/atlas.png", v->set[0] ? v->set : "valley");
    size_t sz = 0;
    void *data = vx_read(rel, &sz);
    if (!data) { SDL_Log("voxfield: no %s — props will not be drawn", rel); return; }
    int w = 0, h = 0, c = 0;
    unsigned char *px = stbi_load_from_memory((const unsigned char *)data, (int)sz, &w, &h, &c, 4);
    SDL_free(data);
    if (!px) return;
    v->atlas_cell = 128;
    char jrel[160];
    snprintf(jrel, sizeof(jrel), "field/tilesets/%s/atlas.json", v->set[0] ? v->set : "valley");
    size_t jsz = 0;
    char *js = (char *)vx_read(jrel, &jsz);
    if (js) { const char *k = strstr(js, "\"cell\""); int n = 0; if (k && sscanf(k + 6, " : %d", &n) == 1 && n > 0) v->atlas_cell = n; SDL_free(js); }
    while (vx_max_tex > 0 && (w > vx_max_tex || h > vx_max_tex)) {   // halve rather than fail to upload
        int nw = w / 2, nh = h / 2;
        unsigned char *sm = (unsigned char *)malloc((size_t)nw * nh * 4);
        for (int y = 0; y < nh; y++) for (int x = 0; x < nw; x++) for (int ch2 = 0; ch2 < 4; ch2++)
            sm[(y * nw + x) * 4 + ch2] = px[((y * 2) * w + x * 2) * 4 + ch2];
        stbi_image_free(px); px = sm; w = nw; h = nh; v->atlas_cell /= 2;
        SDL_Log("voxfield: atlas over GL_MAX_TEXTURE_SIZE — halved to %dx%d", w, h);
    }
    v->atlas = vx_tex_indexed(v, "atlas", px, w, h);
    v->atlas_w = w; v->atlas_h = h;
    if (px) { if (v->atlas) stbi_image_free(px); }
}

static void vx_load_decals(VoxField *v) {
    if (v->decal_count) return;
    static const char *IDS[] = { "tuft", "tuft_tall", "clover", "flower_white", "flower_red",
                                 "daisy_patch", "grass_sprout", "pebble", "pebbles", "mushroom" };
    for (size_t i = 0; i < sizeof(IDS) / sizeof(IDS[0]) && v->decal_count < 24; i++) {
        char rel[192];
        snprintf(rel, sizeof(rel), "field/tilesets/%s/decals/%s.png", v->set[0] ? v->set : "valley", IDS[i]);
        size_t sz = 0;
        void *data = vx_read(rel, &sz);
        if (!data) continue;
        int w = 0, h = 0, c = 0;
        unsigned char *px = stbi_load_from_memory((const unsigned char *)data, (int)sz, &w, &h, &c, 4);
        SDL_free(data);
        if (!px) continue;
        int k = v->decal_count++;
        char label[64];
        snprintf(label, sizeof(label), "decal %s", IDS[i]);
        v->decal_tex[k] = vx_tex_indexed(v, label, px, w, h);
        v->decal_w[k] = w; v->decal_h[k] = h;
        snprintf(v->decal_id[k], 24, "%s", IDS[i]);
        stbi_image_free(px);
    }
    SDL_Log("voxfield: %d detail sprites loaded", v->decal_count);
}

// Where the detail billboards stand: hashed over the grass, never on a path, never under anything.
static void vx_scatter_detail(VoxField *v) {
    v->det_count = 0;
    if (!v->decal_count) return;
    for (int z = 0; z < v->md; z++) for (int x = 0; x < v->mw; x++) {
        if (!v->walk[z][x] || v->place_at[z][x]) continue;
        float y = vx_gy(v, x, z);
        unsigned char top = get_blk(v, x * VX_VPC, v->hgt[z][x], z * VX_VPC);
        if (top != B_GRASS && top != B_GRASS_DRY) continue;
        for (int k = 0; k < 2; k++) {
            uint32_t r = vx_h32(x, z, k * 977);
            if ((r & 255) > 46) continue;                   // sparse: about one in six cells
            if (v->det_count >= 3072) return;
            int d = (int)(vx_rnd(x, z, 31 + k) * v->decal_count);
            if (d >= v->decal_count) d = v->decal_count - 1;
            v->det[v->det_count].x = x + 0.18f + vx_rnd(x, z, 51 + k) * 0.64f;
            v->det[v->det_count].z = z + 0.18f + vx_rnd(x, z, 71 + k) * 0.64f;
            v->det[v->det_count].y = y;
            v->det[v->det_count].d = (unsigned char)d;
            v->det[v->det_count].size = (unsigned char)(28 + (int)(vx_rnd(x, z, 91 + k) * 22));
            v->det_count++;
        }
    }
}

// ───────────────────────── load ─────────────────────────

static void vx_place_party(VoxField *v, int x, int z, int facing) {
    if (!v->noclip && !vx_can_stand(v, x, z)) {
        int bx = x, bz = z, best = 1 << 30;
        for (int sz2 = 0; sz2 < v->md; sz2++) for (int sx = 0; sx < v->mw; sx++) {
            if (!vx_can_stand(v, sx, sz2)) continue;
            int d = (sx - x) * (sx - x) + (sz2 - z) * (sz2 - z);
            if (d < best) { best = d; bx = sx; bz = sz2; }
        }
        SDL_Log("voxfield: spawn %d,%d is not standable — moved to %d,%d", x, z, bx, bz);
        x = bx; z = bz;
    }
    for (int i = 0; i < VX_PARTY; i++) {
        v->act[i].tx = v->act[i].px = (short)x;
        v->act[i].tz = v->act[i].pz = (short)z;
        v->act[i].facing = facing;
        v->act[i].y = v->act[i].y0 = vx_gy(v, x, z);
    }
    v->moving = false; v->move_t = 0;
}

static const char *PARTY_ART[VX_PARTY] = { "falke", "ottilie", "party_c", "party_d" };

bool vx_load_map(VoxField *v, const char *name) {
    if (!v->gl_ready) vx_gl_init(v);
    char clean[32];
    snprintf(clean, sizeof(clean), "%s", name && name[0] ? name : "halm");
    for (char *c = clean; *c; c++) if (!((*c >= 'a' && *c <= 'z') || (*c >= '0' && *c <= '9') || *c == '_')) { *c = 0; break; }
    char rel[128];
    snprintf(rel, sizeof(rel), "field/tmaps/%s.tmap", clean);
    size_t sz = 0;
    char *text = (char *)vx_read(rel, &sz);

    v->place_count = 0; v->trig_count = 0; v->npc_count = 0; v->light_count = 0;
    memset(v->place_at, 0, sizeof(v->place_at));
    memset(v->tsolid, 0, sizeof(v->tsolid));
    memset(v->walk, 0, sizeof(v->walk));
    memset(v->hgt, 0, sizeof(v->hgt));
    for (int z = 0; z < VX_MAXD; z++) for (int x = 0; x < VX_MAXW; x++) v->ground[z][x] = -1;
    v->mw = 24; v->md = 16;
    v->spawn_x = 4; v->spawn_z = 4; v->spawn_f = 0;
    v->amb = 1.0f; v->light_table = 0;
    snprintf(v->map_name, sizeof(v->map_name), "%s", clean);

    if (text) { vx_parse_tmap(v, text); SDL_free(text); }
    else SDL_Log("voxfield: no %s — using a bare fallback map", rel);
    if (!v->def_count) vx_load_tileset(v, "valley");

    vx_build_world(v);
    v->reach_missing = vx_verify_reach(v);
    vx_load_atlas(v);
    vx_load_decals(v);
    vx_scatter_detail(v);
    vx_mesh_all(v);
    vx_sun_defaults(v);
    for (int i = 0; i < VX_PARTY; i++) v->art_party[i] = vx_art_get(v, PARTY_ART[i]);
    for (int i = 0; i < v->npc_count; i++) {
        VxNpc *np = &v->npcs[i];
        np->art = vx_art_get(v, np->walker);
        np->y = np->py_ = vx_gy(v, np->tx, np->tz);
    }
    vx_place_party(v, v->spawn_x, v->spawn_z, v->spawn_f);
    v->steps = 0;
    v->msg[0] = 0; v->msg_who[0] = 0;
    v->selfchecked = false;
    return true;
}

// One line, the first time a map is drawn: everything a reader needs to know whether the world is
// right, without a screenshot. Printed from the tick, once GL has actually drawn a frame.
static void vx_render_shadow(VoxField *v);

static void vx_selfcheck(VoxField *v, int dw, int dh, int draws, int sprites) {
    int chunks = 0;
    for (int i = 0; i < VX_CHUNKS; i++) if (v->chunks[i].verts) chunks++;
    SDL_Log("SELFCHECK vox: map=%s %dx%d cells (%dx%d voxels)  chunks=%d/%d  tris=%d  draws=%d  sprites=%d  "
            "shaped=%d  houses=%d  ramps=%d  reach=%s  mesh=%.1fms  shadow=%dpx/%.1fms  fbo=%dx%d  "
            "maxtex=%d  lamps=%d  npcs=%d  detail=%d  off-palette=%ld",
            v->map_name, v->mw, v->md, v->vw, v->vd, v->chunks_drawn, chunks, v->tris, draws, sprites,
            v->shaped, v->houses, v->ramps,
            v->reach_missing ? "FAIL" : "ok", v->mesh_ms, v->shadow_dim, v->shadow_ms, dw, dh, vx_max_tex,
            v->light_count, v->npc_count, v->det_count, v->off_pal);
    if (v->reach_missing) SDL_Log("SELFCHECK vox: %d cells the tile map can reach are unreachable here", v->reach_missing);
}

// ───────────────────────── walking ─────────────────────────

static int npc_at(VoxField *v, int x, int z) {
    for (int i = 0; i < v->npc_count; i++) if (v->npcs[i].tx == x && v->npcs[i].tz == z) return i;
    return -1;
}

static void vx_say(VoxField *v, const char *who, const char *id) {
    const char *text = nullptr, *name = nullptr;
    for (int i = 0; i < FIELD_TEXT_COUNT; i++)
        if (!strcmp(FIELD_TEXT[i].id, id)) { text = FIELD_TEXT[i].text; name = FIELD_TEXT[i].name; break; }
    if (text) snprintf(v->msg, sizeof(v->msg), "%s", text);
    else snprintf(v->msg, sizeof(v->msg), "[%s]", id);
    const char *speaker = (name && name[0]) ? name : who;
    snprintf(v->msg_who, sizeof(v->msg_who), "%s", speaker ? speaker : "");
    v->msg_t = 0; v->msg_page = 0; v->msg_typing = true;
}

void vx_message(VoxField *v, const char *text) {
    snprintf(v->msg, sizeof(v->msg), "%s", text ? text : "");
    v->msg_who[0] = 0; v->msg_t = 0; v->msg_page = 0; v->msg_typing = true;
}

static void fire(VxEvent *ev, int kind, const char *arg) {
    if (ev->kind != VXE_NONE) return;
    ev->kind = kind;
    snprintf(ev->arg, sizeof(ev->arg), "%s", arg);
}

static int trig_at(VoxField *v, int x, int z, int ka, int kb) {
    for (int i = 0; i < v->trig_count; i++) {
        VxTrig *g = &v->trigs[i];
        if (x < g->x || z < g->z || x >= g->x + g->w || z >= g->z + g->d) continue;
        if (g->kind == ka || g->kind == kb) return i;
    }
    return -1;
}

static void start_map_change(VoxField *v, const char *map, int x, int z, int f) {
    snprintf(v->to_map, sizeof(v->to_map), "%s", map);
    v->to_x = x; v->to_z = z; v->to_f = f;
    v->fade = 1; v->fade_dir = 1;
}

static void on_enter_cell(VoxField *v, VxEvent *ev) {
    v->steps++;
    int x = v->act[0].tx, z = v->act[0].tz;
    for (int i = 0; i < v->trig_count; i++) {
        VxTrig *g = &v->trigs[i];
        bool in = x >= g->x && z >= g->z && x < g->x + g->w && z < g->z + g->d;
        bool was = g->inside;
        g->inside = in;
        if (!in || was) continue;
        if (g->kind == TG_EXIT) { start_map_change(v, g->map, g->ax, g->az, g->af); return; }
        if (g->kind == TG_DOOR && v->act[0].facing == 3) { start_map_change(v, g->map, g->ax, g->az, g->af); return; }
        if (g->kind == TG_ZONE) fire(ev, VXE_ZONE, g->arg);
        if (g->kind == TG_TRAP) vx_say(v, nullptr, g->arg);
        if (g->kind == TG_SCENE) fire(ev, VXE_SCENE, g->arg);
    }
}

static void npc_step(VoxField *v, float dt) {
    for (int i = 0; i < v->npc_count; i++) {
        VxNpc *np = &v->npcs[i];
        if (np->px != np->tx || np->pz != np->tz) {
            np->t += dt;
            if (np->t >= 0.3f) { np->px = np->tx; np->pz = np->tz; np->t = 0.3f; np->py_ = np->y; }
            continue;
        }
        if (!np->wander) continue;
        np->wait -= dt;
        if (np->wait > 0) continue;
        np->wait = 1.0f + vx_rnd((int)(v->anim_t * 60) + i, i, 17) * 2.0f;
        int d = (int)(vx_rnd((int)(v->anim_t * 97) + i, 5, 3) * 4) & 3;
        int nx = np->tx + DX[d], nz = np->tz + DZ[d];
        np->facing = d;
        if (abs(nx - np->hx) > np->wander || abs(nz - np->hz) > np->wander) continue;
        if (nx < 0 || nz < 0 || nx >= v->mw || nz >= v->md) continue;
        if (v->tsolid[nz][nx] || !v->walk[nz][nx]) continue;
        if (abs((int)v->hgt[nz][nx] - (int)v->hgt[np->tz][np->tx]) > 1) continue;
        if (trig_at(v, nx, nz, TG_EXIT, TG_DOOR) >= 0 || trig_at(v, nx, nz, TG_MESSAGE, TG_ZONE) >= 0) continue;
        bool blocked = false;
        for (int p = 0; p < v->party && !blocked; p++) if (v->act[p].tx == nx && v->act[p].tz == nz) blocked = true;
        if (blocked) continue;
        v->walk[np->tz][np->tx] = 1;
        np->px = np->tx; np->pz = np->tz; np->py_ = np->y;
        np->tx = (short)nx; np->tz = (short)nz;
        np->y = vx_gy(v, nx, nz);
        np->t = 0; np->parity ^= 1;
        v->walk[nz][nx] = 0;
    }
}

static bool vx_blocked(VoxField *v, int fx, int fz, int tx, int tz) {
    if (v->noclip) return false;
    if (!vx_can_step(v, fx, fz, tx, tz)) return true;
    return npc_at(v, tx, tz) >= 0;
}

// ───────────────────────── input (the tile field's, unchanged in feel) ─────────────────────────

struct VxTouch { SDL_FingerID id; float x, y; };

static int vx_touches(VxTouch *out, int max, int w, int h) {
    int n = 0, nd = 0;
    SDL_TouchID *devs = SDL_GetTouchDevices(&nd);
    if (devs) {
        for (int d = 0; d < nd && n < max; d++) {
            int nf = 0;
            SDL_Finger **fg = SDL_GetTouchFingers(devs[d], &nf);
            if (!fg) continue;
            for (int i = 0; i < nf && n < max; i++) { out[n].id = fg[i]->id; out[n].x = fg[i]->x * w; out[n].y = fg[i]->y * h; n++; }
            SDL_free(fg);
        }
        SDL_free(devs);
        if (nd > 0) return n;
    }
    if (ImGui::IsMouseDown(0)) { out[0].id = 1; out[0].x = ImGui::GetIO().MousePos.x; out[0].y = ImGui::GetIO().MousePos.y; return 1; }
    return 0;
}

static void vx_input(VoxField *v, int w, int h, bool blocked, float dt) {
    VxTouch tt[8];
    int n = blocked ? 0 : vx_touches(tt, 8, w, h);
    v->tapped = false;
    bool stick_seen = false, act_seen = false;
    for (int i = 0; i < n; i++) {
        if (v->stick_on && tt[i].id == v->stick_id) { stick_seen = true; v->stick_x = tt[i].x; v->stick_y = tt[i].y; }
        if (v->act_on && tt[i].id == v->act_id) act_seen = true;
    }
    if (v->stick_on && !stick_seen) v->stick_on = false;
    if (v->act_on && !act_seen) {
        if (v->act_t <= 0.35f) v->tapped = true;
        v->act_on = false; v->run = false; v->act_t = 0;
    }
    for (int i = 0; i < n; i++) {
        if ((v->stick_on && tt[i].id == v->stick_id) || (v->act_on && tt[i].id == v->act_id)) continue;
        if (tt[i].x < w * 0.5f && !v->stick_on) {
            v->stick_on = true; v->stick_id = tt[i].id;
            v->stick_ox = v->stick_x = tt[i].x; v->stick_oy = v->stick_y = tt[i].y;
        } else if (tt[i].x >= w * 0.5f && !v->act_on) { v->act_on = true; v->act_id = tt[i].id; v->act_t = 0; }
    }
    if (v->act_on) { v->act_t += dt; if (v->act_t > 0.35f) v->run = true; }
    if (!blocked) {
        ImGuiIO &io = ImGui::GetIO();
        if (!io.WantCaptureKeyboard) {
            if (ImGui::IsKeyPressed(ImGuiKey_Space, false) || ImGui::IsKeyPressed(ImGuiKey_Enter, false) || ImGui::IsKeyPressed(ImGuiKey_Z, false)) v->tapped = true;
            if (ImGui::IsKeyDown(ImGuiKey_LeftShift) || ImGui::IsKeyDown(ImGuiKey_RightShift)) v->run = true;
        }
    }
}

static int vx_want_dir(VoxField *v, int w) {
    float dx = 0, dy = 0;
    if (v->stick_on) {
        dx = v->stick_x - v->stick_ox; dy = v->stick_y - v->stick_oy;
        if (dx * dx + dy * dy < (w * 0.024f) * (w * 0.024f)) { dx = dy = 0; }
    }
    ImGuiIO &io = ImGui::GetIO();
    if (!io.WantCaptureKeyboard) {
        if (ImGui::IsKeyDown(ImGuiKey_LeftArrow) || ImGui::IsKeyDown(ImGuiKey_A)) dx -= 100;
        if (ImGui::IsKeyDown(ImGuiKey_RightArrow) || ImGui::IsKeyDown(ImGuiKey_D)) dx += 100;
        if (ImGui::IsKeyDown(ImGuiKey_UpArrow) || ImGui::IsKeyDown(ImGuiKey_W)) dy -= 100;
        if (ImGui::IsKeyDown(ImGuiKey_DownArrow) || ImGui::IsKeyDown(ImGuiKey_S)) dy += 100;
    }
    if (dx == 0 && dy == 0) { v->last_axis = -1; return -1; }
    int axis;
    float ax = fabsf(dx), ay = fabsf(dy);
    if (ax > ay * 1.35f) axis = 0;
    else if (ay > ax * 1.35f) axis = 1;
    else axis = v->last_axis >= 0 ? v->last_axis : (ax >= ay ? 0 : 1);
    v->last_axis = axis;
    return axis == 0 ? (dx < 0 ? 1 : 2) : (dy < 0 ? 3 : 0);
}

static void vx_walk(VoxField *v, int w, float dt, VxEvent *ev) {
    if (v->moving) {
        v->move_t += dt;
        if (v->move_t < v->step_len) return;
        v->moving = false; v->move_t = 0;
        for (int i = 0; i < VX_PARTY; i++) {
            v->act[i].px = v->act[i].tx; v->act[i].pz = v->act[i].tz;
            v->act[i].y0 = v->act[i].y;
        }
        on_enter_cell(v, ev);
        if (v->fade_dir) return;
    }
    if (v->msg[0] || v->fade_dir) return;
    int dir = vx_want_dir(v, w);
    if (dir < 0) { v->hold_t = 0; v->want_dir = -1; return; }
    if (dir != v->want_dir) { v->want_dir = dir; v->hold_t = 0; v->act[0].facing = dir; return; }
    v->hold_t += dt;
    if (v->hold_t < TURN_HOLD) return;
    v->act[0].facing = dir;
    int nx = v->act[0].tx + DX[dir], nz = v->act[0].tz + DZ[dir];
    if (vx_blocked(v, v->act[0].tx, v->act[0].tz, nx, nz)) return;
    short ox[VX_PARTY], oz[VX_PARTY];
    float oy[VX_PARTY];
    for (int i = 0; i < VX_PARTY; i++) { ox[i] = v->act[i].tx; oz[i] = v->act[i].tz; oy[i] = v->act[i].y; }
    v->act[0].px = ox[0]; v->act[0].pz = oz[0]; v->act[0].y0 = oy[0];
    v->act[0].tx = (short)nx; v->act[0].tz = (short)nz;
    v->act[0].y = vx_gy(v, nx, nz);
    for (int i = 1; i < VX_PARTY; i++) {
        v->act[i].px = ox[i]; v->act[i].pz = oz[i]; v->act[i].y0 = oy[i];
        v->act[i].tx = ox[i - 1]; v->act[i].tz = oz[i - 1];
        v->act[i].y = vx_gy(v, v->act[i].tx, v->act[i].tz);
        if (v->act[i].tx != v->act[i].px || v->act[i].tz != v->act[i].pz) {
            int ddx = v->act[i].tx - v->act[i].px, ddz = v->act[i].tz - v->act[i].pz;
            v->act[i].facing = ddx < 0 ? 1 : ddx > 0 ? 2 : ddz < 0 ? 3 : 0;
        }
    }
    v->moving = true; v->move_t = 0;
    v->step_len = v->run ? RUN_STEP : WALK_STEP;
    v->step_parity ^= 1;
}

// ───────────────────────── billboards ─────────────────────────

static int walk_col(const VxArt *a, int parity, float k, bool moving) {
    if (!moving) return a->col_stand;
    if (a->ncols >= 4) { int c = parity ? (k < 0.5f ? 3 : 0) : (k < 0.5f ? 1 : 2); return c; }
    int c = parity ? a->col_b : a->col_a;
    return k < 0.6f ? c : a->col_stand;
}

static void spr_push(VoxField *v, const VxSpr *s) {
    if (v->spr_count < VX_SPRITES) v->spr[v->spr_count++] = *s;
}

// Where a walker's feet are, mid-step. On level ground or a one-voxel step it is a straight lerp; a
// two-voxel step keeps the little hop it always had; a RAMP is read straight off the slope's own
// plane at the fractional position, which is what makes climbing one smooth.
static float vx_actor_y(VoxField *v, const VxActor *a, float x, float z, float k) {
    bool on_ramp = (v->ramp[a->pz][a->px] || v->ramp[a->tz][a->tx]);
    if (on_ramp) {
        int cx = (int)floorf(x), cz = (int)floorf(z);
        if (cx < 0) cx = 0; if (cz < 0) cz = 0;
        if (cx >= v->mw) cx = v->mw - 1;
        if (cz >= v->md) cz = v->md - 1;
        return vx_surface_v(v, cx, cz, x - cx, z - cz) * VOX_S;
    }
    float y = vx_lerp(a->y0, a->y, k);
    float d = a->y - a->y0;
    if (d < 0) d = -d;
    if (d > VOX_S * 1.5f) y += sinf(k * 3.14159f) * 0.18f;        // the hop over a two-voxel lip
    return y;
}

static void vx_light_at_foot(VoxField *v, float x, float y, float z, float *lit, float *warm) {
    float lamp = 0;
    vx_lamp_at(v, x, y + 0.8f, z, &lamp);
    *lit = 0.92f;                      // a body is lit by the sky, not by the face it stands on
    *warm = lamp;
}

static void push_walker(VoxField *v, int art, float x, float y, float z, int facing, int col) {
    if (art < 0 || art >= v->art_count) return;
    VxArt *a = &v->art[art];
    int row = a->row_of[facing & 3];
    bool flip = a->flip[facing & 3];
    float fw = 1.0f / (float)(a->ncols > 0 ? a->ncols : 1), fh = 1.0f / (float)(a->nrows > 0 ? a->nrows : 1);
    if (col >= a->ncols) col = a->ncols - 1;
    VxSpr s;
    memset(&s, 0, sizeof(s));
    s.x = x; s.y = y; s.z = z;
    s.h = 1.6f;                                         // a door is 2 blocks; the party is 1.6
    s.w = s.h * (float)a->fw / (float)(a->fh > 0 ? a->fh : 1);
    s.u0 = flip ? (col + 1) * fw : col * fw;
    s.u1 = flip ? col * fw : (col + 1) * fw;
    s.v0 = row * fh; s.v1 = (row + 1) * fh;
    s.tex = a->tex; s.kind = 0; s.alpha = 1; s.tilt = 1;
    vx_light_at_foot(v, x, y, z, &s.lit, &s.warm);
    spr_push(v, &s);
    VxSpr sh;                                            // the blob shadow, flat on the ground
    memset(&sh, 0, sizeof(sh));
    sh.x = x; sh.y = y + 0.02f; sh.z = z;
    sh.w = s.w * 0.85f; sh.h = s.w * 0.62f;
    sh.kind = 1; sh.alpha = 0.42f; sh.tilt = 0;
    spr_push(v, &sh);
}

static void vx_collect_sprites(VoxField *v) {
    v->spr_count = 0;
    float k = v->moving ? v->move_t / v->step_len : 1.0f;
    for (int i = v->party - 1; i >= 0; i--) {
        VxActor *a = &v->act[i];
        float x = vx_lerp(a->px + 0.5f, a->tx + 0.5f, k);
        float z = vx_lerp(a->pz + 0.5f, a->tz + 0.5f, k);
        float y = vx_actor_y(v, a, x, z, k);
        int art = v->art_party[i];
        int col = art >= 0 ? walk_col(&v->art[art], v->step_parity ^ (i & 1), k, v->moving) : 0;
        push_walker(v, art, x, y, z, a->facing, col);
    }
    for (int i = 0; i < v->npc_count; i++) {
        VxNpc *np = &v->npcs[i];
        float nk = np->t >= 0.3f ? 1.0f : np->t / 0.3f;
        bool mv = (np->px != np->tx || np->pz != np->tz);
        float x = vx_lerp(np->px + 0.5f, np->tx + 0.5f, mv ? nk : 1.0f);
        float z = vx_lerp(np->pz + 0.5f, np->tz + 0.5f, mv ? nk : 1.0f);
        float y = vx_lerp(np->py_, np->y, mv ? nk : 1.0f);
        int col = np->art >= 0 ? walk_col(&v->art[np->art], np->parity, nk, mv) : 0;
        push_walker(v, np->art, x, y, z, np->facing, col);
    }
    // Props: any stamp nobody has modelled, as a billboard of its own atlas cells standing on the map.
    if (v->atlas && v->atlas_cell > 0) {
        float aw = (float)v->atlas_w, ah = (float)v->atlas_h;
        for (int i = 0; i < v->place_count; i++) {
            VxPlace *p = &v->places[i];
            VxDef *d = &v->defs[p->def];
            if (obj_kind(d->name) || d->index < 0) continue;
            int cx = d->index % 16, cz = d->index / 16;
            float px0 = cx * (float)v->atlas_cell, py0 = cz * (float)v->atlas_cell;
            float pw = d->w * (float)v->atlas_cell, ph = d->h * (float)v->atlas_cell;
            if (px0 + pw > aw || py0 + ph > ah) continue;
            float fx = p->x + d->w * 0.5f, fz = p->z + d->h - 0.5f;
            int hx = p->x < v->mw ? p->x : v->mw - 1, hz = p->z < v->md ? p->z : v->md - 1;
            VxSpr s;
            memset(&s, 0, sizeof(s));
            s.x = fx; s.z = fz; s.y = vx_gy(v, hx, hz);
            s.w = (float)d->w; s.h = (float)d->h * 0.95f;
            s.u0 = px0 / aw; s.v0 = py0 / ah; s.u1 = (px0 + pw) / aw; s.v1 = (py0 + ph) / ah;
            s.tex = v->atlas; s.kind = 0; s.alpha = 1; s.tilt = 1;
            vx_light_at_foot(v, s.x, s.y, s.z, &s.lit, &s.warm);
            spr_push(v, &s);
        }
    }
    // Detail: flowers, tufts and pebbles, hashed over the grass. The density slider is live.
    int want = (int)(v->det_count * (v->detail < 0 ? 0 : v->detail > 1 ? 1 : v->detail));
    for (int i = 0; i < want; i++) {
        int d = v->det[i].d;
        if (d >= v->decal_count) continue;
        VxSpr s;
        memset(&s, 0, sizeof(s));
        s.x = v->det[i].x; s.z = v->det[i].z; s.y = (float)v->det[i].y;
        s.h = v->det[i].size / 100.0f;
        s.w = s.h * (float)v->decal_w[d] / (float)(v->decal_h[d] > 0 ? v->decal_h[d] : 1);
        s.u0 = 0; s.v0 = 0; s.u1 = 1; s.v1 = 1;
        s.tex = v->decal_tex[d]; s.kind = 0; s.alpha = 1; s.tilt = 1;
        vx_light_at_foot(v, s.x, s.y, s.z, &s.lit, &s.warm);
        spr_push(v, &s);
    }
    // Lamps at night: a soft additive glow the bloom picks up. Off in full daylight, where it is noise.
    if (v->amb < 0.92f) {
        for (int i = 0; i < v->light_count; i++) {
            VxLight *l = &v->lights[i];
            float fl = l->flicker ? 0.82f + 0.18f * vx_vnoise(v->anim_t * 7.0f + i * 3.7f, 0.5f, 55u) : 1.0f;
            VxSpr s;
            memset(&s, 0, sizeof(s));
            s.x = l->x; s.z = l->z; s.y = l->y - 0.9f;
            s.w = s.h = l->r * 0.9f;
            s.kind = 2; s.alpha = l->level * fl * (1.0f - v->amb) * 0.85f; s.tilt = 1;
            spr_push(v, &s);
        }
    }
}

// ───────────────────────── render ─────────────────────────

struct VxSprVert { float x, y, z, u, vtex, lit, warm, alpha, kind; };

static int vx_emit_sprites(VoxField *v, VxSprVert *out, int cap, int kind, GLuint tex, int from, int *next) {
    float T = v->pitch * 3.14159265f / 180.0f;
    int n = 0, i = from;
    for (; i < v->spr_count; i++) {
        VxSpr *s = &v->spr[i];
        if (s->kind != kind) continue;
        if (kind == 0 && s->tex != tex) continue;
        if (n + 6 > cap) break;
        float ang = kind == 1 ? 0.0f : (kind == 2 ? T : T * v->tilt);
        float ux, uy, uz, rx = s->w * 0.5f;
        if (kind == 1) { ux = 0; uy = 0; uz = s->h; }                    // flat on the ground
        else { ux = 0; uy = s->h * cosf(ang); uz = -s->h * sinf(ang); }
        float bx = s->x, by = s->y, bz = s->z;
        if (kind == 1) { bz -= s->h * 0.5f; }                            // centre the disc on the feet, flat in XZ
        if (kind == 2) { by -= uy * 0.5f; bz -= uz * 0.5f; bx -= 0; }    // centre the glow
        float p[4][3];
        p[0][0] = bx - rx;      p[0][1] = by;      p[0][2] = bz;
        p[1][0] = bx + rx;      p[1][1] = by;      p[1][2] = bz;
        p[2][0] = bx + rx + ux; p[2][1] = by + uy; p[2][2] = bz + uz;
        p[3][0] = bx - rx + ux; p[3][1] = by + uy; p[3][2] = bz + uz;
        float uv[4][2] = { { s->u0, s->v1 }, { s->u1, s->v1 }, { s->u1, s->v0 }, { s->u0, s->v0 } };
        if (kind != 0) { uv[0][0] = 0; uv[0][1] = 1; uv[1][0] = 1; uv[1][1] = 1; uv[2][0] = 1; uv[2][1] = 0; uv[3][0] = 0; uv[3][1] = 0; }
        static const int ORDER[6] = { 0, 1, 2, 0, 2, 3 };
        for (int k = 0; k < 6; k++) {
            int c = ORDER[k];
            VxSprVert *o = &out[n++];
            o->x = p[c][0]; o->y = p[c][1]; o->z = p[c][2];
            o->u = uv[c][0]; o->vtex = uv[c][1];
            o->lit = s->lit; o->warm = s->warm; o->alpha = s->alpha; o->kind = (float)s->kind;
        }
    }
    if (next) *next = i;
    return n;
}

// Upload a whole sprite vertex array in one go and point the attributes at it. Callers then draw
// ranges out of it — one upload a pass, not one an object.
static void vx_sprite_upload(VoxField *v, const VxSprVert *vert, int n) {
    glBindVertexArray(v->spr_vao);
    glBindBuffer(GL_ARRAY_BUFFER, v->spr_vbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof(VxSprVert) * (size_t)n, vert, GL_STREAM_DRAW);
    glEnableVertexAttribArray(0); glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(VxSprVert), (void *)0);
    glEnableVertexAttribArray(1); glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, sizeof(VxSprVert), (void *)12);
    glEnableVertexAttribArray(2); glVertexAttribPointer(2, 4, GL_FLOAT, GL_FALSE, sizeof(VxSprVert), (void *)20);
}

static void vx_draw_sprite_batch(VoxField *v, const VxSprVert *vert, int n) {
    if (n <= 0) return;
    vx_sprite_upload(v, vert, n);
    glDrawArrays(GL_TRIANGLES, 0, n);
    vxp_draw(n / 3);
}

static void vx_camera(VoxField *v, int w, int h, float *proj) {
    float k = v->moving ? v->move_t / v->step_len : 1.0f;
    VxActor *a = &v->act[0];
    float px = vx_lerp(a->px + 0.5f, a->tx + 0.5f, k);
    float pz = vx_lerp(a->pz + 0.5f, a->tz + 0.5f, k);
    float py = vx_lerp(a->y0, a->y, k);
    v->cam_tgt[0] = px; v->cam_tgt[1] = py + 0.9f; v->cam_tgt[2] = pz;
    float p = v->pitch * 3.14159265f / 180.0f;
    float aspect = (float)w / (h > 0 ? (float)h : 1.0f);
    float D;
    if (v->ortho) {
        D = 40.0f;
        mat_ortho(proj, v->view_h * 0.5f * aspect, v->view_h * 0.5f, 0.5f, 140.0f);
    } else {
        D = (v->view_h * 0.5f) / tanf(v->fov * 0.5f * 3.14159265f / 180.0f);
        mat_persp(proj, v->fov, aspect, 0.5f, 200.0f);
    }
    v->cam_eye[0] = v->cam_tgt[0];
    v->cam_eye[1] = v->cam_tgt[1] + D * sinf(p);
    v->cam_eye[2] = v->cam_tgt[2] + D * cosf(p);
    mat_look(v->view, v->cam_eye, v->cam_tgt);
    mat_mul(proj, v->view, v->mvp);
    // Where the party is on screen, and how deep — the cutaway and the tilt-shift both want it.
    float cp[4] = { v->cam_tgt[0], v->cam_tgt[1], v->cam_tgt[2], 1 };
    float cl[4];
    for (int i = 0; i < 4; i++) cl[i] = v->mvp[0 * 4 + i] * cp[0] + v->mvp[1 * 4 + i] * cp[1] + v->mvp[2 * 4 + i] * cp[2] + v->mvp[3 * 4 + i];
    float iw = cl[3] != 0 ? 1.0f / cl[3] : 1.0f;
    v->player_screen[0] = (cl[0] * iw * 0.5f + 0.5f) * w;
    v->player_screen[1] = (cl[1] * iw * 0.5f + 0.5f) * h;
    float vz = v->view[2] * cp[0] + v->view[6] * cp[1] + v->view[10] * cp[2] + v->view[14];
    v->player_screen[2] = -vz;
}

static void vx_set_common(VoxField *v, GLuint prog, float sky[3]) {
    int rowa = v->cmap_row0[v->light_table < v->cmap_tables ? v->light_table : 0];
    int rowb = v->cmap_lamp_row0;
    glUniform1f(vx_uni(prog, "u_amb"), v->amb);
    glUniform1f(vx_uni(prog, "u_levels"), (float)v->cmap_levels);
    glUniform1f(vx_uni(prog, "u_cmaph"), (float)v->cmap_h);
    glUniform1f(vx_uni(prog, "u_rowa"), (float)rowa);
    glUniform1f(vx_uni(prog, "u_rowb"), (float)rowb);
    glUniform1f(vx_uni(prog, "u_fog"), v->fog);
    glUniform1f(vx_uni(prog, "u_fognear"), v->view_h * 1.1f);
    glUniform1f(vx_uni(prog, "u_fogfar"), v->view_h * 4.2f);
    glUniform3f(vx_uni(prog, "u_sky"), sky[0], sky[1], sky[2]);
}

// ───────────────────────── the sun, and its shadow map ─────────────────────────
// The world is static, so its shadow is too: one depth render at map load, and again only when the
// sun moves. The result is a 0..1 factor that feeds the LIGHT LEVEL, not a multiply toward black, so
// shade stays a palette colour (PALETTE.md / D19).

static void vx_sun_defaults(VoxField *v) {
    const char *t = (v->light_table >= 0 && v->light_table < v->cmap_tables) ? v->cmap_tname[v->light_table] : "day";
    if (!strcmp(t, "dusk"))       { v->sun_az = 296; v->sun_el = 16; v->sh_str = 0.95f; v->sh_soft = 3.0f; }
    else if (!strcmp(t, "night")) { v->sun_az = 70;  v->sun_el = 58; v->sh_str = 0.28f; v->sh_soft = 3.4f; }
    else                          { v->sun_az = 312; v->sun_el = 38; v->sh_str = 1.0f;  v->sh_soft = 1.6f; }
    v->shadow_dirty = true;
}

static void vx_sun_matrix(VoxField *v) {
    float az = v->sun_az * 3.14159265f / 180.0f, el = v->sun_el * 3.14159265f / 180.0f;
    v->sundir[0] = cosf(el) * sinf(az);
    v->sundir[1] = sinf(el);
    v->sundir[2] = cosf(el) * cosf(az);
    float cx = v->mw * 0.5f, cz = v->md * 0.5f, cy = 3.0f;
    float R = 0.5f * sqrtf((float)(v->mw * v->mw + v->md * v->md)) + 16.0f;
    float eye[3] = { cx + v->sundir[0] * R * 2.0f, cy + v->sundir[1] * R * 2.0f, cz + v->sundir[2] * R * 2.0f };
    float ctr[3] = { cx, cy, cz };
    float view[16], proj[16];
    mat_look(view, eye, ctr);
    mat_ortho(proj, R, R, 0.5f, R * 4.5f);
    mat_mul(proj, view, v->lmvp);
}

static void vx_render_shadow(VoxField *v) {
    if (!v->gl_ready || !v->built) return;
    uint64_t t0 = SDL_GetTicksNS();
    vxp_begin(VXP_SHADOW);
    vx_sun_matrix(v);
    glBindFramebuffer(GL_FRAMEBUFFER, v->shadow_fbo);
    glViewport(0, 0, v->shadow_dim, v->shadow_dim);
    glClearDepthf(1.0f);
    glClear(GL_DEPTH_BUFFER_BIT);
    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LESS);
    glDepthMask(GL_TRUE);
    glDisable(GL_BLEND);
    // Front faces culled and a slope-scaled offset: between them the small voxels get neither acne
    // nor a shadow that has come unstuck from its object.
    glEnable(GL_CULL_FACE);
    glCullFace(GL_FRONT);
    glEnable(GL_POLYGON_OFFSET_FILL);
    glPolygonOffset(2.4f, 4.0f);
    glUseProgram(v->shadow_prog);
    glUniformMatrix4fv(vx_uni(v->shadow_prog, "u_lmvp"), 1, GL_FALSE, v->lmvp);
    glBindVertexArray(v->vao);
    for (int i = 0; i < VX_CHUNKS; i++) {
        VxChunk *ch = &v->chunks[i];
        if (!ch->verts) continue;
        glBindBuffer(GL_ARRAY_BUFFER, ch->vbo);
        glEnableVertexAttribArray(0); glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(VxVert), (void *)0);
        glDisableVertexAttribArray(1); glDisableVertexAttribArray(2);
        glDisableVertexAttribArray(3); glDisableVertexAttribArray(4);
        glDrawArrays(GL_TRIANGLES, 0, ch->verts);
    }
    glDisable(GL_POLYGON_OFFSET_FILL);
    glCullFace(GL_BACK);
    glBindVertexArray(0);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    vxp_end(VXP_SHADOW);
    v->shadow_dirty = false;
    v->shadow_ms = (double)(SDL_GetTicksNS() - t0) / 1e6;
}

static void vx_set_shadow_uniforms(VoxField *v, GLuint prog, int unit) {
    glUniform1i(vx_uni(prog, "u_shadow"), unit);
    glUniformMatrix4fv(vx_uni(prog, "u_lmvp"), 1, GL_FALSE, v->lmvp);
    glUniform1f(vx_uni(prog, "u_shstr"), v->sh_str);
    glUniform1f(vx_uni(prog, "u_shsoft"), v->sh_soft);
    glUniform1f(vx_uni(prog, "u_shtexel"), 1.0f / (float)v->shadow_dim);
    glUniform3f(vx_uni(prog, "u_sundir"), v->sundir[0], v->sundir[1], v->sundir[2]);
    glActiveTexture((GLenum)(GL_TEXTURE0 + unit));
    glBindTexture(GL_TEXTURE_2D, v->shadow_tex);
    glActiveTexture(GL_TEXTURE0);
}

// Six frustum planes out of the view-projection, for the chunk AABB test.
static void vx_frustum(const float *m, float pl[6][4]) {
    for (int i = 0; i < 3; i++) for (int s = 0; s < 2; s++) {
        int k = i * 2 + s;
        for (int c = 0; c < 4; c++) pl[k][c] = m[c * 4 + 3] + (s ? -m[c * 4 + i] : m[c * 4 + i]);
        float l = sqrtf(pl[k][0]*pl[k][0] + pl[k][1]*pl[k][1] + pl[k][2]*pl[k][2]);
        if (l > 1e-6f) for (int c = 0; c < 4; c++) pl[k][c] /= l;
    }
}
static bool vx_box_visible(const float pl[6][4], const float *lo, const float *hi) {
    for (int k = 0; k < 6; k++) {
        float x = pl[k][0] > 0 ? hi[0] : lo[0];
        float y = pl[k][1] > 0 ? hi[1] : lo[1];
        float z = pl[k][2] > 0 ? hi[2] : lo[2];
        if (pl[k][0]*x + pl[k][1]*y + pl[k][2]*z + pl[k][3] < 0) return false;
    }
    return true;
}

// The sky, drawn AFTER the opaque world and depth-rejected against it.
//
// It used to be drawn first, with the depth test off, which shaded every one of the 2.59 M pixels on
// the phone — two octaves of value noise, eight hash evaluations a pixel — and then had almost all of
// them painted over by the town. The benchmark put that at 2.5 ms of the 12.6 ms frame. Now the quad
// is emitted at the far plane (VX_QUAD_VS writes z = 1.0) with GL_LEQUAL and no depth write, so every
// pixel the world already covers is rejected before the fragment shader runs. The pixels that DO
// survive are exactly the ones that were visible before, shaded by the same code, so nothing moves.
//
// It is drawn before the blended pass (blob shadows, lamp glows) and after the opaque one, which is
// the only order that is also correct: a glow over open sky would be overwritten if the sky came last.
static void vx_draw_sky(VoxField *v, const float *sky) {
    (void)sky;
    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LEQUAL);
    glDepthMask(GL_FALSE);
    glDisable(GL_BLEND);
    glDisable(GL_CULL_FACE);
    glBindVertexArray(v->quad_vao);
    glUseProgram(v->sky_prog);
    glUniform1i(vx_uni(v->sky_prog, "u_cmap"), 1);
    glUniform1f(vx_uni(v->sky_prog, "u_levels"), (float)v->cmap_levels);
    glUniform1f(vx_uni(v->sky_prog, "u_cmaph"), (float)v->cmap_h);
    glUniform1f(vx_uni(v->sky_prog, "u_rowa"), (float)v->cmap_row0[v->light_table < v->cmap_tables ? v->light_table : 0]);
    glUniform1f(vx_uni(v->sky_prog, "u_amb"), v->amb * 0.9f + 0.1f);
    glUniform1f(vx_uni(v->sky_prog, "u_time"), v->anim_t);
    glUniform1f(vx_uni(v->sky_prog, "u_horizon"), 0.30f);
    glUniform1f(vx_uni(v->sky_prog, "u_cloud"), 0.75f * v->q_cloud);
    glUniform1f(vx_uni(v->sky_prog, "u_iz"), 143.0f);
    glUniform1f(vx_uni(v->sky_prog, "u_ih"), 141.0f);
    // the cloud colour is the top of the plaster ramp, looked up by NAME — never a number typed here
    {
        int r = vx_ramp_by_name(v, "neutral warm (plaster, cloth)");
        float ci = 141.0f;
        if (r >= 0 && v->ramp_len[r] > 0) ci = (float)v->ramp_idx[r][v->ramp_len[r] - 1];
        glUniform1f(vx_uni(v->sky_prog, "u_ic"), ci);
    }
    glActiveTexture(GL_TEXTURE1); glBindTexture(GL_TEXTURE_2D, v->cmap);
    glActiveTexture(GL_TEXTURE0);
    vxp_begin(VXP_SKY);
    glDrawArrays(GL_TRIANGLES, 0, 3);
    vxp_end(VXP_SKY);
    vxp_draw(1);
    glBindVertexArray(0);
    glDepthFunc(GL_LESS);
    glDepthMask(GL_TRUE);
}

// Can this chunk hold a fragment the cutaway would discard? The shader's test is
// `v_viewz < u_player.z - 0.6` AND inside a circle of radius `rad` px around u_player.xy, so a chunk
// that is entirely behind the party, or whose screen box misses that circle's box, has no such
// fragment and can be drawn with the discard-free program. The pixels are identical; only early-Z
// changes. Conservative everywhere it is unsure (a corner behind the near plane → yes).
static bool vx_chunk_needs_cut(VoxField *v, const float *lo, const float *hi,
                               int w, int h, float rad) {
    if (rad <= 0.0f) return false;
    float minz = 1e30f, sx0 = 1e30f, sy0 = 1e30f, sx1 = -1e30f, sy1 = -1e30f;
    for (int c = 0; c < 8; c++) {
        float p[3] = { (c & 1) ? hi[0] : lo[0], (c & 2) ? hi[1] : lo[1], (c & 4) ? hi[2] : lo[2] };
        float vz = -(v->view[2] * p[0] + v->view[6] * p[1] + v->view[10] * p[2] + v->view[14]);
        if (vz < minz) minz = vz;
        float cw = v->mvp[3] * p[0] + v->mvp[7] * p[1] + v->mvp[11] * p[2] + v->mvp[15];
        if (cw <= 1e-4f) return true;                       // crosses the near plane — don't guess
        float cx = v->mvp[0] * p[0] + v->mvp[4] * p[1] + v->mvp[8]  * p[2] + v->mvp[12];
        float cy = v->mvp[1] * p[0] + v->mvp[5] * p[1] + v->mvp[9]  * p[2] + v->mvp[13];
        float x = (cx / cw * 0.5f + 0.5f) * (float)w, y = (cy / cw * 0.5f + 0.5f) * (float)h;
        if (x < sx0) sx0 = x; if (x > sx1) sx1 = x;
        if (y < sy0) sy0 = y; if (y > sy1) sy1 = y;
    }
    if (minz >= v->player_screen[2] - 0.6f) return false;   // all of it is at or behind the party
    float px = v->player_screen[0], py = v->player_screen[1];
    return !(sx1 < px - rad || sx0 > px + rad || sy1 < py - rad || sy0 > py + rad);
}

// Both world variants take the same uniforms; this sets them on whichever is handed in and leaves
// it bound. Only the variant actually used pays for the calls.
static void vx_set_world_uniforms(VoxField *v, GLuint prog, float sky[3], float cut_rad) {
    glUseProgram(prog);
    glUniformMatrix4fv(vx_uni(prog, "u_mvp"), 1, GL_FALSE, v->mvp);
    glUniformMatrix4fv(vx_uni(prog, "u_view"), 1, GL_FALSE, v->view);
    glUniform1i(vx_uni(prog, "u_lut"), 0);
    glUniform1i(vx_uni(prog, "u_cmap"), 1);
    glUniform1f(vx_uni(prog, "u_ao"), v->ao_str);
    glUniform1f(vx_uni(prog, "u_time"), v->anim_t);
    glUniform4f(vx_uni(prog, "u_player"), v->player_screen[0], v->player_screen[1],
                v->player_screen[2], cut_rad);
    glUniform1f(vx_uni(prog, "u_snap"), v->sh_snap ? 1.0f : 0.0f);
    glUniform1f(vx_uni(prog, "u_noff"), 0.055f);
    glUniform1f(vx_uni(prog, "u_qpattern"), v->q_pattern);
    glUniform1f(vx_uni(prog, "u_qpcf"), v->q_pcf);
    glUniform1f(vx_uni(prog, "u_qwater"), v->q_water);
    vx_set_common(v, prog, sky);
    vx_set_shadow_uniforms(v, prog, 2);
}

static void vx_render(VoxField *v, int w, int h) {
    float proj[16];
    vx_camera(v, w, h, proj);
    if (v->shadow_dirty) vx_render_shadow(v);
    // The sky and the fog take their colour from the palette, through the same colormap as everything
    // else, so a night sky is the night table's idea of that blue and not a number typed in here.
    float sky[3];
    vx_cpu_colour(v, 141, v->amb * 0.9f + 0.1f, sky);

    glBindFramebuffer(GL_FRAMEBUFFER, v->fbo);
    glViewport(0, 0, w, h);
    glClearColor(sky[0], sky[1], sky[2], 1.0f);
    glClearDepthf(1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LESS);
    glDepthMask(GL_TRUE);
    glDisable(GL_BLEND);
    glEnable(GL_CULL_FACE);
    glCullFace(GL_BACK);
    glFrontFace(GL_CCW);

    float cut_rad = v->cutaway ? (float)h * 0.16f : 0.0f;
    vx_set_world_uniforms(v, v->prog, sky, cut_rad);
    glActiveTexture(GL_TEXTURE0); glBindTexture(GL_TEXTURE_2D, v->lut);
    glActiveTexture(GL_TEXTURE1); glBindTexture(GL_TEXTURE_2D, v->cmap);
    glActiveTexture(GL_TEXTURE0);

    float planes[6][4];
    vx_frustum(v->mvp, planes);
    glBindVertexArray(v->vao);
    int draws = 0;
    v->chunks_drawn = 0;
    vxp_begin(VXP_WORLD);

    // ── the visible chunks, front to back, and only the ones that need it under the cutaway ──
    // Front to back is what lets early-Z do its work at all: a back-to-front order shades every
    // hidden pixel first and then paints over it. The sort is over at most VX_CHUNKS boxes, which is
    // nothing; the win is in the fragments it never has to shade.
    struct VxDrawChunk { VxChunk *ch; float key; bool cut; };
    VxDrawChunk order[VX_CHUNKS];
    int nord = 0, chunks_total = 0, cut_n = 0;
    for (int i = 0; i < VX_CHUNKS; i++) {
        VxChunk *ch = &v->chunks[i];
        if (!ch->verts) continue;
        chunks_total++;
        if (!vx_box_visible(planes, ch->lo, ch->hi)) continue;
        float cx = (ch->lo[0] + ch->hi[0]) * 0.5f, cy = (ch->lo[1] + ch->hi[1]) * 0.5f,
              cz = (ch->lo[2] + ch->hi[2]) * 0.5f;
        order[nord].ch = ch;
        order[nord].key = -(v->view[2] * cx + v->view[6] * cy + v->view[10] * cz + v->view[14]);
        order[nord].cut = vx_chunk_needs_cut(v, ch->lo, ch->hi, w, h, cut_rad);
        if (order[nord].cut) cut_n++;
        nord++;
    }
    // VOX_CHUNKSORT=0 leaves the chunks in index order and =-1 draws them back to front; both exist
    // only so the overdraw measurement can say what the sort is worth. The shipping path is 1.
    static int sort_dir = 2;
    if (sort_dir == 2) { const char *e = SDL_getenv("VOX_CHUNKSORT"); sort_dir = e && e[0] ? atoi(e) : 1; }
    if (sort_dir) for (int i = 1; i < nord; i++) {          // insertion sort, nearest first
        VxDrawChunk t = order[i]; int j = i - 1;
        while (j >= 0 && (sort_dir > 0 ? order[j].key > t.key : order[j].key < t.key)) { order[j + 1] = order[j]; j--; }
        order[j + 1] = t;
    }
    v->od_chunks_cut = cut_n;
    v->od_cut_frac = nord ? (double)cut_n / (double)nord : 0.0;
    v->od_nord = nord;
    for (int i = 0; i < nord; i++) v->od_order[i] = order[i].ch;
    bool cut_ready = false;
    GLuint cur = v->prog;
    for (int i = 0; i < nord; i++) {
        VxChunk *ch = order[i].ch;
        GLuint want = order[i].cut ? v->prog_cut : v->prog;
        if (want != cur) {
            if (want == v->prog_cut && !cut_ready) { vx_set_world_uniforms(v, v->prog_cut, sky, cut_rad); cut_ready = true; }
            else glUseProgram(want);
            cur = want;
        }
        v->chunks_drawn++;
        vxp_draw(ch->verts / 3);
        glBindBuffer(GL_ARRAY_BUFFER, ch->vbo);
        glEnableVertexAttribArray(0); glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(VxVert), (void *)0);
        glEnableVertexAttribArray(1); glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, sizeof(VxVert), (void *)12);
        glEnableVertexAttribArray(2); glVertexAttribIPointer(2, 4, GL_UNSIGNED_BYTE, sizeof(VxVert), (void *)20);
        glEnableVertexAttribArray(3); glVertexAttribPointer(3, 2, GL_UNSIGNED_BYTE, GL_TRUE, sizeof(VxVert), (void *)24);
        glEnableVertexAttribArray(4); glVertexAttribPointer(4, 3, GL_BYTE, GL_TRUE, sizeof(VxVert), (void *)28);
        glDrawArrays(GL_TRIANGLES, 0, ch->verts);
        draws++;
    }
    if (cur != v->prog) glUseProgram(v->prog);
    vxp_end(VXP_WORLD);
    vxp.c.chunks_drawn = v->chunks_drawn;
    vxp.c.chunks_total = chunks_total;

    // Billboards. Alpha-tested with depth write, so they sort against the blocks for nothing.
    vx_collect_sprites(v);
    vxp_begin(VXP_SPRITES);
    static VxSprVert *sv = nullptr;
    static int sv_cap = 0;
    if (sv_cap < VX_SPRITES * 6) { sv_cap = VX_SPRITES * 6; sv = (VxSprVert *)realloc(sv, sizeof(VxSprVert) * (size_t)sv_cap); }
    glUseProgram(v->spr_prog);
    glUniformMatrix4fv(vx_uni(v->spr_prog, "u_mvp"), 1, GL_FALSE, v->mvp);
    glUniformMatrix4fv(vx_uni(v->spr_prog, "u_view"), 1, GL_FALSE, v->view);
    glUniform1i(vx_uni(v->spr_prog, "u_tex"), 0);
    glUniform1i(vx_uni(v->spr_prog, "u_cmap"), 1);
    glUniform1f(vx_uni(v->spr_prog, "u_indexed"), v->pal_ok ? 1.0f : 0.0f);
    vx_set_common(v, v->spr_prog, sky);
    vx_set_shadow_uniforms(v, v->spr_prog, 2);
    // One batch per texture, in the order they were pushed — but ONE buffer upload for all of them.
    // This used to call glBufferData once per texture (a walker sheet each, the atlas, and ten
    // decals = fourteen orphan-and-upload round trips a frame). Every one of those is a driver
    // allocation the GPU may still be reading from, which is exactly the kind of call that blocks on
    // a tile-based mobile driver. Now the whole kind-0 vertex array is built first, uploaded once,
    // and drawn with fourteen glDrawArrays offsets into it.
    struct VxSprBatch { GLuint tex; int first, count; };
    VxSprBatch batch[64];
    int nb = 0, total = 0;
    GLuint done[32];
    int done_n = 0;
    for (int i = 0; i < v->spr_count; i++) {
        if (v->spr[i].kind != 0) continue;
        GLuint tex = v->spr[i].tex;
        bool seen = false;
        for (int k = 0; k < done_n; k++) if (done[k] == tex) seen = true;
        if (seen || done_n >= 32) continue;
        done[done_n++] = tex;
        int from = 0, n;
        while (nb < 64 && (n = vx_emit_sprites(v, sv + total, sv_cap - total, 0, tex, from, &from)) > 0) {
            batch[nb].tex = tex; batch[nb].first = total; batch[nb].count = n; nb++;
            total += n;
        }
    }
    if (total > 0) {
        vx_sprite_upload(v, sv, total);
        for (int b = 0; b < nb; b++) {
            glBindTexture(GL_TEXTURE_2D, batch[b].tex);
            glDrawArrays(GL_TRIANGLES, batch[b].first, batch[b].count);
            vxp_draw(batch[b].count / 3);
            draws++;
        }
    }
    // The sky now, into the pixels the world did not cover, and before anything blended. The sprite
    // scope is closed around it so the sky gets a GPU timer query of its own — a query cannot nest.
    vxp_end(VXP_SPRITES);
    vx_draw_sky(v, sky);
    vxp_begin(VXP_SPRITES);

    // shadows, then the additive lamp glows
    glUseProgram(v->spr_prog);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glDepthMask(GL_FALSE);
    { int from = 0, n = vx_emit_sprites(v, sv, sv_cap, 1, 0, 0, &from); if (n) { vx_draw_sprite_batch(v, sv, n); draws++; } }
    glBlendFunc(GL_ONE, GL_ONE);
    { int from = 0, n = vx_emit_sprites(v, sv, sv_cap, 2, 0, 0, &from); if (n) { vx_draw_sprite_batch(v, sv, n); draws++; } }
    glDepthMask(GL_TRUE);
    glDisable(GL_BLEND);
    glDisable(GL_CULL_FACE);
    glBindVertexArray(0);
    vxp_end(VXP_SPRITES);
    vxp.c.sprites = v->spr_count;
    vxp.c.detail = v->det_count;
    vxp.c.fbo_w = w; vxp.c.fbo_h = h;
    vxp.c.shadow_dim = v->shadow_dim;

    if (!v->selfchecked) { vx_selfcheck(v, w, h, draws, v->spr_count); v->selfchecked = true; }
}

// The HD-2D pass: tilt-shift, bloom, vignette and a gentle grade. Everything but the composite runs
// at half resolution. With `hd2d` off nothing here runs and the scene texture is shown as it is.
// ───────────────────────── overdraw ─────────────────────────
// How many times, on average, does a pixel of the opaque world get written? 1.0 means a perfect
// front-to-back order with early-Z doing its job; 3.0 means two thirds of the shading is thrown away
// and a depth prepass or a deferred pass would be worth arguing about.
//
// It is measured, not modelled: the same chunks are rasterized in the same order into the scene FBO
// with the constant-colour probe, blending ONE/ONE against a fresh depth buffer, and the result is
// read back. That readback is a full pipeline stall, which is why this is only ever called from the
// capture path, the benchmark, and the debug view — never from a frame the owner is playing.
static void vx_overdraw_pass(VoxField *v, int w, int h, float step) {
    glBindFramebuffer(GL_FRAMEBUFFER, v->fbo);
    glViewport(0, 0, w, h);
    glClearColor(0, 0, 0, 0);
    glClearDepthf(1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    glEnable(GL_DEPTH_TEST); glDepthFunc(GL_LESS); glDepthMask(GL_TRUE);
    glEnable(GL_CULL_FACE); glCullFace(GL_BACK); glFrontFace(GL_CCW);
    glEnable(GL_BLEND); glBlendFunc(GL_ONE, GL_ONE);
    glUseProgram(v->od_prog);
    glUniformMatrix4fv(vx_uni(v->od_prog, "u_mvp"), 1, GL_FALSE, v->mvp);
    glUniformMatrix4fv(vx_uni(v->od_prog, "u_view"), 1, GL_FALSE, v->view);
    glUniform1f(vx_uni(v->od_prog, "u_step"), step);
    glBindVertexArray(v->vao);
    for (int i = 0; i < v->od_nord; i++) {                 // the order the real pass just used
        VxChunk *ch = v->od_order[i];
        if (!ch || !ch->verts) continue;
        glBindBuffer(GL_ARRAY_BUFFER, ch->vbo);
        glEnableVertexAttribArray(0); glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(VxVert), (void *)0);
        glEnableVertexAttribArray(1); glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, sizeof(VxVert), (void *)12);
        glEnableVertexAttribArray(2); glVertexAttribIPointer(2, 4, GL_UNSIGNED_BYTE, sizeof(VxVert), (void *)20);
        glEnableVertexAttribArray(3); glVertexAttribPointer(3, 2, GL_UNSIGNED_BYTE, GL_TRUE, sizeof(VxVert), (void *)24);
        glEnableVertexAttribArray(4); glVertexAttribPointer(4, 3, GL_BYTE, GL_TRUE, sizeof(VxVert), (void *)28);
        glDrawArrays(GL_TRIANGLES, 0, ch->verts);
    }
    glDisable(GL_BLEND);
    glBindVertexArray(0);
}

static double vx_measure_overdraw(VoxField *v, int w, int h) {
    vx_overdraw_pass(v, w, h, 1.0f / 255.0f);

    // Read back a stride of rows rather than the whole frame: a 1-in-4 row sample of a two-megapixel
    // buffer is well over a hundred thousand pixels and the mean is stable to three decimals.
    int step = 4, rows = 0;
    size_t rowbytes = (size_t)w * 4;
    unsigned char *px = (unsigned char *)malloc(rowbytes);
    if (!px) return 0.0;
    double sum = 0; long covered = 0, total = 0;
    glPixelStorei(GL_PACK_ALIGNMENT, 1);
    for (int y = 0; y < h; y += step) {
        glReadPixels(0, y, w, 1, GL_RGBA, GL_UNSIGNED_BYTE, px);
        for (int x = 0; x < w; x++) { int c = px[x * 4]; sum += c; total++; if (c) covered++; }
        rows++;
    }
    free(px);
    (void)rows;
    // The colour is the count, one step of 1/255 a write; the mean is over the COVERED pixels, so the
    // sky's empty half of the screen does not flatter the number.
    return covered ? sum / (double)covered : 0.0;
}

static GLuint vx_post(VoxField *v, int w, int h) {
    if (!v->hd2d) return v->fbo_tex;
    glDisable(GL_DEPTH_TEST);
    glDisable(GL_BLEND);
    glBindVertexArray(v->quad_vao);
    glUseProgram(v->blur_prog);
    glUniform1i(vx_uni(v->blur_prog, "u_tex"), 0);
    glActiveTexture(GL_TEXTURE0);
    float hw = (float)v->half_w, hh = (float)v->half_h;
    // scene -> half[0] (horizontal) -> half[1] (vertical): the blurred copy the tilt-shift mixes in
    vxp_begin(VXP_POST_DOWN);
    glBindFramebuffer(GL_FRAMEBUFFER, v->half_fbo[0]);
    glViewport(0, 0, v->half_w, v->half_h);
    glBindTexture(GL_TEXTURE_2D, v->fbo_tex);
    glUniform2f(vx_uni(v->blur_prog, "u_dir"), 1.4f / hw, 0.0f);
    glUniform1f(vx_uni(v->blur_prog, "u_thresh"), 0.0f);
    glDrawArrays(GL_TRIANGLES, 0, 3);
    glBindFramebuffer(GL_FRAMEBUFFER, v->half_fbo[1]);
    glBindTexture(GL_TEXTURE_2D, v->half_tex[0]);
    glUniform2f(vx_uni(v->blur_prog, "u_dir"), 0.0f, 1.4f / hh);
    glDrawArrays(GL_TRIANGLES, 0, 3);
    vxp_draw(2);
    vxp_end(VXP_POST_DOWN);
    // half[1] -> half[2]: the same blur again, thresholded — the bloom
    vxp_begin(VXP_POST_BLOOM);
    if (v->bloom > 0.0f) {
        glBindFramebuffer(GL_FRAMEBUFFER, v->half_fbo[2]);
        glBindTexture(GL_TEXTURE_2D, v->half_tex[1]);
        glUniform2f(vx_uni(v->blur_prog, "u_dir"), 2.6f / hw, 2.6f / hh);
        glUniform1f(vx_uni(v->blur_prog, "u_thresh"), 0.66f);
        glDrawArrays(GL_TRIANGLES, 0, 3);
        vxp_draw(1);
    }
    vxp_end(VXP_POST_BLOOM);
    vxp_begin(VXP_POST_COMP);
    glBindFramebuffer(GL_FRAMEBUFFER, v->out_fbo);
    glViewport(0, 0, w, h);
    glUseProgram(v->post_prog);
    glUniform1i(vx_uni(v->post_prog, "u_scene"), 0);
    glUniform1i(vx_uni(v->post_prog, "u_blur"), 1);
    glUniform1i(vx_uni(v->post_prog, "u_bloom"), 2);
    glUniform1i(vx_uni(v->post_prog, "u_depth"), 3);
    glUniform1f(vx_uni(v->post_prog, "u_dof"), v->dof);
    glUniform1f(vx_uni(v->post_prog, "u_bloom_k"), v->bloom);
    glUniform1f(vx_uni(v->post_prog, "u_vig"), v->vignette);
    glUniform1f(vx_uni(v->post_prog, "u_grade"), v->grade);
    glUniform1f(vx_uni(v->post_prog, "u_pdepth"), v->player_screen[2]);
    glUniform1f(vx_uni(v->post_prog, "u_near"), v->ortho ? 0.5f : 0.5f);
    glUniform1f(vx_uni(v->post_prog, "u_far"), v->ortho ? 140.0f : 200.0f);
    glUniform1f(vx_uni(v->post_prog, "u_band"), v->view_h * 0.42f);
    glUniform1f(vx_uni(v->post_prog, "u_ortho"), v->ortho ? 1.0f : 0.0f);
    glActiveTexture(GL_TEXTURE0); glBindTexture(GL_TEXTURE_2D, v->fbo_tex);
    glActiveTexture(GL_TEXTURE1); glBindTexture(GL_TEXTURE_2D, v->half_tex[1]);
    glActiveTexture(GL_TEXTURE2); glBindTexture(GL_TEXTURE_2D, v->bloom > 0.0f ? v->half_tex[2] : v->half_tex[1]);
    glActiveTexture(GL_TEXTURE3); glBindTexture(GL_TEXTURE_2D, v->fbo_depth);
    glDrawArrays(GL_TRIANGLES, 0, 3);
    vxp_draw(1);
    glActiveTexture(GL_TEXTURE0);
    glBindVertexArray(0);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    vxp_end(VXP_POST_COMP);
    return v->out_tex;
}

// ───────────────────────── the pad and the box (the tile field's, unchanged) ─────────────────────

static void draw_touch_ui(VoxField *v, int w, int h) {
    ImDrawList *dl = ImGui::GetBackgroundDrawList();
    float r = h * 0.14f;
    if (v->stick_on) {
        float dx = v->stick_x - v->stick_ox, dy = v->stick_y - v->stick_oy;
        float len = sqrtf(dx * dx + dy * dy);
        if (len > r) { dx *= r / len; dy *= r / len; }
        dl->AddCircleFilled(ImVec2(v->stick_ox, v->stick_oy), r, IM_COL32(255, 255, 255, 26), 32);
        dl->AddCircle(ImVec2(v->stick_ox, v->stick_oy), r, IM_COL32(255, 255, 255, 70), 32, h * 0.006f);
        dl->AddCircleFilled(ImVec2(v->stick_ox + dx, v->stick_oy + dy), r * 0.42f, IM_COL32(220, 228, 255, 110), 24);
    }
    float ax = w - h * 0.20f, ay = h * 0.74f;
    dl->AddCircle(ImVec2(ax, ay), h * 0.10f, IM_COL32(255, 255, 255, v->act_on ? 130 : 45), 28, h * 0.006f);
    if (v->act_on) dl->AddCircleFilled(ImVec2(ax, ay), h * 0.10f, IM_COL32(255, 255, 255, v->run ? 60 : 30), 28);
    if (v->run) dl->AddText(ImGui::GetFont(), h * 0.05f, ImVec2(ax - h * 0.05f, ay - h * 0.025f), IM_COL32(255, 230, 150, 220), "RUN");
    if (v->exam_trig >= 0 || v->exam_npc >= 0) {
        float px = v->player_screen[0], py = (float)h - v->player_screen[1];
        dl->AddText(ImGui::GetFont(), h * 0.09f, ImVec2(px - h * 0.02f, py - h * 0.22f), IM_COL32(255, 230, 120, 240), "!");
    }
}

static void draw_msg_box(VoxField *v, int w, int h) {
    if (!v->msg[0]) return;
    ImDrawList *dl = ImGui::GetBackgroundDrawList();
    ImFont *font = ImGui::GetFont();
    float ts = h * 0.052f, u = ts * 0.12f;
    float x0 = w * 0.06f, x1 = w * 0.94f, y1 = h * 0.96f, y0 = y1 - h * 0.24f;
    dl->AddRectFilled(ImVec2(x0, y0), ImVec2(x1, y1), IM_COL32(8, 8, 20, 255), u * 1.6f);
    dl->AddRectFilledMultiColor(ImVec2(x0 + u, y0 + u), ImVec2(x1 - u, y1 - u),
        IM_COL32(24, 40, 150, 255), IM_COL32(24, 40, 150, 255), IM_COL32(8, 16, 84, 255), IM_COL32(8, 16, 84, 255));
    dl->AddRect(ImVec2(x0 + u * 0.5f, y0 + u * 0.5f), ImVec2(x1 - u * 0.5f, y1 - u * 0.5f), IM_COL32(225, 225, 235, 255), u * 1.4f, 0, u * 0.7f);
    DlgRect cr = dlg_content(x0, y0, x1, y1, ts);
    float name_h = v->msg_who[0] ? ts * DLG_LINE_H : 0.0f;
    float tw = cr.x1 - cr.x0;
    DlgPages pg;
    dlg_paginate(font, ts, tw, dlg_max_lines((cr.y1 - cr.y0) - name_h, ts), v->msg, &pg);
    if (v->msg_page >= pg.count) v->msg_page = pg.count - 1;
    if (v->msg_page < 0) v->msg_page = 0;
    const char *pb = pg.beg[v->msg_page], *pe = pg.end[v->msg_page];
    int chars = (int)(v->msg_t * VX_TYPE_CPS);
    v->msg_typing = chars < (int)(pe - pb);
    v->msg_more = v->msg_page + 1 < pg.count;
    if (v->msg_who[0]) {
        dl->AddText(font, ts * 0.92f, ImVec2(cr.x0, cr.y0), IM_COL32(255, 216, 74, 255), v->msg_who);
        dlg_draw_text(dl, font, ts, ImVec2(cr.x0, cr.y0 + name_h), tw, IM_COL32_WHITE, pb, pe, chars, false);
    } else {
        float th = dlg_text_height(font, ts, tw, pb, pe);
        dlg_draw_text(dl, font, ts, ImVec2(cr.x0, cr.y0 + ((cr.y1 - cr.y0) - th) * 0.5f), tw, IM_COL32_WHITE, pb, pe, chars, true);
    }
    if (!v->msg_typing) dlg_marker(dl, cr.x1 - ts * 0.45f, cr.y1 - ts * 0.45f, ts, v->msg_more, v->msg_t);
}

static void vx_poll_map_flag(VoxField *v, float dt) {
    // Once a second, not 2.5x. This is a blocking open() on the render thread; on Android's F2FS,
    // with the app's data dir possibly cold, a miss is not free and lands squarely in a frame. The
    // perf flag is polled on the same period but offset half a second (see vx_poll_perf_flag), so
    // the two can never charge the same frame.
    v->map_poll += dt;
    if (v->map_poll < 1.0f) return;
    v->map_poll = 0;
    char path[600];
    snprintf(path, sizeof(path), "%smap.flag", vx_pref());
    size_t sz = 0;
    char *text = (char *)SDL_LoadFile(path, &sz);
    if (!text) return;
    if (!sz) { SDL_free(text); return; }
    char want[64] = "";
    sscanf(text, "%63s", want);
    SDL_free(text);
    SDL_IOStream *tr = SDL_IOFromFile(path, "wb");
    if (tr) SDL_CloseIO(tr);
    for (char *c = want; *c; c++) if (!((*c >= 'a' && *c <= 'z') || (*c >= '0' && *c <= '9') || *c == '_')) { *c = 0; break; }
    if (!want[0]) return;
    SDL_Log("voxfield: map.flag asks for \"%s\"", want);
    vx_load_map(v, want);
}

// ───────────────────────── tick ─────────────────────────

// ───────────────────────── the A/B benchmark ─────────────────────────
// Triggered by the Dev panel's "Run benchmark" or by a `perf.flag` in the pref dir (its contents, if
// any, name the map). It parks the party on a fixed heavy view, runs every configuration for
// VXB_WARM + VXB_MEAS frames, and writes perf_report.md / perf_report.csv next to the flag. The
// owner's own settings are saved on the way in and put back on the way out.
//
// Wall clock is vsync-bound, so it cannot separate two configurations that both beat the refresh
// period. The column that CAN is the GPU total from the timer queries; where those are unavailable
// the benchmark — and only the benchmark — falls back to a glFinish after the post pass.

#define VXB_WARM 30
#define VXB_MEAS 240

enum { VXB_BASE = 0, VXB_HD2D, VXB_DOF, VXB_BLOOM, VXB_GRADE, VXB_SHADOW, VXB_PCF1, VXB_PATTERN,
       VXB_AO, VXB_CUT, VXB_DETAIL, VXB_WATER, VXB_CLOUD, VXB_FOG,
       VXB_R100, VXB_R85, VXB_R75, VXB_R66, VXB_R50, VXB_ALLOFF, VXB_COUNT };

static const char *VXB_NAME[VXB_COUNT] = {
    "baseline", "hd2d off", "dof off", "bloom off", "grade+vignette off", "shadows off",
    "shadow pcf 1", "pattern detail off", "ao off", "cutaway off", "detail sprites off",
    "water anim off", "sky clouds off", "fog off",
    "res 100%", "res 85%", "res 75%", "res 66%", "res 50%", "everything off",
};

// The two parked views. A map with no entry uses its own spawn for both.
static const struct { const char *map; int ax, az; const char *an; int bx, bz; const char *bn; }
VXB_VIEWS[] = { { "halm", 21, 21, "square", 27, 7, "stream" } };

static void vx_place_party(VoxField *v, int x, int z, int f);   // defined above; used by the bench

static void vx_bench_view(VoxField *v, int view) {
    for (size_t i = 0; i < sizeof VXB_VIEWS / sizeof VXB_VIEWS[0]; i++) {
        if (strcmp(VXB_VIEWS[i].map, v->map_name)) continue;
        int x = view ? VXB_VIEWS[i].bx : VXB_VIEWS[i].ax;
        int z = view ? VXB_VIEWS[i].bz : VXB_VIEWS[i].az;
        if (vx_can_stand(v, x, z)) { vx_place_party(v, x, z, 3); return; }
    }
    // no entry, or the cell has moved: the map's own spawn is the fixed view
    vx_place_party(v, v->spawn_x, v->spawn_z, 3);
}

static const char *vx_bench_view_name(VoxField *v, int view) {
    for (size_t i = 0; i < sizeof VXB_VIEWS / sizeof VXB_VIEWS[0]; i++)
        if (!strcmp(VXB_VIEWS[i].map, v->map_name)) return view ? VXB_VIEWS[i].bn : VXB_VIEWS[i].an;
    return view ? "spawn(b)" : "spawn";
}

// Put the owner's look back, then turn off exactly one thing.
static void vx_bench_apply(VoxField *v, int cfg) {
    const VxSave *s = &v->bench_saved;
    v->hd2d = s->hd2d; v->dof = s->dof; v->bloom = s->bloom;
    v->vignette = s->vignette; v->grade = s->grade;
    v->sh_str = s->sh_str; v->ao_str = s->ao; v->cutaway = s->cutaway;
    v->detail = s->detail; v->fog = s->fog; v->res_pct = s->res_scale_pct;
    v->q_pattern = 1.0f; v->q_pcf = 4.0f; v->q_water = 1.0f; v->q_cloud = 1.0f;
    switch (cfg) {
    case VXB_BASE:    break;
    case VXB_HD2D:    v->hd2d = 0; break;
    case VXB_DOF:     v->dof = 0; break;
    case VXB_BLOOM:   v->bloom = 0; break;
    case VXB_GRADE:   v->grade = 0; v->vignette = 0; break;
    case VXB_SHADOW:  v->sh_str = 0; break;
    case VXB_PCF1:    v->q_pcf = 1.0f; break;
    case VXB_PATTERN: v->q_pattern = 0.0f; break;
    case VXB_AO:      v->ao_str = 0; break;
    case VXB_CUT:     v->cutaway = 0; break;
    case VXB_DETAIL:  v->detail = 0; break;
    case VXB_WATER:   v->q_water = 0.0f; break;
    case VXB_CLOUD:   v->q_cloud = 0.0f; break;
    case VXB_FOG:     v->fog = 0; break;
    case VXB_R100:    v->res_pct = 100; break;
    case VXB_R85:     v->res_pct = 85; break;
    case VXB_R75:     v->res_pct = 75; break;
    case VXB_R66:     v->res_pct = 66; break;
    case VXB_R50:     v->res_pct = 50; break;
    case VXB_ALLOFF:
        v->hd2d = 0; v->dof = 0; v->bloom = 0; v->vignette = 0; v->grade = 0;
        v->sh_str = 0; v->ao_str = 0; v->cutaway = 0; v->detail = 0; v->fog = 0;
        v->q_pattern = 0.0f; v->q_pcf = 1.0f; v->q_water = 0.0f; v->q_cloud = 0.0f;
        break;
    }
}

static void vx_bench_start(VoxField *v, const char *map) {
    if (v->bench_on) return;
    if (map && map[0] && strcmp(map, v->map_name)) vx_load_map(v, map);
    vx_save(v, &v->bench_saved);
    v->bench_have_saved = true;
    v->bench_on = 1;                     // 1 = view A, 2 = view B
    v->bench_cfg = 0; v->bench_frame = 0;
    v->bench_row_n = 0; v->bench_base_ms = 0;
    snprintf(v->bench_map, sizeof v->bench_map, "%s", v->map_name);
    vx_bench_view(v, 0);
    vx_bench_apply(v, 0);
    SDL_Log("PERF start: map=%s configs=%d frames=%d+%d per config, two views", v->map_name, VXB_COUNT, VXB_WARM, VXB_MEAS);
}

static void vx_bench_write_report(VoxField *v) {
    char path[768];
    const char *gl_ver = (const char *)glGetString(GL_VERSION);
    const char *gl_ren = (const char *)glGetString(GL_RENDERER);
    char date[32];
    { SDL_Time t = 0; SDL_DateTime dt2;
      if (SDL_GetCurrentTime(&t) && SDL_TimeToDateTime(t, &dt2, true))
          snprintf(date, sizeof date, "%04d-%02d-%02d %02d:%02d", dt2.year, dt2.month, dt2.day, dt2.hour, dt2.minute);
      else snprintf(date, sizeof date, "unknown"); }

    snprintf(path, sizeof path, "%sperf_report.md", vx_pref());
    SDL_IOStream *io = SDL_IOFromFile(path, "w");
    if (io) {
        char hdr[1024];
        int n = snprintf(hdr, sizeof hdr,
            "# voxfield perf report\n\n"
            "- date: %s\n- map: %s\n- platform: %s\n- GL: %s\n- renderer: %s\n"
            "- drawable: %dx%d\n- refresh: %.0f Hz\n- %s\n- mesh: %.1f ms, %d tris\n\n"
            "| view | config | wall ms | p99 ms | fps | gpu ms | cpu ms | delta gpu | overdraw |\n"
            "|---|---|---|---|---|---|---|---|---|\n",
            date, v->bench_map, SDL_GetPlatform(), gl_ver ? gl_ver : "?", gl_ren ? gl_ren : "?",
            vxp.c_shown.fbo_w, vxp.c_shown.fbo_h, vxp.refresh_hz, vxp.gpu_note, v->mesh_ms, v->tris);
        SDL_WriteIO(io, hdr, (size_t)n);
        for (int i = 0; i < v->bench_row_n; i++) SDL_WriteIO(io, v->bench_rows[i], strlen(v->bench_rows[i]));
        // Where the frame actually goes, as the last configuration left it. The scope table is the
        // thing to read first: the A/B rows say what a feature costs, this says what a PASS costs.
        char tail[1024];
        int m = snprintf(tail, sizeof tail,
            "\n## scopes, last configuration (1 s average)\n\n| scope | cpu ms | gpu ms |\n|---|---|---|\n");
        SDL_WriteIO(io, tail, (size_t)m);
        for (int i = 0; i < VXP_COUNT; i++) {
            m = snprintf(tail, sizeof tail, "| %s | %.2f | %.2f |\n", VXP_NAME[i], vxp.s[i].cpu_avg, vxp.s[i].gpu_avg);
            SDL_WriteIO(io, tail, (size_t)m);
        }
        m = snprintf(tail, sizeof tail,
            "\n## counters\n\n- draws %d, triangles %d\n- chunks %d/%d drawn, sprites %d, detail %d\n"
            "- fbo %dx%d, drawable %dx%d, shadow %d px\n- gpu memory estimate %.1f MB (%.1f tex + %.1f vbo)\n"
            "- vsync buckets 1x/2x/3x/4x+/off: %d/%d/%d/%d/%d\n",
            vxp.c_shown.draws, vxp.c_shown.tris, vxp.c_shown.chunks_drawn, vxp.c_shown.chunks_total,
            vxp.c_shown.sprites, vxp.c_shown.detail, vxp.c_shown.fbo_w, vxp.c_shown.fbo_h,
            vxp.c_shown.out_w, vxp.c_shown.out_h, vxp.c_shown.shadow_dim,
            vxp.c_shown.tex_mb + vxp.c_shown.vbo_mb, vxp.c_shown.tex_mb, vxp.c_shown.vbo_mb,
            vxp.bucket[0], vxp.bucket[1], vxp.bucket[2], vxp.bucket[3], vxp.bucket[4]);
        SDL_WriteIO(io, tail, (size_t)m);
        SDL_CloseIO(io);
    }
    snprintf(path, sizeof path, "%sperf_report.csv", vx_pref());
    io = SDL_IOFromFile(path, "w");
    if (io) {
        char hdr[512];
        int n = snprintf(hdr, sizeof hdr, "# %s,%s,%s,%s,%dx%d\nview,config,wall_ms,p99_ms,fps,gpu_ms,cpu_ms\n",
                         date, v->bench_map, gl_ren ? gl_ren : "?", gl_ver ? gl_ver : "?",
                         vxp.c_shown.fbo_w, vxp.c_shown.fbo_h);
        SDL_WriteIO(io, hdr, (size_t)n);
        for (int i = 0; i < v->bench_row_n; i++) {
            // the markdown row, turned back into csv
            // "| a | b |\n" -> "a,b\n": drop the leading and trailing bars and the padding spaces
            char line[320]; int k = 0;
            for (const char *c = v->bench_rows[i]; *c && k < 318; c++) {
                if (*c == '|') { if (k && line[k - 1] != ',') line[k++] = ','; continue; }
                if (*c == ' ') continue;
                line[k++] = *c;
            }
            while (k && (line[k - 1] == ',' || line[k - 1] == '\n')) k--;
            line[k++] = '\n';
            SDL_WriteIO(io, line + (line[0] == ',' ? 1 : 0), (size_t)(k - (line[0] == ',' ? 1 : 0)));
        }
        SDL_CloseIO(io);
    }
    SDL_Log("PERF done: %s", path);
}

// One frame of the benchmark, called after the frame has been presented. Returns true while running.
static void vx_bench_frame(VoxField *v) {
    if (!v->bench_on) return;
    v->bench_frame++;
    if (v->bench_frame > VXB_WARM) {
        double ms = vxp.frame_ms;
        v->bench_sum += ms;
        if (v->bench_hist_n < 256) v->bench_hist[v->bench_hist_n++] = ms;
        for (int i = 0; i < VXP_COUNT; i++) v->bench_gpu[i] += vxp.s[i].gpu_ms;
    }
    if (v->bench_frame < VXB_WARM + VXB_MEAS) return;

    int meas = VXB_MEAS;
    double wall = v->bench_sum / meas;
    double gpu = 0, cpu = 0;
    for (int i = VXP_SHADOW; i <= VXP_UI; i++) gpu += v->bench_gpu[i] / meas;
    for (int i = 0; i < VXP_COUNT; i++) if (i != VXP_MESH) cpu += vxp.s[i].cpu_avg;
    // p99 over the sampled history
    double srt[256]; int n = v->bench_hist_n;
    for (int i = 0; i < n; i++) srt[i] = v->bench_hist[i];
    for (int i = 1; i < n; i++) { double x = srt[i]; int j = i - 1;
        while (j >= 0 && srt[j] < x) { srt[j + 1] = srt[j]; j--; } srt[j + 1] = x; }
    double p99 = n ? srt[n / 100] : 0;

    if (v->bench_cfg == VXB_BASE) v->bench_base_ms = gpu;
    // One overdraw measurement a configuration, at the end of its measured window. It costs a
    // readback and corrupts the one frame it is taken in, which is what a benchmark is for.
    if (vxp.c_shown.fbo_w > 0 && vxp.c_shown.fbo_h > 0)
        v->od_avg = vx_measure_overdraw(v, vxp.c_shown.fbo_w, vxp.c_shown.fbo_h);
    const char *vn = vx_bench_view_name(v, v->bench_on - 1);
    char row[320];
    snprintf(row, sizeof row, "| %s | %s | %.2f | %.2f | %.1f | %.2f | %.2f | %+.2f | %.2f |\n",
             vn, VXB_NAME[v->bench_cfg], wall, p99, wall > 0 ? 1000.0 / wall : 0.0,
             gpu, cpu, gpu - v->bench_base_ms, v->od_avg);
    if (v->bench_row_n < 64) snprintf(v->bench_rows[v->bench_row_n++], 320, "%s", row);
    SDL_Log("PERF %-8s %-20s wall %6.2f ms  p99 %6.2f  %5.1f fps  gpu %6.2f  cpu %6.2f  d %+6.2f  od %.2f",
            vn, VXB_NAME[v->bench_cfg], wall, p99, wall > 0 ? 1000.0 / wall : 0.0, gpu, cpu,
            gpu - v->bench_base_ms, v->od_avg);

    v->bench_sum = 0; v->bench_hist_n = 0; v->bench_frame = 0;
    for (int i = 0; i < VXP_COUNT; i++) v->bench_gpu[i] = 0;
    v->bench_cfg++;
    if (v->bench_cfg >= VXB_COUNT) {
        v->bench_cfg = 0;
        v->bench_base_ms = 0;
        v->bench_on++;
        if (v->bench_on > 2) {                       // both views done
            v->bench_on = 0;
            if (v->bench_have_saved) vx_restore(v, &v->bench_saved);
            v->q_pattern = 1.0f; v->q_pcf = 4.0f; v->q_water = 1.0f; v->q_cloud = 1.0f;
            vx_bench_write_report(v);
            return;
        }
        vx_bench_view(v, v->bench_on - 1);
    }
    vx_bench_apply(v, v->bench_cfg);
}

// `perf.flag` in the pref dir: the Mac's way in (perf.sh writes it). Contents = a map name, or empty.
static void vx_poll_perf_flag(VoxField *v, float dt) {
    if (v->bench_on) return;
    v->bench_poll -= dt;
    if (v->bench_poll > 0) return;
    v->bench_poll = 1.0f;                                // offset from map.flag's by vx_create/vx_restore
    char path[768];
    snprintf(path, sizeof path, "%sperf.flag", vx_pref());
    size_t sz = 0;
    void *d = SDL_LoadFile(path, &sz);
    if (!d) return;
    // A consumed flag is TRUNCATED, not deleted, exactly as reload.flag is — and SDL_LoadFile hands
    // back a valid (empty) buffer for a 0-byte file, so without this the benchmark would restart
    // itself forever half a second after finishing.
    if (sz == 0) { SDL_free(d); return; }
    char map[32] = { 0 };
    if (sz) { size_t n = sz < 31 ? sz : 31; memcpy(map, d, n);
              for (char *c = map; *c; c++) if (*c == '\n' || *c == '\r' || *c == ' ') { *c = 0; break; } }
    SDL_free(d);
    SDL_IOStream *io = SDL_IOFromFile(path, "w");        // consume it, as the reload flag is consumed
    if (io) SDL_CloseIO(io);
    vx_bench_start(v, map);
}

// Cached: SDL_GetPrimaryDisplay + SDL_GetCurrentDisplayMode takes the display lock and, on Android,
// walks the display list. The refresh rate does not change between frames; re-reading it 60 times a
// second was pure per-frame overhead on the render thread. Once a second is plenty.
static double vx_refresh_hz(void) {
    static double cached = 60.0;
    static uint64_t next_at = 0;
    uint64_t now = SDL_GetTicks();
    if (now >= next_at) {
        next_at = now + 1000;
        const SDL_DisplayMode *m = SDL_GetCurrentDisplayMode(SDL_GetPrimaryDisplay());
        if (m && m->refresh_rate > 1.0f) cached = (double)m->refresh_rate;
    }
    return cached;
}

// A rough GPU memory estimate: the render targets, the shadow map, the atlas and the decals. It is
// an estimate and is labelled as one — no GL query for this exists on GLES.
static double vx_tex_mb(VoxField *v, int rw, int rh) {
    double b = 0;
    b += (double)rw * rh * 8.0;                                  // scene colour + depth24
    b += (double)v->half_w * v->half_h * 4.0 * 3.0;              // the three half-res targets
    b += (double)rw * rh * 4.0;                                  // the post output
    b += (double)v->shadow_dim * v->shadow_dim * 4.0;            // depth24
    b += (double)v->atlas_w * v->atlas_h;                        // R8
    for (int i = 0; i < v->decal_count; i++) b += (double)v->decal_w[i] * v->decal_h[i];
    return b / (1024.0 * 1024.0);
}

void vx_tick(VoxField *v, int w, int h, float dt, bool ui_blocked, VxEvent *ev) {
    ev->kind = VXE_NONE; ev->arg[0] = 0;
    if (!v->gl_ready) vx_gl_init(v);
    vxp_init();
    // The GPU timer queries are only armed when somebody is reading them. See vxp_want_gpu.
    vxp_want_gpu(v->perf_hud > 0 || v->bench_on != 0);
    vxp_frame_begin(vx_refresh_hz());
    vxp_begin(VXP_TICK);
    if (!v->built) {
        vx_load_map(v, v->map_name[0] ? v->map_name : "halm");
        const char *b = SDL_getenv("VOX_HD2D");            // a measuring switch, not a setting
        if (b && b[0]) v->hd2d = atoi(b) ? 1 : 0;
    }
    vx_poll_map_flag(v, dt);
    vx_poll_perf_flag(v, dt);
    v->anim_t += dt;
    v->msg_t += dt;

    vx_input(v, w, h, ui_blocked, dt);

    if (v->fade_dir) {
        v->fade += v->fade_dir;
        if (v->fade_dir > 0 && v->fade >= FADE_FRAMES) {
            vx_load_map(v, v->to_map);
            vx_place_party(v, v->to_x, v->to_z, v->to_f);
            for (int i = 0; i < v->trig_count; i++)
                v->trigs[i].inside = (v->act[0].tx >= v->trigs[i].x && v->act[0].tz >= v->trigs[i].z &&
                                      v->act[0].tx < v->trigs[i].x + v->trigs[i].w && v->act[0].tz < v->trigs[i].z + v->trigs[i].d);
            v->fade_dir = -1;
        } else if (v->fade_dir < 0 && v->fade <= 0) { v->fade = 0; v->fade_dir = 0; }
    }

    vx_walk(v, w, dt, ev);
    npc_step(v, dt);

    v->exam_trig = v->exam_npc = -1;
    {
        int fx = v->act[0].tx + DX[v->act[0].facing & 3], fz = v->act[0].tz + DZ[v->act[0].facing & 3];
        v->exam_npc = npc_at(v, fx, fz);
        if (v->exam_npc < 0) {
            v->exam_trig = trig_at(v, fx, fz, TG_MESSAGE, TG_DOOR);
            if (v->exam_trig < 0) v->exam_trig = trig_at(v, v->act[0].tx, v->act[0].tz, TG_MESSAGE, TG_MESSAGE);
        }
    }
    if (v->tapped && !v->fade_dir) {
        if (v->msg[0]) {
            if (v->msg_typing) v->msg_t = 99.0f;
            else if (v->msg_more) { v->msg_page++; v->msg_t = 0; }
            else { v->msg[0] = 0; v->msg_who[0] = 0; v->msg_page = 0; }
        } else if (v->exam_npc >= 0) {
            VxNpc *np = &v->npcs[v->exam_npc];
            static const int OPP[4] = { 3, 2, 1, 0 };
            np->facing = OPP[v->act[0].facing & 3];
            vx_say(v, nullptr, np->text);
        } else if (v->exam_trig >= 0) {
            VxTrig *g = &v->trigs[v->exam_trig];
            if (g->kind == TG_DOOR) start_map_change(v, g->map, g->ax, g->az, g->af);
            else vx_say(v, nullptr, g->arg);
        }
    }

    vxp_end(VXP_TICK);

    int rw = (int)(w * (v->res_pct / 100.0f) + 0.5f), rh = (int)(h * (v->res_pct / 100.0f) + 0.5f);
    if (rw < 64) rw = 64;
    if (rh < 64) rh = 64;
    while (vx_max_tex > 0 && (rw > vx_max_tex || rh > vx_max_tex)) { rw /= 2; rh /= 2; }
    vx_fbo_size(v, rw, rh);
    vx_render(v, rw, rh);
    GLuint shown = vx_post(v, rw, rh);
    // The Overdraw debug view (Dev panel). It replaces the picture with the write count, one warm
    // step a write, and re-measures the number at most once a second — the readback is a stall, so
    // it is rate-limited even here and never runs with the view off.
    if (v->od_view) {
        static uint64_t od_next = 0;
        uint64_t nowt = SDL_GetTicks();
        if (nowt >= od_next) { od_next = nowt + 1000; v->od_avg = vx_measure_overdraw(v, rw, rh); }
        vx_overdraw_pass(v, rw, rh, 0.20f);
        shown = v->fbo_tex;
    }
    // NO glFinish HERE. One used to sit on this line to make the CPU timer below mean something; on
    // a tile-based mobile GPU it flushed and waited inside every frame, which is exactly the stall
    // this instrumentation exists to find. Per-pass GPU cost now comes from timer queries instead.
    // The benchmark is the one caller allowed to stall, and only when the queries are unavailable.
    if (v->bench_on && !vxp.gpu_proven) glFinish();
    // Only the HUD shows this, and it loops over every decal to work it out. Don't pay for it when
    // nobody is looking.
    if (v->perf_hud > 0) vxp.c.tex_mb = vx_tex_mb(v, rw, rh);
    vxp.c.out_w = w; vxp.c.out_h = h;
    v->frame_n++;
    if (v->frame_n >= 300) {
        // The periodic line: wall clock (the honest frame interval), the GPU total, and the three
        // most expensive scopes, so a logcat tail alone tells you where the time went.
        int o[VXP_COUNT];
        for (int i = 0; i < VXP_COUNT; i++) o[i] = i;
        for (int i = 1; i < VXP_COUNT; i++) { int x = o[i]; int j = i - 1;
            double kx = vxp.s[x].gpu_avg > 0 ? vxp.s[x].gpu_avg : vxp.s[x].cpu_avg;
            while (j >= 0) { double kj = vxp.s[o[j]].gpu_avg > 0 ? vxp.s[o[j]].gpu_avg : vxp.s[o[j]].cpu_avg;
                             if (kj >= kx) break; o[j + 1] = o[j]; j--; }
            o[j + 1] = x; }
        SDL_Log("voxfield perf: %.1f fps  wall %.2f ms (1%%w %.2f, max %.2f)  gpu %.2f  cpu %.2f  "
                "at %dx%d (hd2d %s res %d%%)  top: %s %.2f | %s %.2f | %s %.2f",
                vxp.fps, vxp.frame_avg, vxp.frame_p99, vxp.frame_max, vxp_gpu_total(), vxp_cpu_total(),
                rw, rh, v->hd2d ? "on" : "off", v->res_pct,
                VXP_NAME[o[0]], vxp.s[o[0]].gpu_avg > 0 ? vxp.s[o[0]].gpu_avg : vxp.s[o[0]].cpu_avg,
                VXP_NAME[o[1]], vxp.s[o[1]].gpu_avg > 0 ? vxp.s[o[1]].gpu_avg : vxp.s[o[1]].cpu_avg,
                VXP_NAME[o[2]], vxp.s[o[2]].gpu_avg > 0 ? vxp.s[o[2]].gpu_avg : vxp.s[o[2]].cpu_avg);
        v->frame_n = 0;
    }
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glDisable(GL_DEPTH_TEST);

    vxp_begin(VXP_UI);
    ImDrawList *dl = ImGui::GetBackgroundDrawList();
    dl->AddRectFilled(ImVec2(0, 0), ImVec2((float)w, (float)h), IM_COL32(0, 0, 0, 255));
    dl->AddImage((ImTextureID)(intptr_t)shown, ImVec2(0, 0), ImVec2((float)w, (float)h),
                 ImVec2(0.0f, 1.0f), ImVec2(1.0f, 0.0f));
    draw_touch_ui(v, w, h);
    draw_msg_box(v, w, h);
    if (v->fade > 0) {
        int a = v->fade * 255 / FADE_FRAMES;
        dl->AddRectFilled(ImVec2(0, 0), ImVec2((float)w, (float)h), IM_COL32(0, 0, 0, a > 255 ? 255 : a));
    }
    if (v->dbg_coord) {
        char lbl[96];
        snprintf(lbl, sizeof(lbl), "%s %d,%d %s  y%d  steps %d  %d tris", v->map_name, v->act[0].tx, v->act[0].tz,
                 FACE_NAME[v->act[0].facing & 3], (int)v->act[0].y, v->steps, v->tris);
        dl->AddText(ImGui::GetFont(), h * 0.04f, ImVec2(12, 12), IM_COL32(255, 240, 160, 230), lbl);
    }
    vxp_hud(v->perf_hud, w, h);
    if (v->bench_on) {
        char bl[128];
        snprintf(bl, sizeof bl, "BENCHMARK  view %d/2  %s  %d/%d", v->bench_on, VXB_NAME[v->bench_cfg],
                 v->bench_frame, VXB_WARM + VXB_MEAS);
        ImGui::GetForegroundDrawList()->AddText(ImGui::GetFont(), h * 0.045f, ImVec2(w * 0.04f, h * 0.06f),
                                                IM_COL32(255, 200, 120, 240), bl);
    }
    vxp_end(VXP_UI);
    vx_bench_frame(v);
}

// ───────────────────────── capture ─────────────────────────

static void vx_do_capture(VoxField *v) {
    int w = v->cap_wi, h = v->cap_hi;
    vx_probe_limits();
    while (vx_max_tex > 0 && (w > vx_max_tex || h > vx_max_tex)) { w /= 2; h /= 2; }
    vx_fbo_size(v, w, h);
    vx_render(v, w, h);
    // VOX_OVERDRAW=1 measures the opaque world's overdraw for this exact view and says so. It
    // scribbles on the scene FBO, so the real render is simply done again afterwards — this is the
    // capture path, where one extra frame costs nothing and the picture must come out unchanged.
    {
        const char *od = SDL_getenv("VOX_OVERDRAW");
        if (od && od[0] && od[0] != '0') {
            v->od_avg = vx_measure_overdraw(v, w, h);
            SDL_Log("voxfield: OVERDRAW %s %dx%d  world opaque = %.3f writes per covered pixel  "
                    "(cutaway program on %d of %d visible chunks)",
                    v->map_name, w, h, v->od_avg, v->od_chunks_cut, v->chunks_drawn);
            vx_render(v, w, h);
        }
    }
    GLuint shown = vx_post(v, w, h);
    GLuint src_fbo = (shown == v->out_tex) ? v->out_fbo : v->fbo;
    unsigned char *px = (unsigned char *)malloc((size_t)w * h * 4);
    unsigned char *row = (unsigned char *)malloc((size_t)w * 4);
    if (!px || !row) { free(px); free(row); return; }
    glBindFramebuffer(GL_FRAMEBUFFER, src_fbo);
    glPixelStorei(GL_PACK_ALIGNMENT, 1);
    glReadPixels(0, 0, w, h, GL_RGBA, GL_UNSIGNED_BYTE, px);
    for (int y = 0; y < h / 2; y++) {
        memcpy(row, px + (size_t)y * w * 4, (size_t)w * 4);
        memcpy(px + (size_t)y * w * 4, px + (size_t)(h - 1 - y) * w * 4, (size_t)w * 4);
        memcpy(px + (size_t)(h - 1 - y) * w * 4, row, (size_t)w * 4);
    }
    int ok = stbi_write_png(v->cap_out, w, h, 4, px, w * 4);
    SDL_Log("voxfield: capture %s %dx%d %s", v->cap_out, w, h, ok ? "written" : "FAILED");
    free(px); free(row);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    v->cap_ok = true;
}

void vx_capture_to(VoxField *v, const char *map, int w, int h, const char *out_path) {
    if (!v->gl_ready) vx_gl_init(v);
    vx_load_map(v, map);
    v->cap_wi = w > 0 ? w : 1920;
    v->cap_hi = h > 0 ? h : 1080;
    // The knobs a shot can be taken with, from the Mac, with no phone:
    const char *e;
    if ((e = SDL_getenv("VOX_AT")) && e[0]) {
        int x = 0, z = 0;
        if (sscanf(e, "%d,%d", &x, &z) == 2) vx_place_party(v, x, z, 0);
    }
    if ((e = SDL_getenv("VOX_ORTHO")) && e[0]) v->ortho = atoi(e) ? 1 : 0;
    if ((e = SDL_getenv("VOX_HD2D")) && e[0]) v->hd2d = atoi(e) ? 1 : 0;
    if ((e = SDL_getenv("VOX_PITCH")) && e[0]) v->pitch = (float)atof(e);
    if ((e = SDL_getenv("VOX_FOV")) && e[0]) v->fov = (float)atof(e);
    if ((e = SDL_getenv("VOX_VIEWH")) && e[0]) v->view_h = (float)atof(e);
    if ((e = SDL_getenv("VOX_LIGHT")) && e[0]) {
        char tb[16] = "day"; float lv = 1.0f;
        if (sscanf(e, "%15[^:]:%f", tb, &lv) >= 1) { v->light_table = vx_table_by_name(v, tb); v->amb = lv < 0 ? 0 : lv > 1 ? 1 : lv; }
    }
    if ((e = SDL_getenv("VOX_CUT")) && e[0]) v->cutaway = atoi(e) ? 1 : 0;
    if ((e = SDL_getenv("VOX_FOG")) && e[0]) v->fog = (float)atof(e);
    if ((e = SDL_getenv("VOX_TILT")) && e[0]) v->tilt = (float)atof(e);
    if ((e = SDL_getenv("VOX_FACE")) && e[0]) { int f = facing_of(e); for (int i = 0; i < VX_PARTY; i++) v->act[i].facing = f; }
    // The party stands on one cell at spawn: trail them out so the shot shows everyone.
    for (int i = 1; i < v->party; i++) {
        int bx = v->act[i - 1].tx - DX[v->act[0].facing & 3], bz = v->act[i - 1].tz - DZ[v->act[0].facing & 3];
        if (!vx_can_stand(v, bx, bz)) break;
        v->act[i].tx = v->act[i].px = (short)bx; v->act[i].tz = v->act[i].pz = (short)bz;
        v->act[i].y = v->act[i].y0 = vx_gy(v, bx, bz);
    }
    snprintf(v->cap_out, sizeof(v->cap_out), "%s", out_path);
    v->cap_req = 3;
}

bool vx_capture_done(VoxField *v) {
    if (v->cap_req > 0) { if (--v->cap_req == 0) vx_do_capture(v); return false; }
    return v->cap_ok;
}

// ───────────────────────── dev, save, lifecycle ─────────────────────────

bool vx_dev_ui(VoxField *v, char *out, int cap) {
    bool printed = false;
    ImGui::Text("vox field — %s  %d,%d %s  y%d  steps %d  (%dx%d, %d tris)", v->map_name,
                v->act[0].tx, v->act[0].tz, FACE_NAME[v->act[0].facing & 3], (int)v->act[0].y,
                v->steps, v->mw, v->md, v->tris);
    for (int i = 0; i < vx_map_count(); i++) {
        if (i) ImGui::SameLine();
        bool cur = !strcmp(v->map_name, vx_map_name_at(i));
        if (cur) ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.2f, 0.55f, 0.3f, 1));
        if (ImGui::Button(vx_map_name_at(i))) vx_load_map(v, vx_map_name_at(i));
        if (cur) ImGui::PopStyleColor();
    }
    ImGui::SameLine();
    if (ImGui::Button("Respawn")) vx_load_map(v, v->map_name);
    bool c = v->dbg_coord != 0, d = v->noclip != 0, ct = v->cutaway != 0;
    if (ImGui::Checkbox("Coords", &c)) v->dbg_coord = c;
    ImGui::SameLine();
    if (ImGui::Checkbox("No clip", &d)) v->noclip = d;
    ImGui::SameLine();
    if (ImGui::Checkbox("Cut away", &ct)) v->cutaway = ct;
    // camera
    bool o = v->ortho != 0;
    if (ImGui::Checkbox("Ortho", &o)) v->ortho = o;
    ImGui::SameLine();
    ImGui::SetNextItemWidth(ImGui::GetFontSize() * 8);
    ImGui::SliderFloat("pitch", &v->pitch, 20.0f, 75.0f, "%.0f");
    ImGui::SameLine();
    ImGui::SetNextItemWidth(ImGui::GetFontSize() * 8);
    ImGui::SliderFloat("fov", &v->fov, 12.0f, 60.0f, "%.0f");
    ImGui::SetNextItemWidth(ImGui::GetFontSize() * 8);
    ImGui::SliderFloat("view blocks", &v->view_h, 5.0f, 24.0f, "%.1f");
    ImGui::SameLine();
    ImGui::SetNextItemWidth(ImGui::GetFontSize() * 8);
    ImGui::SliderFloat("sprite tilt", &v->tilt, 0.0f, 1.0f, "%.2f");
    ImGui::SetNextItemWidth(ImGui::GetFontSize() * 8);
    ImGui::SliderFloat("AO", &v->ao_str, 0.0f, 1.0f, "%.2f");
    ImGui::SameLine();
    ImGui::SetNextItemWidth(ImGui::GetFontSize() * 8);
    ImGui::SliderFloat("detail", &v->detail, 0.0f, 1.0f, "%.2f");
    ImGui::SameLine();
    ImGui::SetNextItemWidth(ImGui::GetFontSize() * 8);
    ImGui::SliderFloat("fog", &v->fog, 0.0f, 1.0f, "%.2f");
    // HD-2D
    ImGui::Separator();
    bool hd = v->hd2d != 0;
    if (ImGui::Checkbox("HD-2D", &hd)) v->hd2d = hd;
    ImGui::SameLine();
    ImGui::SetNextItemWidth(ImGui::GetFontSize() * 7);
    ImGui::SliderFloat("tilt-shift", &v->dof, 0.0f, 1.0f, "%.2f");
    ImGui::SameLine();
    ImGui::SetNextItemWidth(ImGui::GetFontSize() * 7);
    ImGui::SliderFloat("bloom", &v->bloom, 0.0f, 1.5f, "%.2f");
    ImGui::SetNextItemWidth(ImGui::GetFontSize() * 7);
    ImGui::SliderFloat("vignette", &v->vignette, 0.0f, 1.0f, "%.2f");
    ImGui::SameLine();
    ImGui::SetNextItemWidth(ImGui::GetFontSize() * 7);
    ImGui::SliderFloat("grade", &v->grade, 0.0f, 1.0f, "%.2f");
    ImGui::SameLine();
    ImGui::SetNextItemWidth(ImGui::GetFontSize() * 7);
    ImGui::SliderInt("res %", &v->res_pct, 40, 100);
    // light
    ImGui::Separator();
    for (int i = 0; i < v->cmap_tables && i < 8; i++) {
        if (i) ImGui::SameLine();
        bool cur = v->light_table == i;
        if (cur) ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.2f, 0.4f, 0.65f, 1));
        if (ImGui::Button(v->cmap_tname[i][0] ? v->cmap_tname[i] : "?")) v->light_table = i;
        if (cur) ImGui::PopStyleColor();
    }
    ImGui::SetNextItemWidth(ImGui::GetFontSize() * 12);
    if (ImGui::SliderFloat("ambient", &v->amb, 0.0f, 1.0f, "%.2f")) {}
    // The sun and its shadow map. Every one of these re-renders the map's depth pass, which costs a
    // couple of milliseconds once — nothing per frame.
    ImGui::Separator();
    ImGui::TextUnformatted("Sun / shadow");
    ImGui::SetNextItemWidth(ImGui::GetFontSize() * 10);
    if (ImGui::SliderFloat("azimuth", &v->sun_az, 0.0f, 360.0f, "%.0f")) v->shadow_dirty = true;
    ImGui::SameLine();
    ImGui::SetNextItemWidth(ImGui::GetFontSize() * 10);
    if (ImGui::SliderFloat("elevation", &v->sun_el, 8.0f, 85.0f, "%.0f")) v->shadow_dirty = true;
    ImGui::SetNextItemWidth(ImGui::GetFontSize() * 10);
    ImGui::SliderFloat("shadow", &v->sh_str, 0.0f, 1.0f, "%.2f");
    ImGui::SameLine();
    ImGui::SetNextItemWidth(ImGui::GetFontSize() * 10);
    ImGui::SliderFloat("softness", &v->sh_soft, 0.3f, 8.0f, "%.1f");
    ImGui::SameLine();
    { bool sn = v->sh_snap != 0; if (ImGui::Checkbox("Snapped", &sn)) v->sh_snap = sn; }
    ImGui::SameLine();
    if (ImGui::Button("Sun default")) vx_sun_defaults(v);
    // Performance. The HUD is always available and rides the reload blob; the benchmark is explicit,
    // takes a few minutes, and puts the owner's settings back when it is done.
    ImGui::Separator();
    ImGui::TextUnformatted("Perf HUD");
    for (int m = 0; m < 3; m++) {
        static const char *MODE[3] = { "off", "compact", "full" };
        ImGui::SameLine();
        bool cur = v->perf_hud == m;
        if (cur) ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.2f, 0.55f, 0.3f, 1));
        if (ImGui::Button(MODE[m])) v->perf_hud = m;
        if (cur) ImGui::PopStyleColor();
    }
    ImGui::SameLine();
    if (v->bench_on) {
        if (ImGui::Button("Stop benchmark")) {
            v->bench_on = 0;
            if (v->bench_have_saved) vx_restore(v, &v->bench_saved);
            v->q_pattern = 1.0f; v->q_pcf = 4.0f; v->q_water = 1.0f; v->q_cloud = 1.0f;
        }
    } else if (ImGui::Button("Run benchmark")) {
        vx_bench_start(v, v->map_name);
    }
    // Overdraw: the picture replaced by how many times each pixel of the opaque world was written.
    // Dark warm = 1 (early-Z did its job), bright = the shading a depth prepass would have saved.
    {
        bool od = v->od_view != 0;
        if (ImGui::Checkbox("Overdraw view", &od)) v->od_view = od;
        ImGui::SameLine();
        ImGui::Text("avg %.2f  cutaway on %d/%d chunks", v->od_avg, v->od_chunks_cut, v->chunks_drawn);
    }
    if (ImGui::Button("Print cell")) {
        int x = v->act[0].tx, z = v->act[0].tz;
        unsigned char top = get_blk(v, x * VX_VPC, v->hgt[z][x], z * VX_VPC);
        snprintf(out, cap, "%s: %d,%d height %d, top block %s, %s — %d tris, mesh %.1f ms, reach %s",
                 v->map_name, x, z, v->hgt[z][x], BLOCKS[top].name, v->walk[z][x] ? "walkable" : "blocked",
                 v->tris, v->mesh_ms, v->reach_missing ? "FAIL" : "ok");
        printed = true;
    }
    return printed;
}

VoxField *vx_create() {
    VoxField *v = (VoxField *)calloc(1, sizeof(VoxField));
    snprintf(v->map_name, sizeof(v->map_name), "halm");
    v->exam_trig = v->exam_npc = -1;
    v->want_dir = v->last_axis = -1;
    v->party = 2;
    v->step_len = WALK_STEP;
    v->amb = 1.0f;
    // Octopath, not Minecraft: a low pitched camera, a narrow field of view, the party about a fifth
    // of the screen, and a far fog so the town fades out instead of ending in a cliff.
    v->ortho = 0; v->hd2d = 1; v->cutaway = 1;
    v->pitch = 42.0f; v->fov = 32.0f; v->view_h = 11.0f; v->tilt = 0.68f;
    v->ao_str = 0.85f; v->detail = 1.0f; v->fog = 0.65f;
    v->dof = 0.75f; v->bloom = 0.55f; v->vignette = 0.45f; v->grade = 0.6f;
    v->motes = 0; v->res_pct = 100;
    // the benchmark's knobs, at the shipping look
    v->q_pattern = 1.0f; v->q_pcf = 4.0f; v->q_water = 1.0f; v->q_cloud = 1.0f;
    v->perf_hud = 0;
    v->bench_poll = 0.5f;          // half a period out of phase with map.flag's poll
    for (int i = 0; i < 256; i++) { v->pal[i][0] = (unsigned char)i; v->pal[i][1] = (unsigned char)i; v->pal[i][2] = (unsigned char)i; }
    return v;
}

void vx_destroy(VoxField *v) {
    if (!v) return;
    free(v->cmap_px);
    free(v->spr);
    free(v);
}

// Load every map once, mesh it, and say so. Non-zero means something a capture would have hidden:
// a map that will not build, or a route the tile map has and the voxel world does not.
int vx_selftest(VoxField *v) {
    if (!v->gl_ready) vx_gl_init(v);
    int bad = 0;
    for (int i = 0; i < vx_map_count(); i++) {
        const char *m = vx_map_name_at(i);
        if (!vx_load_map(v, m)) { SDL_Log("SELFCHECK vox: map=%s FAILED TO LOAD", m); bad++; continue; }
        vx_render_shadow(v);
        vx_selfcheck(v, v->fbo_w, v->fbo_h, 0, 0);
        v->selfchecked = true;
        if (v->reach_missing) bad++;
        if (!v->houses && v->place_count) SDL_Log("SELFCHECK vox: map=%s has no buildings", m);
    }
    SDL_Log("SELFCHECK vox: selftest %s (%d map%s bad)", bad ? "FAIL" : "ok", bad, bad == 1 ? "" : "s");
    return bad;
}

void vx_save(VoxField *v, VxSave *s) {
    memset(s, 0, sizeof(*s));
    snprintf(s->map, sizeof(s->map), "%s", v->map_name);
    for (int i = 0; i < VX_PARTY; i++) { s->tx[i] = v->act[i].tx; s->ty[i] = v->act[i].tz; s->facing[i] = v->act[i].facing; }
    s->party = v->party; s->steps = v->steps;
    s->dbg_coord = v->dbg_coord; s->noclip = v->noclip;
    s->ortho = v->ortho; s->hd2d = v->hd2d; s->cutaway = v->cutaway;
    s->pitch = v->pitch; s->fov = v->fov; s->view_h = v->view_h; s->tilt = v->tilt;
    s->ao = v->ao_str; s->detail = v->detail; s->amb = v->amb; s->light_table = v->light_table;
    s->fog = v->fog; s->dof = v->dof; s->bloom = v->bloom; s->vignette = v->vignette; s->grade = v->grade;
    s->motes = v->motes; s->res_scale_pct = v->res_pct;
    s->sun_az = v->sun_az; s->sun_el = v->sun_el;
    s->sh_str = v->sh_str; s->sh_soft = v->sh_soft; s->sh_snap = v->sh_snap;
    s->perf_hud = v->perf_hud;
}

void vx_restore(VoxField *v, const VxSave *s) {
    char map[32];
    snprintf(map, sizeof(map), "%s", s->map[0] ? s->map : "halm");
    for (char *c = map; *c; c++) if (!((*c >= 'a' && *c <= 'z') || (*c >= '0' && *c <= '9') || *c == '_')) { *c = 0; break; }
    // The look knobs first: vx_load_map re-reads the map's own `light:` line, and what the owner was
    // tuning has to win over it, exactly as the tile field does.
    if (s->pitch >= 10.0f && s->pitch <= 85.0f) v->pitch = s->pitch;
    if (s->fov >= 8.0f && s->fov <= 70.0f) v->fov = s->fov;
    if (s->view_h >= 3.0f && s->view_h <= 40.0f) v->view_h = s->view_h;
    if (s->tilt >= 0.0f && s->tilt <= 1.0f) v->tilt = s->tilt;
    if (s->ao >= 0.0f && s->ao <= 1.0f) v->ao_str = s->ao;
    if (s->detail >= 0.0f && s->detail <= 1.0f) v->detail = s->detail;
    if (s->fog >= 0.0f && s->fog <= 1.0f) v->fog = s->fog;
    if (s->dof >= 0.0f && s->dof <= 1.0f) v->dof = s->dof;
    if (s->bloom >= 0.0f && s->bloom <= 2.0f) v->bloom = s->bloom;
    if (s->vignette >= 0.0f && s->vignette <= 1.0f) v->vignette = s->vignette;
    if (s->grade >= 0.0f && s->grade <= 1.0f) v->grade = s->grade;
    v->ortho = s->ortho ? 1 : 0; v->hd2d = s->hd2d ? 1 : 0; v->cutaway = s->cutaway ? 1 : 0;
    v->res_pct = (s->res_scale_pct >= 40 && s->res_scale_pct <= 100) ? s->res_scale_pct : 100;
    v->perf_hud = (s->perf_hud >= 0 && s->perf_hud <= 2) ? s->perf_hud : 0;
    vx_load_map(v, map[0] ? map : "halm");
    v->party = s->party < 1 ? 1 : s->party > VX_PARTY ? VX_PARTY : s->party;
    v->steps = s->steps < 0 ? 0 : s->steps;
    v->dbg_coord = s->dbg_coord ? 1 : 0; v->noclip = s->noclip ? 1 : 0;
    if (s->light_table >= 0 && s->light_table < 8) v->light_table = s->light_table;
    if (s->amb >= 0.0f && s->amb <= 1.0f) v->amb = s->amb;
    // the sun and its shadow, validated: an azimuth or elevation out of range falls back to the
    // light table's own default rather than leaving the map lit from underneath
    if (s->sun_el > 3.0f && s->sun_el < 89.0f && s->sh_str >= 0.0f && s->sh_str <= 1.0f) {
        v->sun_az = s->sun_az; v->sun_el = s->sun_el; v->sh_str = s->sh_str;
        v->sh_soft = (s->sh_soft > 0.0f && s->sh_soft < 12.0f) ? s->sh_soft : 1.6f;
        v->sh_snap = s->sh_snap ? 1 : 0;
    }
    v->shadow_dirty = true;
    v->map_poll = 1.0f;
    v->bench_poll = 0.5f;          // half a period out of phase with map.flag's, so never the same frame
    int lx = s->tx[0], lz = s->ty[0];
    if (lx < 0 || lz < 0 || lx >= v->mw || lz >= v->md) { SDL_Log("voxfield: restored cell %d,%d is off %s", lx, lz, map); return; }
    vx_place_party(v, lx, lz, s->facing[0] & 3);
    for (int i = 1; i < VX_PARTY; i++) {
        int x = s->tx[i], z = s->ty[i];
        if (!vx_can_stand(v, x, z)) continue;
        v->act[i].tx = v->act[i].px = (short)x;
        v->act[i].tz = v->act[i].pz = (short)z;
        v->act[i].y = v->act[i].y0 = vx_gy(v, x, z);
        v->act[i].facing = s->facing[i] & 3;
    }
}
