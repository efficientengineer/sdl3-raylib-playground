#include "text.h"
#include "perf.h"
#include "stb_truetype.h"
#include <SDL3/SDL.h>
#include <string.h>

#define ATLAS_W 512
#define ATLAS_H 512
#define FIRST_CHAR 32
#define NUM_CHARS 96
#define MAX_GLYPHS 2048

static stbtt_bakedchar cdata[NUM_CHARS];
static GLuint tex;
static GLuint prog, vao, vbo;

// per-vertex: x, y, u, v, r, g, b
#define FLOATS_PER_VERT 7
#define VERTS_PER_GLYPH 6
static float batch[MAX_GLYPHS * VERTS_PER_GLYPH * FLOATS_PER_VERT];
static int batch_count; // glyphs queued

#ifdef __ANDROID__
#define GLSL_VERSION "#version 300 es\nprecision mediump float;\n"
#else
#define GLSL_VERSION "#version 330\n"
#endif

static const char *text_vert =
    GLSL_VERSION
    "layout(location=0) in vec2 pos;\n"
    "layout(location=1) in vec2 texcoord;\n"
    "layout(location=2) in vec3 color;\n"
    "out vec2 uv;\n"
    "out vec3 v_color;\n"
    "void main() {\n"
    "  gl_Position = vec4(pos, 0.0, 1.0);\n"
    "  uv = texcoord;\n"
    "  v_color = color;\n"
    "}\n";

static const char *text_frag =
    GLSL_VERSION
    "in vec2 uv;\n"
    "in vec3 v_color;\n"
    "out vec4 fragColor;\n"
    "uniform sampler2D tex;\n"
    "void main() {\n"
    "  float a = texture(tex, uv).r;\n"
    "  fragColor = vec4(v_color, a);\n"
    "}\n";

static GLuint mk_shader(GLenum type, const char *src) {
    GLuint s = glCreateShader(type);
    glShaderSource(s, 1, &src, NULL);
    glCompileShader(s);
    return s;
}

void text_init(const unsigned char *ttf_data, float pixel_height) {
    unsigned char *bitmap = SDL_malloc(ATLAS_W * ATLAS_H);
    stbtt_BakeFontBitmap(ttf_data, 0, pixel_height,
                         bitmap, ATLAS_W, ATLAS_H,
                         FIRST_CHAR, NUM_CHARS, cdata);

    glGenTextures(1, &tex);
    glBindTexture(GL_TEXTURE_2D, tex);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_R8, ATLAS_W, ATLAS_H, 0,
                 GL_RED, GL_UNSIGNED_BYTE, bitmap);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    SDL_free(bitmap);

    GLuint vs = mk_shader(GL_VERTEX_SHADER, text_vert);
    GLuint fs = mk_shader(GL_FRAGMENT_SHADER, text_frag);
    prog = glCreateProgram();
    glAttachShader(prog, vs);
    glAttachShader(prog, fs);
    glLinkProgram(prog);
    glDeleteShader(vs);
    glDeleteShader(fs);

    int stride = FLOATS_PER_VERT * sizeof(float);
    glGenVertexArrays(1, &vao);
    glBindVertexArray(vao);
    glGenBuffers(1, &vbo);
    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof(batch), NULL, GL_DYNAMIC_DRAW);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, stride, (void*)0);
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, stride, (void*)(2 * sizeof(float)));
    glEnableVertexAttribArray(2);
    glVertexAttribPointer(2, 3, GL_FLOAT, GL_FALSE, stride, (void*)(4 * sizeof(float)));

    batch_count = 0;
}

void text_begin(void) {
    batch_count = 0;
}

void text_draw(const char *str, float x, float y, float sx, float sy,
               float r, float g, float b) {
    float cx = x, cy = y;
    int len = (int)strlen(str);

    for (int i = 0; i < len; i++) {
        if (batch_count >= MAX_GLYPHS) break;
        int c = (unsigned char)str[i] - FIRST_CHAR;
        if (c < 0 || c >= NUM_CHARS) { cx += 10.0f; continue; }

        stbtt_aligned_quad q;
        stbtt_GetBakedQuad(cdata, ATLAS_W, ATLAS_H, c, &cx, &cy, &q, 1);

        float x0 = q.x0 * sx - 1.0f, y0 = 1.0f - q.y0 * sy;
        float x1 = q.x1 * sx - 1.0f, y1 = 1.0f - q.y1 * sy;

        float *p = batch + batch_count * VERTS_PER_GLYPH * FLOATS_PER_VERT;
        float quad[] = {
            x0, y1, q.s0, q.t1, r, g, b,
            x0, y0, q.s0, q.t0, r, g, b,
            x1, y0, q.s1, q.t0, r, g, b,
            x0, y1, q.s0, q.t1, r, g, b,
            x1, y0, q.s1, q.t0, r, g, b,
            x1, y1, q.s1, q.t1, r, g, b,
        };
        SDL_memcpy(p, quad, sizeof(quad));
        batch_count++;
    }
}

void text_flush(void) {
    if (batch_count == 0) return;

    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glDisable(GL_DEPTH_TEST);

    glUseProgram(prog);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, tex);

    glBindVertexArray(vao);
    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    int bytes = batch_count * VERTS_PER_GLYPH * FLOATS_PER_VERT * sizeof(float);
    glBufferSubData(GL_ARRAY_BUFFER, 0, bytes, batch);
    int nverts = batch_count * VERTS_PER_GLYPH;
    glDrawArrays(GL_TRIANGLES, 0, nverts);
    perf_add_draw_call(nverts / 3, nverts);

    glEnable(GL_DEPTH_TEST);
    glDisable(GL_BLEND);

    batch_count = 0;
}

void text_cleanup(void) {
    glDeleteProgram(prog);
    glDeleteBuffers(1, &vbo);
    glDeleteVertexArrays(1, &vao);
    glDeleteTextures(1, &tex);
}
