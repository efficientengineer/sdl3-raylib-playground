// vox_render.cpp — GL objects, the camera, the sun, and every draw the field makes.
// OWNS: program compilation and the uniform-location cache, the FBO chain, the matrices, the
//       shadow map, frustum culling, the cutaway, the sprite batches, the sky, the HD-2D post pass,
//       the overdraw probe, the Nav overlay, the touch pad and the message box.
// NEVER: changes game state. Nothing in here moves the party, fires a trigger or loads a map.
// EXPOSES: vx_gl_init, vx_fbo_size, vx_render, vx_post, vx_render_shadow, vx_sun_defaults,
//          vx_measure_overdraw, vx_overdraw_pass, draw_touch_ui, draw_msg_box, vx_poll_map_flag.
// Tested by: the three image captures (capture.sh --vox ...) byte for byte. See src/notes/rendering.md.
#include "vox_internal.h"
#include "vox_shaders.h"

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

void vx_gl_init(VoxField *v) {
    vx_probe_limits();
    vx_uni_reset();          // fresh programs may be handed recycled ids: the cache must not survive
    {   // the world, twice: without the cutaway's discard (early-Z lives) and with it
        const char *plain[2] = { VX_FS_HEAD, VX_FS_BODY };
        const char *cut[3]   = { VX_FS_HEAD, VX_FS_CUT, VX_FS_BODY };
        v->prog     = vx_program_n(VX_VS, plain, 2, "world");
        v->prog_cut = vx_program_n(VX_VS, cut,   3, "world cutaway");
    }
    v->od_prog = vx_program(VX_VS, VX_OD_FS, "overdraw");
    v->nav_prog = vx_program(VX_NAV_VS, VX_NAV_FS, "nav view");
    glGenVertexArrays(1, &v->nav_vao);
    glGenBuffers(1, &v->nav_vbo);
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

void vx_fbo_size(VoxField *v, int w, int h) {
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
    VxActor *a = &v->act[0];
    float px = a->x, pz = a->z;
    // The camera follows the GROUND-projected position with a softened height: a jump is read from
    // the blob shadow and the sprite, not from the whole town moving down the screen. The old 3D
    // field's third reported bug was the camera losing the player, so it never leads or lags in x/z.
    float gy = vx_nav_y(v, px, pz);
    if (!v->cam_y_init) { v->cam_y = gy; v->cam_y_init = true; }
    float dtc = 1.0f / 60.0f;
    v->cam_y += (gy - v->cam_y) * (dtc * 6.0f > 1 ? 1 : dtc * 6.0f);
    if (v->cam_y < a->y - 6.0f) v->cam_y = a->y - 6.0f;          // never lose the party off the top
    if (v->cam_y > a->y + 6.0f) v->cam_y = a->y + 6.0f;
    float py = v->cam_y;
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
    // Night sight (vx_set_night_sight) lifts the EFFECTIVE ambient, so it reaches the world, the
    // sprites and the lamp mix in one place rather than being applied three times slightly wrong.
    float amb = v->amb + v->ns_add;
    if (amb > 1) amb = 1;
    if (amb < 0) amb = 0;
    glUniform1f(vx_uni(prog, "u_amb"), amb);
    glUniform4f(vx_uni(prog, "u_dlamp"), v->dlamp_x, v->dlamp_y, v->dlamp_z, v->dlamp_r);
    glUniform1f(vx_uni(prog, "u_dlev"), v->dlamp_on ? v->dlamp_level : 0.0f);
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

void vx_sun_defaults(VoxField *v) {
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

void vx_render_shadow(VoxField *v) {
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
// ───────────────────────── the Nav view (Dev toggle) ─────────────────────────
// What the movement actually runs on, drawn over the world: the walkable region a translucent
// green, jump-only regions magenta, every LEDGE edge an orange bar, the body's radius circle, and
// the breadcrumb trail the followers walk. `capture.sh --vox <map> --nav 1` renders it into a
// capture so the buffer from a wall can be looked at on the Mac.

static void nav_vert(VoxField *v, float x, float y, float z, const float c[4]) {
    if (v->nav_verts + 1 > v->nav_cap) return;
    float *p = v->nav_buf + (size_t)v->nav_verts * 7;
    p[0] = x; p[1] = y; p[2] = z; p[3] = c[0]; p[4] = c[1]; p[5] = c[2]; p[6] = c[3];
    v->nav_verts++;
}
static void nav_quad(VoxField *v, float x0, float z0, float x1, float z1, float y, const float c[4]) {
    nav_vert(v, x0, y, z0, c); nav_vert(v, x1, y, z0, c); nav_vert(v, x1, y, z1, c);
    nav_vert(v, x0, y, z0, c); nav_vert(v, x1, y, z1, c); nav_vert(v, x0, y, z1, c);
}

static void vx_nav_build_overlay(VoxField *v) {
    if (!v->nav_buf) {
        v->nav_cap = 420000;
        v->nav_buf = (float *)malloc((size_t)v->nav_cap * 7 * sizeof(float));
        if (!v->nav_buf) { v->nav_cap = 0; return; }
    }
    v->nav_verts = 0;
    // Blue, not green: most of this world IS green grass, and a green overlay on it says nothing.
    const float GREEN[4]   = { 0.25f, 0.58f, 1.00f, 0.34f };
    const float JUMPONLY[4]= { 0.95f, 0.30f, 0.90f, 0.40f };
    const float LEDGE[4]   = { 1.00f, 0.52f, 0.10f, 0.85f };
    const float EPS = 0.035f, BAR = 0.06f;
    // The green is the ERODED region — where the body's CENTRE may be — so a side that faces a wall,
    // a fence, water or a step up is pulled back by the radius. That gap is the buffer, and looking
    // at it in a capture is how the old field's clipping bug is checked for.
    float r = v->agent_r;
    for (int j = 0; j < v->vd; j++) for (int i = 0; i < v->vw; i++) {
        if (!v->nav_ok[j][i]) continue;
        float y = v->nav_h[j][i] + EPS;
        float e0 = i * VOX_S, f0 = j * VOX_S, e1 = e0 + VOX_S, f1 = f0 + VOX_S;
        float x0 = e0, z0 = f0, x1 = e1, z1 = f1;
        bool ledge[4];
        for (int d = 0; d < 4; d++) {
            int ni = i + DX[d], nj = j + DZ[d];
            if (ni < 0 || nj < 0 || ni >= v->vw || nj >= v->vd || !v->nav_ok[nj][ni]) ledge[d] = true;
            else { float dh = v->nav_h[nj][ni] - v->nav_h[j][i]; ledge[d] = dh > VX_STEP_UP || dh < -VX_STEP_UP; }
            if (!ledge[d]) continue;
            if (DX[d] > 0) x1 -= r; else if (DX[d] < 0) x0 += r;
            if (DZ[d] > 0) z1 -= r; else if (DZ[d] < 0) z0 += r;
        }
        if (x1 > x0 && z1 > z0) nav_quad(v, x0, z0, x1, z1, y, v->nav_reg[j][i] == 1 ? GREEN : JUMPONLY);
        // and the boundary itself, so the gap between the orange line and the green is the buffer
        for (int d = 0; d < 4; d++) {
            if (!ledge[d]) continue;
            float a0 = e0, b0 = f0, a1 = e1, b1 = f1;
            if (DX[d] > 0) a0 = a1 - BAR; else if (DX[d] < 0) a1 = a0 + BAR;
            if (DZ[d] > 0) b0 = b1 - BAR; else if (DZ[d] < 0) b1 = b0 + BAR;
            nav_quad(v, a0, b0, a1, b1, y + 0.01f, LEDGE);
        }
    }
    v->nav_dirty = false;
}

static void vx_nav_draw(VoxField *v) {
    if (!v->nav_view || !v->nav_prog) return;
    if (v->nav_dirty || v->nav_verts == 0) vx_nav_build_overlay(v);
    // the dynamic part — the body's radius circle and the followers' trail — is appended each frame
    int base = v->nav_verts;
    const float CYAN[4] = { 0.30f, 0.95f, 1.00f, 0.85f };
    const float TRAIL[4] = { 1.00f, 0.92f, 0.35f, 0.55f };
    {
        VxActor *a = &v->act[0];
        float r = v->agent_r, w = 0.035f, y = a->y + 0.06f;
        for (int k = 0; k < 28; k++) {
            float t0 = k * 6.2831853f / 28.0f, t1 = (k + 1) * 6.2831853f / 28.0f;
            float ax = a->x + cosf(t0) * r, az = a->z + sinf(t0) * r;
            float bx = a->x + cosf(t1) * r, bz = a->z + sinf(t1) * r;
            float cx = a->x + cosf(t1) * (r - w), cz = a->z + sinf(t1) * (r - w);
            float dx = a->x + cosf(t0) * (r - w), dz = a->z + sinf(t0) * (r - w);
            nav_vert(v, ax, y, az, CYAN); nav_vert(v, bx, y, bz, CYAN); nav_vert(v, cx, y, cz, CYAN);
            nav_vert(v, ax, y, az, CYAN); nav_vert(v, cx, y, cz, CYAN); nav_vert(v, dx, y, dz, CYAN);
        }
    }
    for (int k = 0; k < v->trail_n; k++) {
        VxCrumb *c = &v->trail[(v->trail_head - 1 - k + 2 * VX_TRAIL) % VX_TRAIL];
        nav_quad(v, c->x - 0.05f, c->z - 0.05f, c->x + 0.05f, c->z + 0.05f, c->y + 0.07f, TRAIL);
    }
    int n = v->nav_verts;
    v->nav_verts = base;                       // the dynamic tail is rebuilt every frame
    if (n <= 0) return;
    glUseProgram(v->nav_prog);
    glUniformMatrix4fv(vx_uni(v->nav_prog, "u_mvp"), 1, GL_FALSE, v->mvp);
    glBindVertexArray(v->nav_vao);
    glBindBuffer(GL_ARRAY_BUFFER, v->nav_vbo);
    glBufferData(GL_ARRAY_BUFFER, (GLsizeiptr)n * 7 * sizeof(float), v->nav_buf, GL_STREAM_DRAW);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 7 * sizeof(float), (void *)0);
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 4, GL_FLOAT, GL_FALSE, 7 * sizeof(float), (void *)(3 * sizeof(float)));
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glDepthMask(GL_FALSE);
    glDrawArrays(GL_TRIANGLES, 0, n);
    glDepthMask(GL_TRUE);
    glDisable(GL_BLEND);
    glBindVertexArray(0);
}

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

void vx_render(VoxField *v, int w, int h) {
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
    // Placeholder field sprites: opaque, untextured, same state as the batches above.
    { int from = 0, n = vx_emit_sprites(v, sv, sv_cap, 3, 0, 0, &from); if (n) { vx_draw_sprite_batch(v, sv, n); draws++; } }
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
    vx_nav_draw(v);
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
void vx_overdraw_pass(VoxField *v, int w, int h, float step) {
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

double vx_measure_overdraw(VoxField *v, int w, int h) {
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

GLuint vx_post(VoxField *v, int w, int h) {
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

void draw_touch_ui(VoxField *v, int w, int h) {
    ImDrawList *dl = ImGui::GetBackgroundDrawList();
    float r = h * 0.14f;
    if (v->stick_on) {
        float dx = v->stick_x - v->stick_ox, dy = v->stick_y - v->stick_oy;
        float len = sqrtf(dx * dx + dy * dy);
        bool running = len > r * 0.55f;
        if (len > r) { dx *= r / len; dy *= r / len; }
        dl->AddCircleFilled(ImVec2(v->stick_ox, v->stick_oy), r, IM_COL32(255, 255, 255, 26), 32);
        dl->AddCircle(ImVec2(v->stick_ox, v->stick_oy), r, IM_COL32(255, 255, 255, 70), 32, h * 0.006f);
        // the run ring: past it the magnitude is a run, so the thumb can feel where the line is
        dl->AddCircle(ImVec2(v->stick_ox, v->stick_oy), r * 0.55f,
                      running ? IM_COL32(255, 230, 150, 120) : IM_COL32(255, 255, 255, 40), 32, h * 0.004f);
        dl->AddCircleFilled(ImVec2(v->stick_ox + dx, v->stick_oy + dy), r * 0.42f,
                            running ? IM_COL32(255, 236, 190, 140) : IM_COL32(220, 228, 255, 110), 24);
    }
    float ax, ay, ar, jx, jy, jr;
    vx_btn_act(w, h, &ax, &ay, &ar);
    vx_btn_jump(w, h, &jx, &jy, &jr);
    dl->AddCircle(ImVec2(ax, ay), ar, IM_COL32(255, 255, 255, v->act_on ? 130 : 45), 28, h * 0.006f);
    if (v->act_on) dl->AddCircleFilled(ImVec2(ax, ay), ar, IM_COL32(255, 255, 255, 30), 28);
    dl->AddCircle(ImVec2(jx, jy), jr, IM_COL32(255, 255, 255, v->jump_held ? 130 : 45), 28, h * 0.006f);
    if (v->jump_held) dl->AddCircleFilled(ImVec2(jx, jy), jr, IM_COL32(255, 255, 255, 30), 28);
    dl->AddText(ImGui::GetFont(), h * 0.045f, ImVec2(jx - h * 0.055f, jy - h * 0.022f),
                IM_COL32(255, 230, 150, 200), "JUMP");
    if (v->exam_trig >= 0 || v->exam_npc >= 0) {
        float px = v->player_screen[0], py = (float)h - v->player_screen[1];
        dl->AddText(ImGui::GetFont(), h * 0.09f, ImVec2(px - h * 0.02f, py - h * 0.22f), IM_COL32(255, 230, 120, 240), "!");
    }
}

// THE SAME GEOMETRY AS THE BOX, with nothing drawn: how many pages this text needs at this window
// size, and whether any one of them is taller than the space it has. The robustness sweep runs every
// id in story/field/text.md through it, so a line that would spill off the bottom of the box on the
// phone is a test failure and not a screenshot somebody happens to take. Kept beside draw_msg_box
// deliberately: if one changes, so must the other.
int vx_msg_measure(const char *who, const char *text, int w, int h, int *overflow) {
    if (overflow) *overflow = 0;
    if (!text || !text[0]) return 0;
    ImFont *font = ImGui::GetFont();
    float ts = h * 0.052f;
    float x0 = w * 0.06f, x1 = w * 0.94f, y1 = h * 0.96f, y0 = y1 - h * 0.24f;
    DlgRect cr = dlg_content(x0, y0, x1, y1, ts);
    float name_h = (who && who[0]) ? ts * DLG_LINE_H : 0.0f;
    float tw = cr.x1 - cr.x0, avail = (cr.y1 - cr.y0) - name_h;
    DlgPages pg;
    dlg_paginate(font, ts, tw, dlg_max_lines(avail, ts), text, &pg);
    for (int i = 0; i < pg.count; i++)
        if (dlg_text_height(font, ts, tw, pg.beg[i], pg.end[i]) > avail + 0.5f && overflow) *overflow = 1;
    return pg.count;
}

void draw_msg_box(VoxField *v, int w, int h) {
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

void vx_poll_map_flag(VoxField *v, float dt) {
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

