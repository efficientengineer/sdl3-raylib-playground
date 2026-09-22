// vox_world.cpp — a .tmap becomes a grid of voxels.
// OWNS: what a tile NAME means in blocks, reading story/field/tmaps/<map>.tmap exactly as the tile
//       field read it, the terrain height field, the rule-based house generator, the trees, and the
//       walkable/solid bookkeeping that falls out of the build.
// NEVER: touches GL, meshes, moves anybody, or fires a trigger. It fills VoxField's grids and stops.
// EXPOSES: vx_parse_tmap, vx_build_world, set_blk/get_blk/get_shp, terr_block, obj_kind, vx_vnoise.
// Tested by: capture.sh --vox-selftest (every map builds) and --vox-walktest (the build is walkable).
// See src/notes/world.md.
#include "vox_internal.h"

// ───────────────────────── what a tile NAME becomes, in blocks ─────────────────────────

static const NameBlock TERR_BLOCK[] = {
    { "grass", B_GRASS }, { "grass_dry", B_GRASS_DRY }, { "crop", B_CROP }, { "mud", B_MUD },
    { "dirt", B_DIRT }, { "gravel", B_GRAVEL }, { "paving", B_PAVING }, { "bridge_deck", B_PLANK },
    { "water", B_WATER }, { "sand", B_GRAVEL },
};
unsigned char terr_block(const char *name) {
    for (size_t i = 0; i < sizeof(TERR_BLOCK) / sizeof(TERR_BLOCK[0]); i++)
        if (!strcmp(TERR_BLOCK[i].name, name)) return TERR_BLOCK[i].blk;
    return B_GRASS;
}

// What kind of thing a stamp is. Names come from valley's tiles.md; anything unrecognised falls
// through to a billboard, which is the safe answer — it never blocks a lane it should not.
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
const ObjKind *obj_kind(const char *name) {
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

void vx_parse_tmap(VoxField *v, char *text) {
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
                : !strcmp(s, "objects") ? 3 : !strcmp(s, "triggers") ? 4 : !strcmp(s, "height") ? 5 : -1;
            row = 0;
            continue;
        }
        // A line beginning with a SINGLE '#' is a comment, and is not a row of anything. The tool's
        // `tmap check` has always skipped these; the engine did not, so a comment inside `## height`
        // was read as a row of base-36 heights and quietly shredded the map — hart_yard went from
        // one walk region to a hundred and fifteen. Maps are written by hand and the height grid is
        // the one section that needs explaining in place, so the comment stays and the engine
        // learns to skip it.
        if (line[0] == '#') continue;
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
            } else if (!strcmp(key, "base")) {
                int b = atoi(val);
                v->base_v = b < 1 ? 1 : b > VX_VY - 4 ? VX_VY - 4 : b;
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
        } else if (sec == 5) {
            // `## height` — the same grid shape as `## ground`, one character a cell:
            //   '0'-'9' then 'a'-'z' = base 36: 0..35 VOXELS (half a walk cell each) above the map
            //                          base, which is `base:` in `## meta`, 4 voxels by default.
            //   '.' or ' '           = unauthored: today's procedural terrain height for that cell.
            //   '~'                  = a bend: the mean of its authored orthogonal neighbours.
            // A map with NO `## height` section behaves exactly as it always did — that is the
            // compatibility rule that matters, since halm, hart_yard and west_road have no grid.
            // A cell the map speaks for is AUTHORITATIVE: the noise does not touch it, the two-voxel
            // smoothing pass does not touch it, and the reach verifier will not flatten it.
            if (row >= v->md) { row++; continue; }
            for (int x = 0; x < v->mw; x++) {
                char c = line[x];
                if (!c) break;
                int h = -1;
                if (c >= '0' && c <= '9') h = c - '0';
                else if (c >= 'a' && c <= 'z') h = 10 + (c - 'a');
                else if (c == '~') { v->hset[row][x] = 2; v->has_height = 1; continue; }
                else continue;                                  // '.', ' ' and anything else: procedural
                if (h > VX_HMAX) h = VX_HMAX;
                v->hmap[row][x] = (unsigned char)h;
                v->hset[row][x] = 1;
                v->has_height = 1;
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
            // `nightsight` is a bare trailing word on any trigger line: the trigger is hidden and
            // non-interactive until the party has night sight (vx_set_night_sight). It is stripped
            // here so every kind below still counts its own arguments the way it always did.
            int night = 0;
            while (n > 5 && (!strcmp(w[n - 1], "nightsight") || !strcmp(w[n - 1], "nightsight:") ||
                             (!strcmp(w[n - 1], "yes") && n > 6 && !strcmp(w[n - 2], "nightsight:")))) {
                if (!strcmp(w[n - 1], "yes")) n--;
                n--; night = 1;
            }
            VxTrig *g = &v->trigs[v->trig_count];
            memset(g, 0, sizeof(*g));
            g->sprart = -1;
            g->night = (unsigned char)night;
            g->x = (short)gx; g->z = (short)gz; g->w = (short)gw; g->d = (short)gd;
            if (!strcmp(k, "message")) { g->kind = TG_MESSAGE; snprintf(g->arg, sizeof(g->arg), "%s", n > 5 ? w[5] : ""); }
            else if (!strcmp(k, "zone")) { g->kind = TG_ZONE; snprintf(g->arg, sizeof(g->arg), "%s", n > 5 ? w[5] : ""); }
            else if (!strcmp(k, "trap")) { g->kind = TG_TRAP; snprintf(g->arg, sizeof(g->arg), "%s", n > 5 ? w[5] : ""); }
            else if (!strcmp(k, "scene")) { g->kind = TG_SCENE; snprintf(g->arg, sizeof(g->arg), "%s", n > 5 ? w[5] : ""); }
            // ── chapter one ──
            // `fight <encounter_id>`  fires VXE_FIGHT on walk-in, like `zone`, and draws the
            //                         encounter's field sprite standing on the trigger.
            else if (!strcmp(k, "fight")) { g->kind = TG_FIGHT; snprintf(g->arg, sizeof(g->arg), "%s", n > 5 ? w[5] : ""); }
            // `goal <Words_with_underscores>`  fires VXE_GOAL on walk-in, underscores become spaces.
            else if (!strcmp(k, "goal")) {
                g->kind = TG_GOAL;
                snprintf(g->arg, sizeof(g->arg), "%s", n > 5 ? w[5] : "");
                for (char *c = g->arg; *c; c++) if (*c == '_') *c = ' ';
            }
            // `pickup <item_id> <text_id>`  an INTERACT, like `message`: the `!` prompt, a button
            //                               press, the text in the field box, VXE_PICKUP, once only.
            else if (!strcmp(k, "pickup")) {
                if (n < 7) continue;
                g->kind = TG_PICKUP;
                snprintf(g->arg, sizeof(g->arg), "%s", w[5]);
                snprintf(g->arg2, sizeof(g->arg2), "%s", w[6]);
            }
            // `sprite <sprite_id>`  a field sprite billboard standing on the cell, nothing else.
            else if (!strcmp(k, "sprite")) {
                if (n < 6) continue;
                g->kind = TG_SPRITE;
                snprintf(g->arg, sizeof(g->arg), "%s", w[5]);
            }
            else if (!strcmp(k, "exit") || !strcmp(k, "door")) {
                if (n < 9) continue;
                g->kind = !strcmp(k, "exit") ? TG_EXIT : TG_DOOR;
                snprintf(g->map, sizeof(g->map), "%s", w[5]);
                // An exit gets a synthetic id so the chapter's condition table can name it. P1's
                // "no exits open yet" (chapter01.md) is `exit:halm` switched off until the swing
                // has been fought; there is no other way to name an exit, because the .tmap line
                // carries no id of its own and the grammar is not this side's to change.
                snprintf(g->arg, sizeof(g->arg), "exit:%s", w[5]);
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

float vx_vnoise(float x, float z, uint32_t seed) {
    int xi = (int)floorf(x), zi = (int)floorf(z);
    float fx = x - xi, fz = z - zi;
    fx = fx * fx * (3 - 2 * fx); fz = fz * fz * (3 - 2 * fz);
    float a = vx_rnd(xi, zi, (int)seed), b = vx_rnd(xi + 1, zi, (int)seed);
    float c = vx_rnd(xi, zi + 1, (int)seed), d = vx_rnd(xi + 1, zi + 1, (int)seed);
    return vx_lerp(vx_lerp(a, b, fx), vx_lerp(c, d, fx), fz);
}

// Everything below here is in VOXELS: x,z run 0..vw/vd, y runs 0..VX_VY, and one voxel is VOX_S of a
// world unit. A walk cell (cx,cz) owns the 2x2 voxel columns at (cx*2, cz*2).
void set_blk(VoxField *v, int x, int y, int z, unsigned char b) {
    if (x < 0 || z < 0 || y < 0 || x >= v->vw || z >= v->vd || y >= VX_VY) return;
    v->blk[y][z][x] = b;
    v->shp[y][z][x] = 0;
}
static void set_shaped(VoxField *v, int x, int y, int z, unsigned char b, unsigned char shape) {
    if (x < 0 || z < 0 || y < 0 || x >= v->vw || z >= v->vd || y >= VX_VY) return;
    v->blk[y][z][x] = b;
    v->shp[y][z][x] = shape;
}
unsigned char get_blk(VoxField *v, int x, int y, int z) {
    if (x < 0 || z < 0 || y < 0 || x >= v->vw || z >= v->vd || y >= VX_VY) return B_AIR;
    return v->blk[y][z][x];
}
unsigned char get_shp(VoxField *v, int x, int y, int z) {
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

void vx_build_world(VoxField *v) {
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
    const int H_FLAT = v->base_v;               // solid voxels under a flat cell (meta `base:`)
    static unsigned char hh[VX_MAXD][VX_MAXW];
    for (int z = 0; z < v->md; z++) for (int x = 0; x < v->mw; x++) {
        // A cell the `## height` section spoke for is AUTHORITATIVE. It is not rolled by the noise,
        // it is not forced flat for an object, and the smoothing pass below leaves it alone: a
        // switchback is meant to climb and a two-cell drop is meant to be a drop.
        if (v->hset[z][x] == 1) {
            int h = H_FLAT + (int)v->hmap[z][x];        // the map's chars are VOXELS above the base
            hh[z][x] = (unsigned char)(h < 1 ? 1 : h > VX_VY - 5 ? VX_VY - 5 : h);
            continue;
        }
        if (flat[z][x]) { hh[z][x] = H_FLAT; continue; }
        float n = vx_vnoise(x / 9.0f, z / 9.0f, 7717u) * 0.72f + vx_vnoise(x / 4.0f, z / 4.0f, 313u) * 0.28f;
        int h = H_FLAT + (int)(n * 5.2f);
        hh[z][x] = (unsigned char)(h < H_FLAT ? H_FLAT : h > H_FLAT + 4 ? H_FLAT + 4 : h);
    }
    // '~' in `## height`: the mean of the authored cells around it, so a bend between two shelves
    // meets both. It is resolved after the grid so it can see its neighbours' final heights.
    for (int z = 0; z < v->md; z++) for (int x = 0; x < v->mw; x++) {
        if (v->hset[z][x] != 2) continue;
        int sum = 0, cnt = 0;
        for (int d = 0; d < 4; d++) {
            int nx = x + DX[d], nz = z + DZ[d];
            if (nx < 0 || nz < 0 || nx >= v->mw || nz >= v->md || v->hset[nz][nx] != 1) continue;
            sum += hh[nz][nx]; cnt++;
        }
        if (cnt) { hh[z][x] = (unsigned char)((sum + cnt / 2) / cnt); v->hset[z][x] = 1; }
        else v->hset[z][x] = 0;
    }
    // A stamp and a trigger rectangle each sit on ONE height — their top-left cell's — so a cart
    // never straddles a step and a trigger's floor is flat under the whole rectangle. Authored or
    // procedural, the rule is the same; it is only visible on a map with a height grid, because
    // everywhere else those cells were already forced flat above.
    if (v->has_height) {
        for (int i = 0; i < v->place_count; i++) {
            VxPlace *p = &v->places[i];
            VxDef *d = &v->defs[p->def];
            unsigned char h0 = hh[p->z][p->x];
            for (int r = 0; r < d->h; r++) for (int c = 0; c < d->w; c++) {
                int mx = p->x + c, mz = p->z + r;
                if (mx < v->mw && mz < v->md) { hh[mz][mx] = h0; v->hset[mz][mx] = 1; }
            }
        }
        for (int i = 0; i < v->trig_count; i++) {
            VxTrig *g = &v->trigs[i];
            if (g->x < 0 || g->z < 0 || g->x >= v->mw || g->z >= v->md) continue;
            unsigned char h0 = hh[g->z][g->x];
            for (int r = 0; r < g->d; r++) for (int c = 0; c < g->w; c++) {
                int mx = g->x + c, mz = g->z + r;
                if (mx < v->mw && mz < v->md) { hh[mz][mx] = h0; v->hset[mz][mx] = 1; }
            }
        }
    }
    // No neighbour may differ by more than two voxels — one voxel is a free smooth step, two is a hop.
    for (int pass = 0; pass < 10; pass++) {
        bool changed = false;
        for (int z = 0; z < v->md; z++) for (int x = 0; x < v->mw; x++) {
            if (v->hset[z][x]) continue;                  // authored: the map's word is final
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

void vx_sun_defaults(VoxField *v);

