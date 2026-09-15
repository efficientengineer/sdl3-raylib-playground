#include "ui.h"
#include "text.h"
#include "perf.h"
#include <SDL3/SDL.h>
#include <string.h>

#ifdef __ANDROID__
#define GLSL_V "#version 300 es\n"
#define GLSL_F "#version 300 es\nprecision mediump float;\n"
#else
#define GLSL_V "#version 330\n"
#define GLSL_F "#version 330\n"
#endif

static const char *rect_vs =
    GLSL_V
    "layout(location=0) in vec2 pos;\n"
    "layout(location=1) in vec4 col;\n"
    "out vec4 v_col;\n"
    "void main() { gl_Position = vec4(pos, 0.0, 1.0); v_col = col; }\n";

static const char *rect_fs =
    GLSL_F
    "in vec4 v_col;\n"
    "out vec4 frag;\n"
    "void main() { frag = v_col; }\n";

static GLuint prog, vao, vbo;
static UIButton buttons[UI_MAX_BUTTONS];
static int nbuttons;

// batched quads: each rect = 6 verts × 6 floats (x,y,r,g,b,a)
#define MAX_RECTS 128
#define FLOATS_PER_VERT 6
static float rect_batch[MAX_RECTS * 6 * FLOATS_PER_VERT];
static int rect_count;

void ui_init(void) {
    GLuint vs = glCreateShader(GL_VERTEX_SHADER);
    glShaderSource(vs, 1, &rect_vs, NULL);
    glCompileShader(vs);
    GLuint fs = glCreateShader(GL_FRAGMENT_SHADER);
    glShaderSource(fs, 1, &rect_fs, NULL);
    glCompileShader(fs);
    prog = glCreateProgram();
    glAttachShader(prog, vs); glAttachShader(prog, fs);
    glLinkProgram(prog);
    glDeleteShader(vs); glDeleteShader(fs);

    int stride = FLOATS_PER_VERT * sizeof(float);
    glGenVertexArrays(1, &vao);
    glBindVertexArray(vao);
    glGenBuffers(1, &vbo);
    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof(rect_batch), NULL, GL_DYNAMIC_DRAW);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, stride, (void*)0);
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 4, GL_FLOAT, GL_FALSE, stride, (void*)(2 * sizeof(float)));

    nbuttons = 0;
}

int ui_add_button(float x, float y, float w, float h, const char *label) {
    if (nbuttons >= UI_MAX_BUTTONS) return -1;
    int id = nbuttons;
    buttons[id] = (UIButton){ x, y, w, h, label, 0, id };
    nbuttons++;
    return id;
}

int ui_touch_up(float px, float py) {
    for (int i = 0; i < nbuttons; i++) {
        UIButton *b = &buttons[i];
        if (px >= b->x && px <= b->x + b->w &&
            py >= b->y && py <= b->y + b->h) {
            b->toggled = !b->toggled;
            return b->id;
        }
    }
    return -1;
}

UIButton *ui_get(int id) {
    if (id < 0 || id >= nbuttons) return NULL;
    return &buttons[id];
}

static void push_rect(float x, float y, float w, float h,
                      int sw, int sh, float r, float g, float b, float a) {
    if (rect_count >= MAX_RECTS) return;
    float x0 = x / sw * 2.0f - 1.0f;
    float y0 = -(y / sh * 2.0f - 1.0f);
    float x1 = (x + w) / sw * 2.0f - 1.0f;
    float y1 = -((y + h) / sh * 2.0f - 1.0f);

    float *p = rect_batch + rect_count * 6 * FLOATS_PER_VERT;
    float verts[] = {
        x0, y1, r, g, b, a,  x1, y1, r, g, b, a,  x1, y0, r, g, b, a,
        x0, y1, r, g, b, a,  x1, y0, r, g, b, a,  x0, y0, r, g, b, a,
    };
    SDL_memcpy(p, verts, sizeof(verts));
    rect_count++;
}

static void flush_rects(void) {
    if (rect_count == 0) return;
    glUseProgram(prog);
    glBindVertexArray(vao);
    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    int bytes = rect_count * 6 * FLOATS_PER_VERT * sizeof(float);
    glBufferSubData(GL_ARRAY_BUFFER, 0, bytes, rect_batch);
    int nverts = rect_count * 6;
    glDrawArrays(GL_TRIANGLES, 0, nverts);
    perf_add_draw_call(nverts / 3, nverts);
    rect_count = 0;
}

void ui_draw(int sw, int sh) {
    glDisable(GL_DEPTH_TEST);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    rect_count = 0;
    for (int i = 0; i < nbuttons; i++) {
        UIButton *b = &buttons[i];
        if (b->toggled)
            push_rect(b->x, b->y, b->w, b->h, sw, sh, 0.1f, 0.5f, 0.2f, 0.85f);
        else
            push_rect(b->x, b->y, b->w, b->h, sw, sh, 0.2f, 0.2f, 0.25f, 0.85f);
        push_rect(b->x, b->y, b->w, 2, sw, sh, 0.5f, 0.5f, 0.5f, 1.0f);
        push_rect(b->x, b->y + b->h - 2, b->w, 2, sw, sh, 0.5f, 0.5f, 0.5f, 1.0f);
        push_rect(b->x, b->y, 2, b->h, sw, sh, 0.5f, 0.5f, 0.5f, 1.0f);
        push_rect(b->x + b->w - 2, b->y, 2, b->h, sw, sh, 0.5f, 0.5f, 0.5f, 1.0f);
    }
    flush_rects();

    // batch all button labels into one text draw
    float scx = 2.0f / sw, scy = 2.0f / sh;
    text_begin();
    for (int i = 0; i < nbuttons; i++) {
        UIButton *b = &buttons[i];
        text_draw(b->label, b->x + 12, b->y + b->h * 0.65f, scx, scy, 1, 1, 1);
    }
    text_flush();

    glDisable(GL_BLEND);
    glEnable(GL_DEPTH_TEST);
}

void ui_cleanup(void) {
    glDeleteProgram(prog);
    glDeleteBuffers(1, &vbo);
    glDeleteVertexArrays(1, &vao);
}
