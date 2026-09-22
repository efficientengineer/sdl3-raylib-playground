// voxfield.cpp — the field's lifecycle and its one tick. src/VOXFIELD_NOTES.md is still the
// contract; src/ENGINE.md says which file holds what.
// OWNS: vx_create/vx_destroy, vx_load_map, the per-frame vx_tick that orders every other module,
//       the desktop capture, the self-test and the colormap identity check, the reload blob
//       (vx_save/vx_restore) and the handful of knobs the chapter script drives (vx_goto,
//       vx_set_light, vx_set_party_lamp, vx_set_night_sight, vx_freeze, ...).
// NEVER: contains a system. World building, meshing, rendering, movement, triggers, sprites, the
//        Dev panel and the bots each live in their own src/vox_*.cpp; this file calls them in order.
// EXPOSES: everything in src/voxfield.h. Internals shared across modules are in src/vox_internal.h.
// Tested by: all of capture.sh. See src/ENGINE.md.
#include "vox_internal.h"

static const char *VX_MAPS[] = { "halm", "hart_yard", "west_road", "hill_path", "high_pasture" };
int vx_map_count() { return (int)(sizeof(VX_MAPS) / sizeof(VX_MAPS[0])); }
const char *vx_map_name_at(int i) { return (i >= 0 && i < vx_map_count()) ? VX_MAPS[i] : "halm"; }

int vx_max_tex = 0;
void vx_probe_limits() {
    if (!vx_max_tex) { GLint m = 0; glGetIntegerv(GL_MAX_TEXTURE_SIZE, &m); vx_max_tex = m > 0 ? m : 2048; }
}

// ───────────────────────── load ─────────────────────────

// Every spawn, teleport and door arrival goes through here, and every one of them SNAPS to the
// nearest valid nav point. The old 3D field's second reported bug was the party not landing on a
// walkable poly at spawn; a spiral search that always terminates is the fix.
void vx_place_party_at(VoxField *v, float wx, float wz, int facing) {
    float sx = wx, sz = wz;
    if (!v->noclip && !vx_nav_snap(v, &sx, &sz, -1.0f))
        SDL_Log("voxfield: nowhere to stand on %s at all — leaving the party at %.2f,%.2f", v->map_name, wx, wz);
    float moved = sqrtf((sx - wx) * (sx - wx) + (sz - wz) * (sz - wz));
    if (moved > 0.5f)
        SDL_Log("voxfield: spawn %.2f,%.2f is not on the navmesh — snapped %.2f cells to %.2f,%.2f",
                wx, wz, moved, sx, sz);
    float y = vx_nav_y(v, sx, sz);
    for (int i = 0; i < VX_PARTY; i++) {
        VxActor *a = &v->act[i];
        a->x = sx; a->z = sz; a->y = a->y0 = y;
        a->tx = a->px = (short)floorf(sx);
        a->tz = a->pz = (short)floorf(sz);
        a->facing = facing;
        a->ang = facing == 3 ? -1.57079633f : facing == 2 ? 0.0f : facing == 1 ? 3.14159265f : 1.57079633f;
        a->phase = 0;
    }
    v->pvx = v->pvz = v->pvy = 0;
    v->airborne = 0; v->coyote = VX_COYOTE; v->jump_buf = 0;
    v->tko_x = sx; v->tko_z = sz; v->tko_y = y;
    v->trail_n = 0; v->trail_head = 0; v->trail_s = 0;
    v->invalid_logged = 0;
    v->moving = false; v->move_t = 0;
}

void vx_place_party(VoxField *v, int x, int z, int facing) {
    vx_place_party_at(v, x + 0.5f, z + 0.5f, facing);
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
    memset(v->hmap, 0, sizeof(v->hmap));
    memset(v->hset, 0, sizeof(v->hset));
    v->has_height = 0; v->base_v = 4;
    v->evq_n = 0;
    v->mw = 24; v->md = 16;
    v->spawn_x = 4; v->spawn_z = 4; v->spawn_f = 0;
    v->amb = 1.0f; v->light_table = 0;
    snprintf(v->map_name, sizeof(v->map_name), "%s", clean);

    if (text) { vx_parse_tmap(v, text); SDL_free(text); }
    else SDL_Log("voxfield: no %s — using a bare fallback map", rel);
    if (!v->def_count) vx_load_tileset(v, "valley");

    vx_build_world(v);
    v->reach_missing = vx_verify_reach(v);
    vx_build_nav(v);                       // the navmesh, generated from the voxel grid; may widen
    v->nav_dirty = true; v->nav_verts = 0;
    vx_load_atlas(v);
    vx_load_decals(v);
    vx_bind_sprites(v);                    // every fight/sprite trigger's field sprite
    vx_scatter_detail(v);
    vx_mesh_all(v);
    vx_sun_defaults(v);
    for (int i = 0; i < VX_PARTY; i++) v->art_party[i] = vx_art_get(v, PARTY_ART[i]);
    for (int i = 0; i < v->npc_count; i++) {
        VxNpc *np = &v->npcs[i];
        np->art = vx_art_get(v, np->walker);
        np->x = np->gx = np->tx + 0.5f;
        np->z = np->gz = np->tz + 0.5f;
        vx_nav_snap(v, &np->x, &np->z, 2.0f);
        np->gx = np->x; np->gz = np->z;
        np->y = np->py_ = vx_nav_y(v, np->x, np->z);
        np->phase = 0; np->wait = vx_rnd(i, 3, 11) * 2.0f;
    }
    vx_place_party(v, v->spawn_x, v->spawn_z, v->spawn_f);
    v->steps = 0;
    v->msg[0] = 0; v->msg_who[0] = 0;
    v->selfchecked = false;
    return true;
}

// One line, the first time a map is drawn: everything a reader needs to know whether the world is
// right, without a screenshot. Printed from the tick, once GL has actually drawn a frame.
void vx_render_shadow(VoxField *v);

void vx_selfcheck(VoxField *v, int dw, int dh, int draws, int sprites) {
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
    // The navmesh's own verdict. `jumponly` is where hidden loot goes later, so it is a number to
    // read and not a failure; `navreach` is the failure.
    {
        int freev = 0;
        for (int z = 0; z < v->vd; z++) for (int x = 0; x < v->vw; x++) if (v->nav_ok[z][x]) freev++;
        char sizes[160]; sizes[0] = 0;
        for (int r = 2; r < v->nav_regions + 1 && r < 16; r++) {
            char one[24];
            snprintf(one, sizeof one, "%s%d", sizes[0] ? "," : "", v->nav_reg_size[r]);
            if (strlen(sizes) + strlen(one) < sizeof(sizes) - 1) strcat(sizes, one);
        }
        SDL_Log("SELFCHECK vox: nav map=%s r=%.2f  walkable=%d vox  walk-reachable=%d  jumponly=%d in %d region(s)"
                "%s%s  widened=%d  navreach=%s  jump-targets=%d  nav=%.2fms",
                v->map_name, v->agent_r, freev, freev - v->nav_jump_vox, v->nav_jump_vox,
                v->nav_regions > 1 ? v->nav_regions - 1 : 0, sizes[0] ? " sizes " : "", sizes,
                v->nav_widened, v->nav_bad ? "FAIL" : "ok", v->nav_jump_tgt_n, v->nav_ms);
        // Named, so the map author can see at a glance whether the list is the design or an accident.
        for (int i = 0; i < v->nav_jump_tgt_n; i++)
            SDL_Log("SELFCHECK vox:   %s jump-only target '%s' at %d,%d (walking cannot reach it; "
                    "src/chapter01.h CH_JUMP_ONLY says whether that is the design)",
                    v->map_name, v->nav_jump_tgt[i].arg, v->nav_jump_tgt[i].x, v->nav_jump_tgt[i].z);
    }
}

void vx_tick(VoxField *v, int w, int h, float dt, bool ui_blocked, VxEvent *ev) {
    ev->kind = VXE_NONE; ev->arg[0] = 0; ev->arg2[0] = 0;
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
            vx_fire(v, VXE_MAP, v->map_name, "");      // an exit or a door completed
        } else if (v->fade_dir < 0 && v->fade <= 0) { v->fade = 0; v->fade_dir = 0; }
    }

    // The one dynamic light rides the leader. Cheap, and it has to happen before the world draw
    // reads the uniform or the pool would lag the body by a frame.
    if (v->dlamp_on) {
        v->dlamp_x = v->act[0].x; v->dlamp_z = v->act[0].z; v->dlamp_y = v->act[0].y + 1.0f;
    }

    vxp_begin(VXP_MOVE);
    vx_walk(v, w, h, dt);
    npc_step(v, dt);
    vxp_end(VXP_MOVE);

    // What the interact button would act on. Free movement means this is a distance and a cone, not
    // "the cell in front of me": within 0.9 cells and roughly facing it (a 100-degree cone), or
    // standing on it. Airborne, nothing is examinable.
    v->exam_trig = v->exam_npc = -1;
    if (!v->airborne) {
        VxActor *a = &v->act[0];
        float fxd = (float)DX[a->facing & 3], fzd = (float)DZ[a->facing & 3];
        const float COS_CONE = -0.1736f;                 // cos(100 degrees)
        int cand = npc_near(v, a->x, a->z, 0.9f + VX_NPC_R);
        if (cand >= 0) {
            float dx = v->npcs[cand].x - a->x, dz = v->npcs[cand].z - a->z;
            float l = sqrtf(dx * dx + dz * dz);
            if (l < 1e-3f || (dx * fxd + dz * fzd) / l > COS_CONE) v->exam_npc = cand;
        }
        if (v->exam_npc < 0) {
            // the cell 0.9 ahead, then the one under the feet
            int fx = (int)floorf(a->x + fxd * 0.9f), fz = (int)floorf(a->z + fzd * 0.9f);
            const unsigned AHEAD = TGM(TG_MESSAGE) | TGM(TG_DOOR) | TGM(TG_PICKUP);
            const unsigned UNDER = TGM(TG_MESSAGE) | TGM(TG_PICKUP);
            // AND WITHIN ARM'S REACH IN Y. Without this the cone is a PLAN test, so a tin nailed
            // under the eaves three cells up could be taken by standing in the yard underneath it
            // and facing north — which is what the hart_yard height field turned the chapter's
            // first platforming lesson into the first time it was authored. A trigger on a SOLID
            // cell (a wall, a well, a house front) keeps that cell's ground height, so examining
            // those from beside them still works exactly as it did.
            // THE WHOLE CELL, not the first trigger on it. The cell ahead wins if it has anything
            // at all; otherwise the one underfoot. `exam_cell` is where a press would look; the
            // prompt shows the first trigger of that cell in delivery order, which is the one the
            // press acts on, and the rest follow as each box is dismissed.
            bool reach = !(fx >= 0 && fz >= 0 && fx < v->mw && fz < v->md &&
                           fabsf(vx_gy(v, fx, fz) - a->y) > VX_EXAM_REACH);
            int q[VX_EXAM_Q], n = 0;
            if (reach) n = trig_collect(v, fx, fz, AHEAD, q, VX_EXAM_Q);
            if (n) { v->exam_cx = fx; v->exam_cz = fz; v->exam_mask = AHEAD; }
            else {
                n = trig_collect(v, a->tx, a->tz, UNDER, q, VX_EXAM_Q);
                v->exam_cx = a->tx; v->exam_cz = a->tz; v->exam_mask = UNDER;
            }
            v->exam_trig = n ? q[0] : -1;
        }
    }
    if (v->tapped && !v->fade_dir) {
        if (v->msg[0]) {
            // HEADLESS: `msg_typing` and `msg_more` are decided by draw_msg_box, which measures the
            // text against the font at the real screen width — so with the picture undrawn they are
            // stale forever, every press only re-finishes a typewriter that is already finished,
            // and the box never closes. The play-test then reads the same line for eternity, the
            // field will not move the body while a box is open, and it presents as "the player
            // cannot walk to the swing", which is how an hour went.
            //
            // So with no renderer a press CLOSES the box, whole. The consequence is stated in
            // VOXFIELD_NOTES.md: the play-test does not exercise the typewriter or pagination.
            // --capture.sh --dialog and the robustness sweep are what cover those.
            bool closed = false;
            if (v->headless) { v->msg[0] = 0; v->msg_who[0] = 0; v->msg_page = 0; closed = true; }
            else if (v->msg_typing) v->msg_t = 99.0f;
            else if (v->msg_more) { v->msg_page++; v->msg_t = 0; }
            else { v->msg[0] = 0; v->msg_who[0] = 0; v->msg_page = 0; closed = true; }
            // THE REST OF THE CELL. Closing a box is what delivers the next thing the cell holds —
            // the bench's line, and then the part that is lying on it.
            if (closed) trig_drain(v);
        } else if (v->exam_npc >= 0) {
            VxNpc *np = &v->npcs[v->exam_npc];
            np->facing = vx_face_pick(-1, v->act[0].x - np->x, v->act[0].z - np->z);   // turn to the player
            np->gx = np->x; np->gz = np->z; np->wait = 2.5f;                           // and stop walking
            vx_say(v, nullptr, np->text);
        } else if (v->exam_trig >= 0) {
            // A PRESS RESOLVES THE WHOLE CELL ONCE, into a queue that then survives the boxes it
            // opens. Rebuilding it every tick instead would re-deliver the message the player has
            // just dismissed, for ever.
            v->exam_qn = trig_collect(v, v->exam_cx, v->exam_cz, v->exam_mask, v->exam_q, VX_EXAM_Q);
            trig_drain(v);
        }
    }

    // One event a tick, oldest first. The queue is what makes this lossless: a pickup reports its
    // text on one tick and the item on the next, and the chapter sees both.
    if (v->evq_n > 0) {
        *ev = v->evq[0];
        for (int i = 1; i < v->evq_n; i++) v->evq[i - 1] = v->evq[i];
        v->evq_n--;
    }

    vxp_end(VXP_TICK);

    // HEADLESS: everything above this line is the simulation — input, movement, NPCs, the fade and
    // the map change, the examine cone, the interact button, the event queue. Everything below is
    // the render and the ImGui overlay. The chapter play-test drives the REAL game through
    // vx_tick, tens of thousands of steps of it, and cannot afford either: the GPU cost would put
    // the run into the tens of minutes, and calling ImGui's draw lists in a blocking loop between
    // one NewFrame and its Render grows them without bound until it falls over.
    //
    // So this is one early return and not a refactor: the simulation the test drives is the same
    // code the player drives, byte for byte, with the picture left undrawn.
    if (v->headless) return;

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
        snprintf(lbl, sizeof(lbl), "%s %.2f,%.2f %s  y%.2f %s  steps %d  %d tris", v->map_name,
                 v->act[0].x, v->act[0].z, FACE_NAME[v->act[0].facing & 3], v->act[0].y,
                 v->airborne ? "AIR" : "", v->steps, v->tris);
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
    // Two measuring switches for the capture path, so the shots the chapter's look is judged on can
    // be taken before src/chapter01.h exists. VOX_LAMP="radius,level" is vx_set_party_lamp;
    // VOX_NIGHTSIGHT is vx_set_night_sight. Neither is a setting anybody is meant to find.
    if ((e = SDL_getenv("VOX_LAMP")) && e[0]) {
        float r = 6.0f, lv = 0.9f;
        sscanf(e, "%f , %f", &r, &lv);
        vx_set_party_lamp(v, r, lv, 1);
    }
    if ((e = SDL_getenv("VOX_NIGHTSIGHT")) && e[0]) vx_set_night_sight(v, (float)atof(e));
    if ((e = SDL_getenv("VOX_CUT")) && e[0]) v->cutaway = atoi(e) ? 1 : 0;
    if ((e = SDL_getenv("VOX_FOG")) && e[0]) v->fog = (float)atof(e);
    if ((e = SDL_getenv("VOX_TILT")) && e[0]) v->tilt = (float)atof(e);
    if ((e = SDL_getenv("VOX_NAV")) && e[0]) { v->nav_view = atoi(e) ? 1 : 0; v->nav_dirty = true; v->nav_verts = 0; }
    if ((e = SDL_getenv("VOX_RADIUS")) && e[0]) { v->agent_r = (float)atof(e); vx_build_nav(v); v->nav_dirty = true; v->nav_verts = 0; }
    if ((e = SDL_getenv("VOX_FACE")) && e[0]) { int f = facing_of(e); for (int i = 0; i < VX_PARTY; i++) v->act[i].facing = f; }
    // The party stands on one point at spawn: lay a straight trail behind the leader so the shot
    // shows everyone, using the same breadcrumbs the followers normally walk.
    {
        VxActor *a = &v->act[0];
        float bx = -(float)DX[a->facing & 3], bz = -(float)DZ[a->facing & 3];
        v->trail_n = 0; v->trail_head = 0; v->trail_s = 0;
        for (int k = 40; k >= 0; k--) {
            float x = a->x + bx * (VX_TRAIL_DS * k), z = a->z + bz * (VX_TRAIL_DS * k);
            if (!vx_nav_at(v, x, z)) continue;
            vx_trail_push(v, x, z, vx_nav_y(v, x, z), false);
        }
        vx_followers(v, 1.0f / 60.0f);
    }
    snprintf(v->cap_out, sizeof(v->cap_out), "%s", out_path);
    v->cap_req = 3;
}

bool vx_capture_done(VoxField *v) {
    if (v->cap_req > 0) { if (--v->cap_req == 0) vx_do_capture(v); return false; }
    return v->cap_ok;
}

VoxField *vx_create() {
    VoxField *v = (VoxField *)calloc(1, sizeof(VoxField));
    snprintf(v->map_name, sizeof(v->map_name), "halm");
    v->exam_trig = v->exam_npc = -1;
    v->want_dir = v->last_axis = -1;
    v->party = 2;
    v->step_len = WALK_STEP;
    // free movement (VOXFIELD_NOTES.md "Movement"); all five ride the reload blob as Dev sliders
    v->agent_r = VX_AGENT_R; v->sp_walk = VX_SP_WALK; v->sp_run = VX_SP_RUN;
    v->jump_apex = VX_JUMP_APEX; v->gravity = VX_GRAVITY;
    v->nav_view = 0;
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
    free(v->nav_buf);
    free(v->cmap_px);
    free(v->spr);
    free(v);
}

// Load every map once, mesh it, and say so. Non-zero means something a capture would have hidden:
// a map that will not build, or a route the tile map has and the voxel world does not.
// ── the colormap identity check ────────────────────────────────────────────────────────────────
// `day` at full light must be the palette, exactly, for every one of the 255 opaque indices. If it
// is not, something between master.hex, colormap.png and the lookup is shifted by a row or a
// column, and the symptom is the one the palette investigation saw: index 24 coming back cyan
// because it was read against column 25, the head of the `sparkle` ramp.
//
// THE ANSWER, 2026-09-21: the engine is NOT shifted. The two shader lookups address texel centres
// (`(idx+0.5)/256.0, (row+0.5)/u_cmaph`) and the CPU lookup indexes `cmap_px` directly, which is
// exact by construction. This check exists so that stays true, and so the next person who sees a
// cyan block has a one-line answer instead of an afternoon.
//
// Also worth recording here: NOTHING IN THIS ENGINE CYCLES COLORMAP COLUMNS. `cmap_px` is written
// once, at load, and never touched again; the old tile field's per-frame column rewrite went with
// the tile field. Water and lamp movement in the voxel world is shader-driven and does not touch a
// palette index, so no face can strobe. Leave it that way until the palette has genuinely reserved
// ranges — the art currently uses 3-28 heavily (Hart's portraits sit 10-14% on `fire_lamp`).
static int vx_colormap_check(VoxField *v) {
    if (!v->cmap_px) { SDL_Log("SELFCHECK vox: colormap — no colormap loaded, nothing to check"); return 0; }
    int day = -1;
    for (int i = 0; i < v->cmap_tables && i < 8; i++) if (!SDL_strcasecmp(v->cmap_tname[i], "day")) day = i;
    if (day < 0) { SDL_Log("SELFCHECK vox: colormap — no `day` table; cannot check the identity row"); return 1; }
    int row = v->cmap_row0[day];                       // level 0 of `day` is full light: the identity
    int bad = 0, first = -1;
    for (int i = 1; i < 256; i++) {
        const unsigned char *p = v->cmap_px + ((size_t)row * 256 + i) * 4;
        if (p[0] == v->pal[i][0] && p[1] == v->pal[i][1] && p[2] == v->pal[i][2]) continue;
        if (first < 0) {
            first = i;
            SDL_Log("SELFCHECK vox: colormap MISMATCH at index %d: palette %02x%02x%02x, day level 0 "
                    "%02x%02x%02x — the colormap, the palette or the lookup is shifted",
                    i, v->pal[i][0], v->pal[i][1], v->pal[i][2], p[0], p[1], p[2]);
        }
        bad++;
    }
    SDL_Log("SELFCHECK vox: colormap %s — `day` level 0 equals master.hex for %d of 255 indices "
            "(texel-centre addressing, no column cycling anywhere in this engine)",
            bad ? "FAIL" : "ok", 255 - bad);
    return bad ? 1 : 0;
}

int vx_selftest(VoxField *v) {
    if (!v->gl_ready) vx_gl_init(v);
    int bad = vx_colormap_check(v);
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

// ───────────────────────── what the chapter script drives ─────────────────────────
// voxfield.h names these and says what they mean. The field knows nothing about flags, goals or
// steps: it is told what the world looks like now, and it reports what the player did.

const char *vx_current_map(VoxField *v) { return v ? v->map_name : ""; }

void vx_goto(VoxField *v, const char *map, int x, int z, const char *facing) {
    if (!v) return;
    if (map && map[0] && strcmp(map, v->map_name)) vx_load_map(v, map);
    // A negative cell means "wherever this map says it starts" — the chapter script's PLAY rows
    // use -1,-1 for every step that does not care, and only name a cell when the story needs a
    // particular doorway. Without this the party is placed at -1,-1 and snapped off the navmesh.
    if (x < 0 || z < 0) { x = v->spawn_x; z = v->spawn_z; if (!facing) facing = "S"; }
    vx_place_party(v, x, z, facing_of(facing ? facing : "S"));
    // Standing inside a trigger after a teleport must not fire it: the party did not walk in.
    for (int i = 0; i < v->trig_count; i++) {
        VxTrig *g = &v->trigs[i];
        g->inside = (v->act[0].tx >= g->x && v->act[0].tz >= g->z &&
                     v->act[0].tx < g->x + g->w && v->act[0].tz < g->z + g->d);
    }
    v->msg[0] = 0; v->msg_who[0] = 0;
    v->fade = 0; v->fade_dir = 0;
}

void vx_set_light(VoxField *v, const char *table, float level) {
    if (!v) return;
    v->light_table = vx_table_by_name(v, table && table[0] ? table : "day");
    v->amb = level < 0 ? 0 : level > 1 ? 1 : level;
    vx_sun_defaults(v);                 // the table's own sun, and shadow_dirty
    if (v->gl_ready && v->built) vx_render_shadow(v);
    SDL_Log("voxfield: light %s %.2f", table ? table : "day", v->amb);
}

void vx_set_party_lamp(VoxField *v, float radius, float level, int on) {
    if (!v) return;
    v->dlamp_on = on ? 1 : 0;
    v->dlamp_r = radius > 0.5f ? radius : 0.5f;
    v->dlamp_level = level < 0 ? 0 : level > 1 ? 1 : level;
    v->dlamp_x = v->act[0].x; v->dlamp_z = v->act[0].z; v->dlamp_y = v->act[0].y + 1.0f;
}

void vx_set_night_sight(VoxField *v, float add) {
    if (!v) return;
    v->ns_add = add < 0 ? 0 : add > 1 ? 1 : add;
}

void vx_set_party(VoxField *v, int count) {
    if (!v) return;
    v->party = count < 1 ? 1 : count > VX_PARTY ? VX_PARTY : count;
}

void vx_disable_trigger(VoxField *v, const char *arg_id) {
    if (!v || !arg_id || !arg_id[0]) return;
    int n = 0;
    for (int i = 0; i < v->trig_count; i++) {
        VxTrig *g = &v->trigs[i];
        if (strcmp(g->arg, arg_id) && strcmp(g->arg2, arg_id)) continue;
        g->off = 1; n++;
    }
    if (!n) SDL_Log("voxfield: vx_disable_trigger(\"%s\") matched nothing on %s", arg_id, v->map_name);
}

// The same switch, both ways and silently — what the chapter's step/flag condition table drives
// (src/chapter01.h, CH_TRIG_COND). It is called for every conditioned trigger on every step change,
// so it must not log: a trigger for another map matching nothing is the normal case, not a warning.
// `taken` is never cleared: a pickup that has been taken stays taken however the window moves.
int vx_set_trigger(VoxField *v, const char *arg_id, int on) {
    if (!v || !arg_id || !arg_id[0]) return 0;
    int n = 0;
    for (int i = 0; i < v->trig_count; i++) {
        VxTrig *g = &v->trigs[i];
        if (strcmp(g->arg, arg_id) && strcmp(g->arg2, arg_id)) continue;
        g->off = on ? 0 : 1;
        // Re-arming a trigger the party is standing inside must not fire it on the spot: `inside`
        // is left set, so it only fires when they walk out and back in. (Walking out of the guild
        // hall and back in is a player's business; a step change is not.)
        n++;
    }
    return n;
}

void vx_say_id(VoxField *v, const char *id) {
    if (!v || !id || !id[0]) return;
    vx_say2(v, nullptr, id, false);     // the chapter's own line: not reported back at it
}

bool vx_busy(VoxField *v) { return v && (v->msg[0] != 0 || v->fade_dir != 0); }

void vx_freeze(VoxField *v, int on) {
    if (!v) return;
    v->frozen = on ? 1 : 0;
    if (v->frozen) { v->move_in_x = v->move_in_z = 0; v->move_mag = 0; v->want_jump = 0; v->jump_buf = 0; }
}

// What is on screen right now, for a test that has to say WHY it is blocked rather than just that
// it is. "" when the box is closed; the fade is reported separately because a map change looks
// exactly like an open box to vx_busy and is a completely different problem.
const char *vx_bot_boxtext(VoxField *v) { return v ? v->msg : ""; }
int vx_bot_fading(VoxField *v) { return v ? v->fade_dir : 0; }
