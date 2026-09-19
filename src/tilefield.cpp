// tilefield.cpp — the field, Sega style. TILES.md is the contract; src/TILEFIELD_NOTES.md says what
// this actually does. The world is 32 LOGICAL px to the tile with a 640x360-class view and grid
// walking; the art is 4x that (128-px atlas cells, 256x384 walker frames) and is drawn into an FBO the
// size of the drawable, linear-filtered, so nothing is thrown away on the way to the screen.
// Nothing here is 3D and nothing here is a navmesh.
#include <SDL3/SDL.h>
#include <math.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <sys/stat.h>
#if defined(__ANDROID__)
#include <GLES3/gl3.h>
#else
#include <OpenGL/gl3.h>
#endif
#include "imgui.h"
#include "stb_image.h"          // implementations live in star_logic.cpp / field.cpp
#include "stb_image_write.h"
#include "tilefield.h"
#include "field_text.h"

#define PREF_ORG "com.playground"
#define PREF_APP "questglory"

#define TS       32              // tile size in LOGICAL pixels; the world, the camera and collision
                                 // are all in these. Art is 4x that (see `cell` below) and drawn at
                                 // the screen's own resolution.
#define VH       360             // internal height, always
#define VW_MIN   640             // TILES.md's 640x360
#define VW_MAX   1024            // filled out to the phone's 20:9 rather than pillar-boxed
#define ATLAS_COLS 16
#define TF_MAXW  96
#define TF_MAXH  96
#define TF_TILES 192             // tile/stamp definitions in one tileset
#define TF_PLACE 768             // object stamps placed on a map
#define TF_TRIGS 128
#define TF_NPCS  48
#define TF_ART   16              // walker sheets cached
#define TF_VERTS 24000
#define TF_LIGHTS 16             // point lights uploaded to the shader; the uniform array's size
#define TF_LEVELS 32             // light levels per colormap table (PALETTE.md)
#define TF_TABLES 8              // colormap tables we have room for

#define WALK_STEP 0.16f          // seconds for one tile, walking
#define RUN_STEP  0.10f
#define TURN_HOLD 0.07f          // a press shorter than this turns in place and does not step
#define FADE_FRAMES 8

// ───────────────────────── little pixel press ─────────────────────────

struct TCol { unsigned char r, g, b, a; };
static TCol tc(int r, int g, int b, int a = 255) {
    TCol c; c.r = (unsigned char)r; c.g = (unsigned char)g; c.b = (unsigned char)b; c.a = (unsigned char)a; return c;
}
// A window onto the atlas: everything a generator draws is in its own 0,0..w,h.
struct Pen { unsigned char *p; int stride, x0, y0, w, h; };

static void pset(Pen *n, int x, int y, TCol c) {
    if (x < 0 || y < 0 || x >= n->w || y >= n->h || c.a == 0) return;
    unsigned char *d = n->p + ((size_t)(n->y0 + y) * n->stride + (n->x0 + x)) * 4;
    if (c.a == 255) { d[0] = c.r; d[1] = c.g; d[2] = c.b; d[3] = 255; return; }
    int a = c.a;
    d[0] = (unsigned char)((c.r * a + d[0] * (255 - a)) / 255);
    d[1] = (unsigned char)((c.g * a + d[1] * (255 - a)) / 255);
    d[2] = (unsigned char)((c.b * a + d[2] * (255 - a)) / 255);
    d[3] = (unsigned char)(a + d[3] * (255 - a) / 255);
}
static void prect(Pen *n, int x0, int y0, int x1, int y1, TCol c) {
    for (int y = y0; y < y1; y++) for (int x = x0; x < x1; x++) pset(n, x, y, c);
}
static void pdisc(Pen *n, float cx, float cy, float rx, float ry, TCol c) {
    for (int y = (int)(cy - ry) - 1; y <= (int)(cy + ry) + 1; y++)
        for (int x = (int)(cx - rx) - 1; x <= (int)(cx + rx) + 1; x++) {
            float dx = (x + 0.5f - cx) / rx, dy = (y + 0.5f - cy) / ry;
            if (dx * dx + dy * dy <= 1.0f) pset(n, x, y, c);
        }
}
static uint32_t thash(uint32_t a, uint32_t b, uint32_t c) {
    uint32_t h = a * 374761393u + b * 668265263u + c * 2246822519u;
    h = (h ^ (h >> 13)) * 1274126177u;
    return h ^ (h >> 16);
}
static float trnd(int a, int b, int c) { return (float)(thash((uint32_t)a, (uint32_t)b, (uint32_t)c) & 0xFFFF) / 65535.0f; }
static uint32_t name_seed(const char *s) {
    uint32_t h = 2166136261u;
    for (; *s; s++) h = (h ^ (unsigned char)*s) * 16777619u;
    return h;
}
static TCol shade(TCol c, float k) {
    int r = (int)(c.r * k), g = (int)(c.g * k), b = (int)(c.b * k);
    return tc(r > 255 ? 255 : r, g > 255 ? 255 : g, b > 255 ? 255 : b, c.a);
}

// ───────────────────────── placeholder art ─────────────────────────
// Every tile the tileset names can be drawn from nothing, so a map is walkable and readable before
// a single sheet comes back from the art pipeline. Keyed by the tile's NAME, which is why the names
// in story/field/tilesets/valley/tiles.md are the art contract and not decoration.

static void gen_ground(Pen *n, TCol base, TCol dark, TCol light, uint32_t seed, int specks) {
    for (int y = 0; y < n->h; y++) for (int x = 0; x < n->w; x++) {
        float v = trnd(x, y, (int)seed);
        TCol c = v < 0.18f ? dark : v > 0.86f ? light : base;
        pset(n, x, y, c);
    }
    for (int i = 0; i < specks; i++) {
        int x = (int)(trnd(i, 7, (int)seed) * n->w), y = (int)(trnd(i, 9, (int)seed) * n->h);
        TCol c = (i & 1) ? light : dark;
        pset(n, x, y, c); pset(n, x, y + 1, c);
    }
}

static void gen_paving(Pen *n, TCol base, uint32_t seed, bool worn) {
    prect(n, 0, 0, n->w, n->h, shade(base, 0.78f));                 // mortar
    for (int gy = 0; gy < 2; gy++) for (int gx = 0; gx < 2; gx++) {
        int off = (gy & 1) ? 4 : 0;
        int x0 = gx * 16 + off + 1, y0 = gy * 16 + 1;
        TCol c = shade(base, 0.88f + trnd(gx, gy, (int)seed) * 0.28f);
        prect(n, x0, y0, x0 + 14, y0 + 14, c);
        prect(n, x0, y0, x0 + 14, y0 + 1, shade(c, 1.12f));         // lit top lip
        if (worn && trnd(gx, gy, (int)seed + 3) > 0.5f)
            pdisc(n, (float)x0 + 7, (float)y0 + 7, 3.0f, 2.0f, shade(c, 0.82f));
    }
}

static void gen_water(Pen *n, int frame) {
    TCol deep = tc(34, 72, 122), mid = tc(48, 96, 150), lit = tc(96, 150, 196);
    gen_ground(n, mid, deep, shade(mid, 1.1f), 11, 0);
    for (int y = 0; y < n->h; y++) {                                // two-frame shimmer: the highlight moves
        int ph = (y / 4 + frame * 2) & 7;
        if (ph == 0 || ph == 5) for (int x = 0; x < n->w; x++)
            if (trnd(x, y, frame * 31 + 5) > 0.55f) pset(n, x, y, lit);
    }
}

// The fringe of terrain U, drawn on top of a cell of another terrain. The convention is fixed in
// story/field/tilesets/README.md and the art has to match it exactly, because the engine only
// rotates: `edge` is a band along the NORTH edge about 10 px deep with a ragged southern boundary;
// `corner_out` is the terrain in the NORTH-EAST corner only, 10 px in from each; `corner_in` is the
// whole tile EXCEPT a ragged bite out of the SOUTH-WEST corner. Everything else is transparent.
static void gen_fringe(Pen *n, const char *what, TCol c, uint32_t seed) {
    TCol lip = shade(c, 0.74f);
    int d[TS], e[TS];
    for (int i = 0; i < TS; i++) {                                   // one ragged profile, reused
        d[i] = 8 + (int)(trnd(i / 4, 0, (int)seed) * 5.0f);          // depth from the north/east
        e[i] = 8 + (int)(trnd(i / 3, 9, (int)seed) * 5.0f);          // depth from the south/west
    }
    if (strstr(what, "corner_in")) {
        for (int y = 0; y < TS; y++) for (int x = 0; x < TS; x++) {
            bool bite = (y >= TS - e[x]) && (x < e[y]);               // the south-west corner only
            if (bite) continue;
            bool lipline = (y == TS - e[x] || x == e[y] - 1) && (y > TS - e[x] - 2 || x < e[y] + 1);
            pset(n, x, y, lipline ? lip : c);
        }
    } else if (strstr(what, "corner_out")) {
        for (int y = 0; y < TS; y++) for (int x = 0; x < TS; x++)
            if (y < d[x] && x >= TS - d[y])                           // the north-east corner only
                pset(n, x, y, (y >= d[x] - 2 || x <= TS - d[y] + 1) ? lip : c);
    } else {
        for (int x = 0; x < TS; x++)                                  // a band along the north edge
            for (int y = 0; y < d[x]; y++) pset(n, x, y, y >= d[x] - 2 ? lip : c);
    }
}

// A building: roof over the top `over` rows plus an eave, plastered wall under it, one door on the
// bottom row and windows where they fit. Everything about it comes from w/h/over and the name's hash.
static void gen_building(Pen *n, int over, uint32_t seed, TCol wall, TCol roof, bool door) {
    int W = n->w, H = n->h;
    int eave = over > 0 ? over * TS + 10 : TS / 2;                 // where the roof stops
    prect(n, 2, eave, W - 2, H, wall);
    prect(n, 2, eave, W - 2, eave + 3, shade(wall, 0.72f));        // shadow under the eave
    prect(n, 2, H - 3, W - 2, H, shade(wall, 0.6f));               // footing
    for (int x = 2; x < W - 2; x += 1) {                           // timber uprights, not on a beat
        if (trnd(x / 7, 1, (int)seed) > 0.55f && (x % 7) == 0)
            prect(n, x, eave + 3, x + 2, H - 3, shade(wall, 0.58f));
    }
    prect(n, 0, 0, W, eave, roof);                                 // roof, eaves overhanging
    for (int y = 0; y < eave; y += 5) prect(n, 0, y, W, y + 1, shade(roof, 0.82f));
    prect(n, 0, 0, W, 3, shade(roof, 1.14f));                      // ridge
    prect(n, 0, eave - 3, W, eave, shade(roof, 0.66f));            // eave board
    int wy = eave + 7;
    for (int x = 6; x + 8 <= W - 6; x += 13) {
        if (wy + 9 > H - 8) break;
        prect(n, x, wy, x + 8, wy + 9, tc(48, 44, 56));
        prect(n, x + 1, wy + 1, x + 7, wy + 8, tc(96, 112, 130));
        prect(n, x + 4, wy, x + 5, wy + 9, tc(48, 44, 56));
    }
    if (door) {
        int dx = W / 2 - 6, dy = H - 18 < eave + 2 ? eave + 2 : H - 18;
        prect(n, dx, dy, dx + 12, H - 2, tc(92, 62, 38));
        prect(n, dx + 1, dy + 1, dx + 11, H - 3, tc(120, 84, 48));
        prect(n, dx + 5, dy + 1, dx + 6, H - 3, tc(92, 62, 38));
        pset(n, dx + 9, dy + 9, tc(220, 200, 120));
    }
}

static void gen_tree(Pen *n, uint32_t seed, TCol leaf, bool bare) {
    int W = n->w, H = n->h;
    int tw = W / 4 < 5 ? 5 : W / 4;
    prect(n, W / 2 - tw / 2, H - TS, W / 2 + tw / 2, H, tc(84, 58, 36));
    prect(n, W / 2 - tw / 2, H - TS, W / 2 - tw / 2 + 2, H, tc(112, 80, 50));
    if (bare) {
        for (int i = 0; i < 6; i++) {
            float a = 0.6f + trnd(i, 1, (int)seed) * 2.0f;
            for (int s = 0; s < 14; s++)
                pset(n, (int)(W / 2 + cosf(a) * s), (int)(H - TS - sinf(a) * s * 0.8f), tc(84, 58, 36));
        }
        return;
    }
    float cy = (float)(H - TS) * 0.46f;
    pdisc(n, W * 0.5f, cy + 4, W * 0.46f, (H - TS) * 0.42f, shade(leaf, 0.70f));
    pdisc(n, W * 0.5f, cy, W * 0.44f, (H - TS) * 0.40f, leaf);
    pdisc(n, W * 0.38f, cy - 3, W * 0.24f, (H - TS) * 0.20f, shade(leaf, 1.20f));
    for (int i = 0; i < 40; i++) {                                 // ragged crown edge
        float a = trnd(i, 3, (int)seed) * 6.283f;
        pset(n, (int)(W * 0.5f + cosf(a) * W * 0.46f), (int)(cy + sinf(a) * (H - TS) * 0.42f), shade(leaf, 0.86f));
    }
}

static void gen_small(Pen *n, const char *name, uint32_t seed) {
    TCol wood = tc(120, 86, 52), wdark = tc(84, 58, 36), stone = tc(150, 146, 138);
    if (strstr(name, "fence") || strstr(name, "wall") || strstr(name, "hedge")) {
        bool ns = strstr(name, "_ns") != nullptr;
        bool hedge = strstr(name, "hedge") != nullptr;
        bool wall = strstr(name, "wall") != nullptr;
        TCol a = hedge ? tc(56, 96, 50) : wall ? stone : wood;
        TCol b = shade(a, hedge ? 1.25f : 0.7f);
        int y0 = hedge ? 6 : 8, th = hedge ? 24 : 18;
        if (strstr(name, "corner")) { prect(n, 8, y0, 24, y0 + th, a); prect(n, 8, y0, 24, y0 + 3, b); return; }
        if (ns) { prect(n, 10, 0, 22, TS, a); prect(n, 10, 0, 13, TS, b); }
        else { prect(n, 0, y0, TS, y0 + th, a); prect(n, 0, y0, TS, y0 + 3, b); }
        if (!hedge && !wall) { prect(n, 2, y0, 5, y0 + th + 4, wdark); prect(n, 26, y0, 29, y0 + th + 4, wdark); }
        return;
    }
    if (strstr(name, "post")) { prect(n, 13, 4, 19, TS, wood); prect(n, 13, 4, 15, TS, wdark); prect(n, 12, 4, 20, 7, wdark); return; }
    if (strstr(name, "barrel")) { pdisc(n, 16, 20, 10, 11, wood); prect(n, 6, 12, 26, 15, wdark); prect(n, 6, 24, 26, 27, wdark); pdisc(n, 16, 11, 9, 4, shade(wood, 1.2f)); return; }
    if (strstr(name, "crate") || strstr(name, "sack")) {
        bool sack = strstr(name, "sack") != nullptr;
        if (sack) { pdisc(n, 16, 21, 11, 10, tc(184, 166, 124)); pdisc(n, 16, 12, 6, 5, tc(160, 142, 104)); }
        else { prect(n, 4, 8, 28, 30, wood); prect(n, 4, 8, 28, 11, shade(wood, 1.2f)); prect(n, 14, 11, 18, 30, wdark); }
        return;
    }
    if (strstr(name, "stone") || strstr(name, "boulder") || strstr(name, "stump")) {
        TCol c = strstr(name, "stump") ? wood : shade(stone, strstr(name, "boulder") ? 0.8f : 1.0f);
        pdisc(n, 16, 20, 11, 9, c); pdisc(n, 15, 17, 8, 6, shade(c, 1.18f));
        return;
    }
    if (strstr(name, "sign")) { prect(n, 14, 14, 18, TS, wdark); prect(n, 4, 4, 28, 16, wood); prect(n, 4, 4, 28, 6, shade(wood, 1.2f)); return; }
    if (strstr(name, "trough")) { prect(n, 2, 12, 30, 28, wdark); prect(n, 4, 15, 28, 25, tc(56, 104, 140)); return; }
    if (strstr(name, "bridge")) {                                   // walkable deck over water
        prect(n, 0, 0, TS, TS, wood);
        for (int y = 0; y < TS; y += 6) prect(n, 0, y, TS, y + 1, wdark);
        prect(n, 0, 0, TS, 3, wdark); prect(n, 0, 29, TS, TS, wdark);
        return;
    }
    if (strstr(name, "gate")) { prect(n, 12, 2, 20, TS, stone); prect(n, 12, 2, 20, 5, shade(stone, 1.2f)); return; }
    pdisc(n, 16, 18, 10, 10, shade(stone, 0.9f));                   // anything unnamed: a lump, visibly a stub
    prect(n, 12, 14, 20, 22, tc(200, 60, 160));
}

static void gen_well(Pen *n) {
    int W = n->w, H = n->h;
    TCol stone = tc(150, 146, 138);
    pdisc(n, W * 0.5f, H * 0.68f, W * 0.42f, H * 0.26f, shade(stone, 0.7f));
    pdisc(n, W * 0.5f, H * 0.62f, W * 0.40f, H * 0.24f, stone);
    pdisc(n, W * 0.5f, H * 0.60f, W * 0.26f, H * 0.15f, tc(26, 34, 48));
    prect(n, W / 2 - 14, 4, W / 2 - 10, (int)(H * 0.60f), tc(110, 78, 46));
    prect(n, W / 2 + 10, 4, W / 2 + 14, (int)(H * 0.60f), tc(110, 78, 46));
    prect(n, 4, 2, W - 4, 8, tc(150, 70, 48));                      // little roof
    prect(n, 4, 2, W - 4, 4, tc(184, 96, 64));
    prect(n, W / 2 - 2, 10, W / 2 + 2, (int)(H * 0.52f), tc(200, 190, 170));   // rope
}

static void gen_cart(Pen *n) {
    int W = n->w, H = n->h;
    TCol wood = tc(120, 86, 52), wdark = tc(84, 58, 36);
    prect(n, 4, H / 2 - 8, W - 4, H / 2 + 6, wood);
    prect(n, 4, H / 2 - 8, W - 4, H / 2 - 5, shade(wood, 1.2f));
    prect(n, 2, H / 2 - 16, 6, H / 2, wdark);
    pdisc(n, W * 0.28f, H * 0.74f, 9, 9, wdark);
    pdisc(n, W * 0.28f, H * 0.74f, 4, 4, wood);
    pdisc(n, W * 0.74f, H * 0.74f, 9, 9, wdark);
    pdisc(n, W * 0.74f, H * 0.74f, 4, 4, wood);
}

static void gen_pile(Pen *n, const char *name, uint32_t seed) {
    int W = n->w, H = n->h;
    if (strstr(name, "hay")) {
        pdisc(n, W * 0.5f, H * 0.66f, W * 0.46f, H * 0.34f, tc(186, 158, 78));
        pdisc(n, W * 0.5f, H * 0.42f, W * 0.34f, H * 0.24f, tc(208, 182, 100));
        for (int i = 0; i < 30; i++)
            pset(n, (int)(trnd(i, 1, (int)seed) * W), (int)(H * 0.3f + trnd(i, 2, (int)seed) * H * 0.6f), tc(150, 124, 60));
        return;
    }
    if (strstr(name, "stall")) {
        prect(n, 2, 2, W - 2, 10, tc(170, 74, 60));
        for (int x = 2; x < W - 2; x += 8) prect(n, x, 2, x + 4, 10, tc(200, 196, 180));
        prect(n, 4, 10, 7, H - 4, tc(110, 78, 46));
        prect(n, W - 7, 10, W - 4, H - 4, tc(110, 78, 46));
        prect(n, 4, H - 16, W - 4, H - 6, tc(120, 86, 52));
        return;
    }
    for (int i = 0; i < 26; i++) {                                  // woodpile: logs seen end on
        float x = 4 + trnd(i, 3, (int)seed) * (W - 12), y = H * 0.35f + trnd(i, 5, (int)seed) * H * 0.55f;
        pdisc(n, x + 3, y + 3, 5, 5, tc(84, 58, 36));
        pdisc(n, x + 3, y + 2, 4, 4, tc(150, 116, 74));
    }
}

// One 32x48 walk sheet's worth of placeholder person, in a colour taken from the id: the leader, the
// follower and every NPC come out different without a single file on disk.
static void gen_walker_sheet(unsigned char *px, int w, int h, const char *id) {
    uint32_t s = name_seed(id);
    TCol coat = tc(70 + (int)(s % 130), 70 + (int)((s >> 8) % 120), 90 + (int)((s >> 16) % 120));
    TCol hair = shade(coat, 0.45f), skin = tc(226, 186, 150);
    memset(px, 0, (size_t)w * h * 4);
    Pen page = { px, w, 0, 0, w, h };
    for (int row = 0; row < 4; row++) for (int col = 0; col < 4; col++) {
        Pen n = { px, w, col * TS, row * 48, TS, 48 };
        int lift = (col == 1) ? 1 : (col == 3) ? -1 : 0;            // stand, step, stand, step
        prect(&n, 12, 30, 15, 44 + lift, shade(coat, 0.6f));        // legs
        prect(&n, 17, 30, 20, 44 - lift, shade(coat, 0.6f));
        prect(&n, 10, 18, 22, 34, coat);                            // body
        prect(&n, 10, 18, 22, 21, shade(coat, 1.2f));
        prect(&n, 12, 8, 20, 18, skin);                             // head
        prect(&n, 11, 5, 21, 11, hair);
        if (row == 0) { pset(&n, 14, 13, tc(30, 26, 30)); pset(&n, 18, 13, tc(30, 26, 30)); }   // S: face
        if (row == 3) prect(&n, 11, 5, 21, 16, hair);                                           // N: back of head
        if (row == 1) { prect(&n, 12, 8, 17, 18, skin); prect(&n, 17, 8, 21, 18, hair); }        // W
        if (row == 2) { prect(&n, 15, 8, 20, 18, skin); prect(&n, 11, 8, 15, 18, hair); }        // E
        prect(&n, 8, 20, 11, 30, coat);                             // arms
        prect(&n, 21, 20, 24, 30, coat);
    }
    (void)page;
}

// ───────────────────────── files ─────────────────────────

static const char *tf_pref() {
    static char path[512];
    if (!path[0]) { const char *p = SDL_GetPrefPath(PREF_ORG, PREF_APP); snprintf(path, sizeof(path), "%s", p ? p : ""); }
    return path;
}
// Phone pref path first (fast_reload.sh pushes there), then the APK assets, then the repo on desktop.
static void *tf_read(const char *rel, size_t *size) {
    char path[768];
    snprintf(path, sizeof(path), "%s%s", tf_pref(), rel);
    void *d = SDL_LoadFile(path, size);
    if (!d) d = SDL_LoadFile(rel, size);
    if (!d) { snprintf(path, sizeof(path), "story/%s", rel); d = SDL_LoadFile(path, size); }
    return d;
}

// ───────────────────────── palette (PALETTE.md / D19) ─────────────────────────
// Every image the field draws is 256 colours, one byte a pixel. The loader maps a decoded RGBA image
// back to palette indices, uploads GL_R8, and the shader does the colour lookup through a COLORMAP —
// one row per (table, light level) — which is what makes night, dusk and a lantern pool free.
//
// Art that has not been through the cutter yet still works: a pixel that matches nothing takes its
// nearest master colour and is counted, so the log says how far off the conversion is.

// Oklab, exactly the transform the tool uses. sRGB in 0..1, linear in between.
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
static void oklab_to_rgb(float L, float A, float B, unsigned char *out) {
    float l = L + 0.3963377774f * A + 0.2158037573f * B;
    float m = L - 0.1055613458f * A - 0.0638541728f * B;
    float s = L - 0.0894841775f * A - 1.2914855480f * B;
    l = l * l * l; m = m * m * m; s = s * s * s;
    float r =  4.0767416621f * l - 3.3077115913f * m + 0.2309699292f * s;
    float g = -1.2684380046f * l + 2.6097574011f * m - 0.3413193965f * s;
    float b = -0.0041960863f * l - 0.7034186147f * m + 1.7076147010f * s;
    out[0] = (unsigned char)(lin_srgb(r) * 255.0f + 0.5f);
    out[1] = (unsigned char)(lin_srgb(g) * 255.0f + 0.5f);
    out[2] = (unsigned char)(lin_srgb(b) * 255.0f + 0.5f);
}

// The tables, in the order their rows sit in the colormap. `day` is first so row 0..31 is the plain
// one, which is also what a lantern pool is lit with.
enum { TBL_DAY = 0, TBL_DUSK, TBL_NIGHT, TBL_FLASH, TBL_POISON, TBL_STONE, TBL_COUNT };
static const char *TBL_NAMES[TBL_COUNT] = { "day", "dusk", "night", "flash", "poison", "stone" };

struct TfLight { float x, y, r, level; int flicker; };   // x,y,r in TILES; level 0..1

// ───────────────────────── data ─────────────────────────

enum { LY_GROUND = 0, LY_OBJECT, LY_OVER };
enum { TG_MESSAGE = 0, TG_EXIT, TG_DOOR, TG_ZONE, TG_TRAP, TG_SCENE, TG_NPC };

struct TfTile {
    char name[24];
    short index, w, h;          // index = atlas cell of the top-left tile; w,h in tiles
    unsigned char layer, over;  // over = how many of the TOP rows draw above the walkers
    unsigned char frames;       // horizontal neighbours in the atlas, animated at ANIM_FPS
    unsigned char fringe;       // 0 = no fringe; otherwise the priority that decides who overlays whom
    unsigned char solid[8];     // bit x of row y
    short fr_edge, fr_out, fr_in;
};

struct TfPlace { short def, x, y; };
struct TfTrig {
    short x, y, w, h;
    int kind;
    char arg[96], map[32];
    short ax, ay, af;
    bool inside;
};
struct TfNpc {
    short hx, hy;                     // home tile, for wander
    short tx, ty, px, py;             // tile and the tile stepped from
    float t, wait;
    int facing, wander, art, parity;
    char walker[24], text[48];
};
struct TfArt { char id[24]; GLuint tex; int w, h, fw, fh; };

struct TfActor { short tx, ty, px, py; int facing; };

// A vertex carries, besides its position and UV, the ATLAS REGION it is allowed to sample (in texels,
// x0,y0,x1,y1 — this replaced the half-texel inset: the shader clamps its four neighbour fetches to
// this rect, so a linear blend can never reach into the next cell) and its light: `lit.x < 0` means
// "work it out per fragment from the point lights", anything else is a fixed level for the whole quad,
// which is how a walker is lit by the light at its feet instead of getting a gradient across the body.
struct TfVert { float x, y, u, v; unsigned char r, g, b, a; float rx0, ry0, rx1, ry1, lit, warm; };

struct TileField {
    // tileset
    char set[24];
    TfTile tiles[TF_TILES];
    int tile_count;
    GLuint atlas;
    int atlas_w, atlas_h, cell;      // cell = atlas pixels per tile (32 placeholder-only, 128 art)

    // map
    char map_name[32], music[16];
    int mw, mh;
    short ground[TF_MAXH][TF_MAXW];
    short place_at[TF_MAXH][TF_MAXW];       // placement index + 1, 0 = nothing
    unsigned char solid[TF_MAXH][TF_MAXW];
    TfPlace places[TF_PLACE];
    int place_count;
    TfTrig trigs[TF_TRIGS];
    int trig_count;
    TfNpc npcs[TF_NPCS];
    int npc_count;
    int spawn_x, spawn_y, spawn_f;

    // party
    TfActor act[TF_PARTY];
    int party;
    bool moving;
    float move_t, step_len;
    int step_parity, steps;
    float hold_t;                            // how long the current direction has been asked for
    int want_dir, last_axis;

    // camera / view
    int cam_x, cam_y, vw, fbo_w, fbo_h;
    float sc;                                 // device pixels per logical pixel (~3.0 on the phone)
    float anim_t;

    // input
    bool stick_on, act_on, tapped, run;
    SDL_FingerID stick_id, act_id;
    float stick_ox, stick_oy, stick_x, stick_y, act_t;

    // dialogue and events
    char msg[320], msg_who[48];
    float msg_t;
    int exam_trig, exam_npc;

    // map change
    char to_map[32];
    int to_x, to_y, to_f, fade, fade_dir;

    // palette and light (PALETTE.md / D19)
    bool pal_ok;
    unsigned char pal[256][3];
    float pal_lab[256][3];                    // the same colours in Oklab, for the nearest-colour search
    GLuint cmap;                              // 256 x (tables*levels), RGBA, GL_NEAREST
    unsigned char *cmap_px;                   // the CPU copy, which is what cycling rewrites
    int cmap_rows, cmap_levels, cmap_tables;
    char cmap_name[TF_TABLES][16];
    int cmap_row0[TF_TABLES];                 // colormap.json's row0 per table
    int light_table;                          // which table the map's ambient is on
    int lamp_table;                           // and which one a point light's pool is lit from
    float ambient;                            // 0..1
    TfLight lights[TF_LIGHTS];
    int light_count;
    bool lantern;                             // the dev "lantern on the player" toggle
    bool one_tap;                             // measurement: nearest index instead of the 4-tap blend
    uint64_t draw_ns;                         // GPU time for the world, summed over the fps window
    float lantern_r;
    struct { unsigned char idx[16]; int n, step; float fps; } cyc[8];
    int cyc_count;
    float cyc_t;
    long off_pal;                             // pixels quantised at load across every file

    // gl
    bool gl_ready;
    GLuint prog, vao, vbo, fbo, fbo_tex, white;
    GLint u_res, u_tex, u_cmap, u_texsz, u_cmaph, u_rowa, u_rowb, u_levels, u_amb,
          u_indexed, u_nl, u_lights, u_sc, u_taps;
    TfVert *v;
    int vn;
    GLuint cur_tex;
    int cur_mode;                             // 0 = plain RGBA texture, 1 = palette index texture

    TfArt art[TF_ART];
    int art_count;

    // capture
    GLuint cap_fbo, cap_tex;
    int cap_w, cap_h, cap_req, cap_scale;
    bool cap_ok, cap_walkers, cap_whole, hide_walkers;
    char cap_out[300];

    // dev
    int dbg_solid, dbg_trig, dbg_coord, noclip;
    char warn[192];
    float map_poll;
};

static const char *TF_MAPS[] = { "halm", "hart_yard", "west_road" };
int tf_map_count() { return (int)(sizeof(TF_MAPS) / sizeof(TF_MAPS[0])); }
const char *tf_map_name_at(int i) { return (i >= 0 && i < tf_map_count()) ? TF_MAPS[i] : "halm"; }

// The one map that is compiled in: if the files are missing entirely there is still somewhere to stand.
static const char *FALLBACK_TMAP =
    "## meta\nname: void\ntileset: valley\nsize: 12 10\nspawn: 6 5 S\n"
    "## legend\n. grass\n# paving\n"
    "## ground\n............\n............\n...#####....\n...#####....\n...#####....\n"
    "...#####....\n...#####....\n............\n............\n............\n"
    "## objects\n............\n............\n............\n............\n............\n"
    "............\n............\n............\n............\n............\n";

// ───────────────────────── palette: load, index, colormap ─────────────────────────

static char *trim(char *s);

// The master palette. `story/palette/master.hex` is the contract; while the cutter is still building
// it, the palette experiment's own output is accepted from tools/palette/, so the engine is never
// blocked on the other half. 256 lines of #rrggbb; index 0 is transparent.
// Every palette colour in Oklab, once, so the nearest-colour search is a dot product per entry
// instead of three cube roots.
static void tf_pal_lab(TileField *t) {
    for (int i = 0; i < 256; i++)
        rgb_to_oklab(t->pal[i][0] / 255.0f, t->pal[i][1] / 255.0f, t->pal[i][2] / 255.0f,
                     &t->pal_lab[i][0], &t->pal_lab[i][1], &t->pal_lab[i][2]);
}

static bool tf_load_palette(TileField *t) {
    size_t sz = 0;
    char *text = (char *)tf_read("palette/master.hex", &sz);
    const char *from = "story/palette/master.hex";
    if (!text) { text = (char *)SDL_LoadFile("tools/palette/master.hex", &sz); from = "tools/palette/master.hex"; }
    if (!text) { text = (char *)SDL_LoadFile("tools/palette/master_ramps.hex", &sz); from = "tools/palette/master_ramps.hex"; }
    if (!text) {
        // The 256x1 PNG is the same palette; accept it so a checkout with only the image still lights.
        size_t isz = 0;
        void *data = tf_read("palette/master.pal.png", &isz);
        if (!data) data = SDL_LoadFile("tools/palette/master.pal.png", &isz);
        if (data) {
            int w = 0, h = 0, c = 0;
            unsigned char *px = stbi_load_from_memory((const unsigned char *)data, (int)isz, &w, &h, &c, 4);
            SDL_free(data);
            if (px && w >= 1 && h >= 1) {
                int n = w < 256 ? w : 256;
                for (int i = 0; i < n; i++) memcpy(t->pal[i], px + (size_t)i * 4, 3);
                stbi_image_free(px);
                t->pal_ok = true;
                tf_pal_lab(t);
                SDL_Log("tilefield: palette from master.pal.png (%d colours)", n);
                return true;
            }
            if (px) stbi_image_free(px);
        }
        SDL_Log("tilefield: NO master palette (story/palette/master.hex) — running unpalettised: "
                "textures stay RGBA and there is no lighting");
        return false;
    }
    int n = 0;
    for (char *c = text; *c && n < 256; c++) {
        if (*c != '#') continue;
        unsigned v = 0;
        if (sscanf(c + 1, "%6x", &v) != 1) continue;
        t->pal[n][0] = (unsigned char)(v >> 16); t->pal[n][1] = (unsigned char)((v >> 8) & 255);
        t->pal[n][2] = (unsigned char)(v & 255);
        n++;
        c += 6;
    }
    SDL_free(text);
    if (n < 2) { SDL_Log("tilefield: %s has %d colours — ignoring it", from, n); return false; }
    for (int i = n; i < 256; i++) { t->pal[i][0] = t->pal[i][1] = t->pal[i][2] = 0; }
    t->pal_ok = true;
    tf_pal_lab(t);
    SDL_Log("tilefield: palette %s — %d colours", from, n);
    return true;
}

// RGBA -> one byte a pixel. Exact match is the normal case (shipped art is already indexed and stb
// expands it); anything else takes the nearest master colour in Oklab and is counted, so not-yet-
// converted art works and says how far off it is. The unique-colour cache is what keeps that fast:
// a 2048x2048 atlas has a few hundred distinct colours, not four million.
// A colour's answer is worked out once and remembered: `val` is the index, `off` whether it had to be
// approximated. An opaque key is never 0, so key == 0 means the slot is free.
// Big enough that converted art (256 colours) never comes near it and un-converted art — which can
// hold tens of thousands — mostly fits. If it ever does fill, the answer is still computed, just not
// remembered: a slow load beats a wrong picture, which is what the first version of this did.
#define TFQ_SLOTS 65536
struct TfQCache { uint32_t key[TFQ_SLOTS]; unsigned char val[TFQ_SLOTS], off[TFQ_SLOTS]; int colours, approx, full; };

static unsigned char tf_quant(TileField *t, TfQCache *q, uint32_t rgba, bool *was_off) {
    *was_off = false;
    if ((rgba >> 24) < 128) return 0;                     // hard alpha: PALETTE.md
    uint32_t k = rgba | 0xFF000000u;
    uint32_t s = ((k * 2654435761u) >> 12) & (TFQ_SLOTS - 1);
    int probe = 0, slot = -1;
    for (; probe < 64; probe++, s = (s + 1) & (TFQ_SLOTS - 1)) {
        if (q->key[s] == k) { *was_off = q->off[s] != 0; return q->val[s]; }
        if (!q->key[s]) { slot = (int)s; break; }
    }
    int r = (int)(k & 255), g = (int)((k >> 8) & 255), b = (int)((k >> 16) & 255);
    int best = 1, off = 0;
    for (int i = 1; i < 256; i++)
        if (t->pal[i][0] == r && t->pal[i][1] == g && t->pal[i][2] == b) { best = i; goto done; }
    {
        float L, A, B2, bd = 1e9f;
        rgb_to_oklab(r / 255.0f, g / 255.0f, b / 255.0f, &L, &A, &B2);
        for (int i = 1; i < 256; i++) {                   // the palette's Oklab is precomputed at load
            float d = (L - t->pal_lab[i][0]) * (L - t->pal_lab[i][0])
                    + (A - t->pal_lab[i][1]) * (A - t->pal_lab[i][1])
                    + (B2 - t->pal_lab[i][2]) * (B2 - t->pal_lab[i][2]);
            if (d < bd) { bd = d; best = i; }
        }
        off = 1;
    }
done:
    if (slot >= 0) {
        q->key[slot] = k; q->val[slot] = (unsigned char)best; q->off[slot] = (unsigned char)off;
        q->colours++;
        if (off) q->approx++;
    } else q->full++;
    *was_off = off != 0;
    return (unsigned char)best;
}

// The whole image. Returns a malloc'd index buffer; `skip` (may be null) is a per-pixel mask of
// regions not to count (the procedural placeholders, which are nearest-mapped on purpose).
static unsigned char *tf_index_image(TileField *t, const char *label, const unsigned char *rgba,
                                     int w, int h, const unsigned char *skip) {
    unsigned char *idx = (unsigned char *)malloc((size_t)w * h);
    if (!idx) return nullptr;
    TfQCache *q = (TfQCache *)calloc(1, sizeof(TfQCache));
    if (!q) { free(idx); return nullptr; }
    long offpx = 0;
    for (size_t i = 0; i < (size_t)w * h; i++) {
        bool off = false;
        idx[i] = tf_quant(t, q, ((const uint32_t *)rgba)[i], &off);
        if (off && !(skip && skip[i])) offpx++;
    }
    int colours = q->colours, approx = q->approx;
    free(q);
    t->off_pal += offpx;
    if (offpx)
        SDL_Log("tilefield: %s — %ld px off-palette (art not converted yet), %d of %d distinct colours approximated",
                label, offpx, approx, colours);
    SDL_Log("tilefield: %s indexed — %dx%d, %.2f MB RGBA -> %.2f MB R8 (saved %.2f MB), %d colours",
            label, w, h, (double)w * h * 4 / (1024 * 1024), (double)w * h / (1024 * 1024),
            (double)w * h * 3 / (1024 * 1024), colours);
    return idx;
}

static void tf_tex_index(GLuint tex, const unsigned char *idx, int w, int h) {
    glBindTexture(GL_TEXTURE_2D, tex);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_R8, w, h, 0, GL_RED, GL_UNSIGNED_BYTE, idx);
    // NEAREST, always: the shader does its own bilinear blend in COLOUR space after the lookup, which
    // is the whole point — filtering indices would average two unrelated palette entries.
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
}

// One colormap row: the palette seen under `table` at light level `k` (0..1). This is the engine's
// stand-in for story/palette/colormap.png — PALETTE.md says the tool owns these numbers, so this
// exists only so night works before the tool ships, and is replaced the moment the file appears.
static void tf_cmap_row(TileField *t, int table, float k, unsigned char *row) {
    for (int i = 0; i < 256; i++) {
        unsigned char *o = row + i * 4;
        if (i == 0) { o[0] = o[1] = o[2] = o[3] = 0; continue; }   // index 0 is transparent, at every level
        float L, A, B;
        rgb_to_oklab(t->pal[i][0] / 255.0f, t->pal[i][1] / 255.0f, t->pal[i][2] / 255.0f, &L, &A, &B);
        float floorL = L < 0.14f ? L : 0.14f;                      // the darkest level still reads
        float nl = floorL + (L - floorL) * k;
        float sat = 0.40f + 0.60f * k;                             // colour drains as the light goes
        float na = A * sat, nb = B * sat;
        float d = 1.0f - k;                                        // how far into the dark we are
        switch (table) {
            case TBL_DAY:    nl = floorL + (L - floorL) * k; na = A * (0.75f + 0.25f * k); nb = B * (0.75f + 0.25f * k); break;
            case TBL_DUSK:   na += 0.035f * d; nb += 0.045f * d; nl *= 0.96f; break;              // amber, warm shadows
            case TBL_NIGHT:  na += 0.030f * d; nb -= 0.075f * d; nl = nl * (0.88f - 0.10f * d); break;  // blue-violet
            case TBL_FLASH:  nl = L + (1.0f - L) * d; na = A * k; nb = B * k; break;             // washes to white
            case TBL_POISON: na -= 0.05f * d; nb += 0.05f * d; nl *= 0.94f; break;               // sick green
            case TBL_STONE:  na = na * 0.15f + 0.004f * d; nb = nb * 0.15f - 0.010f * d; break;  // drained grey
            default: break;
        }
        oklab_to_rgb(nl, na, nb, o);
        o[3] = 255;
    }
}

// story/palette/colormap.png + colormap.json when the tool has written them; the CPU tables above
// otherwise. Either way the engine never computes colour at draw time — it only reads a row.
static void tf_build_colormap(TileField *t) {
    if (!t->pal_ok) return;
    t->cmap_levels = TF_LEVELS;
    t->cmap_tables = TBL_COUNT;
    for (int i = 0; i < TBL_COUNT; i++) snprintf(t->cmap_name[i], sizeof(t->cmap_name[i]), "%s", TBL_NAMES[i]);
    const char *how = "synthesised on the CPU (no story/palette/colormap.png yet)";

    size_t isz = 0;
    void *data = tf_read("palette/colormap.png", &isz);
    unsigned char *file = nullptr;
    int fw = 0, fh = 0;
    if (data) {
        int c = 0;
        file = stbi_load_from_memory((const unsigned char *)data, (int)isz, &fw, &fh, &c, 4);
        SDL_free(data);
        if (file && fw != 256) { SDL_Log("tilefield: colormap.png is %d wide, not 256 — ignored", fw); stbi_image_free(file); file = nullptr; }
    }
    if (file) {
        // colormap.json, as the tool writes it:
        //   {"levels": 32, "tables": [{"table":"day","row0":0,"levels":32}, ...]}
        // and its rows run FROM full light DOWN: level 0 is the brightest row, 31 the darkest. The
        // engine's 0..1 light is turned into that row the same way whether the file or the built-in
        // tables are in use, so there is only one convention to get wrong.
        size_t jsz = 0;
        char *js = (char *)tf_read("palette/colormap.json", &jsz);
        if (js) {
            const char *lv = strstr(js, "\"levels\"");
            int n = 0;
            if (lv && sscanf(lv + 8, " : %d", &n) == 1 && n >= 2 && n <= 256) t->cmap_levels = n;
            int k = 0;
            for (const char *c = strstr(js, "\"table\""); c && k < TF_TABLES; c = strstr(c + 1, "\"table\"")) {
                const char *q = strchr(c + 7, '"');
                const char *e = q ? strchr(q + 1, '"') : nullptr;
                if (!q || !e) break;
                int len = (int)(e - q - 1);
                if (len > 15) len = 15;
                memcpy(t->cmap_name[k], q + 1, (size_t)len);
                t->cmap_name[k][len] = 0;
                const char *r = strstr(e, "\"row0\"");
                int row0 = k * t->cmap_levels;
                if (r) sscanf(r + 6, " : %d", &row0);
                t->cmap_row0[k] = row0;
                k++;
            }
            if (k) t->cmap_tables = k;
            SDL_free(js);
        }
        t->cmap_rows = fh;
        free(t->cmap_px);
        t->cmap_px = (unsigned char *)malloc((size_t)256 * fh * 4);
        memcpy(t->cmap_px, file, (size_t)256 * fh * 4);
        stbi_image_free(file);
        // Index 0 is transparent at every light level (PALETTE.md). The tool writes the colormap as an
        // opaque picture — column 0 comes back as opaque black — and an opaque index 0 draws every
        // sprite and every stamp inside a black rectangle.
        for (int r = 0; r < fh; r++) memset(t->cmap_px + (size_t)r * 256 * 4, 0, 4);
        how = "story/palette/colormap.png";
    } else {
        t->cmap_rows = t->cmap_tables * t->cmap_levels;
        free(t->cmap_px);
        t->cmap_px = (unsigned char *)malloc((size_t)256 * t->cmap_rows * 4);
        if (!t->cmap_px) return;
        for (int tb = 0; tb < t->cmap_tables; tb++) {
            t->cmap_row0[tb] = tb * t->cmap_levels;
            for (int l = 0; l < t->cmap_levels; l++)                      // row 0 = full light, as the file does
                tf_cmap_row(t, tb, 1.0f - (float)l / (float)(t->cmap_levels - 1),
                            t->cmap_px + ((size_t)(tb * t->cmap_levels + l) * 256) * 4);
        }
    }
    if (!t->cmap) glGenTextures(1, &t->cmap);
    glBindTexture(GL_TEXTURE_2D, t->cmap);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 256, t->cmap_rows, 0, GL_RGBA, GL_UNSIGNED_BYTE, t->cmap_px);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glBindTexture(GL_TEXTURE_2D, 0);
    t->lamp_table = 0;                                   // `day` unless the tool ships a warm `lamp` one
    for (int i = 0; i < t->cmap_tables && i < TF_TABLES; i++)
        if (!SDL_strcasecmp(t->cmap_name[i], "lamp") || !SDL_strcasecmp(t->cmap_name[i], "lantern")) t->lamp_table = i;
    SDL_Log("tilefield: colormap %s — %d tables x %d levels = %d rows (%.0f KB), lamp pools lit from \"%s\"",
            how, t->cmap_tables, t->cmap_levels, t->cmap_rows, (double)256 * t->cmap_rows * 4 / 1024,
            t->cmap_name[t->lamp_table]);
}

// story/palette/cycles.md: `name: i1 i2 i3 ... @ fps`. Cycling is a rewrite of those COLUMNS of the
// colormap, every row at once — the indices are the same whatever the light is.
static void tf_load_cycles(TileField *t) {
    t->cyc_count = 0;
    size_t sz = 0;
    char *text = (char *)tf_read("palette/cycles.md", &sz);
    if (!text) return;
    char *save = nullptr;
    for (char *line = SDL_strtok_r(text, "\n", &save); line; line = SDL_strtok_r(nullptr, "\n", &save)) {
        char *s = trim(line);
        char *colon = strchr(s, ':');
        if (!colon || s[0] == '#' || t->cyc_count >= 8) continue;
        char *at = strchr(colon, '@');
        float fps = 6.0f;
        if (at) { fps = (float)atof(at + 1); *at = 0; }
        int n = 0;
        unsigned char idx[16];
        for (char *c = colon + 1; *c && n < 16;) {
            while (*c == ' ' || *c == ',') c++;
            if (*c < '0' || *c > '9') { if (*c) c++; continue; }
            idx[n++] = (unsigned char)atoi(c);
            while (*c >= '0' && *c <= '9') c++;
        }
        if (n < 2) continue;
        memcpy(t->cyc[t->cyc_count].idx, idx, (size_t)n);
        t->cyc[t->cyc_count].n = n;
        t->cyc[t->cyc_count].fps = fps > 0.1f ? fps : 6.0f;
        t->cyc[t->cyc_count].step = 0;
        t->cyc_count++;
    }
    SDL_free(text);
    if (t->cyc_count) SDL_Log("tilefield: %d palette cycle(s) from cycles.md", t->cyc_count);
}

// One frame of cycling: rotate each declared run of indices and re-upload just those columns, every
// row, with one glTexSubImage2D per cycle. A few texels wide by a couple of hundred rows.
static void tf_cycle(TileField *t, float dt) {
    if (!t->cyc_count || !t->cmap_px || !t->cmap) return;
    t->cyc_t += dt;
    for (int c = 0; c < t->cyc_count; c++) {
        int step = (int)(t->cyc_t * t->cyc[c].fps) % t->cyc[c].n;
        if (step == t->cyc[c].step) continue;
        t->cyc[c].step = step;
        int lo = 255, hi = 0;
        for (int i = 0; i < t->cyc[c].n; i++) {
            int v = t->cyc[c].idx[i];
            if (v < lo) lo = v;
            if (v > hi) hi = v;
        }
        int w = hi - lo + 1;
        unsigned char *buf = (unsigned char *)malloc((size_t)w * t->cmap_rows * 4);
        if (!buf) return;
        for (int r = 0; r < t->cmap_rows; r++) {
            memcpy(buf + (size_t)r * w * 4, t->cmap_px + ((size_t)r * 256 + lo) * 4, (size_t)w * 4);
            for (int i = 0; i < t->cyc[c].n; i++) {
                int dst = t->cyc[c].idx[i], src = t->cyc[c].idx[(i + step) % t->cyc[c].n];
                memcpy(buf + ((size_t)r * w + (dst - lo)) * 4, t->cmap_px + ((size_t)r * 256 + src) * 4, 4);
            }
        }
        glBindTexture(GL_TEXTURE_2D, t->cmap);
        glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
        glTexSubImage2D(GL_TEXTURE_2D, 0, lo, 0, w, t->cmap_rows, GL_RGBA, GL_UNSIGNED_BYTE, buf);
        free(buf);
    }
}

// ───────────────────────── tileset ─────────────────────────

// GL_MAX_TEXTURE_SIZE, read once the context exists. 0 means "not asked yet"; every driver this ships
// on reports at least 4096, but an atlas that is one pixel over the limit uploads as nothing at all,
// which on a phone looks like a black map and no error.
static int tf_max_tex = 0;
static void tf_probe_limits() {
    if (tf_max_tex) return;
    GLint m = 0;
    glGetIntegerv(GL_MAX_TEXTURE_SIZE, &m);
    tf_max_tex = m > 0 ? (int)m : 2048;
    SDL_Log("tilefield: GL_MAX_TEXTURE_SIZE %d", tf_max_tex);
}

// Box-average an RGBA image to half size, in place of the original buffer (which it frees).
static unsigned char *tf_halve(unsigned char *px, int *w, int *h) {
    int nw = *w / 2, nh = *h / 2;
    if (nw < 1 || nh < 1) { free(px); return nullptr; }
    unsigned char *out = (unsigned char *)malloc((size_t)nw * nh * 4);
    if (!out) { free(px); return nullptr; }
    for (int y = 0; y < nh; y++) for (int x = 0; x < nw; x++) {
        for (int c = 0; c < 4; c++) {
            const unsigned char *s = px + ((size_t)(y * 2) * *w + x * 2) * 4 + c;
            out[((size_t)y * nw + x) * 4 + c] = (unsigned char)((s[0] + s[4] + s[*w * 4] + s[*w * 4 + 4]) / 4);
        }
    }
    free(px);
    *w = nw; *h = nh;
    return out;
}

static int tile_by_name(TileField *t, const char *name) {
    for (int i = 0; i < t->tile_count; i++) if (!strcmp(t->tiles[i].name, name)) return i;
    return -1;
}

static char *trim(char *s) {
    while (*s == ' ' || *s == '\t') s++;
    char *e = s + strlen(s);
    while (e > s && (e[-1] == ' ' || e[-1] == '\t' || e[-1] == '\r' || e[-1] == '\n')) *--e = 0;
    return s;
}

// `- solid: yes | no | ###/#.#/...`  — one row of #/. per tile row, slash separated. TILES.md writes
// the rows on their own lines; one line keeps the parser to ten lines and the file readable.
static void parse_solid(TfTile *d, const char *val) {
    memset(d->solid, 0, sizeof(d->solid));
    if (!strcmp(val, "no")) return;
    if (!strcmp(val, "yes")) {
        for (int y = 0; y < d->h && y < 8; y++) d->solid[y] = (unsigned char)((1 << d->w) - 1);
        return;
    }
    int row = 0, col = 0;
    for (const char *c = val; *c && row < 8; c++) {
        if (*c == '/') { row++; col = 0; continue; }
        if (*c == ' ') continue;
        if (*c == '#' && col < 8) d->solid[row] |= (unsigned char)(1 << col);
        col++;
    }
}

// One frame of a placeholder, always drawn at 32 px to the tile: the generators are written in those
// numbers and nothing is gained by teaching them a second scale.
static void gen_stub_frame(Pen *pen, TfTile *d, int fr) {
    {
        Pen n = *pen;
        const char *nm = d->name;
        uint32_t seed = name_seed(nm);
        if (strstr(nm, "_edge") || strstr(nm, "_corner")) {
            TCol c = strstr(nm, "water") ? tc(112, 96, 74) : strstr(nm, "paving") ? tc(150, 146, 138)
                   : strstr(nm, "crop") ? tc(168, 150, 70) : tc(140, 108, 72);
            gen_fringe(&n, nm, c, seed);
        } else if (!strcmp(nm, "water") || !strcmp(nm, "water_deep")) {
            gen_water(&n, fr);
            if (!strcmp(nm, "water_deep")) for (int y = 0; y < TS; y++) for (int x = 0; x < TS; x++) pset(&n, x, y, tc(24, 52, 96, 90));
        } else if (strstr(nm, "paving")) {
            gen_paving(&n, tc(150, 146, 138), seed, strstr(nm, "worn") != nullptr);
        } else if (!strcmp(nm, "gravel")) {
            gen_ground(&n, tc(132, 126, 114), tc(104, 100, 92), tc(164, 158, 146), seed, 40);
        } else if (strstr(nm, "dirt") || !strcmp(nm, "mud")) {
            TCol b = strcmp(nm, "mud") ? tc(140, 108, 72) : tc(104, 84, 60);
            gen_ground(&n, b, shade(b, 0.8f), shade(b, 1.16f), seed, 14);
            if (strstr(nm, "rut")) { prect(&n, 6, 0, 9, TS, shade(b, 0.72f)); prect(&n, 21, 0, 24, TS, shade(b, 0.72f)); }
        } else if (!strcmp(nm, "crop")) {
            gen_ground(&n, tc(150, 132, 66), tc(118, 104, 50), tc(186, 166, 88), seed, 0);
            for (int x = 1; x < TS; x += 5) for (int y = 0; y < TS; y += 3)
                prect(&n, x, y, x + 2, y + 2, tc(188, 168, 90));
        } else if (strstr(nm, "grass")) {
            gen_ground(&n, tc(74, 122, 58), tc(56, 100, 46), tc(98, 144, 70), seed, 18);
            if (strstr(nm, "tuft")) for (int i = 0; i < 9; i++) {
                int x = (int)(trnd(i, 1, (int)seed) * 28) + 2, y = (int)(trnd(i, 2, (int)seed) * 24) + 4;
                prect(&n, x, y, x + 1, y + 5, tc(108, 156, 74));
            }
            if (strstr(nm, "flower")) for (int i = 0; i < 6; i++) {
                int x = (int)(trnd(i, 4, (int)seed) * 28) + 2, y = (int)(trnd(i, 5, (int)seed) * 26) + 2;
                pset(&n, x, y, tc(230, 220, 150)); pset(&n, x + 1, y, tc(230, 200, 120));
            }
        } else if (strstr(nm, "tree")) {
            TCol leaf = strstr(nm, "_2") ? tc(58, 104, 52) : tc(70, 124, 60);
            gen_tree(&n, seed, leaf, strstr(nm, "bare") != nullptr);
        } else if (strstr(nm, "well")) {
            gen_well(&n);
        } else if (strstr(nm, "cart")) {
            gen_cart(&n);
        } else if (strstr(nm, "woodpile") || strstr(nm, "haystack") || strstr(nm, "stall")) {
            gen_pile(&n, nm, seed);
        } else if (d->w >= 3 && d->h >= 3) {
            TCol wall = tc(180 + (int)(seed % 40), 168 + (int)((seed >> 4) % 30), 140 + (int)((seed >> 8) % 40));
            TCol roof = tc(150 + (int)((seed >> 12) % 40), 70 + (int)((seed >> 16) % 30), 50);
            if (strstr(nm, "shed") || strstr(nm, "barn") || strstr(nm, "stable")) { wall = tc(126, 94, 60); roof = tc(110, 92, 62); }
            gen_building(&n, d->over, seed, wall, roof, true);
        } else {
            gen_small(&n, nm, seed);
        }
    }
}

// The placeholder, nearest-upscaled into the atlas's own cell size, so a 128-px atlas with half its
// cells still empty is a normal working state and the generated cells simply look like big pixels.
static void gen_into_atlas(TileField *t, TfTile *d, unsigned char *px, int aw) {
    int cell = t->cell;
    int frames = d->frames > 1 ? d->frames : 1;
    int sw = d->w * TS, sh = d->h * TS;
    unsigned char *scratch = (unsigned char *)calloc((size_t)sw * sh * 4, 1);
    if (!scratch) return;
    for (int fr = 0; fr < frames; fr++) {
        memset(scratch, 0, (size_t)sw * sh * 4);
        Pen n = { scratch, sw, 0, 0, sw, sh };
        gen_stub_frame(&n, d, fr);
        int cx = (d->index % ATLAS_COLS) * cell + fr * d->w * cell, cy = (d->index / ATLAS_COLS) * cell;
        int k = cell / TS < 1 ? 1 : cell / TS;
        for (int y = 0; y < sh * k; y++) for (int x = 0; x < sw * k; x++) {
            const unsigned char *s = scratch + ((size_t)(y / k) * sw + (x / k)) * 4;
            unsigned char *o = px + ((size_t)(cy + y) * aw + (cx + x)) * 4;
            o[0] = s[0]; o[1] = s[1]; o[2] = s[2]; o[3] = s[3];
        }
    }
    free(scratch);
}

static bool load_tileset(TileField *t, const char *set) {
    if (t->atlas && !strcmp(t->set, set)) return true;
    snprintf(t->set, sizeof(t->set), "%s", set);
    t->tile_count = 0;

    char rel[128];
    snprintf(rel, sizeof(rel), "field/tilesets/%s/tiles.md", set);
    size_t sz = 0;
    char *text = (char *)tf_read(rel, &sz);
    if (!text) { SDL_Log("tilefield: no tileset \"%s\" (tiles.md); every tile will be a stub", set); return false; }

    TfTile *d = nullptr;
    char *save = nullptr;
    for (char *line = SDL_strtok_r(text, "\n", &save); line; line = SDL_strtok_r(nullptr, "\n", &save)) {
        char *s = trim(line);
        if (s[0] == '#' && s[1] == '#') {
            if (t->tile_count >= TF_TILES) { SDL_Log("tilefield: more than %d tiles in %s", TF_TILES, set); break; }
            d = &t->tiles[t->tile_count++];
            memset(d, 0, sizeof(*d));
            snprintf(d->name, sizeof(d->name), "%s", trim(s + 2));
            d->w = d->h = 1; d->frames = 1; d->layer = LY_GROUND;
            d->fr_edge = d->fr_out = d->fr_in = -1;
            continue;
        }
        if (!d || s[0] != '-') continue;
        char *val = strchr(s, ':');
        if (!val) continue;
        *val = 0;
        char *key = trim(s + 1);
        val = trim(val + 1);
        if (!strcmp(key, "index")) {
            int idx = 0, w = 1, h = 1;
            if (sscanf(val, "%d %dx%d", &idx, &w, &h) < 3) { sscanf(val, "%d", &idx); w = h = 1; }
            d->index = (short)idx; d->w = (short)(w < 1 ? 1 : w > 8 ? 8 : w); d->h = (short)(h < 1 ? 1 : h > 8 ? 8 : h);
        } else if (!strcmp(key, "layer")) {
            d->layer = (unsigned char)(!strcmp(val, "over") ? LY_OVER : !strcmp(val, "object") ? LY_OBJECT : LY_GROUND);
        } else if (!strcmp(key, "solid")) {
            parse_solid(d, val);
        } else if (!strcmp(key, "over")) {
            d->over = (unsigned char)atoi(val);
        } else if (!strcmp(key, "frames")) {
            d->frames = (unsigned char)(atoi(val) < 1 ? 1 : atoi(val));
        } else if (!strcmp(key, "fringe")) {
            d->fringe = (unsigned char)atoi(val);
        }
    }
    SDL_free(text);

    for (int i = 0; i < t->tile_count; i++) {             // a fringed terrain names its three overlays
        TfTile *e = &t->tiles[i];
        if (!e->fringe) continue;
        char nm[40];
        snprintf(nm, sizeof(nm), "%s_edge", e->name);        e->fr_edge = (short)tile_by_name(t, nm);
        snprintf(nm, sizeof(nm), "%s_corner_out", e->name);  e->fr_out  = (short)tile_by_name(t, nm);
        snprintf(nm, sizeof(nm), "%s_corner_in", e->name);   e->fr_in   = (short)tile_by_name(t, nm);
        // all three missing is deliberate — the priority alone says "nothing overlays me"; a partial
        // set is an authoring mistake and worth a line in the log.
        if (e->fr_edge < 0 && (e->fr_out >= 0 || e->fr_in >= 0))
            SDL_Log("tilefield: \"%s\" has corner fringes but no %s_edge", e->name, e->name);
    }

    // How tall the atlas has to be for what the file asked for; a supplied atlas.png is pasted into
    // that, so an art sheet that only covers the first rows still works.
    int rows = 1;
    for (int i = 0; i < t->tile_count; i++) {
        TfTile *e = &t->tiles[i];
        int r = e->index / ATLAS_COLS + e->h;
        if (r > rows) rows = r;
    }
    // `cell` is how many atlas pixels one tile is drawn at: 128 for the art contract, 32 when there is
    // no atlas.json at all, which is the placeholder-only path and still works exactly as it did.
    t->cell = 32;
    snprintf(rel, sizeof(rel), "field/tilesets/%s/atlas.json", set);
    sz = 0;
    char *meta = (char *)tf_read(rel, &sz);
    if (meta) {
        const char *k = strstr(meta, "\"cell\"");
        int c = 0;
        if (k && sscanf(k + 6, " : %d", &c) == 1 && c >= 8 && c <= 512) t->cell = c;
        SDL_free(meta);
    }
    int cell = t->cell;
    int aw = ATLAS_COLS * cell, ah = rows * cell;
    unsigned char *px = (unsigned char *)calloc((size_t)aw * ah * 4, 1);
    if (!px) return false;

    snprintf(rel, sizeof(rel), "field/tilesets/%s/atlas.png", set);
    sz = 0;
    void *data = tf_read(rel, &sz);
    int have = 0;
    if (data) {
        int iw = 0, ih = 0, ic = 0;
        unsigned char *img = stbi_load_from_memory((const unsigned char *)data, (int)sz, &iw, &ih, &ic, 4);
        SDL_free(data);
        if (img) {
            if (iw != aw) SDL_Log("tilefield: atlas.png is %d wide, the tiles.md + cell %d want %d — pasted from the left",
                                  iw, cell, aw);
            for (int y = 0; y < ih && y < ah; y++)
                memcpy(px + (size_t)y * aw * 4, img + (size_t)y * iw * 4, (size_t)(iw < aw ? iw : aw) * 4);
            stbi_image_free(img);
            have = 1;
        }
    }

    // Any tile whose cell is empty gets its placeholder painted straight into the atlas, so half a
    // sheet of real art and half placeholders is a normal, working state. The placeholders are drawn
    // in RGB and then snapped to the nearest master colours with everything else, which is how they
    // come out in palette indices too — `stub_mask` keeps them out of the off-palette count, since
    // being approximated is what they are FOR.
    int stubs = 0;
    unsigned char *stub_mask = t->pal_ok ? (unsigned char *)calloc((size_t)aw * ah, 1) : nullptr;
    for (int i = 0; i < t->tile_count; i++) {
        TfTile *e = &t->tiles[i];
        int cx = (e->index % ATLAS_COLS) * cell, cy = (e->index / ATLAS_COLS) * cell;
        bool empty = true;
        if (have) {
            for (int y = cy; y < cy + e->h * cell && empty; y++)
                for (int x = cx; x < cx + e->w * cell && empty; x++)
                    if (x < aw && y < ah && px[((size_t)y * aw + x) * 4 + 3]) empty = false;
        }
        if (!empty) continue;
        gen_into_atlas(t, e, px, aw);
        if (stub_mask) {
            int fw = e->w * cell * (e->frames > 1 ? e->frames : 1);
            for (int y = cy; y < cy + e->h * cell && y < ah; y++)
                for (int x = cx; x < cx + fw && x < aw; x++) stub_mask[(size_t)y * aw + x] = 1;
        }
        stubs++;
    }
    SDL_Log("tilefield: tileset %s — %d tiles, %d placeholders, cell %d, atlas %dx%d (%.1f MB)%s",
            set, t->tile_count, stubs, cell, aw, ah, (double)aw * ah * 4 / (1024 * 1024), have ? "" : " (no atlas.png)");

    while (tf_max_tex > 0 && (aw > tf_max_tex || ah > tf_max_tex)) {          // never silently fail to upload
        SDL_Log("tilefield: atlas %dx%d is over GL_MAX_TEXTURE_SIZE %d — HALVING; the art will be soft",
                aw, ah, tf_max_tex);
        px = tf_halve(px, &aw, &ah);
        free(stub_mask); stub_mask = nullptr;
        t->cell = cell = cell / 2;
        if (!px) return false;
    }

    if (t->atlas) glDeleteTextures(1, &t->atlas);
    glGenTextures(1, &t->atlas);
    if (t->pal_ok) {
        unsigned char *idx = tf_index_image(t, "atlas.png", px, aw, ah, stub_mask);
        if (idx) { tf_tex_index(t->atlas, idx, aw, ah); free(idx); }
        else t->pal_ok = false;
    }
    if (!t->pal_ok) {
        glBindTexture(GL_TEXTURE_2D, t->atlas);
        glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, aw, ah, 0, GL_RGBA, GL_UNSIGNED_BYTE, px);
        // Unpalettised fallback: linear both ways, as it was before D19. The art is 4x the logical
        // grid, so every tile is MINIFIED about 0.75x and nearest would alias it into mush.
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    }
    free(stub_mask);
    t->atlas_w = aw; t->atlas_h = ah;
    free(px);
    return true;
}

// ───────────────────────── walker art ─────────────────────────

static int art_get(TileField *t, const char *id) {
    for (int i = 0; i < t->art_count; i++) if (!strcmp(t->art[i].id, id)) return i;
    if (t->art_count >= TF_ART) return -1;
    TfArt *a = &t->art[t->art_count];
    snprintf(a->id, sizeof(a->id), "%s", id);
    char rel[128];
    snprintf(rel, sizeof(rel), "field/walkers/%s.png", id);
    size_t sz = 0;
    void *data = tf_read(rel, &sz);
    unsigned char *px = nullptr;
    int w = TS * 4, h = 48 * 4;
    const char *why = "no file";
    if (data) {
        int iw = 0, ih = 0, ic = 0;
        px = stbi_load_from_memory((const unsigned char *)data, (int)sz, &iw, &ih, &ic, 4);
        SDL_free(data);
        if (!px) why = "unreadable png";
        // Any 4x4 grid is legal: the frame is sheet/4 whatever the sheet's size (128x192 and the
        // 384x576 hi-res contract both land here). Only a size that cannot be a 4x4 grid is rejected.
        else if (iw < 4 || ih < 4 || (iw & 3) || (ih & 3)) {
            why = "not a 4x4 grid";
            SDL_Log("tilefield: walker \"%s\" is %dx%d, which is not four frames by four — using the placeholder", id, iw, ih);
            stbi_image_free(px); px = nullptr;
        } else { w = iw; h = ih; why = "file"; }
    }
    bool own = false;
    if (!px) { px = (unsigned char *)malloc((size_t)w * h * 4); own = true; gen_walker_sheet(px, w, h, id); }
    tf_probe_limits();
    while (tf_max_tex > 0 && (w > tf_max_tex || h > tf_max_tex)) {
        SDL_Log("tilefield: walker \"%s\" sheet %dx%d is over GL_MAX_TEXTURE_SIZE %d — halving", id, w, h, tf_max_tex);
        unsigned char *copy = px;
        if (!own) { copy = (unsigned char *)malloc((size_t)w * h * 4); memcpy(copy, px, (size_t)w * h * 4); stbi_image_free(px); own = true; }
        px = tf_halve(copy, &w, &h);
        if (!px) return -1;
    }
    int clear = 0;
    for (size_t i = 3; i < (size_t)w * h * 4; i += 4) if (px[i] < 128) clear++;
    SDL_Log("tilefield: walker \"%s\" — %s, sheet %dx%d, frame %dx%d, %d%% transparent%s",
            id, why, w, h, w / 4, h / 4, clear * 100 / (w * h), clear ? "" : "  <-- NO TRANSPARENCY");
    glGenTextures(1, &a->tex);
    unsigned char *widx = nullptr;
    if (t->pal_ok) {
        char label[64];
        snprintf(label, sizeof(label), "walker %s.png", id);
        widx = tf_index_image(t, label, px, w, h, nullptr);
    }
    if (widx) { tf_tex_index(a->tex, widx, w, h); free(widx); }
    else {
        glBindTexture(GL_TEXTURE_2D, a->tex);
        glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, w, h, 0, GL_RGBA, GL_UNSIGNED_BYTE, px);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);    // 256x384 frames drawn at
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);    // ~96x144: minified, so linear
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    }
    if (own) free(px); else stbi_image_free(px);
    a->w = w; a->h = h; a->fw = w / 4; a->fh = h / 4;
    return t->art_count++;
}

// ───────────────────────── map ─────────────────────────

static bool cell_solid(TileField *t, int x, int y) {
    if (x < 0 || y < 0 || x >= t->mw || y >= t->mh) return true;
    return t->solid[y][x] != 0;
}

static int npc_at(TileField *t, int x, int y) {
    for (int i = 0; i < t->npc_count; i++) if (t->npcs[i].tx == x && t->npcs[i].ty == y) return i;
    return -1;
}

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

static int facing_of(const char *s) { return s[0] == 'W' ? 1 : s[0] == 'E' ? 2 : s[0] == 'N' ? 3 : 0; }
static const char *FACE_NAME[4] = { "S", "W", "E", "N" };
static const int DX[4] = { 0, -1, 1, 0 }, DY[4] = { 1, 0, 0, -1 };

static void stamp_place(TileField *t, int def, int x, int y) {
    TfTile *d = &t->tiles[def];
    for (int r = 0; r < d->h; r++) for (int c = 0; c < d->w; c++) {
        int mx = x + c, my = y + r;
        if (mx >= t->mw || my >= t->mh) { SDL_Log("tilefield: %s at %d,%d runs off the map", d->name, x, y); return; }
        if (t->place_at[my][mx]) {
            SDL_Log("tilefield: %s at %d,%d overlaps %s — skipped", d->name, x, y,
                    t->tiles[t->places[t->place_at[my][mx] - 1].def].name);
            return;
        }
    }
    if (t->place_count >= TF_PLACE) return;
    TfPlace *p = &t->places[t->place_count++];
    p->def = (short)def; p->x = (short)x; p->y = (short)y;
    for (int r = 0; r < d->h; r++) for (int c = 0; c < d->w; c++) {
        t->place_at[y + r][x + c] = (short)t->place_count;
        if (d->solid[r] & (1 << c)) t->solid[y + r][x + c] = 1;
    }
}

static void tf_place_party(TileField *t, int x, int y, int facing) {
    if (!t->noclip && cell_solid(t, x, y)) {              // never spawn inside a wall: walk out to the nearest free tile
        int bx = x, by = y, best = 1 << 30;
        for (int sy = 0; sy < t->mh; sy++) for (int sx = 0; sx < t->mw; sx++) {
            if (cell_solid(t, sx, sy)) continue;
            int d = (sx - x) * (sx - x) + (sy - y) * (sy - y);
            if (d < best) { best = d; bx = sx; by = sy; }
        }
        SDL_Log("tilefield: spawn %d,%d on %s is solid — moved to %d,%d", x, y, t->map_name, bx, by);
        x = bx; y = by;
    }
    for (int i = 0; i < TF_PARTY; i++) {
        t->act[i].tx = t->act[i].px = (short)x;
        t->act[i].ty = t->act[i].py = (short)y;
        t->act[i].facing = facing;
    }
    t->moving = false; t->move_t = 0;
}

static int tf_table_by_name(TileField *t, const char *name) {
    for (int i = 0; i < t->cmap_tables && i < TF_TABLES; i++)
        if (!SDL_strcasecmp(t->cmap_name[i], name)) return i;
    for (int i = 0; i < TBL_COUNT; i++) if (!SDL_strcasecmp(TBL_NAMES[i], name)) return i;
    SDL_Log("tilefield: no light table \"%s\" — using day", name);
    return TBL_DAY;
}

static void parse_tmap(TileField *t, char *text) {
    char legend[128][24];
    memset(legend, 0, sizeof(legend));
    int sec = -1, row = 0;                       // 0 meta, 1 legend, 2 ground, 3 objects, 4 triggers
    char *save = nullptr;
    // The map body is column-significant, so trim only the right side of a row.
    for (char *line = SDL_strtok_r(text, "\n", &save); line; line = SDL_strtok_r(nullptr, "\n", &save)) {
        char *e = line + strlen(line);
        while (e > line && (e[-1] == '\r' || e[-1] == ' ')) *--e = 0;
        if (line[0] == '#' && line[1] == '#') {
            char *s = trim(line + 2);
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
            char *key = trim(line);
            val = trim(val + 1);
            if (!strcmp(key, "name")) snprintf(t->map_name, sizeof(t->map_name), "%s", val);
            else if (!strcmp(key, "tileset")) load_tileset(t, val);
            else if (!strcmp(key, "music")) snprintf(t->music, sizeof(t->music), "%s", val);
            else if (!strcmp(key, "size")) {
                int w = 0, h = 0;
                sscanf(val, "%d %d", &w, &h);
                t->mw = w < 1 ? 1 : w > TF_MAXW ? TF_MAXW : w;
                t->mh = h < 1 ? 1 : h > TF_MAXH ? TF_MAXH : h;
            } else if (!strcmp(key, "light")) {
                // `light: <table> <level 0..1>` — the map's ambient (PALETTE.md). Default day 1.0.
                char tb[16] = "day";
                float lv = 1.0f;
                sscanf(val, "%15s %f", tb, &lv);
                t->light_table = tf_table_by_name(t, tb);
                t->ambient = lv < 0 ? 0 : lv > 1 ? 1 : lv;
            } else if (!strcmp(key, "lamp")) {
                // `lamp: x y radius level [flicker]` — a point light, in TILES. It lives in `## meta`
                // rather than `## triggers` because `story_prompt.py tmap check` only knows the six
                // trigger kinds and would reject a seventh; meta keys it does not know it ignores.
                float x = 0, y = 0, r = 3, lv = 1;
                char fl[16] = "";
                if (sscanf(val, "%f %f %f %f %15s", &x, &y, &r, &lv, fl) >= 4 && t->light_count < TF_LIGHTS) {
                    TfLight *l = &t->lights[t->light_count++];
                    l->x = x; l->y = y; l->r = r < 0.5f ? 0.5f : r;
                    l->level = lv < 0 ? 0 : lv > 1 ? 1 : lv;
                    l->flicker = !strcmp(fl, "flicker");
                }
            } else if (!strcmp(key, "spawn")) {
                char f[8] = "S";
                sscanf(val, "%d %d %7s", &t->spawn_x, &t->spawn_y, f);
                t->spawn_f = facing_of(f);
            }
        } else if (sec == 1) {
            char c = line[0];
            char *nm = trim(line + 1);
            if (c && nm[0]) snprintf(legend[(unsigned char)c & 127], 24, "%s", nm);
        } else if (sec == 2 || sec == 3) {
            if (row >= t->mh) { row++; continue; }
            for (int x = 0; x < t->mw; x++) {
                char c = line[x];
                if (!c) break;
                if (c == '+') continue;                        // the rest of a stamp's footprint
                if (c == '.' && sec == 3) continue;            // objects: '.' is nothing, whatever '.' means below
                const char *nm = legend[(unsigned char)c & 127];
                if (!nm[0]) { SDL_Log("tilefield: %s row %d col %d: legend has no '%c'", t->map_name, row, x, c); continue; }
                int def = tile_by_name(t, nm);
                if (def < 0) { SDL_Log("tilefield: legend '%c' names \"%s\", which the tileset has not", c, nm); continue; }
                if (sec == 3 && t->tiles[def].layer == LY_GROUND) {
                    SDL_Log("tilefield: '%c' (%s) is a ground tile and cannot go in ## objects", c, nm);
                    continue;
                }
                if (sec == 2) {
                    t->ground[row][x] = (short)def;
                    if (t->tiles[def].solid[0] & 1) t->solid[row][x] = 1;
                } else {
                    stamp_place(t, def, x, row);
                }
            }
            row++;
        } else if (sec == 4) {
            char work[256];
            snprintf(work, sizeof(work), "%s", line);
            char *w[12];
            int n = split_words(work, w, 12);
            if (n < 5) continue;
            if (t->trig_count >= TF_TRIGS) continue;
            TfTrig *g = &t->trigs[t->trig_count];
            memset(g, 0, sizeof(*g));
            g->x = (short)atoi(w[0]); g->y = (short)atoi(w[1]);
            g->w = (short)(atoi(w[2]) < 1 ? 1 : atoi(w[2])); g->h = (short)(atoi(w[3]) < 1 ? 1 : atoi(w[3]));
            const char *k = w[4];
            if (!strcmp(k, "npc")) {
                if (t->npc_count >= TF_NPCS || n < 8) continue;
                TfNpc *np = &t->npcs[t->npc_count++];
                memset(np, 0, sizeof(*np));
                np->tx = np->px = np->hx = g->x; np->ty = np->py = np->hy = g->y;
                snprintf(np->walker, sizeof(np->walker), "%s", w[5]);
                np->facing = facing_of(w[6]);
                snprintf(np->text, sizeof(np->text), "%s", w[7]);
                if (n >= 10 && !strcmp(w[8], "wander")) np->wander = atoi(w[9]);
                np->art = art_get(t, np->walker);
                np->wait = 1.0f + trnd(t->npc_count, 3, 7) * 2.0f;
                t->solid[np->ty][np->tx] = 1;                 // an NPC holds its tile
                continue;
            }
            if (!strcmp(k, "light")) {
                // `x y 1 1 light <radius> <level> [flicker]`. Accepted here so a map can carry its
                // lamps as triggers the day story_prompt.py's TRIGGER_KINDS learns the word; until
                // then `tmap check` rejects it, which is why halm uses the `lamp:` meta line.
                if (t->light_count < TF_LIGHTS && n >= 7) {
                    TfLight *l = &t->lights[t->light_count++];
                    l->x = (float)g->x; l->y = (float)g->y;
                    l->r = (float)atof(w[5]); if (l->r < 0.5f) l->r = 0.5f;
                    l->level = (float)atof(w[6]);
                    if (l->level < 0) l->level = 0;
                    if (l->level > 1) l->level = 1;
                    l->flicker = n >= 8 && !strcmp(w[7], "flicker");
                }
                continue;
            }
            if (!strcmp(k, "message")) { g->kind = TG_MESSAGE; snprintf(g->arg, sizeof(g->arg), "%s", n > 5 ? w[5] : ""); }
            else if (!strcmp(k, "zone")) { g->kind = TG_ZONE; snprintf(g->arg, sizeof(g->arg), "%s", n > 5 ? w[5] : ""); }
            else if (!strcmp(k, "trap")) { g->kind = TG_TRAP; snprintf(g->arg, sizeof(g->arg), "%s", n > 5 ? w[5] : ""); }
            else if (!strcmp(k, "scene")) { g->kind = TG_SCENE; snprintf(g->arg, sizeof(g->arg), "%s", n > 5 ? w[5] : ""); }
            else if (!strcmp(k, "exit") || !strcmp(k, "door")) {
                if (n < 9) { SDL_Log("tilefield: %s trigger needs <map> <x> <y> <facing>", k); continue; }
                g->kind = !strcmp(k, "exit") ? TG_EXIT : TG_DOOR;
                snprintf(g->map, sizeof(g->map), "%s", w[5]);
                g->ax = (short)atoi(w[6]); g->ay = (short)atoi(w[7]); g->af = (short)facing_of(w[8]);
            } else { SDL_Log("tilefield: unknown trigger kind \"%s\"", k); continue; }
            t->trig_count++;
        }
    }
    if (row && sec >= 2 && row != t->mh) SDL_Log("tilefield: %s: last section had %d rows, size says %d", t->map_name, row, t->mh);
}

bool tf_load_map(TileField *t, const char *name) {
    char rel[128];
    snprintf(rel, sizeof(rel), "field/tmaps/%s.tmap", name);
    size_t sz = 0;
    char *text = (char *)tf_read(rel, &sz);
    bool ok = text != nullptr;
    if (!ok) {
        SDL_Log("tilefield: no map \"%s\" — falling back to the built-in", name);
        text = (char *)malloc(strlen(FALLBACK_TMAP) + 1);
        strcpy(text, FALLBACK_TMAP);
    }

    memset(t->ground, 0, sizeof(t->ground));
    memset(t->place_at, 0, sizeof(t->place_at));
    memset(t->solid, 0, sizeof(t->solid));
    t->place_count = t->trig_count = t->npc_count = 0;
    t->light_count = 0;                                       // the map's own lamps and ambient
    t->light_table = TBL_DAY;
    t->ambient = 1.0f;
    t->mw = t->mh = 1;
    t->spawn_x = t->spawn_y = 0; t->spawn_f = 0;
    t->music[0] = 0;
    snprintf(t->map_name, sizeof(t->map_name), "%s", name);
    t->msg[0] = 0; t->msg_who[0] = 0;
    t->exam_trig = t->exam_npc = -1;

    parse_tmap(t, text);
    if (ok) SDL_free(text); else free(text);
    snprintf(t->map_name, sizeof(t->map_name), "%s", name);   // the file's `name:` is a label; the file is the truth

    t->party = 2;                                             // the party the intro leaves us with
    tf_place_party(t, t->spawn_x, t->spawn_y, t->spawn_f);
    t->cam_x = t->cam_y = -100000;                            // snap the camera on the first frame
    SDL_Log("tilefield: loaded %s %dx%d — %d stamps, %d triggers, %d npcs, spawn %d,%d %s, light %s %.2f, %d lamp(s)",
            name, t->mw, t->mh, t->place_count, t->trig_count, t->npc_count,
            t->act[0].tx, t->act[0].ty, FACE_NAME[t->spawn_f & 3],
            TBL_NAMES[t->light_table >= 0 && t->light_table < TBL_COUNT ? t->light_table : 0],
            t->ambient, t->light_count);
    return ok;
}

// ───────────────────────── gl ─────────────────────────

// Texel arithmetic on a 2048-px atlas needs more than mediump's ~10 bits of mantissa, so this module
// asks for highp explicitly; every phone this ships on supports it in the fragment stage.
#if defined(__ANDROID__)
static const char *TF_PREFIX = "#version 300 es\nprecision highp float;\nprecision highp sampler2D;\n";
#else
static const char *TF_PREFIX = "#version 330 core\n";
#endif
static const char *TF_VS =
    "layout(location=0) in vec2 a_pos;\n"
    "layout(location=1) in vec2 a_uv;\n"
    "layout(location=2) in vec4 a_col;\n"
    "layout(location=3) in vec4 a_rect;\n"
    "layout(location=4) in vec2 a_lit;\n"
    "uniform vec2 u_res;\n"
    "uniform float u_sc;\n"
    "out vec2 v_uv; out vec4 v_col; out vec4 v_rect; out vec2 v_lit; out vec2 v_world;\n"
    "void main(){ v_uv = a_uv; v_col = a_col; v_rect = a_rect; v_lit = a_lit;\n"
    "  v_world = a_pos / u_sc;\n"                    // back to LOGICAL view pixels, which is where lights live
    "  vec2 p = a_pos / u_res * 2.0 - 1.0;\n"
    "  gl_Position = vec4(p.x, -p.y, 0.0, 1.0); }\n";

// The palette fragment shader (PALETTE.md). Four neighbouring INDICES are fetched, each looked up in
// the colormap at this fragment's light row, and the four COLOURS are blended bilinearly with
// premultiplied alpha — which is what keeps full-resolution art smooth under minification while a
// hard alpha edge still renders soft. The neighbour fetches are clamped to the quad's own atlas
// region, so linear can never reach into the next cell: that replaces the old half-texel inset.
static const char *TF_FS =
    "in vec2 v_uv; in vec4 v_col; in vec4 v_rect; in vec2 v_lit; in vec2 v_world;\n"
    "uniform sampler2D u_tex;\n"
    "uniform sampler2D u_cmap;\n"
    "uniform vec2 u_texsz;\n"
    "uniform float u_cmaph, u_rowa, u_rowb, u_levels, u_amb;\n"
    "uniform int u_indexed, u_nl, u_taps;\n"
    "uniform vec4 u_lights[16];\n"
    "out vec4 o;\n"
    "vec4 look(float idx, float row){\n"
    "  vec4 c = texture(u_cmap, vec2((idx + 0.5) / 256.0, (row + 0.5) / u_cmaph));\n"
    "  return vec4(c.rgb * c.a, c.a); }\n"                 // premultiplied; index 0 is a straight zero
    "vec4 tap(ivec2 p, float ra0, float ra1, float rb0, float rb1, float fr, float w){\n"
    "  float idx = texelFetch(u_tex, p, 0).r * 255.0;\n"
    "  vec4 a = mix(look(idx, ra0), look(idx, ra1), fr);\n"
    "  if (w <= 0.001) return a;\n"
    "  vec4 b = mix(look(idx, rb0), look(idx, rb1), fr);\n"
    "  return mix(a, b, w); }\n"
    "void main(){\n"
    "  if (u_indexed == 0) {\n"
    "    vec4 t = texture(u_tex, v_uv) * v_col;\n"
    "    if (t.a < 0.01) discard;\n"
    "    o = vec4(t.rgb * t.a, t.a); return; }\n"
    "  float lamp = 0.0;\n"
    "  if (v_lit.x < 0.0) {\n"
    "    for (int i = 0; i < u_nl; i++) {\n"
    "      vec4 L = u_lights[i];\n"
    "      float f = 1.0 - clamp(length(v_world - L.xy) / max(L.z, 0.001), 0.0, 1.0);\n"
    "      lamp += L.w * f * f; }\n"
    "    lamp = min(lamp, 1.0); }\n"
    "  else lamp = v_lit.x;\n"
    "  float warm = v_lit.x < 0.0 ? clamp((lamp - u_amb) * 1.6, 0.0, 1.0) : v_lit.y;\n"
    "  float lv = max(u_amb, lamp);\n"
    "  float f = (1.0 - lv) * (u_levels - 1.0);\n"      // row 0 is FULL light; the last row is darkest

    "  float r0 = floor(f), fr = f - r0;\n"
    "  float r1 = min(r0 + 1.0, u_levels - 1.0);\n"
    "  float ra0 = u_rowa + r0, ra1 = u_rowa + r1, rb0 = u_rowb + r0, rb1 = u_rowb + r1;\n"
    "  vec2 p = v_uv * u_texsz - 0.5;\n"
    "  vec2 b = floor(p), g = p - b;\n"
    "  vec2 lo = v_rect.xy, hi = v_rect.zw - 1.0;\n"
    "  ivec2 q0 = ivec2(clamp(b, lo, hi)), q1 = ivec2(clamp(b + 1.0, lo, hi));\n"
    "  if (u_taps == 1) {\n"                      // the measured fallback: nearest index, one lookup
    "    ivec2 q = ivec2(clamp(floor(v_uv * u_texsz), lo, hi));\n"
    "    vec4 n = tap(q, ra0, ra1, rb0, rb1, fr, warm);\n"
    "    if (n.a < 0.01) discard;\n"
    "    o = n * vec4(v_col.rgb * v_col.a, v_col.a); return; }\n"
    "  vec4 c00 = tap(ivec2(q0.x, q0.y), ra0, ra1, rb0, rb1, fr, warm);\n"
    "  vec4 c10 = tap(ivec2(q1.x, q0.y), ra0, ra1, rb0, rb1, fr, warm);\n"
    "  vec4 c01 = tap(ivec2(q0.x, q1.y), ra0, ra1, rb0, rb1, fr, warm);\n"
    "  vec4 c11 = tap(ivec2(q1.x, q1.y), ra0, ra1, rb0, rb1, fr, warm);\n"
    "  vec4 c = mix(mix(c00, c10, g.x), mix(c01, c11, g.x), g.y);\n"
    "  if (c.a < 0.01) discard;\n"
    "  o = c * vec4(v_col.rgb * v_col.a, v_col.a);\n"     // a tint, if a caller ever wants one
    "}\n";

static GLuint tf_shader(GLenum type, const char *src) {
    GLuint s = glCreateShader(type);
    const char *parts[2] = { TF_PREFIX, src };
    glShaderSource(s, 2, parts, nullptr);
    glCompileShader(s);
    GLint ok = 0;
    glGetShaderiv(s, GL_COMPILE_STATUS, &ok);
    if (!ok) { char log[1024] = ""; glGetShaderInfoLog(s, sizeof(log) - 1, nullptr, log); SDL_Log("tilefield shader: %s", log); }
    return s;
}

// Built on the first frame and again after every hot reload; the old .so's GL objects leak, which is
// accepted here as it is in field.cpp (CLAUDE.md: never dlclose on Android).
static void tf_gl_init(TileField *t) {
    tf_probe_limits();
    GLuint vs = tf_shader(GL_VERTEX_SHADER, TF_VS), fs = tf_shader(GL_FRAGMENT_SHADER, TF_FS);
    t->prog = glCreateProgram();
    glAttachShader(t->prog, vs); glAttachShader(t->prog, fs);
    glLinkProgram(t->prog);
    GLint ok = 0;
    glGetProgramiv(t->prog, GL_LINK_STATUS, &ok);
    if (!ok) { char log[1024]; glGetProgramInfoLog(t->prog, sizeof(log), nullptr, log); SDL_Log("tilefield link: %s", log); }
    glDeleteShader(vs); glDeleteShader(fs);
    t->u_res = glGetUniformLocation(t->prog, "u_res");
    t->u_tex = glGetUniformLocation(t->prog, "u_tex");
    t->u_cmap = glGetUniformLocation(t->prog, "u_cmap");
    t->u_texsz = glGetUniformLocation(t->prog, "u_texsz");
    t->u_cmaph = glGetUniformLocation(t->prog, "u_cmaph");
    t->u_rowa = glGetUniformLocation(t->prog, "u_rowa");
    t->u_rowb = glGetUniformLocation(t->prog, "u_rowb");
    t->u_levels = glGetUniformLocation(t->prog, "u_levels");
    t->u_amb = glGetUniformLocation(t->prog, "u_amb");
    t->u_indexed = glGetUniformLocation(t->prog, "u_indexed");
    t->u_nl = glGetUniformLocation(t->prog, "u_nl");
    t->u_lights = glGetUniformLocation(t->prog, "u_lights");
    t->u_sc = glGetUniformLocation(t->prog, "u_sc");
    t->u_taps = glGetUniformLocation(t->prog, "u_taps");

    glGenVertexArrays(1, &t->vao);
    glGenBuffers(1, &t->vbo);
    t->v = (TfVert *)malloc(sizeof(TfVert) * TF_VERTS);

    unsigned char w[4] = { 255, 255, 255, 255 };
    glGenTextures(1, &t->white);
    glBindTexture(GL_TEXTURE_2D, t->white);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 1, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE, w);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

    glGenTextures(1, &t->fbo_tex);
    glGenFramebuffers(1, &t->fbo);
    t->fbo_w = t->fbo_h = 0;
    glBindTexture(GL_TEXTURE_2D, 0);

    // The palette comes first: load_tileset and art_get both need it to know whether to index.
    tf_load_palette(t);
    tf_build_colormap(t);
    tf_load_cycles(t);
    t->gl_ready = true;
}

// The view's FBO, in DEVICE pixels: logical 640x360-class times the view scale. Reallocated only when
// that size changes (a rotation, or a different screen), so the normal frame allocates nothing.
static void tf_fbo_size(TileField *t, int w, int h) {
    if (t->fbo_w == w && t->fbo_h == h) return;
    glBindTexture(GL_TEXTURE_2D, t->fbo_tex);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, w, h, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);       // nearest all the way out
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glBindFramebuffer(GL_FRAMEBUFFER, t->fbo);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, t->fbo_tex, 0);
    GLenum st = glCheckFramebufferStatus(GL_FRAMEBUFFER);
    if (st != GL_FRAMEBUFFER_COMPLETE) SDL_Log("tilefield: FBO incomplete 0x%x", st);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glBindTexture(GL_TEXTURE_2D, 0);
    t->fbo_w = w; t->fbo_h = h;
    SDL_Log("tilefield: view FBO %dx%d device px", w, h);
}

// ───────────────────────── the batch ─────────────────────────

static void tf_flush(TileField *t) {
    if (!t->vn) return;
    glBindVertexArray(t->vao);
    glBindBuffer(GL_ARRAY_BUFFER, t->vbo);
    glBufferData(GL_ARRAY_BUFFER, (GLsizeiptr)(sizeof(TfVert) * t->vn), t->v, GL_STREAM_DRAW);
    glEnableVertexAttribArray(0); glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, sizeof(TfVert), (void *)0);
    glEnableVertexAttribArray(1); glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, sizeof(TfVert), (void *)8);
    glEnableVertexAttribArray(2); glVertexAttribPointer(2, 4, GL_UNSIGNED_BYTE, GL_TRUE, sizeof(TfVert), (void *)16);
    glEnableVertexAttribArray(3); glVertexAttribPointer(3, 4, GL_FLOAT, GL_FALSE, sizeof(TfVert), (void *)20);
    glEnableVertexAttribArray(4); glVertexAttribPointer(4, 2, GL_FLOAT, GL_FALSE, sizeof(TfVert), (void *)36);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, t->cur_tex);
    glUniform1i(t->u_indexed, t->cur_mode);
    if (t->cur_mode) {
        float tw = 1, th = 1;
        if (t->cur_tex == t->atlas) { tw = (float)t->atlas_w; th = (float)t->atlas_h; }
        else for (int i = 0; i < t->art_count; i++)
            if (t->art[i].tex == t->cur_tex) { tw = (float)t->art[i].w; th = (float)t->art[i].h; break; }
        glUniform2f(t->u_texsz, tw, th);
    }
    glDrawArrays(GL_TRIANGLES, 0, t->vn);
    t->vn = 0;
}

static void tf_use(TileField *t, GLuint tex, int mode) {
    if (t->cur_tex != tex || t->cur_mode != mode) { tf_flush(t); t->cur_tex = tex; t->cur_mode = mode; }
}

// One quad. `rot` turns the source image 90 degrees clockwise per step, which is how three fringe
// tiles cover twelve cases. Colour is a straight multiply, used for tints and the debug overlays.
// `rect` is the texel box the shader may sample inside (null = the whole texture); `lit` is the
// fixed light level for the quad, or -1 for "per fragment".
static void push_quad(TileField *t, float x, float y, float w, float h,
                      float u0, float v0, float u1, float v1, int rot, unsigned int col,
                      const float *rect, float lit, float warm) {
    if (t->vn + 6 > TF_VERTS) tf_flush(t);
    float uvx[4] = { u0, u1, u1, u0 }, uvy[4] = { v0, v0, v1, v1 };
    float px[4] = { x, x + w, x + w, x }, py[4] = { y, y, y + h, y + h };
    unsigned char r = (unsigned char)(col & 255), g = (unsigned char)((col >> 8) & 255),
                  b = (unsigned char)((col >> 16) & 255), a = (unsigned char)((col >> 24) & 255);
    static const float FULL[4] = { 0, 0, 1e6f, 1e6f };
    if (!rect) rect = FULL;
    const int order[6] = { 0, 1, 2, 0, 2, 3 };
    for (int i = 0; i < 6; i++) {
        int k = order[i], s = (k + 4 - rot) & 3;
        TfVert *o = &t->v[t->vn++];
        o->x = px[k]; o->y = py[k]; o->u = uvx[s]; o->v = uvy[s];
        o->r = r; o->g = g; o->b = b; o->a = a;
        o->rx0 = rect[0]; o->ry0 = rect[1]; o->rx1 = rect[2]; o->ry1 = rect[3];
        o->lit = lit; o->warm = warm;
    }
}

#define TF_WHITE 0xFFFFFFFFu

// One atlas cell (or a w x h block starting at `index`) at screen pixel x,y.
// Logical pixel to device pixel, rounded. Every quad's edges go through this, so two ground tiles that
// share a logical edge share the same device edge exactly: no crack, no overlap, whatever the scale.
static float dev(TileField *t, float logical) { return floorf(logical * t->sc + 0.5f); }

// One atlas cell (or a cw x ch block starting at `index`) at LOGICAL pixel x,y. The UVs are inset by
// half a texel on every side: with linear filtering a UV sitting exactly on a cell boundary blends in
// the neighbouring cell, which reads as a seam of the wrong tile along every edge.
// With the palette on there is no inset at all: the UVs are the cell's exact bounds and the shader is
// told the cell's texel box, which it clamps its four taps to. That is stricter than the inset was —
// the inset only moved the sample point, the clamp makes reaching out of the cell impossible.
static void push_cell(TileField *t, int index, int cw, int ch, float x, float y, int rot, unsigned int col) {
    float aw = (float)t->atlas_w, ah = (float)t->atlas_h;
    int cell = t->cell;
    float tx0 = (float)((index % ATLAS_COLS) * cell), ty0 = (float)((index / ATLAS_COLS) * cell);
    float tx1 = tx0 + (float)(cw * cell), ty1 = ty0 + (float)(ch * cell);
    float x0 = dev(t, x), y0 = dev(t, y), x1 = dev(t, x + cw * TS), y1 = dev(t, y + ch * TS);
    if (t->pal_ok) {
        float rect[4] = { tx0, ty0, tx1, ty1 };
        push_quad(t, x0, y0, x1 - x0, y1 - y0, tx0 / aw, ty0 / ah, tx1 / aw, ty1 / ah, rot, col, rect, -1.0f, 0.0f);
        return;
    }
    float hu = 0.5f / aw, hv = 0.5f / ah;
    push_quad(t, x0, y0, x1 - x0, y1 - y0,
              tx0 / aw + hu, ty0 / ah + hv, tx1 / aw - hu, ty1 / ah - hv, rot, col, nullptr, -1.0f, 0.0f);
}

static void push_solid(TileField *t, float x, float y, float w, float h, unsigned int col) {
    tf_use(t, t->white, 0);
    float x0 = dev(t, x), y0 = dev(t, y);
    push_quad(t, x0, y0, dev(t, x + w) - x0, dev(t, y + h) - y0, 0.05f, 0.05f, 0.95f, 0.95f, 0, col, nullptr, 1.0f, 0.0f);
}

// ───────────────────────── render ─────────────────────────

#define ANIM_FPS 3.0f

static void draw_ground(TileField *t, int x0, int y0, int x1, int y1) {
    tf_use(t, t->atlas, t->pal_ok ? 1 : 0);
    int frame = (int)(t->anim_t * ANIM_FPS);
    for (int y = y0; y < y1; y++) for (int x = x0; x < x1; x++) {
        TfTile *d = &t->tiles[t->ground[y][x]];
        int idx = d->index + (d->frames > 1 ? (frame % d->frames) : 0);
        push_cell(t, idx, 1, 1, (float)(x * TS - t->cam_x), (float)(y * TS - t->cam_y), 0, TF_WHITE);
    }
}

// The neighbour test. A terrain with `fringe: n` overlays every ground cell of lower priority it
// touches: four edges, four outer corners (diagonal only) and four inner corners (both flanks).
static void draw_fringes(TileField *t, int x0, int y0, int x1, int y1) {
    tf_use(t, t->atlas, t->pal_ok ? 1 : 0);
    const int cdx[4] = { 0, 1, 0, -1 }, cdy[4] = { -1, 0, 1, 0 };          // N E S W, matching rot 0..3
    for (int y = y0; y < y1; y++) for (int x = x0; x < x1; x++) {
        TfTile *self = &t->tiles[t->ground[y][x]];
        float sx = (float)(x * TS - t->cam_x), sy = (float)(y * TS - t->cam_y);
        for (int pri = 1; pri < 8; pri++) {   // one pass per priority, low first, so two fringes stack sanely
            int who = -1;
            for (int d = 0; d < 8 && who < 0; d++) {
                int nx = x + (d < 4 ? cdx[d] : cdx[d & 3] + cdx[(d + 1) & 3]);
                int ny = y + (d < 4 ? cdy[d] : cdy[d & 3] + cdy[(d + 1) & 3]);
                if (nx < 0 || ny < 0 || nx >= t->mw || ny >= t->mh) continue;
                TfTile *o = &t->tiles[t->ground[ny][nx]];
                if (o->fringe == pri && o != self && o->fringe > self->fringe) who = t->ground[ny][nx];
            }
            if (who < 0) continue;
            TfTile *u = &t->tiles[who];
            bool card[4];
            for (int d = 0; d < 4; d++) {
                int nx = x + cdx[d], ny = y + cdy[d];
                card[d] = (nx >= 0 && ny >= 0 && nx < t->mw && ny < t->mh) && t->tiles[t->ground[ny][nx]].fringe == pri;
                if (card[d] && u->fr_edge >= 0) push_cell(t, t->tiles[u->fr_edge].index, 1, 1, sx, sy, d, TF_WHITE);
            }
            for (int g = 0; g < 4; g++) {                                  // diagonals: NE SE SW NW
                int a = g, b = (g + 1) & 3;                                // the two cardinals flanking it
                int nx = x + cdx[a] + cdx[b], ny = y + cdy[a] + cdy[b];
                bool diag = (nx >= 0 && ny >= 0 && nx < t->mw && ny < t->mh) && t->tiles[t->ground[ny][nx]].fringe == pri;
                if (card[a] && card[b]) { if (u->fr_in >= 0) push_cell(t, t->tiles[u->fr_in].index, 1, 1, sx, sy, g, TF_WHITE); }
                else if (diag && !card[a] && !card[b]) { if (u->fr_out >= 0) push_cell(t, t->tiles[u->fr_out].index, 1, 1, sx, sy, g, TF_WHITE); }
            }
        }
    }
}

// Stamps. `over` is how many of a stamp's TOP rows belong above the walkers, so a roof ridge or a
// tree crown hides whoever walks behind it; everything else is drawn under them.
static void draw_stamps(TileField *t, int x0, int y0, int x1, int y1, bool over_pass) {
    tf_use(t, t->atlas, t->pal_ok ? 1 : 0);
    for (int i = 0; i < t->place_count; i++) {
        TfPlace *p = &t->places[i];
        TfTile *d = &t->tiles[p->def];
        if (p->x >= x1 || p->y >= y1 || p->x + d->w <= x0 || p->y + d->h <= y0) continue;
        int split = d->layer == LY_OVER ? d->h : d->over;                  // whole stamp over, or its top rows
        int r0 = over_pass ? 0 : split, r1 = over_pass ? split : d->h;
        if (r1 <= r0) continue;
        // ONE quad for the whole contiguous block of rows: a row-per-quad stamp shows a hairline seam
        // between its rows once the art is filtered, and a house is one picture, not a stack of strips.
        push_cell(t, d->index + r0 * ATLAS_COLS, d->w, r1 - r0,
                  (float)(p->x * TS - t->cam_x), (float)((p->y + r0) * TS - t->cam_y), 0, TF_WHITE);
    }
}

// One tile wide, one and a half tall, feet on the tile's bottom edge and centred on it — whatever the
// sheet's own resolution is. The frame is sheet/4 by sheet/4, so 128x192 and 1024x1536 both land here
// and only the sharpness differs.
// `lit`/`warm` are the light at the sprite's FEET, worked out once on the CPU: one level for the whole
// body, so a character walking past a lantern brightens as a whole instead of being lit in a gradient
// from the waist down, which is how the Sega games did it and what reads as a sprite rather than 3D.
static void draw_walker(TileField *t, int art, float fx, float fy, int facing, int col, unsigned int tint,
                        float lit, float warm) {
    if (art < 0 || art >= t->art_count) return;
    TfArt *a = &t->art[art];
    tf_use(t, a->tex, t->pal_ok ? 1 : 0);
    float u = (float)(col & 3) / 4.0f, v = (float)(facing & 3) / 4.0f;
    float x0 = dev(t, fx - t->cam_x), y0 = dev(t, fy - 16.0f - t->cam_y);
    float x1 = dev(t, fx - t->cam_x + TS), y1 = dev(t, fy - 16.0f - t->cam_y + 48.0f);
    if (t->pal_ok) {
        float fw = (float)a->fw, fh = (float)a->fh;
        float tx0 = (float)(col & 3) * fw, ty0 = (float)(facing & 3) * fh;
        float rect[4] = { tx0, ty0, tx0 + fw, ty0 + fh };
        push_quad(t, x0, y0, x1 - x0, y1 - y0, u, v, u + 0.25f, v + 0.25f, 0, tint, rect, lit, warm);
        return;
    }
    float hu = 0.5f / (float)a->w, hv = 0.5f / (float)a->h;
    push_quad(t, x0, y0, x1 - x0, y1 - y0,
              u + hu, v + hv, u + 0.25f - hu, v + 0.25f - hv, 0, tint, nullptr, lit, warm);
}

// The walk cycle. The sheet is stand, step-left, stand, step-right; a step shows two of those four, and
// the two steps alternate, so walking two tiles plays 1,2,3,0 — the same alternating feet PS4 has.
// `parity` flips on every step, `k` is how far through the step we are.
static int walk_col(int parity, float k, bool moving) {
    if (!moving) return 0;
    return ((parity ? 2 : 0) + (k < 0.5f ? 1 : 2)) & 3;
}

struct Drawable { float y; int art, facing, col; float x, sy; unsigned int tint; float lit, warm; };

static int drawable_cmp(const void *a, const void *b) {
    float d = ((const Drawable *)a)->y - ((const Drawable *)b)->y;
    return d < 0 ? -1 : d > 0 ? 1 : 0;
}

static float lerp(float a, float b, float k) { return a + (b - a) * k; }

// The light at one point, in LOGICAL view pixels — the same arithmetic the fragment shader does, so a
// walker lit on the CPU and the ground lit under its feet agree. Returns the point-light total; the
// ambient is folded in by the caller.
static float tf_light_at(TileField *t, float sx, float sy, int n, const float *lights) {
    float lamp = 0;
    for (int i = 0; i < n; i++) {
        const float *L = lights + i * 4;
        float dx = sx - L[0], dy = sy - L[1];
        float d = sqrtf(dx * dx + dy * dy);
        float f = 1.0f - (d / (L[2] > 0.001f ? L[2] : 0.001f));
        if (f < 0) f = 0;
        lamp += L[3] * f * f;
    }
    return lamp > 1.0f ? 1.0f : lamp;
}

// Every light this frame, in logical view pixels: the map's lamps (flickering ones wobble) plus the
// dev lantern on the leader. Returns how many were written.
static int tf_gather_lights(TileField *t, float *out, float leader_x, float leader_y) {
    int n = 0;
    for (int i = 0; i < t->light_count && n < TF_LIGHTS; i++) {
        TfLight *l = &t->lights[i];
        float lv = l->level;
        if (l->flicker) {
            float p = t->anim_t * 7.3f + (float)i * 1.7f;
            lv *= 0.88f + 0.12f * (sinf(p) * 0.6f + sinf(p * 2.7f) * 0.4f);
        }
        out[n * 4 + 0] = l->x * TS + TS * 0.5f - (float)t->cam_x;
        out[n * 4 + 1] = l->y * TS + TS * 0.5f - (float)t->cam_y;
        out[n * 4 + 2] = l->r * TS;
        out[n * 4 + 3] = lv;
        n++;
    }
    if (t->lantern && n < TF_LIGHTS) {
        out[n * 4 + 0] = leader_x + TS * 0.5f - (float)t->cam_x;
        out[n * 4 + 1] = leader_y + TS * 0.5f - (float)t->cam_y;
        out[n * 4 + 2] = t->lantern_r * TS;
        out[n * 4 + 3] = 0.95f;
        n++;
    }
    return n;
}

// `vw`,`vh` are the LOGICAL view; `sc` device pixels per logical pixel. Nothing in the world changes
// with sc — it is only how big the quads and their textures come out on this screen.
static void tf_render(TileField *t, int vw, int vh, float sc) {
    t->sc = sc;
    int dw = (int)(vw * sc + 0.5f), dh = (int)(vh * sc + 0.5f);
    glBindFramebuffer(GL_FRAMEBUFFER, t->fbo);
    glViewport(0, 0, dw, dh);
    glDisable(GL_DEPTH_TEST);
    glDisable(GL_CULL_FACE);
    glEnable(GL_BLEND);
    // Everything this shader writes is PREMULTIPLIED — the palette lookup has to be, because the four
    // blended taps may include transparent index 0 and only premultiplied alpha blends those without
    // dragging index 0's colour in. The plain path premultiplies too, so one blend mode covers both.
    glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
    glClearColor(0, 0, 0, 1);
    glClear(GL_COLOR_BUFFER_BIT);
    glUseProgram(t->prog);
    glUniform2f(t->u_res, (float)dw, (float)dh);        // the batch is built in device pixels
    glUniform1i(t->u_tex, 0);
    glUniform1f(t->u_sc, sc);

    // Light, once for the frame. The lights are handed over in logical VIEW pixels, which is what the
    // vertex shader recovers from the device-pixel position, so nothing in the shader knows about the
    // camera or the map.
    float kk = t->moving ? t->move_t / t->step_len : 1.0f;
    if (kk > 1) kk = 1;
    float lead_x = lerp(t->act[0].px * (float)TS, t->act[0].tx * (float)TS, kk);
    float lead_y = lerp(t->act[0].py * (float)TS, t->act[0].ty * (float)TS, kk);
    float lights[TF_LIGHTS * 4];
    int nl = t->pal_ok ? tf_gather_lights(t, lights, lead_x, lead_y) : 0;
    if (t->pal_ok) {
        glUniform1i(t->u_cmap, 1);
        glActiveTexture(GL_TEXTURE1);
        glBindTexture(GL_TEXTURE_2D, t->cmap);
        glActiveTexture(GL_TEXTURE0);
        int tbl = t->light_table < 0 ? 0 : t->light_table >= t->cmap_tables ? 0 : t->light_table;
        glUniform1f(t->u_cmaph, (float)t->cmap_rows);
        glUniform1f(t->u_levels, (float)t->cmap_levels);
        glUniform1f(t->u_rowa, (float)t->cmap_row0[tbl]);
        // A lamp pool is lit from its OWN table, so it can be warmer than plain daylight. If the
        // colormap has no `lamp` table yet, `day` is the honest stand-in: full-strength true colour.
        glUniform1f(t->u_rowb, (float)t->cmap_row0[t->lamp_table]);
        glUniform1f(t->u_amb, t->ambient);
        glUniform1i(t->u_nl, nl);
        glUniform1i(t->u_taps, t->one_tap ? 1 : 0);
        if (nl) glUniform4fv(t->u_lights, nl, lights);
    }
    glActiveTexture(GL_TEXTURE0);
    t->vn = 0; t->cur_tex = 0; t->cur_mode = -1;

    int x0 = t->cam_x / TS - 1, y0 = t->cam_y / TS - 1;
    int x1 = (t->cam_x + vw) / TS + 2, y1 = (t->cam_y + vh) / TS + 2;
    if (x0 < 0) x0 = 0;
    if (y0 < 0) y0 = 0;
    if (x1 > t->mw) x1 = t->mw;
    if (y1 > t->mh) y1 = t->mh;

    draw_ground(t, x0, y0, x1, y1);
    draw_fringes(t, x0, y0, x1, y1);
    if (t->dbg_solid) {
        for (int y = y0; y < y1; y++) for (int x = x0; x < x1; x++)
            if (t->solid[y][x]) push_solid(t, (float)(x * TS - t->cam_x), (float)(y * TS - t->cam_y), TS, TS, 0x604040FFu);
    }
    if (t->dbg_trig) {
        for (int i = 0; i < t->trig_count; i++) {
            TfTrig *g = &t->trigs[i];
            unsigned int c = g->kind == TG_MESSAGE ? 0x6000FFFFu : g->kind == TG_EXIT ? 0x6000FF00u : 0x60FF8000u;
            push_solid(t, (float)(g->x * TS - t->cam_x), (float)(g->y * TS - t->cam_y), (float)(g->w * TS), (float)(g->h * TS), c);
        }
    }
    draw_stamps(t, x0, y0, x1, y1, false);

    // Everyone that walks, sorted by the row their feet are on.
    Drawable dr[TF_NPCS + TF_PARTY];
    int nd = 0;
    float k = t->moving ? t->move_t / t->step_len : 1.0f;
    if (k > 1) k = 1;
    if (!t->hide_walkers) {                    // a capture can ask for the town with nobody in it
    for (int i = 0; i < t->party && nd < (int)(sizeof(dr) / sizeof(dr[0])); i++) {
        TfActor *a = &t->act[i];
        float fx = lerp(a->px * (float)TS, a->tx * (float)TS, k), fy = lerp(a->py * (float)TS, a->ty * (float)TS, k);
        bool mv = t->moving && (a->px != a->tx || a->py != a->ty);
        int col = walk_col(t->step_parity, k, mv);
        // The ids are the walker FILE names, which are the story's names: story/field/walkers/falke.png.
        static const char *PARTY_ART[TF_PARTY] = { "falke", "ottilie", "party_c", "party_d" };
        int art = art_get(t, PARTY_ART[i]);
        dr[nd].y = fy; dr[nd].x = fx; dr[nd].sy = fy; dr[nd].art = art;
        dr[nd].facing = a->facing; dr[nd].col = col; dr[nd].tint = TF_WHITE;
        float lamp = tf_light_at(t, fx - t->cam_x + TS * 0.5f, fy - t->cam_y + TS * 0.5f, nl, lights);
        dr[nd].lit = t->pal_ok ? (lamp > t->ambient ? lamp : t->ambient) : -1.0f;
        dr[nd].warm = (lamp - t->ambient) * 1.6f;
        if (dr[nd].warm < 0) dr[nd].warm = 0;
        if (dr[nd].warm > 1) dr[nd].warm = 1;
        nd++;
    }
    for (int i = 0; i < t->npc_count && nd < (int)(sizeof(dr) / sizeof(dr[0])); i++) {
        TfNpc *np = &t->npcs[i];
        float nk = np->t < 0.3f ? np->t / 0.3f : 1.0f;
        float fx = lerp(np->px * (float)TS, np->tx * (float)TS, nk), fy = lerp(np->py * (float)TS, np->ty * (float)TS, nk);
        if (fx < t->cam_x - TS * 2 || fx > t->cam_x + vw + TS * 2) continue;
        dr[nd].y = fy; dr[nd].x = fx; dr[nd].sy = fy; dr[nd].art = np->art;
        dr[nd].facing = np->facing;
        dr[nd].col = walk_col(np->parity, nk, np->px != np->tx || np->py != np->ty);   // the same cycle
        dr[nd].tint = TF_WHITE;
        float lamp = tf_light_at(t, fx - t->cam_x + TS * 0.5f, fy - t->cam_y + TS * 0.5f, nl, lights);
        dr[nd].lit = t->pal_ok ? (lamp > t->ambient ? lamp : t->ambient) : -1.0f;
        dr[nd].warm = (lamp - t->ambient) * 1.6f;
        if (dr[nd].warm < 0) dr[nd].warm = 0;
        if (dr[nd].warm > 1) dr[nd].warm = 1;
        nd++;
    }
    qsort(dr, (size_t)nd, sizeof(dr[0]), drawable_cmp);
    for (int i = 0; i < nd; i++)
        draw_walker(t, dr[i].art, dr[i].x, dr[i].sy, dr[i].facing, dr[i].col, dr[i].tint, dr[i].lit, dr[i].warm);
    }

    draw_stamps(t, x0, y0, x1, y1, true);

    // The "!" over the leader when the tile faced has something to say.
    if ((t->exam_trig >= 0 || t->exam_npc >= 0) && !t->msg[0]) {
        TfActor *a = &t->act[0];
        float fx = lerp(a->px * (float)TS, a->tx * (float)TS, k) - t->cam_x + TS / 2 - 2;
        float fy = lerp(a->py * (float)TS, a->ty * (float)TS, k) - t->cam_y - 34 + (fmodf(t->anim_t, 1.0f) < 0.5f ? 0 : 1);
        push_solid(t, fx - 1, fy - 1, 6, 16, 0xFF201818u);
        push_solid(t, fx, fy, 4, 9, 0xFF40E8FFu);
        push_solid(t, fx, fy + 11, 4, 3, 0xFF40E8FFu);
    }
    if (t->fade) {
        int f = t->fade > FADE_FRAMES ? FADE_FRAMES : t->fade;
        push_solid(t, 0, 0, (float)vw, (float)vh, (unsigned int)(f * 255 / FADE_FRAMES) << 24);
    }
    tf_flush(t);

    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindVertexArray(0);
    glBindTexture(GL_TEXTURE_2D, 0);
    glUseProgram(0);
}

// ───────────────────────── touch and keys ─────────────────────────

struct TfTouch { SDL_FingerID id; float x, y; };

static int tf_touches(TfTouch *out, int max, int w, int h) {
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

static void tf_input(TileField *t, int w, int h, bool blocked, float dt) {
    TfTouch tt[8];
    int n = blocked ? 0 : tf_touches(tt, 8, w, h);
    t->tapped = false;
    bool stick_seen = false, act_seen = false;
    for (int i = 0; i < n; i++) {
        if (t->stick_on && tt[i].id == t->stick_id) { stick_seen = true; t->stick_x = tt[i].x; t->stick_y = tt[i].y; }
        if (t->act_on && tt[i].id == t->act_id) act_seen = true;
    }
    if (t->stick_on && !stick_seen) t->stick_on = false;
    if (t->act_on && !act_seen) {
        if (t->act_t <= 0.35f) t->tapped = true;              // right half: a short press interacts
        t->act_on = false; t->run = false; t->act_t = 0;
    }
    for (int i = 0; i < n; i++) {
        if ((t->stick_on && tt[i].id == t->stick_id) || (t->act_on && tt[i].id == t->act_id)) continue;
        if (tt[i].x < w * 0.5f && !t->stick_on) {
            t->stick_on = true; t->stick_id = tt[i].id;
            t->stick_ox = t->stick_x = tt[i].x; t->stick_oy = t->stick_y = tt[i].y;
        } else if (tt[i].x >= w * 0.5f && !t->act_on) {
            t->act_on = true; t->act_id = tt[i].id; t->act_t = 0;
        }
    }
    if (t->act_on) { t->act_t += dt; if (t->act_t > 0.35f) t->run = true; }   // long press = run

    if (!blocked) {
        ImGuiIO &io = ImGui::GetIO();
        if (!io.WantCaptureKeyboard) {
            if (ImGui::IsKeyPressed(ImGuiKey_Space, false) || ImGui::IsKeyPressed(ImGuiKey_Enter, false) || ImGui::IsKeyPressed(ImGuiKey_Z, false)) t->tapped = true;
            if (ImGui::IsKeyDown(ImGuiKey_LeftShift) || ImGui::IsKeyDown(ImGuiKey_RightShift)) t->run = true;
        }
    }
}

// The one direction the player is asking for, or -1. Touch resolves to the dominant axis with a
// hysteresis band, so a thumb held near a diagonal does not flicker between two axes every frame.
static int tf_want_dir(TileField *t, int w) {
    float dx = 0, dy = 0;
    if (t->stick_on) {
        dx = t->stick_x - t->stick_ox; dy = t->stick_y - t->stick_oy;
        if (dx * dx + dy * dy < (w * 0.024f) * (w * 0.024f)) { dx = dy = 0; }
    }
    ImGuiIO &io = ImGui::GetIO();
    if (!io.WantCaptureKeyboard) {
        if (ImGui::IsKeyDown(ImGuiKey_LeftArrow) || ImGui::IsKeyDown(ImGuiKey_A)) dx -= 100;
        if (ImGui::IsKeyDown(ImGuiKey_RightArrow) || ImGui::IsKeyDown(ImGuiKey_D)) dx += 100;
        if (ImGui::IsKeyDown(ImGuiKey_UpArrow) || ImGui::IsKeyDown(ImGuiKey_W)) dy -= 100;
        if (ImGui::IsKeyDown(ImGuiKey_DownArrow) || ImGui::IsKeyDown(ImGuiKey_S)) dy += 100;
    }
    if (dx == 0 && dy == 0) { t->last_axis = -1; return -1; }
    int axis;
    float ax = fabsf(dx), ay = fabsf(dy);
    if (ax > ay * 1.35f) axis = 0;
    else if (ay > ax * 1.35f) axis = 1;
    else axis = t->last_axis >= 0 ? t->last_axis : (ax >= ay ? 0 : 1);
    t->last_axis = axis;
    return axis == 0 ? (dx < 0 ? 1 : 2) : (dy < 0 ? 3 : 0);
}

// ───────────────────────── talking ─────────────────────────

template <typename T> static auto ft_name2(const T &e, int) -> decltype(e.name) { return e.name; }
template <typename T> static const char *ft_name2(const T &, long) { return ""; }

static void tf_say(TileField *t, const char *who, const char *id) {
    const char *text = nullptr, *name = nullptr;
    for (int i = 0; i < FIELD_TEXT_COUNT; i++)
        if (!strcmp(FIELD_TEXT[i].id, id)) { text = FIELD_TEXT[i].text; name = ft_name2(FIELD_TEXT[i], 0); break; }
    if (text) snprintf(t->msg, sizeof(t->msg), "%s", text);
    else snprintf(t->msg, sizeof(t->msg), "[%s]", id);
    const char *speaker = (name && name[0]) ? name : who;
    snprintf(t->msg_who, sizeof(t->msg_who), "%s", speaker ? speaker : "");
    t->msg_t = 0;
}

void tf_message(TileField *t, const char *text) {
    snprintf(t->msg, sizeof(t->msg), "%s", text ? text : "");
    t->msg_who[0] = 0;
    t->msg_t = 0;
}

static void fire(TfEvent *ev, int kind, const char *arg) {
    if (ev->kind != TFE_NONE) return;
    ev->kind = kind;
    snprintf(ev->arg, sizeof(ev->arg), "%s", arg);
}

static int trig_at(TileField *t, int x, int y, int kind_a, int kind_b) {
    for (int i = 0; i < t->trig_count; i++) {
        TfTrig *g = &t->trigs[i];
        if (x < g->x || y < g->y || x >= g->x + g->w || y >= g->y + g->h) continue;
        if (g->kind == kind_a || g->kind == kind_b) return i;
    }
    return -1;
}

// ───────────────────────── the step ─────────────────────────

static void start_map_change(TileField *t, const char *map, int x, int y, int f) {
    snprintf(t->to_map, sizeof(t->to_map), "%s", map);
    t->to_x = x; t->to_y = y; t->to_f = f;
    t->fade = 1; t->fade_dir = 1;
}

static void npc_step(TileField *t, float dt) {
    for (int i = 0; i < t->npc_count; i++) {
        TfNpc *np = &t->npcs[i];
        if (np->px != np->tx || np->py != np->ty) {
            np->t += dt;
            if (np->t >= 0.3f) { np->px = np->tx; np->py = np->ty; np->t = 0.3f; }
            continue;
        }
        if (!np->wander) continue;
        np->wait -= dt;
        if (np->wait > 0) continue;
        np->wait = 1.0f + trnd((int)(t->anim_t * 60) + i, i, 17) * 2.0f;
        int d = (int)(trnd((int)(t->anim_t * 97) + i, 5, 3) * 4) & 3;
        int nx = np->tx + DX[d], ny = np->ty + DY[d];
        np->facing = d;
        if (abs(nx - np->hx) > np->wander || abs(ny - np->hy) > np->wander) continue;
        if (cell_solid(t, nx, ny)) continue;
        if (trig_at(t, nx, ny, TG_EXIT, TG_DOOR) >= 0 || trig_at(t, nx, ny, TG_MESSAGE, TG_ZONE) >= 0) continue;
        bool blocked = false;
        for (int p = 0; p < t->party && !blocked; p++) if (t->act[p].tx == nx && t->act[p].ty == ny) blocked = true;
        if (blocked) continue;
        t->solid[np->ty][np->tx] = 0;
        np->px = np->tx; np->py = np->ty;
        np->tx = (short)nx; np->ty = (short)ny;
        np->t = 0;
        np->parity ^= 1;                                  // alternate feet, exactly as the party does
        t->solid[ny][nx] = 1;
    }
}

// Entering a tile: exits, traps and zones fire here; a door fires on entering it from the south too,
// which is what walking into a doorway feels like.
static void on_enter_tile(TileField *t, TfEvent *ev) {
    t->steps++;
    int x = t->act[0].tx, y = t->act[0].ty;
    for (int i = 0; i < t->trig_count; i++) {
        TfTrig *g = &t->trigs[i];
        bool in = x >= g->x && y >= g->y && x < g->x + g->w && y < g->y + g->h;
        bool was = g->inside;
        g->inside = in;
        if (!in || was) continue;
        if (g->kind == TG_EXIT) { start_map_change(t, g->map, g->ax, g->ay, g->af); return; }
        if (g->kind == TG_DOOR && t->act[0].facing == 3) { start_map_change(t, g->map, g->ax, g->ay, g->af); return; }
        if (g->kind == TG_ZONE) { SDL_Log("tilefield: zone %s at %d,%d", g->arg, x, y); fire(ev, TFE_ZONE, g->arg); }
        if (g->kind == TG_TRAP) tf_say(t, nullptr, g->arg);
        if (g->kind == TG_SCENE) fire(ev, TFE_SCENE, g->arg);
    }
}

static void tf_walk(TileField *t, int w, float dt, TfEvent *ev) {
    if (t->moving) {
        t->move_t += dt;
        if (t->move_t < t->step_len) return;
        t->moving = false;
        t->move_t = 0;
        for (int i = 0; i < TF_PARTY; i++) { t->act[i].px = t->act[i].tx; t->act[i].py = t->act[i].ty; }
        on_enter_tile(t, ev);
        if (t->fade_dir) return;                             // the tile we landed on changed the map
    }
    if (t->msg[0] || t->fade_dir) return;                    // no walking with the box open

    int dir = tf_want_dir(t, w);
    if (dir < 0) { t->hold_t = 0; t->want_dir = -1; return; }
    if (dir != t->want_dir) { t->want_dir = dir; t->hold_t = 0; t->act[0].facing = dir; return; }
    t->hold_t += dt;
    if (t->hold_t < TURN_HOLD) return;                       // a tap turns in place; a hold walks

    t->act[0].facing = dir;
    int nx = t->act[0].tx + DX[dir], ny = t->act[0].ty + DY[dir];
    if (!t->noclip && cell_solid(t, nx, ny)) return;         // blocked: face it, don't step
    if (!t->noclip && npc_at(t, nx, ny) >= 0) return;

    short ox[TF_PARTY], oy[TF_PARTY];
    for (int i = 0; i < TF_PARTY; i++) { ox[i] = t->act[i].tx; oy[i] = t->act[i].ty; }
    t->act[0].px = ox[0]; t->act[0].py = oy[0];
    t->act[0].tx = (short)nx; t->act[0].ty = (short)ny;
    for (int i = 1; i < TF_PARTY; i++) {                     // the snake: each takes the one ahead's tile
        t->act[i].px = ox[i]; t->act[i].py = oy[i];
        t->act[i].tx = ox[i - 1]; t->act[i].ty = oy[i - 1];
        if (t->act[i].tx != t->act[i].px || t->act[i].ty != t->act[i].py) {
            int ddx = t->act[i].tx - t->act[i].px, ddy = t->act[i].ty - t->act[i].py;
            t->act[i].facing = ddx < 0 ? 1 : ddx > 0 ? 2 : ddy < 0 ? 3 : 0;
        }
    }
    t->moving = true;
    t->move_t = 0;
    t->step_len = t->run ? RUN_STEP : WALK_STEP;
    t->step_parity ^= 1;
}

// ───────────────────────── the box and the pad ─────────────────────────

static void draw_touch_ui(TileField *t, int w, int h) {
    ImDrawList *dl = ImGui::GetBackgroundDrawList();
    float r = h * 0.14f;
    if (t->stick_on) {
        float dx = t->stick_x - t->stick_ox, dy = t->stick_y - t->stick_oy;
        float len = sqrtf(dx * dx + dy * dy);
        if (len > r) { dx *= r / len; dy *= r / len; }
        dl->AddCircleFilled(ImVec2(t->stick_ox, t->stick_oy), r, IM_COL32(255, 255, 255, 26), 32);
        dl->AddCircle(ImVec2(t->stick_ox, t->stick_oy), r, IM_COL32(255, 255, 255, 70), 32, h * 0.006f);
        dl->AddCircleFilled(ImVec2(t->stick_ox + dx, t->stick_oy + dy), r * 0.42f, IM_COL32(220, 228, 255, 110), 24);
    }
    float ax = w - h * 0.20f, ay = h * 0.74f;
    dl->AddCircle(ImVec2(ax, ay), h * 0.10f, IM_COL32(255, 255, 255, t->act_on ? 130 : 45), 28, h * 0.006f);
    if (t->act_on) dl->AddCircleFilled(ImVec2(ax, ay), h * 0.10f, IM_COL32(255, 255, 255, t->run ? 60 : 30), 28);
    if (t->run) dl->AddText(ImGui::GetFont(), h * 0.05f, ImVec2(ax - h * 0.05f, ay - h * 0.025f), IM_COL32(255, 230, 150, 220), "RUN");
}

static void draw_msg_box(TileField *t, int w, int h) {
    if (!t->msg[0]) return;
    ImDrawList *dl = ImGui::GetBackgroundDrawList();
    ImFont *font = ImGui::GetFont();
    float ts = h * 0.052f, u = ts * 0.12f;
    float x0 = w * 0.06f, x1 = w * 0.94f, y1 = h * 0.96f, y0 = y1 - h * 0.24f;
    dl->AddRectFilled(ImVec2(x0, y0), ImVec2(x1, y1), IM_COL32(8, 8, 20, 255), u * 1.6f);
    dl->AddRectFilledMultiColor(ImVec2(x0 + u, y0 + u), ImVec2(x1 - u, y1 - u),
        IM_COL32(24, 40, 150, 255), IM_COL32(24, 40, 150, 255), IM_COL32(8, 16, 84, 255), IM_COL32(8, 16, 84, 255));
    dl->AddRect(ImVec2(x0 + u * 0.5f, y0 + u * 0.5f), ImVec2(x1 - u * 0.5f, y1 - u * 0.5f), IM_COL32(225, 225, 235, 255), u * 1.4f, 0, u * 0.7f);
    float ty = y0 + ts * 0.7f, tw = x1 - x0 - ts * 1.5f;
    if (t->msg_who[0]) { dl->AddText(font, ts * 0.92f, ImVec2(x0 + ts * 0.75f, ty), IM_COL32(255, 216, 74, 255), t->msg_who); ty += ts * 1.35f; }
    dl->AddText(font, ts, ImVec2(x0 + ts * 0.75f, ty), IM_COL32_WHITE, t->msg, nullptr, tw);
    if (fmodf(t->msg_t, 1.0f) < 0.6f)
        dl->AddTriangleFilled(ImVec2(x1 - ts * 1.5f, y1 - ts * 0.95f), ImVec2(x1 - ts * 0.7f, y1 - ts * 0.95f),
                              ImVec2(x1 - ts * 1.1f, y1 - ts * 0.5f), IM_COL32_WHITE);
}

// ───────────────────────── map.flag ─────────────────────────
// The same one-file remote control field.cpp has: `printf halm > files/map.flag` and the phone goes
// there, with no taps at all. Truncated once consumed, exactly like reload.flag.
static void tf_poll_map_flag(TileField *t, float dt) {
    t->map_poll += dt;
    if (t->map_poll < 0.4f) return;
    t->map_poll = 0;
    char path[600];
    snprintf(path, sizeof(path), "%smap.flag", tf_pref());
    size_t sz = 0;
    char *text = (char *)SDL_LoadFile(path, &sz);
    if (!text) return;
    if (!sz) { SDL_free(text); return; }
    char want[64] = "";
    sscanf(text, "%63s", want);
    SDL_free(text);
    SDL_IOStream *tr = SDL_IOFromFile(path, "wb");
    if (tr) SDL_CloseIO(tr);
    for (char *c = want; *c; c++)
        if (!((*c >= 'a' && *c <= 'z') || (*c >= '0' && *c <= '9') || *c == '_')) { *c = 0; break; }
    if (!want[0]) return;
    SDL_Log("tilefield: map.flag asks for \"%s\"", want);
    tf_load_map(t, want);
}

// ───────────────────────── tick ─────────────────────────

void tf_tick(TileField *t, int w, int h, float dt, bool ui_blocked, TfEvent *ev) {
    ev->kind = TFE_NONE; ev->arg[0] = 0;
    if (!t->gl_ready) tf_gl_init(t);
    if (!t->mw || t->mw == 1) tf_load_map(t, t->map_name[0] ? t->map_name : "halm");
    tf_poll_map_flag(t, dt);
    tf_cycle(t, dt);                                    // water and lamps, as colormap columns
    t->anim_t += dt;
    t->msg_t += dt;

    tf_input(t, w, h, ui_blocked, dt);

    // The fade that carries a map change: out, load, in. Eight frames each, as the brief asks.
    if (t->fade_dir) {
        t->fade += t->fade_dir;
        if (t->fade_dir > 0 && t->fade >= FADE_FRAMES) {
            tf_load_map(t, t->to_map);
            tf_place_party(t, t->to_x, t->to_y, t->to_f);
            for (int i = 0; i < t->trig_count; i++)                 // don't re-fire the door we arrived on
                t->trigs[i].inside = (t->act[0].tx >= t->trigs[i].x && t->act[0].ty >= t->trigs[i].y &&
                                      t->act[0].tx < t->trigs[i].x + t->trigs[i].w && t->act[0].ty < t->trigs[i].y + t->trigs[i].h);
            t->fade_dir = -1;
        } else if (t->fade_dir < 0 && t->fade <= 0) { t->fade = 0; t->fade_dir = 0; }
    }

    tf_walk(t, w, dt, ev);
    npc_step(t, dt);

    // What an interact press would open: the tile faced, or an NPC standing on it.
    t->exam_trig = t->exam_npc = -1;
    {
        int fx = t->act[0].tx + DX[t->act[0].facing & 3], fy = t->act[0].ty + DY[t->act[0].facing & 3];
        t->exam_npc = npc_at(t, fx, fy);
        if (t->exam_npc < 0) {
            t->exam_trig = trig_at(t, fx, fy, TG_MESSAGE, TG_DOOR);
            if (t->exam_trig < 0) t->exam_trig = trig_at(t, t->act[0].tx, t->act[0].ty, TG_MESSAGE, TG_MESSAGE);
        }
    }
    if (t->tapped && !t->fade_dir) {
        if (t->msg[0]) { t->msg[0] = 0; t->msg_who[0] = 0; }
        else if (t->exam_npc >= 0) {
            TfNpc *np = &t->npcs[t->exam_npc];
            static const int OPP[4] = { 3, 2, 1, 0 };            // the NPC turns to face the player
            np->facing = OPP[t->act[0].facing & 3];
            tf_say(t, nullptr, np->text);
        } else if (t->exam_trig >= 0) {
            TfTrig *g = &t->trigs[t->exam_trig];
            if (g->kind == TG_DOOR) start_map_change(t, g->map, g->ax, g->ay, g->af);
            else tf_say(t, nullptr, g->arg);
        }
    }

    // Camera: centred on the leader, snapped to whole pixels, clamped to the map. A map smaller than
    // the view is centred instead, which is what letterboxes it.
    int vw = (int)((float)VH * (float)w / (h > 0 ? (float)h : 1.0f) + 0.5f);
    if (vw < VW_MIN) vw = VW_MIN;
    if (vw > VW_MAX) vw = VW_MAX;
    vw &= ~1;
    t->vw = vw;
    float k = t->moving ? t->move_t / t->step_len : 1.0f;
    float lx = lerp(t->act[0].px * (float)TS, t->act[0].tx * (float)TS, k) + TS * 0.5f;
    float ly = lerp(t->act[0].py * (float)TS, t->act[0].ty * (float)TS, k) + TS * 0.5f;
    int cx = (int)(lx - vw * 0.5f + 0.5f), cy = (int)(ly - VH * 0.5f + 0.5f);
    int mapw = t->mw * TS, maph = t->mh * TS;
    t->cam_x = mapw <= vw ? (mapw - vw) / 2 : (cx < 0 ? 0 : cx > mapw - vw ? mapw - vw : cx);
    t->cam_y = maph <= VH ? (maph - VH) / 2 : (cy < 0 ? 0 : cy > maph - VH ? maph - VH : cy);

    // The view is rendered at the SCREEN's own resolution: the logical world is still 640x360-class and
    // 32 px to the tile, but every quad comes out `sc` times bigger and its texture is sampled at that
    // size, so 128-px tiles and 256x384 walker frames arrive as themselves and nothing is thrown away.
    float sc = fminf((float)w / vw, (float)h / VH);
    int dwi = (int)(vw * sc + 0.5f), dhi = (int)(VH * sc + 0.5f);
    tf_fbo_size(t, dwi, dhi);
    // The world is drawn into its own FBO, so a glFinish here times THAT and nothing else. It is the
    // only honest number on a vsynced phone, where the frame rate says only what the display does.
    uint64_t t0 = SDL_GetTicksNS();
    tf_render(t, vw, VH, sc);
    glFinish();
    t->draw_ns += SDL_GetTicksNS() - t0;

    float dw = (float)dwi, dh = (float)dhi, ox = (w - dw) * 0.5f, oy = (h - dh) * 0.5f;
    ImDrawList *dl = ImGui::GetBackgroundDrawList();
    dl->AddRectFilled(ImVec2(0, 0), ImVec2((float)w, (float)h), IM_COL32(0, 0, 0, 255));
    dl->AddImage((ImTextureID)(intptr_t)t->fbo_tex, ImVec2(ox, oy), ImVec2(ox + dw, oy + dh),
                 ImVec2(0.0f, 1.0f), ImVec2(1.0f, 0.0f));                    // GL's origin is bottom-left
    {   // Fill rate, in the log rather than guessed at: the view is ~2.6 Mpx a frame at 3x.
        static int frames = 0; static float secs = 0;
        secs += dt;
        if (++frames >= 600) {
            SDL_Log("tilefield: %d frames in %.1fs — %.1f fps, %dx%d device px, world draw %.2f ms/frame (%s)",
                    frames, secs, frames / secs, dwi, dhi, (double)t->draw_ns / frames / 1e6,
                    !t->pal_ok ? "unpalettised" : t->one_tap ? "1 tap" : "4-tap blend");
            frames = 0; secs = 0; t->draw_ns = 0;
        }
    }
    draw_touch_ui(t, w, h);
    draw_msg_box(t, w, h);
    if (t->dbg_coord) {
        char lbl[64];
        snprintf(lbl, sizeof(lbl), "%s %d,%d %s  steps %d", t->map_name, t->act[0].tx, t->act[0].ty,
                 FACE_NAME[t->act[0].facing & 3], t->steps);
        dl->AddText(ImGui::GetFont(), h * 0.04f, ImVec2(ox + 8, oy + 8), IM_COL32(255, 240, 160, 230), lbl);
    }
}

// ───────────────────────── capture ─────────────────────────
// The whole map in one PNG, so the town can be read on the Mac without a phone screenshot.

static void tf_do_capture(TileField *t) {
    tf_probe_limits();
    // Two shapes: the whole map at one device pixel per logical pixel (how a .tmap is read as a town),
    // or the player's own 640x360 view at the capture scale — 3 gives 1920x1080, which is what the
    // phone draws, so the art can be judged at the size it will actually be seen.
    int lw, lh, sc = t->cap_scale < 1 ? 1 : t->cap_scale;
    if (t->cap_whole) { lw = t->mw * TS; lh = t->mh * TS; sc = 1; }
    else { lw = VW_MIN; lh = VH; }
    int cw = lw * sc, ch = lh * sc;
    while (tf_max_tex > 0 && (cw > tf_max_tex || ch > tf_max_tex) && sc > 1) { sc--; cw = lw * sc; ch = lh * sc; }
    if (cw > 4096) { lw = 4096 / sc; cw = lw * sc; }
    if (ch > 4096) { lh = 4096 / sc; ch = lh * sc; }
    t->hide_walkers = !t->cap_walkers;
    // At spawn the whole party stands on one tile, so a capture would show one sprite. Trail them out
    // behind the leader for the picture only: the point of the shot is to see everyone's art.
    if (t->cap_walkers) {
        for (int i = 1; i < t->party; i++) {
            int bx = t->act[i - 1].tx - DX[t->act[0].facing & 3], by = t->act[i - 1].ty - DY[t->act[0].facing & 3];
            if (bx < 0 || by < 0 || bx >= t->mw || by >= t->mh) break;
            t->act[i].tx = t->act[i].px = (short)bx; t->act[i].ty = t->act[i].py = (short)by;
        }
    }
    if (t->cap_whole) { t->cam_x = t->cam_y = 0; }
    else {                                                   // centre on the leader, clamped like the game
        int mapw = t->mw * TS, maph = t->mh * TS;
        int cx = t->act[0].tx * TS + TS / 2 - lw / 2, cy = t->act[0].ty * TS + TS / 2 - lh / 2;
        t->cam_x = mapw <= lw ? (mapw - lw) / 2 : (cx < 0 ? 0 : cx > mapw - lw ? mapw - lw : cx);
        t->cam_y = maph <= lh ? (maph - lh) / 2 : (cy < 0 ? 0 : cy > maph - lh ? maph - lh : cy);
    }
    if (t->cap_w != cw || t->cap_h != ch) {
        if (t->cap_tex) { glDeleteTextures(1, &t->cap_tex); glDeleteFramebuffers(1, &t->cap_fbo); }
        glGenTextures(1, &t->cap_tex);
        glBindTexture(GL_TEXTURE_2D, t->cap_tex);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, cw, ch, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        glGenFramebuffers(1, &t->cap_fbo);
        glBindFramebuffer(GL_FRAMEBUFFER, t->cap_fbo);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, t->cap_tex, 0);
        if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) { SDL_Log("tilefield: capture FBO incomplete"); return; }
        t->cap_w = cw; t->cap_h = ch;
    }
    GLuint keep = t->fbo;
    t->fbo = t->cap_fbo;
    tf_render(t, lw, lh, (float)sc);
    t->fbo = keep;
    t->hide_walkers = false;

    unsigned char *px = (unsigned char *)malloc((size_t)cw * ch * 4);
    unsigned char *row = (unsigned char *)malloc((size_t)cw * 4);
    if (!px || !row) { free(px); free(row); return; }
    glBindFramebuffer(GL_FRAMEBUFFER, t->cap_fbo);
    glPixelStorei(GL_PACK_ALIGNMENT, 1);
    glReadPixels(0, 0, cw, ch, GL_RGBA, GL_UNSIGNED_BYTE, px);
    for (int y = 0; y < ch / 2; y++) {                              // GL reads bottom-up
        memcpy(row, px + (size_t)y * cw * 4, (size_t)cw * 4);
        memcpy(px + (size_t)y * cw * 4, px + (size_t)(ch - 1 - y) * cw * 4, (size_t)cw * 4);
        memcpy(px + (size_t)(ch - 1 - y) * cw * 4, row, (size_t)cw * 4);
    }
    int ok = stbi_write_png(t->cap_out, cw, ch, 4, px, cw * 4);
    SDL_Log("tilefield: capture %s %dx%d (%s, %s walkers) %s", t->cap_out, cw, ch,
            t->cap_whole ? "whole map" : "one view", t->cap_walkers ? "with" : "no", ok ? "written" : "FAILED");
    free(px); free(row);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    t->cap_ok = true;
}

void tf_capture_to(TileField *t, const char *map, int scale, const char *out_path, bool walkers, bool whole) {
    if (!t->gl_ready) tf_gl_init(t);
    tf_load_map(t, map);
    t->cap_scale = scale < 1 ? 1 : scale > 4 ? 4 : scale;
    t->cap_walkers = walkers;
    t->cap_whole = whole;
    // The light the shot is taken under, so the Mac can produce day/dusk/night/lantern side by side
    // with no phone: TILE_LIGHT=<table>:<level>, TILE_LANTERN=<radius in tiles>.
    const char *lt = SDL_getenv("TILE_LIGHT");
    if (lt && lt[0]) {
        char tb[16] = "day";
        float lv = 1.0f;
        if (sscanf(lt, "%15[^:]:%f", tb, &lv) >= 1) {
            t->light_table = tf_table_by_name(t, tb);
            t->ambient = lv < 0 ? 0 : lv > 1 ? 1 : lv;
        }
    }
    const char *ln = SDL_getenv("TILE_LANTERN");
    if (ln && ln[0]) { t->lantern = true; t->lantern_r = (float)atof(ln); if (t->lantern_r < 1) t->lantern_r = 5; }
    snprintf(t->cap_out, sizeof(t->cap_out), "%s", out_path);
    t->cap_req = 3;                                                // let the tileset settle a frame or two
}

bool tf_capture_done(TileField *t) {
    if (t->cap_req > 0) {
        if (--t->cap_req == 0) tf_do_capture(t);
        return false;
    }
    return t->cap_ok;
}

// ───────────────────────── dev, save, lifecycle ─────────────────────────

bool tf_dev_ui(TileField *t, char *out, int cap) {
    bool printed = false;
    ImGui::Text("tile field — %s  %d,%d %s  steps %d  (%dx%d)", t->map_name, t->act[0].tx, t->act[0].ty,
                FACE_NAME[t->act[0].facing & 3], t->steps, t->mw, t->mh);
    for (int i = 0; i < tf_map_count(); i++) {
        if (i) ImGui::SameLine();
        bool cur = !strcmp(t->map_name, tf_map_name_at(i));
        if (cur) ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.2f, 0.55f, 0.3f, 1));
        if (ImGui::Button(tf_map_name_at(i))) { tf_load_map(t, tf_map_name_at(i)); }
        if (cur) ImGui::PopStyleColor();
    }
    ImGui::SameLine();
    if (ImGui::Button("Respawn")) tf_load_map(t, t->map_name);
    bool a = t->dbg_solid != 0, b = t->dbg_trig != 0, c = t->dbg_coord != 0, d = t->noclip != 0;
    if (ImGui::Checkbox("Collision", &a)) t->dbg_solid = a;
    ImGui::SameLine();
    if (ImGui::Checkbox("Triggers", &b)) t->dbg_trig = b;
    ImGui::SameLine();
    if (ImGui::Checkbox("Coords", &c)) t->dbg_coord = c;
    ImGui::SameLine();
    if (ImGui::Checkbox("No clip", &d)) t->noclip = d;
    // Light (PALETTE.md / D19). The table picker and the ambient slider are how the owner sees night
    // in Halm without editing a map; the lantern is the point light that proves the warm pool.
    if (t->pal_ok) {
        ImGui::Separator();
        ImGui::Text("light  %d lamp(s)  %ld px off-palette", t->light_count, t->off_pal);
        for (int i = 0; i < t->cmap_tables && i < TF_TABLES; i++) {
            if (i) ImGui::SameLine();
            bool cur = t->light_table == i;
            if (cur) ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.2f, 0.4f, 0.65f, 1));
            if (ImGui::Button(t->cmap_name[i][0] ? t->cmap_name[i] : "?")) t->light_table = i;
            if (cur) ImGui::PopStyleColor();
        }
        ImGui::SetNextItemWidth(ImGui::GetFontSize() * 12);
        ImGui::SliderFloat("ambient", &t->ambient, 0.0f, 1.0f, "%.2f");
        bool one = t->one_tap;
        if (ImGui::Checkbox("1 tap (no blend)", &one)) { t->one_tap = one; t->draw_ns = 0; }
        ImGui::SameLine();
        bool lan = t->lantern;
        if (ImGui::Checkbox("Lantern on the player", &lan)) t->lantern = lan;
        if (t->lantern) {
            ImGui::SameLine();
            ImGui::SetNextItemWidth(ImGui::GetFontSize() * 10);
            ImGui::SliderFloat("radius", &t->lantern_r, 1.0f, 14.0f, "%.1f tiles");
        }
    } else {
        ImGui::Separator();
        ImGui::TextUnformatted("light: no story/palette/master.hex — unpalettised, no lighting");
    }
    if (ImGui::Button("Print tile")) {
        snprintf(out, cap, "%s: standing on %d,%d facing %s (%s)", t->map_name, t->act[0].tx, t->act[0].ty,
                 FACE_NAME[t->act[0].facing & 3], t->solid[t->act[0].ty][t->act[0].tx] ? "solid" : "free");
        printed = true;
    }
    return printed;
}

TileField *tf_create() {
    TileField *t = (TileField *)calloc(1, sizeof(TileField));
    snprintf(t->map_name, sizeof(t->map_name), "halm");
    t->exam_trig = t->exam_npc = -1;
    t->want_dir = t->last_axis = -1;
    t->party = 2;
    t->step_len = WALK_STEP;
    t->ambient = 1.0f;
    t->light_table = TBL_DAY;
    t->lantern_r = 5.0f;
    return t;
}

void tf_destroy(TileField *t) {
    if (!t) return;
    free(t->cmap_px);
    free(t->v);
    free(t);
}

void tf_save(TileField *t, TfSave *s) {
    memset(s, 0, sizeof(*s));
    snprintf(s->map, sizeof(s->map), "%s", t->map_name);
    for (int i = 0; i < TF_PARTY; i++) { s->tx[i] = t->act[i].tx; s->ty[i] = t->act[i].ty; s->facing[i] = t->act[i].facing; }
    s->party = t->party; s->steps = t->steps;
    s->dbg_solid = t->dbg_solid; s->dbg_trig = t->dbg_trig; s->dbg_coord = t->dbg_coord; s->noclip = t->noclip;
    s->light_table = t->light_table; s->lantern = t->lantern ? 1 : 0;
    s->ambient = t->ambient; s->lantern_r = t->lantern_r;
}

// Everything read back is validated: a map that no longer has that tile, or a tile that is now solid,
// must not strand the party inside a wall after an edit-and-reload.
void tf_restore(TileField *t, const TfSave *s) {
    char map[32];
    snprintf(map, sizeof(map), "%s", s->map[0] ? s->map : "halm");
    for (char *c = map; *c; c++) if (!((*c >= 'a' && *c <= 'z') || (*c >= '0' && *c <= '9') || *c == '_')) { *c = 0; break; }
    tf_load_map(t, map[0] ? map : "halm");
    t->party = s->party < 1 ? 1 : s->party > TF_PARTY ? TF_PARTY : s->party;
    t->steps = s->steps < 0 ? 0 : s->steps;
    t->dbg_solid = s->dbg_solid ? 1 : 0; t->dbg_trig = s->dbg_trig ? 1 : 0;
    t->dbg_coord = s->dbg_coord ? 1 : 0; t->noclip = s->noclip ? 1 : 0;
    // After tf_load_map, so what the owner was looking at wins over the map's own `light:` line.
    if (s->light_table >= 0 && s->light_table < TF_TABLES) t->light_table = s->light_table;
    if (s->ambient >= 0.0f && s->ambient <= 1.0f) t->ambient = s->ambient;
    t->lantern = s->lantern != 0;
    t->lantern_r = (s->lantern_r >= 1.0f && s->lantern_r <= 32.0f) ? s->lantern_r : 5.0f;
    int lx = s->tx[0], ly = s->ty[0];
    if (lx < 0 || ly < 0 || lx >= t->mw || ly >= t->mh) { SDL_Log("tilefield: restored tile %d,%d is off %s", lx, ly, map); return; }
    tf_place_party(t, lx, ly, s->facing[0] & 3);
    for (int i = 1; i < TF_PARTY; i++) {                         // the trail, where it is still legal
        int x = s->tx[i], y = s->ty[i];
        if (x < 0 || y < 0 || x >= t->mw || y >= t->mh || cell_solid(t, x, y)) continue;
        t->act[i].tx = t->act[i].px = (short)x;
        t->act[i].ty = t->act[i].py = (short)y;
        t->act[i].facing = s->facing[i] & 3;
    }
}
