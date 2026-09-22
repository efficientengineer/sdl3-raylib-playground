// vox_dev.cpp — the Dev panel and the A/B benchmark. Nothing here ships a behaviour.
// OWNS: vx_dev_ui's sliders and buttons, the benchmark state machine and its report, the
//       perf.flag poll, the refresh-rate and texture-memory readouts.
// NEVER: is called from the render or movement path in normal play. Every knob it writes is one
//        the shipping default already set; turning the panel off must change nothing.
// EXPOSES: vx_dev_ui, vx_bench_frame, vx_poll_perf_flag, vx_refresh_hz, vx_tex_mb.
// Tested by: nothing directly — it must not break the build or the reload blob. See src/notes/testing.md.
#include "vox_internal.h"

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

const char *VXB_NAME[VXB_COUNT] = {
    "baseline", "hd2d off", "dof off", "bloom off", "grade+vignette off", "shadows off",
    "shadow pcf 1", "pattern detail off", "ao off", "cutaway off", "detail sprites off",
    "water anim off", "sky clouds off", "fog off",
    "res 100%", "res 85%", "res 75%", "res 66%", "res 50%", "everything off",
};

// The two parked views. A map with no entry uses its own spawn for both.
static const struct { const char *map; int ax, az; const char *an; int bx, bz; const char *bn; }
VXB_VIEWS[] = { { "halm", 21, 21, "square", 27, 7, "stream" } };

void vx_place_party(VoxField *v, int x, int z, int f);   // defined above; used by the bench

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
void vx_bench_frame(VoxField *v) {
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
void vx_poll_perf_flag(VoxField *v, float dt) {
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
double vx_refresh_hz(void) {
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
double vx_tex_mb(VoxField *v, int rw, int rh) {
    double b = 0;
    b += (double)rw * rh * 8.0;                                  // scene colour + depth24
    b += (double)v->half_w * v->half_h * 4.0 * 3.0;              // the three half-res targets
    b += (double)rw * rh * 4.0;                                  // the post output
    b += (double)v->shadow_dim * v->shadow_dim * 4.0;            // depth24
    b += (double)v->atlas_w * v->atlas_h;                        // R8
    for (int i = 0; i < v->decal_count; i++) b += (double)v->decal_w[i] * v->decal_h[i];
    return b / (1024.0 * 1024.0);
}

// ───────────────────────── dev, save, lifecycle ─────────────────────────

bool vx_dev_ui(VoxField *v, char *out, int cap) {
    bool printed = false;
    ImGui::Text("vox field — %s  %.2f,%.2f %s  y%.2f%s  steps %d  (%dx%d, %d tris)", v->map_name,
                v->act[0].x, v->act[0].z, FACE_NAME[v->act[0].facing & 3], v->act[0].y,
                v->airborne ? " AIR" : "", v->steps, v->mw, v->md, v->tris);
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
    // ── movement (VOXFIELD_NOTES.md "Movement") ──
    ImGui::Separator();
    { bool nv = v->nav_view != 0;
      if (ImGui::Checkbox("Nav view", &nv)) { v->nav_view = nv; v->nav_dirty = true; } }
    ImGui::SameLine();
    ImGui::Text("walkable %s | jump-only %d vox in %d region(s) | nav %.1f ms",
                v->nav_bad ? "UNREACHABLE TARGETS" : "ok", v->nav_jump_vox,
                v->nav_regions > 1 ? v->nav_regions - 1 : 0, v->nav_ms);
    // Field sprites: which ids this map stands on the ground, and which of them are still
    // placeholders. This is the "id label in Dev mode" — in the panel, not painted into the world,
    // where a 3D text label would cost a font atlas and a pass of its own for a debugging aid.
    if (v->sprart_count) {
        char line[512];
        int n = 0;
        line[0] = 0;
        for (int i = 0; i < v->sprart_count; i++)
            n += snprintf(line + n, sizeof(line) - (size_t)n, "%s%s%s", n ? "  " : "",
                          v->sprart[i].id, v->sprart[i].tex ? "" : "*");
        ImGui::Text("sprites: %s   (* = placeholder, no story/field/sprites/<id>.png yet)", line);
    }
    { float ns = v->ns_add;
      ImGui::SetNextItemWidth(ImGui::GetFontSize() * 8);
      if (ImGui::SliderFloat("night sight", &ns, 0.0f, 0.6f, "%.2f")) vx_set_night_sight(v, ns);
      ImGui::SameLine();
      bool lamp = v->dlamp_on != 0;
      if (ImGui::Checkbox("party lamp", &lamp)) vx_set_party_lamp(v, v->dlamp_r > 0 ? v->dlamp_r : 6.0f,
                                                                  v->dlamp_level > 0 ? v->dlamp_level : 0.9f, lamp); }
    ImGui::SetNextItemWidth(ImGui::GetFontSize() * 8);
    ImGui::SliderFloat("walk", &v->sp_walk, 1.0f, 12.0f, "%.1f");
    ImGui::SameLine();
    ImGui::SetNextItemWidth(ImGui::GetFontSize() * 8);
    ImGui::SliderFloat("run", &v->sp_run, 1.0f, 18.0f, "%.1f");
    ImGui::SetNextItemWidth(ImGui::GetFontSize() * 8);
    ImGui::SliderFloat("jump apex", &v->jump_apex, 0.2f, 3.0f, "%.2f");
    ImGui::SameLine();
    ImGui::SetNextItemWidth(ImGui::GetFontSize() * 8);
    ImGui::SliderFloat("gravity", &v->gravity, 8.0f, 120.0f, "%.0f");
    ImGui::SameLine();
    ImGui::SetNextItemWidth(ImGui::GetFontSize() * 8);
    // The radius is read live by the collision; the nav data is only re-derived (and the reach
    // re-proved) when the slider is LET GO, because that pass is milliseconds, not microseconds.
    ImGui::SliderFloat("radius", &v->agent_r, 0.05f, 0.45f, "%.2f");
    if (ImGui::IsItemDeactivatedAfterEdit()) { vx_build_nav(v); v->nav_dirty = true; v->nav_verts = 0; }
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
        int ix = (int)floorf(v->act[0].x / VOX_S), iz = (int)floorf(v->act[0].z / VOX_S);
        if (ix < 0) ix = 0; if (iz < 0) iz = 0;
        if (ix >= v->vw) ix = v->vw - 1; if (iz >= v->vd) iz = v->vd - 1;
        unsigned char top = get_blk(v, x * VX_VPC, v->hgt[z][x], z * VX_VPC);
        snprintf(out, cap, "%s: %d,%d height %d, top block %s, %s — nav %s region %d clearance %.2f, "
                 "%d tris, mesh %.1f ms, nav %.1f ms, reach %s/%s",
                 v->map_name, x, z, v->hgt[z][x], BLOCKS[top].name, v->walk[z][x] ? "walkable" : "blocked",
                 v->nav_ok[iz][ix] ? "ok" : "blocked", v->nav_reg[iz][ix], v->nav_clr[iz][ix] / 32.0f,
                 v->tris, v->mesh_ms, v->nav_ms,
                 v->reach_missing ? "FAIL" : "ok", v->nav_bad ? "FAIL" : "ok");
        printed = true;
    }
    return printed;
}

