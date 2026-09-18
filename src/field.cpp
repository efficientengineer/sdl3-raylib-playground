// field.cpp — the walkable world. 2.5D: tiled ground/walls in real 3D, everything else a billboard.
// Renders into an offscreen FBO during tick() because the host clears the screen after tick and then
// draws ImGui; the FBO texture goes into ImGui's background draw list as one nearest-filtered image.
//
// Movement runs on a hand-authored NAVMESH (`## nav`), not on the tile grid: the tiles, walls and
// props are rendering only. Cameras are per-zone shots (`## cameras`) so a street can be an FF7-style
// depth shot. See FIELD.md for the art contract and src/FIELD_NOTES.md for both additions.
#include <SDL3/SDL.h>
#include <math.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

#ifdef __ANDROID__
#include <GLES3/gl3.h>
#else
#include <OpenGL/gl3.h>
#endif

#include "imgui.h"
#include "stb_image.h"          // implementation is in star_logic.cpp; this is declarations only
#include "field.h"
#include "field_text.h"

#define PREF_ORG "com.playground"
#define PREF_APP "questglory"

#define FM_W 64
#define FM_H 64
#define F_TILES 16
#define F_PROPS 160
#define F_NPCS 40
#define F_EXITS 16
#define F_TRIGS 48
#define F_ART 64
#define F_POLYS 128
#define F_POLY_V 8
#define F_ZONES 24
#define F_WALLS 64
#define F_WALL_PTS 48
#define F_PROFS 48
#define F_PROF_PTS 40
#define F_SWEEPS 64
#define F_LATHES 16

#define FBO_W 1024              // widest internal buffer; the used width tracks the screen aspect
#define FBO_H 360               // FIELD.md's 640x360, height fixed, width filled to the phone's 20:9
#define HALF_STEP 0.25f         // FIELD.md: a half-step is 0.25 units; nav heights use the same unit
#define PX_PER_CELL 64.0f       // art contract: sprites are drawn 1 px = 1 internal px at 64/cell

#define WALK_SPEED 3.2f
#define RUN_MULT 1.75f
#define NPC_RADIUS 0.55f
#define WALK_RADIUS 0.30f     // the walker is a disc, not a point: keep it off the fences
#define ZONE_BLEND 0.40f
#define DEG (3.14159265f / 180.0f)

// ───────────────────────── tiny matrix maths ─────────────────────────
// cglm is fetched for the desktop build but the Android game_logic target never links it, and the
// three helpers below are all a fixed-shot camera needs. Column-major, GL order.

static void m_mul(float *o, const float *a, const float *b) {
    float r[16];
    for (int c = 0; c < 4; c++)
        for (int i = 0; i < 4; i++)
            r[c * 4 + i] = a[i] * b[c * 4] + a[4 + i] * b[c * 4 + 1] + a[8 + i] * b[c * 4 + 2] + a[12 + i] * b[c * 4 + 3];
    memcpy(o, r, sizeof(r));
}

static void m_persp(float *m, float fovy_deg, float aspect, float zn, float zf) {
    float f = 1.0f / tanf(fovy_deg * DEG * 0.5f);
    memset(m, 0, 64);
    m[0] = f / aspect; m[5] = f;
    m[10] = (zf + zn) / (zn - zf); m[11] = -1.0f;
    m[14] = 2.0f * zf * zn / (zn - zf);
}

static void v_norm(float *v) {
    float l = sqrtf(v[0] * v[0] + v[1] * v[1] + v[2] * v[2]);
    if (l > 1e-6f) { v[0] /= l; v[1] /= l; v[2] /= l; }
}

static void m_look(float *m, const float *eye, const float *at) {
    float fwd[3] = {at[0] - eye[0], at[1] - eye[1], at[2] - eye[2]};
    if (fabsf(fwd[0]) + fabsf(fwd[1]) + fabsf(fwd[2]) < 1e-5f) fwd[2] = -1.0f;
    v_norm(fwd);
    float up[3] = {0, 1, 0};
    if (fabsf(fwd[1]) > 0.999f) { up[1] = 0; up[2] = 1; }            // straight-down shot needs another up
    float s[3] = {fwd[1] * up[2] - fwd[2] * up[1], fwd[2] * up[0] - fwd[0] * up[2], fwd[0] * up[1] - fwd[1] * up[0]};
    v_norm(s);
    float u[3] = {s[1] * fwd[2] - s[2] * fwd[1], s[2] * fwd[0] - s[0] * fwd[2], s[0] * fwd[1] - s[1] * fwd[0]};
    m[0] = s[0]; m[1] = u[0]; m[2] = -fwd[0]; m[3] = 0;
    m[4] = s[1]; m[5] = u[1]; m[6] = -fwd[1]; m[7] = 0;
    m[8] = s[2]; m[9] = u[2]; m[10] = -fwd[2]; m[11] = 0;
    m[12] = -(s[0] * eye[0] + s[1] * eye[1] + s[2] * eye[2]);
    m[13] = -(u[0] * eye[0] + u[1] * eye[1] + u[2] * eye[2]);
    m[14] = (fwd[0] * eye[0] + fwd[1] * eye[1] + fwd[2] * eye[2]);
    m[15] = 1;
}

// ───────────────────────── placeholder art: a 4-channel pixel buffer ─────────────────────────

struct Col { unsigned char r, g, b, a; };
struct Img { unsigned char *p; int w, h; };

static Col col(int r, int g, int b, int a = 255) {
    Col c = { (unsigned char)(r < 0 ? 0 : r > 255 ? 255 : r), (unsigned char)(g < 0 ? 0 : g > 255 ? 255 : g),
              (unsigned char)(b < 0 ? 0 : b > 255 ? 255 : b), (unsigned char)a };
    return c;
}
static Img img_new(int w, int h) { Img i = { (unsigned char *)calloc((size_t)w * h * 4, 1), w, h }; return i; }
static void img_put(Img *im, int x, int y, Col c) {
    if (x < 0 || y < 0 || x >= im->w || y >= im->h) return;
    unsigned char *p = im->p + ((size_t)y * im->w + x) * 4;
    p[0] = c.r; p[1] = c.g; p[2] = c.b; p[3] = c.a;
}
static Col img_at(Img *im, int x, int y) {
    Col c = {0, 0, 0, 0};
    if (x < 0 || y < 0 || x >= im->w || y >= im->h) return c;
    unsigned char *p = im->p + ((size_t)y * im->w + x) * 4;
    c.r = p[0]; c.g = p[1]; c.b = p[2]; c.a = p[3];
    return c;
}
static void img_rect(Img *im, int x0, int y0, int x1, int y1, Col c) {
    for (int y = y0; y < y1; y++) for (int x = x0; x < x1; x++) img_put(im, x, y, c);
}
static void img_disc(Img *im, float cx, float cy, float rx, float ry, Col c) {
    for (int y = (int)(cy - ry) - 1; y <= (int)(cy + ry) + 1; y++)
        for (int x = (int)(cx - rx) - 1; x <= (int)(cx + rx) + 1; x++) {
            float dx = (x + 0.5f - cx) / rx, dy = (y + 0.5f - cy) / ry;
            if (dx * dx + dy * dy <= 1.0f) img_put(im, x, y, c);
        }
}
// Filled triangle, used for every gable roof.
static void img_tri(Img *im, float ax, float ay, float bx, float by, float cx, float cy, Col c) {
    int x0 = (int)fminf(ax, fminf(bx, cx)), x1 = (int)fmaxf(ax, fmaxf(bx, cx)) + 1;
    int y0 = (int)fminf(ay, fminf(by, cy)), y1 = (int)fmaxf(ay, fmaxf(by, cy)) + 1;
    float d = (by - cy) * (ax - cx) + (cx - bx) * (ay - cy);
    if (fabsf(d) < 1e-5f) return;
    for (int y = y0; y <= y1; y++) for (int x = x0; x <= x1; x++) {
        float px = x + 0.5f, py = y + 0.5f;
        float l0 = ((by - cy) * (px - cx) + (cx - bx) * (py - cy)) / d;
        float l1 = ((cy - ay) * (px - cx) + (ax - cx) * (py - cy)) / d;
        if (l0 >= 0 && l1 >= 0 && l0 + l1 <= 1.0f) img_put(im, x, y, c);
    }
}
// Grows a dark rim outward so the silhouette reads against any tile, like the pixel art will.
static void img_outline(Img *im, Col line, int thick) {
    for (int t = 0; t < thick; t++) {
        unsigned char *mask = (unsigned char *)calloc((size_t)im->w * im->h, 1);
        if (!mask) return;
        for (int y = 0; y < im->h; y++) for (int x = 0; x < im->w; x++) {
            if (img_at(im, x, y).a) continue;
            if (img_at(im, x - 1, y).a || img_at(im, x + 1, y).a || img_at(im, x, y - 1).a || img_at(im, x, y + 1).a)
                mask[(size_t)y * im->w + x] = 1;
        }
        for (int y = 0; y < im->h; y++) for (int x = 0; x < im->w; x++)
            if (mask[(size_t)y * im->w + x]) img_put(im, x, y, line);
        free(mask);
    }
}

static uint32_t fhash(int a, int b, int c) {
    uint32_t h = (uint32_t)a * 374761393u + (uint32_t)b * 668265263u + (uint32_t)c * 2246822519u;
    h ^= h >> 13; h *= 1274126177u; h ^= h >> 16;
    return h;
}
static float frnd(int a, int b, int c) { return (float)(fhash(a, b, c) & 0xFFFF) / 65535.0f; }
// Seamless value noise over a 64x64 tile: the lattice wraps at `cells`, so all four edges match.
static float snoise(float x, float y, int cells, int seed) {
    float u = x * cells / 64.0f, v = y * cells / 64.0f;
    int i = (int)floorf(u), j = (int)floorf(v);
    float fu = u - i, fv = v - j;
    fu = fu * fu * (3 - 2 * fu); fv = fv * fv * (3 - 2 * fv);
    float a = frnd((i) % cells, (j) % cells, seed), b = frnd((i + 1) % cells, (j) % cells, seed);
    float c = frnd((i) % cells, (j + 1) % cells, seed), d = frnd((i + 1) % cells, (j + 1) % cells, seed);
    float t = a + (b - a) * fu;
    return t + ((c + (d - c) * fu) - t) * fv;
}

// ───────────────────────── placeholder tiles ─────────────────────────

static void gen_tile(const char *id, Img *im) {
    *im = img_new(64, 64);
    int base_r = 90, base_g = 90, base_b = 95, seed = (int)fhash((int)strlen(id), id[0], 7);
    bool plank = false, blocky = false, wave = false, stripe = false;
    if (!strcmp(id, "grass")) { base_r = 62; base_g = 104; base_b = 54; }
    else if (!strcmp(id, "dirt")) { base_r = 122; base_g = 95; base_b = 64; }
    else if (!strcmp(id, "stone")) { base_r = 126; base_g = 124; base_b = 122; blocky = true; }
    else if (!strcmp(id, "plank")) { base_r = 136; base_g = 100; base_b = 62; plank = true; }
    else if (!strcmp(id, "water")) { base_r = 42; base_g = 78; base_b = 118; wave = true; }
    else if (!strcmp(id, "cliff")) { base_r = 104; base_g = 92; base_b = 78; stripe = true; }
    else if (!strcmp(id, "wall_plaster")) { base_r = 186; base_g = 172; base_b = 144; }
    else if (!strcmp(id, "wall_timber")) { base_r = 182; base_g = 168; base_b = 140; }
    else if (!strcmp(id, "wall_stone")) { base_r = 132; base_g = 128; base_b = 120; blocky = true; }
    else if (!strcmp(id, "wall_stone_top")) { base_r = 148; base_g = 144; base_b = 134; blocky = true; }
    else if (!strcmp(id, "fence_wood")) { base_r = 132; base_g = 98; base_b = 62; plank = true; }
    else if (!strcmp(id, "fence_top")) { base_r = 112; base_g = 84; base_b = 54; plank = true; }
    else if (!strcmp(id, "hedge")) { base_r = 58; base_g = 98; base_b = 52; }
    else if (!strcmp(id, "hedge_top")) { base_r = 68; base_g = 112; base_b = 58; }

    for (int y = 0; y < 64; y++) for (int x = 0; x < 64; x++) {
        float n = snoise((float)x, (float)y, 8, seed) * 0.6f + snoise((float)x, (float)y, 16, seed + 1) * 0.4f;
        float k = 0.86f + n * 0.28f;
        if (wave) k = 0.85f + 0.3f * snoise((float)x, (float)(y * 2 % 64), 4, seed);
        if (plank) {                                     // 16 px boards with a dark seam
            int row = y / 16;
            k *= 0.92f + 0.10f * frnd(row, 0, seed);
            if (y % 16 == 0) k *= 0.55f;
            if ((x + row * 21) % 32 == 0) k *= 0.62f;
        }
        if (blocky) {                                    // offset courses of stone with mortar
            int row = y / 16, offs = (row & 1) ? 16 : 0;
            k *= 0.90f + 0.16f * frnd(row, (x + offs) / 32, seed);
            if (y % 16 == 0 || (x + offs) % 32 == 0) k *= 0.66f;
        }
        if (stripe) k *= 0.80f + 0.35f * snoise((float)(x * 3 % 64), (float)y, 8, seed + 3);
        img_put(im, x, y, col((int)(base_r * k), (int)(base_g * k), (int)(base_b * k)));
    }
    if (!strcmp(id, "grass"))                            // a few blades so the ground is not flat noise
        for (int i = 0; i < 90; i++) {
            int x = (int)(frnd(i, 1, seed) * 63), y = (int)(frnd(i, 2, seed) * 63);
            img_rect(im, x, y, x + 1, y + 2, col(base_r + 26, base_g + 34, base_b + 18));
        }
    if (!strcmp(id, "wall_timber")) {                    // dark beams over plaster
        img_rect(im, 0, 0, 64, 6, col(74, 52, 34));
        img_rect(im, 0, 58, 64, 64, col(74, 52, 34));
        img_rect(im, 4, 0, 10, 64, col(78, 56, 36));
        img_rect(im, 42, 0, 48, 64, col(78, 56, 36));
    }
    if (!strcmp(id, "hedge") || !strcmp(id, "hedge_top"))   // leafy clumps, so it is not flat green
        for (int i = 0; i < 160; i++) {
            int x = (int)(frnd(i, 7, seed) * 60), y = (int)(frnd(i, 8, seed) * 60);
            img_disc(im, (float)x, (float)y, 3.0f, 2.4f, col(base_r + (int)(frnd(i, 9, seed) * 40) - 12,
                                                            base_g + (int)(frnd(i, 10, seed) * 44) - 10, base_b - 4));
        }
    if (!strcmp(id, "water"))                            // highlights, so the UV scroll reads as flow
        for (int i = 0; i < 40; i++) {
            int x = (int)(frnd(i, 5, seed) * 57), y = (int)(frnd(i, 6, seed) * 63);
            img_rect(im, x, y, x + 7, y + 1, col(120, 170, 205));
        }
}

// ───────────────────────── placeholder props ─────────────────────────

enum { PK_HOUSE, PK_HALL, PK_SHED, PK_TREE, PK_WELL, PK_CART, PK_FENCE, PK_POST, PK_BARREL, PK_SIGN, PK_STONE, PK_LADDER, PK_GATE, PK_BENCH, PK_YARDWALL, PK_MARK, PK_MISSING };
struct PropDef { const char *id; short w, h; short kind; unsigned char r, g, b; };
static const PropDef PROP_DEFS[] = {
    { "house_a",       192, 184, PK_HOUSE,  196, 180, 150 },
    { "house_b",       132, 160, PK_HOUSE,  178, 168, 146 },
    { "guild_hall",    260, 216, PK_HALL,   170, 160, 142 },
    { "grain_shed",    196, 152, PK_SHED,   152, 122,  82 },
    { "ladder_house",  136, 184, PK_LADDER, 186, 172, 146 },
    { "well",           68,  92, PK_WELL,   130, 128, 124 },
    { "cart",          100,  76, PK_CART,   134, 100,  62 },
    { "fence",          68,  52, PK_FENCE,  128,  98,  62 },
    { "tree_a",         96, 168, PK_TREE,    58, 102,  54 },
    { "tree_b",         84, 132, PK_TREE,    70, 112,  58 },
    { "practice_post",  36, 100, PK_POST,   126,  96,  60 },
    { "barrel",         44,  60, PK_BARREL, 120,  92,  58 },
    { "sign",           44,  68, PK_SIGN,   132, 102,  64 },
    { "milestone",      36,  52, PK_STONE,  136, 134, 128 },
    { "gate_post",      40, 132, PK_GATE,   120,  92,  58 },
    { "yard_wall",     128,  84, PK_YARDWALL,126, 122, 114 },
    { "scale_bench",    96,  64, PK_BENCH,  128,  98,  62 },
    { "mark_examine",   20,  28, PK_MARK,   255, 245, 210 },
};

static void gen_prop(const char *id, Img *im) {
    const PropDef *d = nullptr;
    for (unsigned i = 0; i < sizeof(PROP_DEFS) / sizeof(PROP_DEFS[0]); i++)
        if (!strcmp(PROP_DEFS[i].id, id)) { d = &PROP_DEFS[i]; break; }
    PropDef fallback = { id, 64, 64, PK_MISSING, 255, 0, 255 };   // unmistakable: art is missing
    if (!d) { d = &fallback; SDL_Log("field: no prop recipe or art for \"%s\"", id); }
    int W = d->w, H = d->h, seed = (int)fhash(d->w, d->h, id[0]);
    *im = img_new(W, H);
    Col body = col(d->r, d->g, d->b), dark = col(d->r * 6 / 10, d->g * 6 / 10, d->b * 6 / 10);
    Col roof = col(112, 62, 52), wood = col(96, 68, 44), shadow = col(28, 22, 30);

    switch (d->kind) {
    case PK_HOUSE: case PK_HALL: case PK_LADDER: {
        int ry = (int)(H * (d->kind == PK_HALL ? 0.46f : 0.40f));
        img_rect(im, 4, ry, W - 4, H - 3, body);
        img_tri(im, 0.0f, (float)ry + 6, (float)W, (float)ry + 6, W * 0.5f, 3.0f, roof);
        img_rect(im, 0, ry + 2, W, ry + 10, col(88, 48, 40));          // eaves
        int dw = d->kind == PK_HALL ? 44 : 26;
        img_rect(im, W / 2 - dw / 2, H - 3 - (int)(H * 0.30f), W / 2 + dw / 2, H - 3, col(72, 50, 36));
        for (int i = 0; i < (d->kind == PK_HALL ? 4 : 2); i++) {
            int wx = 12 + i * (W - 34) / (d->kind == PK_HALL ? 3 : 1);
            img_rect(im, wx, ry + 20, wx + 18, ry + 40, col(58, 66, 92));
            img_rect(im, wx + 8, ry + 20, wx + 10, ry + 40, wood);
        }
        if (d->kind == PK_HALL) {                                       // a guild banner, so it reads as the hall
            img_rect(im, W / 2 - 10, ry + 8, W / 2 + 10, ry + 46, col(150, 42, 46));
            img_tri(im, (float)(W / 2 - 10), (float)ry + 46, (float)(W / 2 + 10), (float)ry + 46, (float)(W / 2), (float)ry + 58, col(150, 42, 46));
        }
        if (d->kind == PK_LADDER) {                                     // the ladder house wears its ladder
            img_rect(im, W - 30, ry - 4, W - 26, H - 3, wood);
            img_rect(im, W - 16, ry - 4, W - 12, H - 3, wood);
            for (int y = ry; y < H - 6; y += 12) img_rect(im, W - 30, y, W - 12, y + 3, wood);
        }
    } break;
    case PK_SHED: {
        int ry = (int)(H * 0.34f);
        img_rect(im, 4, ry, W - 4, H - 3, body);
        img_tri(im, 0.0f, (float)ry + 8, (float)W, (float)ry + 8, W * 0.5f, 2.0f, col(150, 128, 70));   // thatch
        img_rect(im, W / 2 - 34, ry + 22, W / 2 + 34, H - 3, col(44, 34, 30));                           // open front
        for (int i = 0; i < 5; i++) img_rect(im, W / 2 - 30 + i * 13, H - 26, W / 2 - 22 + i * 13, H - 5, col(176, 150, 78));
    } break;
    case PK_TREE: {
        img_rect(im, W / 2 - 7, H - (int)(H * 0.34f), W / 2 + 7, H - 3, col(84, 60, 40));
        img_disc(im, W * 0.5f, H * 0.32f, W * 0.46f, H * 0.28f, body);
        img_disc(im, W * 0.30f, H * 0.46f, W * 0.30f, H * 0.20f, dark);
        img_disc(im, W * 0.70f, H * 0.44f, W * 0.30f, H * 0.20f, body);
        img_disc(im, W * 0.44f, H * 0.20f, W * 0.30f, H * 0.16f, col(d->r + 26, d->g + 30, d->b + 18));
    } break;
    case PK_WELL: {
        img_disc(im, W * 0.5f, H - 16.0f, W * 0.44f, 14.0f, body);
        img_disc(im, W * 0.5f, H - 20.0f, W * 0.32f, 9.0f, col(24, 30, 44));
        img_rect(im, 8, 14, 13, H - 20, wood);
        img_rect(im, W - 13, 14, W - 8, H - 20, wood);
        img_tri(im, 0.0f, 18.0f, (float)W, 18.0f, W * 0.5f, 0.0f, roof);
        img_rect(im, 10, 24, W - 10, 28, wood);
    } break;
    case PK_CART: {
        img_rect(im, 6, H - 44, W - 6, H - 18, body);
        img_rect(im, 6, H - 44, W - 6, H - 38, dark);
        img_disc(im, 20.0f, H - 14.0f, 13.0f, 13.0f, wood);
        img_disc(im, W - 20.0f, H - 14.0f, 13.0f, 13.0f, wood);
        img_disc(im, 20.0f, H - 14.0f, 5.0f, 5.0f, shadow);
        img_disc(im, W - 20.0f, H - 14.0f, 5.0f, 5.0f, shadow);
    } break;
    case PK_FENCE: {
        img_rect(im, 0, H - 34, W, H - 28, wood);
        img_rect(im, 0, H - 20, W, H - 14, wood);
        for (int i = 0; i < 3; i++) img_rect(im, 3 + i * (W - 10) / 2, H - 42, 9 + i * (W - 10) / 2, H - 3, body);
    } break;
    case PK_POST: {
        img_rect(im, W / 2 - 7, 10, W / 2 + 7, H - 3, body);
        img_rect(im, 3, 22, W - 3, 30, wood);                             // crossbar
        img_rect(im, W / 2 - 9, 10, W / 2 + 9, 22, col(178, 152, 82));     // straw head
    } break;
    case PK_BARREL: {
        img_disc(im, W * 0.5f, H * 0.58f, W * 0.44f, H * 0.42f, body);
        img_rect(im, 2, (int)(H * 0.38f), W - 2, (int)(H * 0.38f) + 4, dark);
        img_rect(im, 2, (int)(H * 0.72f), W - 2, (int)(H * 0.72f) + 4, dark);
        img_disc(im, W * 0.5f, H * 0.20f, W * 0.40f, 6.0f, col(96, 72, 46));
    } break;
    case PK_GATE: {                                                   // a road-side gate post
        img_rect(im, W / 2 - 8, 16, W / 2 + 8, H - 3, body);
        img_rect(im, W / 2 - 13, 6, W / 2 + 13, 18, wood);
        img_tri(im, (float)(W / 2 - 13), 8.0f, (float)(W / 2 + 13), 8.0f, W * 0.5f, 0.0f, roof);
        img_rect(im, W / 2 - 10, H - 12, W / 2 + 10, H - 3, dark);
    } break;
    case PK_SIGN: {
        img_rect(im, W / 2 - 4, H / 2, W / 2 + 4, H - 3, wood);
        img_rect(im, 2, 6, W - 2, H / 2 + 2, body);
        img_rect(im, 8, 14, W - 8, 18, dark);
        img_rect(im, 8, 24, W - 14, 28, dark);
    } break;
    case PK_YARDWALL: {
        img_rect(im, 0, 12, W, H - 3, body);
        for (int y = 12; y < H - 3; y += 14) img_rect(im, 0, y, W, y + 2, dark);
        img_rect(im, 0, 8, W, 14, col(d->r + 14, d->g + 14, d->b + 14));    // coping
    } break;
    case PK_BENCH: {
        img_rect(im, 4, H - 26, W - 4, H - 18, body);
        img_rect(im, 10, H - 18, 18, H - 3, dark);
        img_rect(im, W - 18, H - 18, W - 10, H - 3, dark);
    } break;
    case PK_MARK: {                                               // the examine "!"
        img_rect(im, W / 2 - 3, 3, W / 2 + 3, H - 12, body);
        img_rect(im, W / 2 - 3, H - 8, W / 2 + 3, H - 2, body);
    } break;
    case PK_MISSING: {                                            // magenta/black hazard checker
        for (int y = 0; y < H; y++) for (int x = 0; x < W; x++)
            img_put(im, x, y, ((x / 8 + y / 8) & 1) ? col(255, 0, 255) : col(20, 20, 20));
        img_rect(im, 0, H / 2 - 3, W, H / 2 + 3, col(255, 255, 0));
    } break;
    default: {
        img_disc(im, W * 0.5f, (float)H, W * 0.46f, H * 0.80f, body);
        img_rect(im, 0, H - 4, W, H, dark);
    } break;
    }

    for (int y = 0; y < H; y++) for (int x = 0; x < W; x++) {         // grain plus a top-lit gradient
        Col c = img_at(im, x, y);
        if (!c.a) continue;
        float k = 0.90f + 0.14f * frnd(x, y, seed) + 0.16f * (1.0f - (float)y / H);
        img_put(im, x, y, col((int)(c.r * k), (int)(c.g * k), (int)(c.b * k), 255));
    }
    img_outline(im, col(22, 18, 28), 2);
}

// ───────────────────── placeholder blockers and building faces ─────────────────────
// A strip that stands on an unshared navmesh edge, so "you can't walk there" is always visible
// instead of being an invisible wall in the middle of a field of grass.

static void gen_edge(const char *kind, Img *im) {
    *im = img_new(64, 64);
    int seed = (int)fhash(kind[0], (int)strlen(kind), 91);
    if (!strcmp(kind, "hedge")) {
        for (int x = 0; x < 64; x++) {
            int top = 8 + (int)(9 * snoise((float)x, 0.0f, 8, seed));
            for (int y = top; y < 64; y++) {
                float k = 0.70f + 0.42f * snoise((float)x, (float)y, 10, seed) + 0.22f * (1.0f - (float)y / 64);
                img_put(im, x, y, col((int)(58 * k), (int)(98 * k), (int)(52 * k)));
            }
        }
    } else if (!strcmp(kind, "wall")) {
        for (int y = 5; y < 64; y++) for (int x = 0; x < 64; x++) {
            int row = y / 14, offs = (row & 1) ? 16 : 0;
            float k = 0.86f + 0.20f * frnd(row, (x + offs) / 32, seed);
            if (y % 14 == 0 || (x + offs) % 32 == 0) k *= 0.66f;
            img_put(im, x, y, col((int)(130 * k), (int)(126 * k), (int)(118 * k)));
        }
    } else {                                                 // fence: posts and two rails, seamless at 64
        img_rect(im, 0, 7, 7, 64, col(128, 96, 60));
        img_rect(im, 32, 7, 39, 64, col(128, 96, 60));
        img_rect(im, 0, 20, 64, 27, col(104, 76, 48));
        img_rect(im, 0, 41, 64, 48, col(104, 76, 48));
        for (int y = 0; y < 64; y++) for (int x = 0; x < 64; x++) {
            Col c = img_at(im, x, y);
            if (!c.a) continue;
            float k = 0.88f + 0.20f * frnd(x, y, seed);
            img_put(im, x, y, col((int)(c.r * k), (int)(c.g * k), (int)(c.b * k), 255));
        }
    }
    img_outline(im, col(22, 18, 26), 1);
}

// One face of a building. `<id>_front` is stretched across the whole front wall (so a painted door
// lands where the artist put it), `<id>_side` tiles once per cell and per world unit, `<id>_roof`
// tiles once per cell. See src/FIELD_NOTES.md for the contract the art pipeline builds to.
static void gen_bface(const char *id, Img *im) {
    const char *u = strrchr(id, '_');
    const char *part = u ? u + 1 : "side";
    int seed = (int)fhash((int)strlen(id), id[0], 33);
    if (!strcmp(part, "roof")) {
        *im = img_new(64, 64);
        for (int y = 0; y < 64; y++) for (int x = 0; x < 64; x++) {
            int row = y / 8, offs = (row & 1) ? 8 : 0;
            float k = 0.86f + 0.16f * frnd(row, (x + offs) / 16, seed) + 0.12f * (1.0f - (float)(y % 8) / 8.0f);
            if ((x + offs) % 16 == 0) k *= 0.70f;
            if (y % 8 == 0) k *= 0.76f;
            img_put(im, x, y, col((int)(128 * k), (int)(68 * k), (int)(58 * k)));
        }
        return;
    }
    *im = img_new(128, 128);
    bool front = !strcmp(part, "front");
    for (int y = 0; y < 128; y++) for (int x = 0; x < 128; x++) {
        float k = 0.90f + 0.16f * snoise((float)(x % 64), (float)(y % 64), 12, seed) + 0.10f * (1.0f - (float)y / 128.0f);
        img_put(im, x, y, col((int)(198 * k), (int)(182 * k), (int)(152 * k)));
    }
    Col beam = col(86, 60, 40);
    img_rect(im, 0, 0, 128, 7, beam);                        // wall plate, so stacked units read as storeys
    img_rect(im, 0, 121, 128, 128, col(70, 62, 54));
    if (front) {
        img_rect(im, 50, 72, 78, 121, col(70, 48, 32));      // door
        img_rect(im, 62, 72, 66, 121, beam);
        img_rect(im, 18, 28, 48, 32, beam);
        img_rect(im, 80, 28, 110, 32, beam);
        img_rect(im, 20, 32, 46, 58, col(58, 66, 92));       // two windows
        img_rect(im, 82, 32, 108, 58, col(58, 66, 92));
        img_rect(im, 31, 32, 35, 58, beam);
        img_rect(im, 93, 32, 97, 58, beam);
    } else {
        img_rect(im, 4, 0, 12, 128, beam);                   // timbers
        img_rect(im, 60, 0, 68, 128, beam);
        img_rect(im, 0, 60, 128, 68, beam);
        img_rect(im, 32, 26, 56, 52, col(58, 66, 92));
    }
}

// ───────────────────────── placeholder walkers ─────────────────────────
// 4 rows (S, W, E, N) x 4 columns (stand, step-left, stand, step-right) of 32x48 frames.

static void gen_walker(const char *id, Img *im) {
    *im = img_new(128, 192);
    uint32_t h = fhash((int)strlen(id), id[0], id[1] ? id[1] : 3);
    Col tunic = col(70 + (int)(h % 130), 60 + (int)((h >> 8) % 120), 70 + (int)((h >> 16) % 130));
    if (!strcmp(id, "falke")) tunic = col(64, 96, 168);           // the player reads blue
    else if (!strcmp(id, "hart")) tunic = col(150, 60, 50);
    else if (!strcmp(id, "stolz")) tunic = col(58, 62, 74);
    else if (!strcmp(id, "clerk")) tunic = col(86, 76, 110);
    Col dark = col(tunic.r * 6 / 10, tunic.g * 6 / 10, tunic.b * 6 / 10);
    Col skin = col(214, 172, 132), hair = col(56, 40, 34), boot = col(62, 46, 38);

    for (int row = 0; row < 4; row++) for (int c = 0; c < 4; c++) {
        int ox = c * 32, oy = row * 48;
        int step = (c == 1) ? -1 : (c == 3) ? 1 : 0;
        // legs, offset by the step so the walk cycle reads at a glance
        img_rect(im, ox + 11 + step, oy + 34, ox + 15 + step, oy + 45, dark);
        img_rect(im, ox + 17 - step, oy + 34, ox + 21 - step, oy + 45, dark);
        img_rect(im, ox + 10 + step, oy + 43, ox + 16 + step, oy + 46, boot);
        img_rect(im, ox + 16 - step, oy + 43, ox + 22 - step, oy + 46, boot);
        // tunic and arms
        img_rect(im, ox + 9, oy + 18, ox + 23, oy + 36, tunic);
        img_rect(im, ox + 6, oy + 20, ox + 9, oy + 32 + step, dark);
        img_rect(im, ox + 23, oy + 20, ox + 26, oy + 32 - step, dark);
        img_rect(im, ox + 9, oy + 33, ox + 23, oy + 36, dark);
        // head
        img_disc(im, ox + 16.0f, oy + 12.0f, 6.5f, 7.0f, skin);
        img_rect(im, ox + 9, oy + 5, ox + 23, oy + 10, hair);
        if (row == 3) img_rect(im, ox + 9, oy + 5, ox + 23, oy + 17, hair);               // N: back of the head
        else if (row == 0) {                                                              // S: two eyes
            img_rect(im, ox + 12, oy + 12, ox + 14, oy + 14, col(30, 26, 30));
            img_rect(im, ox + 18, oy + 12, ox + 20, oy + 14, col(30, 26, 30));
        } else {                                                                          // W / E: one eye
            int ex = (row == 1) ? 11 : 19;
            img_rect(im, ox + ex, oy + 12, ox + ex + 2, oy + 14, col(30, 26, 30));
            if (row == 1) img_rect(im, ox + 19, oy + 6, ox + 23, oy + 16, hair);
            else img_rect(im, ox + 9, oy + 6, ox + 13, oy + 16, hair);
        }
    }
    img_outline(im, col(20, 16, 24), 1);
}

// ───────────────────────── textures and files ─────────────────────────

struct FTex { GLuint id; int w, h; };

static const char *pref_path() {
    static char path[512];
    if (!path[0]) { const char *p = SDL_GetPrefPath(PREF_ORG, PREF_APP); snprintf(path, sizeof(path), "%s", p ? p : ""); }
    return path;
}

// Phone pref path first (fast_reload.sh pushes there), then the APK assets — exactly like the panels.
static void *field_read(const char *rel, size_t *size) {
    char path[768];
    snprintf(path, sizeof(path), "%s%s", pref_path(), rel);
    void *d = SDL_LoadFile(path, size);
    if (!d) d = SDL_LoadFile(rel, size);
    return d;
}

static FTex ftex_upload(unsigned char *px, int w, int h, bool repeat, bool mip) {
    FTex t = {0, w, h};
    glGenTextures(1, &t.id);
    glBindTexture(GL_TEXTURE_2D, t.id);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, w, h, 0, GL_RGBA, GL_UNSIGNED_BYTE, px);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);            // pixel art stays crisp
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, repeat ? GL_REPEAT : GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, repeat ? GL_REPEAT : GL_CLAMP_TO_EDGE);
    if (mip) { glGenerateMipmap(GL_TEXTURE_2D); glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR); }
    else glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    return t;
}

// Ground layers blend by height as well as by weight, so every tile texture carries a height mask in
// its **alpha channel**: from `story/field/tiles/<id>_h.png` if the pipeline supplies one, else
// contrast-stretched from the tile's own luminance (slab faces bright/high, mortar and moss dark/low).
// Tiles are opaque, so the alpha was free and the shader still costs one sample per layer.
static void pack_height(const char *id, unsigned char *px, int w, int h) {
    char rel[256];
    snprintf(rel, sizeof(rel), "field/tiles/%s_h.png", id);
    size_t sz = 0;
    void *data = field_read(rel, &sz);
    if (data) {
        int mw = 0, mh = 0, mc = 0;
        unsigned char *m = stbi_load_from_memory((const unsigned char *)data, (int)sz, &mw, &mh, &mc, 4);
        SDL_free(data);
        if (m) {
            for (int y = 0; y < h; y++) for (int x = 0; x < w; x++) {
                int sx = mw == w ? x : x * mw / w, sy = mh == h ? y : y * mh / h;
                px[((size_t)y * w + x) * 4 + 3] = m[((size_t)sy * mw + sx) * 4];
            }
            stbi_image_free(m);
            return;
        }
    }
    int lo = 255, hi = 0;
    for (int i = 0; i < w * h; i++) {
        int l = (px[i * 4] * 77 + px[i * 4 + 1] * 151 + px[i * 4 + 2] * 28) >> 8;
        if (l < lo) lo = l;
        if (l > hi) hi = l;
    }
    int span = hi - lo < 8 ? 8 : hi - lo;
    for (int i = 0; i < w * h; i++) {
        int l = (px[i * 4] * 77 + px[i * 4 + 1] * 151 + px[i * 4 + 2] * 28) >> 8;
        int v = (l - lo) * 255 / span;
        px[i * 4 + 3] = (unsigned char)(v < 0 ? 0 : v > 255 ? 255 : v);
    }
}

// kind is "tiles" / "props" / "walkers"; falls back to the generated placeholder when the file is absent.
static FTex art_load(const char *kind, const char *id) {
    char rel[256];
    snprintf(rel, sizeof(rel), "field/%s/%s.png", kind, id);
    size_t size = 0;
    void *data = field_read(rel, &size);
    bool tile = !strcmp(kind, "tiles");
    bool rep = tile || !strcmp(kind, "edges") || !strcmp(kind, "buildings");   // these all tile
    if (data) {
        int w = 0, h = 0, n = 0;
        unsigned char *px = stbi_load_from_memory((const unsigned char *)data, (int)size, &w, &h, &n, 4);
        SDL_free(data);
        if (px) {
            // A sprite with no transparent pixel anywhere is not a sprite — it is a stub or a sheet
            // saved without its alpha, and drawing it gives a solid rectangle the size of its box.
            // Fall back to the placeholder and say so, rather than putting a coloured block in the town.
            bool clear = false;
            if (!tile) for (int i = 0; i < w * h && !clear; i++) if (px[i * 4 + 3] < 250) clear = true;
            if (tile || clear) {
                if (tile) pack_height(id, px, w, h);
                FTex t = ftex_upload(px, w, h, rep, rep);
                stbi_image_free(px);
                return t;
            }
            stbi_image_free(px);
            SDL_Log("field: %s/%s.png has no transparency, ignoring it", kind, id);
        }
    }
    SDL_Log("field: no %s art for \"%s\", drawing the placeholder", kind, id);
    Img im = {nullptr, 0, 0};
    if (tile) gen_tile(id, &im);
    else if (!strcmp(kind, "walkers")) gen_walker(id, &im);
    else if (!strcmp(kind, "edges")) gen_edge(id, &im);
    else if (!strcmp(kind, "buildings")) gen_bface(id, &im);
    else gen_prop(id, &im);
    if (tile) pack_height(id, im.p, im.w, im.h);
    FTex t = ftex_upload(im.p, im.w, im.h, rep, rep);
    free(im.p);
    return t;
}

// ───────────────────────── map data ─────────────────────────

struct FTile { char id[24]; FTex tex; bool water; };
struct FProp { short x, y, w, d; char id[24]; int art; };
struct FNpc { short x, y; int facing; char walker[24]; char name[32]; char scene[48]; char say[160]; int art; };
struct FExit { short x, y, w, h; char map[32]; short sx, sy; int facing; bool inside; };
enum { TG_MESSAGE, TG_SCENE, TG_ZONE, TG_TRAP };
struct FTrig { short x, y, w, h; int kind; char arg[160]; bool inside; };

// A convex navmesh polygon: vertices in map units with per-vertex height, so a ramp is one sloped
// polygon. `nb[i]` is the polygon across edge i (v[i] -> v[i+1]), or -1 for a wall.
struct NavPoly {
    int id, n;
    float vx[F_POLY_V], vz[F_POLY_V], vy[F_POLY_V];
    int nb[F_POLY_V];
    float cx, cz;                       // centroid, for the debug label
    float pa, pb, pc;                   // plane: y = pa*x + pb*z + pc
    char bkind[12];                     // `| fence|hedge|wall|none` — what to draw on its open edges
};

// A 3D building: a box with a gabled roof. It blocks nothing; the navmesh already excludes it.
struct Bld { float x, z, w, d, h, rot, pitch, eave; char id[24]; int a_front, a_side, a_roof; };

// A run of world triangles that shares one texture: edge strips and building faces, appended to the
// static world buffer after the tiles and walls.
struct GeoRange { int art, first, count; bool alpha; };

// A PROFILE is a 2D cross-section in the plane perpendicular to a path: x sideways, y up, with a
// tile and a repeat scale per segment. Sweep it along a path and you get a wall, a kerb, a bridge or
// a staircase; revolve it about a vertical axis and you get a well ring, a column or a tower.
struct FProfile {
    char id[24];
    int n, closed;
    float x[F_PROF_PTS], y[F_PROF_PTS], scale[F_PROF_PTS];
    int art[F_PROF_PTS];
};

struct FSweep {
    int prof, spline, caps, n;
    int cap_art0, cap_art1;              // per-end cap texture; -1 = the profile's first tile
    float along, s0, s1;                 // texture repeat along the path; profile scale start/end
    float px[F_WALL_PTS], pz[F_WALL_PTS], py[F_WALL_PTS];
    unsigned char fixed_y[F_WALL_PTS];   // 1 = the map gave an explicit height here
};

struct FLathe { int prof, segs; float x, z, y; unsigned char fixed_y; };

struct FVert { float x, y, z, u, v; unsigned char r, g, b, a; };

struct Field {
    char map_name[32];
    int mw, mh;
    unsigned char hgt[FM_H][FM_W], gnd[FM_H][FM_W];       // rendering only
    FTile tiles[F_TILES]; int tile_count, wall_tile;
    FProp props[F_PROPS]; int prop_count;
    FNpc npcs[F_NPCS]; int npc_count;
    FExit exits[F_EXITS]; int exit_count;
    FTrig trigs[F_TRIGS]; int trig_count;
    NavPoly polys[F_POLYS]; int poly_count;
    CamZone zones[F_ZONES]; int zone_count;
    Bld blds[48]; int bld_count;
    FProfile profs[F_PROFS]; int prof_count;
    FSweep sweeps[F_SWEEPS]; int sweep_count;
    FLathe lathes[F_LATHES]; int lathe_count;
    GeoRange geo[640]; int geo_count;
    char edge_kind[16];                  // map-wide default for open navmesh edges
    char landmark[96];                   // FIELD.md rule 9: the one memorable thing on this map
    // Ground splat: four weighted layers over the whole map instead of one tile id per cell.
    char splat_file[64]; int splat_n;    // texels per cell
    int layer_tile[4];                   // index into f->tiles
    float layer_cells[4];                // how many cells one tile of that layer spans
    float layer_sharp[4], layer_hk[4];   // per layer: 1 = smooth, 8+ = a crisp edge; height influence
    float splat_dither;
    FTex splat_tex;
    int floor_first, floor_count;        // every non-water ground quad, drawn in one splat pass
    short spawn_x, spawn_y; int spawn_f;

    struct { char key[48]; FTex tex; } art[F_ART];
    int art_count;

    float px, pz, py;                    // position in map units; py is the navmesh height in world units
    int poly, facing; float anim_t; bool walking;
    float water_t;

    int zone_idx; float blend_t;
    float eye[3], at[3], fov;            // live camera
    float b_eye[3], b_at[3], b_fov;      // where the blend started
    float foc[3];                        // follow focus: a little ahead of the player, lagging
    float frame_u;                       // 0 = the authored shot, 1 = looking straight at the player
    float move_dx, move_dz;              // smoothed heading, for the focus lead
    float warn_cool; char warn[160]; bool warn_pending;
    float see_r, see_radius, walk_radius; int see_on;   // the live see-through radius, its target, and the toggle
    int nav_debug, fill_width, show_blockers;

    bool gl_ready;
    GLuint prog, vao, vbo_world, vbo_spr, fbo, fbo_tex, fbo_depth;
    GLint u_mvp, u_uvoff, u_tex, u_alpha, u_see, u_occl;
    GLuint sprog;
    GLint s_mvp, s_splat, s_lay[4], s_scale, s_map, s_blend, s_sharp, s_hk;
    FTex white;
    int range_first[F_TILES], range_count[F_TILES];
    int wall_first, wall_count;
    FVert *spr; int spr_cap;
    float mvp[16];                       // last frame's, for projecting nav labels

    bool stick_on; SDL_FingerID stick_id; float stick_ox, stick_oy, stick_x, stick_y;
    bool act_on; SDL_FingerID act_id; float act_t; bool run, tapped;

    char msg[256], msg_who[40]; float msg_t;
    int exam_trig, exam_npc;   // what the interact press would open, -1 for none
};

static const char *FIELD_MAPS[] = { "halm", "hart_yard" };
int field_map_count() { return (int)(sizeof(FIELD_MAPS) / sizeof(FIELD_MAPS[0])); }
const char *field_map_name_at(int i) { return (i >= 0 && i < field_map_count()) ? FIELD_MAPS[i] : "halm"; }

// A map that always parses, so a missing or broken file is a flat green square and not a crash.
static const char *FALLBACK_MAP =
    "## meta\nname: void\nsize: 16 16\nspawn: 8 8 S\nground: grass\nwall: cliff\n"
    "## nav\n0: 1 1 0, 15 1 0, 15 15 0, 1 15 0\n";

// ───────────────────────── navmesh ─────────────────────────

static void poly_finish(NavPoly *p) {
    double ax = 0, az = 0, area = 0;
    for (int i = 0; i < p->n; i++) {
        int j = (i + 1) % p->n;
        area += (double)p->vx[i] * p->vz[j] - (double)p->vx[j] * p->vz[i];
        ax += p->vx[i]; az += p->vz[i];
    }
    if (area < 0) {                                   // normalise the winding so "inside" is one test
        for (int i = 0, j = p->n - 1; i < j; i++, j--) {
            float t;
            t = p->vx[i]; p->vx[i] = p->vx[j]; p->vx[j] = t;
            t = p->vz[i]; p->vz[i] = p->vz[j]; p->vz[j] = t;
            t = p->vy[i]; p->vy[i] = p->vy[j]; p->vy[j] = t;
        }
    }
    p->cx = (float)(ax / p->n); p->cz = (float)(az / p->n);
    // Plane through the three best-spread vertices, so height is interpolated, never sampled.
    int i0 = 0, i1 = 1, i2 = 2;
    float best = -1;
    for (int i = 1; i < p->n; i++) {
        float d = (p->vx[i] - p->vx[0]) * (p->vx[i] - p->vx[0]) + (p->vz[i] - p->vz[0]) * (p->vz[i] - p->vz[0]);
        if (d > best) { best = d; i1 = i; }
    }
    best = -1;
    for (int i = 1; i < p->n; i++) {
        if (i == i1) continue;
        float d = fabsf((p->vx[i1] - p->vx[0]) * (p->vz[i] - p->vz[0]) - (p->vz[i1] - p->vz[0]) * (p->vx[i] - p->vx[0]));
        if (d > best) { best = d; i2 = i; }
    }
    float x1 = p->vx[i1] - p->vx[i0], z1 = p->vz[i1] - p->vz[i0], y1 = p->vy[i1] - p->vy[i0];
    float x2 = p->vx[i2] - p->vx[i0], z2 = p->vz[i2] - p->vz[i0], y2 = p->vy[i2] - p->vy[i0];
    float det = x1 * z2 - x2 * z1;
    if (fabsf(det) < 1e-5f) { p->pa = p->pb = 0; p->pc = p->vy[0]; return; }
    p->pa = (y1 * z2 - y2 * z1) / det;
    p->pb = (x1 * y2 - x2 * y1) / det;
    p->pc = p->vy[i0] - p->pa * p->vx[i0] - p->pb * p->vz[i0];
}

static float poly_height(const NavPoly *p, float x, float z) { return p->pa * x + p->pb * z + p->pc; }

static bool poly_contains(const NavPoly *p, float x, float z, float eps) {
    for (int i = 0; i < p->n; i++) {
        int j = (i + 1) % p->n;
        float ex = p->vx[j] - p->vx[i], ez = p->vz[j] - p->vz[i];
        if (ex * (z - p->vz[i]) - ez * (x - p->vx[i]) < -eps) return false;
    }
    return true;
}

static int mesh_find(Field *f, float x, float z) {
    for (int i = 0; i < f->poly_count; i++) if (poly_contains(&f->polys[i], x, z, 1e-4f)) return i;
    return -1;
}
// Nearest polygon by centroid — only used to rescue a position that fell outside the mesh.
static int mesh_nearest(Field *f, float x, float z) {
    int best = -1; float bd = 1e18f;
    for (int i = 0; i < f->poly_count; i++) {
        float dx = f->polys[i].cx - x, dz = f->polys[i].cz - z, d = dx * dx + dz * dz;
        if (d < bd) { bd = d; best = i; }
    }
    return best;
}

static void mesh_link(Field *f) {
    for (int i = 0; i < f->poly_count; i++) for (int e = 0; e < f->polys[i].n; e++) f->polys[i].nb[e] = -1;
    for (int i = 0; i < f->poly_count; i++) {
        NavPoly *A = &f->polys[i];
        for (int e = 0; e < A->n; e++) {
            if (A->nb[e] >= 0) continue;
            int e2 = (e + 1) % A->n;
            for (int j = i + 1; j < f->poly_count && A->nb[e] < 0; j++) {
                NavPoly *B = &f->polys[j];
                for (int g = 0; g < B->n; g++) {
                    int g2 = (g + 1) % B->n;
                    bool same = (fabsf(A->vx[e] - B->vx[g2]) < 0.01f && fabsf(A->vz[e] - B->vz[g2]) < 0.01f &&
                                 fabsf(A->vx[e2] - B->vx[g]) < 0.01f && fabsf(A->vz[e2] - B->vz[g]) < 0.01f) ||
                                (fabsf(A->vx[e] - B->vx[g]) < 0.01f && fabsf(A->vz[e] - B->vz[g]) < 0.01f &&
                                 fabsf(A->vx[e2] - B->vx[g2]) < 0.01f && fabsf(A->vz[e2] - B->vz[g2]) < 0.01f);
                    if (same) { A->nb[e] = j; B->nb[g] = i; break; }
                }
            }
        }
    }
    // An edge with no neighbour whose midpoint sits inside another polygon means the author meant
    // them to join but the vertices don't match — the one navmesh bug worth shouting about.
    for (int i = 0; i < f->poly_count; i++) {
        NavPoly *A = &f->polys[i];
        for (int e = 0; e < A->n; e++) {
            if (A->nb[e] >= 0) continue;
            int e2 = (e + 1) % A->n;
            float mx = (A->vx[e] + A->vx[e2]) * 0.5f, mz = (A->vz[e] + A->vz[e2]) * 0.5f;
            for (int j = 0; j < f->poly_count; j++)
                if (j != i && poly_contains(&f->polys[j], mx, mz, -0.02f))
                    SDL_Log("field: nav poly %d edge %d overlaps poly %d but shares no vertices (%.2f,%.2f)",
                            A->id, e, f->polys[j].id, mx, mz);
        }
    }
}

// Slide the point along the mesh. Crossing an edge with a neighbour walks into it; an edge without
// one projects the rest of the motion along it. Penetration is impossible by construction.
static int mesh_move(Field *f, int poly, float *px, float *pz, float mx, float mz) {
    if (f->poly_count <= 0) return -1;
    if (poly < 0 || poly >= f->poly_count || !poly_contains(&f->polys[poly], *px, *pz, 1e-3f)) {
        int p = mesh_find(f, *px, *pz);
        poly = (p >= 0) ? p : mesh_nearest(f, *px, *pz);
        if (poly < 0) return -1;
    }
    float x = *px, z = *pz;
    for (int iter = 0; iter < 4; iter++) {
        if (fabsf(mx) < 1e-7f && fabsf(mz) < 1e-7f) break;
        NavPoly *P = &f->polys[poly];
        float tx = x + mx, tz = z + mz;
        int hit = -1; float best_t = 2.0f;
        for (int i = 0; i < P->n; i++) {
            int j = (i + 1) % P->n;
            float ex = P->vx[j] - P->vx[i], ez = P->vz[j] - P->vz[i];
            float d0 = ex * (z - P->vz[i]) - ez * (x - P->vx[i]);
            float d1 = ex * (tz - P->vz[i]) - ez * (tx - P->vx[i]);
            if (d1 >= 0.0f) continue;                       // the target is still inside this edge
            float den = d0 - d1, t = (den > 1e-8f) ? d0 / den : 0.0f;
            if (t < 0) t = 0;
            if (t < best_t) { best_t = t; hit = i; }
        }
        if (hit < 0) { x = tx; z = tz; break; }
        x += mx * best_t; z += mz * best_t;
        mx *= (1.0f - best_t); mz *= (1.0f - best_t);
        int j = (hit + 1) % P->n;
        float ex = P->vx[j] - P->vx[hit], ez = P->vz[j] - P->vz[hit];
        float len = sqrtf(ex * ex + ez * ez);
        if (len < 1e-6f) break;
        ex /= len; ez /= len;
        if (P->nb[hit] >= 0) {
            poly = P->nb[hit];
            x += ez * 1e-3f; z += -ex * 1e-3f;              // a hair past the edge, into the neighbour
        } else {
            float d = mx * ex + mz * ez;                    // wall: keep only the along-edge component
            mx = ex * d; mz = ez * d;
            x += -ez * 2e-3f; z += ex * 2e-3f;              // stay a hair inside
        }
    }
    if (!poly_contains(&f->polys[poly], x, z, 2e-3f)) {     // numerical rescue: never leave the mesh
        int p = mesh_find(f, x, z);
        if (p < 0) { x = *px; z = *pz; }
        else poly = p;
    }
    *px = x; *pz = z;
    return poly;
}

// The navmesh resolves a point, but the walker is a sprite with width. After the slide-resolve,
// push the point out of any UNSHARED edge (a fence, a wall, a cliff) closer than its radius and
// re-resolve, twice. Shared edges are left alone so crossings still line up exactly.
static int wall_buffer(Field *f, int poly, float *px, float *pz, float r) {
    if (poly < 0 || poly >= f->poly_count || r <= 0.0f) return poly;
    for (int iter = 0; iter < 2; iter++) {
        float mx = 0.0f, mz = 0.0f;
        NavPoly *cur = &f->polys[poly];
        for (int k = -1; k < cur->n; k++) {
            int pi = (k < 0) ? poly : cur->nb[k];
            if (pi < 0 || pi >= f->poly_count) continue;
            NavPoly *P = &f->polys[pi];
            for (int e = 0; e < P->n; e++) {
                if (P->nb[e] >= 0) continue;
                int j = (e + 1) % P->n;
                float ax = P->vx[e], az = P->vz[e], ex = P->vx[j] - ax, ez = P->vz[j] - az;
                float l2 = ex * ex + ez * ez;
                if (l2 < 1e-9f) continue;
                float t = ((*px - ax) * ex + (*pz - az) * ez) / l2;
                if (t < 0) t = 0;
                if (t > 1) t = 1;
                float qx = ax + ex * t, qz = az + ez * t;
                float dx = *px - qx, dz = *pz - qz, d = sqrtf(dx * dx + dz * dz);
                if (d >= r) continue;
                float len = sqrtf(l2);
                if (d < 1e-4f) { dx = -ez / len; dz = ex / len; d = 1e-4f; }   // on the line: inward normal
                else { dx /= d; dz /= d; }
                float push = r - d;
                mx += dx * push; mz += dz * push;
            }
        }
        if (fabsf(mx) < 1e-5f && fabsf(mz) < 1e-5f) break;
        int np = mesh_move(f, poly, px, pz, mx, mz);            // push through the mesh, never around it
        if (np >= 0) poly = np;
    }
    return poly;
}

static int art_get(Field *f, const char *kind, const char *id);
static float grid_h(Field *f, float x, float z);

// A profile is looked up by name, or built on the spot from a spec: `box:w:h[:side:top]`,
// `slab:w:h[:tile]`, `kerb:w:h[:tile]`, `bridge:w:h[:deck:parapet]`, `stair:rise:run:n[:tile]`.
// Built-ins are cached under their spec string, so repeats cost nothing.
static void prof_pt(FProfile *p, Field *f, float x, float y, const char *tile, float sc);

static int profile_get(Field *f, const char *spec) {
    for (int i = 0; i < f->prof_count; i++) if (!strcmp(f->profs[i].id, spec)) return i;
    if (!strchr(spec, ':') || f->prof_count >= F_PROFS) return -1;
    char buf[80];
    snprintf(buf, sizeof(buf), "%s", spec);
    char *a[6] = {buf, nullptr, nullptr, nullptr, nullptr, nullptr};
    int na = 1;
    for (char *c = buf; *c && na < 6; c++) if (*c == ':') { *c = 0; a[na++] = c + 1; }
    FProfile *p = &f->profs[f->prof_count];
    memset(p, 0, sizeof(*p));
    snprintf(p->id, sizeof(p->id), "%s", spec);
    p->closed = 1;
    float w = na > 1 ? (float)atof(a[1]) : 0.4f, h = na > 2 ? (float)atof(a[2]) : 0.8f;
    if (w < 0.02f) w = 0.4f;
    if (h < 0.02f) h = 0.8f;
    if (!strcmp(a[0], "box") || !strcmp(a[0], "slab")) {
        const char *side = na > 3 ? a[3] : "wall_stone", *top = na > 4 ? a[4] : (na > 3 ? a[3] : "wall_stone_top");
        prof_pt(p, f, -w / 2, 0, side, 1); prof_pt(p, f, -w / 2, h, top, 1);
        prof_pt(p, f, w / 2, h, side, 1);  prof_pt(p, f, w / 2, 0, side, 1);
    } else if (!strcmp(a[0], "kerb")) {
        const char *tl = na > 3 ? a[3] : "wall_stone";
        prof_pt(p, f, -w / 2, 0, tl, 1); prof_pt(p, f, -w / 2, h * 0.7f, tl, 1);
        prof_pt(p, f, -w * 0.3f, h, tl, 1); prof_pt(p, f, w * 0.3f, h, tl, 1);
        prof_pt(p, f, w / 2, h * 0.7f, tl, 1); prof_pt(p, f, w / 2, 0, tl, 1);
    } else if (!strcmp(a[0], "bridge")) {
        const char *deck = na > 3 ? a[3] : "plank", *par = na > 4 ? a[4] : "wall_stone";
        float pw = w * 0.10f, ph = h * 1.6f;
        prof_pt(p, f, -w / 2, 0, par, 1);          prof_pt(p, f, -w / 2, h + ph, par, 1);
        prof_pt(p, f, -w / 2 + pw, h + ph, par, 1); prof_pt(p, f, -w / 2 + pw, h, deck, 1);
        prof_pt(p, f, w / 2 - pw, h, par, 1);      prof_pt(p, f, w / 2 - pw, h + ph, par, 1);
        prof_pt(p, f, w / 2, h + ph, par, 1);      prof_pt(p, f, w / 2, 0, par, 1);
    } else if (!strcmp(a[0], "house")) {
        // house:width:wallheight:pitch:eave[:side:roof] — floor, wall, soffit, roof to the ridge,
        // and back down. Swept along the building's length, so the end caps are its gable ends.
        float pitch = na > 3 ? (float)atof(a[3]) : 0.55f, eave = na > 4 ? (float)atof(a[4]) : 0.18f;
        const char *side = na > 5 ? a[5] : "wall_plaster", *roof = na > 6 ? a[6] : "wall_timber";
        if (pitch < 0.05f) pitch = 0.55f;
        if (eave < 0.0f) eave = 0.0f;
        float hw2 = w * 0.5f, ridge = h + (hw2 + eave) * pitch;
        prof_pt(p, f, -hw2, 0, side, 1);
        prof_pt(p, f, -hw2, h, side, 1);
        prof_pt(p, f, -hw2 - eave, h, roof, 1);
        prof_pt(p, f, 0, ridge, roof, 1);
        prof_pt(p, f, hw2 + eave, h, side, 1);
        prof_pt(p, f, hw2, h, side, 1);
        prof_pt(p, f, hw2, 0, side, 1);
    } else if (!strcmp(a[0], "ring")) {          // ring:radius:height:thickness[:tile] — for lathes
        float thick = na > 3 ? (float)atof(a[3]) : 0.18f;
        const char *tl = na > 4 ? a[4] : "wall_stone";
        if (thick < 0.02f) thick = 0.18f;
        prof_pt(p, f, w - thick, 0, tl, 1); prof_pt(p, f, w - thick, h, tl, 1);
        prof_pt(p, f, w, h, tl, 1);         prof_pt(p, f, w, 0, tl, 1);
    } else if (!strcmp(a[0], "stair")) {
        float rise = w, run = h;                                  // stair:rise:run:n[:tile]
        int steps = na > 3 ? atoi(a[3]) : 4;
        const char *tl = na > 4 ? a[4] : "wall_stone";
        if (steps < 1) steps = 1;
        if (steps > (F_PROF_PTS - 4) / 2) steps = (F_PROF_PTS - 4) / 2;
        prof_pt(p, f, 0, 0, tl, 1);
        for (int i = 0; i < steps; i++) {
            prof_pt(p, f, i * run, (i + 1) * rise, tl, 1);
            prof_pt(p, f, (i + 1) * run, (i + 1) * rise, tl, 1);
        }
        prof_pt(p, f, steps * run, 0, tl, 1);
    } else return -1;
    return f->prof_count++;
}

static void prof_pt(FProfile *p, Field *f, float x, float y, const char *tile, float sc) {
    if (p->n >= F_PROF_PTS) return;
    p->x[p->n] = x; p->y[p->n] = y; p->scale[p->n] = sc > 0.01f ? sc : 1.0f;
    p->art[p->n] = art_get(f, "tiles", tile);
    p->n++;
}

// ───────────────────────── map parsing ─────────────────────────

static int art_get(Field *f, const char *kind, const char *id) {
    char key[48];
    snprintf(key, sizeof(key), "%c:%s", kind[0], id);
    for (int i = 0; i < f->art_count; i++) if (!strcmp(f->art[i].key, key)) return i;
    if (f->art_count >= F_ART) return -1;
    snprintf(f->art[f->art_count].key, sizeof(f->art[0].key), "%s", key);
    f->art[f->art_count].tex = art_load(kind, id);
    return f->art_count++;
}

static int tile_get(Field *f, const char *id) {
    for (int i = 0; i < f->tile_count; i++) if (!strcmp(f->tiles[i].id, id)) return i;
    if (f->tile_count >= F_TILES) return 0;
    FTile *t = &f->tiles[f->tile_count];
    snprintf(t->id, sizeof(t->id), "%s", id);
    t->water = !strcmp(id, "water");
    t->tex = art_load("tiles", id);
    return f->tile_count++;
}

static int hchar(int c) {                                    // 0-9 a-z = height in half-steps
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'z') return 10 + c - 'a';
    return 0;
}
static int facing_char(int c) { return c == 'W' ? 1 : c == 'E' ? 2 : c == 'N' ? 3 : 0; }

static int split_toks(char *s, char **t, int max) {
    int n = 0;
    while (*s && n < max) {
        while (*s == ' ' || *s == '\t') *s++ = 0;
        if (!*s) break;
        t[n++] = s;
        while (*s && *s != ' ' && *s != '\t') s++;
    }
    return n;
}

static void field_free_map(Field *f) {
    if (f->splat_tex.id) { glDeleteTextures(1, &f->splat_tex.id); f->splat_tex.id = 0; }
    for (int i = 0; i < f->tile_count; i++) if (f->tiles[i].tex.id) glDeleteTextures(1, &f->tiles[i].tex.id);
    for (int i = 0; i < f->art_count; i++) if (f->art[i].tex.id) glDeleteTextures(1, &f->art[i].tex.id);
    f->tile_count = 0; f->art_count = 0; f->wall_tile = 0;
    f->prop_count = f->npc_count = f->exit_count = f->trig_count = 0;
    f->poly_count = 0; f->zone_count = 0; f->bld_count = 0; f->geo_count = 0;
    f->prof_count = 0; f->sweep_count = 0; f->lathe_count = 0;
    snprintf(f->edge_kind, sizeof(f->edge_kind), "fence");
    f->landmark[0] = 0;
    f->splat_file[0] = 0;
    f->splat_n = 8;
    f->splat_dither = 0.0f;
    for (int i = 0; i < 4; i++) {
        f->layer_cells[i] = 1.0f; f->layer_tile[i] = 0;
        f->layer_sharp[i] = 1.0f; f->layer_hk[i] = 0.0f;
    }
    if (f->splat_tex.id) glDeleteTextures(1, &f->splat_tex.id);
    f->splat_tex.id = 0;
    memset(f->hgt, 0, sizeof(f->hgt));
    memset(f->gnd, 0, sizeof(f->gnd));
}

static void build_world(Field *f);

static void zone_defaults(CamZone *z) {
    memset(z, 0, sizeof(*z));
    z->mode = CAM_FOLLOW;
    z->yaw = 0; z->pitch = 50; z->fov = 30; z->dist = 13; z->height = 0.8f;
}

// `id: x y tile scale, ... [closed]` — used by the map's `## profiles` and by the shared
// story/field/profiles.md, which is loaded first so maps can reuse cross-sections.
static void parse_profile_line(Field *f, char *line) {
    char *c = strchr(line, ':');
    if (!c || f->prof_count >= F_PROFS) return;
    *c = 0;
    FProfile *pf = &f->profs[f->prof_count];
    memset(pf, 0, sizeof(*pf));
    snprintf(pf->id, sizeof(pf->id), "%s", line);
    if (strstr(c + 1, "closed")) pf->closed = 1;
    char *v = c + 1;
    while (*v && pf->n < F_PROF_PTS) {
        float x = 0, y = 0, sc = 1;
        char tl[24] = "wall_stone";
        if (sscanf(v, " %f %f %23s %f", &x, &y, tl, &sc) < 3) break;
        prof_pt(pf, f, x, y, tl, sc);
        char *cm = strchr(v, ',');
        if (!cm) break;
        v = cm + 1;
    }
    if (pf->n >= 2) f->prof_count++;
}

static void load_shared_profiles(Field *f) {
    size_t sz = 0;
    char *text = (char *)field_read("field/profiles.md", &sz);
    if (!text) return;
    char *p = text;
    while (p && *p) {
        char *nl = strchr(p, '\n');
        if (nl) *nl = 0;
        char *line = p;
        p = nl ? nl + 1 : nullptr;
        while (*line == ' ' || *line == '-') line++;
        if (line[0] && line[0] != '#' && strchr(line, ':')) parse_profile_line(f, line);
    }
    SDL_free(text);
}

static void parse_map(Field *f, char *text) {
    char *gchars = (char *)calloc(FM_W * FM_H, 1);
    if (!gchars) return;
    signed char legend[128];
    memset(legend, -1, sizeof(legend));
    char wall_id[24] = "cliff", ground_id[24] = "grass";
    char layer_id[4][24] = {"", "dirt", "stone", "plank"};
    int section = -1, row_h = 0, row_g = 0;
    enum { S_META, S_HEIGHT, S_GROUND, S_TILES, S_PROPS, S_NPCS, S_EXITS, S_TRIGGERS, S_NAV, S_CAMERAS, S_BLDS, S_WALLS, S_PROFS, S_SWEEPS, S_LATHES };
    f->mw = 16; f->mh = 16; f->spawn_x = 8; f->spawn_y = 8; f->spawn_f = 0;

    load_shared_profiles(f);
    char *p = text;
    while (p && *p) {
        char *nl = strchr(p, '\n');
        if (nl) *nl = 0;
        char *line = p;
        p = nl ? nl + 1 : nullptr;
        if (line[0] == '#' && line[1] == '#') {
            char *s = line + 2;
            while (*s == ' ') s++;
            section = !strncmp(s, "meta", 4) ? S_META : !strncmp(s, "height", 6) ? S_HEIGHT :
                      !strncmp(s, "ground", 6) ? S_GROUND : !strncmp(s, "tiles", 5) ? S_TILES :
                      !strncmp(s, "props", 5) ? S_PROPS : !strncmp(s, "npcs", 4) ? S_NPCS :
                      !strncmp(s, "exits", 5) ? S_EXITS : !strncmp(s, "triggers", 8) ? S_TRIGGERS :
                      !strncmp(s, "nav", 3) ? S_NAV : !strncmp(s, "cameras", 7) ? S_CAMERAS :
                      !strncmp(s, "buildings", 9) ? S_BLDS :
                      !strncmp(s, "walls", 5) ? S_WALLS :
                      !strncmp(s, "profiles", 8) ? S_PROFS : !strncmp(s, "sweeps", 6) ? S_SWEEPS :
                      !strncmp(s, "lathes", 6) ? S_LATHES : -1;
            continue;
        }
        if (line[0] == '#') continue;                         // a whole-line comment, in any section
        if (section == S_HEIGHT || section == S_GROUND) {
            if (!line[0] || line[0] == '\r') continue;
            int y = (section == S_HEIGHT) ? row_h++ : row_g++;
            if (y >= FM_H) continue;
            for (int x = 0; x < f->mw && x < FM_W && line[x] && line[x] != '\r'; x++) {
                if (section == S_HEIGHT) f->hgt[y][x] = (unsigned char)hchar(line[x]);
                else gchars[y * FM_W + x] = line[x];
            }
            continue;
        }
        char *hash = strchr(line, '#');
        if (hash && hash != line) *hash = 0;                  // trailing comment (never on a grid row)
        char *tok[16];
        int n;
        switch (section) {
        case S_META: {
            char *c = strchr(line, ':');
            if (!c) break;
            *c = 0;
            char *v = c + 1;
            while (*v == ' ') v++;
            if (!strcmp(line, "size")) sscanf(v, "%d %d", &f->mw, &f->mh);
            else if (!strcmp(line, "spawn")) {
                int sx = 0, sy = 0; char fc = 'S';
                sscanf(v, "%d %d %c", &sx, &sy, &fc);
                f->spawn_x = (short)sx; f->spawn_y = (short)sy; f->spawn_f = facing_char(fc);
            } else if (!strcmp(line, "ground")) snprintf(ground_id, sizeof(ground_id), "%s", v);
            else if (!strcmp(line, "wall")) snprintf(wall_id, sizeof(wall_id), "%s", v);
            else if (!strcmp(line, "edges")) snprintf(f->edge_kind, sizeof(f->edge_kind), "%s", v);
            else if (!strcmp(line, "landmark")) snprintf(f->landmark, sizeof(f->landmark), "%s", v);
            else if (!strcmp(line, "splat")) { int nn = 8; sscanf(v, "%63s %d", f->splat_file, &nn); f->splat_n = nn > 0 ? nn : 8; }
            else if (!strcmp(line, "splat_layers")) {
                // `grass:1 dirt:1 stone:8:0.6 plank:8:0.4` — name[:sharpness[:height influence]]
                char raw[4][40] = {"", "", "", ""};
                sscanf(v, "%39s %39s %39s %39s", raw[0], raw[1], raw[2], raw[3]);
                for (int i = 0; i < 4; i++) {
                    if (!raw[i][0]) continue;
                    char *c1 = strchr(raw[i], ':');
                    if (c1) {
                        *c1 = 0;
                        char *c2 = strchr(c1 + 1, ':');
                        if (c2) { *c2 = 0; f->layer_hk[i] = (float)atof(c2 + 1); }
                        f->layer_sharp[i] = (float)atof(c1 + 1);
                        if (f->layer_sharp[i] < 1.0f) f->layer_sharp[i] = 1.0f;
                    }
                    snprintf(layer_id[i], sizeof(layer_id[i]), "%s", raw[i]);
                }
            }
            else if (!strcmp(line, "tile_scale")) sscanf(v, "%f %f %f %f", &f->layer_cells[0], &f->layer_cells[1], &f->layer_cells[2], &f->layer_cells[3]);
            else if (!strcmp(line, "splat_blend")) {
                char mode[16] = "smooth", extra[16] = "";
                float sh = 1.0f;
                sscanf(v, "%15s %f %15s", mode, &sh, extra);
                if (!strcmp(mode, "hard"))                       // legacy: a default for layers with no :n
                    for (int i = 0; i < 4; i++) if (f->layer_sharp[i] <= 1.001f) f->layer_sharp[i] = sh > 0.2f ? sh : 6.0f;
                f->splat_dither = strstr(v, "dither") ? 0.18f : 0.0f;
            }
        } break;
        case S_TILES: {
            n = split_toks(line, tok, 4);
            if (n >= 2 && (unsigned char)tok[0][0] < 128) legend[(int)(unsigned char)tok[0][0]] = (signed char)tile_get(f, tok[1]);
        } break;
        case S_NAV: {
            char *c = strchr(line, ':');
            if (!c || f->poly_count >= F_POLYS) break;
            *c = 0;
            NavPoly *np = &f->polys[f->poly_count];
            memset(np, 0, sizeof(*np));
            np->id = atoi(line);
            char *bar = strchr(c + 1, '|');          // `id: x z h, ... | fence` — what its open edges wear
            if (bar) {
                *bar = 0;
                char *k = bar + 1;
                while (*k == ' ') k++;
                snprintf(np->bkind, sizeof(np->bkind), "%s", k);
                for (char *q = np->bkind; *q; q++) if (*q == ' ' || *q == '\r') { *q = 0; break; }
            }
            char *v = c + 1;
            while (*v && np->n < F_POLY_V) {
                float x = 0, z = 0, hh = 0;
                if (sscanf(v, " %f %f %f", &x, &z, &hh) < 2) break;
                np->vx[np->n] = x; np->vz[np->n] = z; np->vy[np->n] = hh * HALF_STEP;
                np->n++;
                char *cm = strchr(v, ',');
                if (!cm) break;
                v = cm + 1;
            }
            if (np->n >= 3) { poly_finish(np); f->poly_count++; }
            else SDL_Log("field: nav poly %d has %d vertices, skipped", np->id, np->n);
        } break;
        case S_CAMERAS: {
            n = split_toks(line, tok, 16);
            if (n < 5 || f->zone_count >= F_ZONES) break;
            CamZone *z = &f->zones[f->zone_count];
            zone_defaults(z);
            z->x = (float)atof(tok[0]); z->z = (float)atof(tok[1]);
            z->w = (float)atof(tok[2]); z->d = (float)atof(tok[3]);
            if (!strcmp(tok[4], "fixed") && n >= 12) {
                z->mode = CAM_FIXED;
                z->cx = (float)atof(tok[5]); z->cy = (float)atof(tok[6]); z->cz = (float)atof(tok[7]);
                z->tx = (float)atof(tok[8]); z->ty = (float)atof(tok[9]); z->tz = (float)atof(tok[10]);
                z->fov = (float)atof(tok[11]);
                z->pan = (n >= 13 && !strcmp(tok[12], "pan")) ? 1 : 0;
            } else if (!strcmp(tok[4], "rail") && n >= 15) {
                z->mode = CAM_RAIL;
                z->ax = (float)atof(tok[5]); z->ay = (float)atof(tok[6]); z->az = (float)atof(tok[7]);
                z->bx = (float)atof(tok[8]); z->by = (float)atof(tok[9]); z->bz = (float)atof(tok[10]);
                z->tx = (float)atof(tok[11]); z->ty = (float)atof(tok[12]); z->tz = (float)atof(tok[13]);
                z->fov = (float)atof(tok[14]);
            } else if (!strcmp(tok[4], "fixed") || !strcmp(tok[4], "rail")) {
                SDL_Log("field: camera zone %d: %s needs %s numbers, got %d tokens - ignored",
                        f->zone_count, tok[4], strcmp(tok[4], "rail") ? "7" : "10", n);
                break;
            } else if (n >= 10) {
                z->mode = CAM_FOLLOW;
                z->yaw = (float)atof(tok[5]); z->pitch = (float)atof(tok[6]); z->fov = (float)atof(tok[7]);
                z->dist = (float)atof(tok[8]); z->height = (float)atof(tok[9]);
            } else break;
            f->zone_count++;
        } break;
        case S_PROFS: parse_profile_line(f, line); break;
        case S_SWEEPS: {
            // id profile [spline] [caps] [along S] [scale s0 s1]  x z [h], x z [h], ...
            char save[440];
            snprintf(save, sizeof(save), "%s", line);
            n = split_toks(line, tok, 12);
            if (n < 3 || f->sweep_count >= F_SWEEPS) break;
            FSweep *sw = &f->sweeps[f->sweep_count];
            memset(sw, 0, sizeof(*sw));
            sw->along = 1.0f; sw->s0 = 1.0f; sw->s1 = 1.0f;
            sw->prof = profile_get(f, tok[1]);
            int first = 2;
            while (first < n) {
                if (!strcmp(tok[first], "spline")) { sw->spline = 1; first++; }
                else if (!strcmp(tok[first], "caps")) { sw->caps = 1; first++; }
                else if (!strcmp(tok[first], "along") && first + 1 < n) { sw->along = (float)atof(tok[first + 1]); first += 2; }
                else if (!strcmp(tok[first], "scale") && first + 2 < n) {
                    sw->s0 = (float)atof(tok[first + 1]); sw->s1 = (float)atof(tok[first + 2]); first += 3;
                } else break;
            }
            int seen = 0, skip = -1;
            for (int i = 0; save[i]; i++) {
                if (save[i] != ' ' && (i == 0 || save[i - 1] == ' ')) seen++;
                if (seen == first + 1) { skip = i; break; }
            }
            if (skip < 0 || sw->prof < 0) break;
            char *v = save + skip;
            while (*v && sw->n < F_WALL_PTS) {
                float x = 0, z = 0, hh = 0;
                int got = sscanf(v, " %f %f %f", &x, &z, &hh);
                if (got < 2) break;
                sw->px[sw->n] = x; sw->pz[sw->n] = z;
                sw->py[sw->n] = hh; sw->fixed_y[sw->n] = (got >= 3) ? 1 : 0;
                sw->n++;
                char *cm = strchr(v, ',');
                if (!cm) break;
                v = cm + 1;
            }
            if (sw->n >= 2) f->sweep_count++;
        } break;
        case S_LATHES: {
            // id profile x z [h] [segments]
            n = split_toks(line, tok, 7);
            if (n < 4 || f->lathe_count >= F_LATHES) break;
            FLathe *la = &f->lathes[f->lathe_count];
            memset(la, 0, sizeof(*la));
            la->prof = profile_get(f, tok[1]);
            la->x = (float)atof(tok[2]); la->z = (float)atof(tok[3]);
            if (n > 4) { la->y = (float)atof(tok[4]); la->fixed_y = 1; }
            la->segs = n > 5 ? atoi(tok[5]) : 16;
            if (la->segs < 4) la->segs = 4;
            if (la->segs > 48) la->segs = 48;
            if (la->prof >= 0) f->lathe_count++;
        } break;
        case S_WALLS: {
            // Sugar for a box sweep: id w h tex_side tex_top [cap] [spline] x z, ...
            char save[400];
            snprintf(save, sizeof(save), "%s", line);
            n = split_toks(line, tok, 9);
            if (n < 6 || f->sweep_count >= F_SWEEPS) break;
            char spec[80];
            snprintf(spec, sizeof(spec), "box:%s:%s:%s:%s", tok[1], tok[2], tok[3], tok[4]);
            FSweep *sw = &f->sweeps[f->sweep_count];
            memset(sw, 0, sizeof(*sw));
            sw->along = 1.0f; sw->s0 = 1.0f; sw->s1 = 1.0f;
            sw->prof = profile_get(f, spec);
            int first = 5;
            while (first < n && (!strcmp(tok[first], "cap") || !strcmp(tok[first], "spline"))) {
                if (!strcmp(tok[first], "cap")) sw->caps = 1; else sw->spline = 1;
                first++;
            }
            int seen = 0, skip = -1;
            for (int i = 0; save[i]; i++) {
                if (save[i] != ' ' && (i == 0 || save[i - 1] == ' ')) seen++;
                if (seen == first + 1) { skip = i; break; }
            }
            if (skip < 0 || sw->prof < 0) break;
            char *v = save + skip;
            while (*v && sw->n < F_WALL_PTS) {
                float x = 0, z = 0;
                if (sscanf(v, " %f %f", &x, &z) < 2) break;
                sw->px[sw->n] = x; sw->pz[sw->n] = z; sw->n++;
                char *cm = strchr(v, ',');
                if (!cm) break;
                v = cm + 1;
            }
            if (sw->n >= 2) f->sweep_count++;
        } break;
        case S_BLDS: {
            // x z w d h id — footprint in map units, height in world units, gabled roof.
            n = split_toks(line, tok, 8);
            if (n < 6 || f->bld_count >= (int)(sizeof(f->blds) / sizeof(f->blds[0]))) break;
            Bld *b = &f->blds[f->bld_count];
            memset(b, 0, sizeof(*b));
            b->x = (float)atof(tok[0]); b->z = (float)atof(tok[1]);
            b->w = (float)atof(tok[2]); b->d = (float)atof(tok[3]); b->h = (float)atof(tok[4]);
            if (b->w < 0.5f) b->w = 1; if (b->d < 0.5f) b->d = 1;
            if (b->h < 0.5f) b->h = 2.0f;
            snprintf(b->id, sizeof(b->id), "%s", tok[5]);
            b->pitch = 0.55f; b->eave = 0.18f;
            for (int i = 6; i + 1 < n; i++) {
                if (!strcmp(tok[i], "rot")) b->rot = (float)atof(tok[i + 1]);
                else if (!strcmp(tok[i], "pitch")) b->pitch = (float)atof(tok[i + 1]);
                else if (!strcmp(tok[i], "eave")) b->eave = (float)atof(tok[i + 1]);
            }
            char face[48];
            snprintf(face, sizeof(face), "%s_front", b->id); b->a_front = art_get(f, "buildings", face);
            snprintf(face, sizeof(face), "%s_side", b->id);  b->a_side = art_get(f, "buildings", face);
            snprintf(face, sizeof(face), "%s_roof", b->id);  b->a_roof = art_get(f, "buildings", face);
            f->bld_count++;
            // The box-with-a-gable is gone: a building is a `house` cross-section swept along its
            // length, so its end caps are its gable ends and the front cap carries the painted face.
            if (f->sweep_count < F_SWEEPS) {
                char spec[160], sd[48], rf[48];
                snprintf(sd, sizeof(sd), "%s_side", b->id);
                snprintf(rf, sizeof(rf), "%s_roof", b->id);
                snprintf(spec, sizeof(spec), "house:%.3f:%.3f:%.3f:%.3f:%s:%s", b->w, b->h, b->pitch, b->eave, sd, rf);
                FSweep *sw = &f->sweeps[f->sweep_count];
                memset(sw, 0, sizeof(*sw));
                sw->along = 1.0f; sw->s0 = 1.0f; sw->s1 = 1.0f; sw->caps = 1;
                sw->prof = profile_get(f, spec);
                sw->cap_art0 = b->a_front;
                sw->cap_art1 = b->a_side;
                float cx = b->x + b->w * 0.5f, cz = b->z + b->d * 0.5f;
                float c = cosf(b->rot * DEG), sn = sinf(b->rot * DEG), hd = b->d * 0.5f;
                sw->px[0] = cx - sn * hd; sw->pz[0] = cz + c * hd;      // front, the face with the door
                sw->px[1] = cx + sn * hd; sw->pz[1] = cz - c * hd;      // back
                sw->py[0] = sw->py[1] = grid_h(f, cx, cz);              // one base: a house is level
                sw->fixed_y[0] = sw->fixed_y[1] = 1;
                sw->n = 2;
                if (sw->prof >= 0) f->sweep_count++;
            }
        } break;
        case S_PROPS: {
            n = split_toks(line, tok, 8);
            if (n < 3 || f->prop_count >= F_PROPS) break;
            FProp *pr = &f->props[f->prop_count];
            memset(pr, 0, sizeof(*pr));
            pr->x = (short)atoi(tok[0]); pr->y = (short)atoi(tok[1]); pr->w = 1; pr->d = 1;
            snprintf(pr->id, sizeof(pr->id), "%s", tok[2]);
            for (int i = 3; i < n; i++)                        // `solid` / `walk` are accepted and ignored:
                if (!strcmp(tok[i], "wide") && i + 1 < n) {    // blocking is the navmesh's job now
                    int ww = 1, dd = 1;
                    sscanf(tok[++i], "%dx%d", &ww, &dd);
                    pr->w = (short)ww; pr->d = (short)dd;
                }
            pr->art = art_get(f, "props", pr->id);
            f->prop_count++;
        } break;
        case S_NPCS: {
            // x y walker facing Name [scene-id | say:text...] — `say:` is an additive extension so a
            // villager can have a line before any talk scene exists for them (see FIELD_NOTES.md).
            char *say = strstr(line, " say:");
            char saybuf[160] = {0};
            if (say) { snprintf(saybuf, sizeof(saybuf), "%s", say + 5); *say = 0; }
            n = split_toks(line, tok, 8);
            if (n < 3 || f->npc_count >= F_NPCS) break;
            FNpc *np = &f->npcs[f->npc_count];
            memset(np, 0, sizeof(*np));
            np->x = (short)atoi(tok[0]); np->y = (short)atoi(tok[1]);
            snprintf(np->walker, sizeof(np->walker), "%s", tok[2]);
            np->facing = n > 3 ? facing_char(tok[3][0]) : 0;
            if (n > 4) snprintf(np->name, sizeof(np->name), "%s", tok[4]);
            for (char *q = np->name; *q; q++) if (*q == '_') *q = ' ';
            if (n > 5) snprintf(np->scene, sizeof(np->scene), "%s", tok[5]);
            snprintf(np->say, sizeof(np->say), "%s", saybuf);
            np->art = art_get(f, "walkers", np->walker);
            f->npc_count++;
        } break;
        case S_EXITS: {
            n = split_toks(line, tok, 9);
            if (n < 8 || f->exit_count >= F_EXITS) break;
            FExit *e = &f->exits[f->exit_count++];
            e->x = (short)atoi(tok[0]); e->y = (short)atoi(tok[1]); e->w = (short)atoi(tok[2]); e->h = (short)atoi(tok[3]);
            snprintf(e->map, sizeof(e->map), "%s", tok[4]);
            e->sx = (short)atoi(tok[5]); e->sy = (short)atoi(tok[6]);
            e->facing = facing_char(tok[7][0]);
        } break;
        case S_TRIGGERS: {
            char save[176];
            snprintf(save, sizeof(save), "%s", line);
            n = split_toks(line, tok, 6);
            if (n < 5 || f->trig_count >= F_TRIGS) break;
            FTrig *t = &f->trigs[f->trig_count++];
            memset(t, 0, sizeof(*t));
            t->x = (short)atoi(tok[0]); t->y = (short)atoi(tok[1]); t->w = (short)atoi(tok[2]); t->h = (short)atoi(tok[3]);
            t->kind = !strcmp(tok[4], "scene") ? TG_SCENE : !strcmp(tok[4], "zone") ? TG_ZONE :
                      !strcmp(tok[4], "trap") ? TG_TRAP : TG_MESSAGE;
            if (n >= 6) {                                     // the arg is the rest of the original line,
                int seen = 0, skip = -1;                      // so message text keeps its spaces
                for (int i = 0; save[i]; i++) {
                    if (save[i] != ' ' && (i == 0 || save[i - 1] == ' ')) seen++;
                    if (seen == 6) { skip = i; break; }
                }
                if (skip >= 0) snprintf(t->arg, sizeof(t->arg), "%s", save + skip);
            }
        } break;
        default: break;
        }
    }

    if (f->mw < 1) f->mw = 1;
    if (f->mw > FM_W) f->mw = FM_W;
    if (f->mh < 1) f->mh = 1;
    if (f->mh > FM_H) f->mh = FM_H;
    int def = tile_get(f, ground_id);
    f->wall_tile = tile_get(f, wall_id);
    if (!layer_id[0][0]) snprintf(layer_id[0], sizeof(layer_id[0]), "%s", ground_id);
    for (int i = 0; i < 4; i++) {
        f->layer_tile[i] = tile_get(f, layer_id[i]);
        if (f->layer_cells[i] < 0.05f) f->layer_cells[i] = 1.0f;
    }
    for (int y = 0; y < f->mh; y++) for (int x = 0; x < f->mw; x++) {
        char c = gchars[y * FM_W + x];
        signed char t = (c && (unsigned char)c < 128) ? legend[(int)(unsigned char)c] : -1;
        f->gnd[y][x] = (unsigned char)((c == 0 || c == '.' || t < 0) ? def : t);
    }
    free(gchars);
    // The ground splat: the PNG if the map has one, otherwise rasterised from the legacy `## ground`
    // grid with a one-cell soft falloff, so an old map still beats hard per-cell tiling.
    {
        char rel[160];
        size_t sz = 0;
        void *data = nullptr;
        if (f->splat_file[0]) {
            snprintf(rel, sizeof(rel), "field/maps/%s", f->splat_file);
            data = field_read(rel, &sz);
            if (!data) SDL_Log("field: splat %s not found, rasterising %s instead", f->splat_file, "## ground");
        }
        if (data) {
            int w = 0, h = 0, nc = 0;
            unsigned char *px = stbi_load_from_memory((const unsigned char *)data, (int)sz, &w, &h, &nc, 4);
            SDL_free(data);
            if (px) {
                f->splat_tex = ftex_upload(px, w, h, false, false);
                glBindTexture(GL_TEXTURE_2D, f->splat_tex.id);      // bilinear: that is the whole point
                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
                stbi_image_free(px);
            }
        }
        if (!f->splat_tex.id) {
            int n = f->splat_n, W = f->mw * n, H = f->mh * n;
            unsigned char *px = (unsigned char *)calloc((size_t)W * H * 4, 1);
            if (px) {
                for (int ty = 0; ty < H; ty++) for (int tx = 0; tx < W; tx++) {
                    float wx = (tx + 0.5f) / n, wz = (ty + 0.5f) / n, acc[4] = {0, 0, 0, 0}, tot = 0;
                    for (int dz = -1; dz <= 1; dz++) for (int dx = -1; dx <= 1; dx++) {
                        int cx = (int)wx + dx, cz = (int)wz + dz;
                        if (cx < 0 || cz < 0 || cx >= f->mw || cz >= f->mh) continue;
                        float d = sqrtf((cx + 0.5f - wx) * (cx + 0.5f - wx) + (cz + 0.5f - wz) * (cz + 0.5f - wz));
                        float k = 1.0f - d / 1.45f;
                        if (k <= 0) continue;
                        int l = 0;
                        for (int i = 0; i < 4; i++) if (f->gnd[cz][cx] == f->layer_tile[i]) { l = i; break; }
                        acc[l] += k; tot += k;
                    }
                    unsigned char *o = px + ((size_t)ty * W + tx) * 4;
                    for (int i = 0; i < 4; i++) o[i] = (unsigned char)(tot > 0 ? acc[i] / tot * 255.0f : (i == 0 ? 255 : 0));
                }
                f->splat_tex = ftex_upload(px, W, H, false, false);
                glBindTexture(GL_TEXTURE_2D, f->splat_tex.id);
                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
                free(px);
            }
        }
        glBindTexture(GL_TEXTURE_2D, 0);
    }
    mesh_link(f);
    if (!f->poly_count) SDL_Log("field: %s has no ## nav section; nothing is walkable", f->map_name);
    if (!f->zone_count) {                                     // a default zone covering the map is required
        SDL_Log("field: %s has no ## cameras section; using the default follow shot", f->map_name);
        zone_defaults(&f->zones[0]);
        f->zones[0].w = (float)f->mw; f->zones[0].d = (float)f->mh;
        f->zone_count = 1;
    }
    if (f->spawn_x < 0 || f->spawn_x >= f->mw) f->spawn_x = (short)(f->mw / 2);
    if (f->spawn_y < 0 || f->spawn_y >= f->mh) f->spawn_y = (short)(f->mh / 2);
}

// Exits and triggers fire on the rising edge only, so landing on top of one (the usual case for a
// door you just walked through) never bounces the player straight back.
static void field_place(Field *f, float x, float z, int facing) {
    f->px = x; f->pz = z; f->facing = facing;
    f->poly = mesh_find(f, x, z);
    if (f->poly < 0) f->poly = mesh_nearest(f, x, z);
    f->poly = wall_buffer(f, f->poly, &f->px, &f->pz, f->walk_radius);   // never spawn inside a fence
    x = f->px; z = f->pz;
    f->py = (f->poly >= 0) ? poly_height(&f->polys[f->poly], x, z) : 0.0f;
    f->foc[0] = x; f->foc[1] = f->py; f->foc[2] = z;
    f->move_dx = f->move_dz = 0.0f;
    f->frame_u = 0.0f;
    for (int i = 0; i < f->exit_count; i++) {
        FExit *e = &f->exits[i];
        e->inside = x >= e->x && z >= e->y && x < e->x + e->w && z < e->y + e->h;
    }
    for (int i = 0; i < f->trig_count; i++) {
        FTrig *t = &f->trigs[i];
        t->inside = x >= t->x && z >= t->y && x < t->x + t->w && z < t->y + t->h;
    }
}

bool field_load_map(Field *f, const char *name) {
    char rel[128];
    snprintf(rel, sizeof(rel), "field/maps/%s.map", name);
    size_t size = 0;
    char *text = (char *)field_read(rel, &size);
    bool from_file = text != nullptr;
    if (!text) { SDL_Log("field: %s not found, using the fallback map", rel); text = SDL_strdup(FALLBACK_MAP); }
    field_free_map(f);
    snprintf(f->map_name, sizeof(f->map_name), "%s", name);
    parse_map(f, text);
    SDL_free(text);
    field_place(f, f->spawn_x + 0.5f, f->spawn_y + 0.5f, f->spawn_f);
    f->msg[0] = 0; f->msg_who[0] = 0; f->msg_t = 0;
    f->zone_idx = -1; f->blend_t = 0; f->fov = 0.0f;       // fov 0 = snap the new map's shot, don't blend
    if (f->gl_ready) build_world(f);
    return from_file;
}

// ───────────────────────── geometry ─────────────────────────

static void pushv(FVert **v, int *n, int cap, float x, float y, float z, float u, float vv, float s) {
    if (*n >= cap) return;
    FVert *o = *v + (*n)++;
    o->x = x; o->y = y; o->z = z; o->u = u; o->v = vv;
    int c = (int)(s * 255.0f);
    if (c > 255) c = 255;
    if (c < 0) c = 0;
    o->r = o->g = o->b = (unsigned char)c; o->a = 255;
}
static void quad(FVert **v, int *n, int cap, const float *a, const float *b, const float *c, const float *d,
                 float u0, float v0, float u1, float v1, float s0, float s1) {
    pushv(v, n, cap, a[0], a[1], a[2], u0, v0, s0);
    pushv(v, n, cap, b[0], b[1], b[2], u1, v0, s0);
    pushv(v, n, cap, c[0], c[1], c[2], u1, v1, s1);
    pushv(v, n, cap, a[0], a[1], a[2], u0, v0, s0);
    pushv(v, n, cap, c[0], c[1], c[2], u1, v1, s1);
    pushv(v, n, cap, d[0], d[1], d[2], u0, v1, s1);
}

static float face_shade(float nx, float nz) {                 // sun from (+X, -Z)
    float d = (nx * 0.7071f - nz * 0.7071f);
    return 0.60f + 0.50f * (0.5f + 0.5f * d);
}

static void tri3(FVert **v, int *n, int cap, const float *a, const float *b, const float *c,
                 float ua, float va, float ub, float vb, float uc, float vc, float sh) {
    pushv(v, n, cap, a[0], a[1], a[2], ua, va, sh);
    pushv(v, n, cap, b[0], b[1], b[2], ub, vb, sh);
    pushv(v, n, cap, c[0], c[1], c[2], uc, vc, sh);
}

static float grid_h(Field *f, float x, float z) {
    int cx = (int)floorf(x), cz = (int)floorf(z);
    if (cx < 0) cx = 0; if (cx >= f->mw) cx = f->mw - 1;
    if (cz < 0) cz = 0; if (cz >= f->mh) cz = f->mh - 1;
    return f->hgt[cz][cx] * HALF_STEP;
}

static bool in_building(Field *f, float x, float z, float pad) {
    for (int i = 0; i < f->bld_count; i++) {
        Bld *b = &f->blds[i];
        float cx = b->x + b->w * 0.5f, cz = b->z + b->d * 0.5f;
        float c = cosf(-b->rot * DEG), sn = sinf(-b->rot * DEG);
        float lx = (x - cx) * c - (z - cz) * sn, lz = (x - cx) * sn + (z - cz) * c;
        if (fabsf(lx) <= b->w * 0.5f + pad && fabsf(lz) <= b->d * 0.5f + pad) return true;
    }
    return false;
}

// An exit is a way out, not a wall: no blocker is drawn on a navmesh edge that lies in or near one.
static bool near_exit(Field *f, float x, float z, float pad) {
    for (int i = 0; i < f->exit_count; i++) {
        FExit *e = &f->exits[i];
        if (x >= e->x - pad && x <= e->x + e->w + pad && z >= e->y - pad && z <= e->y + e->h + pad) return true;
    }
    return false;
}

// Sweep a profile along a path. Corners are mitred (the profile's x is stretched by 1/cos of the
// half-angle, clamped), the base follows the terrain unless the map gave an explicit height, and each
// face is lit from its own normal. This is what walls, kerbs, bridges and stairs are all made of.
static void emit_sweep(Field *f, FSweep *sw, FVert **w, int *n, int cap) {
    if (sw->prof < 0 || sw->prof >= f->prof_count) return;
    FProfile *pf = &f->profs[sw->prof];
    const int MAXP = F_WALL_PTS * 4;
    float xs[MAXP], zs[MAXP], ys[MAXP], len[MAXP], nx[MAXP], nz[MAXP], mit[MAXP];
    int np = 0;
    if (sw->spline && sw->n >= 3) {
        for (int i = 0; i + 1 < sw->n && np < MAXP - 2; i++) {
            int i0 = i > 0 ? i - 1 : 0, i2 = i + 1, i3 = (i + 2 < sw->n) ? i + 2 : sw->n - 1;
            float seg = sqrtf((sw->px[i2] - sw->px[i]) * (sw->px[i2] - sw->px[i]) +
                              (sw->pz[i2] - sw->pz[i]) * (sw->pz[i2] - sw->pz[i]));
            int steps = (int)(seg / 0.5f) + 1;
            for (int k = 0; k < steps && np < MAXP - 2; k++) {
                float t = (float)k / steps, t2 = t * t, t3 = t2 * t;
                #define CR(A) (0.5f * ((2 * A[i]) + (-A[i0] + A[i2]) * t + \
                        (2 * A[i0] - 5 * A[i] + 4 * A[i2] - A[i3]) * t2 + \
                        (-A[i0] + 3 * A[i] - 3 * A[i2] + A[i3]) * t3))
                xs[np] = CR(sw->px); zs[np] = CR(sw->pz);
                ys[np] = (sw->fixed_y[i] || sw->fixed_y[i2]) ? CR(sw->py) : grid_h(f, xs[np], zs[np]);
                #undef CR
                np++;
            }
        }
        xs[np] = sw->px[sw->n - 1]; zs[np] = sw->pz[sw->n - 1];
        ys[np] = sw->fixed_y[sw->n - 1] ? sw->py[sw->n - 1] : grid_h(f, xs[np], zs[np]);
        np++;
    } else {
        for (int i = 0; i < sw->n && i < MAXP; i++) {
            xs[i] = sw->px[i]; zs[i] = sw->pz[i];
            ys[i] = sw->fixed_y[i] ? sw->py[i] : grid_h(f, xs[i], zs[i]);
        }
        np = sw->n < MAXP ? sw->n : MAXP;
    }
    if (np < 2) return;

    float run = 0.0f;
    for (int i = 0; i < np; i++) {
        float dx, dz;
        if (i == 0) { dx = xs[1] - xs[0]; dz = zs[1] - zs[0]; }
        else if (i == np - 1) { dx = xs[np - 1] - xs[np - 2]; dz = zs[np - 1] - zs[np - 2]; }
        else {
            float ax = xs[i] - xs[i - 1], az = zs[i] - zs[i - 1], la = sqrtf(ax * ax + az * az) + 1e-6f;
            float bx = xs[i + 1] - xs[i], bz = zs[i + 1] - zs[i], lb = sqrtf(bx * bx + bz * bz) + 1e-6f;
            dx = ax / la + bx / lb; dz = az / la + bz / lb;
        }
        float l = sqrtf(dx * dx + dz * dz) + 1e-6f;
        dx /= l; dz /= l;
        nx[i] = -dz; nz[i] = dx;
        mit[i] = 1.0f;
        if (i > 0 && i < np - 1) {
            float ax = xs[i] - xs[i - 1], az = zs[i] - zs[i - 1], la = sqrtf(ax * ax + az * az) + 1e-6f;
            float c = (-az / la) * nx[i] + (ax / la) * nz[i];
            mit[i] = 1.0f / fmaxf(0.35f, fabsf(c));
        }
        if (i) run += sqrtf((xs[i] - xs[i - 1]) * (xs[i] - xs[i - 1]) + (zs[i] - zs[i - 1]) * (zs[i] - zs[i - 1]));
        len[i] = run;
    }

    int segs = pf->closed ? pf->n : pf->n - 1;
    for (int sgi = 0; sgi < segs; sgi++) {
        int j = (sgi + 1) % pf->n;
        if (f->geo_count >= (int)(sizeof(f->geo) / sizeof(f->geo[0]))) return;
        GeoRange *g = &f->geo[f->geo_count++];
        g->art = pf->art[sgi]; g->first = *n; g->alpha = false;
        float px0 = pf->x[sgi], py0 = pf->y[sgi], px1 = pf->x[j], py1 = pf->y[j];
        float plen = sqrtf((px1 - px0) * (px1 - px0) + (py1 - py0) * (py1 - py0)) * pf->scale[sgi];
        for (int i = 0; i + 1 < np; i++) {
            float sc0 = sw->s0 + (sw->s1 - sw->s0) * (np > 1 ? (float)i / (np - 1) : 0.0f);
            float sc1 = sw->s0 + (sw->s1 - sw->s0) * (np > 1 ? (float)(i + 1) / (np - 1) : 0.0f);
            float a[3] = {xs[i] + nx[i] * px0 * mit[i] * sc0, ys[i] + py0 * sc0, zs[i] + nz[i] * px0 * mit[i] * sc0};
            float b[3] = {xs[i + 1] + nx[i + 1] * px0 * mit[i + 1] * sc1, ys[i + 1] + py0 * sc1, zs[i + 1] + nz[i + 1] * px0 * mit[i + 1] * sc1};
            float c[3] = {xs[i + 1] + nx[i + 1] * px1 * mit[i + 1] * sc1, ys[i + 1] + py1 * sc1, zs[i + 1] + nz[i + 1] * px1 * mit[i + 1] * sc1};
            float d[3] = {xs[i] + nx[i] * px1 * mit[i] * sc0, ys[i] + py1 * sc0, zs[i] + nz[i] * px1 * mit[i] * sc0};
            float ex = b[0] - a[0], ey = b[1] - a[1], ez = b[2] - a[2];
            float fx2 = d[0] - a[0], fy2 = d[1] - a[1], fz2 = d[2] - a[2];
            float fnx = ey * fz2 - ez * fy2, fny = ez * fx2 - ex * fz2, fnz = ex * fy2 - ey * fx2;
            float fl = sqrtf(fnx * fnx + fny * fny + fnz * fnz) + 1e-6f;
            float sh = face_shade(fnx / fl, fnz / fl) * (0.82f + 0.24f * fabsf(fny / fl));
            quad(w, n, cap, a, b, c, d, len[i] * sw->along, 0.0f, len[i + 1] * sw->along, plen, sh, sh * 0.92f);
        }
        g->count = *n - g->first;
    }

    if (sw->caps && pf->closed && pf->n >= 3) {     // fan from the profile's centroid: profiles are small
        float cxp = 0, cyp = 0, x0 = 1e9f, x1 = -1e9f, y0 = 1e9f, y1 = -1e9f;
        for (int k = 0; k < pf->n; k++) {
            cxp += pf->x[k]; cyp += pf->y[k];
            if (pf->x[k] < x0) x0 = pf->x[k];
            if (pf->x[k] > x1) x1 = pf->x[k];
            if (pf->y[k] < y0) y0 = pf->y[k];
            if (pf->y[k] > y1) y1 = pf->y[k];
        }
        cxp /= pf->n; cyp /= pf->n;
        float sx2 = 1.0f / fmaxf(0.01f, x1 - x0), sy2 = 1.0f / fmaxf(0.01f, y1 - y0);
        for (int e = 0; e < 2; e++) {
            if (f->geo_count >= (int)(sizeof(f->geo) / sizeof(f->geo[0]))) return;
            GeoRange *g = &f->geo[f->geo_count++];
            int ca = e ? sw->cap_art1 : sw->cap_art0;
            g->art = ca >= 0 ? ca : pf->art[0];
            g->first = *n; g->alpha = false;
            int i = e ? np - 1 : 0;
            float sc = e ? sw->s1 : sw->s0;
            float ctr[3] = {xs[i] + nx[i] * cxp * sc, ys[i] + cyp * sc, zs[i] + nz[i] * cxp * sc};
            float cu = (cxp - x0) * sx2, cv = 1.0f - (cyp - y0) * sy2;
            for (int k = 0; k < pf->n; k++) {
                int k2 = (k + 1) % pf->n;
                float a[3] = {xs[i] + nx[i] * pf->x[k] * sc, ys[i] + pf->y[k] * sc, zs[i] + nz[i] * pf->x[k] * sc};
                float b[3] = {xs[i] + nx[i] * pf->x[k2] * sc, ys[i] + pf->y[k2] * sc, zs[i] + nz[i] * pf->x[k2] * sc};
                tri3(w, n, cap, ctr, a, b, cu, cv,
                     (pf->x[k] - x0) * sx2, 1.0f - (pf->y[k] - y0) * sy2,
                     (pf->x[k2] - x0) * sx2, 1.0f - (pf->y[k2] - y0) * sy2, e ? 0.92f : 0.80f);
            }
            g->count = *n - g->first;
        }
    }
}

// The same profile revolved about a vertical axis: x is the radius. Wells, columns, towers, domes.
static void emit_lathe(Field *f, FLathe *la, FVert **w, int *n, int cap) {
    if (la->prof < 0 || la->prof >= f->prof_count) return;
    FProfile *pf = &f->profs[la->prof];
    float base = la->fixed_y ? la->y : grid_h(f, la->x, la->z);
    int segs = pf->closed ? pf->n : pf->n - 1;
    for (int sgi = 0; sgi < segs; sgi++) {
        int j = (sgi + 1) % pf->n;
        if (f->geo_count >= (int)(sizeof(f->geo) / sizeof(f->geo[0]))) return;
        GeoRange *g = &f->geo[f->geo_count++];
        g->art = pf->art[sgi]; g->first = *n; g->alpha = false;
        float r0 = pf->x[sgi], y0 = pf->y[sgi], r1 = pf->x[j], y1 = pf->y[j];
        float plen = sqrtf((r1 - r0) * (r1 - r0) + (y1 - y0) * (y1 - y0)) * pf->scale[sgi];
        for (int k = 0; k < la->segs; k++) {
            float t0 = (float)k / la->segs * 6.2831853f, t1 = (float)(k + 1) / la->segs * 6.2831853f;
            float c0 = cosf(t0), s0 = sinf(t0), c1 = cosf(t1), s1 = sinf(t1);
            float a[3] = {la->x + c0 * r0, base + y0, la->z + s0 * r0};
            float b[3] = {la->x + c1 * r0, base + y0, la->z + s1 * r0};
            float c[3] = {la->x + c1 * r1, base + y1, la->z + s1 * r1};
            float d[3] = {la->x + c0 * r1, base + y1, la->z + s0 * r1};
            float mid = (t0 + t1) * 0.5f;
            float sh = face_shade(cosf(mid), sinf(mid));
            float u0 = (float)k / la->segs * 6.2831853f * fmaxf(r0, r1);
            float u1 = (float)(k + 1) / la->segs * 6.2831853f * fmaxf(r0, r1);
            quad(w, n, cap, a, b, c, d, u0, 0.0f, u1, plen, sh, sh * 0.94f);
        }
        g->count = *n - g->first;
    }
}

// Every open (unshared) navmesh edge gets something standing on it, unless the terrain already says// Every open (unshared) navmesh edge gets something standing on it, unless the terrain already says
// "no" — a height step or a building wall. Invisible walls in the middle of grass were the complaint.
static void emit_edges(Field *f, FVert **w, int *n, int cap) {
    static const char *KINDS[3] = {"fence", "hedge", "wall"};
    static const float KH[3] = {0.55f, 0.75f, 0.85f};
    for (int k = 0; k < 3; k++) {
        int art = -1, first = *n;
        for (int i = 0; i < f->poly_count; i++) {
            NavPoly *P = &f->polys[i];
            const char *kind = P->bkind[0] ? P->bkind : f->edge_kind;
            if (strcmp(kind, KINDS[k])) continue;
            for (int e = 0; e < P->n; e++) {
                if (P->nb[e] >= 0) continue;
                int j = (e + 1) % P->n;
                float ax = P->vx[e], az = P->vz[e], bx = P->vx[j], bz = P->vz[j];
                float ex = bx - ax, ez = bz - az, len = sqrtf(ex * ex + ez * ez);
                if (len < 0.05f) continue;
                float mx = (ax + bx) * 0.5f, mz = (az + bz) * 0.5f;
                float inx = -ez / len, inz = ex / len;                          // inward normal
                if (fabsf(grid_h(f, mx + inx * 0.45f, mz + inz * 0.45f) -
                          grid_h(f, mx - inx * 0.45f, mz - inz * 0.45f)) > 0.01f) continue;   // it is a cliff
                if (in_building(f, mx - inx * 0.35f, mz - inz * 0.35f, 0.60f)) continue;      // it is a wall
                if (near_exit(f, mx, mz, 0.5f)) continue;                                    // it is the way out
                if (art < 0) art = art_get(f, "edges", KINDS[k]);
                float h = KH[k], ya = P->vy[e], yb = P->vy[j];
                float p0[3] = {ax, ya + h, az}, p1[3] = {bx, yb + h, bz};
                float p2[3] = {bx, yb, bz}, p3[3] = {ax, ya, az};
                float sh = 0.74f + 0.32f * fabsf(inx);
                quad(w, n, cap, p0, p1, p2, p3, 0, 0, len, 1.0f, sh, sh * 0.80f);
            }
        }
        if (*n > first && art >= 0) {
            GeoRange *g = &f->geo[f->geo_count++];
            g->art = art; g->first = first; g->count = *n - first; g->alpha = true;
        }
    }
}

// One pass per tile id so the whole ground is a handful of draw calls; walls share the wall tile.
// Vertex colours bake the light (CLAUDE.md: no realtime lighting). Sun from +X, -Z.
static void build_world(Field *f) {
    f->geo_count = 0;
    int cap = f->mw * f->mh * 26 + F_POLYS * F_POLY_V * 6 + 48 * 66 + F_SWEEPS * F_WALL_PTS * 4 * 8 + F_LATHES * 48 * 8 * 6 + 64;
    FVert *buf = (FVert *)malloc(sizeof(FVert) * (size_t)cap);
    if (!buf) return;
    FVert *w = buf;
    int n = 0;
    // One pass for every non-water cell: the splat shader decides what each pixel of it is.
    f->floor_first = n;
    for (int y = 0; y < f->mh; y++) for (int x = 0; x < f->mw; x++) {
        if (f->tiles[f->gnd[y][x]].water) continue;
        float h = f->hgt[y][x] * HALF_STEP;
        float s = 0.86f + 0.035f * f->hgt[y][x] + 0.05f * frnd(x, y, 11);
        float a[3] = {(float)x, h, (float)y}, b[3] = {x + 1.0f, h, (float)y};
        float c[3] = {x + 1.0f, h, y + 1.0f}, d[3] = {(float)x, h, y + 1.0f};
        quad(&w, &n, cap, a, b, c, d, (float)x, (float)y, x + 1.0f, y + 1.0f, s, s);
    }
    f->floor_count = n - f->floor_first;
    for (int t = 0; t < f->tile_count; t++) {          // water still gets its own scrolling pass
        f->range_first[t] = n;
        if (f->tiles[t].water)
            for (int y = 0; y < f->mh; y++) for (int x = 0; x < f->mw; x++) {
                if (f->gnd[y][x] != t) continue;
                float h = f->hgt[y][x] * HALF_STEP;
                float s = 0.86f + 0.035f * f->hgt[y][x];
                float a[3] = {(float)x, h, (float)y}, b[3] = {x + 1.0f, h, (float)y};
                float c[3] = {x + 1.0f, h, y + 1.0f}, d[3] = {(float)x, h, y + 1.0f};
                quad(&w, &n, cap, a, b, c, d, (float)x, (float)y, x + 1.0f, y + 1.0f, s, s);
            }
        f->range_count[t] = n - f->range_first[t];
    }
    f->wall_first = n;
    for (int y = 0; y < f->mh; y++) for (int x = 0; x < f->mw; x++) {
        int hh = f->hgt[y][x];
        float top = hh * HALF_STEP;
        // three faces: east (+X, lit), west (-X, dark), south (+Z, toward the camera). North never shows.
        const int dxs[3] = {1, -1, 0}, dys[3] = {0, 0, 1};
        const float shade[3] = {1.06f, 0.60f, 0.74f};
        for (int e = 0; e < 3; e++) {
            int nx = x + dxs[e], ny = y + dys[e];
            int nh = (nx < 0 || ny < 0 || nx >= f->mw || ny >= f->mh) ? 0 : f->hgt[ny][nx];
            if (nh >= hh) continue;
            float bot = nh * HALF_STEP, span = (top - bot) / (HALF_STEP * 2.0f);   // one tile = one full step
            float a[3], b[3], c[3], d[3];
            if (e == 0) { a[0] = b[0] = c[0] = d[0] = x + 1.0f; a[2] = d[2] = (float)y; b[2] = c[2] = y + 1.0f; }
            else if (e == 1) { a[0] = b[0] = c[0] = d[0] = (float)x; a[2] = d[2] = (float)y; b[2] = c[2] = y + 1.0f; }
            else { a[2] = b[2] = c[2] = d[2] = y + 1.0f; a[0] = d[0] = (float)x; b[0] = c[0] = x + 1.0f; }
            a[1] = b[1] = top; c[1] = d[1] = bot;
            quad(&w, &n, cap, a, b, c, d, 0.0f, 0.0f, 1.0f, span, shade[e], shade[e] * 0.62f);   // base darker
        }
    }
    f->wall_count = n - f->wall_first;
    // Authored sweeps win: the automatic strips are only the fallback for a map with none.
    for (int i = 0; i < f->sweep_count; i++) emit_sweep(f, &f->sweeps[i], &w, &n, cap);
    for (int i = 0; i < f->lathe_count; i++) emit_lathe(f, &f->lathes[i], &w, &n, cap);
    if (!f->sweep_count) emit_edges(f, &w, &n, cap);
    glBindBuffer(GL_ARRAY_BUFFER, f->vbo_world);
    glBufferData(GL_ARRAY_BUFFER, sizeof(FVert) * (size_t)n, buf, GL_STATIC_DRAW);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    free(buf);
}

// ───────────────────────── GL setup ─────────────────────────

static const char *VS =
    "#version 300 es\n"
    "layout(location=0) in vec3 a_pos;\n"
    "layout(location=1) in vec2 a_uv;\n"
    "layout(location=2) in vec4 a_col;\n"
    "uniform mat4 u_mvp; uniform vec2 u_uvoff;\n"
    "out vec2 v_uv; out vec4 v_col;\n"
    "void main(){ v_uv = a_uv + u_uvoff; v_col = a_col; gl_Position = u_mvp * vec4(a_pos,1.0); }\n";
// Occluders (buildings, walls, fences, props, other walkers) cut a dithered circle out of
// themselves where they sit between the camera and the player. Screen-door, not alpha: no blending,
// no sorting, the depth test stays intact, and it reads as pixel art.
static const char *FS =
    "#version 300 es\n"
    "precision mediump float;\n"
    "in vec2 v_uv; in vec4 v_col;\n"
    "uniform sampler2D u_tex; uniform float u_alpha;\n"
    "uniform vec4 u_see;    // xy = player screen px, z = player window depth, w = radius px (0 = off)\n"
    "uniform float u_occl;  // 1 on anything that may block the player, 0 on ground and on the player\n"
    "out vec4 o;\n"
    "float bayer(vec2 p){\n"
    "  int m[16] = int[16](0,8,2,10,12,4,14,6,3,11,1,9,15,7,13,5);\n"
    "  return float(m[int(mod(p.y,4.0))*4 + int(mod(p.x,4.0))]) / 16.0;\n"
    "}\n"
    "void main(){\n"
    "  vec4 t = texture(u_tex, v_uv);\n"
    "  if (t.a < u_alpha) discard;\n"
    "  if (u_occl > 0.5 && u_see.w > 0.5 && gl_FragCoord.z < u_see.z - 0.0006) {\n"
    "    float d = distance(gl_FragCoord.xy, u_see.xy) / u_see.w;\n"
    "    if (d < 1.0 && bayer(gl_FragCoord.xy) + d < 1.0) discard;\n"
    "  }\n"
    "  o = vec4(t.rgb * v_col.rgb, 1.0);\n"
    "}\n";

// Ground: one splat texture (bilinear, once over the whole map) weighting four tile textures that
// each repeat in world space. Per-cell tile ids made every cell boundary read; this does not.
static const char *SVS =
    "#version 300 es\n"
    "layout(location=0) in vec3 a_pos;\n"
    "layout(location=1) in vec2 a_uv;\n"
    "layout(location=2) in vec4 a_col;\n"
    "uniform mat4 u_mvp;\n"
    "out vec2 v_uv; out vec4 v_col;\n"
    "void main(){ v_uv = a_uv; v_col = a_col; gl_Position = u_mvp * vec4(a_pos,1.0); }\n";
static const char *SFS =
    "#version 300 es\n"
    "precision mediump float;\n"
    "in vec2 v_uv; in vec4 v_col;\n"
    "uniform sampler2D u_splat, u_l0, u_l1, u_l2, u_l3;\n"
    "uniform vec4 u_scale;   // tiles per cell for each layer\n"
    "uniform vec4 u_sharp;   // per layer: 1 = smooth into its neighbour, 8+ = a crisp edge\n"
    "uniform vec4 u_hk;      // per layer: how much its height mask (the alpha) pushes the edge\n"
    "uniform vec2 u_map;     // map size in cells\n"
    "uniform vec2 u_blend;   // x unused, y = dither amount\n"
    "out vec4 o;\n"
    "float bayer(vec2 p){\n"
    "  int m[16] = int[16](0,8,2,10,12,4,14,6,3,11,1,9,15,7,13,5);\n"
    "  return float(m[int(mod(p.y,4.0))*4 + int(mod(p.x,4.0))]) / 16.0;\n"
    "}\n"
    "void main(){\n"
    "  vec4 w = texture(u_splat, v_uv / u_map);\n"
    "  w.x += max(0.0, 1.0 - (w.x + w.y + w.z + w.w));        // whatever is left is layer 0\n"
    "  vec4 t0 = texture(u_l0, v_uv * u_scale.x);\n"
    "  vec4 t1 = texture(u_l1, v_uv * u_scale.y);\n"
    "  vec4 t2 = texture(u_l2, v_uv * u_scale.z);\n"
    "  vec4 t3 = texture(u_l3, v_uv * u_scale.w);\n"
    "  // Height-aware weights: a tile's raised pixels hold on longer than its cracks, so the edge\n"
    "  // follows the artwork instead of the splat map's resolution.\n"
    "  vec4 hgt = vec4(t0.a, t1.a, t2.a, t3.a) - 0.5;\n"
    "  vec4 e = max(w + hgt * u_hk, vec4(0.0));\n"
    "  if (u_blend.y > 0.001) e = max(e + (bayer(gl_FragCoord.xy) - 0.5) * u_blend.y, vec4(0.0));\n"
    "  float m = max(max(e.x, e.y), max(e.z, e.w));\n"
    "  // Contrast around the winner, each layer at its own sharpness: a hard layer wins its pair.\n"
    "  vec4 p = pow(e / max(m, 1e-4), u_sharp);\n"
    "  p /= max(p.x + p.y + p.z + p.w, 1e-4);\n"
    "  vec3 c = p.x * t0.rgb + p.y * t1.rgb + p.z * t2.rgb + p.w * t3.rgb;\n"
    "  o = vec4(c * v_col.rgb, 1.0);\n"
    "}\n";

static GLuint make_shader(GLenum type, const char *src) {
    GLuint s = glCreateShader(type);
    glShaderSource(s, 1, &src, nullptr);
    glCompileShader(s);
    GLint ok = 0;
    glGetShaderiv(s, GL_COMPILE_STATUS, &ok);
    if (!ok) { char log[1024]; glGetShaderInfoLog(s, sizeof(log), nullptr, log); SDL_Log("field shader: %s", log); }
    return s;
}

// Created lazily on the first field frame and again after every hot reload; the old .so's GL objects
// leak, which is accepted (CLAUDE.md: never dlclose on Android).
static void field_gl_init(Field *f) {
    GLuint vs = make_shader(GL_VERTEX_SHADER, VS), fs = make_shader(GL_FRAGMENT_SHADER, FS);
    f->prog = glCreateProgram();
    glAttachShader(f->prog, vs); glAttachShader(f->prog, fs);
    glLinkProgram(f->prog);
    GLint ok = 0;
    glGetProgramiv(f->prog, GL_LINK_STATUS, &ok);
    if (!ok) { char log[1024]; glGetProgramInfoLog(f->prog, sizeof(log), nullptr, log); SDL_Log("field link: %s", log); }
    glDeleteShader(vs); glDeleteShader(fs);
    f->u_mvp = glGetUniformLocation(f->prog, "u_mvp");
    f->u_uvoff = glGetUniformLocation(f->prog, "u_uvoff");
    f->u_tex = glGetUniformLocation(f->prog, "u_tex");
    f->u_alpha = glGetUniformLocation(f->prog, "u_alpha");
    f->u_see = glGetUniformLocation(f->prog, "u_see");
    f->u_occl = glGetUniformLocation(f->prog, "u_occl");

    {
        GLuint sv = make_shader(GL_VERTEX_SHADER, SVS), sf = make_shader(GL_FRAGMENT_SHADER, SFS);
        f->sprog = glCreateProgram();
        glAttachShader(f->sprog, sv); glAttachShader(f->sprog, sf);
        glLinkProgram(f->sprog);
        GLint sok = 0;
        glGetProgramiv(f->sprog, GL_LINK_STATUS, &sok);
        if (!sok) { char log[1024]; glGetProgramInfoLog(f->sprog, sizeof(log), nullptr, log); SDL_Log("field splat link: %s", log); }
        glDeleteShader(sv); glDeleteShader(sf);
        f->s_mvp = glGetUniformLocation(f->sprog, "u_mvp");
        f->s_splat = glGetUniformLocation(f->sprog, "u_splat");
        const char *ln[4] = {"u_l0", "u_l1", "u_l2", "u_l3"};
        for (int i = 0; i < 4; i++) f->s_lay[i] = glGetUniformLocation(f->sprog, ln[i]);
        f->s_scale = glGetUniformLocation(f->sprog, "u_scale");
        f->s_map = glGetUniformLocation(f->sprog, "u_map");
        f->s_blend = glGetUniformLocation(f->sprog, "u_blend");
        f->s_sharp = glGetUniformLocation(f->sprog, "u_sharp");
        f->s_hk = glGetUniformLocation(f->sprog, "u_hk");
    }

    glGenVertexArrays(1, &f->vao);
    glGenBuffers(1, &f->vbo_world);
    glGenBuffers(1, &f->vbo_spr);

    unsigned char white[4] = {255, 255, 255, 255};
    f->white = ftex_upload(white, 1, 1, false, false);

    glGenTextures(1, &f->fbo_tex);
    glBindTexture(GL_TEXTURE_2D, f->fbo_tex);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, FBO_W, FBO_H, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);         // nearest upscale: pixel art
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glGenRenderbuffers(1, &f->fbo_depth);
    glBindRenderbuffer(GL_RENDERBUFFER, f->fbo_depth);
    glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT24, FBO_W, FBO_H);
    glGenFramebuffers(1, &f->fbo);
    glBindFramebuffer(GL_FRAMEBUFFER, f->fbo);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, f->fbo_tex, 0);
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, f->fbo_depth);
    GLenum st = glCheckFramebufferStatus(GL_FRAMEBUFFER);
    if (st != GL_FRAMEBUFFER_COMPLETE) SDL_Log("field: FBO incomplete 0x%x", st);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glBindRenderbuffer(GL_RENDERBUFFER, 0);
    glBindTexture(GL_TEXTURE_2D, 0);

    f->spr_cap = F_POLYS * F_POLY_V * 2 + (F_PROPS + F_NPCS + 4) * 6;
    f->spr = (FVert *)malloc(sizeof(FVert) * (size_t)f->spr_cap);
    f->gl_ready = true;
    build_world(f);
}

// ───────────────────────── camera zones ─────────────────────────

static int zone_at(Field *f, float x, float z) {
    int hit = f->zone_idx >= 0 && f->zone_idx < f->zone_count ? f->zone_idx : 0;
    for (int i = 0; i < f->zone_count; i++) {                 // last definition wins on overlap
        CamZone *c = &f->zones[i];
        if (x >= c->x && z >= c->z && x < c->x + c->w && z < c->z + c->d) hit = i;
    }
    return hit;
}

// Turns the active zone into an eye, a target and a field of view. `u` blends the authored shot
// toward looking straight at the player: 0 is the shot as written, 1 always frames the player.
static void zone_shot(Field *f, const CamZone *z, float u, float *eye, float *at, float *fov) {
    float head[3] = {f->px, f->py + 0.9f, f->pz};
    switch (z->mode) {
    case CAM_FIXED:
        eye[0] = z->cx; eye[1] = z->cy; eye[2] = z->cz;
        at[0] = z->tx; at[1] = z->ty; at[2] = z->tz;
        *fov = z->fov;
        break;
    case CAM_RAIL: {
        float p;
        if (z->w >= z->d) p = (f->px - z->x) / (z->w > 0.01f ? z->w : 1.0f);
        else p = (f->pz - z->z) / (z->d > 0.01f ? z->d : 1.0f);
        p = p < 0 ? 0 : p > 1 ? 1 : p;
        eye[0] = z->ax + (z->bx - z->ax) * p;
        eye[1] = z->ay + (z->by - z->ay) * p;
        eye[2] = z->az + (z->bz - z->az) * p;
        at[0] = z->tx; at[1] = z->ty; at[2] = z->tz;
        *fov = z->fov;
    } break;
    default: {                                    // follow: the eye is rigid to the target
        at[0] = f->foc[0] + (f->px - f->foc[0]) * u;
        at[2] = f->foc[2] + (f->pz - f->foc[2]) * u;
        at[1] = (f->py + z->height) + (head[1] - (f->py + z->height)) * u;
        float y = z->yaw * DEG, p = z->pitch * DEG, back = z->dist * cosf(p);
        eye[0] = at[0] + sinf(y) * back;
        eye[1] = at[1] + z->dist * sinf(p);
        eye[2] = at[2] + cosf(y) * back;
        *fov = z->fov;
        return;                                   // follow blends inside `at` above, not after
    }
    }
    for (int i = 0; i < 3; i++) at[i] += (head[i] - at[i]) * u;   // fixed and rail pan toward the player
}

// Where the player's head lands in normalised device coordinates for a candidate shot.
static bool project_head(Field *f, const float *eye, const float *at, float fov, float aspect, float *nx, float *ny) {
    float view[16], proj[16], mvp[16];
    m_look(view, eye, at);
    m_persp(proj, fov, aspect, 0.4f, 300.0f);
    m_mul(mvp, proj, view);
    float p[3] = {f->px, f->py + 0.9f, f->pz};
    float cx = mvp[0] * p[0] + mvp[4] * p[1] + mvp[8] * p[2] + mvp[12];
    float cy = mvp[1] * p[0] + mvp[5] * p[1] + mvp[9] * p[2] + mvp[13];
    float cw = mvp[3] * p[0] + mvp[7] * p[1] + mvp[11] * p[2] + mvp[15];
    if (cw < 0.05f) return false;
    *nx = cx / cw; *ny = cy / cw;
    return true;
}

static bool head_framed(Field *f, const float *eye, const float *at, float fov, float aspect, float lim) {
    float nx, ny;
    if (!project_head(f, eye, at, fov, aspect, &nx, &ny)) return false;
    return fabsf(nx) <= lim && fabsf(ny) <= lim;
}

static void cam_update(Field *f, float dt, float aspect) {
    // The follow focus leads the player a little and lags behind them; the framing rule below is what
    // actually guarantees they stay on screen, so this only has to feel right, not be safe.
    float lead = 0.9f;
    float des[3] = {f->px + f->move_dx * lead, f->py, f->pz + f->move_dz * lead};
    if (des[0] < 0) des[0] = 0;
    if (des[0] > f->mw) des[0] = (float)f->mw;
    if (des[2] < 0) des[2] = 0;
    if (des[2] > f->mh) des[2] = (float)f->mh;
    float kf = 1.0f - expf(-dt / 0.28f);
    for (int i = 0; i < 3; i++) f->foc[i] += (des[i] - f->foc[i]) * kf;

    int zi = zone_at(f, f->px, f->pz);
    if (zi != f->zone_idx) {
        if (f->zone_idx >= 0) {
            memcpy(f->b_eye, f->eye, sizeof(f->b_eye));
            memcpy(f->b_at, f->at, sizeof(f->b_at));
            f->b_fov = f->fov;
            f->blend_t = ZONE_BLEND;
        }
        f->zone_idx = zi;
    }
    CamZone *z = &f->zones[f->zone_idx < 0 ? 0 : f->zone_idx];

    // Keep the player inside the middle 50% of frame on a follow shot and inside 80% on a fixed or
    // rail shot: binary-search the smallest blend toward them that does it. `pan` just forces it on.
    float lim = (z->mode == CAM_FOLLOW) ? 0.50f : 0.70f;   // 0.70, not 0.80: at 0.8 a long street
                                                           // shot leaves the player half behind a prop
    float eye[3], at[3], fov, need = 1.0f;
    if (!(z->mode == CAM_FIXED && z->pan)) {
        zone_shot(f, z, 0.0f, eye, at, &fov);
        if (head_framed(f, eye, at, fov, aspect, lim)) need = 0.0f;
        else {
            float lo = 0.0f, hi = 1.0f;
            for (int it = 0; it < 10; it++) {
                float mid = (lo + hi) * 0.5f;
                zone_shot(f, z, mid, eye, at, &fov);
                if (head_framed(f, eye, at, fov, aspect, lim)) hi = mid; else lo = mid;
            }
            need = hi;
        }
    }
    if (need > f->frame_u) f->frame_u = need;                       // push at once, release slowly
    else f->frame_u += (need - f->frame_u) * (1.0f - expf(-dt / 0.5f));
    zone_shot(f, z, f->frame_u, eye, at, &fov);

    if (f->blend_t > 0) {
        f->blend_t -= dt;
        float u = 1.0f - (f->blend_t > 0 ? f->blend_t / ZONE_BLEND : 0.0f);
        u = u * u * (3.0f - 2.0f * u);
        for (int i = 0; i < 3; i++) {
            f->eye[i] = f->b_eye[i] + (eye[i] - f->b_eye[i]) * u;
            f->at[i] = f->b_at[i] + (at[i] - f->b_at[i]) * u;
        }
        f->fov = f->b_fov + (fov - f->b_fov) * u;
        return;
    }
    float tau = (z->mode == CAM_FOLLOW) ? 0.14f : 0.06f;
    float k = 1.0f - expf(-dt / tau);
    if (f->fov <= 0.0f) { memcpy(f->eye, eye, sizeof(f->eye)); memcpy(f->at, at, sizeof(f->at)); f->fov = fov; return; }
    for (int i = 0; i < 3; i++) { f->eye[i] += (eye[i] - f->eye[i]) * k; f->at[i] += (at[i] - f->at[i]) * k; }
    f->fov += (fov - f->fov) * k;

    // Framing is a guarantee, not a target. The blend above aims the shot; this clamps the *smoothed*
    // camera, which is what actually gets drawn, so nothing (lag, a zone edge, a rail end) can let the
    // player slip off screen. It warns first, so the Dev log still says when a shot needed rescuing.
    if (f->warn_cool > 0) f->warn_cool -= dt;
    float nx, ny;
    bool on = project_head(f, f->eye, f->at, f->fov, aspect, &nx, &ny) && fabsf(nx) <= 0.90f && fabsf(ny) <= 0.90f;
    if (!on) {
        if (f->warn_cool <= 0 && !f->warn_pending) {
            snprintf(f->warn, sizeof(f->warn), "camera: clamped onto the player at %s %.1f,%.1f zone %d u %.2f (ndc %.2f,%.2f)",
                     f->map_name, f->px, f->pz, f->zone_idx, f->frame_u, nx, ny);
            f->warn_pending = true;
            f->warn_cool = 4.0f;
        }
        float head[3] = {f->px, f->py + 0.9f, f->pz};
        float base[3] = {f->at[0], f->at[1], f->at[2]};
        float lo = 0.0f, hi = 1.0f;
        for (int it = 0; it < 10; it++) {
            float mid = (lo + hi) * 0.5f, a2[3], tx, ty;
            for (int i = 0; i < 3; i++) a2[i] = base[i] + (head[i] - base[i]) * mid;
            if (project_head(f, f->eye, a2, f->fov, aspect, &tx, &ty) && fabsf(tx) <= 0.90f && fabsf(ty) <= 0.90f) hi = mid;
            else lo = mid;
        }
        for (int i = 0; i < 3; i++) f->at[i] = base[i] + (head[i] - base[i]) * hi;
    }
}

// The ground projection of the view direction: "up" on the stick always walks away from the camera,
// whatever the shot does, so turning the camera never inverts the controls.
static void cam_basis(Field *f, float *fx, float *fz, float *rx, float *rz) {
    float dx = f->at[0] - f->eye[0], dz = f->at[2] - f->eye[2];
    float l = sqrtf(dx * dx + dz * dz);
    if (l < 1e-4f) { dx = 0; dz = -1; l = 1; }
    *fx = dx / l; *fz = dz / l;
    *rx = -*fz; *rz = *fx;
}

// ───────────────────────── touch input ─────────────────────────

struct Touch { SDL_FingerID id; float x, y; };

static int gather_touch(Touch *out, int max, int w, int h) {
    int n = 0, nd = 0;
    SDL_TouchID *devs = SDL_GetTouchDevices(&nd);
    if (devs) {
        for (int d = 0; d < nd && n < max; d++) {
            int nf = 0;
            SDL_Finger **fg = SDL_GetTouchFingers(devs[d], &nf);
            if (!fg) continue;
            for (int i = 0; i < nf && n < max; i++) {
                out[n].id = fg[i]->id;
                out[n].x = fg[i]->x * w;
                out[n].y = fg[i]->y * h;
                n++;
            }
            SDL_free(fg);
        }
        SDL_free(devs);
        if (nd > 0) return n;
    }
    // Desktop (and any device with no touch panel): the mouse stands in for one finger.
    if (ImGui::IsMouseDown(0)) { out[0].id = 1; out[0].x = ImGui::GetIO().MousePos.x; out[0].y = ImGui::GetIO().MousePos.y; return 1; }
    return 0;
}

static void field_input(Field *f, int w, int h, bool blocked, float dt) {
    Touch t[8];
    int n = blocked ? 0 : gather_touch(t, 8, w, h);
    f->tapped = false;

    bool stick_seen = false, act_seen = false;
    for (int i = 0; i < n; i++) {
        if (f->stick_on && t[i].id == f->stick_id) { stick_seen = true; f->stick_x = t[i].x; f->stick_y = t[i].y; }
        if (f->act_on && t[i].id == f->act_id) act_seen = true;
    }
    if (f->stick_on && !stick_seen) f->stick_on = false;
    if (f->act_on && !act_seen) {                                   // release: a short press is the action
        if (f->act_t <= 0.35f) f->tapped = true;
        f->act_on = false; f->run = false; f->act_t = 0;
    }
    for (int i = 0; i < n; i++) {
        bool mine = (f->stick_on && t[i].id == f->stick_id) || (f->act_on && t[i].id == f->act_id);
        if (mine) continue;
        if (t[i].x < w * 0.5f && !f->stick_on) {                    // left half: the stick appears under the thumb
            f->stick_on = true; f->stick_id = t[i].id;
            f->stick_ox = t[i].x; f->stick_oy = t[i].y; f->stick_x = t[i].x; f->stick_y = t[i].y;
        } else if (t[i].x >= w * 0.5f && !f->act_on) {
            f->act_on = true; f->act_id = t[i].id; f->act_t = 0;
        }
    }
    if (f->act_on) { f->act_t += dt; if (f->act_t > 0.35f) f->run = true; }   // long press = run
}

// 8-direction snap with a dead zone, in SCREEN space; the caller maps it through the camera basis.
static bool stick_dir(Field *f, int w, float *sx, float *sy) {
    if (!f->stick_on) return false;
    float dx = f->stick_x - f->stick_ox, dy = f->stick_y - f->stick_oy;
    float len = sqrtf(dx * dx + dy * dy);
    if (len < w * 0.022f) return false;
    float a = atan2f(dy, dx);
    int oct = (int)floorf(a / (45.0f * DEG) + 0.5f);
    a = oct * 45.0f * DEG;
    *sx = cosf(a); *sy = sinf(a);
    if (fabsf(*sx) < 0.01f) *sx = 0;
    if (fabsf(*sy) < 0.01f) *sy = 0;
    return true;
}

// ───────────────────────── render ─────────────────────────

static void set_attribs() {
    glEnableVertexAttribArray(0); glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(FVert), (void *)0);
    glEnableVertexAttribArray(1); glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, sizeof(FVert), (void *)12);
    glEnableVertexAttribArray(2); glVertexAttribPointer(2, 4, GL_UNSIGNED_BYTE, GL_TRUE, sizeof(FVert), (void *)20);
}

// A full camera-facing billboard: its axes are the camera's right and up, so it always projects to a
// true rectangle at its authored pixel size. A Y-only billboard splays badly at the edges of frame
// under a pitched camera (world-up lines diverge), which is what the first build looked like.
// Leaning back along the camera's up vector also lifts the sprite clear of the ground behind it,
// so the depth test alone is enough — no sorting, no stretch factor.
static void push_billboard(Field *f, int *n, float cx, float base_y, float cz,
                           const float *sv, const float *uv, float wu, float hu,
                           float u0, float v0, float u1, float v1, float shade) {
    if (*n + 6 > f->spr_cap) return;
    FVert *w = f->spr;
    float hx = sv[0] * wu * 0.5f, hy = sv[1] * wu * 0.5f, hz = sv[2] * wu * 0.5f;
    float tx = uv[0] * hu, ty = uv[1] * hu, tz = uv[2] * hu;
    float a[3] = {cx - hx + tx, base_y - hy + ty, cz - hz + tz};
    float b[3] = {cx + hx + tx, base_y + hy + ty, cz + hz + tz};
    float c[3] = {cx + hx, base_y + hy, cz + hz}, d[3] = {cx - hx, base_y - hy, cz - hz};
    quad(&w, n, f->spr_cap, a, b, c, d, u0, v0, u1, v1, shade, shade);
}

static int prop_base(Field *f, const FProp *pr, float *bx, float *bz) {
    int cx = pr->x < 0 ? 0 : pr->x >= f->mw ? f->mw - 1 : pr->x;
    int cy = pr->y < 0 ? 0 : pr->y >= f->mh ? f->mh - 1 : pr->y;
    *bx = pr->x + pr->w * 0.5f; *bz = pr->y + 0.5f;
    return f->hgt[cy][cx];
}

// Which walker row (S/W/E/N) to draw for a world-space facing, given the camera. Facings are authored
// in world terms; with a rotated zone the sprite still has to look right on screen.
static int facing_row(int facing, float fx, float fz, float rx, float rz) {
    const float wx[4] = {0, -1, 1, 0}, wz[4] = {1, 0, 0, -1};
    float dx = wx[facing & 3], dz = wz[facing & 3];
    float toward = -(dx * fx + dz * fz), side = dx * rx + dz * rz;
    if (fabsf(toward) >= fabsf(side)) return toward >= 0 ? 0 : 3;
    return side >= 0 ? 2 : 1;
}

// Is anything between the camera and the player's chest? A ray against each building box plus a
// march over the wall heightfield. Cheap, and it keeps the shader's circle from opening on nothing.
static bool player_occluded(Field *f) {
    float to[3] = {f->px, f->py + 0.55f, f->pz};
    float d[3] = {to[0] - f->eye[0], to[1] - f->eye[1], to[2] - f->eye[2]};
    float len = sqrtf(d[0] * d[0] + d[1] * d[1] + d[2] * d[2]);
    if (len < 0.2f) return false;
    for (int i = 0; i < f->bld_count; i++) {                 // ray vs the oriented box
        Bld *b = &f->blds[i];
        float cx = b->x + b->w * 0.5f, cz = b->z + b->d * 0.5f;
        float c = cosf(-b->rot * DEG), sn = sinf(-b->rot * DEG);
        float ox = f->eye[0] - cx, oz = f->eye[2] - cz;
        float lo0 = ox * c - oz * sn, lo2 = ox * sn + oz * c;
        float ld0 = d[0] * c - d[2] * sn, ld2 = d[0] * sn + d[2] * c;
        float lo1 = f->eye[1] - grid_h(f, cx, cz), ld1 = d[1];
        float half[3] = {b->w * 0.5f, b->h * 0.5f + b->h * 0.17f, b->d * 0.5f};
        float lo[3] = {lo0, lo1 - half[1], lo2}, ld[3] = {ld0, ld1, ld2};
        float t0 = 0.0f, t1 = 1.0f;
        bool hit = true;
        for (int a = 0; a < 3 && hit; a++) {
            if (fabsf(ld[a]) < 1e-6f) { if (fabsf(lo[a]) > half[a]) hit = false; continue; }
            float ta = (-half[a] - lo[a]) / ld[a], tb = (half[a] - lo[a]) / ld[a];
            if (ta > tb) { float t = ta; ta = tb; tb = t; }
            if (ta > t0) t0 = ta;
            if (tb < t1) t1 = tb;
            if (t0 > t1) hit = false;
        }
        if (hit && t1 > 0.02f && t0 < 0.96f) return true;
    }
    // then the fence/hedge/wall strips: a 2D ray-segment test against every unshared navmesh edge
    for (int i = 0; i < f->poly_count; i++) {
        NavPoly *P = &f->polys[i];
        for (int e = 0; e < P->n; e++) {
            if (P->nb[e] >= 0) continue;
            int j = (e + 1) % P->n;
            float ax = P->vx[e], az = P->vz[e], ex = P->vx[j] - ax, ez = P->vz[j] - az;
            float den = d[0] * ez - d[2] * ex;
            if (fabsf(den) < 1e-6f) continue;
            float rx = ax - f->eye[0], rz = az - f->eye[2];
            float t = (rx * ez - rz * ex) / den;              // along the camera -> player ray
            float u = (rx * d[2] - rz * d[0]) / -den;         // along the edge
            if (t <= 0.03f || t >= 0.94f || u < 0.0f || u > 1.0f) continue;
            float top = (P->vy[e] + (P->vy[j] - P->vy[e]) * u) + 0.70f;
            if (f->eye[1] + d[1] * t < top) return true;
        }
    }
    for (int i = 0; i < f->prop_count; i++) {                 // and anything tall enough on the ground
        FProp *pr = &f->props[i];
        if (pr->art < 0) continue;
        FTex *tx = &f->art[pr->art].tex;
        float bx = pr->x + pr->w * 0.5f, bz = pr->y + 0.5f;
        float t = ((bx - f->eye[0]) * d[0] + (bz - f->eye[2]) * d[2]) / (d[0] * d[0] + d[2] * d[2] + 1e-6f);
        if (t <= 0.03f || t >= 0.94f) continue;
        float qx = f->eye[0] + d[0] * t, qz = f->eye[2] + d[2] * t;
        float rad = tx->w / PX_PER_CELL * 0.30f;
        if ((qx - bx) * (qx - bx) + (qz - bz) * (qz - bz) > rad * rad) continue;
        if (f->eye[1] + d[1] * t < grid_h(f, bx, bz) + tx->h / PX_PER_CELL * 0.80f) return true;
    }
    int steps = (int)(len / 0.35f);                          // then the wall heightfield
    if (steps > 120) steps = 120;
    for (int i = 1; i < steps; i++) {
        float t = (float)i / steps;
        float x = f->eye[0] + d[0] * t, yy = f->eye[1] + d[1] * t, z = f->eye[2] + d[2] * t;
        if (x < 0 || z < 0 || x >= f->mw || z >= f->mh) continue;
        if (grid_h(f, x, z) > yy + 0.12f) return true;
    }
    return false;
}

static void field_render(Field *f, int win_w, int win_h, int vw, int vh) {
    glBindFramebuffer(GL_FRAMEBUFFER, f->fbo);
    glViewport(0, 0, vw, vh);
    glClearColor(0.10f, 0.12f, 0.17f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LEQUAL);
    glDepthMask(GL_TRUE);
    glDisable(GL_BLEND);
    glDisable(GL_CULL_FACE);
    glUseProgram(f->prog);
    glUniform1i(f->u_tex, 0);
    glActiveTexture(GL_TEXTURE0);

    float view[16], proj[16];
    m_look(view, f->eye, f->at);
    m_persp(proj, f->fov, (float)vw / (float)vh, 0.4f, 300.0f);
    m_mul(f->mvp, proj, view);
    glUniformMatrix4fv(f->u_mvp, 1, GL_FALSE, f->mvp);

    // Where the player's chest lands on screen, and how deep it is: the shader cuts occluders there.
    {
        float p[3] = {f->px, f->py + 0.55f, f->pz};
        const float *mm = f->mvp;
        float cx = mm[0] * p[0] + mm[4] * p[1] + mm[8] * p[2] + mm[12];
        float cy = mm[1] * p[0] + mm[5] * p[1] + mm[9] * p[2] + mm[13];
        float cz = mm[2] * p[0] + mm[6] * p[1] + mm[10] * p[2] + mm[14];
        float cw = mm[3] * p[0] + mm[7] * p[1] + mm[11] * p[2] + mm[15];
        if (cw > 0.05f && f->see_r > 0.5f)
            glUniform4f(f->u_see, (cx / cw * 0.5f + 0.5f) * vw, (cy / cw * 0.5f + 0.5f) * vh,
                        cz / cw * 0.5f + 0.5f, f->see_r);
        else glUniform4f(f->u_see, 0.0f, 0.0f, 1.0f, 0.0f);
    }
    glUniform1f(f->u_occl, 0.0f);                           // ground and water never cut

    glBindVertexArray(f->vao);
    glBindBuffer(GL_ARRAY_BUFFER, f->vbo_world);
    set_attribs();

    if (f->floor_count && f->splat_tex.id) {           // the ground, as four blended layers
        glUseProgram(f->sprog);
        glUniformMatrix4fv(f->s_mvp, 1, GL_FALSE, f->mvp);
        glUniform2f(f->s_map, (float)f->mw, (float)f->mh);
        glUniform2f(f->s_blend, 0.0f, f->splat_dither);
        glUniform4f(f->s_sharp, f->layer_sharp[0], f->layer_sharp[1], f->layer_sharp[2], f->layer_sharp[3]);
        glUniform4f(f->s_hk, f->layer_hk[0], f->layer_hk[1], f->layer_hk[2], f->layer_hk[3]);
        glUniform4f(f->s_scale, 1.0f / f->layer_cells[0], 1.0f / f->layer_cells[1],
                    1.0f / f->layer_cells[2], 1.0f / f->layer_cells[3]);
        glUniform1i(f->s_splat, 0);
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, f->splat_tex.id);
        for (int i = 0; i < 4; i++) {
            glUniform1i(f->s_lay[i], i + 1);
            glActiveTexture(GL_TEXTURE1 + i);
            glBindTexture(GL_TEXTURE_2D, f->tiles[f->layer_tile[i]].tex.id);
        }
        glDrawArrays(GL_TRIANGLES, f->floor_first, f->floor_count);
        for (int i = 0; i < 4; i++) { glActiveTexture(GL_TEXTURE1 + i); glBindTexture(GL_TEXTURE_2D, 0); }
        glActiveTexture(GL_TEXTURE0);
        glUseProgram(f->prog);
        glUniform1i(f->u_tex, 0);
        glUniformMatrix4fv(f->u_mvp, 1, GL_FALSE, f->mvp);
    }

    glUniform1f(f->u_alpha, 0.0f);
    for (int t = 0; t < f->tile_count; t++) {
        if (!f->range_count[t]) continue;
        glBindTexture(GL_TEXTURE_2D, f->tiles[t].tex.id);
        if (f->tiles[t].water) glUniform2f(f->u_uvoff, f->water_t * 0.06f, f->water_t * 0.22f);
        else glUniform2f(f->u_uvoff, 0.0f, 0.0f);
        glDrawArrays(GL_TRIANGLES, f->range_first[t], f->range_count[t]);
    }
    glUniform2f(f->u_uvoff, 0.0f, 0.0f);
    glUniform1f(f->u_occl, 1.0f);                           // walls, fences and buildings all cut
    if (f->wall_count) {
        glBindTexture(GL_TEXTURE_2D, f->tiles[f->wall_tile].tex.id);
        glDrawArrays(GL_TRIANGLES, f->wall_first, f->wall_count);
    }
    for (int i = 0; i < f->geo_count; i++) {               // edge strips and building faces
        GeoRange *g = &f->geo[i];
        if (g->count <= 0 || g->art < 0) continue;
        glUniform1f(f->u_alpha, g->alpha ? 0.5f : 0.0f);
        glBindTexture(GL_TEXTURE_2D, f->art[g->art].tex.id);
        glDrawArrays(GL_TRIANGLES, g->first, g->count);
    }
    glUniform1f(f->u_alpha, 0.0f);

    float fx, fz, rx, rz;
    cam_basis(f, &fx, &fz, &rx, &rz);
    float fwdv[3] = {f->at[0] - f->eye[0], f->at[1] - f->eye[1], f->at[2] - f->eye[2]};
    v_norm(fwdv);
    float sv[3] = {-fwdv[2], 0.0f, fwdv[0]};                        // camera right (world up is +Y)
    v_norm(sv);
    float uv[3] = {sv[1] * fwdv[2] - sv[2] * fwdv[1], sv[2] * fwdv[0] - sv[0] * fwdv[2],
                   sv[0] * fwdv[1] - sv[1] * fwdv[0]};              // camera up
    v_norm(uv);
    glUniform1f(f->u_alpha, 0.5f);                                  // discard, so no blend sorting
    int n = 0, dn = 0;
    struct Draw { int art, first; bool occl; };
    static Draw draws[F_PROPS + F_NPCS + 8];
    for (int i = 0; i < f->prop_count && dn < (int)(sizeof(draws) / sizeof(draws[0])); i++) {
        FProp *pr = &f->props[i];
        if (pr->art < 0) continue;
        FTex *tx = &f->art[pr->art].tex;
        float bx, bz;
        int hh = prop_base(f, pr, &bx, &bz);
        draws[dn].art = pr->art; draws[dn].first = n; draws[dn].occl = true; dn++;
        push_billboard(f, &n, bx, hh * HALF_STEP, bz, sv, uv,
                       tx->w / PX_PER_CELL, tx->h / PX_PER_CELL, 0, 0, 1, 1, 1.0f);
    }
    for (int i = 0; i <= f->npc_count && dn < (int)(sizeof(draws) / sizeof(draws[0])); i++) {
        bool player = (i == f->npc_count);
        int art, face, frame;
        float wx, wz, base;
        if (player) {
            art = art_get(f, "walkers", "falke");
            wx = f->px; wz = f->pz; base = f->py; face = f->facing;
            frame = f->walking ? ((int)(f->anim_t * 7.0f) & 3) : 0;
        } else {
            FNpc *np = &f->npcs[i];
            int cx = np->x < 0 ? 0 : np->x >= f->mw ? f->mw - 1 : np->x;
            int cy = np->y < 0 ? 0 : np->y >= f->mh ? f->mh - 1 : np->y;
            art = np->art; wx = np->x + 0.5f; wz = np->y + 0.5f;
            int pi = mesh_find(f, wx, wz);
            base = (pi >= 0) ? poly_height(&f->polys[pi], wx, wz) : f->hgt[cy][cx] * HALF_STEP;
            face = np->facing; frame = 0;
        }
        if (art < 0) continue;
        FTex *tx = &f->art[art].tex;
        int row = facing_row(face, fx, fz, rx, rz);
        if (player && !f->msg[0] && (f->exam_trig >= 0 || f->exam_npc >= 0) &&
            dn + 1 < (int)(sizeof(draws) / sizeof(draws[0]))) {
            int mk = art_get(f, "props", "mark_examine");
            if (mk >= 0) {
                FTex *mt = &f->art[mk].tex;
                float bob = 0.03f * sinf(f->water_t * 5.0f);
                draws[dn].art = mk; draws[dn].first = n; draws[dn].occl = false; dn++;
                push_billboard(f, &n, wx, base + tx->h / 4.0f / PX_PER_CELL + 0.12f + bob, wz, sv, uv,
                               mt->w / PX_PER_CELL, mt->h / PX_PER_CELL, 0, 0, 1, 1, 1.0f);
            }
        }
        float fw = tx->w / 4.0f, fh = tx->h / 4.0f;
        draws[dn].art = art; draws[dn].first = n; draws[dn].occl = !player; dn++;
        push_billboard(f, &n, wx, base, wz, sv, uv, fw / PX_PER_CELL, fh / PX_PER_CELL,
                       frame * 0.25f, row * 0.25f, frame * 0.25f + 0.25f, row * 0.25f + 0.25f, 1.0f);
    }
    if (n) {
        glBindBuffer(GL_ARRAY_BUFFER, f->vbo_spr);
        glBufferData(GL_ARRAY_BUFFER, sizeof(FVert) * (size_t)n, f->spr, GL_STREAM_DRAW);
        set_attribs();
        for (int i = 0; i < dn; i++) {
            glUniform1f(f->u_occl, draws[i].occl ? 1.0f : 0.0f);   // the player and its mark never cut
            glBindTexture(GL_TEXTURE_2D, f->art[draws[i].art].tex.id);
            glDrawArrays(GL_TRIANGLES, draws[i].first, 6);
        }
    }

    // Navmesh wireframe. The owner and the map agents author these polygons by hand, so this is not
    // an optional debug view: raised a hair off the ground, still depth-tested against the walls.
    glUniform1f(f->u_occl, 0.0f);
    if (f->nav_debug || f->show_blockers) {
        int ln = 0;
        for (int i = 0; f->show_blockers && i < f->poly_count; i++) {
            NavPoly *P = &f->polys[i];                       // every open edge in red: needs a blocker
            for (int e = 0; e < P->n && ln + 2 <= f->spr_cap; e++) {
                if (P->nb[e] >= 0) continue;
                int j = (e + 1) % P->n;
                for (int k = 0; k < 2; k++) {
                    int vi = k ? j : e;
                    FVert *o = &f->spr[ln++];
                    o->x = P->vx[vi]; o->y = P->vy[vi] + 0.30f; o->z = P->vz[vi];
                    o->u = o->v = 0.5f;
                    o->r = 255; o->g = 40; o->b = 40; o->a = 255;
                }
            }
        }
        for (int i = 0; f->nav_debug && i < f->poly_count; i++) {
            NavPoly *P = &f->polys[i];
            uint32_t hh = fhash(P->id, 17, 5);
            float cr = 0.45f + (hh & 127) / 255.0f, cg = 0.45f + ((hh >> 8) & 127) / 255.0f, cb = 0.45f + ((hh >> 16) & 127) / 255.0f;
            for (int e = 0; e < P->n && ln + 2 <= f->spr_cap; e++) {
                int j = (e + 1) % P->n;
                float open = (P->nb[e] >= 0) ? 1.0f : 0.45f;         // a wall edge draws darker
                for (int k = 0; k < 2; k++) {
                    int vi = k ? j : e;
                    FVert *o = &f->spr[ln++];
                    o->x = P->vx[vi]; o->y = P->vy[vi] + 0.06f; o->z = P->vz[vi];
                    o->u = o->v = 0.5f;
                    o->r = (unsigned char)(cr * open * 255); o->g = (unsigned char)(cg * open * 255);
                    o->b = (unsigned char)(cb * open * 255); o->a = 255;
                }
            }
        }
        if (ln) {
            glBindBuffer(GL_ARRAY_BUFFER, f->vbo_spr);
            glBufferData(GL_ARRAY_BUFFER, sizeof(FVert) * (size_t)ln, f->spr, GL_STREAM_DRAW);
            set_attribs();
            glUniform1f(f->u_alpha, 0.0f);
            glBindTexture(GL_TEXTURE_2D, f->white.id);
            glDrawArrays(GL_LINES, 0, ln);
        }
    }

    // Hand the GL state back the way ImGui's backend expects to find it.
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glViewport(0, 0, win_w, win_h);
    glDisable(GL_DEPTH_TEST);
    glDepthMask(GL_TRUE);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindVertexArray(0);
    glBindTexture(GL_TEXTURE_2D, 0);
    glActiveTexture(GL_TEXTURE0);
    glUseProgram(0);
}

// ───────────────────────── touch UI and the message box ─────────────────────────

static void draw_touch_ui(Field *f, int w, int h) {
    ImDrawList *dl = ImGui::GetBackgroundDrawList();
    float r = h * 0.14f;
    if (f->stick_on) {
        float dx = f->stick_x - f->stick_ox, dy = f->stick_y - f->stick_oy;
        float len = sqrtf(dx * dx + dy * dy);
        if (len > r) { dx *= r / len; dy *= r / len; }
        dl->AddCircleFilled(ImVec2(f->stick_ox, f->stick_oy), r, IM_COL32(255, 255, 255, 26), 32);
        dl->AddCircle(ImVec2(f->stick_ox, f->stick_oy), r, IM_COL32(255, 255, 255, 70), 32, h * 0.006f);
        dl->AddCircleFilled(ImVec2(f->stick_ox + dx, f->stick_oy + dy), r * 0.42f, IM_COL32(220, 228, 255, 110), 24);
    }
    // The action pad only shows a thin ring until it is touched, so it never sits on top of the world.
    float ax = w - h * 0.20f, ay = h * 0.74f;
    dl->AddCircle(ImVec2(ax, ay), h * 0.10f, IM_COL32(255, 255, 255, f->act_on ? 130 : 45), 28, h * 0.006f);
    if (f->act_on) dl->AddCircleFilled(ImVec2(ax, ay), h * 0.10f, IM_COL32(255, 255, 255, f->run ? 60 : 30), 28);
    if (f->run)
        dl->AddText(ImGui::GetFont(), h * 0.05f, ImVec2(ax - h * 0.05f, ay - h * 0.025f), IM_COL32(255, 230, 150, 220), "RUN");
}

// The same blue 16-bit box the cutscene player uses, drawn here so the field owns its own dialogue.
static void draw_msg_box(Field *f, int w, int h) {
    if (!f->msg[0]) return;
    ImDrawList *dl = ImGui::GetBackgroundDrawList();
    ImFont *font = ImGui::GetFont();
    float ts = h * 0.052f, u = ts * 0.12f;
    float x0 = w * 0.06f, x1 = w * 0.94f, y1 = h * 0.96f, y0 = y1 - h * 0.24f;
    dl->AddRectFilled(ImVec2(x0, y0), ImVec2(x1, y1), IM_COL32(8, 8, 20, 255), u * 1.6f);
    dl->AddRectFilledMultiColor(ImVec2(x0 + u, y0 + u), ImVec2(x1 - u, y1 - u),
        IM_COL32(24, 40, 150, 255), IM_COL32(24, 40, 150, 255), IM_COL32(8, 16, 84, 255), IM_COL32(8, 16, 84, 255));
    dl->AddRect(ImVec2(x0 + u * 0.5f, y0 + u * 0.5f), ImVec2(x1 - u * 0.5f, y1 - u * 0.5f), IM_COL32(225, 225, 235, 255), u * 1.4f, 0, u * 0.7f);
    float ty = y0 + ts * 0.7f, tw = x1 - x0 - ts * 1.5f;
    if (f->msg_who[0]) {
        dl->AddText(font, ts * 0.92f, ImVec2(x0 + ts * 0.75f, ty), IM_COL32(255, 216, 74, 255), f->msg_who);
        ty += ts * 1.35f;
    }
    dl->AddText(font, ts, ImVec2(x0 + ts * 0.75f, ty), IM_COL32_WHITE, f->msg, nullptr, tw);
    if (fmodf(f->msg_t, 1.0f) < 0.6f)
        dl->AddTriangleFilled(ImVec2(x1 - ts * 1.5f, y1 - ts * 0.95f), ImVec2(x1 - ts * 0.7f, y1 - ts * 0.95f),
                              ImVec2(x1 - ts * 1.1f, y1 - ts * 0.5f), IM_COL32_WHITE);
}

// Polygon ids, projected through the same matrix the world was drawn with.
static void draw_nav_labels(Field *f, float ox, float oy, float dw, float dh) {
    ImDrawList *dl = ImGui::GetBackgroundDrawList();
    ImFont *font = ImGui::GetFont();
    for (int i = 0; i < f->poly_count; i++) {
        NavPoly *P = &f->polys[i];
        float y = poly_height(P, P->cx, P->cz) + 0.1f;
        const float *m = f->mvp;
        float cx = m[0] * P->cx + m[4] * y + m[8] * P->cz + m[12];
        float cy = m[1] * P->cx + m[5] * y + m[9] * P->cz + m[13];
        float cw = m[3] * P->cx + m[7] * y + m[11] * P->cz + m[15];
        if (cw < 0.1f) continue;
        float sx = ox + (cx / cw * 0.5f + 0.5f) * dw, sy = oy + (0.5f - cy / cw * 0.5f) * dh;
        char lbl[16];
        snprintf(lbl, sizeof(lbl), "%d", P->id);
        dl->AddText(font, dh * 0.045f, ImVec2(sx, sy), IM_COL32(255, 255, 160, 220), lbl);
    }
}

// ───────────────────────── public ─────────────────────────

Field *field_create() {
    Field *f = (Field *)calloc(1, sizeof(Field));
    f->fill_width = 1;
    f->see_on = 1;
    f->see_radius = 46.0f;               // FBO pixels at 360 high; a Dev slider tunes it
    f->walk_radius = WALK_RADIUS;
    f->zone_idx = -1;
    f->poly = -1;
    f->fov = 0.0f;                                  // 0 = "camera not placed yet", snapped on frame 1
    snprintf(f->map_name, sizeof(f->map_name), "halm");
    return f;
}

void field_destroy(Field *f) {
    if (!f) return;
    field_free_map(f);
    free(f->spr);
    free(f);
}

bool field_take_warning(Field *f, char *out, int cap) {
    if (!f->warn_pending) return false;
    snprintf(out, cap, "%s", f->warn);
    f->warn_pending = false;
    return true;
}

void field_message(Field *f, const char *text) {
    snprintf(f->msg, sizeof(f->msg), "%s", text ? text : "");
    f->msg_who[0] = 0;
    f->msg_t = 0;
}

// Prose lives in story/field/text.md and reaches the game through the generated src/field_text.h.
// A map only ever names an id; an id with no entry shows as "[the.id]", which is the point.
static void field_say(Field *f, const char *who, const char *id) {
    const char *text = nullptr;
    for (int i = 0; i < FIELD_TEXT_COUNT; i++) if (!strcmp(FIELD_TEXT[i].id, id)) { text = FIELD_TEXT[i].text; break; }
    if (text) snprintf(f->msg, sizeof(f->msg), "%s", text);
    else snprintf(f->msg, sizeof(f->msg), "[%s]", id);
    snprintf(f->msg_who, sizeof(f->msg_who), "%s", who ? who : "");
    f->msg_t = 0;
}

static void fire(FieldEvent *ev, int kind, const char *arg) {
    if (ev->kind != FE_NONE) return;
    ev->kind = kind;
    snprintf(ev->arg, sizeof(ev->arg), "%s", arg);
}

void field_tick(Field *f, int w, int h, float dt, bool ui_blocked, FieldEvent *ev) {
    ev->kind = FE_NONE; ev->arg[0] = 0;
    if (!f->gl_ready) field_gl_init(f);
    if (!f->tile_count) field_load_map(f, f->map_name);
    f->water_t += dt;
    f->msg_t += dt;

    field_input(f, w, h, ui_blocked, dt);

    // Movement. Screen-space stick mapped through the camera basis, then resolved on the navmesh.
    float sx = 0, sy = 0, head_x = 0, head_z = 0;
    bool moving = f->msg[0] ? false : stick_dir(f, w, &sx, &sy);
    float fx, fz, rx, rz;
    cam_basis(f, &fx, &fz, &rx, &rz);
    if (moving) {
        float dx = rx * sx + fx * (-sy), dz = rz * sx + fz * (-sy);
        float l = sqrtf(dx * dx + dz * dz);
        if (l > 1e-4f) { dx /= l; dz /= l; }
        float sp = WALK_SPEED * (f->run ? RUN_MULT : 1.0f) * dt;
        int np = mesh_move(f, f->poly, &f->px, &f->pz, dx * sp, dz * sp);
        if (np >= 0) f->poly = np;
        f->poly = wall_buffer(f, f->poly, &f->px, &f->pz, f->walk_radius);
        f->facing = (fabsf(dx) > fabsf(dz)) ? (dx > 0 ? 2 : 1) : (dz > 0 ? 0 : 3);
        f->anim_t += dt * (f->run ? RUN_MULT : 1.0f);
        head_x = dx; head_z = dz;
    }
    f->walking = moving;
    float kh = fminf(dt * 4.0f, 1.0f);                     // the focus lead eases, it never snaps
    f->move_dx += (head_x - f->move_dx) * kh;
    f->move_dz += (head_z - f->move_dz) * kh;

    // NPCs are soft bodies: push out, then let the mesh re-resolve the pushed position.
    for (int i = 0; i < f->npc_count; i++) {
        float dx = f->px - (f->npcs[i].x + 0.5f), dz = f->pz - (f->npcs[i].y + 0.5f);
        float d = sqrtf(dx * dx + dz * dz);
        if (d >= NPC_RADIUS || d < 1e-4f) continue;
        float push = NPC_RADIUS - d;
        int np = mesh_move(f, f->poly, &f->px, &f->pz, dx / d * push, dz / d * push);
        if (np >= 0) f->poly = np;
        f->poly = wall_buffer(f, f->poly, &f->px, &f->pz, f->walk_radius);
    }
    if (f->poly >= 0 && f->poly < f->poly_count) f->py = poly_height(&f->polys[f->poly], f->px, f->pz);

    int vw = f->fill_width ? (int)(FBO_H * (float)w / (float)h + 0.5f) : 640;
    if (vw < 640) vw = 640;
    if (vw > FBO_W) vw = FBO_W;
    cam_update(f, dt, (float)vw / (float)FBO_H);
    // See-through: open the dithered circle only while something really stands between the camera
    // and the player, and fade the radius so it never pops.
    float see_target = (f->see_on && player_occluded(f)) ? f->see_radius : 0.0f;
    f->see_r += (see_target - f->see_r) * (1.0f - expf(-dt / 0.09f));
    if (f->see_r < 0.4f) f->see_r = 0.0f;

    // Only transitions fire on entry: scenes, encounter zones and traps. Anything you *read* waits
    // for the interact press, and announces itself with a "!" over the player's head instead.
    f->exam_trig = f->exam_npc = -1;
    float best = 1e9f;
    const float fwx[4] = {0, -1, 1, 0}, fwz[4] = {1, 0, 0, -1};
    for (int i = 0; i < f->trig_count; i++) {
        FTrig *t = &f->trigs[i];
        bool in = f->px >= t->x && f->pz >= t->y && f->px < t->x + t->w && f->pz < t->y + t->h;
        if (t->kind == TG_SCENE || t->kind == TG_ZONE || t->kind == TG_TRAP) {
            if (in && !t->inside) {
                if (t->kind == TG_SCENE) fire(ev, FE_SCENE, t->arg);
                else if (t->kind == TG_ZONE) fire(ev, FE_ZONE, t->arg);
                else field_say(f, nullptr, t->arg);                  // trap: on entry, for later
            }
            t->inside = in;
            continue;
        }
        t->inside = in;
        float cx = f->px < t->x ? t->x : (f->px > t->x + t->w ? t->x + t->w : f->px);
        float cz = f->pz < t->y ? t->y : (f->pz > t->y + t->h ? t->y + t->h : f->pz);
        float dx = cx - f->px, dz = cz - f->pz, d = sqrtf(dx * dx + dz * dz);
        if (!in) {                                                   // just outside: must be facing it
            if (d > 0.6f) continue;
            if (d > 1e-4f && (dx / d) * fwx[f->facing & 3] + (dz / d) * fwz[f->facing & 3] < 0.25f) continue;
        }
        if (d < best) { best = d; f->exam_trig = i; }
    }
    for (int i = 0; i < f->npc_count; i++) {
        float ex = f->npcs[i].x + 0.5f - f->px, ez = f->npcs[i].y + 0.5f - f->pz;
        float d = sqrtf(ex * ex + ez * ez);
        if (d > 1.8f || d < 1e-3f) continue;
        if ((ex / d) * fwx[f->facing & 3] + (ez / d) * fwz[f->facing & 3] < 0.25f) continue;
        if (d < best) { best = d; f->exam_npc = i; f->exam_trig = -1; }
    }
    for (int i = 0; i < f->exit_count; i++) {
        FExit *e = &f->exits[i];
        bool in = f->px >= e->x && f->pz >= e->y && f->px < e->x + e->w && f->pz < e->y + e->h;
        if (!in || e->inside) { e->inside = in; continue; }
        char to[32];
        snprintf(to, sizeof(to), "%s", e->map);
        short sx2 = e->sx, sy2 = e->sy;
        int fc = e->facing;
        field_load_map(f, to);
        field_place(f, sx2 + 0.5f, sy2 + 0.5f, fc);
        break;
    }

    // The action button: dismiss the box, else talk to whoever is in front.
    if (f->tapped) {
        if (f->msg[0]) { f->msg[0] = 0; f->msg_who[0] = 0; }
        else if (f->exam_npc >= 0) {
            FNpc *np = &f->npcs[f->exam_npc];
            if (np->scene[0]) fire(ev, FE_SCENE, np->scene);
            else field_say(f, np->name, np->say[0] ? np->say : "missing.line");
        } else if (f->exam_trig >= 0) {
            field_say(f, nullptr, f->trigs[f->exam_trig].arg);
        }
    }

    // Render, then hand the FBO to ImGui as one nearest-filtered full-screen image.
    field_render(f, w, h, vw, FBO_H);

    float sc = fminf((float)w / vw, (float)h / FBO_H);
    float dw = vw * sc, dh = FBO_H * sc, ox = (w - dw) * 0.5f, oy = (h - dh) * 0.5f;
    ImDrawList *dl = ImGui::GetBackgroundDrawList();
    dl->AddRectFilled(ImVec2(0, 0), ImVec2((float)w, (float)h), IM_COL32(0, 0, 0, 255));
    dl->AddImage((ImTextureID)(intptr_t)f->fbo_tex, ImVec2(ox, oy), ImVec2(ox + dw, oy + dh),
                 ImVec2(0.0f, 1.0f), ImVec2((float)vw / FBO_W, 0.0f));   // GL's origin is bottom-left
    if (f->nav_debug) draw_nav_labels(f, ox, oy, dw, dh);
    draw_touch_ui(f, w, h);
    draw_msg_box(f, w, h);
}

bool field_dev_ui(Field *f, char *out, int cap) {
    bool printed = false;
    CamZone *z = (f->zone_idx >= 0 && f->zone_idx < f->zone_count) ? &f->zones[f->zone_idx] : nullptr;
    ImGui::Text("map %s  x %.2f z %.2f  poly %d  zone %d  facing %s", f->map_name, f->px, f->pz,
                (f->poly >= 0 && f->poly < f->poly_count) ? f->polys[f->poly].id : -1, f->zone_idx,
                f->facing == 0 ? "S" : f->facing == 1 ? "W" : f->facing == 2 ? "E" : "N");
    for (int i = 0; i < field_map_count(); i++) {
        if (i) ImGui::SameLine();
        bool cur = !strcmp(f->map_name, field_map_name_at(i));
        if (cur) ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.2f, 0.55f, 0.3f, 1));
        if (ImGui::Button(field_map_name_at(i))) field_load_map(f, field_map_name_at(i));
        if (cur) ImGui::PopStyleColor();
    }
    if (f->landmark[0]) ImGui::TextDisabled("landmark: %s", f->landmark);
    ImGui::SameLine();
    if (ImGui::Button("Respawn")) field_load_map(f, f->map_name);
    ImGui::SameLine();
    bool nav = f->nav_debug != 0;
    if (ImGui::Checkbox("Nav", &nav)) f->nav_debug = nav ? 1 : 0;
    ImGui::SameLine();
    bool blk = f->show_blockers != 0;
    if (ImGui::Checkbox("Blockers", &blk)) f->show_blockers = blk ? 1 : 0;
    ImGui::SameLine();
    bool fill = f->fill_width != 0;
    if (ImGui::Checkbox("fill width", &fill)) f->fill_width = fill ? 1 : 0;

    if (!z) return false;
    const char *modes[3] = {"follow", "fixed", "rail"};
    ImGui::Text("zone %d mode %s", f->zone_idx, modes[z->mode < 0 || z->mode > 2 ? 0 : z->mode]);
    ImGui::SameLine();
    if (ImGui::Button("print zone")) {
        if (z->mode == CAM_FIXED)
            snprintf(out, cap, "%.0f %.0f %.0f %.0f fixed %.2f %.2f %.2f %.2f %.2f %.2f %.1f%s",
                     z->x, z->z, z->w, z->d, z->cx, z->cy, z->cz, z->tx, z->ty, z->tz, z->fov, z->pan ? " pan" : "");
        else if (z->mode == CAM_RAIL)
            snprintf(out, cap, "%.0f %.0f %.0f %.0f rail %.2f %.2f %.2f %.2f %.2f %.2f %.2f %.2f %.2f %.1f",
                     z->x, z->z, z->w, z->d, z->ax, z->ay, z->az, z->bx, z->by, z->bz, z->tx, z->ty, z->tz, z->fov);
        else
            snprintf(out, cap, "%.0f %.0f %.0f %.0f follow %.1f %.1f %.1f %.2f %.2f",
                     z->x, z->z, z->w, z->d, z->yaw, z->pitch, z->fov, z->dist, z->height);
        printed = true;
    }
    if (z->mode == CAM_FOLLOW) {
        ImGui::SliderFloat("yaw", &z->yaw, -180.0f, 180.0f, "%.0f deg");
        ImGui::SliderFloat("pitch", &z->pitch, 5.0f, 85.0f, "%.0f deg");
        ImGui::SliderFloat("distance", &z->dist, 2.0f, 40.0f, "%.1f");
        ImGui::SliderFloat("height", &z->height, -3.0f, 8.0f, "%.2f");
    } else if (z->mode == CAM_FIXED) {
        ImGui::SliderFloat("eye x", &z->cx, -10.0f, 70.0f, "%.2f");
        ImGui::SliderFloat("eye y", &z->cy, 0.0f, 40.0f, "%.2f");
        ImGui::SliderFloat("eye z", &z->cz, -10.0f, 70.0f, "%.2f");
        ImGui::SliderFloat("at x", &z->tx, -10.0f, 70.0f, "%.2f");
        ImGui::SliderFloat("at y", &z->ty, -2.0f, 20.0f, "%.2f");
        ImGui::SliderFloat("at z", &z->tz, -10.0f, 70.0f, "%.2f");
        bool pan = z->pan != 0;
        if (ImGui::Checkbox("pan (hold the player)", &pan)) z->pan = pan ? 1 : 0;
    } else {
        ImGui::SliderFloat("A x", &z->ax, -10.0f, 70.0f, "%.2f");
        ImGui::SliderFloat("A y", &z->ay, 0.0f, 40.0f, "%.2f");
        ImGui::SliderFloat("A z", &z->az, -10.0f, 70.0f, "%.2f");
        ImGui::SliderFloat("B x", &z->bx, -10.0f, 70.0f, "%.2f");
        ImGui::SliderFloat("B y", &z->by, 0.0f, 40.0f, "%.2f");
        ImGui::SliderFloat("B z", &z->bz, -10.0f, 70.0f, "%.2f");
        ImGui::SliderFloat("at x", &z->tx, -10.0f, 70.0f, "%.2f");
        ImGui::SliderFloat("at y", &z->ty, -2.0f, 20.0f, "%.2f");
        ImGui::SliderFloat("at z", &z->tz, -10.0f, 70.0f, "%.2f");
    }
    ImGui::SliderFloat("fov", &z->fov, 8.0f, 75.0f, "%.0f deg");
    bool see = f->see_on != 0;
    if (ImGui::Checkbox("see-through occluders", &see)) f->see_on = see ? 1 : 0;
    ImGui::SameLine();
    ImGui::SliderFloat("radius px", &f->see_radius, 12.0f, 140.0f, "%.0f");
    ImGui::SliderFloat("walk radius", &f->walk_radius, 0.0f, 0.8f, "%.2f cells");
    ImGui::Text("ground splat");
    ImGui::SameLine();
    ImGui::SetNextItemWidth(140);
    ImGui::SliderFloat("dither", &f->splat_dither, 0.0f, 0.5f, "%.2f");
    for (int i = 0; i < 4; i++) {
        char a[24], b[24];
        const char *nm = f->tiles[f->layer_tile[i]].id;
        snprintf(a, sizeof(a), "%s hard", nm);
        snprintf(b, sizeof(b), "%s height", nm);
        ImGui::SetNextItemWidth(220);
        ImGui::SliderFloat(a, &f->layer_sharp[i], 1.0f, 16.0f, "%.1f");
        ImGui::SameLine();
        ImGui::SetNextItemWidth(220);
        ImGui::SliderFloat(b, &f->layer_hk[i], 0.0f, 1.2f, "%.2f");
    }
    return printed;
}

void field_save(Field *f, FieldSave *s) {
    memset(s, 0, sizeof(*s));
    snprintf(s->map, sizeof(s->map), "%s", f->map_name);
    s->x = f->px; s->z = f->pz;
    s->poly = (f->poly >= 0 && f->poly < f->poly_count) ? f->polys[f->poly].id : -1;
    s->facing = f->facing;
    s->zone_idx = f->zone_idx;
    s->nav_debug = f->nav_debug;
    s->fill_width = f->fill_width;
    s->show_blockers = f->show_blockers;
    s->see_on = f->see_on;
    s->see_radius = f->see_radius;
    s->walk_radius = f->walk_radius;
    if (f->zone_idx >= 0 && f->zone_idx < f->zone_count) s->zone = f->zones[f->zone_idx];
}

static float clampf(float v, float lo, float hi, float def) {
    if (!(v == v) || v < lo || v > hi) return def;                   // NaN-safe: a bad blob never sticks
    return v;
}

void field_restore(Field *f, const FieldSave *s) {
    char name[32];
    int i = 0;
    for (; i < 31 && s->map[i]; i++) {                               // only ever a plain file-name word
        char c = s->map[i];
        if (!((c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '_')) break;
        name[i] = c;
    }
    name[i] = 0;
    field_load_map(f, name[0] ? name : "halm");
    f->nav_debug = s->nav_debug ? 1 : 0;
    f->fill_width = s->fill_width ? 1 : 0;
    f->show_blockers = s->show_blockers ? 1 : 0;
    f->see_on = s->see_on ? 1 : 0;
    f->see_radius = clampf(s->see_radius, 4.0f, 400.0f, 46.0f);
    f->walk_radius = clampf(s->walk_radius, 0.0f, 1.2f, WALK_RADIUS);

    float x = clampf(s->x, -1.0f, (float)f->mw + 1.0f, -1e9f), z = clampf(s->z, -1.0f, (float)f->mh + 1.0f, -1e9f);
    if (x > -1e8f && z > -1e8f) {
        int p = mesh_find(f, x, z);                                  // the mesh may have changed under us
        if (p < 0 && s->poly >= 0)
            for (int j = 0; j < f->poly_count; j++) if (f->polys[j].id == s->poly) { p = j; break; }
        if (p >= 0) {
            f->px = x; f->pz = z;
            f->poly = poly_contains(&f->polys[p], x, z, 1e-3f) ? p : mesh_find(f, x, z);
            if (f->poly < 0) { f->px = f->polys[p].cx; f->pz = f->polys[p].cz; f->poly = p; }
            f->py = poly_height(&f->polys[f->poly], f->px, f->pz);
        }
    }
    if (s->facing >= 0 && s->facing < 4) f->facing = s->facing;

    // Restore the zone the owner was tuning, but only onto a zone of the same mode: the map file may
    // have been edited between reloads and a half-matched shot is worse than the authored one.
    if (s->zone_idx >= 0 && s->zone_idx < f->zone_count && f->zones[s->zone_idx].mode == s->zone.mode) {
        CamZone z = s->zone;
        z.fov = clampf(z.fov, 5.0f, 100.0f, 30.0f);
        z.pitch = clampf(z.pitch, 1.0f, 89.0f, 50.0f);
        z.dist = clampf(z.dist, 0.5f, 80.0f, 13.0f);
        z.yaw = clampf(z.yaw, -360.0f, 360.0f, 0.0f);
        z.height = clampf(z.height, -20.0f, 40.0f, 0.8f);
        z.cx = clampf(z.cx, -500.0f, 500.0f, 0.0f); z.cy = clampf(z.cy, -500.0f, 500.0f, 10.0f); z.cz = clampf(z.cz, -500.0f, 500.0f, 0.0f);
        z.tx = clampf(z.tx, -500.0f, 500.0f, 0.0f); z.ty = clampf(z.ty, -500.0f, 500.0f, 0.0f); z.tz = clampf(z.tz, -500.0f, 500.0f, 0.0f);
        z.ax = clampf(z.ax, -500.0f, 500.0f, 0.0f); z.ay = clampf(z.ay, -500.0f, 500.0f, 10.0f); z.az = clampf(z.az, -500.0f, 500.0f, 0.0f);
        z.bx = clampf(z.bx, -500.0f, 500.0f, 0.0f); z.by = clampf(z.by, -500.0f, 500.0f, 10.0f); z.bz = clampf(z.bz, -500.0f, 500.0f, 0.0f);
        f->zones[s->zone_idx] = z;
    }
    f->zone_idx = -1;                                                // recomputed on the next tick
    f->fov = 0.0f;                                                   // and snapped, not blended in
}
