// vox_bot.cpp — the field's test hands: the walk test, and the play-test bot's API.
// OWNS: capture.sh --vox-walktest (scripted input against walls, A* to every exit/door/NPC, 200
//       jumps), the A* itself, and the vx_bot_* calls src/star_logic.cpp's --chapter-playtest
//       drives the real game through.
// NEVER: sets a flag, fires an event or shortcuts the game. Every one of these lands on exactly the
//        variables a thumb lands on; everything that follows happens inside vx_tick as it always does.
// EXPOSES: vx_walktest, vx_bot, vx_bot_stick/_interact/_jump/_where/_cell/_path/_waypoint,
//          vx_bot_can_walk/_jump_only/_examinable/_jump_targets, vx_find_trigger.
// Tested by: it IS the test. See src/notes/testing.md, "The three tests".
#include "vox_internal.h"

// ───────────────────────── the walk test (capture.sh --vox-walktest) ─────────────────────────
// A deterministic bot driving the SAME movement code the player's thumb drives, with no rendering
// and no phone. Three parts, and it exits non-zero on any of them:
//   (a) a scripted input sequence run at jittered dt against walls, fences and corners, asserting
//       the body never leaves the eroded region, never NaNs, and never sticks for more than 3 s
//       while the input is held along a path that is open;
//   (b) A* on the nav grid from the spawn to every exit, door and NPC, then the bot WALKS that path
//       and has to arrive;
//   (c) 200 jumps from random valid spots on random headings, each of which has to end either on
//       valid ground or in a clean respawn.
// A* is the bot's, not the game's: nothing in the field pathfinds.

struct VxBotRng { uint32_t s; };
static uint32_t bot_rand(VxBotRng *r) { r->s = r->s * 1664525u + 1013904223u; return r->s; }
static float bot_f(VxBotRng *r) { return (float)(bot_rand(r) & 0xFFFFFF) / (float)0xFFFFFF; }

static void vx_bot_frame(VoxField *v, float dt, float mx, float mz, bool run, bool jump) {
    v->bot_on = 1; v->bot_mx = mx; v->bot_mz = mz; v->bot_run = run ? 1 : 0;
    v->want_jump = jump ? 1 : 0;
    vx_leader_move(v, 1920, 1080, dt);
    vx_followers(v, dt);
    npc_step(v, dt);
    // a map change would pull the world out from under the test; the bot never takes one
    v->fade_dir = 0; v->fade = 0; v->to_map[0] = 0;
    v->msg[0] = 0;
}

// A* over the nav grid with WALK connectivity only. Returns the length, or 0.
#define VXB_PATH 4096
static int vx_bot_astar(VoxField *v, int sx, int sz, int gx, int gz, short *px, short *pz) {
    static float g[VX_VD][VX_VW];
    static short pi[VX_VD][VX_VW], pj[VX_VD][VX_VW];
    static unsigned char closed[VX_VD][VX_VW];
    static int qi[VX_VW * VX_VD], qj[VX_VW * VX_VD];
    static float qf[VX_VW * VX_VD];
    for (int j = 0; j < v->vd; j++) for (int i = 0; i < v->vw; i++) { g[j][i] = 1e18f; closed[j][i] = 0; }
    int n = 0;
    g[sz][sx] = 0; qi[n] = sx; qj[n] = sz; qf[n] = 0; n++;
    while (n > 0) {
        int best = 0;
        for (int k = 1; k < n; k++) if (qf[k] < qf[best]) best = k;
        int x = qi[best], z = qj[best];
        qi[best] = qi[n - 1]; qj[best] = qj[n - 1]; qf[best] = qf[n - 1]; n--;
        if (closed[z][x]) continue;
        closed[z][x] = 1;
        if (x == gx && z == gz) break;
        for (int d = 0; d < 4; d++) {
            int nx = x + DX[d], nz = z + DZ[d];
            if (nx < 0 || nz < 0 || nx >= v->vw || nz >= v->vd) continue;
            if (!v->nav_ok[nz][nx] || closed[nz][nx]) continue;
            float dh = v->nav_h[nz][nx] - v->nav_h[z][x];
            if (dh > VX_STEP_UP || dh < -VX_STEP_UP) continue;
            float ng = g[z][x] + 1.0f;
            if (ng >= g[nz][nx]) continue;
            g[nz][nx] = ng; pi[nz][nx] = (short)x; pj[nz][nx] = (short)z;
            if (n < VX_VW * VX_VD) { qi[n] = nx; qj[n] = nz; qf[n] = ng + (float)(abs(nx - gx) + abs(nz - gz)); n++; }
        }
    }
    if (g[gz][gx] > 1e17f) return 0;
    short tx[VXB_PATH], tz[VXB_PATH];
    int m = 0, x = gx, z = gz;
    while (m < VXB_PATH && !(x == sx && z == sz)) { tx[m] = (short)x; tz[m] = (short)z; m++; int ax = pi[z][x], az = pj[z][x]; x = ax; z = az; }
    for (int k = 0; k < m; k++) { px[k] = tx[m - 1 - k]; pz[k] = tz[m - 1 - k]; }
    return m;
}

// The nav voxel a cell's reachable corner sits on, or false.
static bool vx_bot_cell_vox(VoxField *v, int cx, int cz, int *ox, int *oz) {
    for (int dz = 0; dz < VX_VPC; dz++) for (int dx = 0; dx < VX_VPC; dx++) {
        int i = cx * VX_VPC + dx, j = cz * VX_VPC + dz;
        if (i < v->vw && j < v->vd && v->nav_reg[j][i] == 1) { *ox = i; *oz = j; return true; }
    }
    return false;
}

static int vx_walktest_map(VoxField *v, const char *map, char *summary, int cap) {
    if (!vx_load_map(v, map)) { snprintf(summary, cap, "%s: FAILED TO LOAD", map); return 1; }
    int fails = 0;
    VxBotRng rng; rng.s = 0x9E3779B9u ^ (uint32_t)strlen(map) * 2654435761u;
    for (const char *c = map; *c; c++) rng.s = rng.s * 31u + (unsigned char)*c;
    v->noclip = 0;
    int out_of_region = 0, nan_hits = 0, stuck_hits = 0;

    // ── (a) scripted input against the world ──
    // Eight headings held for a second each, then a long diagonal grind, run on jittered dt. The
    // body is pressed into whatever it meets; nothing here should ever push it off the region.
    const int FRAMES = 6000;
    float held_x = 0, held_z = 0, stuck_t = 0;
    float last_x = v->act[0].x, last_z = v->act[0].z;
    for (int f = 0; f < FRAMES; f++) {
        float dt = 1.0f / 60.0f * (0.5f + bot_f(&rng) * 1.6f);       // dt jitter: 8 ms to 35 ms
        int phase = (f / 90) % 10;
        float ang = phase < 8 ? phase * 0.7853981f : (f * 0.013f);
        held_x = cosf(ang); held_z = sinf(ang);
        bool run = (f / 90) % 3 == 0;
        vx_bot_frame(v, dt, held_x, held_z, run, false);
        VxActor *a = &v->act[0];
        if (!(a->x == a->x) || !(a->z == a->z) || !(a->y == a->y)) { nan_hits++; break; }
        if (!v->airborne && !vx_nav_at(v, a->x, a->z)) out_of_region++;
        float moved = sqrtf((a->x - last_x) * (a->x - last_x) + (a->z - last_z) * (a->z - last_z));
        last_x = a->x; last_z = a->z;
        // "stuck" only counts when the way the body is being pushed is actually open a cell ahead
        // AT THE BODY'S OWN HEIGHT. vx_nav_at alone is a plan test: on a map with an authored
        // height field the cell in front of you can be perfectly walkable and still be the face of
        // a four-voxel step, and standing still against a hillside is the correct behaviour, not a
        // stuck body. (hill_path reported three of these before the feet were taken into account.)
        // The probe walks OUT FROM THE BODY, and every sample has to be open at the body's own
        // height. Two things were wrong with a single sample a cell ahead: it is a plan test, so a
        // hillside four voxels tall read as open ground; and it skips the voxel the body is
        // actually pressed against, so a body correctly stopped by a step it cannot climb was
        // reported stuck because the ledge BEYOND that step happened to be level with it.
        float ax = a->x + held_x, az = a->z + held_z;
        bool open_ahead = true;
        for (int k = 1; k <= 4 && open_ahead; k++) {
            float t = (v->agent_r + 0.10f) + (1.0f - v->agent_r - 0.10f) * (k / 4.0f);
            float sx = a->x + held_x * t, sz = a->z + held_z * t;
            if (!vx_nav_at(v, sx, sz) ||
                vx_ground_block(v, (int)floorf(sx / VOX_S), (int)floorf(sz / VOX_S), a->y)) open_ahead = false;
        }
        (void)ax; (void)az;
        if (moved < 0.002f && open_ahead) stuck_t += dt; else stuck_t = 0;
        if (stuck_t > 3.0f) {
            stuck_hits++; stuck_t = 0;
            SDL_Log("WALKTEST %s: stuck at %.2f,%.2f y=%.2f pushing %.2f,%.2f with the way open",
                    v->map_name, a->x, a->z, a->y, held_x, held_z);
        }
    }
    fails += (out_of_region > 0) + (nan_hits > 0) + (stuck_hits > 0);

    // ── (b) path-walk to every exit, door and NPC ──
    int targets = 0, arrived = 0;
    short px[VXB_PATH], pz[VXB_PATH];
    struct { int x, z; } tgt[128];
    int nt = 0;
    for (int i = 0; i < v->trig_count && nt < 128; i++) {
        if (v->trigs[i].kind != TG_EXIT && v->trigs[i].kind != TG_DOOR) continue;
        tgt[nt].x = v->trigs[i].x + v->trigs[i].w / 2; tgt[nt].z = v->trigs[i].z + v->trigs[i].d / 2; nt++;
    }
    for (int i = 0; i < v->npc_count && nt < 128; i++) { tgt[nt].x = v->npcs[i].tx; tgt[nt].z = v->npcs[i].tz; nt++; }
    for (int t = 0; t < nt; t++) {
        int gx, gz, sx, sz;
        bool have = vx_bot_cell_vox(v, tgt[t].x, tgt[t].z, &gx, &gz);
        if (!have) for (int d = 0; d < 4 && !have; d++) have = vx_bot_cell_vox(v, tgt[t].x + DX[d], tgt[t].z + DZ[d], &gx, &gz);
        if (!have) continue;                                   // nothing the game asks to be walkable
        targets++;
        vx_place_party(v, v->spawn_x, v->spawn_z, 0);
        VxActor *a = &v->act[0];
        sx = (int)floorf(a->x / VOX_S); sz = (int)floorf(a->z / VOX_S);
        if (sx < 0 || sz < 0 || sx >= v->vw || sz >= v->vd || !v->nav_ok[sz][sx]) continue;
        int n = vx_bot_astar(v, sx, sz, gx, gz, px, pz);
        if (!n) { SDL_Log("WALKTEST %s: no walk path from the spawn to %d,%d", map, tgt[t].x, tgt[t].z); continue; }
        int wp = 0, budget = n * 60 + 600;
        while (wp < n && budget-- > 0) {
            float wx = (px[wp] + 0.5f) * VOX_S, wz = (pz[wp] + 0.5f) * VOX_S;
            float dx = wx - v->act[0].x, dz = wz - v->act[0].z;
            float d = sqrtf(dx * dx + dz * dz);
            if (d < VOX_S * 0.6f) { wp++; continue; }
            vx_bot_frame(v, 1.0f / 60.0f, dx / d, dz / d, false, false);
            if (!v->airborne && !vx_nav_at(v, v->act[0].x, v->act[0].z)) out_of_region++;
        }
        float fx = (px[n - 1] + 0.5f) * VOX_S, fz = (pz[n - 1] + 0.5f) * VOX_S;
        float dd = sqrtf((fx - v->act[0].x) * (fx - v->act[0].x) + (fz - v->act[0].z) * (fz - v->act[0].z));
        if (dd < 1.0f) arrived++;
        else SDL_Log("WALKTEST %s: did not arrive at %d,%d (%.2f cells short)", map, tgt[t].x, tgt[t].z, dd);
    }
    if (arrived != targets) fails++;

    // ── (c) 200 jumps ──
    int jumps = 0, jump_bad = 0, respawns0 = v->land_fails;
    for (int k = 0; k < 200; k++) {
        int i, j, tries = 0;
        do { i = (int)(bot_f(&rng) * v->vw); j = (int)(bot_f(&rng) * v->vd); }
        while (++tries < 400 && (i >= v->vw || j >= v->vd || !v->nav_ok[j][i]));
        if (i >= v->vw || j >= v->vd || !v->nav_ok[j][i]) continue;
        vx_place_party_at(v, (i + 0.5f) * VOX_S, (j + 0.5f) * VOX_S, 0);
        float ang = bot_f(&rng) * 6.2831853f;
        float hx = cosf(ang), hz = sinf(ang);
        jumps++;
        int before = v->land_fails;
        vx_bot_frame(v, 1.0f / 60.0f, hx, hz, true, true);
        int guard = 600;
        while (v->airborne && guard-- > 0) vx_bot_frame(v, 1.0f / 60.0f, hx, hz, true, false);
        // a respawn is a clean outcome; what is not allowed is ending off the region, or NaN, or
        // never coming down at all
        VxActor *a = &v->act[0];
        bool nan = !(a->x == a->x) || !(a->z == a->z) || !(a->y == a->y);
        bool clean = (v->land_fails > before) || (!v->airborne && vx_nav_at(v, a->x, a->z));
        if (nan || !clean || guard <= 0) {
            jump_bad++;
            if (jump_bad <= 3) SDL_Log("WALKTEST %s: jump %d from %.2f,%.2f ended badly (air=%d nan=%d guard=%d)",
                                       map, k, (i + 0.5f) * VOX_S, (j + 0.5f) * VOX_S, v->airborne, (int)nan, guard);
        }
    }
    if (jump_bad) fails++;

    v->bot_on = 0;
    snprintf(summary, cap,
             "WALKTEST %s: %s  frames=%d off-region=%d nan=%d stuck=%d | paths %d/%d arrived | "
             "jumps %d/%d clean (%d respawns) | nav %.2f ms r=%.2f",
             map, fails ? "FAIL" : "ok", FRAMES, out_of_region, nan_hits, stuck_hits,
             arrived, targets, jumps - jump_bad, jumps, v->land_fails - respawns0, v->nav_ms, v->agent_r);
    return fails;
}

// ───────────────────── the play-test bot's hands (src/star_logic.cpp --chapter-playtest) ─────────
// The walk test above drives vx_leader_move directly, because all it is testing is the body. The
// CHAPTER play-test has to go through vx_tick — triggers, messages, events, map changes, the lot —
// so it needs the stick and the two buttons rather than the movement function. That is all this is:
// a stick and two buttons, plus the A* the bot needs to know where to push the stick.
//
// NOTHING HERE SETS A FLAG, OPENS A BOX OR FIRES AN EVENT. If the bot reaches a trigger it is
// because it walked onto it, and if a box opens it is because vx_tick opened it.

void vx_bot(VoxField *v, int on) {
    if (!v) return;
    v->bot_on = on ? 1 : 0;
    v->headless = v->bot_on;              // the bot never needs the picture; see vx_tick
    if (!v->bot_on) { v->bot_mx = v->bot_mz = 0; v->bot_run = v->bot_tap = v->bot_jump = 0; }
}
void vx_bot_stick(VoxField *v, float mx, float mz, int run) {
    if (!v) return;
    v->bot_mx = mx; v->bot_mz = mz; v->bot_run = run ? 1 : 0;
}
void vx_bot_interact(VoxField *v) { if (v) v->bot_tap = 1; }
void vx_bot_jump(VoxField *v)     { if (v) v->bot_jump = 1; }

void vx_bot_where(VoxField *v, float *x, float *z, int *airborne) {
    if (!v) return;
    if (x) *x = v->act[0].x;
    if (z) *z = v->act[0].z;
    if (airborne) *airborne = v->airborne;
}

// Walk connectivity only, cell to cell, as a pure query: can the player get from here to there
// WITHOUT jumping? This is the negative test for a hidden item ("reachable by plain walking" is a
// failure) and the positive test for everything mandatory.
bool vx_bot_can_walk(VoxField *v, int fx, int fz, int tx, int tz) {
    if (!v) return false;
    int sx, sz, gx, gz;
    if (!vx_bot_cell_vox(v, fx, fz, &sx, &sz)) return false;
    bool have = vx_bot_cell_vox(v, tx, tz, &gx, &gz);
    for (int d = 0; d < 4 && !have; d++) have = vx_bot_cell_vox(v, tx + DX[d], tz + DZ[d], &gx, &gz);
    if (!have) return false;
    return v->nav_reg[sz][sx] && v->nav_reg[sz][sx] == v->nav_reg[gz][gx];
}

// Is this cell somewhere a body could stand, but not somewhere walking can get to? — the engine's
// own definition of a jump-only place, asked about one cell.
bool vx_bot_jump_only(VoxField *v, int cx, int cz) {
    if (!v) return false;
    return vx_cell_standable(v, cx, cz) && !vx_cell_reachable(v, cx, cz);
}

// The party's current cell as the bot sees it.
void vx_bot_cell(VoxField *v, int *cx, int *cz) {
    if (!v) return;
    if (cx) *cx = (int)floorf(v->act[0].x / VOX_S / VX_VPC);
    if (cz) *cz = (int)floorf(v->act[0].z / VOX_S / VX_VPC);
}

// A* from where the party is standing to a cell, in CELLS, walk edges only. Writes at most `cap`
// waypoints and returns how many; 0 means there is no walking route, which is a fact the test
// wants rather than an error.
int vx_bot_path(VoxField *v, int tx, int tz, short *out_x, short *out_z, int cap) {
    if (!v || cap < 1) return 0;
    static short px[VXB_PATH], pz[VXB_PATH];
    int sx = (int)floorf(v->act[0].x / VOX_S), sz = (int)floorf(v->act[0].z / VOX_S);
    if (sx < 0 || sz < 0 || sx >= v->vw || sz >= v->vd || !v->nav_ok[sz][sx]) return 0;
    int gx, gz;
    bool have = vx_bot_cell_vox(v, tx, tz, &gx, &gz);
    for (int d = 0; d < 4 && !have; d++) have = vx_bot_cell_vox(v, tx + DX[d], tz + DZ[d], &gx, &gz);
    if (!have) return 0;
    int n = vx_bot_astar(v, sx, sz, gx, gz, px, pz);
    if (n > cap) {
        // Decimate rather than truncate: the bot only needs enough waypoints to steer by, and a
        // truncated path would stop it halfway and read as "did not arrive".
        int step = (n + cap - 1) / cap, m = 0;
        for (int i = 0; i < n && m < cap; i += step) { out_x[m] = px[i]; out_z[m] = pz[i]; m++; }
        if (m && (out_x[m - 1] != px[n - 1] || out_z[m - 1] != pz[n - 1])) { out_x[m - 1] = px[n - 1]; out_z[m - 1] = pz[n - 1]; }
        return m;
    }
    for (int i = 0; i < n; i++) { out_x[i] = px[i]; out_z[i] = pz[i]; }
    return n;
}

// The waypoints are in NAV VOXELS; this is the world position of one.
void vx_bot_waypoint(VoxField *v, int wx, int wz, float *x, float *z) {
    (void)v;
    if (x) *x = (wx + 0.5f) * VOX_S;
    if (z) *z = (wz + 0.5f) * VOX_S;
}

// What the interact button would act on right now, as an id, so the bot can tell whether standing
// here and pressing would do the thing it came to do. "" when there is nothing.
const char *vx_bot_examinable(VoxField *v) {
    if (!v) return "";
    if (v->exam_npc >= 0) return v->npcs[v->exam_npc].text;
    if (v->exam_trig >= 0) {
        VxTrig *g = &v->trigs[v->exam_trig];
        return g->kind == TG_PICKUP ? g->arg2 : g->arg;
    }
    return "";
}

// WHERE IS THIS TRIGGER? The centre cell of the trigger whose arg (or, for a pickup, whose text
// id) is `id`. This is what lets the play-test be told a FLAG and find its way to the thing that
// sets it without anybody writing a cell number into a test: the chapter's binding table says
// which id, and the map says where. False means the map does not have it — which is exactly the
// shape of the three blockers the 2026-09-21 audit found.
bool vx_find_trigger(VoxField *v, const char *id, short *cx, short *cz) {
    if (!v || !id || !id[0]) return false;
    for (int i = 0; i < v->trig_count; i++) {
        VxTrig *g = &v->trigs[i];
        if (strcmp(g->arg, id) && strcmp(g->arg2, id)) continue;
        if (cx) *cx = (short)(g->x + g->w / 2);
        if (cz) *cz = (short)(g->z + g->d / 2);
        return true;
    }
    for (int i = 0; i < v->npc_count; i++) {
        if (strcmp(v->npcs[i].text, id)) continue;
        if (cx) *cx = v->npcs[i].tx;
        if (cz) *cz = v->npcs[i].tz;
        return true;
    }
    return false;
}

int vx_bot_jump_targets(VoxField *v, const char **ids, short *xs, short *zs, int cap) {
    if (!v) return 0;
    int n = v->nav_jump_tgt_n < cap ? v->nav_jump_tgt_n : cap;
    for (int i = 0; i < n; i++) {
        if (ids) ids[i] = v->nav_jump_tgt[i].arg;
        if (xs) xs[i] = v->nav_jump_tgt[i].x;
        if (zs) zs[i] = v->nav_jump_tgt[i].z;
    }
    return v->nav_jump_tgt_n;
}

int vx_walktest(VoxField *v, const char *one_map) {
    if (!v->gl_ready) vx_gl_init(v);
    int bad = 0;
    char line[320];
    for (int i = 0; i < vx_map_count(); i++) {
        const char *m = vx_map_name_at(i);
        if (one_map && one_map[0] && strcmp(one_map, "all") && strcmp(one_map, m)) continue;
        int f = vx_walktest_map(v, m, line, sizeof line);
        SDL_Log("%s", line);
        bad += f;
    }
    SDL_Log("WALKTEST verdict: %s (%d map%s bad)", bad ? "FAIL" : "ok", bad, bad == 1 ? "" : "s");
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
    for (int i = 0; i < VX_PARTY; i++) { s->px[i] = v->act[i].x; s->pz[i] = v->act[i].z; }
    s->agent_r = v->agent_r; s->sp_walk = v->sp_walk; s->sp_run = v->sp_run;
    s->jump_apex = v->jump_apex; s->gravity = v->gravity; s->nav_view = v->nav_view;
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
    // The movement tunables go in BEFORE the map load, because the nav build reads the radius.
    if (s->agent_r >= 0.05f && s->agent_r <= 0.45f) v->agent_r = s->agent_r;
    if (s->sp_walk >= 0.5f && s->sp_walk <= 20.0f) v->sp_walk = s->sp_walk;
    if (s->sp_run >= 0.5f && s->sp_run <= 30.0f) v->sp_run = s->sp_run;
    if (s->jump_apex >= 0.1f && s->jump_apex <= 6.0f) v->jump_apex = s->jump_apex;
    if (s->gravity >= 4.0f && s->gravity <= 200.0f) v->gravity = s->gravity;
    v->nav_view = s->nav_view ? 1 : 0;
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
    // The float position is the truth; the cell is only the fallback for a blob written before free
    // movement existed. Either way the party comes back ON THE GROUND and never airborne.
    float rx = s->px[0], rz = s->pz[0];
    if (!(rx > 0.0f && rz > 0.0f && rx < v->mw && rz < v->md)) { rx = lx + 0.5f; rz = lz + 0.5f; }
    vx_place_party_at(v, rx, rz, s->facing[0] & 3);
    for (int i = 1; i < VX_PARTY; i++) {
        float x = s->px[i], z = s->pz[i];
        if (!(x > 0.0f && z > 0.0f && x < v->mw && z < v->md)) continue;
        if (!vx_nav_snap(v, &x, &z, 2.0f)) continue;
        v->act[i].x = x; v->act[i].z = z;
        v->act[i].tx = v->act[i].px = (short)floorf(x);
        v->act[i].tz = v->act[i].pz = (short)floorf(z);
        v->act[i].y = v->act[i].y0 = vx_nav_y(v, x, z);
        v->act[i].facing = s->facing[i] & 3;
    }
}

