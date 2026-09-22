// vox_actors.cpp — everything that is a sprite rather than a block.
// OWNS: the walker sheets and the tileset atlas, story/field/sprites/<id>.png and its sidecar, the
//       scattered detail billboards, the NPCs' wander, and the per-frame collection of every
//       billboard (party, followers, NPCs, props, blob shadows, lamp glows) into VoxField::spr.
// NEVER: issues a draw call — vox_render.cpp does that — and never decides where the leader goes.
// EXPOSES: vx_art_get, vx_sprart_get, vx_load_atlas, vx_load_decals, vx_bind_sprites,
//          vx_scatter_detail, npc_step, vx_collect_sprites, vx_light_at_foot.
// Tested by: the three image captures and --vox-walktest (NPCs are walked into). See src/notes/world.md.
#include "vox_internal.h"

// ───────────────────────── the walker sheets and the atlas ─────────────────────────

int vx_art_get(VoxField *v, const char *id) {
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

// ───────────────────────── field sprites ─────────────────────────
// story/field/sprites/<id>.png — the contract in VOXFIELD_NOTES.md, "Field sprites". Indexed on the
// master palette, index 0 transparent, anchored BOTTOM-CENTRE, 64 px to the map cell, an upright
// camera-facing billboard. NOT a walker sheet: there are no direction rows. A still by default; the
// optional sidecar <id>.json is the only thing that may say otherwise, and its `frame` is READ.
//
// Loading is by file-exists and is CACHED PER ID INCLUDING THE MISS, so a missing PNG costs one
// failed read a map load and then draws a placeholder — the chapter is playable before any art
// exists, and picks the real art up the next time the map loads after the file appears.
static int vx_sprart_get(VoxField *v, const char *id) {
    if (!id || !id[0]) return -1;
    for (int i = 0; i < v->sprart_count; i++) if (!strcmp(v->sprart[i].id, id)) return i;
    if (v->sprart_count >= VX_SPRART) return -1;
    int k = v->sprart_count++;
    VxSprArt *a = &v->sprart[k];
    memset(a, 0, sizeof(*a));
    snprintf(a->id, sizeof(a->id), "%s", id);
    a->frames = 1; a->fps = 3; a->cw = 1.0f; a->ch = 1.5f;

    char rel[192];
    snprintf(rel, sizeof(rel), "field/sprites/%s.png", id);
    size_t sz = 0;
    void *data = vx_read(rel, &sz);
    if (data) {
        int w = 0, h = 0, c = 0;
        unsigned char *px = stbi_load_from_memory((const unsigned char *)data, (int)sz, &w, &h, &c, 4);
        SDL_free(data);
        if (px) {
            char label[64];
            snprintf(label, sizeof(label), "sprite %s", id);
            a->tex = vx_tex_indexed(v, label, px, w, h);
            a->w = w; a->h = h; a->fw = w; a->fh = h;
            stbi_image_free(px);
        }
    }
    // the sidecar, if there is one
    char jrel[192];
    snprintf(jrel, sizeof(jrel), "field/sprites/%s.json", id);
    size_t jsz = 0;
    char *js = (char *)vx_read(jrel, &jsz);
    if (js) {
        const char *k2;
        int n = 0;
        if ((k2 = strstr(js, "\"frames\"")) && sscanf(k2 + 8, " : %d", &n) == 1 && n > 0) a->frames = n;
        if ((k2 = strstr(js, "\"fps\"")) && sscanf(k2 + 5, " : %d", &n) == 1 && n > 0) a->fps = n;
        if ((k2 = strstr(js, "\"loop\"")) && strstr(k2, "pingpong") && strstr(k2, "pingpong") < k2 + 24) a->pingpong = 1;
        int fw = 0, fh = 0;
        if ((k2 = strstr(js, "\"frame\"")) && strchr(k2, '[')) sscanf(strchr(k2, '[') + 1, "%d , %d", &fw, &fh);
        if (fw > 0 && fh > 0) { a->fw = fw; a->fh = fh; }          // READ, never divided
        float cw = 0, ch = 0;
        if ((k2 = strstr(js, "\"footprint\"")) && strchr(k2, '[')) sscanf(strchr(k2, '[') + 1, "%f , %f", &cw, &ch);
        if (cw > 0 && ch > 0) { a->cw = cw; a->ch = ch; }
        SDL_free(js);
    }
    if (a->tex) {
        if (a->fw <= 0 || a->fw > a->w) a->fw = a->w;
        if (a->fh <= 0 || a->fh > a->h) a->fh = a->h;
        if (a->frames < 1) a->frames = 1;
        int percol = a->fw > 0 ? a->w / a->fw : 1;
        if (percol < 1) percol = 1;
        if (a->frames > percol * (a->fh > 0 ? a->h / a->fh : 1)) a->frames = percol;
        // 64 px to the map cell, unless the sidecar gave an explicit footprint
        if (a->cw == 1.0f && a->ch == 1.5f) { a->cw = a->fw / 64.0f; a->ch = a->fh / 64.0f; }
        SDL_Log("voxfield: field sprite \"%s\" %dx%d, frame %dx%d, %d frame%s, %.2fx%.2f cells",
                id, a->w, a->h, a->fw, a->fh, a->frames, a->frames == 1 ? "" : "s", a->cw, a->ch);
    } else {
        SDL_Log("voxfield: no story/field/sprites/%s.png — drawing a placeholder", id);
    }
    return k;
}

// Every trigger that stands something on the map resolves its art once, at load. A `fight` draws its
// encounter's sprite where the encounter is; a `sprite` line draws whatever it names.
void vx_bind_sprites(VoxField *v) {
    for (int i = 0; i < v->trig_count; i++) {
        VxTrig *g = &v->trigs[i];
        g->sprart = (g->kind == TG_FIGHT || g->kind == TG_SPRITE) ? (short)vx_sprart_get(v, g->arg) : (short)-1;
    }
}

void vx_load_atlas(VoxField *v) {
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

void vx_load_decals(VoxField *v) {
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
void vx_scatter_detail(VoxField *v) {
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

// ── NPCs ──
// They wander freely inside their home radius: pick a nearby target, walk straight at it, and give
// up on it if the straight line leaves the region. They push the player out of the way SOFTLY —
// both bodies are moved, the NPC taking the larger share, so a wedged player is never trapped.

void npc_step(VoxField *v, float dt) {
    const float SP = 1.5f;
    for (int i = 0; i < v->npc_count; i++) {
        VxNpc *np = &v->npcs[i];
        np->py_ = np->y;
        if (!np->wander) { np->y = vx_nav_y(v, np->x, np->z); continue; }
        float dx = np->gx - np->x, dz = np->gz - np->z;
        float d = sqrtf(dx * dx + dz * dz);
        if (d < 0.12f) {
            np->wait -= dt;
            if (np->wait <= 0) {
                np->wait = 1.0f + vx_rnd((int)(v->anim_t * 60) + i, i, 17) * 2.0f;
                float ang = vx_rnd((int)(v->anim_t * 97) + i, 5, 3) * 6.2831853f;
                float rad = 0.6f + vx_rnd(i, (int)(v->anim_t * 31), 9) * (float)np->wander;
                float tx = np->hx + 0.5f + cosf(ang) * rad, tz = np->hz + 0.5f + sinf(ang) * rad;
                int gcx = (int)floorf(tx), gcz = (int)floorf(tz);
                bool in = gcx >= 0 && gcz >= 0 && gcx < v->mw && gcz < v->md;
                if (in && vx_nav_at(v, tx, tz) && !v->tsolid[gcz][gcx]) {
                    // only take it if the straight line stays inside the region
                    bool ok = true;
                    int n = (int)(rad / (VOX_S * 0.5f)) + 1;
                    for (int s = 1; s <= n && ok; s++)
                        ok = vx_nav_at(v, np->x + (tx - np->x) * s / n, np->z + (tz - np->z) * s / n);
                    if (ok && trig_at(v, gcx, gcz, TGM(TG_EXIT) | TGM(TG_DOOR)) < 0) { np->gx = tx; np->gz = tz; }
                }
            }
            np->phase = 0;
            np->y = vx_nav_y(v, np->x, np->z);
            continue;
        }
        float step = SP * dt;
        if (step > d) step = d;
        float feet = vx_nav_y(v, np->x, np->z);
        float bx = np->x, bz = np->z;
        vx_move_slide(v, &np->x, &np->z, dx / d * step, dz / d * step, feet, false, VX_NPC_R);
        // walked into something: give up on this goal rather than grinding along a wall
        if ((np->x - bx) * (np->x - bx) + (np->z - bz) * (np->z - bz) < step * step * 0.09f) {
            np->gx = np->x; np->gz = np->z; np->wait = 0.4f;
        }
        np->facing = vx_face_pick(np->facing, dx, dz);
        np->phase += SP * 1.35f * dt;
        np->y = vx_nav_y(v, np->x, np->z);
        np->tx = (short)floorf(np->x); np->tz = (short)floorf(np->z);
        if (np->x < 0.01f || np->z < 0.01f) { np->gx = np->x; np->gz = np->z; }
    }
    // soft separation: the player out of every NPC, the NPC out of the player
    VxActor *a = &v->act[0];
    if (v->airborne) return;
    for (int i = 0; i < v->npc_count; i++) {
        VxNpc *np = &v->npcs[i];
        float dx = a->x - np->x, dz = a->z - np->z;
        float d2 = dx * dx + dz * dz, R = v->agent_r + VX_NPC_R;
        if (d2 >= R * R) continue;
        float d = sqrtf(d2);
        if (d < 1e-4f) { dx = 0.01f; dz = 0; d = 0.01f; }
        float pen = R - d, ux = dx / d, uz = dz / d;
        float feet = a->y;
        // the NPC takes the larger share, which is what "steps aside" means in practice
        vx_move_slide(v, &np->x, &np->z, -ux * pen * 0.65f, -uz * pen * 0.65f, np->y, false, VX_NPC_R);
        np->gx = np->x; np->gz = np->z;
        np->tx = (short)floorf(np->x); np->tz = (short)floorf(np->z);
        vx_move_slide(v, &a->x, &a->z, ux * pen * 0.35f, uz * pen * 0.35f, feet, false, v->agent_r);
    }
}

// ───────────────────────── billboards ─────────────────────────

// The walk cycle is driven by a PHASE that advances with the body's actual speed, so a creep on the
// touch stick ambles and a run strides, and a body pressed into a wall stops moving its legs.
static int walk_col_phase(const VxArt *a, float phase, bool moving) {
    if (!moving) return a->col_stand;
    float f = phase - floorf(phase);
    int q = (int)(f * 4.0f) & 3;
    if (a->ncols >= 4) { static const int S4[4] = { 1, 0, 2, 3 }; return S4[q]; }
    const int S3[4] = { a->col_a, a->col_stand, a->col_b, a->col_stand };
    return S3[q];
}

static void spr_push(VoxField *v, const VxSpr *s) {
    if (v->spr_count < VX_SPRITES) v->spr[v->spr_count++] = *s;
}

// The ONE dynamic point light, on top of whatever the .tmap baked. Same falloff as vx_lamp_at, so
// the moving lantern and a static one in a wall bracket read as the same kind of light.
static float vx_dyn_lamp_at(VoxField *v, float x, float y, float z) {
    if (!v->dlamp_on || v->dlamp_level <= 0.0f) return 0.0f;
    float dx = x - v->dlamp_x, dy = y - v->dlamp_y, dz = z - v->dlamp_z;
    float d = sqrtf(dx * dx + dy * dy + dz * dz);
    float r = v->dlamp_r > 0.001f ? v->dlamp_r : 0.001f;
    float f = 1.0f - d / r;
    if (f <= 0) return 0.0f;
    f = f * f * (3.0f - 2.0f * f);                    // the same smoothstep the shader uses
    float lit = v->dlamp_level * f;
    return lit > VX_LAMP_CAP ? VX_LAMP_CAP : lit;
}

static void vx_light_at_foot(VoxField *v, float x, float y, float z, float *lit, float *warm) {
    float lamp = 0;
    vx_lamp_at(v, x, y + 0.8f, z, &lamp);
    lamp += vx_dyn_lamp_at(v, x, y + 0.8f, z);
    if (lamp > 1) lamp = 1;
    *lit = 0.92f;                      // a body is lit by the sky, not by the face it stands on
    *warm = lamp;
}

// `gy` is the ground under the character, which is where the blob shadow goes. With jumping in the
// game the blob is what makes height readable: it stays on the ground, shrinks and fades as the
// body rises. Pass y for gy when the body is on the ground and the two are the same.
static void push_walker(VoxField *v, int art, float x, float y, float z, int facing, int col, float gy) {
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
    float air = y - gy;
    if (air < 0) air = 0;
    float f = 1.0f / (1.0f + air * 0.55f);               // shrinks and fades with height
    sh.x = x; sh.y = gy + 0.02f; sh.z = z;
    sh.w = s.w * 0.85f * f; sh.h = s.w * 0.62f * f;
    sh.kind = 1; sh.alpha = 0.42f * f; sh.tilt = 0;
    spr_push(v, &sh);
}

void vx_collect_sprites(VoxField *v) {
    v->spr_count = 0;
    for (int i = v->party - 1; i >= 0; i--) {
        VxActor *a = &v->act[i];
        int art = v->art_party[i];
        bool mv = a->phase != 0.0f;
        int col = art >= 0 ? walk_col_phase(&v->art[art], a->phase, mv) : 0;
        push_walker(v, art, a->x, a->y, a->z, a->facing, col, vx_nav_y(v, a->x, a->z));
    }
    for (int i = 0; i < v->npc_count; i++) {
        VxNpc *np = &v->npcs[i];
        bool mv = np->phase != 0.0f;
        int col = np->art >= 0 ? walk_col_phase(&v->art[np->art], np->phase, mv) : 0;
        push_walker(v, np->art, np->x, np->y, np->z, np->facing, col, np->y);
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
    // Field sprites: everything a `fight` or a `sprite` trigger stands on the map. The art is drawn
    // bottom-centre on the trigger rectangle's centre cell, upright and camera-facing.
    for (int i = 0; i < v->trig_count; i++) {
        VxTrig *g = &v->trigs[i];
        if (g->sprart < 0 || !trig_live(v, g)) continue;
        VxSprArt *a = &v->sprart[g->sprart];
        float fx = g->x + g->w * 0.5f, fz = g->z + g->d * 0.5f;
        int hx = g->x + g->w / 2, hz = g->z + g->d / 2;
        if (hx >= v->mw) hx = v->mw - 1;
        if (hz >= v->md) hz = v->md - 1;
        float gy = vx_gy(v, hx < 0 ? 0 : hx, hz < 0 ? 0 : hz);
        float lit = 0, warm = 0;
        vx_light_at_foot(v, fx, gy, fz, &lit, &warm);
        if (a->tex) {
            int frame = 0;
            if (a->frames > 1) {
                int t = (int)(v->anim_t * (float)(a->fps > 0 ? a->fps : 3));
                if (a->pingpong && a->frames > 1) {
                    int span = a->frames * 2 - 2;
                    int q = ((t % span) + span) % span;
                    frame = q < a->frames ? q : span - q;
                } else frame = ((t % a->frames) + a->frames) % a->frames;
            }
            int percol = a->fw > 0 ? a->w / a->fw : 1;
            if (percol < 1) percol = 1;
            int cxi = frame % percol, czi = frame / percol;
            VxSpr sp;
            memset(&sp, 0, sizeof(sp));
            sp.x = fx; sp.z = fz; sp.y = gy;
            sp.w = a->cw; sp.h = a->ch;
            sp.u0 = (float)(cxi * a->fw) / (float)a->w;
            sp.u1 = (float)((cxi + 1) * a->fw) / (float)a->w;
            sp.v0 = (float)(czi * a->fh) / (float)a->h;
            sp.v1 = (float)((czi + 1) * a->fh) / (float)a->h;
            sp.tex = a->tex; sp.kind = 0; sp.alpha = 1; sp.tilt = 1;
            sp.lit = lit; sp.warm = warm;
            spr_push(v, &sp);
        } else {
            // No art yet: a flat palette-ramp silhouette of the right footprint, five stacked bands
            // tapering to the top so it reads as a thing standing there and never as finished art.
            int r = vx_ramp_by_name(v, "neutral warm (plaster, cloth)");
            if (r < 0) r = 0;
            static const float TAPER[5] = { 1.00f, 0.94f, 0.84f, 0.68f, 0.46f };
            for (int b = 0; b < 5; b++) {
                int step = r < v->ramp_count && v->ramp_len[r] > 0
                         ? v->ramp_idx[r][(b * v->ramp_len[r]) / 5] : 1;
                VxSpr sp;
                memset(&sp, 0, sizeof(sp));
                sp.x = fx; sp.z = fz; sp.y = gy + a->ch * (b / 5.0f);
                sp.w = a->cw * TAPER[b]; sp.h = a->ch / 5.0f;
                sp.kind = 3; sp.alpha = 1; sp.tilt = 1;
                sp.lit = (float)step / 255.0f;               // kind 3 reads lit as a palette INDEX
                sp.warm = warm;
                spr_push(v, &sp);
            }
        }
    }
    // The moving lantern's own glow, so the pool is visible and not merely felt.
    if (v->dlamp_on && v->dlamp_level > 0.0f && v->amb + v->ns_add < 0.92f) {
        VxSpr s;
        memset(&s, 0, sizeof(s));
        s.x = v->dlamp_x; s.z = v->dlamp_z; s.y = v->dlamp_y - 0.9f;
        s.w = s.h = v->dlamp_r * 0.9f;
        s.kind = 2; s.alpha = v->dlamp_level * (1.0f - (v->amb + v->ns_add)) * 0.85f; s.tilt = 1;
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

