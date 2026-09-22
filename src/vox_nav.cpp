// vox_nav.cpp — the navmesh, and everything that moves the leader across it.
// OWNS: walkability and the check that it survived the height field; the nav voxel grid, its
//       regions, clearance and reachability proof; collision (depenetrate, slide); jump, gravity
//       and coyote time; facing hysteresis; the breadcrumb trail; the touch stick and the two
//       buttons; the leader's and the followers' per-frame step.
// NEVER: draws, loads a file, or decides what a trigger means (it only reports the cell change).
// EXPOSES: vx_build_nav, vx_nav_at/_y/_snap, vx_can_stand, vx_cell_reachable/_standable, vx_gy,
//          vx_move_slide, vx_face_pick, vx_trail_push, vx_input, vx_leader_move, vx_followers, vx_walk.
// Tested by: capture.sh --vox-walktest all. See src/notes/movement.md — VX_AGENT_R's proof is there.
#include "vox_internal.h"

// ───────────────────────── walkability, and the check that it survived the height field ─────────
// A step is legal when the target cell is walkable, its top is within one block, and there are two
// blocks of headroom. Every route the .tmap allows must still exist; the flood fill says so at load.

bool vx_can_stand(VoxField *v, int x, int z) {
    if (x < 0 || z < 0 || x >= v->mw || z >= v->md) return false;
    if (!v->walk[z][x]) return false;
    int y = v->hgt[z][x];                             // the voxel the feet stand on
    // Headroom. The party is 1.6 walk cells, so three voxels (1.5 cells) must be wholly clear and
    // the fourth may hold only a top slab — which is what lets a sill, an eave or a rail oversail a
    // lane without closing it. A ramp's own slope voxels are what you walk on and never count.
    for (int dz = 0; dz < VX_VPC; dz++) for (int dx = 0; dx < VX_VPC; dx++)
        for (int k = 1; k <= 4; k++) {
            int vx = x * VX_VPC + dx, vy = y + k, vz = z * VX_VPC + dz;
            // A ramp cell's own first two voxels ARE the surface you walk on, whatever shape they
            // happen to be: the generator fills the far row with a solid CUBE and puts the slope on
            // top of it, so testing for a ramp shape here failed every ramp cell in the game and the
            // reach pass then flattened all of them away. (halm lost both, west_road all thirteen.)
            if (v->ramp[z][x] && k <= 2) continue;
            if (get_blk(v, vx, vy, vz) == B_AIR) continue;
            unsigned char sp = get_shp(v, vx, vy, vz);
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
                // On a map with an authored `## height`, a ledge is part of the map, not a bug in
                // the noise: the tile reference has to see it too, or the verifier would call the
                // shelf above "unreachable" and flatten the hill the author drew.
                if (ok && v->has_height) {
                    int a2 = v->hgt[z][x], b2 = v->hgt[nz][nx];
                    if ((a2 > b2 ? a2 - b2 : b2 - a2) > 2) ok = false;
                }
            }
            if (!ok) continue;
            out[nz * VX_MAXW + nx] = 1;
            qx[tail] = (short)nx; qz[tail++] = (short)nz;
        }
    }
}

// Flatten until the voxel world is at least as connected as the tile map. In practice the flatten
// set above makes this pass first time; it exists so an edit to the noise can never strand a door.
int vx_verify_reach(VoxField *v) {
    static unsigned char tile_r[VX_MAXW * VX_MAXD], vox_r[VX_MAXW * VX_MAXD];
    int missing = 0;
    for (int round = 0; round < 6; round++) {
        flood(v, tile_r, v->spawn_x, v->spawn_z, false);
        flood(v, vox_r, v->spawn_x, v->spawn_z, true);
        missing = 0;
        for (int z = 0; z < v->md; z++) for (int x = 0; x < v->mw; x++)
            if (tile_r[z * VX_MAXW + x] && !vox_r[z * VX_MAXW + x]) missing++;
        if (!missing) return 0;
        // A map with an authored `## height` is the author's shape, and flattening it would be the
        // tool second-guessing them: a shelf you can only reach by jumping down onto it is the
        // point of the high pasture, and the tile map — which has no idea there is a hill — would
        // call it unreachable every time. So on such a map this pass only REPORTS, and the real
        // test of "can the story still be played" is vx_build_nav's target check (navreach), which
        // walks to every exit, door, message and NPC and fails loudly if one is cut off.
        if (v->has_height) {
            SDL_Log("voxfield: %s has an authored height field — %d cell%s the flat tile map could "
                    "reach are behind a ledge here (by design; navreach is the real check)",
                    v->map_name, missing, missing == 1 ? "" : "s");
            return 0;
        }
        // Flatten every unreachable cell and its neighbours back to the flat level and rebuild them.
        for (int z = 0; z < v->md; z++) for (int x = 0; x < v->mw; x++) {
            if (!tile_r[z * VX_MAXW + x] || vox_r[z * VX_MAXW + x]) continue;
            for (int dz = -1; dz <= 1; dz++) for (int dx = -1; dx <= 1; dx++) {
                int nx = x + dx, nz = z + dz;
                if (nx < 0 || nz < 0 || nx >= v->mw || nz >= v->md) continue;
                if (v->place_at[nz][nx]) continue;
                if (v->hset[nz][nx]) continue;             // never flatten what the map authored
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
float vx_gy(VoxField *v, int x, int z) {
    if (x < 0 || z < 0 || x >= v->mw || z >= v->md) return 0;
    return ((float)v->hgt[z][x] + 1.0f) * VOX_S;
}

// ───────────────────────── the navmesh ─────────────────────────
// Generated from the voxel grid at load; nothing is hand-authored. One entry per VOXEL (half a walk
// cell), which is finer than the .tmap and is what lets a fence post block half a cell without the
// whole cell going solid.
//
// A nav voxel is walkable when: the tile map calls its cell walkable, the voxel column's top is the
// cell's own walking surface (so nothing is built on it), that top face is something you can stand
// on, and four voxels of headroom are clear above it — the same rule vx_can_stand enforces for the
// houses, applied per voxel instead of per cell.
//
// Connectivity is decided by height: a difference of one voxel or less is a WALK edge, anything more
// is a LEDGE — you may drop or jump off it, you may not walk up it. That test is done live against
// the body's own height rather than baked, because after a jump the body can be anywhere.
//
// EROSION is exact and continuous, not a second grid: the body is a circle of radius `agent_r` and
// every move is resolved out of the square of any blocked nav voxel it overlaps. That is the
// Minkowski erosion of the walkable region by the radius, computed per move, with no quantisation —
// which is the "buffer" the old 3D field was missing when the party clipped into fences.

static bool vx_stand_solid(VoxField *v, int x, int y, int z) {
    unsigned char b = get_blk(v, x, y, z);
    return b != B_AIR && b != B_WATER;
}

// The highest voxel at or below `lim` you could put a foot on in this column, or -1.
static int vx_col_top(VoxField *v, int ix, int iz, int lim) {
    for (int y = lim; y >= 0; y--) if (vx_stand_solid(v, ix, y, iz)) return y;
    return -1;
}

// Four voxels clear above `top`, with the same two exemptions vx_can_stand grants: a ramp's own
// slope voxels are what you walk on, and the fourth voxel may hold a top slab so a sill, an eave or
// a rail can oversail a lane.
static bool vx_head_clear(VoxField *v, int ix, int iz, int top, bool on_ramp) {
    for (int k = 1; k <= 4; k++) {
        int y = top + k;
        if (y >= VX_VY) return true;
        if (get_blk(v, ix, y, iz) == B_AIR) continue;
        unsigned char sp = get_shp(v, ix, y, iz);
        if (on_ramp && k <= 2 && shape_is_ramp(sp)) continue;
        if (k == 4 && SH_OF(sp) == SH_SLAB && OR_OF(sp) == SL_TOP) continue;
        return false;
    }
    return true;
}

static void vx_nav_cells(VoxField *v) {
    memset(v->nav_ok, 0, sizeof(v->nav_ok));
    for (int iz = 0; iz < v->vd; iz++) for (int ix = 0; ix < v->vw; ix++) {
        int cx = ix / VX_VPC, cz = iz / VX_VPC;
        if (cx >= v->mw || cz >= v->md) continue;
        if (!v->walk[cz][cx]) continue;
        int base = v->hgt[cz][cx];
        bool ramp = v->ramp[cz][cx] != 0;
        float fx = (float)(ix % VX_VPC) * VOX_S + VOX_S * 0.5f;
        float fz = (float)(iz % VX_VPC) * VOX_S + VOX_S * 0.5f;
        float sv = vx_surface_v(v, cx, cz, fx, fz);            // the surface, in voxels
        int top = vx_col_top(v, ix, iz, ramp ? base + 2 : base);
        if (ramp) {
            if (top < base) continue;                          // a hole in the ramp
        } else {
            if (top != base) continue;                         // a hole, or something built on it
            unsigned char sp = get_shp(v, ix, top, iz);
            // a post, a pane or an upside-down slab is not a floor
            if (!shape_covers(sp, F_TOP) && !shape_is_ramp(sp)) continue;
        }
        if (!vx_head_clear(v, ix, iz, top, ramp)) continue;
        v->nav_ok[iz][ix] = 1;
        v->nav_h[iz][ix] = sv * VOX_S;
    }
}

// A two-pass chamfer distance transform over the blocked set. This is for the overlay and for the
// corridor-width log ONLY — the collision uses the exact circle-vs-square test. Stored in 1/32 of a
// walk cell, saturating at 255 (about 8 cells).
static void vx_nav_clearance(VoxField *v) {
    static float d[VX_VD][VX_VW];
    const float A = 1.0f, B = 1.41421356f;
    for (int z = 0; z < v->vd; z++) for (int x = 0; x < v->vw; x++) d[z][x] = v->nav_ok[z][x] ? 1e9f : 0.0f;
    for (int z = 0; z < v->vd; z++) for (int x = 0; x < v->vw; x++) {
        float m = d[z][x];
        if (z > 0) { if (d[z-1][x] + A < m) m = d[z-1][x] + A;
                     if (x > 0 && d[z-1][x-1] + B < m) m = d[z-1][x-1] + B;
                     if (x + 1 < v->vw && d[z-1][x+1] + B < m) m = d[z-1][x+1] + B; }
        if (x > 0 && d[z][x-1] + A < m) m = d[z][x-1] + A;
        d[z][x] = m;
    }
    for (int z = v->vd - 1; z >= 0; z--) for (int x = v->vw - 1; x >= 0; x--) {
        float m = d[z][x];
        if (z + 1 < v->vd) { if (d[z+1][x] + A < m) m = d[z+1][x] + A;
                             if (x > 0 && d[z+1][x-1] + B < m) m = d[z+1][x-1] + B;
                             if (x + 1 < v->vw && d[z+1][x+1] + B < m) m = d[z+1][x+1] + B; }
        if (x + 1 < v->vw && d[z][x+1] + A < m) m = d[z][x+1] + A;
        d[z][x] = m;
        // d is in nav voxels from centre to centre; the free space from the centre to the blocked
        // SQUARE's face is half a voxel less. In walk cells that is (d - 0.5) * VOX_S.
        float world = (m - 0.5f) * VOX_S;
        if (world < 0) world = 0;
        int q = (int)(world * 32.0f + 0.5f);
        v->nav_clr[z][x] = (unsigned char)(q > 255 ? 255 : q);
    }
}

// Walk connectivity: a height difference of one voxel or less. The flood from the spawn is region 1;
// every other component is a "jump-only" region — somewhere you can only get to by dropping or
// jumping, which is where the hidden loot goes later.
static void vx_nav_regions(VoxField *v) {
    memset(v->nav_reg, 0, sizeof(v->nav_reg));
    memset(v->nav_reg_size, 0, sizeof(v->nav_reg_size));
    static short qx[VX_VW * VX_VD], qz[VX_VW * VX_VD];
    int reg = 0;
    // The spawn is seeded first, so what you can walk to from where the party starts is ALWAYS
    // region 1 and every other component is by definition jump-only.
    int seed_x = -1, seed_z = -1;
    for (int dz = 0; dz < VX_VPC && seed_x < 0; dz++) for (int dx = 0; dx < VX_VPC && seed_x < 0; dx++) {
        int i = v->spawn_x * VX_VPC + dx, j = v->spawn_z * VX_VPC + dz;
        if (i < v->vw && j < v->vd && v->nav_ok[j][i]) { seed_x = i; seed_z = j; }
    }
    // -1 is the seed pass; then every remaining free voxel in reading order.
    for (int start = -1; start < v->vw * v->vd; start++) {
        int bx, bz;
        if (start < 0) { if (seed_x < 0) continue; bx = seed_x; bz = seed_z; }
        else { bx = start % v->vw; bz = start / v->vw; }
        if (!v->nav_ok[bz][bx] || v->nav_reg[bz][bx]) continue;
        if (reg >= 250) continue;
        reg++;
        int head = 0, tail = 0, size = 0;
        v->nav_reg[bz][bx] = (unsigned char)reg;
        qx[tail] = (short)bx; qz[tail++] = (short)bz;
        while (head < tail) {
            int x = qx[head], z = qz[head]; head++;
            size++;
            for (int d = 0; d < 4; d++) {
                int nx = x + DX[d], nz = z + DZ[d];
                if (nx < 0 || nz < 0 || nx >= v->vw || nz >= v->vd) continue;
                if (!v->nav_ok[nz][nx] || v->nav_reg[nz][nx]) continue;
                float dh = v->nav_h[nz][nx] - v->nav_h[z][x];
                if (dh > VX_STEP_UP || dh < -VX_STEP_UP) continue;      // a ledge, not a walk edge
                v->nav_reg[nz][nx] = (unsigned char)reg;
                qx[tail] = (short)nx; qz[tail++] = (short)nz;
            }
        }
        if (reg < 16) v->nav_reg_size[reg] = size;
    }
    v->nav_regions = reg;
    v->nav_jump_vox = 0;
    for (int z = 0; z < v->vd; z++) for (int x = 0; x < v->vw; x++)
        if (v->nav_ok[z][x] && v->nav_reg[z][x] != 1) v->nav_jump_vox++;
}

// Is any of this cell's four nav voxels in region 1 — i.e. can you WALK there from the spawn?
bool vx_cell_reachable(VoxField *v, int cx, int cz) {
    if (cx < 0 || cz < 0 || cx >= v->mw || cz >= v->md) return false;
    for (int dz = 0; dz < VX_VPC; dz++) for (int dx = 0; dx < VX_VPC; dx++)
        if (v->nav_reg[cz * VX_VPC + dz][cx * VX_VPC + dx] == 1) return true;
    return false;
}

// Remove decoration from a cell: anything above the walking surface that is not a full cube, plus
// any loose single voxel. Structure (a wall, a house, a fence POST line) is cube-shaped and stays;
// this is the "widen the corridor and log it" escape hatch of brief item 8.
static int vx_nav_widen_cell(VoxField *v, int cx, int cz) {
    if (cx < 0 || cz < 0 || cx >= v->mw || cz >= v->md) return 0;
    int removed = 0, base = v->hgt[cz][cx];
    for (int dz = 0; dz < VX_VPC; dz++) for (int dx = 0; dx < VX_VPC; dx++)
        for (int k = 1; k <= 4; k++) {
            int ix = cx * VX_VPC + dx, iz = cz * VX_VPC + dz, y = base + k;
            if (y >= VX_VY) continue;
            if (get_blk(v, ix, y, iz) == B_AIR) continue;
            unsigned char sp = get_shp(v, ix, y, iz);
            int sh = SH_OF(sp);
            if (sh == SH_CUBE) continue;               // structure: leave it alone
            set_blk(v, ix, y, iz, B_AIR);
            v->shp[y][iz][ix] = 0;
            removed++;
        }
    return removed;
}

// Is any of this cell's four nav voxels walkable AT ALL, whatever region it is in — i.e. could you
// stand there if something put you there? A shelf you can only jump onto answers true here and
// false to vx_cell_reachable, and the difference between those two answers is the whole of the
// "designed jump-only" idea below.
bool vx_cell_standable(VoxField *v, int cx, int cz) {
    if (cx < 0 || cz < 0 || cx >= v->mw || cz >= v->md) return false;
    for (int dz = 0; dz < VX_VPC; dz++) for (int dx = 0; dx < VX_VPC; dx++)
        if (v->nav_ok[cz * VX_VPC + dz][cx * VX_VPC + dx]) return true;
    return false;
}

// Everything the story needs to be able to reach on foot: every exit, door and message trigger, and
// the cell next to every NPC.
//
// THREE ANSWERS, NOT TWO (2026-09-21). A target that walking cannot reach used to be a flat
// failure, which is why high_pasture reported `navreach=FAIL` for a shelf that LOOT.md deliberately
// puts behind a jump. A target is now classified:
//
//   reachable   — some cell of it is in walk region 1. Nothing to say.
//   jump-only   — no cell of it is in region 1, but some cell of it is STANDABLE. You can get there,
//                 by jumping or by dropping; the navmesh already calls that a jump-only region.
//                 Recorded in v->nav_jump_tgt[] and REPORTED, never failed, and never widened —
//                 widening a designed jump is the tool undoing the level.
//   dead        — no cell of it, and no cell beside it, is standable at all. That is a real break
//                 and it is what `nav_bad` counts.
//
// Which of the jump-only ones are DESIGNED is not the field's business: the field has no idea what
// a hidden item is. src/chapter01.h's CH_JUMP_ONLY table is the design, and the chapter play-test
// checks this list against it in both directions.
static int vx_nav_check_targets(VoxField *v, short *bad_x, short *bad_z, int cap) {
    int n = 0;
    v->nav_jump_tgt_n = 0;
    for (int i = 0; i < v->trig_count; i++) {
        VxTrig *g = &v->trigs[i];
        bool any = false, stand = false;
        for (int z = g->z; z < g->z + g->d && !any; z++) for (int x = g->x; x < g->x + g->w && !any; x++)
            if (vx_cell_reachable(v, x, z)) any = true;
        // A door in a wall — or an examine on anything solid — is reached from the cell in front of
        // it, not from its own cell. But only if that cell is within the interact button's reach in
        // Y as well as in plan: a ledge three cells up with walkable ground underneath is NOT
        // reachable from underneath, and saying it was is what made the eaves tin a ground pickup.
        for (int d = 0; d < 4 && !any; d++) {
            int nx = g->x + DX[d], nz = g->z + DZ[d];
            if (!vx_cell_reachable(v, nx, nz)) continue;
            if (fabsf(vx_gy(v, nx, nz) - vx_gy(v, g->x, g->z)) > VX_EXAM_REACH) continue;
            any = true;
        }
        if (any) continue;
        for (int z = g->z; z < g->z + g->d && !stand; z++) for (int x = g->x; x < g->x + g->w && !stand; x++)
            if (vx_cell_standable(v, x, z)) stand = true;
        if (stand) {
            if (v->nav_jump_tgt_n < VX_JUMPTGT) {
                VxJumpTgt *t = &v->nav_jump_tgt[v->nav_jump_tgt_n++];
                t->x = g->x; t->z = g->z; t->kind = (short)g->kind;
                snprintf(t->arg, sizeof(t->arg), "%s", g->arg);
            }
            continue;                                   // designed or not, never a failure here
        }
        if (n < cap) { bad_x[n] = g->x; bad_z[n] = g->z; }
        n++;
    }
    for (int i = 0; i < v->npc_count; i++) {
        VxNpc *np = &v->npcs[i];
        bool any = vx_cell_reachable(v, np->tx, np->tz);
        for (int d = 0; d < 4 && !any; d++) any = vx_cell_reachable(v, np->tx + DX[d], np->tz + DZ[d]);
        if (any) continue;
        if (n < cap) { bad_x[n] = np->tx; bad_z[n] = np->tz; }
        n++;
    }
    return n;
}

void vx_build_nav(VoxField *v) {
    uint64_t t0 = SDL_GetPerformanceCounter();
    v->nav_widened = 0;
    short bx[64], bz[64];
    int bad = 0;
    for (int round = 0; round < 4; round++) {
        vx_nav_cells(v);
        vx_nav_regions(v);
        bad = vx_nav_check_targets(v, bx, bz, 64);
        if (!bad) break;
        int did = 0;
        for (int i = 0; i < bad && i < 64; i++)
            for (int dz = -1; dz <= 1; dz++) for (int dx = -1; dx <= 1; dx++)
                did += vx_nav_widen_cell(v, bx[i] + dx, bz[i] + dz);
        v->nav_widened += did;
        SDL_Log("voxfield nav: %d target%s unreachable on foot — removed %d decoration voxels around "
                "them and rebuilding", bad, bad == 1 ? "" : "s", did);
        if (!did) break;                                    // nothing left to remove; report it
    }
    v->nav_bad = bad;
    vx_nav_clearance(v);
    v->nav_ms = (double)(SDL_GetPerformanceCounter() - t0) * 1000.0 / (double)SDL_GetPerformanceFrequency();
    int freev = 0;
    for (int z = 0; z < v->vd; z++) for (int x = 0; x < v->vw; x++) if (v->nav_ok[z][x]) freev++;
    { char rl[160]; rl[0] = 0;                      // where the ramps are, so a capture can be aimed at one
      for (int z = 0; z < v->md; z++) for (int x = 0; x < v->mw; x++) if (v->ramp[z][x]) {
          char one[16]; snprintf(one, sizeof one, "%s%d,%d", rl[0] ? " " : "", x, z);
          if (strlen(rl) + strlen(one) < sizeof(rl) - 1) strcat(rl, one); }
      if (rl[0]) SDL_Log("voxfield nav: %s ramps at %s", v->map_name, rl); }
    // VOX_HDUMP=1 prints the surface height of every cell and the walk region beside it, in the same
    // grid shape as the .tmap's `## height` section. It is how a hand-authored height field is
    // debugged: an authored plateau that turns out to be walk-connected to the ground shows up here
    // as a run of heights climbing where the author wrote a cliff. Base 36, '.' = not walkable.
    if (SDL_getenv("VOX_HDUMP")) {
        SDL_Log("voxfield hdump %s: surface height (base 36, relative to base %d)", v->map_name, v->base_v);
        for (int z = 0; z < v->md; z++) {
            char h[VX_MAXW + 1], r[VX_MAXW + 1];
            for (int x = 0; x < v->mw; x++) {
                int dh = (int)v->hgt[z][x] - (v->base_v - 1);
                h[x] = !v->walk[z][x] ? '#' : dh < 0 ? '-' : dh < 10 ? (char)('0' + dh) : dh < 36 ? (char)('a' + dh - 10) : '+';
                int reg = 0;
                for (int dz = 0; dz < VX_VPC && !reg; dz++) for (int dx = 0; dx < VX_VPC && !reg; dx++)
                    if (v->nav_ok[z * VX_VPC + dz][x * VX_VPC + dx]) reg = v->nav_reg[z * VX_VPC + dz][x * VX_VPC + dx];
                r[x] = !reg ? '.' : reg < 10 ? (char)('0' + reg) : (char)('a' + reg - 10);
            }
            h[v->mw] = r[v->mw] = 0;
            SDL_Log("voxfield hdump %s: %2d  %s   %s", v->map_name, z, h, r);
        }
    }
    SDL_Log("voxfield nav: %s %dx%d voxels, %d walkable (%d walk-reachable, %d jump-only in %d region%s), "
            "radius %.2f, %.2f ms%s", v->map_name, v->vw, v->vd, freev, freev - v->nav_jump_vox,
            v->nav_jump_vox, v->nav_regions > 1 ? v->nav_regions - 1 : 0, v->nav_regions == 2 ? "" : "s",
            v->agent_r, v->nav_ms, bad ? "  TARGETS UNREACHABLE" : "");
}

// ── queries ──

bool vx_nav_at(VoxField *v, float x, float z) {
    int ix = (int)floorf(x / VOX_S), iz = (int)floorf(z / VOX_S);
    if (ix < 0 || iz < 0 || ix >= v->vw || iz >= v->vd) return false;
    return v->nav_ok[iz][ix] != 0;
}

// The walking surface at a continuous position: bilinear over the four nearest nav-voxel CENTRES,
// dropping any that is not walkable so a ledge never drags the surface down with it.
float vx_nav_y(VoxField *v, float x, float z) {
    float gx = x / VOX_S - 0.5f, gz = z / VOX_S - 0.5f;
    int i0 = (int)floorf(gx), j0 = (int)floorf(gz);
    float fx = gx - i0, fz = gz - j0;
    float sum = 0, wsum = 0, best = 0; bool any = false;
    for (int dj = 0; dj < 2; dj++) for (int di = 0; di < 2; di++) {
        int i = i0 + di, j = j0 + dj;
        if (i < 0 || j < 0 || i >= v->vw || j >= v->vd || !v->nav_ok[j][i]) continue;
        float w = (di ? fx : 1 - fx) * (dj ? fz : 1 - fz);
        sum += v->nav_h[j][i] * w; wsum += w;
        if (!any || v->nav_h[j][i] > best) { best = v->nav_h[j][i]; any = true; }
    }
    if (wsum > 1e-4f) return sum / wsum;
    if (any) return best;
    int cx = (int)floorf(x), cz = (int)floorf(z);
    return vx_gy(v, cx < 0 ? 0 : cx >= v->mw ? v->mw - 1 : cx, cz < 0 ? 0 : cz >= v->md ? v->md - 1 : cz);
}

// The nearest valid nav point to (x,z), searched outward a ring at a time. `limit` is in cells; -1
// means the whole map. Returns false when there is nowhere to stand at all.
bool vx_nav_snap(VoxField *v, float *x, float *z, float limit) {
    if (vx_nav_at(v, *x, *z)) return true;
    int ix = (int)floorf(*x / VOX_S), iz = (int)floorf(*z / VOX_S);
    int maxr = limit < 0 ? (v->vw > v->vd ? v->vw : v->vd) : (int)(limit / VOX_S) + 1;
    for (int r = 1; r <= maxr; r++) {
        int bi = -1, bj = -1; float bd = 1e9f;
        for (int dj = -r; dj <= r; dj++) for (int di = -r; di <= r; di++) {
            if (abs(di) != r && abs(dj) != r) continue;             // the ring only
            int i = ix + di, j = iz + dj;
            if (i < 0 || j < 0 || i >= v->vw || j >= v->vd || !v->nav_ok[j][i]) continue;
            float cx = (i + 0.5f) * VOX_S, cz = (j + 0.5f) * VOX_S;
            float d = (cx - *x) * (cx - *x) + (cz - *z) * (cz - *z);
            if (d < bd) { bd = d; bi = i; bj = j; }
        }
        if (bi >= 0) { *x = (bi + 0.5f) * VOX_S; *z = (bj + 0.5f) * VOX_S; return true; }
    }
    return false;
}

// ───────────────────────── free movement on the navmesh ─────────────────────────
// The owner, 2026-09-20: 8-directional, fully free, with jumping. The leader is a POINT with a
// radius; nothing about a cell survives except the trigger grid, which is deliberately still
// cell-based so the .tmap keeps meaning what it meant.
//
// Rules, in one place:
//   * ground move  = desired velocity * dt, sub-stepped so no step exceeds half a nav voxel, each
//     sub-step resolved out of every blocked nav voxel square the body's circle overlaps. That is
//     exact Minkowski erosion by the radius, so the body's centre can never come closer than
//     `agent_r` to a wall, a fence, water or a ledge that steps UP more than one voxel.
//   * a ledge that steps DOWN is not a wall: you walk off it and fall.
//   * airborne the body is off the navmesh entirely and is tested against voxel SOLIDS instead,
//     the same circle against any column with something in the band from the feet to head height.
//   * every frame ends with the centre inside the region. If it ever does not (a bug), the body is
//     put back where it last was valid and one line is logged.

// Anything solid in this voxel column between world heights y0 and y1.
static bool vx_solid_band(VoxField *v, int ix, int iz, float y0, float y1) {
    if (ix < 0 || iz < 0 || ix >= v->vw || iz >= v->vd) return true;       // off the map is a wall
    int a = (int)floorf(y0 / VOX_S), b = (int)floorf((y1 - 0.0001f) / VOX_S);
    if (a < 0) a = 0;
    if (b >= VX_VY) b = VX_VY - 1;
    for (int y = a; y <= b; y++) if (blk_solid(get_blk(v, ix, y, iz))) return true;
    return false;
}

// A walking blocker: off the navmesh, or a surface more than one voxel above the feet.
bool vx_ground_block(VoxField *v, int ix, int iz, float feet) {
    if (ix < 0 || iz < 0 || ix >= v->vw || iz >= v->vd) return true;
    if (!v->nav_ok[iz][ix]) return true;
    return v->nav_h[iz][ix] > feet + VX_STEP_UP;
}

static bool vx_air_block(VoxField *v, int ix, int iz, float feet) {
    if (ix < 0 || iz < 0 || ix >= v->vw || iz >= v->vd) return true;
    return vx_solid_band(v, ix, iz, feet + 0.12f, feet + VX_BODY_H);
}

// Push the circle at (*x,*z) out of every blocker it overlaps, deepest first. Up to `iters` passes,
// which is what makes a corner between two blockers converge instead of oscillating.
static int vx_depenetrate(VoxField *v, float *x, float *z, float feet, bool air, float r, int iters) {
    int moved = 0;
    for (int it = 0; it < iters; it++) {
        int i0 = (int)floorf((*x - r) / VOX_S), i1 = (int)floorf((*x + r) / VOX_S);
        int j0 = (int)floorf((*z - r) / VOX_S), j1 = (int)floorf((*z + r) / VOX_S);
        float bestpen = 0, bnx = 0, bnz = 0;
        for (int j = j0; j <= j1; j++) for (int i = i0; i <= i1; i++) {
            bool blocked = air ? vx_air_block(v, i, j, feet) : vx_ground_block(v, i, j, feet);
            if (!blocked) continue;
            float qx0 = i * VOX_S, qz0 = j * VOX_S, qx1 = qx0 + VOX_S, qz1 = qz0 + VOX_S;
            float cx = *x < qx0 ? qx0 : *x > qx1 ? qx1 : *x;     // the closest point on the square
            float cz = *z < qz0 ? qz0 : *z > qz1 ? qz1 : *z;
            float dx = *x - cx, dz = *z - cz;
            float d2 = dx * dx + dz * dz;
            float pen, nx, nz;
            if (d2 > 1e-8f) {
                float d = sqrtf(d2);
                if (d >= r) continue;
                pen = r - d; nx = dx / d; nz = dz / d;
            } else {
                // the centre is inside the square: leave by the nearest face
                float dl = *x - qx0, dr = qx1 - *x, dn = *z - qz0, ds = qz1 - *z;
                float m = dl; nx = -1; nz = 0;
                if (dr < m) { m = dr; nx = 1; nz = 0; }
                if (dn < m) { m = dn; nx = 0; nz = -1; }
                if (ds < m) { m = ds; nx = 0; nz = 1; }
                pen = m + r;
            }
            if (pen > bestpen) { bestpen = pen; bnx = nx; bnz = nz; }
        }
        if (bestpen <= 0) break;
        *x += bnx * (bestpen + 0.0005f);
        *z += bnz * (bestpen + 0.0005f);
        moved++;
    }
    return moved;
}

// One move-and-slide. Sub-stepped so a single step can never exceed half a nav voxel — the body
// cannot tunnel through a one-voxel-thick fence however large dt is.
void vx_move_slide(VoxField *v, float *x, float *z, float dx, float dz, float feet, bool air, float r) {
    float len = sqrtf(dx * dx + dz * dz);
    int steps = (int)(len / VX_MAX_SUBSTEP) + 1;
    if (steps > 64) steps = 64;
    for (int s = 0; s < steps; s++) {
        *x += dx / steps;
        *z += dz / steps;
        vx_depenetrate(v, x, z, feet, air, r, 3);
    }
    float w = v->vw * VOX_S, d = v->vd * VOX_S;
    if (*x < r) *x = r;
    if (*z < r) *z = r;
    if (*x > w - r) *x = w - r;
    if (*z > d - r) *z = d - r;
}

// ── facing, with hysteresis ──
// Four rows of art (S, side, N; side mirrored for E) and eight directions of movement, so a diagonal
// sits exactly on the boundary between two rows and flickers if you take the nearest one every
// frame. The facing is KEPT until the move vector is more than VX_FACE_HYST degrees off its axis.

int vx_face_pick(int cur, float mx, float mz) {
    float len = sqrtf(mx * mx + mz * mz);
    if (len < 1e-4f) return cur;
    mx /= len; mz /= len;
    if (cur >= 0 && cur < 4) {
        float c = mx * DX[cur] + mz * DZ[cur];
        if (c > cosf(VX_FACE_HYST * 3.14159265f / 180.0f)) return cur;
    }
    int best = cur < 0 ? 0 : cur; float bd = -2;
    for (int d = 0; d < 4; d++) {
        float c = mx * DX[d] + mz * DZ[d];
        if (c > bd) { bd = c; best = d; }
    }
    return best;
}

static float vx_face_ang(float mx, float mz, float cur) {
    if (mx * mx + mz * mz < 1e-6f) return cur;
    return atan2f(mz, mx);
}

// ── the breadcrumb trail the followers walk ──

void vx_trail_push(VoxField *v, float x, float z, float y, bool air) {
    int last = (v->trail_head - 1 + VX_TRAIL) % VX_TRAIL;
    if (v->trail_n > 0) {
        float dx = x - v->trail[last].x, dz = z - v->trail[last].z;
        float d = sqrtf(dx * dx + dz * dz);
        if (d < VX_TRAIL_DS) return;
        v->trail_s += d;
    }
    VxCrumb *c = &v->trail[v->trail_head];
    c->x = x; c->z = z; c->y = y; c->s = v->trail_s; c->air = air ? 1 : 0;
    v->trail_head = (v->trail_head + 1) % VX_TRAIL;
    if (v->trail_n < VX_TRAIL) v->trail_n++;
}

// The point `back` cells behind the head of the trail. Returns false when the trail is too short,
// which is the only case the follower has to teleport for.
static bool vx_trail_sample(VoxField *v, float back, float *x, float *z, float *y, float *mx, float *mz, unsigned char *air) {
    if (v->trail_n < 2) return false;
    float want = v->trail_s - back;
    int newer = (v->trail_head - 1 + VX_TRAIL) % VX_TRAIL;
    for (int k = 1; k < v->trail_n; k++) {
        int older = (v->trail_head - 1 - k + 2 * VX_TRAIL) % VX_TRAIL;
        VxCrumb *a = &v->trail[older], *b = &v->trail[newer];
        if (a->s <= want) {
            float span = b->s - a->s;
            float t = span > 1e-5f ? (want - a->s) / span : 0.0f;
            *x = a->x + (b->x - a->x) * t;
            *z = a->z + (b->z - a->z) * t;
            *y = a->y + (b->y - a->y) * t;
            float dx = b->x - a->x, dz = b->z - a->z;
            float l = sqrtf(dx * dx + dz * dz);
            *mx = l > 1e-5f ? dx / l : 0; *mz = l > 1e-5f ? dz / l : 0;
            *air = (a->air || b->air) ? 1 : 0;
            return true;
        }
        newer = older;
    }
    return false;
}

// ───────────────────────── input ─────────────────────────
// Touch is a FULLY FREE floating analog stick: it appears wherever the left thumb lands in the left
// 45% of the screen, any angle, dead zone 12% of its radius, and the MAGNITUDE picks the speed —
// under 55% of the throw is a walk, over it is a run. There is no run button any more; the stick
// carries it, which is one fewer thing for a thumb to find. The right side has two buttons, Jump and
// the interact button that was already there, both low and right so nothing overlaps Dev/Settings
// at the top right.
// Keyboard is WASD/arrows (8 directions, normalised so a diagonal is not faster), Shift to run,
// Space to jump, Enter or Z to interact.

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

// The two thumb buttons, in screen pixels. One place, so draw_touch_ui and the hit test agree.
void vx_btn_act(int w, int h, float *x, float *y, float *r)  { *x = w - h * 0.19f; *y = h * 0.72f; *r = h * 0.115f; }
void vx_btn_jump(int w, int h, float *x, float *y, float *r) { *x = w - h * 0.49f; *y = h * 0.84f; *r = h * 0.105f; }

void vx_input(VoxField *v, int w, int h, bool blocked, float dt) {
    VxTouch tt[8];
    int n = blocked ? 0 : vx_touches(tt, 8, w, h);
    v->tapped = false;
    v->want_jump = 0;
    // THE BOT PRESSES THE SAME TWO BUTTONS. `bot_tap` and `bot_jump` are one-shot latches set by
    // vx_bot_interact / vx_bot_jump, and they land on exactly the two variables the Act button and
    // the Jump button land on — so the chapter play-test's interact goes through trig_at, the cone
    // test, the message pager and vx_fire, not around them. Everything below this block is touch
    // and keyboard and is skipped entirely while the bot drives.
    if (v->bot_on) {
        if (v->bot_tap) { v->tapped = true; v->bot_tap = 0; }
        if (v->bot_jump) { v->want_jump = 1; v->bot_jump = 0; }
        return;
    }
    bool stick_seen = false, act_seen = false, jump_seen = false;
    float jx, jy, jr, ax2, ay2, ar;
    vx_btn_jump(w, h, &jx, &jy, &jr);
    vx_btn_act(w, h, &ax2, &ay2, &ar);
    for (int i = 0; i < n; i++) {
        if (v->stick_on && tt[i].id == v->stick_id) { stick_seen = true; v->stick_x = tt[i].x; v->stick_y = tt[i].y; }
        if (v->act_on && tt[i].id == v->act_id) act_seen = true;
        if (v->jump_held && tt[i].id == v->jump_id) jump_seen = true;
    }
    if (v->stick_on && !stick_seen) v->stick_on = false;
    if (v->act_on && !act_seen) { v->tapped = true; v->act_on = false; v->act_t = 0; }
    if (v->jump_held && !jump_seen) v->jump_held = 0;
    for (int i = 0; i < n; i++) {
        if ((v->stick_on && tt[i].id == v->stick_id) || (v->act_on && tt[i].id == v->act_id) ||
            (v->jump_held && tt[i].id == v->jump_id)) continue;
        float dxj = tt[i].x - jx, dyj = tt[i].y - jy;
        float dxa = tt[i].x - ax2, dya = tt[i].y - ay2;
        if (!v->jump_held && dxj * dxj + dyj * dyj <= jr * jr * 1.44f) {
            v->jump_held = 1; v->jump_id = tt[i].id; v->want_jump = 1;
        } else if (!v->act_on && dxa * dxa + dya * dya <= ar * ar * 1.44f) {
            v->act_on = true; v->act_id = tt[i].id; v->act_t = 0;
        } else if (tt[i].x < w * 0.45f && !v->stick_on) {
            v->stick_on = true; v->stick_id = tt[i].id;
            v->stick_ox = v->stick_x = tt[i].x; v->stick_oy = v->stick_y = tt[i].y;
        }
    }
    if (v->act_on) v->act_t += dt;
    if (!blocked) {
        ImGuiIO &io = ImGui::GetIO();
        if (!io.WantCaptureKeyboard) {
            if (ImGui::IsKeyPressed(ImGuiKey_Enter, false) || ImGui::IsKeyPressed(ImGuiKey_KeypadEnter, false) ||
                ImGui::IsKeyPressed(ImGuiKey_Z, false)) v->tapped = true;
            if (ImGui::IsKeyPressed(ImGuiKey_Space, false)) v->want_jump = 1;
        }
    }
}

// The desired move vector, in world units, normalised, plus a magnitude 0..1 that decides walk or
// run on touch. Screen up is north (-z) because the camera never rotates.
static void vx_move_input(VoxField *v, int w, int h) {
    float dx = 0, dy = 0, mag = 0;
    bool from_stick = false;
    if (v->bot_on) {
        v->move_in_x = v->bot_mx; v->move_in_z = v->bot_mz;
        float l = sqrtf(v->bot_mx * v->bot_mx + v->bot_mz * v->bot_mz);
        if (l > 1e-4f) { v->move_in_x /= l; v->move_in_z /= l; v->move_mag = l > 1 ? 1.0f : l; }
        else { v->move_in_x = v->move_in_z = 0; v->move_mag = 0; }
        v->run = v->bot_run != 0;
        return;
    }
    float R = h * 0.14f;                               // the stick's throw, as drawn
    if (v->stick_on) {
        dx = v->stick_x - v->stick_ox; dy = v->stick_y - v->stick_oy;
        float l = sqrtf(dx * dx + dy * dy);
        if (l < R * 0.12f) { dx = dy = 0; }             // dead zone
        else { mag = l / R; if (mag > 1) mag = 1; dx /= l; dy /= l; from_stick = true; }
    }
    float kx = 0, ky = 0;
    ImGuiIO &io = ImGui::GetIO();
    if (!io.WantCaptureKeyboard) {
        if (ImGui::IsKeyDown(ImGuiKey_LeftArrow) || ImGui::IsKeyDown(ImGuiKey_A)) kx -= 1;
        if (ImGui::IsKeyDown(ImGuiKey_RightArrow) || ImGui::IsKeyDown(ImGuiKey_D)) kx += 1;
        if (ImGui::IsKeyDown(ImGuiKey_UpArrow) || ImGui::IsKeyDown(ImGuiKey_W)) ky -= 1;
        if (ImGui::IsKeyDown(ImGuiKey_DownArrow) || ImGui::IsKeyDown(ImGuiKey_S)) ky += 1;
    }
    if (kx != 0 || ky != 0) {
        float l = sqrtf(kx * kx + ky * ky);             // a diagonal is not faster
        dx = kx / l; dy = ky / l; mag = 1; from_stick = false;
    }
    v->move_in_x = dx; v->move_in_z = dy; v->move_mag = mag;
    bool shift = false;
    if (!io.WantCaptureKeyboard)
        shift = ImGui::IsKeyDown(ImGuiKey_LeftShift) || ImGui::IsKeyDown(ImGuiKey_RightShift);
    v->run = from_stick ? (mag > 0.55f) : shift;
}

// ───────────────────────── the leader ─────────────────────────

static void vx_respawn_at_takeoff(VoxField *v, const char *why) {
    SDL_Log("voxfield: %s — no valid landing, returning to %.2f,%.2f", why, v->tko_x, v->tko_z);
    v->land_fails++;
    VxActor *a = &v->act[0];
    a->x = v->tko_x; a->z = v->tko_z; a->y = v->tko_y;
    v->pvx = v->pvz = v->pvy = 0;
    v->airborne = 0; v->coyote = VX_COYOTE;
    v->fade = FADE_FRAMES; v->fade_dir = -1;              // a quick fade back in, no damage
}

void vx_leader_move(VoxField *v, int w, int h, float dt) {
    VxActor *a = &v->act[0];
    if (dt > 0.1f) dt = 0.1f;                            // a hitch must not become a teleport

    bool frozen = v->msg[0] || v->fade_dir > 0 || v->frozen;   // vx_freeze: a scripted beat
    if (frozen) { v->move_in_x = v->move_in_z = 0; v->move_mag = 0; v->want_jump = 0; v->jump_buf = 0; }
    else vx_move_input(v, w, h);

    float mx = v->move_in_x, mz = v->move_in_z;
    float target = 0;
    if (mx != 0 || mz != 0) target = v->run ? v->sp_run : v->sp_walk;
    // On touch the magnitude is the speed, so a small push is a creep and a full push is a run.
    if (v->move_mag > 0 && v->move_mag < 0.55f && !v->bot_on) target = v->sp_walk * (0.35f + v->move_mag);

    float want_vx = mx * target, want_vz = mz * target;
    // Ground movement is nearly direct (a JRPG wants the character to start when you push), air
    // control is a fraction of it.
    float tau = v->airborne ? 0.06f / VX_AIR_CTRL : 0.06f;
    float k = dt / (tau + dt);
    v->pvx += (want_vx - v->pvx) * k;
    v->pvz += (want_vz - v->pvz) * k;

    // jump buffering and coyote time
    if (v->want_jump) v->jump_buf = VX_JUMPBUF;
    else if (v->jump_buf > 0) v->jump_buf -= dt;
    if (!v->airborne) v->coyote = VX_COYOTE; else if (v->coyote > 0) v->coyote -= dt;
    if (v->jump_buf > 0 && v->coyote > 0 && !frozen && !v->noclip) {
        v->pvy = sqrtf(2.0f * v->gravity * v->jump_apex);
        v->airborne = 1; v->coyote = 0; v->jump_buf = 0;
        v->tko_x = a->x; v->tko_z = a->z; v->tko_y = a->y;
    }

    if (v->noclip) {
        a->x += v->pvx * dt; a->z += v->pvz * dt;
        a->y = vx_nav_y(v, a->x, a->z);
        v->airborne = 0;
    } else if (!v->airborne) {
        float ox = a->x, oz = a->z;
        vx_move_slide(v, &a->x, &a->z, v->pvx * dt, v->pvz * dt, a->y, false, v->agent_r);
        // what the body actually achieved is its speed, so a body pressed into a wall stops animating
        float adx = a->x - ox, adz = a->z - oz;
        v->speed_now = dt > 0 ? sqrtf(adx * adx + adz * adz) / dt : 0;
        float g = vx_nav_y(v, a->x, a->z);
        if (g < a->y - 0.30f) { v->airborne = 1; v->pvy = 0; v->tko_x = ox; v->tko_z = oz; v->tko_y = a->y; }
        else a->y += (g - a->y) * (dt * 18.0f > 1 ? 1 : dt * 18.0f);
    } else {
        float ox = a->x, oz = a->z;
        vx_move_slide(v, &a->x, &a->z, v->pvx * dt, v->pvz * dt, a->y, true, v->agent_r);
        float adx = a->x - ox, adz = a->z - oz;
        v->speed_now = dt > 0 ? sqrtf(adx * adx + adz * adz) / dt : 0;
        v->pvy -= v->gravity * dt;
        float ny = a->y + v->pvy * dt;
        if (v->pvy > 0) {
            // head bump: something in the band just above the head stops the climb
            int ix = (int)floorf(a->x / VOX_S), iz = (int)floorf(a->z / VOX_S);
            if (vx_solid_band(v, ix, iz, ny + VX_BODY_H - 0.1f, ny + VX_BODY_H + 0.05f)) { v->pvy = 0; ny = a->y; }
        } else {
            float g = vx_nav_y(v, a->x, a->z);
            if (ny <= g) {
                float lx = a->x, lz = a->z;
                bool ok = vx_nav_at(v, lx, lz);
                if (!ok) ok = vx_nav_snap(v, &lx, &lz, v->agent_r);
                if (ok) {
                    a->x = lx; a->z = lz; a->y = vx_nav_y(v, lx, lz); ny = a->y;
                    v->airborne = 0; v->pvy = 0; v->coyote = VX_COYOTE;
                } else { vx_respawn_at_takeoff(v, "landed on water, a void or a top too narrow to stand on"); return; }
            }
        }
        if (v->airborne) a->y = ny;
        if (a->y < -2.0f) { vx_respawn_at_takeoff(v, "fell off the world"); return; }
    }

    // The invariant. If a frame ever ends with the centre outside the region it is a bug, so say so
    // once and put the body back rather than letting it walk away inside a wall.
    if (!v->noclip && !v->airborne && !vx_nav_at(v, a->x, a->z)) {
        float sx = a->x, sz = a->z;
        if (!v->invalid_logged) {
            SDL_Log("voxfield: BUG — the leader ended a frame off the navmesh at %.3f,%.3f on %s; snapping back",
                    a->x, a->z, v->map_name);
            v->invalid_logged = 1;
        }
        if (vx_nav_snap(v, &sx, &sz, 3.0f)) { a->x = sx; a->z = sz; a->y = vx_nav_y(v, sx, sz); }
        else { a->x = v->tko_x; a->z = v->tko_z; a->y = v->tko_y; }
        v->pvx = v->pvz = 0;
    }

    a->facing = vx_face_pick(a->facing, v->pvx, v->pvz);
    a->ang = vx_face_ang(v->pvx, v->pvz, a->ang);
    if (v->speed_now > 0.15f) a->phase += v->speed_now * 1.35f * dt; else a->phase = 0;
    v->moving = v->speed_now > 0.15f;

    vx_trail_push(v, a->x, a->z, a->y, v->airborne != 0);

    int cx = (int)floorf(a->x), cz = (int)floorf(a->z);
    if (cx < 0) cx = 0; if (cz < 0) cz = 0;
    if (cx >= v->mw) cx = v->mw - 1;
    if (cz >= v->md) cz = v->md - 1;
    if (cx != a->tx || cz != a->tz) {
        a->tx = (short)cx; a->tz = (short)cz;
        on_enter_cell(v, v->airborne == 0);
    }
}

// The rest of the party walks the leader's recorded path: same route, same jumps, same spots, and
// no collision between party members at all — she can never be the thing standing in your way.
void vx_followers(VoxField *v, float dt) {
    for (int i = 1; i < VX_PARTY; i++) {
        VxActor *a = &v->act[i];
        float x, z, y, mx, mz; unsigned char air;
        bool got = vx_trail_sample(v, VX_FOLLOW_D * i, &x, &z, &y, &mx, &mz, &air);
        float lx = v->act[0].x, lz = v->act[0].z;
        float far2 = (a->x - lx) * (a->x - lx) + (a->z - lz) * (a->z - lz);
        bool bad = !got || far2 > 8.0f * 8.0f || (!air && !vx_nav_at(v, x, z));
        if (bad) v->follow_stuck[i] += dt; else v->follow_stuck[i] = 0;
        if (v->follow_stuck[i] > 2.0f || far2 > 14.0f * 14.0f) {
            float sx = lx, sz = lz;
            vx_nav_snap(v, &sx, &sz, 3.0f);
            a->x = sx; a->z = sz; a->y = vx_nav_y(v, sx, sz);
            a->phase = 0; v->follow_stuck[i] = 0; v->follow_warp_t[i] = 0.25f;
            continue;
        }
        if (v->follow_warp_t[i] > 0) v->follow_warp_t[i] -= dt;
        if (!got) continue;
        float dx = x - a->x, dz = z - a->z;
        float d = sqrtf(dx * dx + dz * dz);
        a->x = x; a->z = z; a->y = y;
        if (d > 1e-4f) { a->facing = vx_face_pick(a->facing, dx, dz); a->ang = vx_face_ang(dx, dz, a->ang); }
        float sp = dt > 0 ? d / dt : 0;
        if (sp > 0.15f) a->phase += sp * 1.35f * dt; else a->phase = 0;
        a->tx = (short)floorf(a->x); a->tz = (short)floorf(a->z);
    }
}

void vx_walk(VoxField *v, int w, int h, float dt) {
    vx_leader_move(v, w, h, dt);
    vx_followers(v, dt);
}

