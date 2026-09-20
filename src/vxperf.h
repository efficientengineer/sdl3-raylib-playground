// vxperf.h — the voxel field's performance instrumentation. Header-only on purpose: it is included
// by voxfield.cpp alone, so nothing in CMakeLists.txt or fast_reload.sh has to change to carry it.
//
// What it gives you:
//   * named CPU scopes (SDL_GetPerformanceCounter) and GPU scopes (GL timer queries, read back three
//     frames late so nothing ever stalls the pipeline),
//   * a wall-clock TOTAL measured tick-entry to tick-entry, which is the honest frame interval
//     including the host's swap — no host.cpp hook needed, so no ./deploy.sh,
//   * frame-time history, 1% worst and max, and a vsync-bucket histogram,
//   * counters (draws, triangles, chunks, sprites, FBO size, GPU memory estimate),
//   * an ImGui HUD in three modes, and the tables the benchmark writes out.
//
// GPU timers: GL_EXT_disjoint_timer_query on GLES, GL_TIME_ELAPSED on desktop GL. Every entry point
// is fetched with SDL_GL_GetProcAddress (EXT suffix first, then core), because the macOS gl3.h does
// not declare glGetQueryObjectui64v and the GLES3 headers do not declare the EXT entry points at all.
// If any of them is missing we fall back to CPU-only and say so in the HUD; nothing else changes.
//
// GL_TIME_ELAPSED queries CANNOT NEST, so the scopes below are kept flat: where one pass has to be
// drawn inside another (the sky, between the sprites' opaque and blended halves), the outer scope is
// CLOSED around it rather than nested. A scope opened while another is still open anyway is timed on
// the CPU only and its GPU column reads "-". A scope may be entered more than once in a frame; see
// VxpScope for how that is accounted.
#pragma once

#include <SDL3/SDL.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

// ───────────────────────── the scope list ─────────────────────────
// Water is not a pass of its own: water voxels are in the chunk meshes and are shaded by the world
// program, so their cost lands in WORLD. Saying so beats inventing a scope that would always read 0.
enum VxpId {
    VXP_TICK = 0,      // walking, NPCs, triggers, input — all the logic in vx_tick
    VXP_MOVE,          // free movement: the navmesh slide, jumps, followers, NPCs (nested in TICK)
    VXP_MESH,          // building and uploading chunk VBOs (load time, not per frame)
    VXP_SHADOW,        // the sun's depth pass (only when the sun moves)
    VXP_SKY,           // the full-screen sky gradient + clouds
    VXP_WORLD,         // the chunk draws: opaque voxels, water included
    VXP_SPRITES,       // character billboards, detail decals, blob shadows, lamp glows
    VXP_POST_DOWN,     // post: the two half-res blur passes
    VXP_POST_BLOOM,    // post: the thresholded second blur
    VXP_POST_COMP,     // post: the composite (tilt-shift, bloom, grade, vignette)
    VXP_UI,            // the ImGui draw lists for the pad, the dialogue box and the HUD
    VXP_COUNT
};

static const char *VXP_NAME[VXP_COUNT] = {
    "tick/logic", "movement", "mesh/upload", "shadow map", "sky", "world opaque",
    "sprites+detail", "post: blur", "post: bloom", "post: composite", "ui/dialogue",
};

#define VXP_HIST 120           // frame-time graph samples
#define VXP_RING 4             // GPU query ring: read back this many frames late, never stalling

// ───────────────────────── GL entry points, fetched at run time ─────────────────────────

#ifndef GL_TIME_ELAPSED_EXT
#define GL_TIME_ELAPSED_EXT 0x88BF
#endif
#ifndef GL_GPU_DISJOINT_EXT
#define GL_GPU_DISJOINT_EXT 0x8FBB
#endif
#ifndef GL_QUERY_RESULT_AVAILABLE_VXP
#define GL_QUERY_RESULT_AVAILABLE_VXP 0x8867
#define GL_QUERY_RESULT_VXP           0x8866
#endif

typedef void (*VxpGenQueries)(GLsizei, GLuint *);
typedef void (*VxpDelQueries)(GLsizei, const GLuint *);
typedef void (*VxpBeginQuery)(GLenum, GLuint);
typedef void (*VxpEndQuery)(GLenum);
typedef void (*VxpGetQueryObjectuiv)(GLuint, GLenum, GLuint *);
typedef void (*VxpGetQueryObjectui64v)(GLuint, GLenum, uint64_t *);

struct VxpCounters {
    int draws, tris, chunks_drawn, chunks_total, sprites, detail;
    int fbo_w, fbo_h, out_w, out_h, shadow_dim;
    double tex_mb, vbo_mb;
};

// A scope may be ENTERED MORE THAN ONCE in a frame — sprites+detail is, because the sky is drawn
// between its opaque and its blended halves. So the CPU time is summed into a per-frame accumulator
// and folded into the average once a frame, not once an entry; otherwise the average would be the
// average of a half-frame. There is one query object a scope a ring slot, so only the FIRST entry in
// a frame takes a GPU query: the sprites' GPU figure is its opaque half, and that is said out loud.
struct VxpScope {
    double cpu_ms, gpu_ms;          // the last frame's numbers
    double cpu_avg, gpu_avg;        // a 1 s rolling average, which is what the HUD and reports show
    double cpu_frame;               // this frame's total so far, across every entry
    double cpu_acc, gpu_acc;
    int acc_n, gpu_acc_n;
    uint64_t t0;
    bool open, gpu_open, gpu_taken;
};

struct VxpState {
    bool inited;
    // GPU timers
    VxpGenQueries gen; VxpDelQueries del; VxpBeginQuery beginq; VxpEndQuery endq;
    VxpGetQueryObjectuiv getuiv; VxpGetQueryObjectui64v getui64v;
    bool gpu_ok;                    // the entry points loaded
    bool gpu_proven;                // ...and actually returned a number
    char gpu_note[96];
    GLuint q[VXP_COUNT][VXP_RING];
    bool q_used[VXP_COUNT][VXP_RING];
    int ring;                       // the slot this frame writes
    int gpu_active;                 // the scope id holding the one TIME_ELAPSED query, or -1

    VxpScope s[VXP_COUNT];
    VxpCounters c, c_shown;

    // GPU timers are only ARMED when somebody is looking (the HUD, the benchmark, or VXPERF_GPU=1).
    // Left on all the time they cost a glGetIntegerv(GL_GPU_DISJOINT_EXT) and VXP_COUNT
    // glGetQueryObjectuiv a frame; on Adreno a glGet* can flush the command stream, so the
    // instrumentation would be measuring itself. CPU scopes are two SDL_GetPerformanceCounter calls
    // and stay on always — they are what the spike recorder needs.
    bool gpu_want;

    // wall clock
    uint64_t last_tick;             // tick entry to tick entry: the real frame interval
    double freq;
    double hist[VXP_HIST];
    int hist_n, hist_head;
    double frame_ms, frame_avg, frame_p99, frame_max, fps;
    double acc_sum; int acc_n; uint64_t acc_t0;
    double acc_max;
    uint64_t frames;

    // the spike recorder: one line naming every scope's cost in the frame that went long
    double spike_ms;                // log a frame that took longer than this (0 = off)
    uint64_t spike_last;            // rate limit, in performance counter ticks
    int spike_n;                    // how many have been seen since the last reset

    // vsync buckets: how many intervals land near 1x, 2x, 3x the display period, and how many are
    // off the grid entirely. That is the frame-pacing check.
    double refresh_hz;
    int bucket[5];                  // 1x, 2x, 3x, 4x+, off-grid
    int bucket_total;
};

static VxpState vxp;

static inline void vxp_reset_averages(void) {
    for (int i = 0; i < VXP_COUNT; i++) {
        vxp.s[i].cpu_acc = vxp.s[i].gpu_acc = 0;
        vxp.s[i].acc_n = vxp.s[i].gpu_acc_n = 0;
    }
    vxp.acc_sum = 0; vxp.acc_n = 0; vxp.acc_max = 0;
    vxp.acc_t0 = SDL_GetPerformanceCounter();
    for (int i = 0; i < 5; i++) vxp.bucket[i] = 0;
    vxp.bucket_total = 0;
}

static void vxp_init(void) {
    if (vxp.inited) return;
    memset(&vxp, 0, sizeof(vxp));
    vxp.inited = true;
    vxp.freq = (double)SDL_GetPerformanceFrequency();
    vxp.gpu_active = -1;
    vxp.refresh_hz = 60.0;
    vxp.spike_ms = 25.0;
    {   // VXPERF_GPU=1 arms the timer queries from the first frame, for a run with no HUD open
        const char *e = SDL_getenv("VXPERF_GPU");
        vxp.gpu_want = e && e[0] && e[0] != '0';
    }

    // EXT first (that is what GLES exposes), then the core names (desktop GL 3.3).
    vxp.gen      = (VxpGenQueries)SDL_GL_GetProcAddress("glGenQueriesEXT");
    vxp.del      = (VxpDelQueries)SDL_GL_GetProcAddress("glDeleteQueriesEXT");
    vxp.beginq   = (VxpBeginQuery)SDL_GL_GetProcAddress("glBeginQueryEXT");
    vxp.endq     = (VxpEndQuery)SDL_GL_GetProcAddress("glEndQueryEXT");
    vxp.getuiv   = (VxpGetQueryObjectuiv)SDL_GL_GetProcAddress("glGetQueryObjectuivEXT");
    vxp.getui64v = (VxpGetQueryObjectui64v)SDL_GL_GetProcAddress("glGetQueryObjectui64vEXT");
    if (!vxp.gen || !vxp.beginq || !vxp.endq || !vxp.getuiv || !vxp.getui64v) {
        vxp.gen      = (VxpGenQueries)SDL_GL_GetProcAddress("glGenQueries");
        vxp.del      = (VxpDelQueries)SDL_GL_GetProcAddress("glDeleteQueries");
        vxp.beginq   = (VxpBeginQuery)SDL_GL_GetProcAddress("glBeginQuery");
        vxp.endq     = (VxpEndQuery)SDL_GL_GetProcAddress("glEndQuery");
        vxp.getuiv   = (VxpGetQueryObjectuiv)SDL_GL_GetProcAddress("glGetQueryObjectuiv");
        vxp.getui64v = (VxpGetQueryObjectui64v)SDL_GL_GetProcAddress("glGetQueryObjectui64v");
    }
    vxp.gpu_ok = vxp.gen && vxp.beginq && vxp.endq && vxp.getuiv && vxp.getui64v;
    if (vxp.gpu_ok) {
        for (int i = 0; i < VXP_COUNT; i++) vxp.gen(VXP_RING, vxp.q[i]);
        snprintf(vxp.gpu_note, sizeof vxp.gpu_note, "gpu timers: waiting for first result");
    } else {
        snprintf(vxp.gpu_note, sizeof vxp.gpu_note, "gpu timers unavailable (no timer query) — CPU only");
    }
    vxp_reset_averages();
    vxp.last_tick = 0;
    SDL_Log("vxperf: %s", vxp.gpu_note);
}

// ───────────────────────── scopes ─────────────────────────

static inline void vxp_begin(int id) {
    if (!vxp.inited || id < 0 || id >= VXP_COUNT) return;
    VxpScope *s = &vxp.s[id];
    s->t0 = SDL_GetPerformanceCounter();
    s->open = true;
    s->gpu_open = false;
    if (vxp.gpu_ok && vxp.gpu_want && vxp.gpu_active < 0 && !s->gpu_taken) {
        s->gpu_taken = true;
        vxp.gpu_active = id;
        s->gpu_open = true;
        vxp.q_used[id][vxp.ring] = true;
        vxp.beginq(GL_TIME_ELAPSED_EXT, vxp.q[id][vxp.ring]);
    }
}

static inline void vxp_end(int id) {
    if (!vxp.inited || id < 0 || id >= VXP_COUNT) return;
    VxpScope *s = &vxp.s[id];
    if (!s->open) return;
    s->open = false;
    if (s->gpu_open) {
        vxp.endq(GL_TIME_ELAPSED_EXT);
        vxp.gpu_active = -1;
        s->gpu_open = false;
    }
    s->cpu_frame += (double)(SDL_GetPerformanceCounter() - s->t0) / vxp.freq * 1000.0;
}

// RAII, so an early return inside a pass can never leave a query open.
struct VxpZone {
    int id;
    VxpZone(int i) : id(i) { vxp_begin(i); }
    ~VxpZone() { vxp_end(id); }
};
#define VXP_ZONE(id) VxpZone vxp_zone_##id(id)

// ───────────────────────── the frame ─────────────────────────

// Read back the ring slot we are about to overwrite. It is VXP_RING-1 frames old, so the result is
// always ready and glGetQueryObject never blocks; we check AVAILABLE anyway and skip if it is not.
static void vxp_collect(void) {
    if (!vxp.gpu_ok) return;
    int slot = (vxp.ring + 1) % VXP_RING;
    // Nothing was queried into this slot — take the glGet path only when there is something to read.
    // That is what keeps the instrumentation free while the HUD is closed.
    bool any = false;
    for (int i = 0; i < VXP_COUNT; i++) if (vxp.q_used[i][slot]) { any = true; break; }
    if (!any) return;
    GLint disjoint = 0;
    glGetIntegerv(GL_GPU_DISJOINT_EXT, &disjoint);
    for (int i = 0; i < VXP_COUNT; i++) {
        if (!vxp.q_used[i][slot]) continue;
        vxp.q_used[i][slot] = false;
        GLuint avail = 0;
        vxp.getuiv(vxp.q[i][slot], GL_QUERY_RESULT_AVAILABLE_VXP, &avail);
        if (!avail) continue;                    // never stall: just miss this sample
        uint64_t ns = 0;
        vxp.getui64v(vxp.q[i][slot], GL_QUERY_RESULT_VXP, &ns);
        if (disjoint) continue;                  // the GPU was interrupted; the number is nonsense
        double ms = (double)ns / 1e6;
        vxp.s[i].gpu_ms = ms;
        vxp.s[i].gpu_acc += ms;
        vxp.s[i].gpu_acc_n++;
        if (!vxp.gpu_proven && ns > 0) {
            vxp.gpu_proven = true;
            snprintf(vxp.gpu_note, sizeof vxp.gpu_note, "gpu timers: ok");
        }
    }
    if (disjoint) {
        // Clearing it is the driver's job on read; the flag being set at all is worth one note.
        snprintf(vxp.gpu_note, sizeof vxp.gpu_note, "gpu timers: ok (disjoint seen — samples dropped)");
    }
}

static void vxp_frame_begin(double refresh_hz) {
    if (!vxp.inited) return;
    if (refresh_hz > 20.0 && refresh_hz < 400.0) vxp.refresh_hz = refresh_hz;
    uint64_t now = SDL_GetPerformanceCounter();
    if (vxp.last_tick) {
        double ms = (double)(now - vxp.last_tick) / vxp.freq * 1000.0;
        if (ms > 0.0 && ms < 2000.0) {
            vxp.frame_ms = ms;
            vxp.hist[vxp.hist_head] = ms;
            vxp.hist_head = (vxp.hist_head + 1) % VXP_HIST;
            if (vxp.hist_n < VXP_HIST) vxp.hist_n++;
            vxp.acc_sum += ms; vxp.acc_n++;
            if (ms > vxp.acc_max) vxp.acc_max = ms;
            // vsync buckets
            double period = 1000.0 / vxp.refresh_hz;
            double k = ms / period;
            int b = 4;
            for (int m = 1; m <= 4; m++) if (fabs(k - (double)m) < 0.35) { b = m - 1; break; }
            if (b == 4 && k > 4.0) b = 3;
            vxp.bucket[b]++; vxp.bucket_total++;
        }
    }
    vxp.last_tick = now;
    vxp.frames++;

    for (int i = 0; i < VXP_COUNT; i++) {
        VxpScope *s = &vxp.s[i];
        // cpu_ms is THIS frame's, zero included: the spike recorder below reads it, and a stale value
        // (mesh/upload's 70 ms from load time) would be read as a cause forever after.
        s->cpu_ms = s->cpu_frame;
        if (s->cpu_frame > 0.0) { s->cpu_acc += s->cpu_frame; s->acc_n++; }
        s->cpu_frame = 0.0;
        s->gpu_taken = false;
    }

    // ── the spike recorder ──
    // The interval just measured is the PREVIOUS frame's, and the per-scope cpu_ms folded in above is
    // that same frame's, so the two line up. One line names every scope, which is the whole point:
    // a 40 ms frame whose scopes add up to 3 ms was not the game's doing (scheduler, GPU queue,
    // another process); one whose "tick/logic" reads 20 ms is. Rate-limited to one a second so a bad
    // stretch cannot turn into logcat spam — and the count says how many were swallowed.
    if (vxp.spike_ms > 0.0 && vxp.frame_ms > vxp.spike_ms && vxp.frames > 10) {
        vxp.spike_n++;
        double since_log = vxp.spike_last ? (double)(now - vxp.spike_last) / vxp.freq : 1e9;
        if (since_log >= 1.0) {
            vxp.spike_last = now;
            char line[512];
            int p = snprintf(line, sizeof line, "VXSPIKE %.1f ms (frame %llu, %d since last)",
                             vxp.frame_ms, (unsigned long long)vxp.frames, vxp.spike_n);
            double sum = 0;
            for (int i = 0; i < VXP_COUNT && p > 0 && p < (int)sizeof line - 1; i++) {
                sum += vxp.s[i].cpu_ms;
                p += snprintf(line + p, sizeof line - (size_t)p, "  %s=%.2f", VXP_NAME[i], vxp.s[i].cpu_ms);
            }
            if (p > 0 && p < (int)sizeof line - 1)
                snprintf(line + p, sizeof line - (size_t)p, "  | scopes=%.2f unaccounted=%.2f",
                         sum, vxp.frame_ms - sum);
            SDL_Log("%s", line);
            vxp.spike_n = 0;
        }
    }

    vxp.ring = (int)(vxp.frames % VXP_RING);
    vxp_collect();
    vxp.c.draws = vxp.c.tris = vxp.c.sprites = vxp.c.detail = vxp.c.chunks_drawn = 0;

    // roll the 1 s averages
    double since = (double)(now - vxp.acc_t0) / vxp.freq;
    if (since >= 1.0 && vxp.acc_n > 0) {
        vxp.frame_avg = vxp.acc_sum / vxp.acc_n;
        vxp.fps = vxp.acc_n / since;
        vxp.frame_max = vxp.acc_max;
        // 1% worst over the visible history
        double sorted[VXP_HIST];
        int n = vxp.hist_n;
        for (int i = 0; i < n; i++) sorted[i] = vxp.hist[i];
        for (int i = 1; i < n; i++) { double v = sorted[i]; int j = i - 1;
            while (j >= 0 && sorted[j] < v) { sorted[j + 1] = sorted[j]; j--; } sorted[j + 1] = v; }
        int k = n / 100; if (k >= n) k = n ? n - 1 : 0;
        vxp.frame_p99 = n ? sorted[k] : 0.0;
        for (int i = 0; i < VXP_COUNT; i++) {
            vxp.s[i].cpu_avg = vxp.s[i].acc_n ? vxp.s[i].cpu_acc / vxp.s[i].acc_n : 0.0;
            vxp.s[i].gpu_avg = vxp.s[i].gpu_acc_n ? vxp.s[i].gpu_acc / vxp.s[i].gpu_acc_n : 0.0;
        }
        vxp.c_shown = vxp.c;
        vxp_reset_averages();
    }
}

static inline void vxp_draw(int tris) { vxp.c.draws++; vxp.c.tris += tris; }

// Arm or disarm the GPU timer queries. Call it once a frame with (hud open || benchmark running).
// Disarming leaves the CPU scopes and the spike recorder alone; those cost nothing measurable.
static inline void vxp_want_gpu(bool on) {
    if (vxp.gpu_want == on) return;
    vxp.gpu_want = on;
    if (!on) for (int i = 0; i < VXP_COUNT; i++) { vxp.s[i].gpu_ms = vxp.s[i].gpu_avg = 0; }
}

static inline double vxp_gpu_total(void) {
    double t = 0;
    for (int i = VXP_SHADOW; i <= VXP_UI; i++) t += vxp.s[i].gpu_avg;
    return t;
}
static inline double vxp_cpu_total(void) {
    double t = 0;
    for (int i = 0; i < VXP_COUNT; i++) if (i != VXP_MESH) t += vxp.s[i].cpu_avg;
    return t;
}

// ───────────────────────── the HUD ─────────────────────────
// Modes: 0 off, 1 compact, 2 full. It must be cheap: everything below is drawn into the foreground
// draw list, with no windows, no tables and no per-frame string building beyond one snprintf a line.

static void vxp_hud(int mode, int w, int h) {
    if (!vxp.inited || mode <= 0) return;
    ImDrawList *dl = ImGui::GetForegroundDrawList();
    ImFont *font = ImGui::GetFont();
    float ts = (float)h * 0.030f;
    if (ts < 11.0f) ts = 11.0f;
    float line = ts * 1.22f;
    float pad = ts * 0.45f;
    float gw = ts * 12.0f, gh = ts * 3.0f;          // the graph
    int rows = (mode >= 2) ? (4 + VXP_COUNT + 6) : 3;
    float bw = (mode >= 2) ? ts * 22.0f : gw + pad * 2;
    float bh = rows * line + gh + pad * 3;
    float x0 = (float)w - bw - ts * 0.6f, y0 = ts * 0.6f;

    dl->AddRectFilled(ImVec2(x0, y0), ImVec2(x0 + bw, y0 + bh), IM_COL32(6, 8, 14, 190), ts * 0.3f);
    dl->AddRect(ImVec2(x0, y0), ImVec2(x0 + bw, y0 + bh), IM_COL32(120, 140, 190, 120), ts * 0.3f);

    float x = x0 + pad, y = y0 + pad;
    char b[160];
    ImU32 good = IM_COL32(150, 240, 160, 255), warn = IM_COL32(250, 220, 130, 255), bad = IM_COL32(250, 140, 130, 255);
    ImU32 dim = IM_COL32(180, 190, 210, 235);

    ImU32 fc = vxp.fps >= 55 ? good : (vxp.fps >= 30 ? warn : bad);
    snprintf(b, sizeof b, "%.0f fps  %.1f ms", vxp.fps, vxp.frame_avg);
    dl->AddText(font, ts, ImVec2(x, y), fc, b); y += line;
    snprintf(b, sizeof b, "1%%w %.1f  max %.1f", vxp.frame_p99, vxp.frame_max);
    dl->AddText(font, ts * 0.9f, ImVec2(x, y), dim, b); y += line;
    snprintf(b, sizeof b, "cpu %.1f  gpu %.1f", vxp_cpu_total(), vxp_gpu_total());
    dl->AddText(font, ts * 0.9f, ImVec2(x, y), dim, b); y += line;

    // the graph: 120 samples, with the 16.7 and 33.3 ms lines
    float gx = x, gy = y, gxe = x + (mode >= 2 ? bw - pad * 2 : gw);
    dl->AddRectFilled(ImVec2(gx, gy), ImVec2(gxe, gy + gh), IM_COL32(0, 0, 0, 120));
    float scale = gh / 50.0f;                        // 50 ms full height
    for (int m = 0; m < 2; m++) {
        float ms = m ? 33.3f : 16.7f;
        float ly = gy + gh - ms * scale;
        dl->AddLine(ImVec2(gx, ly), ImVec2(gxe, ly), m ? IM_COL32(200, 120, 90, 150) : IM_COL32(110, 200, 130, 150), 1.0f);
    }
    float step = (gxe - gx) / (float)VXP_HIST;
    for (int i = 0; i < vxp.hist_n; i++) {
        int idx = (vxp.hist_head - vxp.hist_n + i + VXP_HIST * 2) % VXP_HIST;
        double v = vxp.hist[idx];
        float bhh = (float)v * scale; if (bhh > gh) bhh = gh;
        ImU32 col = v <= 17.5 ? good : (v <= 34.0 ? warn : bad);
        dl->AddRectFilled(ImVec2(gx + i * step, gy + gh - bhh), ImVec2(gx + (i + 1) * step - 0.5f, gy + gh), col);
    }
    y = gy + gh + pad;

    if (mode < 2) return;

    // per-scope table, sorted by cost (GPU where we have it, CPU otherwise)
    int order[VXP_COUNT];
    for (int i = 0; i < VXP_COUNT; i++) order[i] = i;
    for (int i = 1; i < VXP_COUNT; i++) {
        int v = order[i]; int j = i - 1;
        double kv = vxp.s[v].gpu_avg > 0 ? vxp.s[v].gpu_avg : vxp.s[v].cpu_avg;
        while (j >= 0) {
            double kj = vxp.s[order[j]].gpu_avg > 0 ? vxp.s[order[j]].gpu_avg : vxp.s[order[j]].cpu_avg;
            if (kj >= kv) break;
            order[j + 1] = order[j]; j--;
        }
        order[j + 1] = v;
    }
    dl->AddText(font, ts * 0.82f, ImVec2(x, y), IM_COL32(150, 170, 220, 230), "scope            cpu    gpu"); y += line * 0.95f;
    for (int k = 0; k < VXP_COUNT; k++) {
        int i = order[k];
        if (i == VXP_MESH && vxp.s[i].cpu_avg <= 0.0) continue;
        if (vxp.s[i].gpu_avg > 0.0)
            snprintf(b, sizeof b, "%-15s %5.2f  %5.2f", VXP_NAME[i], vxp.s[i].cpu_avg, vxp.s[i].gpu_avg);
        else
            snprintf(b, sizeof b, "%-15s %5.2f      -", VXP_NAME[i], vxp.s[i].cpu_avg);
        dl->AddText(font, ts * 0.82f, ImVec2(x, y), dim, b); y += line * 0.95f;
    }
    y += pad * 0.5f;
    VxpCounters *c = &vxp.c_shown;
    snprintf(b, sizeof b, "draws %d  tris %d", c->draws, c->tris);
    dl->AddText(font, ts * 0.82f, ImVec2(x, y), dim, b); y += line * 0.95f;
    snprintf(b, sizeof b, "chunks %d/%d  sprites %d", c->chunks_drawn, c->chunks_total, c->sprites);
    dl->AddText(font, ts * 0.82f, ImVec2(x, y), dim, b); y += line * 0.95f;
    snprintf(b, sizeof b, "fbo %dx%d  shadow %d", c->fbo_w, c->fbo_h, c->shadow_dim);
    dl->AddText(font, ts * 0.82f, ImVec2(x, y), dim, b); y += line * 0.95f;
    snprintf(b, sizeof b, "gpu mem ~%.1f MB (%.1f tex + %.1f vbo)", c->tex_mb + c->vbo_mb, c->tex_mb, c->vbo_mb);
    dl->AddText(font, ts * 0.82f, ImVec2(x, y), dim, b); y += line * 0.95f;
    int swap = 0; SDL_GL_GetSwapInterval(&swap);
    snprintf(b, sizeof b, "%.0f Hz  swap %d  vsync 1x %d%%  off %d%%", vxp.refresh_hz, swap,
             vxp.bucket_total ? vxp.bucket[0] * 100 / vxp.bucket_total : 0,
             vxp.bucket_total ? vxp.bucket[4] * 100 / vxp.bucket_total : 0);
    dl->AddText(font, ts * 0.82f, ImVec2(x, y), dim, b); y += line * 0.95f;
    dl->AddText(font, ts * 0.78f, ImVec2(x, y), IM_COL32(160, 160, 180, 200), vxp.gpu_note);
}
