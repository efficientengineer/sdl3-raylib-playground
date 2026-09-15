#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>
#include <math.h>
#include <string.h>

#ifdef __ANDROID__
#include <GLES3/gl3.h>
#else
#include <OpenGL/gl3.h>
#endif

#include "text.h"
#include "ui.h"
#include "perf.h"
#include "par_shapes.h"
#include "FastNoiseLite.h"

#define PI 3.14159265f

// --- shader (baked color + distance fog + height fog) ---
#ifdef __ANDROID__
#define V "#version 300 es\n"
#define F "#version 300 es\nprecision mediump float;\n"
#else
#define V "#version 330\n"
#define F "#version 330\n"
#endif

static const char *vs_src =
    V
    "layout(location=0) in vec3 pos;\n"
    "layout(location=1) in vec3 col;\n"
    "uniform mat4 mvp;\n"
    "uniform vec3 cam_pos;\n"
    "uniform float fog_start, fog_end, hf_floor, hf_ceil;\n"
    "out vec3 v_col;\n"
    "out float v_dfog, v_hfog;\n"
    "void main() {\n"
    "  v_col = col;\n"
    "  float d = length(pos - cam_pos);\n"
    "  v_dfog = clamp((d - fog_start) / (fog_end - fog_start), 0.0, 1.0);\n"
    "  float h = clamp((pos.y - hf_floor) / (hf_ceil - hf_floor), 0.0, 1.0);\n"
    "  v_hfog = 1.0 - h;\n"
    "  gl_Position = mvp * vec4(pos, 1.0);\n"
    "}\n";

static const char *fs_src =
    F
    "in vec3 v_col;\n"
    "in float v_dfog, v_hfog;\n"
    "uniform vec3 fog_color;\n"
    "out vec4 frag;\n"
    "void main() {\n"
    "  vec3 c = v_col;\n"
    "  c = mix(c, fog_color * 0.5, v_hfog * 0.35);\n"
    "  c = mix(c, fog_color, v_dfog * v_dfog);\n"
    "  frag = vec4(c, 1.0);\n"
    "}\n";

// --- wireframe shader ---
static const char *wire_vs =
    V
    "layout(location=0) in vec3 pos;\n"
    "uniform mat4 mvp;\n"
    "void main() { gl_Position = mvp * vec4(pos, 1.0); }\n";

static const char *wire_fs =
    F
    "uniform vec3 wire_color;\n"
    "out vec4 frag;\n"
    "void main() { frag = vec4(wire_color, 1.0); }\n";

// --- math ---
static void m4_id(float *m) { memset(m,0,64); m[0]=m[5]=m[10]=m[15]=1; }
static void m4_persp(float *m, float fov, float a, float n, float f) {
    float t=1.0f/tanf(fov*0.5f); memset(m,0,64);
    m[0]=t/a; m[5]=t; m[10]=(f+n)/(n-f); m[11]=-1; m[14]=2*f*n/(n-f);
}
static void m4_mul(float *o, const float *a, const float *b) {
    float t[16];
    for(int i=0;i<4;i++) for(int j=0;j<4;j++) {
        t[j*4+i]=0; for(int k=0;k<4;k++) t[j*4+i]+=a[k*4+i]*b[j*4+k];
    } memcpy(o,t,64);
}
static void m4_lookat(float *m, float ex, float ey, float ez,
                      float tx, float ty, float tz) {
    float fx=tx-ex, fy=ty-ey, fz=tz-ez;
    float fl=sqrtf(fx*fx+fy*fy+fz*fz);
    fx/=fl; fy/=fl; fz/=fl;
    float ux=0,uy=1,uz=0;
    float sx=fy*uz-fz*uy, sy=fz*ux-fx*uz, sz=fx*uy-fy*ux;
    float sl=sqrtf(sx*sx+sy*sy+sz*sz);
    sx/=sl; sy/=sl; sz/=sl;
    ux=sy*fz-sz*fy; uy=sz*fx-sx*fz; uz=sx*fy-sy*fx;
    m4_id(m);
    m[0]=sx; m[4]=sy; m[8]=sz;
    m[1]=ux; m[5]=uy; m[9]=uz;
    m[2]=-fx; m[6]=-fy; m[10]=-fz;
    m[12]=-(sx*ex+sy*ey+sz*ez);
    m[13]=-(ux*ex+uy*ey+uz*ez);
    m[14]=(fx*ex+fy*ey+fz*ez);
}

static GLuint mk_shader(GLenum type, const char *src) {
    GLuint s = glCreateShader(type);
    glShaderSource(s, 1, &src, NULL);
    glCompileShader(s);
    GLint ok; glGetShaderiv(s, GL_COMPILE_STATUS, &ok);
    if (!ok) { char log[512]; glGetShaderInfoLog(s,512,NULL,log); SDL_Log("shader: %s",log); }
    return s;
}

// --- mesh type: position + baked color ---
typedef struct { float x,y,z, r,g,b; } PVert;

static void bake_color(float *rgb, float nx, float ny, float nz,
                       float base_r, float base_g, float base_b) {
    float lx=0.3f, ly=0.8f, lz=0.4f;
    float ll=sqrtf(lx*lx+ly*ly+lz*lz); lx/=ll; ly/=ll; lz/=ll;
    float d = nx*lx + ny*ly + nz*lz;
    if (d < 0) d = 0;
    float b = 0.3f + d * 0.7f;
    rgb[0] = base_r * b;
    rgb[1] = base_g * b;
    rgb[2] = base_b * b;
}

// Growable PVert array for collecting all geometry before upload
typedef struct { PVert *data; int count, cap; } PVertArray;

static void pva_init(PVertArray *a, int cap) {
    a->data = SDL_malloc(cap * sizeof(PVert));
    a->count = 0; a->cap = cap;
}

static void pva_grow(PVertArray *a, int need) {
    if (a->count + need <= a->cap) return;
    while (a->cap < a->count + need) a->cap *= 2;
    a->data = SDL_realloc(a->data, a->cap * sizeof(PVert));
}

static void pva_push(PVertArray *a, PVert v) {
    pva_grow(a, 1);
    a->data[a->count++] = v;
}

static void collect_par(PVertArray *a, par_shapes_mesh *pm,
                        float ox, float oy, float oz,
                        float scale, float cr, float cg, float cb) {
    int nv = pm->npoints;
    PVert *verts = SDL_malloc(nv * sizeof(PVert));
    for (int i = 0; i < nv; i++) {
        float *p = pm->points + i * 3;
        float *n = pm->normals + i * 3;
        verts[i].x = p[0] * scale + ox;
        verts[i].y = p[1] * scale + oy;
        verts[i].z = p[2] * scale + oz;
        bake_color(&verts[i].r, n[0], n[1], n[2], cr, cg, cb);
    }
    int ni = pm->ntriangles * 3;
    pva_grow(a, ni);
    for (int i = 0; i < ni; i++)
        a->data[a->count++] = verts[pm->triangles[i]];
    SDL_free(verts);
}

static void collect_terrain(PVertArray *a) {
    int sz = 80;
    float half = sz * 0.5f;
    fnl_state noise = fnlCreateState();
    noise.noise_type = FNL_NOISE_OPENSIMPLEX2;
    noise.frequency = 0.03f;

    pva_grow(a, sz * sz * 6);
    for (int z = 0; z < sz; z++) {
        for (int x = 0; x < sz; x++) {
            float x0 = x - half, x1 = x + 1 - half;
            float z0 = z - half, z1 = z + 1 - half;
            float y00 = fnlGetNoise2D(&noise, x0, z0) * 0.5f;
            float y10 = fnlGetNoise2D(&noise, x1, z0) * 0.5f;
            float y01 = fnlGetNoise2D(&noise, x0, z1) * 0.5f;
            float y11 = fnlGetNoise2D(&noise, x1, z1) * 0.5f;

            float ax=1, ay=y10-y00, az=0, bx=0, by=y01-y00, bz=1;
            float nx = ay*bz - az*by, ny = az*bx - ax*bz, nz = ax*by - ay*bx;
            float nl = sqrtf(nx*nx+ny*ny+nz*nz);
            nx/=nl; ny/=nl; nz/=nl;

            float green = 0.35f + fnlGetNoise2D(&noise, x0*2, z0*2) * 0.1f;
            float rgb[3];
            bake_color(rgb, nx, ny, nz, 0.35f, green + 0.15f, 0.2f);

            PVert va = {x0, y00, z0, rgb[0], rgb[1], rgb[2]};
            PVert vb = {x1, y10, z0, rgb[0], rgb[1], rgb[2]};
            PVert vc = {x1, y11, z1, rgb[0], rgb[1], rgb[2]};
            PVert vd = {x0, y01, z1, rgb[0], rgb[1], rgb[2]};
            pva_push(a, va); pva_push(a, vb); pva_push(a, vc);
            pva_push(a, va); pva_push(a, vc); pva_push(a, vd);
        }
    }
}

typedef struct { GLuint vao, vbo, line_ibo; int count, line_count; } SceneBatch;

static SceneBatch upload_scene(PVertArray *a) {
    SceneBatch s;
    s.count = a->count;

    glGenVertexArrays(1, &s.vao);
    glBindVertexArray(s.vao);
    glGenBuffers(1, &s.vbo);
    glBindBuffer(GL_ARRAY_BUFFER, s.vbo);
    glBufferData(GL_ARRAY_BUFFER, a->count * sizeof(PVert), a->data, GL_STATIC_DRAW);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(PVert), (void*)0);
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(PVert), (void*)12);

    // wireframe line indices
    int ntris = a->count / 3;
    int nlines = ntris * 6;
    unsigned int *idx = SDL_malloc(nlines * sizeof(unsigned int));
    for (int i = 0; i < ntris; i++) {
        int b = i * 3;
        idx[i*6+0] = b;   idx[i*6+1] = b+1;
        idx[i*6+2] = b+1; idx[i*6+3] = b+2;
        idx[i*6+4] = b+2; idx[i*6+5] = b;
    }
    s.line_count = nlines;
    glGenBuffers(1, &s.line_ibo);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, s.line_ibo);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, nlines * sizeof(unsigned int), idx, GL_STATIC_DRAW);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
    SDL_free(idx);

    SDL_free(a->data);
    a->data = NULL; a->count = 0; a->cap = 0;
    return s;
}

int main(int argc, char *argv[]) {
    SDL_Init(SDL_INIT_VIDEO);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
#ifdef __ANDROID__
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 0);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_ES);
#else
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
#endif
    SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 24);

    SDL_Window *win = SDL_CreateWindow("Scene", 800, 600,
        SDL_WINDOW_OPENGL | SDL_WINDOW_FULLSCREEN);
    SDL_GL_CreateContext(win);

    GLuint vs = mk_shader(GL_VERTEX_SHADER, vs_src);
    GLuint fs = mk_shader(GL_FRAGMENT_SHADER, fs_src);
    GLuint prog = glCreateProgram();
    glAttachShader(prog, vs); glAttachShader(prog, fs);
    glLinkProgram(prog);
    glUseProgram(prog);

    GLuint wvs = mk_shader(GL_VERTEX_SHADER, wire_vs);
    GLuint wfs = mk_shader(GL_FRAGMENT_SHADER, wire_fs);
    GLuint wire_prog = glCreateProgram();
    glAttachShader(wire_prog, wvs); glAttachShader(wire_prog, wfs);
    glLinkProgram(wire_prog);
    GLint wu_mvp = glGetUniformLocation(wire_prog, "mvp");
    GLint wu_col = glGetUniformLocation(wire_prog, "wire_color");

    GLint u_mvp = glGetUniformLocation(prog, "mvp");
    GLint u_cam = glGetUniformLocation(prog, "cam_pos");
    GLint u_fog = glGetUniformLocation(prog, "fog_color");
    GLint u_fs = glGetUniformLocation(prog, "fog_start");
    GLint u_fe = glGetUniformLocation(prog, "fog_end");
    GLint u_hf = glGetUniformLocation(prog, "hf_floor");
    GLint u_hc = glGetUniformLocation(prog, "hf_ceil");

    // collect all geometry into one batch
    PVertArray all_geo;
    pva_init(&all_geo, 80 * 80 * 6 + 8192);

    collect_terrain(&all_geo);

    struct { float x, z; int type; float r, g, b; float scale; } spawns[] = {
        { 0,  0,  0, 0.7f, 0.3f, 0.2f, 1.5f},
        { 5, -3,  1, 0.8f, 0.7f, 0.3f, 1.0f},
        {-4,  6,  2, 0.3f, 0.5f, 0.8f, 1.2f},
        {12,  8,  0, 0.9f, 0.4f, 0.4f, 2.0f},
        {-10, -12, 1, 0.5f, 0.7f, 0.5f, 1.5f},
        { 15, -10, 2, 0.6f, 0.4f, 0.7f, 1.0f},
        {-18,  15, 0, 0.8f, 0.6f, 0.3f, 1.8f},
        { 20,  18, 3, 0.7f, 0.5f, 0.4f, 1.0f},
        {-22, -20, 1, 0.4f, 0.6f, 0.7f, 2.0f},
        { 25,  -5, 2, 0.5f, 0.3f, 0.6f, 1.5f},
        {-8,  25,  3, 0.6f, 0.6f, 0.4f, 1.2f},
        { 30,  25, 0, 0.7f, 0.4f, 0.5f, 2.5f},
    };
    int nspawns = sizeof(spawns) / sizeof(spawns[0]);

    for (int i = 0; i < nspawns; i++) {
        par_shapes_mesh *pm = NULL;
        switch (spawns[i].type) {
            case 0: pm = par_shapes_create_parametric_sphere(20, 20); break;
            case 1: pm = par_shapes_create_cylinder(20, 4); break;
            case 2: pm = par_shapes_create_torus(20, 12, 0.3f); break;
            case 3: pm = par_shapes_create_dodecahedron(); par_shapes_unweld(pm, true); par_shapes_compute_normals(pm); break;
        }
        if (!pm) continue;
        float s = spawns[i].scale;
        collect_par(&all_geo, pm, spawns[i].x, s, spawns[i].z,
                    s, spawns[i].r, spawns[i].g, spawns[i].b);
        par_shapes_free_mesh(pm);
    }

    SceneBatch scene = upload_scene(&all_geo);

    // font
#ifdef __ANDROID__
    const char *fp = "Roboto-Regular.ttf";
#else
    const char *fp = "assets/Roboto-Regular.ttf";
#endif
    size_t fsz = 0;
    unsigned char *fd = (unsigned char *)SDL_LoadFile(fp, &fsz);
    if (fd) { text_init(fd, 48.0f); SDL_free(fd); }

    glEnable(GL_DEPTH_TEST);

    ui_init();
    perf_init();
    int btn_wire = ui_add_button(20, 20, 180, 60, "Wireframe");
    int btn_perf = ui_add_button(220, 20, 140, 60, "Perf");

    // fly cam state
    float cam_x = 0, cam_y = 8, cam_z = 20;
    float cam_yaw = PI, cam_pitch = -0.2f;
    float move_speed = 12.0f;
    float look_speed = 2.5f;

    // touch tracking: left stick, right look
    SDL_FingerID left_id = -1, right_id = -1;
    float left_ox = 0, left_oy = 0; // anchor (normalized)
    float left_dx = 0, left_dy = 0; // current offset
    float right_dx = 0, right_dy = 0; // delta this frame

    int running = 1;
    int wireframe = 0;
    float fog_col[] = {0.6f, 0.7f, 0.8f};
    Uint64 last_tick = SDL_GetTicksNS();

    while (running) {
        Uint64 now = SDL_GetTicksNS();
        float dt = (now - last_tick) / 1e9f;
        if (dt > 0.05f) dt = 0.05f;
        last_tick = now;

        right_dx = 0; right_dy = 0;

        SDL_Event e;
        while (SDL_PollEvent(&e)) {
            if (e.type == SDL_EVENT_QUIT) running = 0;

            if (e.type == SDL_EVENT_FINGER_DOWN) {
                if (e.tfinger.x < 0.5f && left_id == (SDL_FingerID)-1) {
                    left_id = e.tfinger.fingerID;
                    left_ox = e.tfinger.x;
                    left_oy = e.tfinger.y;
                    left_dx = 0; left_dy = 0;
                } else if (e.tfinger.x >= 0.5f && right_id == (SDL_FingerID)-1) {
                    right_id = e.tfinger.fingerID;
                }
            }

            if (e.type == SDL_EVENT_FINGER_MOTION) {
                if (e.tfinger.fingerID == left_id) {
                    left_dx = (e.tfinger.x - left_ox) * 4.0f;
                    left_dy = (e.tfinger.y - left_oy) * 4.0f;
                    if (left_dx > 1) left_dx = 1; if (left_dx < -1) left_dx = -1;
                    if (left_dy > 1) left_dy = 1; if (left_dy < -1) left_dy = -1;
                } else if (e.tfinger.fingerID == right_id) {
                    right_dx += e.tfinger.dx;
                    right_dy += e.tfinger.dy;
                }
            }

            if (e.type == SDL_EVENT_FINGER_UP) {
                int sw, sh;
                SDL_GetWindowSizeInPixels(win, &sw, &sh);
                float px = e.tfinger.x * sw, py = e.tfinger.y * sh;
                int hit = ui_touch_up(px, py);
                if (hit == btn_wire)
                    wireframe = ui_get(btn_wire)->toggled;
                if (hit == btn_perf)
                    perf_toggle();

                if (e.tfinger.fingerID == left_id) {
                    left_id = -1; left_dx = 0; left_dy = 0;
                } else if (e.tfinger.fingerID == right_id) {
                    right_id = -1;
                }
            }
        }

        // apply look
        cam_yaw -= right_dx * look_speed;
        cam_pitch -= right_dy * look_speed;
        if (cam_pitch > 1.4f) cam_pitch = 1.4f;
        if (cam_pitch < -1.4f) cam_pitch = -1.4f;

        // forward/right vectors (horizontal plane)
        float fwd_x = -sinf(cam_yaw), fwd_z = -cosf(cam_yaw);
        float rgt_x = cosf(cam_yaw),  rgt_z = -sinf(cam_yaw);

        // apply movement
        float mx = left_dx, mz = -left_dy; // stick: x=strafe, y=forward
        cam_x += (fwd_x * mz + rgt_x * mx) * move_speed * dt;
        cam_z += (fwd_z * mz + rgt_z * mx) * move_speed * dt;
        cam_y += 0; // locked height for now

        // look target
        float tx = cam_x - sinf(cam_yaw) * cosf(cam_pitch);
        float ty = cam_y + sinf(cam_pitch);
        float tz = cam_z - cosf(cam_yaw) * cosf(cam_pitch);

        int w, h;
        SDL_GetWindowSizeInPixels(win, &w, &h);
        glViewport(0, 0, w, h);
        glClearColor(fog_col[0], fog_col[1], fog_col[2], 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        perf_frame_begin();
        UIButton *pb = ui_get(btn_perf);
        if (pb) pb->toggled = perf_visible();

        float proj[16], view[16], mvp[16];
        m4_persp(proj, PI / 4.0f, (float)w / h, 0.1f, 120.0f);
        m4_lookat(view, cam_x, cam_y, cam_z, tx, ty, tz);
        m4_mul(mvp, proj, view);

        glUseProgram(prog);
        glUniformMatrix4fv(u_mvp, 1, GL_FALSE, mvp);
        glUniform3f(u_cam, cam_x, cam_y, cam_z);
        glUniform3fv(u_fog, 1, fog_col);
        glUniform1f(u_fs, 20.0f);
        glUniform1f(u_fe, 55.0f);
        glUniform1f(u_hf, -1.0f);
        glUniform1f(u_hc, 4.0f);

        glBindVertexArray(scene.vao);
        glDrawArrays(GL_TRIANGLES, 0, scene.count);
        perf_add_draw_call(scene.count / 3, scene.count);

        if (wireframe) {
            glUseProgram(wire_prog);
            glUniformMatrix4fv(wu_mvp, 1, GL_FALSE, mvp);
            glUniform3f(wu_col, 0.0f, 1.0f, 0.3f);
            glDepthFunc(GL_LEQUAL);
            glBindVertexArray(scene.vao);
            glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, scene.line_ibo);
            glDrawElements(GL_LINES, scene.line_count, GL_UNSIGNED_INT, 0);
            perf_add_draw_call(scene.line_count / 2, scene.count);
            glDepthFunc(GL_LESS);
        }

        perf_draw(w, h);
        ui_draw(w, h);

        perf_frame_end();
        SDL_GL_SwapWindow(win);
    }

    SDL_DestroyWindow(win);
    SDL_Quit();
    return 0;
}
