#include "ui.h"
#include "text.h"
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
    "void main() { gl_Position = vec4(pos, 0.0, 1.0); }\n";

static const char *rect_fs =
    GLSL_F
    "uniform vec4 color;\n"
    "out vec4 frag;\n"
    "void main() { frag = color; }\n";

static GLuint prog, vao, vbo;
static GLint u_color;
static UIButton buttons[UI_MAX_BUTTONS];
static int nbuttons;

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
    u_color = glGetUniformLocation(prog, "color");

    glGenVertexArrays(1, &vao);
    glBindVertexArray(vao);
    glGenBuffers(1, &vbo);
    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    glBufferData(GL_ARRAY_BUFFER, 12 * sizeof(float), NULL, GL_DYNAMIC_DRAW);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 0, 0);

    nbuttons = 0;
}

int ui_add_button(float x, float y, float w, float h, const char *label) {
    if (nbuttons >= UI_MAX_BUTTONS) return -1;
    int id = nbuttons;
    buttons[id].x = x;
    buttons[id].y = y;
    buttons[id].w = w;
    buttons[id].h = h;
    buttons[id].label = label;
    buttons[id].toggled = 0;
    buttons[id].id = id;
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

static void draw_rect(float x, float y, float w, float h,
                      int sw, int sh, float r, float g, float b, float a) {
    float x0 = x / sw * 2.0f - 1.0f;
    float y0 = -(y / sh * 2.0f - 1.0f);
    float x1 = (x + w) / sw * 2.0f - 1.0f;
    float y1 = -((y + h) / sh * 2.0f - 1.0f);

    float verts[] = {
        x0, y1, x1, y1, x1, y0,
        x0, y1, x1, y0, x0, y0,
    };

    glUseProgram(prog);
    glUniform4f(u_color, r, g, b, a);
    glBindVertexArray(vao);
    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    glBufferSubData(GL_ARRAY_BUFFER, 0, sizeof(verts), verts);
    glDrawArrays(GL_TRIANGLES, 0, 6);
}

void ui_draw(int sw, int sh) {
    glDisable(GL_DEPTH_TEST);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    float scx = 2.0f / sw, scy = 2.0f / sh;

    for (int i = 0; i < nbuttons; i++) {
        UIButton *b = &buttons[i];

        if (b->toggled)
            draw_rect(b->x, b->y, b->w, b->h, sw, sh, 0.1f, 0.5f, 0.2f, 0.85f);
        else
            draw_rect(b->x, b->y, b->w, b->h, sw, sh, 0.2f, 0.2f, 0.25f, 0.85f);

        // border
        draw_rect(b->x, b->y, b->w, 2, sw, sh, 0.5f, 0.5f, 0.5f, 1.0f);
        draw_rect(b->x, b->y + b->h - 2, b->w, 2, sw, sh, 0.5f, 0.5f, 0.5f, 1.0f);
        draw_rect(b->x, b->y, 2, b->h, sw, sh, 0.5f, 0.5f, 0.5f, 1.0f);
        draw_rect(b->x + b->w - 2, b->y, 2, b->h, sw, sh, 0.5f, 0.5f, 0.5f, 1.0f);

        float tx = b->x + 12;
        float ty = b->y + b->h * 0.65f;
        text_draw(b->label, tx, ty, scx, scy, 1, 1, 1);
    }

    glDisable(GL_BLEND);
    glEnable(GL_DEPTH_TEST);
}

void ui_cleanup(void) {
    glDeleteProgram(prog);
    glDeleteBuffers(1, &vbo);
    glDeleteVertexArrays(1, &vao);
}
