#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>
#include <math.h>

#ifdef __ANDROID__
#include <GLES3/gl3.h>
#else
#include <OpenGL/gl3.h>
#endif

#include "text.h"
#include "ui.h"
#include "perf.h"

#define RINGS 24
#define SECTORS 24
#define PI 3.14159265f

static GLuint vao, vbo, ibo;
static int index_count;
static GLuint program;
static GLint u_mvp;

static const char *vert_src =
#ifdef __ANDROID__
    "#version 300 es\n"
#else
    "#version 330\n"
#endif
    "layout(location=0) in vec3 pos;\n"
    "uniform mat4 mvp;\n"
    "out vec3 v_normal;\n"
    "void main() {\n"
    "  v_normal = pos;\n"
    "  gl_Position = mvp * vec4(pos, 1.0);\n"
    "}\n";

static const char *frag_src =
#ifdef __ANDROID__
    "#version 300 es\nprecision mediump float;\n"
#else
    "#version 330\n"
#endif
    "in vec3 v_normal;\n"
    "out vec4 color;\n"
    "void main() {\n"
    "  vec3 n = normalize(v_normal);\n"
    "  vec3 light = normalize(vec3(1.0, 1.0, 1.0));\n"
    "  float d = max(dot(n, light), 0.15);\n"
    "  color = vec4(vec3(0.2, 0.5, 0.9) * d, 1.0);\n"
    "}\n";

static GLuint compile_shader(GLenum type, const char *src) {
    GLuint s = glCreateShader(type);
    glShaderSource(s, 1, &src, NULL);
    glCompileShader(s);
    return s;
}

static void make_sphere(float radius) {
    int vcount = (RINGS + 1) * (SECTORS + 1);
    float *verts = SDL_malloc(vcount * 3 * sizeof(float));
    int icount = RINGS * SECTORS * 6;
    unsigned short *indices = SDL_malloc(icount * sizeof(unsigned short));

    int v = 0;
    for (int r = 0; r <= RINGS; r++) {
        float phi = PI * r / RINGS;
        for (int s = 0; s <= SECTORS; s++) {
            float theta = 2.0f * PI * s / SECTORS;
            verts[v++] = radius * sinf(phi) * cosf(theta);
            verts[v++] = radius * cosf(phi);
            verts[v++] = radius * sinf(phi) * sinf(theta);
        }
    }

    int i = 0;
    for (int r = 0; r < RINGS; r++) {
        for (int s = 0; s < SECTORS; s++) {
            int a = r * (SECTORS + 1) + s;
            int b = a + SECTORS + 1;
            indices[i++] = a;
            indices[i++] = b;
            indices[i++] = a + 1;
            indices[i++] = a + 1;
            indices[i++] = b;
            indices[i++] = b + 1;
        }
    }
    index_count = icount;

    glGenVertexArrays(1, &vao);
    glBindVertexArray(vao);
    glGenBuffers(1, &vbo);
    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    glBufferData(GL_ARRAY_BUFFER, vcount * 3 * sizeof(float), verts, GL_STATIC_DRAW);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 0, 0);
    glGenBuffers(1, &ibo);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ibo);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, icount * sizeof(unsigned short), indices, GL_STATIC_DRAW);

    SDL_free(verts);
    SDL_free(indices);
}

static void mat4_identity(float *m) {
    SDL_memset(m, 0, 16 * sizeof(float));
    m[0] = m[5] = m[10] = m[15] = 1.0f;
}

static void mat4_perspective(float *m, float fovy, float aspect, float near, float far) {
    float f = 1.0f / tanf(fovy * 0.5f);
    SDL_memset(m, 0, 16 * sizeof(float));
    m[0] = f / aspect;
    m[5] = f;
    m[10] = (far + near) / (near - far);
    m[11] = -1.0f;
    m[14] = 2.0f * far * near / (near - far);
}

static void mat4_translate(float *m, float x, float y, float z) {
    mat4_identity(m);
    m[12] = x; m[13] = y; m[14] = z;
}

static void mat4_multiply(float *out, const float *a, const float *b) {
    float tmp[16];
    for (int i = 0; i < 4; i++)
        for (int j = 0; j < 4; j++) {
            tmp[j * 4 + i] = 0;
            for (int k = 0; k < 4; k++)
                tmp[j * 4 + i] += a[k * 4 + i] * b[j * 4 + k];
        }
    SDL_memcpy(out, tmp, sizeof(tmp));
}

static void mat4_rotate_y(float *m, float angle) {
    mat4_identity(m);
    float c = cosf(angle), s = sinf(angle);
    m[0] = c; m[2] = s;
    m[8] = -s; m[10] = c;
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
    SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 16);

    SDL_Window *win = SDL_CreateWindow("SDL3 OpenGL", 800, 600,
        SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE | SDL_WINDOW_FULLSCREEN);
    SDL_GLContext gl = SDL_GL_CreateContext(win);

    GLuint vs = compile_shader(GL_VERTEX_SHADER, vert_src);
    GLuint fs = compile_shader(GL_FRAGMENT_SHADER, frag_src);
    program = glCreateProgram();
    glAttachShader(program, vs);
    glAttachShader(program, fs);
    glLinkProgram(program);
    u_mvp = glGetUniformLocation(program, "mvp");

    make_sphere(1.0f);
    glEnable(GL_DEPTH_TEST);

#ifdef __ANDROID__
    const char *font_path = "Roboto-Regular.ttf";
#else
    const char *font_path = "assets/Roboto-Regular.ttf";
#endif
    size_t font_size = 0;
    unsigned char *font_data = (unsigned char *)SDL_LoadFile(font_path, &font_size);
    if (font_data) {
        text_init(font_data, 48.0f);
        SDL_free(font_data);
    }

    ui_init();
    perf_init();
    int btn_perf = ui_add_button(640, 16, 140, 48, "Perf");

    float sphere_x = 0.0f, sphere_y = 0.0f;
    float touch_active = 0;
    float angle = 0.0f;
    int running = 1;

    while (running) {
        SDL_Event e;
        while (SDL_PollEvent(&e)) {
            if (e.type == SDL_EVENT_QUIT) running = 0;
            if (e.type == SDL_EVENT_FINGER_UP) {
                int fw, fh;
                SDL_GetWindowSizeInPixels(win, &fw, &fh);
                float px = e.tfinger.x * fw;
                float py = e.tfinger.y * fh;
                int hit = ui_touch_up(px, py);
                if (hit == btn_perf) perf_toggle();
            }
            if (e.type == SDL_EVENT_FINGER_DOWN || e.type == SDL_EVENT_FINGER_MOTION) {
                sphere_x = (e.tfinger.x - 0.5f) * 6.0f;
                sphere_y = -(e.tfinger.y - 0.5f) * 6.0f;
                touch_active = 1;
            }
        }

        int w, h;
        SDL_GetWindowSizeInPixels(win, &w, &h);
        glViewport(0, 0, w, h);
        glClearColor(0.12f, 0.12f, 0.24f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        perf_frame_begin();

        UIButton *pb = ui_get(btn_perf);
        if (pb) { pb->x = w - 160; pb->toggled = perf_visible(); }

        glUseProgram(program);
        angle += 0.01f;

        float proj[16], view[16], model[16], rot[16], tmp[16], mvp[16];
        mat4_perspective(proj, PI / 4.0f, (float)w / h, 0.1f, 100.0f);
        mat4_translate(view, 0, 0, -8.0f);
        mat4_translate(model, sphere_x, sphere_y, 0.0f);
        mat4_rotate_y(rot, angle);
        mat4_multiply(tmp, model, rot);
        mat4_multiply(mvp, view, tmp);
        mat4_multiply(mvp, proj, mvp);

        glUniformMatrix4fv(u_mvp, 1, GL_FALSE, mvp);
        glBindVertexArray(vao);
        glDrawElements(GL_TRIANGLES, index_count, GL_UNSIGNED_SHORT, 0);
        perf_add_draw_call(index_count / 3, (RINGS + 1) * (SECTORS + 1));

        float scx = 2.0f / w, scy = 2.0f / h;
        text_draw("Touch to move sphere", 20, 50, scx, scy, 1, 1, 1);

        perf_draw(w, h);
        ui_draw(w, h);

        perf_frame_end();
        SDL_GL_SwapWindow(win);
    }

    text_cleanup();
    ui_cleanup();

    SDL_GL_DestroyContext(gl);
    SDL_DestroyWindow(win);
    SDL_Quit();
    return 0;
}
